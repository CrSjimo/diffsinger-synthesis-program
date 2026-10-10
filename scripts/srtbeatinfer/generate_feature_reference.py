"""Regenerate the independent Torch frontend probes used by BeatTest."""
from pathlib import Path
import math
import torch
from beat_this.preprocessing import LogMelSpect

audio = [math.sin(i * 0.043) * 0.2 + ((i * 17) % 31 - 15) * 0.003 for i in range(2049)]
with torch.inference_mode():
    spect = LogMelSpect()(torch.tensor(audio, dtype=torch.float32)).flatten()
probes = sorted(set([f * 128 + m for f in range(5) for m in (0, 1, 5, 10, 20, 40, 70, 100, 126, 127)]))
header = '''// Generated independently with beat_this.preprocessing.LogMelSpect (Torch).
#ifndef DSSP_TESTS_BEATFEATUREREFERENCE_H
#define DSSP_TESTS_BEATFEATUREREFERENCE_H
#include <array>
#include <cmath>
#include <utility>
#include <vector>
namespace dssp::tests {
inline std::vector<float> beatReferenceAudio() {
    std::vector<float> audio(2049);
    for (int i = 0; i < 2049; ++i) audio[i] = static_cast<float>(std::sin(i * 0.043) * 0.2 + ((i * 17) % 31 - 15) * 0.003);
    return audio;
}
inline constexpr std::array BeatReferenceProbes{
'''
header += ",\n".join(f"    std::pair<std::size_t, float>{{{i}, {spect[i].item():.9g}f}}" for i in probes)
header += "\n};\n}\n#endif\n"
Path("src/tests/BeatFeatureReference.h").write_text(header, encoding="utf-8")
