#include "BeatFeatures.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace srtbeatinfer::beatthis {
    namespace {
        constexpr int Size = 1024, Hop = 441, Bins = 513, Mels = 128;

        double hzToMel(double hz) { return hz < 1000 ? hz / (200.0 / 3) : 15 + std::log(hz / 1000) / (std::log(6.4) / 27); }

        double melToHz(double mel) { return mel < 15 ? mel * (200.0 / 3) : 1000 * std::exp((mel - 15) * (std::log(6.4) / 27)); }

        std::vector<double> peaks(std::span<const float> logits) {
            std::vector<double> result;
            int count = 0;

            for (std::size_t i = 0; i < logits.size(); ++i) {
                if (logits[i] <= 0) continue;

                bool maximum = true;
                for (auto j = i > 3 ? i - 3 : 0; j < std::min(logits.size(), i + 4); ++j)
                    if (logits[j] > logits[i]) {
                        maximum = false;
                        break;
                    }

                if (!maximum) continue;

                // Match upstream deduplicate_peaks: compare with the running mean.
                if (!result.empty() && i - result.back() <= 1) {
                    result.back() += (i - result.back()) / ++count;
                } else {
                    result.push_back(static_cast<double>(i));
                    count = 1;
                }
            }

            return result;
        }
    }

    Features::Features() : m_window(Size), m_filter(Bins * Mels), m_reversed(Size), m_roots(Size / 2) {
        for (int i = 0; i < Size; ++i) {
            m_window[i] = static_cast<float>(0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / Size));

            std::size_t value = i, reversed = 0;
            for (int n = Size; n > 1; n >>= 1) {
                reversed = (reversed << 1) | (value & 1);
                value >>= 1;
            }
            m_reversed[i] = reversed;
        }

        for (int i = 0; i < Size / 2; ++i) m_roots[i] = std::polar(1.0, -2 * std::numbers::pi * i / Size);

        // torchaudio uses float32 linspace and triangular filters, with no area normalization.
        std::vector<float> points(Mels + 2);
        const auto minimum = hzToMel(30), maximum = hzToMel(11000);
        for (int m = 0; m < Mels + 2; ++m) points[m] = static_cast<float>(melToHz(static_cast<float>(minimum + (maximum - minimum) * m / (Mels + 1))));

        for (int bin = 0; bin < Bins; ++bin) {
            const auto frequency = static_cast<float>(11025.0 * bin / (Bins - 1));
            for (int mel = 0; mel < Mels; ++mel)
                m_filter[bin * Mels + mel] = std::max(0.0f, std::min((frequency - points[mel]) / (points[mel + 1] - points[mel]), (points[mel + 2] - frequency) / (points[mel + 2] - points[mel + 1])));
        }
    }

    srt::Expected<std::vector<float>> Features::compute(std::span<const float> audio, const std::function<bool()> &cancelled) const {
        if (audio.empty()) return srt::Error(srt::Error::InvalidArgument, "Beat frontend requires nonempty audio");

        const auto frames = audio.size() / Hop + 1;
        std::vector<float> output(frames * Mels), magnitude(Bins);
        std::vector<std::complex<double>> values(Size);

        for (std::size_t frame = 0; frame < frames; ++frame) {
            if (cancelled && cancelled()) return cancelledError();

            for (int i = 0; i < Size; ++i) {
                auto index = static_cast<std::ptrdiff_t>(frame * Hop) + i - Size / 2;

                // Extend reflection to very short clips, where torch reflect padding cannot operate.
                if (audio.size() == 1) index = 0;
                else {
                    const auto period = 2 * static_cast<std::ptrdiff_t>(audio.size() - 1);
                    index = (index % period + period) % period;
                    if (index >= static_cast<std::ptrdiff_t>(audio.size())) index = period - index;
                }

                values[m_reversed[i]] = static_cast<double>(audio[index]) * m_window[i];
            }

            for (int span = 2; span <= Size; span *= 2) {
                const int half = span / 2, step = Size / span;
                for (int begin = 0; begin < Size; begin += span)
                    for (int j = 0; j < half; ++j) {
                        const auto even = values[begin + j], odd = values[begin + j + half] * m_roots[j * step];
                        values[begin + j] = even + odd;
                        values[begin + j + half] = even - odd;
                    }
            }

            for (int bin = 0; bin < Bins; ++bin) magnitude[bin] = static_cast<float>(std::abs(values[bin]) / 32);

            for (int mel = 0; mel < Mels; ++mel) {
                double sum = 0;
                for (int bin = 0; bin < Bins; ++bin) sum += magnitude[bin] * m_filter[bin * Mels + mel];
                output[frame * Mels + mel] = static_cast<float>(std::log1p(1000 * sum));
                if (!std::isfinite(output[frame * Mels + mel])) return srt::Error(srt::Error::InvalidArgument, "Beat features contain nonfinite values");
            }
        }

        return output;
    }

    std::vector<std::ptrdiff_t> chunkStarts(std::size_t frames, int chunk, int border) {
        std::vector<std::ptrdiff_t> starts;

        for (std::ptrdiff_t start = -border; start < static_cast<std::ptrdiff_t>(frames) - border; start += chunk - 2 * border) starts.push_back(start);
        if (frames > static_cast<std::size_t>(chunk - 2 * border)) starts.back() = static_cast<std::ptrdiff_t>(frames) - (chunk - border);

        return starts;
    }

    std::vector<Api::Beat::L1::Beat> postprocess(std::span<const float> beat, std::span<const float> downbeat, double startTime, double duration) {
        const auto beats = peaks(beat), downbeats = peaks(downbeat);
        std::vector<Api::Beat::L1::Beat> result;
        for (const auto frame : beats) result.push_back({startTime + frame / 50, false});

        for (const auto frame : downbeats) {
            if (beats.empty()) break;

            // Upstream argmin resolves ties to the first (earlier) beat.
            const auto it = std::min_element(beats.begin(), beats.end(), [frame](double a, double b) { return std::abs(a - frame) < std::abs(b - frame); });
            result[it - beats.begin()].downbeat = true;
        }

        std::erase_if(result, [=](const auto &value) { return value.time > startTime + duration; });

        return result;
    }
}
