#ifndef SRTBEATINFER_BEATERROR_H
#define SRTBEATINFER_BEATERROR_H

#include <system_error>
#include <type_traits>

#include <srtbeatinfer/export.h>
#include <synthrt/Support/Error.h>

namespace srtbeatinfer {
    enum class BeatError {
        Cancelled = 1,
        NoWorker,
        Internal,
        ContractViolation,
        ModelFailed,
        ModelMismatch
    };
    SRTBEATINFER_EXPORT const std::error_category &beatErrorCategory() noexcept;
    inline std::error_code make_error_code(BeatError error) noexcept {
        return {static_cast<int>(error), beatErrorCategory()};
    }
}
template <>
struct std::is_error_code_enum<srtbeatinfer::BeatError> : std::true_type {};
namespace srtbeatinfer {
    inline srt::Error cancelledError() {
        return srt::Error(BeatError::Cancelled, "Audio beat was cancelled");
    }
}
#endif
