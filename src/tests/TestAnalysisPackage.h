#ifndef DSSP_TESTS_TESTANALYSISPACKAGE_H
#define DSSP_TESTS_TESTANALYSISPACKAGE_H

#include <string>

#include <dssptests/TestFiles.h>

namespace dssp::tests {
    // Synthrt normalizes the manifest version "1" to "1.0" in public references.
    inline const std::string analysisNoteId = "example/analysis=1.0:inference/note";
    inline const std::string analysisPitchId = "example/analysis=1.0:inference/pitch";

    // Real provider declarations with absent model files prove discovery stays cold.
    inline void writeAnalysisPackage(const TestFiles &files, const std::filesystem::path &root, std::string version = "1") {
        files.write(root / "desc.json", R"({"$version":"1.0","id":"example/analysis","version":")" + version + R"(","runtimeLevel":1,"contributions":{"inference":[{"id":"note","path":"note.json"},{"id":"pitch","path":"pitch.json"}]}})");
        files.write(root / "note.json", R"({"name":{"_":"Notes","en-US":"Notes","zh-CN":"音符"},"interface":"org.openvpi.otter.inference.Note","variant":"game","level":1,
            "exports":{"sampleRate":44100,"maxSegmentDuration":0.05},
            "configuration":{"encoder":"missing-encoder.onnx","segmenter":"missing-segmenter.onnx","estimator":"missing-estimator.onnx","boundaryToDuration":"missing-boundary.onnx"}})");
        files.write(root / "pitch.json", R"({"name":"Pitch","interface":"org.openvpi.otter.inference.F0","variant":"rmvpe","level":1,
            "exports":{"sampleRate":16000,"interval":0.01,"maxSegmentDuration":0.04},"configuration":{"model":"missing-pitch.onnx"}})");
    }
}

#endif // DSSP_TESTS_TESTANALYSISPACKAGE_H
