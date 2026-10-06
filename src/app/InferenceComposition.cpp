#include "InferenceComposition.h"

#include <utility>

#include <dsspinference/NativeError.h>

namespace dssp {

    std::expected<std::shared_ptr<inference::Environment>, inference::InitializationError> initializeInference(const runtime::ServerConfig &config, std::vector<inference::PluginSearchPath> architecturePaths) {
        inference::EnvironmentOptions options;
        options.packageRoots.push_back(config.packageDir);
        // The independently deployed language packages share the same unit and
        // participate in dependency resolution with installed user packages.
        const auto languagePackages = config.resourceDir / "wolf" / "packages";
        std::error_code error;
        if (std::filesystem::is_directory(languagePackages, error)) {
            options.packageRoots.push_back(languagePackages);
        } else if (error && error != std::errc::no_such_file_or_directory) {
            return std::unexpected{inference::InitializationError("Cannot inspect deployed Wolf packages", inference::nativeError(error, "is_directory"))};
        }
        options.pluginPaths = {
            {"inference", config.pluginDir / "dsinfer" / "inferenceinterpreters"},
            {"inference", config.pluginDir / "wolf" / "inferenceinterpreters"},
            {"linguist", config.pluginDir / "wolf" / "linguistproviders"},
            {"analysis", config.pluginDir / "otter" / "analysisproviders"}
        };
        for (auto &path : architecturePaths) {
            options.pluginPaths.push_back(std::move(path));
        }
        options.onnx = inference::OnnxOptions{config.pluginDir / "dsinfer" / "inferencedrivers", config.runtimePath, config.executionProvider, config.deviceIndex};
        return inference::Environment::create(options);
    }
}
