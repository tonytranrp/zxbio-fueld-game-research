# pyright: reportMissingImports=false
from pathlib import Path
import time
import traceback
import tkinter as tk
from tkinter import ttk, messagebox

import cv2
from PIL import Image, ImageTk

from .export import export_capture
from .preprocess import prepare_camera_frame, draw_prediction_overlay
from .quality import analyze_mesh
from .roi import HandRoiTracker
from .runtime import MobileHandRuntime
from .smoothing import PredictionSmoother


class HandCaptureApp(tk.Tk):
    def __init__(self, capture_dir=None, smoke_frames=0):
        super().__init__()
        self.title("Hand Capture Studio - MobileHand Runtime")
        self.geometry("1180x760")
        self.minsize(980, 640)

        self.capture_dir = Path(capture_dir or Path(__file__).resolve().parents[1] / "captures")
        self.smoke_frames = int(smoke_frames or 0)
        self.processed_frames = 0

        self.runtime = None
        self.runtime_dataset = None
        self.cap = None
        self.running = False
        self.last_prediction = None
        self.last_quality = None
        self.last_frame_time = time.perf_counter()
        self.photo = None
        self.smoother = PredictionSmoother()
        self.roi_tracker = HandRoiTracker()

        self.dataset_var = tk.StringVar(value="freihand")
        self.camera_var = tk.IntVar(value=0)
        self.mirror_var = tk.BooleanVar(value=True)
        self.cuda_var = tk.BooleanVar(value=False)
        self.enhance_var = tk.BooleanVar(value=True)
        self.roi_mode_var = tk.StringVar(value="auto")
        self.crop_var = tk.DoubleVar(value=0.72)
        self.smoothing_var = tk.DoubleVar(value=0.35)
        self.status_var = tk.StringVar(value="Ready. Choose camera and press Start.")
        self.mesh_var = tk.StringVar(value="No mesh captured yet.")

        self._build_ui()
        self.protocol("WM_DELETE_WINDOW", self.close)

    def _build_ui(self):
        root = ttk.Frame(self, padding=10)
        root.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(root)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        right = ttk.Frame(root, width=320)
        right.pack(side=tk.RIGHT, fill=tk.Y, padx=(12, 0))

        self.image_label = ttk.Label(left, text="Camera preview", anchor=tk.CENTER)
        self.image_label.pack(fill=tk.BOTH, expand=True)

        controls = ttk.LabelFrame(right, text="Runtime Controls", padding=10)
        controls.pack(fill=tk.X)

        ttk.Label(controls, text="Dataset / weights").grid(row=0, column=0, sticky=tk.W)
        ttk.OptionMenu(controls, self.dataset_var, self.dataset_var.get(), "freihand", "stb").grid(row=0, column=1, sticky=tk.EW)

        ttk.Label(controls, text="Camera index").grid(row=1, column=0, sticky=tk.W, pady=(8, 0))
        ttk.Spinbox(controls, from_=0, to=8, textvariable=self.camera_var, width=8).grid(row=1, column=1, sticky=tk.EW, pady=(8, 0))

        ttk.Checkbutton(controls, text="Mirror camera", variable=self.mirror_var).grid(row=2, column=0, columnspan=2, sticky=tk.W, pady=(8, 0))
        ttk.Checkbutton(controls, text="Use CUDA if available", variable=self.cuda_var).grid(row=3, column=0, columnspan=2, sticky=tk.W)
        ttk.Checkbutton(controls, text="Normalize lighting", variable=self.enhance_var).grid(row=4, column=0, columnspan=2, sticky=tk.W)

        ttk.Label(controls, text="Hand crop mode").grid(row=5, column=0, sticky=tk.W, pady=(10, 0))
        ttk.OptionMenu(controls, self.roi_mode_var, self.roi_mode_var.get(), "auto", "mediapipe", "motion", "skin", "center").grid(row=5, column=1, sticky=tk.EW, pady=(10, 0))

        ttk.Label(controls, text="Fallback crop size").grid(row=6, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Scale(controls, from_=0.35, to=1.0, variable=self.crop_var, orient=tk.HORIZONTAL).grid(row=7, column=0, columnspan=2, sticky=tk.EW)
        ttk.Label(controls, text="Smoothing").grid(row=8, column=0, columnspan=2, sticky=tk.W, pady=(10, 0))
        ttk.Scale(controls, from_=0.0, to=0.9, variable=self.smoothing_var, orient=tk.HORIZONTAL).grid(row=9, column=0, columnspan=2, sticky=tk.EW)

        button_row = ttk.Frame(controls)
        button_row.grid(row=10, column=0, columnspan=2, sticky=tk.EW, pady=(12, 0))
        ttk.Button(button_row, text="Start", command=self.start).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(button_row, text="Stop", command=self.stop).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(6, 0))
        ttk.Button(controls, text="Capture OBJ + JSON", command=self.capture).grid(row=11, column=0, columnspan=2, sticky=tk.EW, pady=(8, 0))
        controls.columnconfigure(1, weight=1)

        stats = ttk.LabelFrame(right, text="Latest Mesh", padding=10)
        stats.pack(fill=tk.BOTH, expand=True, pady=(12, 0))
        ttk.Label(stats, textvariable=self.mesh_var, justify=tk.LEFT, wraplength=290).pack(fill=tk.X)
        ttk.Label(stats, text="Capture History").pack(fill=tk.X, pady=(12, 2))
        self.history_list = tk.Listbox(stats, height=8)
        self.history_list.pack(fill=tk.BOTH, expand=True)

        help_text = (
            "Auto crop tries MediaPipe, motion, then skin color.\n"
            "Use Center mode if tracking jumps too much.\n"
            "Fallback crop is used when no hand is found.\n"
            "Capture writes OBJ mesh plus JSON pose data."
        )
        ttk.Label(stats, text=help_text, justify=tk.LEFT, wraplength=290).pack(fill=tk.X, pady=(14, 0))

        status = ttk.Label(self, textvariable=self.status_var, anchor=tk.W, padding=(10, 4))
        status.pack(fill=tk.X, side=tk.BOTTOM)

    def _ensure_runtime(self):
        dataset = self.dataset_var.get()
        if self.runtime is not None and self.runtime_dataset == dataset:
            return
        self.status_var.set("Loading MobileHand {} model...".format(dataset))
        self.update_idletasks()
        self.runtime = MobileHandRuntime(dataset=dataset, use_cuda=self.cuda_var.get())
        self.runtime_dataset = dataset

    def start(self):
        if self.running:
            return
        try:
            self._ensure_runtime()
            index = int(self.camera_var.get())
            cap = cv2.VideoCapture(index)
            if not cap.isOpened():
                raise RuntimeError("Could not open camera index {}".format(index))
            self.cap = cap
            self.running = True
            self.processed_frames = 0
            self.last_frame_time = time.perf_counter()
            self.smoother.reset()
            self.roi_tracker.reset()
            self.status_var.set("Camera running. Press Capture to save the current hand mesh.")
            self.after(1, self._tick)
        except Exception as exc:
            self.status_var.set("Start failed: {}".format(exc))
            messagebox.showerror("Start failed", "{}\n\n{}".format(exc, traceback.format_exc()))
            self.stop()

    def stop(self):
        self.running = False
        if self.cap is not None:
            self.cap.release()
            self.cap = None
        self.roi_tracker.reset()
        self.status_var.set("Stopped.")

    def _tick(self):
        if not self.running or self.cap is None:
            return

        ok, frame = self.cap.read()
        if not ok:
            self.status_var.set("Camera frame read failed.")
            self.stop()
            return

        try:
            display_frame = cv2.flip(frame, 1) if self.mirror_var.get() else frame.copy()
            roi = self.roi_tracker.locate(
                display_frame,
                mode=self.roi_mode_var.get(),
                crop_scale=self.crop_var.get(),
            )
            prepared = prepare_camera_frame(
                display_frame,
                mirror=False,
                crop_scale=self.crop_var.get(),
                roi=roi,
                enhance=self.enhance_var.get(),
            )
            runtime = self.runtime
            if runtime is None:
                raise RuntimeError("MobileHand runtime is not loaded")
            prediction = runtime.predict_tensor(prepared.tensor)
            prediction = self.smoother.apply(prediction, self.smoothing_var.get())
            prediction.quality = analyze_mesh(prediction.vertices, prediction.faces)
            self.last_prediction = prediction
            self.last_quality = prediction.quality
            overlay = draw_prediction_overlay(prepared.display_bgr, prediction, prepared.transform)
            self._show_frame(overlay)
            self._update_stats(prediction, prepared.transform)
        except Exception as exc:
            self.status_var.set("Frame failed: {}".format(exc))

        self.processed_frames += 1
        if self.smoke_frames > 0 and self.processed_frames >= self.smoke_frames:
            if self.last_prediction is not None:
                self.capture()
            self.close()
            return
        self.after(1, self._tick)

    def _show_frame(self, frame_bgr):
        rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
        image = Image.fromarray(rgb)
        max_w = max(320, self.image_label.winfo_width())
        max_h = max(240, self.image_label.winfo_height())
        image.thumbnail((max_w, max_h), Image.BILINEAR)
        self.photo = ImageTk.PhotoImage(image=image)
        self.image_label.configure(image=self.photo, text="")

    def _update_stats(self, prediction, transform):
        now = time.perf_counter()
        dt = max(now - self.last_frame_time, 0.0001)
        self.last_frame_time = now
        fps = 1.0 / dt
        self.mesh_var.set(
            "Dataset: {}\nROI: {} ({:.2f})\nCrop: {}px\nVertices: {}\nFaces: {}\nFPS: {:.1f}\nQuality: {}\nBBox: {:.1f} x {:.1f} x {:.1f}\nWarnings: {}\nCapture folder:\n{}".format(
                prediction.dataset,
                transform.source,
                transform.confidence,
                transform.size,
                len(prediction.vertices),
                len(prediction.faces),
                fps,
                prediction.quality.status if prediction.quality else "unknown",
                *(prediction.quality.bbox_size if prediction.quality else (0.0, 0.0, 0.0)),
                "none" if not prediction.quality or not prediction.quality.warnings else "; ".join(prediction.quality.warnings),
                self.capture_dir,
            )
        )

    def capture(self):
        if self.last_prediction is None:
            self.status_var.set("No prediction to capture yet.")
            return
        obj_path, json_path = export_capture(self.capture_dir, self.last_prediction)
        quality = analyze_mesh(self.last_prediction.vertices, self.last_prediction.faces)
        self.last_prediction.quality = quality
        label = "{} [{}]".format(obj_path.name, quality.status)
        self.history_list.insert(0, label)
        self.status_var.set("Saved {} and {} ({})".format(obj_path.name, json_path.name, quality.status))

    def close(self):
        self.stop()
        self.destroy()
