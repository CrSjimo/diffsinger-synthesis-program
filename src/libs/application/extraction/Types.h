#ifndef DSSP_EXTRACTION_TYPES_H
#define DSSP_EXTRACTION_TYPES_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <dsspextraction/ExtractionError.h>

namespace dssp::extraction {
    enum class Kind { Note, Pitch, Tempo, Separation };
    struct ExtractorInfo {
        std::string id;
        std::string name;
        std::uint32_t sampleRate = 0;
    };
    struct ExtractorList {
        std::vector<ExtractorInfo> note;
        std::vector<ExtractorInfo> pitch;
    };
    struct Input {
        std::string extractor;
        std::string audioUrl;
    };
    struct ExtractedNote {
        double gap = 0;
        double duration = 0;
        std::uint32_t cent = 0;
    };
    struct NoteOutput {
        std::vector<ExtractedNote> notes;
    };
    struct PitchSegment {
        std::uint64_t gap = 0;
        std::vector<double> pitch;
    };
    struct PitchOutput {
        std::vector<PitchSegment> segments;
        double sampleRate = 0;
    };
    template <class T> using Completion = std::function<void(ExtractionResult<T>)>;
}

#endif // DSSP_EXTRACTION_TYPES_H
