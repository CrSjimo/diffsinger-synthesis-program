#include "Display.h"

#include <exception>
#include <vector>

#include <dssplocalization/LocaleMatcher.h>

namespace dssp::inference {
    std::expected<std::string, localization::LocalizationError> localize(const srt::DisplayText &text, std::string_view acceptLanguage, std::string_view fallback) {
        try {
            const auto declared = text.locales();
            std::vector<std::string> locales(declared.begin(), declared.end());
            auto matched = localization::matchLocale(locales, acceptLanguage);
            if (!matched) {
                return std::unexpected{std::move(matched.error())};
            }
            if (*matched) {
                const auto &selected = text.text(locales[**matched]);
                if (!selected.empty()) {
                    return selected;
                }
            }
            const auto &defaultText = text.text();
            return defaultText.empty() ? std::string(fallback) : defaultText;
        } catch (const std::exception &error) {
            return std::unexpected{localization::LocalizationError("Cannot read multilingual declaration", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"synthrt", "DisplayText", "exception"}))};
        }
    }
}
