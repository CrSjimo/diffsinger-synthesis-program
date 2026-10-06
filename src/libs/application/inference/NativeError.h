#ifndef DSSP_INFERENCE_NATIVEERROR_H
#define DSSP_INFERENCE_NATIVEERROR_H

#include <string_view>
#include <system_error>

#include <synthrt/Support/Error.h>

#include <dssperror/Error.h>

namespace dssp::inference {
    Error::Ptr nativeError(const srt::Error &, std::string_view operation);
    Error::Ptr nativeError(const std::error_code &, std::string_view operation);
}

#endif // DSSP_INFERENCE_NATIVEERROR_H
