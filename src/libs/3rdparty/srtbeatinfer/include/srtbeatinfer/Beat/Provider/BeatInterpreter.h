#ifndef SRTBEATINFER_BEATINTERPRETER_H
#define SRTBEATINFER_BEATINTERPRETER_H
#include <srtbeatinfer/export.h>
#include <synthrt/SVS/InferenceInterpreter.h>
namespace srtbeatinfer {
    /// Shared import policy for Beat Level 1 interpreters. Configuration
    /// and model geometry belong to each variant, exports belong to the API.
    class SRTBEATINFER_EXPORT BeatInterpreter : public srt::InferenceInterpreter {
    public:
        srt::Expected<std::unique_ptr<srt::ContribImportOptions>> createImportOptions(const srt::ContribSpec &, const srt::JsonValue &) const override;
    };
}
#endif
