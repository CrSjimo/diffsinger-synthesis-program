#ifndef SRTSEPINFER_SEPARATIONINTERPRETER_H
#define SRTSEPINFER_SEPARATIONINTERPRETER_H
#include <srtsepinfer/export.h>
#include <synthrt/SVS/InferenceInterpreter.h>
namespace srtsepinfer {
    /// Shared import policy for Separation Level 1 interpreters. Configuration
    /// and model geometry belong to each variant, exports belong to the API.
    class SRTSEPINFER_EXPORT SeparationInterpreter : public srt::InferenceInterpreter {
    public:
        srt::Expected<std::unique_ptr<srt::ContribImportOptions>> createImportOptions(const srt::ContribSpec &, const srt::JsonValue &) const override;
    };
}
#endif
