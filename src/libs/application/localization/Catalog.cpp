#include "Catalog.h"

#include <algorithm>
#include <exception>
#include <memory>

#include <stdcorelib/path.h>
#include <stdcorelib/str.h>
#include <stdcorelib/support/logging.h>
#include <unicode/unistr.h>
#include <unicode/ures.h>

#include <dssplocalization/LocaleMatcher.h>

namespace dssp::localization {
    namespace {
        stdc::LogCategory log("dssp.localization.Catalog");
        using Bundle = std::unique_ptr<UResourceBundle, decltype(&ures_close)>;
        std::expected<std::map<std::string, std::string>, LocalizationError> readBundle(const std::filesystem::path &directory, const std::string &locale) {
            UErrorCode status = U_ZERO_ERROR;
            const auto path = stdc::path::to_utf8(directory);
            Bundle bundle(ures_openDirect(path.c_str(), locale.c_str(), &status), ures_close);
            if (U_FAILURE(status)) {
                return std::unexpected{LocalizationError("Cannot load translation bundle " + locale, std::make_shared<const ExternalError>(u_errorName(status), NativeDiagnostic{"ICU", "ures_openDirect", "UErrorCode", status}))};
            }
            if (ures_getType(bundle.get()) != URES_TABLE) {
                return std::unexpected{LocalizationError("Translation bundle must be a keyed resource table")};
            }

            std::map<std::string, std::string> result;
            for (int32_t i = 0; i < ures_getSize(bundle.get()); ++i) {
                Bundle item(ures_getByIndex(bundle.get(), i, nullptr, &status), ures_close);
                if (U_FAILURE(status) || !item) {
                    return std::unexpected{LocalizationError("Cannot read translation resource", std::make_shared<const ExternalError>(u_errorName(status), NativeDiagnostic{"ICU", "ures_getByIndex", "UErrorCode", status}))};
                }
                int32_t length = 0;
                const auto *value = ures_getString(item.get(), &length, &status);
                if (U_FAILURE(status)) {
                    return std::unexpected{LocalizationError("Translation bundle must contain string resources", std::make_shared<const ExternalError>(u_errorName(status), NativeDiagnostic{"ICU", "ures_getString", "UErrorCode", status}))};
                }
                std::string text;
                icu::UnicodeString(value, length).toUTF8String(text);
                const auto *key = ures_getKey(item.get());
                if (!key) {
                    return std::unexpected{LocalizationError("Translation resource has no key")};
                }
                result.emplace(key, std::move(text));
            }
            return result;
        }
    }

    std::expected<std::shared_ptr<const Catalog>, LocalizationError> Catalog::load(const std::filesystem::path &directory, std::string_view fallbackLocale) {
        try {
            if (log.isLevelEnabled(stdc::Logger::Debug)) {
                log.stdcDebug("Loading translation directory %1 with preferred fallback %2", stdc::path::to_utf8(directory), fallbackLocale);
            }

            auto result = std::shared_ptr<Catalog>(new Catalog);
            result->fallbackLocale_ = fallbackLocale;
            std::error_code error;
            std::filesystem::directory_iterator iterator(directory, error), end;
            // A deployment without translation files may omit the directory.
            if (error == std::errc::no_such_file_or_directory) {
                error.clear();
            }

            std::vector<std::string> locales;
            while (!error && iterator != end) {
                const auto path = iterator->path();
                if (path.extension() == ".res" && path.stem() != "root" && iterator->is_regular_file(error)) {
                    locales.push_back(stdc::path::to_utf8(path.stem()));
                }
                if (!error) {
                    iterator.increment(error);
                }
            }
            if (error) {
                return std::unexpected{LocalizationError("Cannot enumerate translation bundles", std::make_shared<const ExternalError>(error.message(), NativeDiagnostic{"filesystem", "directory_iterator", "error_code", error.value()}))};
            }

            std::ranges::sort(locales);
            for (const auto &locale : locales) {
                auto strings = readBundle(directory, locale);
                if (!strings) {
                    return std::unexpected{std::move(strings.error())};
                }
                result->locales_.push_back(locale);
                result->translations_.push_back(std::move(*strings));
            }

            if (locales.empty()) {
                log.stdcDebug("No translation bundles available; display values will use resource keys");
            } else if (std::ranges::find(locales, result->fallbackLocale_) == locales.end()) {
                log.stdcDebug("Preferred fallback bundle %1 is absent; using available translations", fallbackLocale);
            }

            log.stdcDebug("Translation locales available: %1; directory=%2", stdc::str::join(locales, ", "), stdc::path::to_utf8(directory));
            return std::shared_ptr<const Catalog>(std::move(result));
        } catch (const std::exception &error) {
            return std::unexpected{LocalizationError("Translation resource library failed", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"ICU/filesystem", "load translation resources", "exception"}))};
        }
    }

    std::expected<std::string, LocalizationError> Catalog::text(std::string_view key, std::string_view acceptLanguage) const {
        const std::string resourceKey(key);
        // Only bundles containing this key participate in language matching.
        std::vector<std::string> locales;
        std::vector<const std::string *> strings;
        for (std::size_t i = 0; i < locales_.size(); ++i) {
            if (const auto it = translations_[i].find(resourceKey); it != translations_[i].end()) {
                locales.push_back(locales_[i]);
                strings.push_back(&it->second);
            }
        }

        if (strings.empty()) {
            // The untranslated display value is deliberately not a contract.
            return resourceKey;
        }

        auto matched = matchLocale(locales, acceptLanguage);
        if (!matched) {
            return std::unexpected{std::move(matched.error())};
        }
        if (*matched) {
            return *strings[**matched];
        }
        if (const auto fallback = std::ranges::find(locales, fallbackLocale_); fallback != locales.end()) {
            return *strings[static_cast<std::size_t>(fallback - locales.begin())];
        }

        // The preferred fallback bundle is optional, too.
        return *strings.front();
    }
}
