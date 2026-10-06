#include "ArchComposition.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <dssparchdiffsinger/Factory.h>

namespace dssp {
    namespace {
        struct BuiltinArchitecture {
            std::string_view id;
            std::expected<arch::ArchitecturePtr, arch::CatalogConstructionError> (*create)(const arch::ArchitectureServices &);
            std::vector<inference::PluginSearchPath> (*pluginPaths)(const std::filesystem::path &);
        };
        // The application's complete supported set is fixed at compile time.
        constexpr std::array builtins{BuiltinArchitecture{arch_diffsinger::Id, &arch_diffsinger::create, &arch_diffsinger::pluginPaths}};

        consteval bool validBuiltins() {
            for (std::size_t i = 0; i < builtins.size(); ++i) {
                if (builtins[i].id.empty() || !builtins[i].create || !builtins[i].pluginPaths) {
                    return false;
                }
                for (std::size_t j = 0; j < i; ++j) {
                    if (builtins[i].id == builtins[j].id) {
                        return false;
                    }
                }
            }
            return true;
        }
        static_assert(validBuiltins(), "Built-in architecture IDs must be nonempty and unique, with a factory");
    }

    std::vector<inference::PluginSearchPath> architecturePluginPaths(const std::filesystem::path &root) {
        std::vector<inference::PluginSearchPath> paths;
        for (const auto &builtin : builtins) {
            auto additions = builtin.pluginPaths(root);
            for (auto &path : additions) {
                paths.push_back(std::move(path));
            }
        }
        return paths;
    }

    std::expected<std::shared_ptr<const arch::ArchitectureCatalog>, arch::CatalogConstructionError> composeArchitectures(const arch::ArchitectureServices &services) {
        std::vector<arch::ArchitecturePtr> entries;
        entries.reserve(builtins.size());
        for (const auto &builtin : builtins) {
            auto implementation = builtin.create(services);
            if (!implementation) {
                return std::unexpected{std::move(implementation.error())};
            }
            if (!*implementation || (*implementation)->id() != builtin.id) {
                return std::unexpected{arch::CatalogConstructionError("Architecture factory returned an unexpected ID for " + std::string(builtin.id))};
            }
            entries.push_back(std::move(*implementation));
        }
        auto catalog = arch::ArchitectureCatalog::create(std::move(entries));
        if (!catalog) {
            return std::unexpected{std::move(catalog.error())};
        }
        return std::make_shared<const arch::ArchitectureCatalog>(std::move(*catalog));
    }

}
