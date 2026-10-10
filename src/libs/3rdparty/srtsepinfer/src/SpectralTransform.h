#ifndef SRTSEPINFER_SPECTRALTRANSFORM_H
#define SRTSEPINFER_SPECTRALTRANSFORM_H

#include <complex>
#include <functional>
#include <span>
#include <vector>

#include <srtsepinfer/Separation/SeparationError.h>
#include <synthrt/Support/Expected.h>

namespace srtsepinfer::demucs {
    /// HTDemucs's normalized, periodic-Hann STFT, including its two-frame crop
    /// and dropped Nyquist bin. Storage is [channel, real/imag, bin, frame].
    class SpectralTransform {
    public:
        explicit SpectralTransform(int fftSize = 4096, int hopLength = 1024);
        srt::Expected<std::vector<float>> forward(std::span<const float> planar, int channels, const std::function<bool()> &cancelled = {}) const;
        srt::Expected<std::vector<float>> inverse(std::span<const float> spectrum, int channels, std::size_t length, const std::function<bool()> &cancelled = {}) const;

    private:
        void fft(std::vector<std::complex<double>> &, bool inverse) const;
        int size_, hop_;
        std::vector<double> window_;
        std::vector<std::size_t> reversed_;
        std::vector<std::complex<double>> roots_;
    };
}
#endif
