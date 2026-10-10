#ifndef SRTBEATINFER_BEATAPIL1_H
#define SRTBEATINFER_BEATAPIL1_H

#include <vector>
#include <srtbeatinfer/Beat/Provider/BeatTask.h>
#include <synthrt/SVS/InferenceContrib.h>

namespace srtbeatinfer::Api::Beat::L1 {
    inline constexpr char API_INTERFACE[] = "org.openvpi.srtbeatinfer.inference.Beat";
    inline constexpr int API_LEVEL = 1;
    /// Finite, interleaved float32 PCM. Hosts decode and resample before calling.
    struct AudioSegment {
        int sampleRate = 0;
        int channelCount = 0;
        std::vector<float> samples;
        double startTime = 0;
    };
    class SRTBEATINFER_EXPORT BeatSchema : public srt::ContribExports {
    public:
        explicit BeatSchema(std::string variant);
        ~BeatSchema() override;
        int sampleRate = 0;
        int channelCount = 0;
        double maxSegmentDuration = 0; // Zero means the interpreter handles arbitrary lengths.
        bool downbeats = false; // Can identify the first beat of a measure.
    };
    SRTBEATINFER_EXPORT srt::Expected<std::unique_ptr<BeatSchema>> readBeatSchema(const srt::ContribSpec &, std::string variant);
    class BeatImportOptions : public srt::ContribImportOptions {
    public:
        explicit BeatImportOptions(std::string variant) : ContribImportOptions(API_INTERFACE, std::move(variant), API_LEVEL) {}
    };
    class BeatRuntimeOptions : public srt::InferenceRuntimeOptions {
    public:
        explicit BeatRuntimeOptions(std::string variant) : InferenceRuntimeOptions(API_INTERFACE, std::move(variant), API_LEVEL) {}
    };
    class BeatStartInput : public srt::TaskStartInput {
    public:
        static constexpr const char *API_INTERFACE = L1::API_INTERFACE;
        static constexpr int API_LEVEL = L1::API_LEVEL;
        BeatStartInput() : TaskStartInput(API_INTERFACE, API_LEVEL) {}
        AudioSegment audio;
        std::function<bool(double)> progress; // Monotonic [0,1]; false requests cancellation.
    };
    struct Beat {
        double time = 0; // Absolute seconds, including AudioSegment::startTime.
        bool downbeat = false;
    };
    class BeatResult : public srt::TaskResult {
    public:
        static constexpr const char *API_INTERFACE = L1::API_INTERFACE;
        static constexpr int API_LEVEL = L1::API_LEVEL;
        BeatResult() : TaskResult(API_INTERFACE, API_LEVEL) {}
        std::vector<Beat> beats; // Strictly increasing times within the input; may be empty.
    };
    class SRTBEATINFER_EXPORT BeatExecutive : public srt::InferenceExecutive {
    public:
        using AsyncCallback = std::function<void(srt::Expected<std::unique_ptr<BeatResult>>)>;
        ~BeatExecutive() override;
        srt::Expected<std::unique_ptr<BeatResult>> start(const BeatStartInput &input) { return task_.run(input); }
        srt::Expected<void> startAsync(std::shared_ptr<const BeatStartInput> input, AsyncCallback callback) { return task_.runAsync(std::move(input), std::move(callback)); }
        srt::ITask::State state() const noexcept override { return task_.state(); }
        srt::Expected<void> stop() override { return task_.stop(); }
        srt::Expected<void> waitForFinished() override { return task_.waitForFinished(); }
    protected:
        explicit BeatExecutive(srt::InferenceSpec &);
        virtual srt::Expected<std::unique_ptr<BeatResult>> run(const BeatStartInput &) = 0;
        bool cancelled() const noexcept { return task_.cancelled(); }
    private:
        // Providers stop and wait before destroying members accessed by run().
        BeatTask<BeatStartInput, BeatResult> task_;
    };
    SRTBEATINFER_EXPORT srt::Expected<std::unique_ptr<BeatExecutive>> createBeatTracker(srt::InferenceSpec &);
}
#endif
