#ifndef DSSP_EXTRACTION_SLICING_H
#define DSSP_EXTRACTION_SLICING_H

#include <cstddef>
#include <span>
#include <stop_token>
#include <vector>

#include <dsspextraction/ExtractionError.h>

namespace dssp::extraction {
    struct SampleSpan {
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    // The editor's silence profile, followed by balanced hard-limit splitting.
    // A positive output interval additionally aligns cuts to the global F0 grid.
    ExtractionResult<std::vector<SampleSpan>> sliceAudio(std::span<const float> mono, int sampleRate, double maximumDuration, double outputInterval, double threshold, std::stop_token = {});
}

#endif // DSSP_EXTRACTION_SLICING_H
