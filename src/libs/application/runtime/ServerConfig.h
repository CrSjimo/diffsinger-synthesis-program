#ifndef DSSP_SERVERCONFIG_H
#define DSSP_SERVERCONFIG_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>

#include <dsspresource/CacheOptions.h>
#include <dsspruntime/ConfigurationError.h>
#include <dsspruntime/DeviceInfo.h>
#include <dsspruntime/InferenceCaches.h>

namespace dssp::runtime {

    struct ServerConfig {
        std::string host = "127.0.0.1";
        std::uint16_t port = 13711;
        std::size_t maxRequestBodySize = 1024U * 1024U * 1024U;
        std::filesystem::path packageDir;
        ExecutionProvider executionProvider = DeviceInfo::defaultExecutionProvider();
        int deviceIndex = 0;
        double silenceThreshold = 0.02;
        std::chrono::seconds streamHeartbeatInterval{10};
        std::map<std::string, resource::CacheOptions> inferenceCaches = defaultInferenceCaches();
        // Bundled deployment paths derived from the executable location.
        std::filesystem::path pluginDir;
        std::filesystem::path runtimePath;
        std::filesystem::path resourceDir;
    };

    using ConfigValues = std::map<std::string, std::string>;
    // Resolve the legacy per-user package location without creating it.
    std::expected<std::filesystem::path, ConfigurationError> defaultPackageDir();
    std::expected<ServerConfig, ConfigurationError> resolveServerConfig(const ConfigValues &options, const ConfigValues &environment, const std::filesystem::path &applicationDir);

}

#endif // DSSP_SERVERCONFIG_H
