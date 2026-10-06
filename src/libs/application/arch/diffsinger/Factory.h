#ifndef DSSP_ARCHDIFFSINGER_FACTORY_H
#define DSSP_ARCHDIFFSINGER_FACTORY_H

#include <string_view>

#include <dssparch/Architecture.h>
#include <dssparch/ArchitectureCatalog.h>
#include <dssparch/ArchitectureServices.h>
#include <dsspinference/Environment.h>

namespace dssp::arch_diffsinger {
    inline constexpr std::string_view Id = "diffsinger";
    std::vector<inference::PluginSearchPath> pluginPaths(const std::filesystem::path &root);
    std::expected<arch::ArchitecturePtr, arch::CatalogConstructionError> create(const arch::ArchitectureServices &);
}

#endif // DSSP_ARCHDIFFSINGER_FACTORY_H
