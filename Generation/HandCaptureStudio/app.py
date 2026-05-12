# pyright: reportImplicitRelativeImport=false
import argparse
from pathlib import Path

from handcapture.export import export_capture
from handcapture.gui import HandCaptureApp
from handcapture.preprocess import prepare_image_file
from handcapture.quality import audit_capture_folder
from handcapture.runtime import MobileHandRuntime, ROOT


def run_sample_smoke(dataset, capture_dir):
    sample = ROOT / "mobilehand" / "data" / ("stb_SK_color_0.png" if dataset == "stb" else "freihand_00000000.jpg")
    runtime = MobileHandRuntime(dataset=dataset, use_cuda=False)
    prepared = prepare_image_file(sample)
    prediction = runtime.predict_tensor(prepared.tensor)
    obj_path, json_path = export_capture(capture_dir, prediction, prefix="smoke_sample")
    print("sample smoke ok")
    print(obj_path)
    print(json_path)


def main():
    parser = argparse.ArgumentParser(description="Hand Capture Studio")
    parser.add_argument("--capture-dir", default=str(Path(__file__).resolve().parent / "captures"))
    parser.add_argument("--dataset", choices=("freihand", "stb"), default="freihand")
    parser.add_argument("--sample-smoke", action="store_true", help="Run one sample prediction and export OBJ/JSON without GUI")
    parser.add_argument("--gui-smoke-frames", type=int, default=0, help="Open GUI, process N camera frames, then close")
    parser.add_argument("--audit-captures", action="store_true", help="Analyze OBJ captures for obvious mesh artifacts")
    args = parser.parse_args()

    capture_dir = Path(args.capture_dir)
    if args.audit_captures:
        reports = audit_capture_folder(capture_dir)
        if not reports:
            print("No OBJ captures found in {}".format(capture_dir))
            return
        bad = 0
        for obj_path, report in reports:
            if not report.ok:
                bad += 1
            print("{}\n{}\n".format(obj_path, report.summary()))
        print("Audit complete: {} file(s), {} warning/bad.".format(len(reports), bad))
        return
    if args.sample_smoke:
        run_sample_smoke(args.dataset, capture_dir)
        return

    app = HandCaptureApp(capture_dir=capture_dir, smoke_frames=args.gui_smoke_frames)
    if args.gui_smoke_frames > 0:
        app.after(100, app.start)
    app.mainloop()


if __name__ == "__main__":
    main()
