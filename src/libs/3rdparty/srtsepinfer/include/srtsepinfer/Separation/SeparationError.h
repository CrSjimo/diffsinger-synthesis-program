#ifndef SRTSEPINFER_SEPARATIONERROR_H
#define SRTSEPINFER_SEPARATIONERROR_H

#include <system_error>
#include <type_traits>

#include <srtsepinfer/export.h>
#include <synthrt/Support/Error.h>

namespace srtsepinfer {
    enum class SeparationError {
        Cancelled = 1,
        NoWorker,
        Internal,
        ContractViolation,
        ModelFailed,
        ModelMismatch
    };
    SRTSEPINFER_EXPORT const std::error_category &separationErrorCategory() noexcept;
    inline std::error_code make_error_code(SeparationError error) noexcept {
        return {static_cast<int>(error), separationErrorCategory()};
    }
}
template <>
struct std::is_error_code_enum<srtsepinfer::SeparationError> : std::true_type {};
namespace srtsepinfer {
    inline srt::Error cancelledError() {
        return srt::Error(SeparationError::Cancelled, "Audio separation was cancelled");
    }
}
#endif
