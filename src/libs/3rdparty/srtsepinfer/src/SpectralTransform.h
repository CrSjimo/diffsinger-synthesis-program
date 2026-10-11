#ifndef SRTSEPINFER_SPECTRALTRANSFORM_H
#define SRTSEPINFER_SPECTRALTRANSFORM_H

#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <srtsepinfer/Separation/SeparationError.h>
#include <synthrt/Support/Expected.h>

struct PFFFT_Setup;

namespace srtsepinfer::demucs {
    /// HTDemucs's normalized, periodic-Hann STFT, including its two-frame crop
    /// and dropped Nyquist bin. Storage is [channel, real/imag, bin, frame].
    class SpectralTransform {
    public:
        explicit SpectralTransform(int fftSize = 4096, int hopLength = 1024);
        srt::Expected<std::vector<float>> forward(std::span<const float> planar, int channels, const std::function<bool()> &cancelled = {}) const;
        srt::Expected<std::vector<float>> inverse(std::span<const float> spectrum, int channels, std::size_t length, const std::function<bool()> &cancelled = {}) const;

    private:
        struct SetupDeleter {
            void operator()(PFFFT_Setup *) const;
        };

        int size_, hop_;
        std::vector<double> window_;
        std::unique_ptr<PFFFT_Setup, SetupDeleter> setup_;
    };
}

#endif
