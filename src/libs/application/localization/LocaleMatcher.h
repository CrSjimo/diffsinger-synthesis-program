#ifndef DSSP_LOCALIZATION_LOCALEMATCHER_H
#define DSSP_LOCALIZATION_LOCALEMATCHER_H

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <dssplocalization/LocalizationError.h>

namespace dssp::localization {
    // An empty match requests the declaration's default text/path. Invalid HTTP
    // preferences also fall back; internal ICU/RE2 failures remain errors.
    std::expected<std::optional<std::size_t>, LocalizationError> matchLocale(std::span<const std::string> available, std::string_view acceptLanguage);
}

#endif // DSSP_LOCALIZATION_LOCALEMATCHER_H
