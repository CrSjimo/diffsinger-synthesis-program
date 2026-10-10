#include "SpectralTransform.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include <pffft/pffft.h>

namespace srtsepinfer::demucs {
    namespace {
        // PFFFT requires SIMD-aligned storage. Reuse these buffers across all
        // frames in a call; keep them local so the cached read-only plan can be
        // used concurrently without sharing mutable scratch space.
        auto alignedBuffer(int size) {
            std::unique_ptr<float, decltype(&pffft_aligned_free)> buffer(static_cast<float *>(pffft_aligned_malloc(size * sizeof(float))), &pffft_aligned_free);
            if (!buffer)
                throw std::bad_alloc();
            return buffer;
        }
    }
    void SpectralTransform::SetupDeleter::operator()(PFFFT_Setup *setup) const { pffft_destroy_setup(setup); }
    SpectralTransform::SpectralTransform(int fftSize, int hopLength) : size_(fftSize), hop_(hopLength) {
        if (size_ < 32 || (size_ & (size_ - 1)) || hop_ != size_ / 4)
            throw std::invalid_argument("Demucs requires a power-of-two FFT of at least 32 samples and quarter-window hop");
        setup_.reset(pffft_new_setup(size_, PFFFT_REAL));
        if (!setup_)
            throw std::bad_alloc();
        window_.resize(size_);
        for (int i = 0; i < size_; ++i)
            window_[i] = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / size_);
    }
    srt::Expected<std::vector<float>> SpectralTransform::forward(std::span<const float> planar, int channels, const std::function<bool()> &cancelled) const {
        if (channels <= 0 || planar.empty() || planar.size() % channels)
            return srt::Error(srt::Error::InvalidArgument, "Invalid planar audio size");
        const auto length = planar.size() / channels;
        if (length < static_cast<std::size_t>(size_))
            return srt::Error(srt::Error::InvalidArgument, "STFT requires padded model-length audio");
        const auto frames = (length + hop_ - 1) / hop_;
        const auto bins = static_cast<std::size_t>(size_ / 2);
        std::vector<float> result(channels * 2 * bins * frames);
        auto scratch = alignedBuffer(size_);
        auto work = alignedBuffer(size_);
        auto *values = scratch.get();
        const double scale = 1 / std::sqrt(static_cast<double>(size_));
        for (int channel = 0; channel < channels; ++channel) {
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (cancelled && cancelled())
                    return cancelledError();
                for (int i = 0; i < size_; ++i) {
                    auto index = static_cast<std::ptrdiff_t>(frame * hop_) - hop_ * 3 / 2 + i;
                    // Reflection excludes the edge sample, exactly as torch pad(mode=reflect).
                    if (index < 0)
                        index = -index;
                    if (index >= static_cast<std::ptrdiff_t>(length))
                        index = 2 * static_cast<std::ptrdiff_t>(length) - 2 - index;
                    values[i] = static_cast<float>(planar[channel * length + index] * window_[i]);
                }
                pffft_transform_ordered(setup_.get(), values, values, work.get(), PFFFT_FORWARD);
                // The real FFT packs DC and Nyquist into its first complex
                // pair. Demucs drops Nyquist and keeps an imaginary DC of zero.
                result[(channel * 2 * bins) * frames + frame] = static_cast<float>(values[0] * scale);
                result[((channel * 2 + 1) * bins) * frames + frame] = 0;
                for (std::size_t bin = 1; bin < bins; ++bin) {
                    result[((channel * 2) * bins + bin) * frames + frame] = static_cast<float>(values[2 * bin] * scale);
                    result[((channel * 2 + 1) * bins + bin) * frames + frame] = static_cast<float>(values[2 * bin + 1] * scale);
                }
            }
        }
        return result;
    }
    srt::Expected<std::vector<float>> SpectralTransform::inverse(std::span<const float> spectrum, int channels, std::size_t length, const std::function<bool()> &cancelled) const {
        const auto frames = (length + hop_ - 1) / hop_;
        const auto bins = static_cast<std::size_t>(size_ / 2);
        if (!length || channels <= 0 || spectrum.size() != channels * 2 * bins * frames)
            return srt::Error(SeparationError::ModelMismatch, "Invalid Demucs spectrum size");
        const auto fullLength = (frames + 3) * hop_ + size_;
        const auto trim = static_cast<std::size_t>(size_ / 2 + hop_ * 3 / 2);
        std::vector<double> weights(fullLength, 0), accumulated(fullLength);
        // Include the four zero-padded frames in the denominator, as torch.istft does.
        for (std::size_t frame = 0; frame < frames + 4; ++frame) {
            for (int i = 0; i < size_; ++i)
                weights[frame * hop_ + i] += window_[i] * window_[i];
        }
        std::vector<float> result(channels * length);
        auto scratch = alignedBuffer(size_);
        auto work = alignedBuffer(size_);
        auto *values = scratch.get();
        // PFFFT's backward transform is unnormalized. Together with the
        // normalized forward STFT this needs 1/sqrt(N), rather than sqrt(N).
        const double scale = 1 / std::sqrt(static_cast<double>(size_));
        for (int channel = 0; channel < channels; ++channel) {
            std::fill(accumulated.begin(), accumulated.end(), 0);
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (cancelled && cancelled())
                    return cancelledError();
                values[0] = spectrum[(channel * 2 * bins) * frames + frame];
                values[1] = 0; // The model never predicts Nyquist.
                for (std::size_t bin = 1; bin < bins; ++bin) {
                    values[2 * bin] = spectrum[((channel * 2) * bins + bin) * frames + frame];
                    values[2 * bin + 1] = spectrum[((channel * 2 + 1) * bins + bin) * frames + frame];
                }
                pffft_transform_ordered(setup_.get(), values, values, work.get(), PFFFT_BACKWARD);
                for (int i = 0; i < size_; ++i)
                    accumulated[(frame + 2) * hop_ + i] += values[i] * scale * window_[i];
            }
            for (std::size_t i = 0; i < length; ++i)
                result[channel * length + i] = static_cast<float>(accumulated[trim + i] / weights[trim + i]);
        }
        return result;
    }
}
