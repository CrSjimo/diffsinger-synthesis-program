#include "ServerConfig.h"

#include <charconv>
#include <cmath>
#include <exception>
#include <optional>

#include <stdcorelib/path.h>
#include <stdcorelib/support/systempaths.h>

namespace dssp::runtime {

    namespace {

        template <class T>
        bool number(const std::string &text, T &value) {
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size();
        }

        std::expected<std::filesystem::path, ConfigurationError> resolvePath(const char *option, const std::string &text) {
            if (text.empty()) {
                return std::unexpected{ConfigurationError(option, "A nonempty path must be configured")};
            }
            std::filesystem::path path;
            try {
                path = stdc::path::from_utf8(text);
            } catch (const std::exception &error) {
                return std::unexpected{ConfigurationError(option, "Cannot decode path", std::make_shared<ExternalError>(error.what(), NativeDiagnostic{"stdcorelib", "from_utf8", "exception"}))};
            }
            std::error_code error;
            auto absolute = std::filesystem::absolute(path, error);
            if (error) {
                return std::unexpected{ConfigurationError(option, "Cannot resolve absolute path", std::make_shared<ExternalError>(error.message(), NativeDiagnostic{"filesystem", "absolute", error.category().name(), error.value()}))};
            }
            return absolute.lexically_normal();
        }

    }

    std::expected<std::filesystem::path, ConfigurationError> defaultPackageDir() {
        using SystemPaths = stdc::system::SystemPaths;
        std::optional<std::filesystem::path> base;
        try {
#ifdef _WIN32
            // StateDirectory maps to LocalAppData on Windows.
            base = SystemPaths::writableDirectory(SystemPaths::StateDirectory);
#else
            base = SystemPaths::writableDirectory(SystemPaths::AppDataDirectory);
#endif
        } catch (const std::exception &error) {
            return std::unexpected{ConfigurationError("--package-dir", "Cannot resolve the default package directory", std::make_shared<ExternalError>(error.what(), NativeDiagnostic{"stdcorelib", "SystemPaths::writableDirectory", "exception"}))};
        }
        if (!base || base->empty() || !base->is_absolute()) {
            return std::unexpected{ConfigurationError("--package-dir", "Cannot determine the user data directory; configure --package-dir or DSSP_PACKAGE_DIR")};
        }
        return (*base / "OpenVPI" / "DiffScope_packages").lexically_normal();
    }

    std::expected<ServerConfig, ConfigurationError> resolveServerConfig(const ConfigValues &options, const ConfigValues &environment, const std::filesystem::path &applicationDir) {
        ServerConfig config;
        auto value = [&](const char *option, const char *env) -> std::optional<std::string> {
            if (const auto it = options.find(option); it != options.end()) {
                return it->second;
            }
            if (const auto it = environment.find(env); it != environment.end()) {
                return it->second;
            }
            return {};
        };
        if (const auto text = value("--host", "DSSP_HOST")) {
            config.host = *text;
        }
        if (config.host.empty()) {
            return std::unexpected{ConfigurationError("--host", "Listening host cannot be empty")};
        }
        if (const auto text = value("--port", "DSSP_PORT")) {
            unsigned port = 0;
            if (!number(*text, port) || port == 0 || port > 65535) {
                return std::unexpected{ConfigurationError("--port", "Port must be an integer in [1, 65535]")};
            }
            config.port = static_cast<std::uint16_t>(port);
        }
        const auto packages = value("--package-dir", "DSSP_PACKAGE_DIR");
        auto packagePath = packages ? resolvePath("--package-dir", *packages) : defaultPackageDir();
        if (!packagePath) {
            return std::unexpected{std::move(packagePath.error())};
        }
        std::error_code pathError;
        if (!std::filesystem::is_directory(*packagePath, pathError)) {
            Error::Ptr cause;
            if (pathError) {
                cause = std::make_shared<ExternalError>(pathError.message(), NativeDiagnostic{"filesystem", "is_directory", pathError.category().name(), pathError.value()});
            }
            return std::unexpected{ConfigurationError("--package-dir", "Package root must be an existing directory", std::move(cause))};
        }
        config.packageDir = std::move(*packagePath);
        if (const auto text = value("--execution-provider", "DSSP_EXECUTION_PROVIDER")) {
            auto provider = DeviceInfo::executionProviderFromString(*text);
            if (!provider) {
                return std::unexpected{ConfigurationError("--execution-provider", "Expected cpu, cuda, directml or coreml", provider.error().clone())};
            }
            config.executionProvider = *provider;
        }
        if (const auto text = value("--device-index", "DSSP_DEVICE_INDEX")) {
            if (!number(*text, config.deviceIndex) || config.deviceIndex < 0) {
                return std::unexpected{ConfigurationError("--device-index", "Device index must be a nonnegative integer")};
            }
        }
        if (config.executionProvider == ExecutionProvider::CPU && config.deviceIndex != 0) {
            return std::unexpected{ConfigurationError("--device-index", "The CPU backend has only device index 0")};
        }
        if (const auto text = value("--silence-threshold", "DSSP_SILENCE_THRESHOLD")) {
            if (!number(*text, config.silenceThreshold) || !std::isfinite(config.silenceThreshold) || config.silenceThreshold < 0) {
                return std::unexpected{ConfigurationError("--silence-threshold", "Silence threshold must be a finite, nonnegative linear RMS value")};
            }
        }
#ifdef _WIN32
        constexpr const char *runtimeFile = "onnxruntime.dll";
#elif defined(__APPLE__)
        constexpr const char *runtimeFile = "libonnxruntime.dylib";
#else
        constexpr const char *runtimeFile = "libonnxruntime.so";
#endif
        if (!applicationDir.is_absolute()) {
            return std::unexpected{ConfigurationError("application-directory", "Deployment paths require an absolute application directory")};
        }
        config.resourceDir = (applicationDir / DSSP_DATA_RELATIVE_DIR).lexically_normal();
        if (const auto plugins = value("--plugin-dir", "DSSP_PLUGIN_DIR")) {
            auto path = resolvePath("--plugin-dir", *plugins);
            if (!path) {
                return std::unexpected{std::move(path.error())};
            }
            config.pluginDir = std::move(*path);
        } else {
            config.pluginDir = (applicationDir / DSSP_PLUGIN_RELATIVE_DIR).lexically_normal();
        }
        if (const auto runtime = value("--runtime-path", "DSSP_RUNTIME_PATH")) {
            auto path = resolvePath("--runtime-path", *runtime);
            if (!path) {
                return std::unexpected{std::move(path.error())};
            }
            config.runtimePath = std::move(*path);
        } else {
            auto runtimeDirectory = config.pluginDir / "dsinfer" / "inferencedrivers" / "onnx" / "runtime";
            if (config.executionProvider == ExecutionProvider::CUDA) {
                runtimeDirectory /= "cuda";
            }
            config.runtimePath = runtimeDirectory / runtimeFile;
        }
        // The inference environment checks provider/device availability and
        // deployed plugins. Configuration parsing never loads a runtime.
        return config;
    }

}
