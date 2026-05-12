# pyright: reportMissingImports=false
from dataclasses import replace

import numpy as np


class PredictionSmoother:
    def __init__(self):
        self.previous = None

    def reset(self):
        self.previous = None

    def apply(self, prediction, strength):
        strength = float(np.clip(strength, 0.0, 0.95))
        if strength <= 0.0 or self.previous is None:
            self.previous = prediction
            return prediction

        prev = self.previous
        smoothed = replace(
            prediction,
            keypoints_2d=_blend(prev.keypoints_2d, prediction.keypoints_2d, strength),
            joints_3d=_blend(prev.joints_3d, prediction.joints_3d, strength),
            vertices=_blend(prev.vertices, prediction.vertices, strength),
            pose=_blend(prev.pose, prediction.pose, strength),
            params=_blend(prev.params, prediction.params, strength),
        )
        self.previous = smoothed
        return smoothed


def _blend(previous, current, strength):
    previous = np.asarray(previous, dtype=np.float32)
    current = np.asarray(current, dtype=np.float32)
    return previous * strength + current * (1.0 - strength)
