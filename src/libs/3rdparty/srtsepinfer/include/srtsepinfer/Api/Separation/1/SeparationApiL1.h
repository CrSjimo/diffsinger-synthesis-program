#ifndef SRTSEPINFER_SEPARATIONAPIL1_H
#define SRTSEPINFER_SEPARATIONAPIL1_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <srtsepinfer/Separation/Provider/SeparationTask.h>
#include <synthrt/SVS/InferenceContrib.h>
#include <synthrt/Support/DisplayText.h>

namespace srtsepinfer::Api::Separation::L1 {
    inline constexpr char API_INTERFACE[] = "org.openvpi.srtsepinfer.inference.Separation";
    inline constexpr int API_LEVEL = 1;

    /// Interleaved finite float32 PCM. No decoding or resampling occurs in the library.
    struct AudioSegment {
        int sampleRate = 0;
        int channelCount = 0;
        std::vector<float> samples;
        double startTime = 0;
    };

    struct TrackInfo {
        std::string id; // Stable identifier, also returned in Track::id.
        srt::DisplayText description; // Human-readable meaning of the source.
    };

    class SRTSEPINFER_EXPORT SeparationSchema : public srt::ContribExports {
    public:
        explicit SeparationSchema(std::string variant);
        ~SeparationSchema() override;
        int sampleRate = 0;
        int channelCount = 0;
        double maxSegmentDuration = 0; // 0: the interpreter partitions arbitrary-length audio.
        std::vector<TrackInfo> tracks; // Public result order; providers map the model's source axis.
    };

    SRTSEPINFER_EXPORT srt::Expected<std::unique_ptr<SeparationSchema>> readSeparationSchema(const srt::ContribSpec &, std::string variant);

    class SeparationImportOptions : public srt::ContribImportOptions {
    public:
        explicit SeparationImportOptions(std::string variant) : ContribImportOptions(API_INTERFACE, std::move(variant), API_LEVEL) {}
    };

    class SeparationRuntimeOptions : public srt::InferenceRuntimeOptions {
    public:
        explicit SeparationRuntimeOptions(std::string variant) : InferenceRuntimeOptions(API_INTERFACE, std::move(variant), API_LEVEL) {}
    };

    class SeparationStartInput : public srt::TaskStartInput {
    public:
        static constexpr const char *API_INTERFACE = L1::API_INTERFACE;
        static constexpr int API_LEVEL = L1::API_LEVEL;
        SeparationStartInput() : TaskStartInput(API_INTERFACE, API_LEVEL) {}

        AudioSegment audio;
        /// Returning false cancels the execution. Progress is monotonic in [0,1].
        std::function<bool(double)> progress;
    };

    struct Track {
        std::string id;
        AudioSegment audio; // Same format, frame count and startTime as the input.
    };

    class SeparationResult : public srt::TaskResult {
    public:
        static constexpr const char *API_INTERFACE = L1::API_INTERFACE;
        static constexpr int API_LEVEL = L1::API_LEVEL;
        SeparationResult() : TaskResult(API_INTERFACE, API_LEVEL) {}

        std::vector<Track> tracks;
    };

    class SRTSEPINFER_EXPORT SeparationExecutive : public srt::InferenceExecutive {
    public:
        using AsyncCallback = std::function<void(srt::Expected<std::unique_ptr<SeparationResult>>)>;
        ~SeparationExecutive() override;
        srt::Expected<std::unique_ptr<SeparationResult>> start(const SeparationStartInput &input) { return task_.run(input); }
        srt::Expected<void> startAsync(std::shared_ptr<const SeparationStartInput> input, AsyncCallback callback) { return task_.runAsync(std::move(input), std::move(callback)); }

        srt::ITask::State state() const noexcept override { return task_.state(); }
        srt::Expected<void> stop() override { return task_.stop(); }
        srt::Expected<void> waitForFinished() override { return task_.waitForFinished(); }

    protected:
        explicit SeparationExecutive(srt::InferenceSpec &);
        virtual srt::Expected<std::unique_ptr<SeparationResult>> run(const SeparationStartInput &) = 0;
        bool cancelled() const noexcept { return task_.cancelled(); }

    private:
        // Providers must stop and wait before destroying objects read by run().
        SeparationTask<SeparationStartInput, SeparationResult> task_;
    };

    SRTSEPINFER_EXPORT srt::Expected<std::unique_ptr<SeparationExecutive>> createSeparator(srt::InferenceSpec &);
}
#endif
