#include "InferenceComposition.h"

#include <utility>

#include <stdcorelib/path.h>
#include <stdcorelib/support/logging.h>

#include <dsspinference/NativeError.h>

namespace dssp {

    namespace {
        stdc::LogCategory log("dssp.InferenceComposition");
    }

    std::expected<std::shared_ptr<inference::Environment>, inference::InitializationError> initializeInference(const runtime::ServerConfig &config, std::vector<inference::PluginSearchPath> architecturePaths) {
        inference::EnvironmentOptions options;
        options.cacheGroups = config.inferenceCaches;
        options.packageRoots.push_back(config.packageDir);
        // The independently deployed language packages share the same unit and
        // participate in dependency resolution with installed user packages.
        const auto languagePackages = config.resourceDir / "wolf" / "packages";
        std::error_code error;
        if (std::filesystem::is_directory(languagePackages, error)) {
            options.packageRoots.push_back(languagePackages);
            if (log.isLevelEnabled(stdc::Logger::Debug)) {
                log.stdcDebug("Using deployed Wolf language packages: %1", stdc::path::to_utf8(languagePackages));
            }
        } else if (error && error != std::errc::no_such_file_or_directory) {
            return std::unexpected{inference::InitializationError("Cannot inspect deployed Wolf packages", inference::nativeError(error, "is_directory"))};
        } else {
            log.stdcDebug("No optional deployed Wolf package directory; using configured user packages");
        }
        options.pluginPaths = {
            {"inference", config.pluginDir / "dsinfer" / "inferenceinterpreters"},
            {"inference", config.pluginDir / "wolf" / "inferenceinterpreters"},
            {"linguist", config.pluginDir / "wolf" / "linguistproviders"},
            {"inference", config.pluginDir / "otter" / "inferenceinterpreters"},
            {"inference", config.pluginDir / "srtsepinfer" / "inferenceinterpreters"},
            {"inference", config.pluginDir / "srtbeatinfer" / "inferenceinterpreters"}
        };
        for (auto &path : architecturePaths) {
            options.pluginPaths.push_back(std::move(path));
        }
        options.onnx = inference::OnnxOptions{config.pluginDir / "dsinfer" / "inferencedrivers", config.runtimePath, config.executionProvider, config.deviceIndex};
        return inference::Environment::create(options);
    }
}
