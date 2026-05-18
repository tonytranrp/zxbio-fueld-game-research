from __future__ import annotations

import argparse
import ctypes
from dataclasses import dataclass
import json
import os
from pathlib import Path
import queue
import socket
import sys
import threading
import time
from typing import Any

from protocol import hand_from_result, pack_frame, pack_preview


@dataclass(frozen=True)
class CameraSnapshot:
    sequence: int
    monotonic_ms: int
    frame: Any


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
        self.latest_camera_frame: Any | None = None
        self.latest_camera_sequence = 0
        self.latest_camera_time_ms = 0
        self.camera_lock = threading.Lock()


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


def camera_indices(config: dict[str, Any], requested_index: int) -> list[int]:
    camera_config = config.get("camera", {})
    indices = [requested_index]
    for value in camera_config.get("candidate_indices", [0, 1, 2, 3]):
        try:
            index = int(value)
        except (TypeError, ValueError):
            continue
        if index not in indices:
            indices.append(index)
    return indices


def camera_backends(config: dict[str, Any]) -> list[tuple[str, int]]:
    import cv2

    camera_config = config.get("camera", {})
    names = camera_config.get("backends")
    if not names:
        names = ["dshow", "default", "msmf"] if os.name == "nt" else ["default"]

    values: list[tuple[str, int]] = []
    for raw_name in names:
        name = str(raw_name).lower()
        if name == "default":
            values.append(("default", 0))
        elif name == "dshow" and hasattr(cv2, "CAP_DSHOW"):
            values.append(("dshow", int(cv2.CAP_DSHOW)))
        elif name == "msmf" and hasattr(cv2, "CAP_MSMF"):
            values.append(("msmf", int(cv2.CAP_MSMF)))
        elif name == "v4l2" and hasattr(cv2, "CAP_V4L2"):
            values.append(("v4l2", int(cv2.CAP_V4L2)))
    return values or [("default", 0)]


def open_camera(config: dict[str, Any], requested_index: int) -> tuple[Any, int, str]:
    import cv2

    for index in camera_indices(config, requested_index):
        for backend_name, backend_id in camera_backends(config):
            capture = cv2.VideoCapture(index) if backend_id == 0 else cv2.VideoCapture(index, backend_id)
            if capture.isOpened():
                return capture, index, backend_name
            capture.release()
    raise RuntimeError(f"Could not open any configured camera index near {requested_index}")


def read_camera_frame(capture: Any, attempts: int = 30, delay_seconds: float = 0.05) -> Any | None:
    for _ in range(max(1, attempts)):
        ok, frame = capture.read()
        if ok and frame is not None and frame.size > 0:
            return frame
        time.sleep(delay_seconds)
    return None


def choose_camera_mode(capture: Any, config: dict[str, Any]) -> tuple[tuple[int, int, int], Any | None]:
    import cv2

    candidates = config.get("camera", {}).get("candidate_modes", [])
    if not candidates:
        candidates = [
            {"width": 1280, "height": 720, "fps": 30},
            {"width": 640, "height": 480, "fps": 30},
        ]
    best = (640, 480, 30)
    warmup_frame = None
    for candidate in candidates:
        width = int(candidate.get("width", 640))
        height = int(candidate.get("height", 480))
        fps = int(candidate.get("fps", 30))
        capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        capture.set(cv2.CAP_PROP_FPS, fps)
        frame = read_camera_frame(capture, attempts=8, delay_seconds=0.04)
        if frame is not None:
            actual_width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH)) or width
            actual_height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT)) or height
            actual_fps = int(capture.get(cv2.CAP_PROP_FPS)) or fps
            best = (actual_width, actual_height, actual_fps)
            warmup_frame = frame
            break
    capture.set(cv2.CAP_PROP_FRAME_WIDTH, best[0])
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, best[1])
    capture.set(cv2.CAP_PROP_FPS, best[2])
    return best, warmup_frame


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
                client.settimeout(0.01)
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
                        except (socket.timeout, BlockingIOError):
                            # Send would block — skip this frame and grab the
                            # latest on the next iteration.  The C++ side
                            # receives the freshest frame instead of a stale
                            # queued frame, avoiding burst-and-wait stutter.
                            continue
                        except OSError:
                            break
                    else:
                        time.sleep(0.01)


