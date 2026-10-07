#ifndef DSSP_INFERENCE_NATIVECALL_H
#define DSSP_INFERENCE_NATIVECALL_H

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

#include <dsspinference/InferenceError.h>
#include <dsspinference/NativeError.h>

namespace dssp::inference {

    // Only wrap an actual native-library call. Application errors use expected.
    template <class Function>
    auto nativeCall(std::string_view operation, Function &&function) -> std::expected<typename std::invoke_result_t<Function>::value_type, ExecutionError> {
        using Value = typename std::invoke_result_t<Function>::value_type;
        try {
            auto result = std::invoke(std::forward<Function>(function));
            if (!result) {
                return std::unexpected{ExecutionError(std::string(operation) + " failed", nativeError(result.error(), operation))};
            }
            if constexpr (std::is_void_v<Value>) {
                return {};
            } else {
                return result.take();
            }
        } catch (const std::exception &error) {
            return std::unexpected{ExecutionError(std::string(operation) + " threw an exception", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"synthrt inference", std::string(operation), "exception"}))};
        } catch (...) {
            return std::unexpected{ExecutionError(std::string(operation) + " threw an unknown native exception", std::make_shared<const ExternalError>("Nonstandard native exception", NativeDiagnostic{"synthrt inference", std::string(operation), "exception"}))};
        }
    }
}

#endif // DSSP_INFERENCE_NATIVECALL_H
