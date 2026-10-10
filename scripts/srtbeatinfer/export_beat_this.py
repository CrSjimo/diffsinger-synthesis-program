"""Export the user's local Beat This checkpoint; never downloads model weights."""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import shutil
import zipfile

import numpy as np
import onnx
import onnxruntime as ort
import torch
from beat_this.inference import load_model
from rotary_embedding_torch import RotaryEmbedding


def sha(path):
    return hashlib.file_digest(open(path, "rb"), "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("--output", type=Path, default=Path("data/beat-this"))
    parser.add_argument("--report", type=Path, default=Path("data/docs/srtbeatinfer/conversion.json"))
    args = parser.parse_args()
    torch.set_num_threads(4)
    torch.manual_seed(20261010)
    package = args.output / "package"
    (package / "models").mkdir(parents=True, exist_ok=True)
    (package / "inferences").mkdir(exist_ok=True)
    model = load_model(str(args.checkpoint)).eval()
    for module in model.modules():
        if isinstance(module, RotaryEmbedding):
            module.cache_if_possible = False  # Keep sequence positions dynamic in ONNX.

    class Core(torch.nn.Module):
        def __init__(self, model):
            super().__init__()
            self.model = model

        def forward(self, spectrogram):
            output = self.model(spectrogram)
            return output["beat"], output["downbeat"]

    core = Core(model).eval()
    target = package / "models/beat_this_final0.onnx"
    example = torch.rand(1, 64, 128) * 5
    with torch.inference_mode():
        torch.onnx.export(core, example, str(target), opset_version=17,
                          input_names=["spectrogram"], output_names=["beat", "downbeat"],
                          dynamic_axes={"spectrogram": {1: "frames"}, "beat": {1: "frames"}, "downbeat": {1: "frames"}},
                          do_constant_folding=True, dynamo=False)
    onnx.checker.check_model(str(target), full_check=True)
    options = ort.SessionOptions()
    options.intra_op_num_threads = 4
    session = ort.InferenceSession(str(target), options, providers=["CPUExecutionProvider"])
    checks = []
    for frames in (13, 87, 1500):
        x = torch.rand(1, frames, 128) * 5
        with torch.inference_mode():
            expected = core(x)
        actual = session.run(None, {"spectrogram": x.numpy()})
        errors = []
        for reference, output in zip(expected, actual):
            diff = output - reference.numpy()
            errors.append({"maxAbs": float(np.max(np.abs(diff))), "rmse": float(np.sqrt(np.mean(diff**2)))})
            np.testing.assert_allclose(output, reference.numpy(), atol=3e-4, rtol=3e-4)
        checks.append({"frames": frames, "outputs": errors})
        print(checks[-1], flush=True)
    distribution = importlib.metadata.distribution("beat-this")
    license_file = next(p for p in distribution.files if p.name == "LICENSE")
    shutil.copyfile(distribution.locate_file(license_file), package / "LICENSE.beat-this.txt")
    declaration = {"name": {"_": "Beat This! final0", "zh-CN": "Beat This! 节拍与强拍提取"},
                   "interface": "org.openvpi.srtbeatinfer.inference.Beat", "variant": "beat-this", "level": 1,
                   "exports": {"sampleRate": 22050, "channelCount": 1, "maxSegmentDuration": 0, "downbeats": True},
                   "configuration": {"model": "../models/beat_this_final0.onnx", "chunkFrames": 1500, "borderFrames": 6}}
    descriptor = {"$version": "1.0", "id": "srtbeatinfer/beat-this", "version": "1.0.0", "runtimeLevel": 1,
                  "name": "SynthRT Beat This!", "vendor": "Institute of Computational Perception, JKU Linz, Austria",
                  "license": "LICENSE.beat-this.txt", "description": "Beat This! final0: beat and downbeat tracking with official minimal postprocessing.",
                  "contributions": {"inference": [{"id": "final0", "path": "inferences/final0.json"}]}}
    (package / "inferences/final0.json").write_text(json.dumps(declaration, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (package / "desc.json").write_text(json.dumps(descriptor, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    archive = args.output / "srtbeatinfer-beat-this-1.0.0.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for path in sorted(package.rglob("*")):
            if path.is_file():
                z.write(path, path.relative_to(package).as_posix())
    report = {"checkpoint": str(args.checkpoint), "checkpointSha256": sha(args.checkpoint), "onnxSha256": sha(target),
              "packageSha256": sha(archive), "packageBytes": archive.stat().st_size, "opset": 17,
              "versions": {name: importlib.metadata.version(name) for name in ("torch", "torchaudio", "beat-this", "rotary-embedding-torch", "onnx", "onnxruntime")},
              "checks": checks}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
