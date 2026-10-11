#include "Slicing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace dssp::extraction {
    namespace {
        std::size_t minimum(std::span<const double> levels, std::size_t begin, std::size_t end) {
            const auto range = levels.subspan(begin, end - begin);
            return begin + static_cast<std::size_t>(std::ranges::min_element(range) - range.begin());
        }
    }

    ExtractionResult<std::vector<SampleSpan>> sliceAudio(std::span<const float> samples, int sampleRate, double maximumDuration, double outputInterval, double threshold, std::stop_token stop) {
        if (samples.empty() || sampleRate <= 0 || !std::isfinite(threshold) || threshold < 0 || !std::isfinite(maximumDuration) || maximumDuration < 0 || !std::isfinite(outputInterval) || outputInterval < 0) {
            return fail<ExtractionErrors>(OperationError("Invalid audio slicing configuration"));
        }

        const auto hop = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(0.010 * sampleRate)));
        const auto window = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(0.040 * sampleRate)));
        const auto frames = (samples.size() - 1) / hop + 1;
        const auto minLength = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(2.0 * sampleRate)) / hop);
        const auto minInterval = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(0.300 * sampleRate)) / hop);
        const auto maxSilence = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(0.500 * sampleRate)) / hop);
        std::vector<SampleSpan> spans;
        if (frames > minLength && threshold > 0) {
            std::vector<double> levels;
            levels.reserve(frames);
            // Rolling sums give the same fixed-window RMS without revisiting
            // every PCM sample for each overlapping window.
            std::size_t left = 0, right = 0;
            double energy = 0;
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (stop.stop_requested()) {
                    return fail<ExtractionErrors>(CancelledError("Audio slicing was cancelled"));
                }

                const auto center = frame * hop;
                const auto begin = center > window / 2 ? center - window / 2 : 0;
                const auto end = center + std::min(window - window / 2, samples.size() - center);
                while (right < end) {
                    const double value = samples[right++];
                    energy += value * value;
                }
                while (left < begin) {
                    const double value = samples[left++];
                    energy -= value * value;
                }
                levels.push_back(std::sqrt(std::max(0.0, energy) / static_cast<double>(window)));
            }

            std::vector<SampleSpan> silences;
            std::optional<std::size_t> silenceStart;
            std::size_t clipStart = 0;
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (levels[frame] < threshold) {
                    if (!silenceStart) silenceStart = frame;
                    continue;
                }
                if (!silenceStart) continue;
                const auto begin = *silenceStart;
                const bool leading = begin == 0 && frame > maxSilence;
                const bool worthCutting = frame - begin >= minInterval && frame - clipStart >= minLength;
                if (leading || worthCutting) {
                    if (frame - begin <= maxSilence) {
                        const auto at = minimum(levels, begin, frame + 1);
                        silences.push_back({begin == 0 ? 0 : at, at});
                        clipStart = at;
                    } else {
                        const auto before = minimum(levels, begin, begin + maxSilence + 1);
                        const auto after = minimum(levels, frame - maxSilence, frame + 1);
                        silences.push_back({begin == 0 ? 0 : before, after});
                        clipStart = after;
                    }
                }
                silenceStart.reset();
            }

            if (silenceStart && frames - *silenceStart >= minInterval) {
                const auto at = minimum(levels, *silenceStart, std::min(frames, *silenceStart + maxSilence + 1));
                silences.push_back({at, frames});
            }

            std::size_t begin = 0;
            for (const auto &silence : silences) {
                const auto end = std::min(samples.size(), silence.begin * hop);
                if (end > begin) spans.push_back({begin, end});
                begin = std::min(samples.size(), silence.end * hop);
            }

            // Preserve the final partial hop unless an actual trailing silence
            // was selected. Analysis frames are not the PCM length.
            if (begin < samples.size() && (silences.empty() || silences.back().end < frames)) {
                spans.push_back({begin, samples.size()});
            }
        }

        if (spans.empty()) spans.push_back({0, samples.size()});

        const double grid = outputInterval > 0 ? outputInterval * sampleRate : 1.0;
        if (!std::isfinite(grid) || grid <= 0 || static_cast<double>(samples.size()) / grid > static_cast<double>(std::numeric_limits<std::int64_t>::max() / 2)) {
            return fail<ExtractionErrors>(OperationError("Output frame grid cannot represent this audio"));
        }

        const auto boundary = [&](std::uint64_t index) {
            return static_cast<std::size_t>(std::clamp(std::round(static_cast<double>(index) * grid), 0.0, static_cast<double>(samples.size())));
        };

        const double limit = maximumDuration > 0 ? std::floor(maximumDuration * sampleRate) : static_cast<double>(samples.size());
        if (!std::isfinite(limit) || limit < 1) {
            return fail<ExtractionErrors>(OperationError("Analyzer maximum duration does not allow a PCM frame"));
        }

        const auto maxUnits = static_cast<std::uint64_t>(std::floor(std::min(limit, static_cast<double>(samples.size())) / grid + 1e-10));
        std::vector<SampleSpan> result;
        for (const auto &span : spans) {
            if (stop.stop_requested()) return fail<ExtractionErrors>(CancelledError("Audio slicing was cancelled"));
            const auto first = static_cast<std::uint64_t>(std::llround(static_cast<double>(span.begin) / grid));
            auto last = static_cast<std::uint64_t>(std::llround(static_cast<double>(span.end) / grid));
            if (span.end == samples.size()) last = static_cast<std::uint64_t>(std::ceil(static_cast<double>(span.end) / grid));
            const auto begin = boundary(first);
            const auto end = span.end == samples.size() ? samples.size() : boundary(last);
            if (end <= begin) continue;
            if (static_cast<double>(end - begin) <= limit) {
                result.push_back({begin, end});
                continue;
            }
            if (!maxUnits || last <= first) {
                return fail<ExtractionErrors>(OperationError("Analyzer maximum duration cannot accommodate an aligned output frame"));
            }

            const auto units = last - first;
            const auto pieces = (units - 1) / maxUnits + 1;
            auto at = begin;
            for (std::uint64_t piece = 1; piece <= pieces; ++piece) {
                // Quotient/remainder arithmetic avoids multiplication overflow.
                const auto offset = (units / pieces) * piece + std::min(piece, units % pieces);
                const auto to = piece == pieces ? end : boundary(first + offset);
                if (to <= at || static_cast<double>(to - at) > limit) {
                    return fail<ExtractionErrors>(OperationError("Cannot partition audio within the analyzer duration limit"));
                }
                result.push_back({at, to});
                at = to;
            }
        }

        if (result.empty()) return fail<ExtractionErrors>(OperationError("Audio slicing produced no nonempty spans"));
        return result;
    }
}
