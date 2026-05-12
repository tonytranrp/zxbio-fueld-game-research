from __future__ import annotations

import argparse
import ctypes
import json
import os
from pathlib import Path
import queue
import socket
import struct
import sys
import threading
import time
from typing import Any

from protocol import hand_from_result, pack_frame, pack_preview


class WorkerState:
    def __init__(self, args: argparse.Namespace, config: dict[str, Any]) -> None:
        self.args = args
        self.config = config
        self.stop_event = threading.Event()
        self.tracking_event = threading.Event()
        self.preview_enabled = bool(config.get("preview", {}).get("enabled", False))
        self.command_queue: queue.Queue[dict[str, Any]] = queue.Queue()
        self.latest_preview: bytes | None = None
        self.latest_preview_sequence = 0
        self.preview_lock = threading.Lock()


def load_config(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def parent_alive(parent_pid: int) -> bool:
    if parent_pid <= 0:
        return True
    if os.name == "nt":
        synchronize = 0x00100000
        handle = ctypes.windll.kernel32.OpenProcess(synchronize, False, parent_pid)
        if not handle:
            return False
        wait_result = ctypes.windll.kernel32.WaitForSingleObject(handle, 0)
        ctypes.windll.kernel32.CloseHandle(handle)
        return wait_result == 0x00000102
    try:
        os.kill(parent_pid, 0)
        return True
    except OSError:
        return False


def choose_camera_mode(capture: Any, config: dict[str, Any]) -> tuple[int, int, int]:
    candidates = config.get("camera", {}).get("candidate_modes", [])
    if not candidates:
        candidates = [
            {"width": 1280, "height": 720, "fps": 30},
            {"width": 640, "height": 480, "fps": 30},
        ]
    best = (640, 480, 30)
    for candidate in candidates:
        width = int(candidate.get("width", 640))
        height = int(candidate.get("height", 480))
        fps = int(candidate.get("fps", 30))
        capture.set(3, width)
        capture.set(4, height)
        capture.set(5, fps)
        ok, frame = capture.read()
        if ok and frame is not None and frame.size > 0:
            actual_width = int(capture.get(3)) or width
            actual_height = int(capture.get(4)) or height
            actual_fps = int(capture.get(5)) or fps
            best = (actual_width, actual_height, actual_fps)
            break
    capture.set(3, best[0])
    capture.set(4, best[1])
    capture.set(5, best[2])
    return best


def control_server(state: WorkerState) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", int(state.args.control_port)))
        server.listen(4)
        server.settimeout(0.25)
        while not state.stop_event.is_set():
            try:
                client, _ = server.accept()
            except TimeoutError:
                continue
            with client:
                data = client.recv(4096)
                if not data:
                    continue
                for line in data.splitlines():
                    try:
                        command = json.loads(line.decode("utf-8"))
                    except json.JSONDecodeError:
                        continue
                    name = command.get("command")
                    if name == "shutdown":
                        state.tracking_event.clear()
                        state.stop_event.set()
                    elif name == "stop":
                        state.tracking_event.clear()
                    elif name == "preview":
                        state.preview_enabled = bool(command.get("preview", False))
                    elif name == "start":
                        state.preview_enabled = bool(command.get("preview", state.preview_enabled))
                        state.command_queue.put(command)
                        state.tracking_event.set()


def preview_server(state: WorkerState) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind(("127.0.0.1", int(state.args.preview_port)))
        server.listen(1)
        server.settimeout(0.25)
        while not state.stop_event.is_set():
            try:
                client, _ = server.accept()
            except TimeoutError:
                continue
            with client:
                last_sequence = -1
                client.settimeout(0.25)
                while not state.stop_event.is_set():
                    if not state.preview_enabled:
                        time.sleep(0.05)
                        continue
                    with state.preview_lock:
                        preview = state.latest_preview
                        sequence = state.latest_preview_sequence
                    if preview is not None and sequence != last_sequence:
                        try:
                            client.sendall(pack_preview(sequence, preview))
                            last_sequence = sequence
                        except OSError:
                            break
                    else:
                        time.sleep(0.01)


def encode_preview(frame: Any, config: dict[str, Any]) -> bytes | None:
    import cv2

    preview_config = config.get("preview", {})
    max_width = int(preview_config.get("max_width", 480))
    quality = int(preview_config.get("jpeg_quality", 72))
    output = frame
    height, width = frame.shape[:2]
    if width > max_width:
        scale = max_width / float(width)
        output = cv2.resize(frame, (max_width, int(height * scale)))
    ok, encoded = cv2.imencode(".jpg", output, [int(cv2.IMWRITE_JPEG_QUALITY), quality])
    if not ok:
        return None
    return encoded.tobytes()


def tracking_loop(state: WorkerState) -> None:
    import cv2
    import mediapipe as mp

    BaseOptions = mp.tasks.BaseOptions
    GestureRecognizer = mp.tasks.vision.GestureRecognizer
    GestureRecognizerOptions = mp.tasks.vision.GestureRecognizerOptions
    VisionRunningMode = mp.tasks.vision.RunningMode

    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sequence = 0

    while not state.stop_event.is_set():
        state.tracking_event.wait(0.1)
        if not state.tracking_event.is_set() or state.stop_event.is_set():
            continue

        command = {}
        while not state.command_queue.empty():
            command = state.command_queue.get_nowait()

        camera_config = state.config.get("camera", {})
        mp_config = state.config.get("mediapipe", {})
        camera_index = int(command.get("camera_index", camera_config.get("index", 0)))
        capture = cv2.VideoCapture(camera_index)
        if not capture.isOpened():
            state.tracking_event.clear()
            time.sleep(0.5)
            continue

        width, height, _fps = choose_camera_mode(capture, state.config)
        options = GestureRecognizerOptions(
            base_options=BaseOptions(model_asset_path=str(Path(state.args.model).resolve())),
            running_mode=VisionRunningMode.VIDEO,
            num_hands=int(mp_config.get("num_hands", 2)),
            min_hand_detection_confidence=float(mp_config.get("min_hand_detection_confidence", 0.5)),
            min_hand_presence_confidence=float(mp_config.get("min_hand_presence_confidence", 0.5)),
            min_tracking_confidence=float(mp_config.get("min_tracking_confidence", 0.5)),
        )

        with GestureRecognizer.create_from_options(options) as recognizer:
            started_ms = int(time.monotonic() * 1000)
            while state.tracking_event.is_set() and not state.stop_event.is_set():
                ok, frame = capture.read()
                if not ok or frame is None:
                    time.sleep(0.02)
                    continue
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                frame_ms = int(time.monotonic() * 1000) - started_ms
                result = recognizer.recognize_for_video(mp_image, frame_ms)
                hands = [hand_from_result(result, index) for index in range(min(len(result.hand_landmarks), 2))]
                packet = pack_frame(
                    sequence=sequence,
                    timestamp_ms=int(time.time() * 1000),
                    camera_width=int(width),
                    camera_height=int(height),
                    hands=hands,
                )
                udp.sendto(packet, (state.args.udp_host, int(state.args.udp_port)))

                if state.preview_enabled:
                    jpeg = encode_preview(frame, state.config)
                    if jpeg is not None:
                        with state.preview_lock:
                            state.latest_preview = jpeg
                            state.latest_preview_sequence = sequence

                sequence += 1

        capture.release()


def parent_monitor(state: WorkerState) -> None:
    while not state.stop_event.is_set():
        if not parent_alive(int(state.args.parent_pid)):
            state.stop_event.set()
            state.tracking_event.clear()
            return
        time.sleep(0.5)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Biofuel hand-tracking worker")
    parser.add_argument("--model", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--udp-host", default="127.0.0.1")
    parser.add_argument("--udp-port", type=int, default=40241)
    parser.add_argument("--control-port", type=int, default=40242)
    parser.add_argument("--preview-port", type=int, default=40243)
    parser.add_argument("--parent-pid", type=int, default=0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config = load_config(Path(args.config))
    state = WorkerState(args, config)

    threads = [
        threading.Thread(target=control_server, args=(state,), daemon=True),
        threading.Thread(target=preview_server, args=(state,), daemon=True),
        threading.Thread(target=tracking_loop, args=(state,), daemon=True),
        threading.Thread(target=parent_monitor, args=(state,), daemon=True),
    ]
    for thread in threads:
        thread.start()

    try:
        while not state.stop_event.is_set():
            time.sleep(0.1)
    except KeyboardInterrupt:
        state.stop_event.set()
        state.tracking_event.clear()

    for thread in threads:
        thread.join(timeout=1.0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
