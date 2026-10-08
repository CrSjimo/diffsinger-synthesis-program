#ifndef DSSP_EXTRACTION_RESULTS_H
#define DSSP_EXTRACTION_RESULTS_H

#include <utility>

#include <dsspextraction/OtterTypes.h>
#include <dsspextraction/Types.h>

namespace dssp::extraction {
    std::expected<NoteOutput, OperationError> noteOutput(std::vector<otter::Api::Note::L1::NoteInfo>, double duration);
    class PitchBuilder {
    public:
        explicit PitchBuilder(double interval) : interval_(interval) { output_.sampleRate = 1.0 / interval; }
        std::expected<void, OperationError> append(const otter::Api::F0::L1::F0Result &, double spanStart, double spanEnd, int audioSampleRate);
        PitchOutput take() { return std::move(output_); }

    private:
        double interval_;
        std::uint64_t consumedEnd_ = 0;
        std::uint64_t voicedEnd_ = 0;
        PitchOutput output_;
    };
}

#endif // DSSP_EXTRACTION_RESULTS_H
