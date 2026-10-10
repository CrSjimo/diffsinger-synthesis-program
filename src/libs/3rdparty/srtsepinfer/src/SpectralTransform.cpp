#include "SpectralTransform.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace srtsepinfer::demucs {
    SpectralTransform::SpectralTransform(int fftSize, int hopLength) : size_(fftSize), hop_(hopLength) {
        if (size_ < 4 || (size_ & (size_ - 1)) || hop_ != size_ / 4)
            throw std::invalid_argument("Demucs requires a power-of-two FFT and quarter-window hop");
        window_.resize(size_);
        reversed_.resize(size_);
        roots_.resize(size_ / 2);
        for (int i = 0; i < size_; ++i) {
            window_[i] = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / size_);
            std::size_t value = i, reversed = 0;
            for (int n = size_; n > 1; n >>= 1) {
                reversed = (reversed << 1) | (value & 1);
                value >>= 1;
            }
            reversed_[i] = reversed;
        }
        for (int i = 0; i < size_ / 2; ++i)
            roots_[i] = std::polar(1.0, -2 * std::numbers::pi * i / size_);
    }
    void SpectralTransform::fft(std::vector<std::complex<double>> &values, bool inverse) const {
        for (std::size_t i = 0; i < values.size(); ++i)
            if (i < reversed_[i])
                std::swap(values[i], values[reversed_[i]]);
        for (int span = 2; span <= size_; span *= 2) {
            const int half = span / 2, step = size_ / span;
            for (int begin = 0; begin < size_; begin += span) {
                for (int j = 0; j < half; ++j) {
                    const auto root = inverse ? std::conj(roots_[j * step]) : roots_[j * step];
                    const auto even = values[begin + j], odd = values[begin + j + half] * root;
                    values[begin + j] = even + odd;
                    values[begin + j + half] = even - odd;
                }
            }
        }
        if (inverse)
            for (auto &value : values)
                value /= size_;
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
        std::vector<std::complex<double>> scratch(size_);
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
                    scratch[i] = static_cast<double>(planar[channel * length + index]) * window_[i];
                }
                fft(scratch, false);
                for (std::size_t bin = 0; bin < bins; ++bin) {
                    result[((channel * 2) * bins + bin) * frames + frame] = static_cast<float>(scratch[bin].real() * scale);
                    result[((channel * 2 + 1) * bins + bin) * frames + frame] = static_cast<float>(scratch[bin].imag() * scale);
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
        std::vector<std::complex<double>> scratch(size_);
        const double scale = std::sqrt(static_cast<double>(size_));
        for (int channel = 0; channel < channels; ++channel) {
            std::fill(accumulated.begin(), accumulated.end(), 0);
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (cancelled && cancelled())
                    return cancelledError();
                for (std::size_t bin = 0; bin < bins; ++bin) {
                    scratch[bin] = {spectrum[((channel * 2) * bins + bin) * frames + frame], spectrum[((channel * 2 + 1) * bins + bin) * frames + frame]};
                    if (bin)
                        scratch[size_ - bin] = std::conj(scratch[bin]);
                }
                scratch[bins] = 0; // The model never predicts Nyquist.
                fft(scratch, true);
                for (int i = 0; i < size_; ++i)
                    accumulated[(frame + 2) * hop_ + i] += scratch[i].real() * scale * window_[i];
            }
            for (std::size_t i = 0; i < length; ++i)
                result[channel * length + i] = static_cast<float>(accumulated[trim + i] / weights[trim + i]);
        }
        return result;
    }
}
