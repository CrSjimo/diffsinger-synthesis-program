#include "Factory.h"

#include <memory>

#include <dssparchdiffsinger/DiffSinger.h>
#include <dssparchdiffsinger/SingerCatalog.h>
#include <dssplocalization/Catalog.h>

namespace dssp::arch_diffsinger {
    std::vector<inference::PluginSearchPath> pluginPaths(const std::filesystem::path &root) {
        return {{"singer", root / "dsinfer" / "singerproviders"}};
    }
    std::expected<arch::ArchitecturePtr, arch::CatalogConstructionError> create(const arch::ArchitectureServices &services) {
        if (!services.media) {
            return std::unexpected{arch::CatalogConstructionError("DiffSinger requires a media resource service")};
        }
        auto translations = localization::Catalog::load(services.translationRoot / Id, "en_US");
        if (!translations) {
            return std::unexpected{arch::CatalogConstructionError("Cannot load DiffSinger translation resources", translations.error().clone())};
        }
        auto catalog = SingerCatalog::discover(services.inference);
        if (!catalog) {
            return std::unexpected{arch::CatalogConstructionError("Cannot initialize the DiffSinger singer catalogue", catalog.error().clone())};
        }
        const auto pool = services.inference->inferences("diffsinger-language");
        if (!pool) {
            return std::unexpected{arch::CatalogConstructionError("Cannot initialize the language inference cache group", pool.error().clone())};
        }
        return std::make_shared<const DiffSinger>(std::move(*catalog), services.media, std::move(*translations));
    }
}
