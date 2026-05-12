# pyright: reportMissingImports=false
from dataclasses import dataclass

import cv2
import numpy as np
import torch


@dataclass
class CropTransform:
    x: int
    y: int
    size: int
    output_size: int = 224
    source: str = "center"
    confidence: float = 0.0

    def points_to_frame(self, points):
        pts = np.asarray(points, dtype=np.float32).copy()
        scale = float(self.size) / float(self.output_size)
        pts[:, 0] = pts[:, 0] * scale + float(self.x)
        pts[:, 1] = pts[:, 1] * scale + float(self.y)
        return pts


@dataclass
class PreparedFrame:
    display_bgr: np.ndarray
    crop_rgb: np.ndarray
    tensor: torch.Tensor
    transform: CropTransform


def prepare_camera_frame(frame_bgr, mirror=True, crop_scale=1.0, output_size=224, roi=None, enhance=True):
    if frame_bgr is None or frame_bgr.size == 0:
        raise ValueError("empty camera frame")

    display = cv2.flip(frame_bgr, 1) if mirror else frame_bgr.copy()
    height, width = display.shape[:2]
    if roi is None:
        crop_scale = float(np.clip(crop_scale, 0.35, 1.0))
        crop_size = max(32, int(min(width, height) * crop_scale))
        x = max(0, (width - crop_size) // 2)
        y = max(0, (height - crop_size) // 2)
        source = "center"
        confidence = 0.0
    else:
        crop_size = int(roi.size)
        x = int(roi.x)
        y = int(roi.y)
        source = roi.source
        confidence = float(roi.confidence)

    crop = display[y:y + crop_size, x:x + crop_size]
    if enhance:
        crop = enhance_crop(crop)
    resized = cv2.resize(crop, (output_size, output_size), interpolation=cv2.INTER_AREA)
    crop_rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
    tensor = torch.as_tensor(crop_rgb, dtype=torch.float32).permute(2, 0, 1) / 255.0
    return PreparedFrame(
        display_bgr=display,
        crop_rgb=crop_rgb,
        tensor=tensor,
        transform=CropTransform(x=x, y=y, size=crop_size, output_size=output_size, source=source, confidence=confidence),
    )


def enhance_crop(crop_bgr):
    lab = cv2.cvtColor(crop_bgr, cv2.COLOR_BGR2LAB)
    l, a, b = cv2.split(lab)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    l = clahe.apply(l)
    enhanced = cv2.merge((l, a, b))
    return cv2.cvtColor(enhanced, cv2.COLOR_LAB2BGR)


def prepare_image_file(path, output_size=224):
    frame = cv2.imread(str(path))
    if frame is None:
        raise FileNotFoundError(str(path))
    return prepare_camera_frame(frame, mirror=False, crop_scale=1.0, output_size=output_size)


def draw_prediction_overlay(frame_bgr, prediction, transform):
    canvas = frame_bgr.copy()
    x0, y0, size = transform.x, transform.y, transform.size
    cv2.rectangle(canvas, (x0, y0), (x0 + size, y0 + size), (70, 210, 255), 2)
    cv2.putText(canvas, "ROI: {} {:.2f}".format(transform.source, transform.confidence),
                (x0, max(18, y0 - 8)), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (70, 210, 255), 2, cv2.LINE_AA)

    if prediction is None or prediction.keypoints_2d is None:
        return canvas

    points = transform.points_to_frame(prediction.keypoints_2d).astype(np.int32)
    edges = [
        (0, 1), (1, 2), (2, 3), (3, 4),
        (0, 5), (5, 6), (6, 7), (7, 8),
        (0, 9), (9, 10), (10, 11), (11, 12),
        (0, 13), (13, 14), (14, 15), (15, 16),
        (0, 17), (17, 18), (18, 19), (19, 20),
    ]
    for a, b in edges:
        pa = tuple(points[a])
        pb = tuple(points[b])
        cv2.line(canvas, pa, pb, (30, 255, 120), 2, cv2.LINE_AA)
    for index, point in enumerate(points):
        color = (0, 0, 0) if index == 0 else (255, 220, 70)
        cv2.circle(canvas, tuple(point), 4, color, -1, cv2.LINE_AA)
    return canvas
