#ifndef SRTBEATINFER_BEATFEATURES_H
#define SRTBEATINFER_BEATFEATURES_H
#include <complex>
#include <functional>
#include <span>
#include <vector>
#include <srtbeatinfer/Api/Beat/1/BeatApiL1.h>
namespace srtbeatinfer::beatthis {
    /// Official 22050 Hz, FFT 1024, hop 441, Slaney 128-bin log-mel frontend.
    class Features {
    public:
        Features();
        srt::Expected<std::vector<float>> compute(std::span<const float>, const std::function<bool()> &cancelled = {}) const;
    private:
        std::vector<float> window_, filter_;
        std::vector<std::size_t> reversed_;
        std::vector<std::complex<double>> roots_;
    };
    std::vector<Api::Beat::L1::Beat> postprocess(std::span<const float> beat, std::span<const float> downbeat, double startTime, double duration);
    std::vector<std::ptrdiff_t> chunkStarts(std::size_t frames, int chunk = 1500, int border = 6);
}
#endif