def encode_preview(frame: Any, config: dict[str, Any]) -> bytes | None:
    import cv2

    preview_config = config.get("preview", {})
    max_width = max(1, int(preview_config.get("max_width", 480)))
    quality = min(100, max(1, int(preview_config.get("jpeg_quality", 72))))
    output = frame
    height, width = frame.shape[:2]
    if width <= 0 or height <= 0:
        return None
    if width > max_width:
        scale = max_width / float(width)
        output = cv2.resize(frame, (max_width, max(1, int(round(height * scale)))))
    ok, encoded = cv2.imencode(".jpg", output, [int(cv2.IMWRITE_JPEG_QUALITY), quality])
    if not ok:
        return None
    return encoded.tobytes()


def set_latest_camera_frame(state: WorkerState, frame: Any, sequence: int) -> None:
    now_ms = int(time.monotonic() * 1000)
    with state.camera_lock:
        state.latest_camera_frame = frame
        state.latest_camera_sequence = sequence
        state.latest_camera_time_ms = now_ms


def latest_camera_frame(state: WorkerState) -> CameraSnapshot | None:
    with state.camera_lock:
        if state.latest_camera_frame is None:
            return None
        return CameraSnapshot(
            sequence=state.latest_camera_sequence,
            monotonic_ms=state.latest_camera_time_ms,
            frame=state.latest_camera_frame,
        )


def clear_camera_frame(state: WorkerState) -> None:
    with state.camera_lock:
        state.latest_camera_frame = None
        state.latest_camera_sequence = 0
        state.latest_camera_time_ms = 0


def set_latest_preview(state: WorkerState, sequence: int, frame: Any) -> None:
    jpeg = encode_preview(frame, state.config)
    if jpeg is None:
        return
    with state.preview_lock:
        state.latest_preview = jpeg
        state.latest_preview_sequence = sequence


def interval_seconds(config: dict[str, Any], section: str) -> float | None:
    value = config.get(section, {}).get("max_fps", "camera")
    if value is None:
        return None
    if isinstance(value, str) and value.strip().lower() in {"", "camera", "auto", "adaptive"}:
        return None
    try:
        max_fps = float(value)
    except (TypeError, ValueError):
        return None
    if max_fps <= 0.0:
        return None
    return 1.0 / max(1.0, max_fps)


def tracking_interval_seconds(config: dict[str, Any]) -> float:
    return interval_seconds(config, "mediapipe") or 0.0


def camera_capture_loop(state: WorkerState, capture: Any, first_sequence: int) -> None:
    sequence = first_sequence
    while state.tracking_event.is_set() and not state.stop_event.is_set():
        ok, frame = capture.read()
        if not ok or frame is None or frame.size == 0:
            time.sleep(0.01)
            continue

        set_latest_camera_frame(state, frame, sequence)
        sequence += 1


def preview_encode_loop(state: WorkerState) -> None:
    last_preview_time = 0.0
    last_preview_sequence = -1
    while state.tracking_event.is_set() and not state.stop_event.is_set():
        if state.preview_enabled:
            snapshot = latest_camera_frame(state)
            if snapshot is not None:
                sequence = snapshot.sequence
                now = time.monotonic()
                interval = interval_seconds(state.config, "preview")
                paced = interval is None or now - last_preview_time >= interval
                if sequence != last_preview_sequence and paced:
                    set_latest_preview(state, sequence, snapshot.frame)
                    last_preview_sequence = sequence
                    last_preview_time = now
                    continue
        time.sleep(0.002)


