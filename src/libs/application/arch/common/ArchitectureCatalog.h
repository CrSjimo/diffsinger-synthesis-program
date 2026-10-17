#ifndef DSSP_ARCH_ARCHITECTURECATALOG_H
#define DSSP_ARCH_ARCHITECTURECATALOG_H

#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <dssparch/Architecture.h>

namespace dssp::arch {

    class CatalogConstructionError final : public ErrorType<CatalogConstructionError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.arch.catalog-construction"; }
    };

    // The owner supplies implementations. Membership and order never change
    // after construction; concurrent reads require no registry lock.
    class ArchitectureCatalog {
    public:
        ArchitectureCatalog(const ArchitectureCatalog &) = default;
        ArchitectureCatalog(ArchitectureCatalog &&) noexcept = default;
        ArchitectureCatalog &operator=(const ArchitectureCatalog &) = delete;
        ArchitectureCatalog &operator=(ArchitectureCatalog &&) = delete;

        static std::expected<ArchitectureCatalog, CatalogConstructionError> create(std::vector<ArchitecturePtr> entries);
        std::span<const ArchitecturePtr> entries() const noexcept;
        ArchitecturePtr find(std::string_view id) const noexcept;

    private:
        explicit ArchitectureCatalog(std::vector<ArchitecturePtr> entries);
        std::vector<ArchitecturePtr> m_entries;
    };

}

#endif // DSSP_ARCH_ARCHITECTURECATALOG_H
