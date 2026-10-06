#ifndef DSSP_LOCALIZATION_CATALOG_H
#define DSSP_LOCALIZATION_CATALOG_H

#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <dssplocalization/LocalizationError.h>

namespace dssp::localization {
    // ResourceBundle strings are copied at startup into an immutable snapshot.
    // Requests do not mutate a global locale or share mutable ICU bundle cursors.
    class Catalog final {
    public:
        // fallbackLocale is an ICU resource basename, such as en_US. No bundle
        // is mandatory; absent translations leave a displayable resource key.
        static std::expected<std::shared_ptr<const Catalog>, LocalizationError> load(const std::filesystem::path &, std::string_view fallbackLocale);
        std::expected<std::string, LocalizationError> text(std::string_view key, std::string_view acceptLanguage) const;

    private:
        using Strings = std::map<std::string, std::string>;
        std::string fallbackLocale_;
        std::vector<std::string> locales_;
        std::vector<Strings> translations_;
    };
}

#endif // DSSP_LOCALIZATION_CATALOG_H