def tracking_loop(state: WorkerState) -> None:
    import cv2

    sequence = 0

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        while not state.stop_event.is_set():
            state.tracking_event.wait(0.1)
            if not state.tracking_event.is_set() or state.stop_event.is_set():
                continue

            command = {}
            while not state.command_queue.empty():
                command = state.command_queue.get_nowait()

            capture = None
            camera_thread = None
            preview_thread = None
            try:
                camera_config = state.config.get("camera", {})
                mp_config = state.config.get("mediapipe", {})
                camera_index = int(command.get("camera_index", camera_config.get("index", 0)))
                gesture_threshold = float(mp_config.get("gesture_score_threshold", 0.0))
                handedness_threshold = float(mp_config.get("handedness_score_threshold", 0.0))
                handedness_margin_threshold = float(mp_config.get("handedness_margin_threshold", 0.0))
                capture, active_camera_index, active_backend = open_camera(state.config, camera_index)
                print(
                    f"hand-tracking camera opened: index={active_camera_index} backend={active_backend}",
                    file=sys.stderr,
                    flush=True,
                )

                _, warmup_frame = choose_camera_mode(capture, state.config)
                if warmup_frame is None:
                    warmup_frame = read_camera_frame(capture)
                if warmup_frame is None:
                    raise RuntimeError("Camera opened but did not deliver frames")

                clear_camera_frame(state)
                set_latest_camera_frame(state, warmup_frame, 0)
                frame_height, frame_width = warmup_frame.shape[:2]
                udp.sendto(
                    pack_frame(
                        sequence=sequence,
                        timestamp_ms=int(time.time() * 1000),
                        camera_width=int(frame_width),
                        camera_height=int(frame_height),
                        hands=[],
                    ),
                    (state.args.udp_host, int(state.args.udp_port)),
                )
                if state.preview_enabled:
                    set_latest_preview(state, sequence, warmup_frame)
                sequence += 1
                camera_thread = threading.Thread(
                    target=camera_capture_loop,
                    args=(state, capture, 1),
                    daemon=True,
                )
                camera_thread.start()
                preview_thread = threading.Thread(target=preview_encode_loop, args=(state,), daemon=True)
                preview_thread.start()

                import mediapipe as mp

                BaseOptions = mp.tasks.BaseOptions
                GestureRecognizer = mp.tasks.vision.GestureRecognizer
                GestureRecognizerOptions = mp.tasks.vision.GestureRecognizerOptions
                VisionRunningMode = mp.tasks.vision.RunningMode

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
                    last_frame_ms = -1
                    last_camera_sequence = -1
                    last_tracking_time = 0.0
                    while state.tracking_event.is_set() and not state.stop_event.is_set():
                        snapshot = latest_camera_frame(state)
                        if snapshot is None:
                            time.sleep(0.005)
                            continue

                        camera_sequence = snapshot.sequence
                        if camera_sequence == last_camera_sequence:
                            time.sleep(0.002)
                            continue

                        now = time.monotonic()
                        min_interval = tracking_interval_seconds(state.config)
                        if min_interval > 0.0 and now - last_tracking_time < min_interval:
                            time.sleep(min_interval - (now - last_tracking_time))
                            continue

                        last_camera_sequence = camera_sequence
                        last_tracking_time = time.monotonic()
                        rgb = cv2.cvtColor(snapshot.frame, cv2.COLOR_BGR2RGB)
                        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                        frame_ms = max(0, snapshot.monotonic_ms - started_ms)
                        if frame_ms <= last_frame_ms:
                            frame_ms = last_frame_ms + 1
                        last_frame_ms = frame_ms
                        result = recognizer.recognize_for_video(mp_image, frame_ms)
                        hands = [
                            hand_from_result(
                                result,
                                index,
                                gesture_threshold,
                                handedness_threshold,
                                handedness_margin_threshold,
                            )
                            for index in range(min(len(result.hand_landmarks), 2))
                        ]
                        frame_height, frame_width = snapshot.frame.shape[:2]
                        packet = pack_frame(
                            sequence=sequence,
                            timestamp_ms=int(time.time() * 1000),
                            camera_width=int(frame_width),
                            camera_height=int(frame_height),
                            hands=hands,
                        )
                        udp.sendto(packet, (state.args.udp_host, int(state.args.udp_port)))

                        sequence += 1
            except Exception as exc:  # noqa: BLE001 - keep worker alive after camera/model faults.
                print(f"hand-tracking worker error: {exc}", file=sys.stderr, flush=True)
                state.tracking_event.clear()
                time.sleep(0.5)
            finally:
                state.tracking_event.clear()
                if preview_thread is not None:
                    preview_thread.join(timeout=1.0)
                if camera_thread is not None:
                    camera_thread.join(timeout=1.0)
                if capture is not None:
                    capture.release()
                clear_camera_frame(state)


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
