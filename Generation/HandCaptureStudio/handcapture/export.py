# pyright: reportMissingImports=false
from datetime import datetime
from pathlib import Path
import json

import numpy as np

from .quality import analyze_mesh


def timestamp_name(prefix="hand_capture"):
    return "{}_{}".format(prefix, datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3])


def save_obj(path, vertices, faces):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    vertices = np.asarray(vertices, dtype=np.float32)
    faces = np.asarray(faces, dtype=np.int32)
    with path.open("w", encoding="utf-8") as file:
        file.write("# HandCaptureStudio MobileHand export\n")
        for vertex in vertices:
            file.write("v {:.6f} {:.6f} {:.6f}\n".format(vertex[0], vertex[1], vertex[2]))
        for face in faces:
            file.write("f {} {} {}\n".format(face[0] + 1, face[1] + 1, face[2] + 1))
    return path


def save_prediction_json(path, prediction):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    quality = prediction.quality or analyze_mesh(prediction.vertices, prediction.faces)
    payload = {
        "dataset": prediction.dataset,
        "vertex_count": int(len(prediction.vertices)),
        "face_count": int(len(prediction.faces)),
        "keypoints_2d": prediction.keypoints_2d.tolist(),
        "joints_3d": prediction.joints_3d.tolist(),
        "pose": prediction.pose.tolist(),
        "params": prediction.params.tolist(),
        "quality": {
            "status": quality.status,
            "finite": quality.finite,
            "degenerate_faces": quality.degenerate_faces,
            "max_edge_length": quality.max_edge_length,
            "bbox_size": list(quality.bbox_size),
            "warnings": quality.warnings,
        },
    }
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return path


def export_capture(capture_dir, prediction, prefix="hand_capture"):
    capture_dir = Path(capture_dir)
    name = timestamp_name(prefix)
    obj_path = save_obj(capture_dir / "{}.obj".format(name), prediction.vertices, prediction.faces)
    json_path = save_prediction_json(capture_dir / "{}.json".format(name), prediction)
    return obj_path, json_path
