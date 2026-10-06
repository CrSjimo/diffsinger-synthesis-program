#ifndef DSSP_INFERENCE_DISPLAY_H
#define DSSP_INFERENCE_DISPLAY_H

#include <expected>
#include <string>
#include <string_view>

#include <synthrt/Support/DisplayText.h>

#include <dssplocalization/LocalizationError.h>

namespace dssp::inference {
    std::expected<std::string, localization::LocalizationError> localize(const srt::DisplayText &, std::string_view acceptLanguage, std::string_view fallback = {});
}

#endif // DSSP_INFERENCE_DISPLAY_H
