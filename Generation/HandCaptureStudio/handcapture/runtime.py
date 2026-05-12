# pyright: reportMissingImports=false
from dataclasses import dataclass
from pathlib import Path
import os
import sys
from types import SimpleNamespace
from typing import Optional

import numpy as np
import torch


ROOT = Path(__file__).resolve().parents[2]
MOBILEHAND_ROOT = ROOT / "mobilehand"
MOBILEHAND_CODE = MOBILEHAND_ROOT / "code"
MOBILEHAND_MODEL = MOBILEHAND_ROOT / "model"

if str(MOBILEHAND_CODE) not in sys.path:
    sys.path.insert(0, str(MOBILEHAND_CODE))

from utils_neural_network import HMR  # noqa: E402

from .quality import MeshQualityReport


@dataclass
class HandPrediction:
    keypoints_2d: np.ndarray
    joints_3d: np.ndarray
    vertices: np.ndarray
    pose: np.ndarray
    params: np.ndarray
    faces: np.ndarray
    dataset: str
    quality: Optional[MeshQualityReport] = None


class MobileHandRuntime:
    def __init__(self, dataset="freihand", use_cuda=False):
        self.dataset = dataset
        self.device = torch.device("cuda" if torch.cuda.is_available() and use_cuda else "cpu")
        self.model = None
        self._load_model()

    def _load_model(self):
        if self.dataset not in ("freihand", "stb"):
            raise ValueError("dataset must be 'freihand' or 'stb'")

        mano_path = MOBILEHAND_MODEL / "MANO_RIGHT.pkl"
        weights_path = MOBILEHAND_MODEL / "hmr_model_{}_auc.pth".format(self.dataset)
        if not mano_path.exists():
            raise FileNotFoundError("Missing MANO model: {}".format(mano_path))
        if not weights_path.exists():
            raise FileNotFoundError("Missing MobileHand weights: {}".format(weights_path))

        args = SimpleNamespace(data=self.dataset)
        previous_cwd = os.getcwd()
        os.chdir(str(MOBILEHAND_CODE))
        try:
            model = HMR(args)
        finally:
            os.chdir(previous_cwd)
        state = torch.load(str(weights_path), map_location=self.device)
        model.load_state_dict(state)
        model.to(self.device)
        model.eval()
        self.model = model

    @property
    def faces(self):
        model = self.model
        if model is None:
            raise RuntimeError("MobileHandRuntime has no loaded model")
        return np.asarray(model.mano.F, dtype=np.int32)

    def predict_tensor(self, image_tensor):
        if self.model is None:
            raise RuntimeError("MobileHandRuntime has no loaded model")
        with torch.no_grad():
            result = self.model(image_tensor.to(self.device).unsqueeze(0))
        keypoints, joints, vertices, pose, params = result
        return HandPrediction(
            keypoints_2d=keypoints[0].detach().cpu().numpy().astype(np.float32),
            joints_3d=joints[0].detach().cpu().numpy().astype(np.float32),
            vertices=vertices[0].detach().cpu().numpy().astype(np.float32),
            pose=pose[0].detach().cpu().numpy().astype(np.float32),
            params=params[0].detach().cpu().numpy().astype(np.float32),
            faces=self.faces,
            dataset=self.dataset,
        )
