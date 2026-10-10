"""Replace exported HTDemucs GLU Split nodes with equivalent constant-bound Slices.

Works on local ONNX files; does not download weights or edit package metadata.
Workaround for https://github.com/microsoft/onnxruntime/issues/32146.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import onnx
from onnx import helper, numpy_helper


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def replace_glu_splits(model):
    """Rewrite only the two-way, channel-axis GLUs emitted by this HTDemucs export."""
    if next(item.version for item in model.opset_import if not item.domain) < 13:
        raise ValueError("Expected HTDemucs ONNX opset >= 13")
    producers = {name: node for node in model.graph.node for name in node.output}
    initializers = {tensor.name: tensor for tensor in model.graph.initializer}
    consumers = {}
    for node in model.graph.node:
        for name in node.input:
            consumers.setdefault(name, []).append(node)
    graph_outputs = {item.name for item in model.graph.output}
    names = set(producers) | set(initializers) | {item.name for item in model.graph.input}
    constants = {}
    added = []

    def constant(name):
        if name in initializers:
            return initializers[name]
        node = producers.get(name)
        if node is not None and node.op_type == "Identity":
            return constant(node.input[0])
        return None

    def channels_for(name):
        node = producers.get(name)
        if node is None:
            raise ValueError(f"Cannot determine GLU channels for {name}")
        if node.op_type == "Conv":
            weight = constant(node.input[1])
            if weight is not None:
                return weight.dims[0]
        if node.op_type == "Add":
            # Exported GroupNorm affine bias broadcasts [channels, 1] onto
            # [batch, channels, flattened spatial dimensions].
            for operand in node.input:
                bias = constant(operand)
                if bias is not None and len(bias.dims) == 2 and bias.dims[1] == 1:
                    return bias.dims[0]
        raise ValueError(f"Unsupported GLU input producer: {node.name} ({node.op_type})")

    def bound(value):
        if value not in constants:
            name = f"__srtsepinfer_glu_slice_{value}"
            if name in names:
                raise ValueError(f"New initializer would collide: {name}")
            names.add(name)
            constants[value] = name
            added.append(numpy_helper.from_array(np.array([value], dtype=np.int64), name))
        return constants[value]

    rewritten = []
    nodes = []
    for node in model.graph.node:
        if node.op_type != "Split":
            nodes.append(node)
            continue
        attributes = {item.name: helper.get_attribute_value(item) for item in node.attribute}
        if node.domain or len(node.input) != 1 or len(node.output) != 2 or attributes != {"axis": 1}:
            raise ValueError(f"Unexpected HTDemucs Split contract: {node.name}")
        a, b = node.output
        a_users, b_users = consumers.get(a, []), consumers.get(b, [])
        if a in graph_outputs or b in graph_outputs or len(a_users) != 1 or len(b_users) != 1:
            raise ValueError(f"Split is not a private two-input GLU: {node.name}")
        multiply, sigmoid = a_users[0], b_users[0]
        if (sigmoid.op_type != "Sigmoid" or multiply.op_type != "Mul"
                or len(sigmoid.output) != 1
                or set(multiply.input) != {a, sigmoid.output[0]}):
            raise ValueError(f"Split is not followed by Sigmoid and Mul: {node.name}")
        channels = channels_for(node.input[0])
        if channels <= 0 or channels % 2:
            raise ValueError(f"Invalid GLU channel count {channels}: {node.name}")
        half = channels // 2
        # Preserve the original output names and every downstream operand.
        # Separate single-output nodes avoid DML's multi-output Split aliasing.
        for suffix, output, start, end in [("a", a, 0, half), ("b", b, half, channels)]:
            nodes.append(helper.make_node("Slice", [node.input[0], bound(start), bound(end), bound(1)],
                                          [output], name=f"{node.name}_slice_{suffix}"))
        rewritten.append({"node": node.name, "channels": channels, "half": half})
    if len(rewritten) != 48:
        raise ValueError(f"Expected 48 HTDemucs GLUs, found {len(rewritten)}; no output written")
    del model.graph.node[:]
    model.graph.node.extend(nodes)
    model.graph.initializer.extend(added)
    return rewritten


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Directory containing the six original Demucs ONNX models")
    parser.add_argument("output", type=Path, help="Separate directory for the corrected models")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve():
        parser.error("Use a separate output directory to preserve the originals for validation")
    models = sorted(args.source.glob("*.onnx"))
    if len(models) != 6:
        parser.error(f"Expected six models, found {len(models)}")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"workaround": "GLU Split -> two Slice nodes", "models": []}
    for source in models:
        target = args.output / source.name
        model = onnx.load(source)
        weights = {item.name: item.SerializeToString() for item in model.graph.initializer}
        signature = [(item.name, item.SerializeToString()) for item in [*model.graph.input, *model.graph.output]]
        changed = replace_glu_splits(model)
        assert all(item.SerializeToString() == weights[item.name] for item in model.graph.initializer if item.name in weights)
        assert signature == [(item.name, item.SerializeToString()) for item in [*model.graph.input, *model.graph.output]]
        onnx.checker.check_model(model, full_check=True)
        onnx.save(model, target)
        entry = {"name": source.stem, "sourceSha256": sha256(source), "targetSha256": sha256(target),
                 "bytes": target.stat().st_size, "rewrittenGLUs": len(changed),
                 "weightsUnchanged": True, "signatureUnchanged": True, "nodes": changed}
        report["models"].append(entry)
        print(f"{source.stem}: {len(changed)} GLUs rewritten; ONNX checker passed", flush=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
