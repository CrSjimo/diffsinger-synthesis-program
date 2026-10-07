#include "LocaleMatcher.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <vector>

#include <re2/re2.h>
#include <stdcorelib/str.h>
#include <unicode/localematcher.h>

namespace dssp::localization {
    namespace {
        std::string normalize(std::string_view input) {
            auto text = std::string(stdc::str::trim(input, " \t"));
            if (const auto suffix = text.find_first_of(".@"); suffix != std::string::npos) {
                text.resize(suffix);
            }
            std::ranges::replace(text, '_', '-');
            return text;
        }
        LocalizationError icuError(UErrorCode code, std::string_view operation) {
            return LocalizationError("CLDR language matching failed", std::make_shared<const ExternalError>(u_errorName(code), NativeDiagnostic{"ICU", std::string(operation), "UErrorCode", code}));
        }
    }

    std::expected<std::optional<std::size_t>, LocalizationError> matchLocale(std::span<const std::string> available, std::string_view acceptLanguage) {
        try {
            if (available.empty() || acceptLanguage.empty()) {
                return std::optional<std::size_t>{};
            }
            if (acceptLanguage.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
                return std::unexpected{LocalizationError("Language preference header exceeds the ICU input limit")};
            }
            // ICU parses weights, ordering and duplicates. A wildcard expresses
            // no specific display locale, so it leaves the declaration fallback.
            // Remove only wildcard items, which ICU's public parser rejects.
            static const re2::RE2 wildcard(R"((^|,)[ \t]*\*[ \t]*(?:;[ \t]*[qQ]=[ \t]*(?:0(?:\.[0-9]{0,3})?|1(?:\.0{0,3})?)[ \t]*)?(,|$))", [] {
                re2::RE2::Options options;
                options.set_log_errors(false);
                return options;
            }());
            if (!wildcard.ok()) {
                return std::unexpected{LocalizationError("Cannot initialize wildcard compatibility", std::make_shared<const ExternalError>(wildcard.error(), NativeDiagnostic{"RE2", "compile language wildcard pattern", "re2", wildcard.error_code()}))};
            }
            std::string preferences(acceptLanguage);
            std::ranges::replace(preferences, '\t', ' ');
            while (re2::RE2::GlobalReplace(&preferences, wildcard, R"(\1\2)")) {}
            UErrorCode status = U_ZERO_ERROR;
            std::vector<icu::Locale> locales;
            std::vector<std::size_t> indices;
            for (std::size_t i = 0; i < available.size(); ++i) {
                auto locale = icu::Locale::forLanguageTag(normalize(available[i]), status);
                if (U_FAILURE(status) || locale.isBogus()) {
                    status = U_ZERO_ERROR;
                    continue;
                }
                locale.canonicalize(status);
                if (U_FAILURE(status)) {
                    return std::unexpected{icuError(status, "canonicalize supported locale")};
                }
                indices.push_back(i);
                locales.push_back(std::move(locale));
            }
            auto matcher = icu::LocaleMatcher::Builder().setSupportedLocales(locales.begin(), locales.end()).setNoDefaultLocale().build(status);
            if (U_FAILURE(status)) {
                return std::unexpected{icuError(status, "build LocaleMatcher")};
            }
            const auto *matched = matcher.getBestMatchForListString(icu::StringPiece(preferences.data(), static_cast<int32_t>(preferences.size())), status);
            if (status == U_ILLEGAL_ARGUMENT_ERROR) {
                return std::optional<std::size_t>{};
            }
            if (U_FAILURE(status)) {
                return std::unexpected{icuError(status, "getBestMatchForListString")};
            }
            if (matched) {
                for (std::size_t i = 0; i < locales.size(); ++i) {
                    if (*matched == locales[i]) {
                        return std::optional{indices[i]};
                    }
                }
            }
            return std::optional<std::size_t>{};
        } catch (const std::exception &error) {
            return std::unexpected{LocalizationError("Language preference library failed", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"ICU/RE2", "match locale", "exception"}))};
        }
    }
}
