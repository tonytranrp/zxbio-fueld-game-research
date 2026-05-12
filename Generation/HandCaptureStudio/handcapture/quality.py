# pyright: reportMissingImports=false
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

import numpy as np


@dataclass
class MeshQualityReport:
    vertex_count: int
    face_count: int
    finite: bool
    degenerate_faces: int
    max_edge_length: float
    bbox_size: Tuple[float, float, float]
    status: str
    warnings: List[str]

    @property
    def ok(self):
        return self.status == "ok"

    def summary(self):
        warning_text = "none" if not self.warnings else "; ".join(self.warnings)
        return (
            "Status: {}\n"
            "Vertices: {} | Faces: {}\n"
            "BBox: {:.1f} x {:.1f} x {:.1f}\n"
            "Degenerate faces: {}\n"
            "Max edge: {:.1f}\n"
            "Warnings: {}"
        ).format(
            self.status,
            self.vertex_count,
            self.face_count,
            self.bbox_size[0],
            self.bbox_size[1],
            self.bbox_size[2],
            self.degenerate_faces,
            self.max_edge_length,
            warning_text,
        )


def analyze_mesh(vertices, faces):
    vertices = np.asarray(vertices, dtype=np.float32)
    faces = np.asarray(faces, dtype=np.int32)
    warnings = []

    finite = bool(np.isfinite(vertices).all()) and bool(np.isfinite(faces).all())
    if not finite:
        warnings.append("non-finite values")

    vertex_count = int(vertices.shape[0]) if vertices.ndim >= 1 else 0
    face_count = int(faces.shape[0]) if faces.ndim >= 1 else 0
    if vertex_count == 0:
        warnings.append("no vertices")
    if face_count == 0:
        warnings.append("no faces")

    degenerate = 0
    max_edge = 0.0
    if vertex_count > 0 and face_count > 0 and faces.ndim == 2 and faces.shape[1] == 3:
        valid_indices = (faces >= 0).all() and (faces < vertex_count).all()
        if not valid_indices:
            warnings.append("face indices out of range")
        else:
            tri = vertices[faces]
            e01 = np.linalg.norm(tri[:, 0] - tri[:, 1], axis=1)
            e12 = np.linalg.norm(tri[:, 1] - tri[:, 2], axis=1)
            e20 = np.linalg.norm(tri[:, 2] - tri[:, 0], axis=1)
            max_edge = float(max(e01.max(initial=0.0), e12.max(initial=0.0), e20.max(initial=0.0)))
            areas = np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1) * 0.5
            degenerate = int(np.count_nonzero(areas < 1e-5))
            if degenerate > 0:
                warnings.append("{} degenerate faces".format(degenerate))

    if vertex_count > 0:
        mins = vertices.min(axis=0)
        maxs = vertices.max(axis=0)
        diff = maxs - mins
        bbox = (float(diff[0]), float(diff[1]), float(diff[2]))
        if max(bbox) < 5.0:
            warnings.append("mesh is suspiciously tiny")
        if max(bbox) > 5000.0:
            warnings.append("mesh is suspiciously huge")
    else:
        bbox = (0.0, 0.0, 0.0)

    status = "ok" if not warnings else "warning"
    if not finite or vertex_count == 0 or face_count == 0:
        status = "bad"

    return MeshQualityReport(
        vertex_count=vertex_count,
        face_count=face_count,
        finite=finite,
        degenerate_faces=degenerate,
        max_edge_length=max_edge,
        bbox_size=bbox,
        status=status,
        warnings=warnings,
    )


def load_obj_counts(path):
    vertices = []
    faces = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if line.startswith("v "):
            parts = line.split()
            vertices.append([float(parts[1]), float(parts[2]), float(parts[3])])
        elif line.startswith("f "):
            parts = line.split()
            faces.append([int(part.split("/")[0]) - 1 for part in parts[1:4]])
    return np.asarray(vertices, dtype=np.float32), np.asarray(faces, dtype=np.int32)


def audit_capture_folder(path):
    path = Path(path)
    reports = []
    for obj_path in sorted(path.rglob("*.obj")):
        vertices, faces = load_obj_counts(obj_path)
        reports.append((obj_path, analyze_mesh(vertices, faces)))
    return reports
