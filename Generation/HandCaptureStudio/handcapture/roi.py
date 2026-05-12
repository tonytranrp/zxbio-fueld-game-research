# pyright: reportMissingImports=false
from dataclasses import dataclass

import cv2
import numpy as np


@dataclass
class RoiResult:
    x: int
    y: int
    size: int
    source: str
    confidence: float = 0.0


class HandRoiTracker:
    def __init__(self):
        self.previous_roi = None
        self.previous_gray = None
        self._mp_hands = None
        self._mp_module = None
        self._mediapipe_failed = False

    def reset(self):
        self.previous_roi = None
        self.previous_gray = None

    def locate(self, frame_bgr, mode="auto", crop_scale=0.72):
        height, width = frame_bgr.shape[:2]
        mode = (mode or "auto").lower()
        crop_scale = float(np.clip(crop_scale, 0.35, 1.0))

        roi = None
        if mode in ("auto", "mediapipe"):
            roi = self._mediapipe_roi(frame_bgr)
            if roi is not None:
                roi = self._smooth_roi(roi, width, height)
                self._remember(frame_bgr, roi)
                return roi
            if mode == "mediapipe":
                roi = self._previous_or_center(width, height, crop_scale, "mediapipe-missing")
                self._remember(frame_bgr, roi)
                return roi

        if mode in ("auto", "motion"):
            roi = self._motion_roi(frame_bgr)
            if roi is not None:
                roi = self._smooth_roi(roi, width, height)
                self._remember(frame_bgr, roi)
                return roi

        if mode in ("auto", "skin"):
            roi = self._skin_roi(frame_bgr)
            if roi is not None:
                roi = self._smooth_roi(roi, width, height)
                self._remember(frame_bgr, roi)
                return roi

        roi = self._previous_or_center(width, height, crop_scale, "center")
        self._remember(frame_bgr, roi)
        return roi

    def _mediapipe_roi(self, frame_bgr):
        if self._mediapipe_failed:
            return None
        try:
            if self._mp_hands is None:
                import mediapipe as mp
                self._mp_module = mp
                self._mp_hands = mp.solutions.hands.Hands(
                    static_image_mode=False,
                    max_num_hands=1,
                    model_complexity=0,
                    min_detection_confidence=0.45,
                    min_tracking_confidence=0.45,
                )
            rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
            results = self._mp_hands.process(rgb)
            if not results.multi_hand_landmarks:
                return None
            hand = results.multi_hand_landmarks[0]
            height, width = frame_bgr.shape[:2]
            xs = np.array([lm.x * width for lm in hand.landmark], dtype=np.float32)
            ys = np.array([lm.y * height for lm in hand.landmark], dtype=np.float32)
            score = 0.85
            if results.multi_handedness:
                score = float(results.multi_handedness[0].classification[0].score)
            return _square_from_points(xs, ys, width, height, 1.75, "mediapipe", score)
        except Exception:
            self._mediapipe_failed = True
            return None

    def _motion_roi(self, frame_bgr):
        gray = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY)
        if self.previous_gray is None:
            return None
        diff = cv2.absdiff(gray, self.previous_gray)
        _, mask = cv2.threshold(diff, 24, 255, cv2.THRESH_BINARY)
        mask = cv2.medianBlur(mask, 5)
        mask = cv2.dilate(mask, None, iterations=2)
        return _roi_from_mask(mask, frame_bgr.shape[1], frame_bgr.shape[0], 1.55, "motion")

    def _skin_roi(self, frame_bgr):
        ycrcb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2YCrCb)
        lower = np.array([0, 133, 77], dtype=np.uint8)
        upper = np.array([255, 173, 127], dtype=np.uint8)
        mask = cv2.inRange(ycrcb, lower, upper)
        mask = cv2.medianBlur(mask, 5)
        mask = cv2.dilate(mask, None, iterations=1)
        return _roi_from_mask(mask, frame_bgr.shape[1], frame_bgr.shape[0], 1.65, "skin")

    def _previous_or_center(self, width, height, crop_scale, source):
        if self.previous_roi is not None:
            return RoiResult(self.previous_roi.x, self.previous_roi.y, self.previous_roi.size, source, self.previous_roi.confidence)
        size = max(32, int(min(width, height) * crop_scale))
        return RoiResult(max(0, (width - size) // 2), max(0, (height - size) // 2), size, source, 0.0)

    def _smooth_roi(self, roi, width, height):
        if self.previous_roi is None:
            return roi
        alpha = 0.68
        x = int(round(self.previous_roi.x * alpha + roi.x * (1.0 - alpha)))
        y = int(round(self.previous_roi.y * alpha + roi.y * (1.0 - alpha)))
        size = int(round(self.previous_roi.size * alpha + roi.size * (1.0 - alpha)))
        return _clamp_roi(x, y, size, width, height, roi.source, roi.confidence)

    def _remember(self, frame_bgr, roi):
        self.previous_roi = roi
        self.previous_gray = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY)


def _roi_from_mask(mask, width, height, padding, source):
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return None
    contour = max(contours, key=cv2.contourArea)
    area = cv2.contourArea(contour)
    if area < 850.0:
        return None
    x, y, w, h = cv2.boundingRect(contour)
    xs = np.array([x, x + w], dtype=np.float32)
    ys = np.array([y, y + h], dtype=np.float32)
    confidence = float(min(1.0, area / float(width * height) * 10.0))
    return _square_from_points(xs, ys, width, height, padding, source, confidence)


def _square_from_points(xs, ys, width, height, padding, source, confidence):
    min_x = float(np.clip(xs.min(), 0, width - 1))
    max_x = float(np.clip(xs.max(), 0, width - 1))
    min_y = float(np.clip(ys.min(), 0, height - 1))
    max_y = float(np.clip(ys.max(), 0, height - 1))
    cx = (min_x + max_x) * 0.5
    cy = (min_y + max_y) * 0.5
    size = int(max(max_x - min_x, max_y - min_y) * padding)
    size = max(64, min(size, min(width, height)))
    x = int(round(cx - size * 0.5))
    y = int(round(cy - size * 0.5))
    return _clamp_roi(x, y, size, width, height, source, confidence)


def _clamp_roi(x, y, size, width, height, source, confidence):
    size = max(32, min(int(size), min(width, height)))
    x = max(0, min(int(x), width - size))
    y = max(0, min(int(y), height - size))
    return RoiResult(x=x, y=y, size=size, source=source, confidence=float(confidence))
