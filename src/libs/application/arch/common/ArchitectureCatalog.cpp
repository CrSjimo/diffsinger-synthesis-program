#include "ArchitectureCatalog.h"

#include <algorithm>
#include <string>
#include <utility>

#include <stdcorelib/support/logging.h>

namespace dssp::arch {
    namespace {
        stdc::LogCategory log("dssp.arch.ArchitectureCatalog");
    }

    ArchitectureCatalog::ArchitectureCatalog(std::vector<ArchitecturePtr> entries) : m_entries(std::move(entries)) {}

    std::expected<ArchitectureCatalog, CatalogConstructionError> ArchitectureCatalog::create(std::vector<ArchitecturePtr> entries) {
        for (auto current = entries.begin(); current != entries.end(); ++current) {
            if (!*current || (*current)->id().empty()) {
                return std::unexpected{CatalogConstructionError("Architecture entries must have an instance and an ID")};
            }
            const auto duplicate = std::find_if(entries.begin(), current, [&](const auto &previous) { return previous->id() == (*current)->id(); });
            if (duplicate != current) {
                return std::unexpected{CatalogConstructionError("Duplicate architecture ID: " + std::string((*current)->id()))};
            }
        }

        std::string ids;
        for (const auto &entry : entries) {
            if (!ids.empty()) {
                ids += ", ";
            }
            ids += entry->id();
        }

        log.stdcInfo("Available architectures: %1", ids.empty() ? "none" : ids);
        return ArchitectureCatalog(std::move(entries));
    }

    std::span<const ArchitecturePtr> ArchitectureCatalog::entries() const noexcept {
        return m_entries;
    }

    ArchitecturePtr ArchitectureCatalog::find(std::string_view id) const noexcept {
        const auto found = std::find_if(m_entries.begin(), m_entries.end(), [&](const auto &implementation) { return implementation->id() == id; });
        return found == m_entries.end() ? ArchitecturePtr{} : *found;
    }

}
