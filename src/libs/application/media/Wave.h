#ifndef DSSP_MEDIA_WAVE_H
#define DSSP_MEDIA_WAVE_H

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <stop_token>
#include <vector>

#include <dsspmedia/MediaError.h>

namespace dssp::media {
    // Encode generated native float32 PCM in memory. No upload or output file.
    std::expected<std::string, MediaError> waveDataUrl(std::span<const std::uint8_t> pcm, int sampleRate, double duration);
    struct WaveAudio {
        int sampleRate = 0;
        int channelCount = 0;
        std::vector<float> samples;
    };
    // Decode the actual container in memory; no file access or resampling.
    std::expected<WaveAudio, MediaInputErrors> readWaveDataUrl(std::string_view, std::stop_token = {});
}

#endif // DSSP_MEDIA_WAVE_H
