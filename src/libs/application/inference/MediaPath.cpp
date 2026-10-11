#include "MediaPath.h"

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace dssp::inference {
    std::expected<srt::DisplayText, ResourceError> preserveEmptyMediaPaths(const srt::DisplayText &resolved, const srt::JsonValue &declaration) {
        try {
            // The pinned SynthRT resolves an empty string against the module directory.
            // Use the parsed declaration to distinguish it from an explicit directory path.
            if (declaration.isString()) {
                return declaration.toString().empty() ? srt::DisplayText{} : resolved;
            }
            if (!declaration.isObject()) {
                return std::unexpected{ResourceError("Media path declaration must be a string or a language map")};
            }

            auto fallback = resolved.text();
            std::map<std::string, std::string> localized;
            for (const auto &[locale, value] : declaration.toObject()) {
                if (!value.isString()) {
                    return std::unexpected{ResourceError("Media path language map must contain string values")};
                }
                if (locale == "_") {
                    if (value.toString().empty()) {
                        fallback.clear();
                    }
                } else {
                    localized.emplace(locale, value.toString().empty() ? std::string{} : resolved.text(locale));
                }
            }

            return srt::DisplayText(std::move(fallback), localized);
        } catch (const std::exception &error) {
            return std::unexpected{ResourceError("Cannot preserve optional media path values", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"synthrt", "read media path declaration", "exception"}))};
        }
    }
}
