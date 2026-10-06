#include "NativeError.h"

#include <dsspruntime/SrtErrors.h>

namespace dssp::inference {

    Error::Ptr nativeError(const srt::Error &error, std::string_view operation) {
        return runtime::snapshotSrtError(error, std::string(operation));
    }

    Error::Ptr nativeError(const std::error_code &error, std::string_view operation) {
        return std::make_shared<const ExternalError>(error.message(), NativeDiagnostic{"filesystem", std::string(operation), error.category().name(), error.value()});
    }

}
