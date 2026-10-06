#ifndef DSSP_APP_ARCHCOMPOSITION_H
#define DSSP_APP_ARCHCOMPOSITION_H

#include <expected>
#include <memory>

#include <dssparch/ArchitectureCatalog.h>
#include <dssparch/ArchitectureServices.h>
#include <dsspinference/Environment.h>

namespace dssp {
    std::vector<inference::PluginSearchPath> architecturePluginPaths(const std::filesystem::path &root);
    std::expected<std::shared_ptr<const arch::ArchitectureCatalog>, arch::CatalogConstructionError> composeArchitectures(const arch::ArchitectureServices &);
}

#endif // DSSP_APP_ARCHCOMPOSITION_H
