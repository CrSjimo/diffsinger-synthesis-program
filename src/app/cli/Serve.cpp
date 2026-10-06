#include "Serve.h"

#include <exception>
#include <optional>

#include <drogon/drogon.h>
#include <stdcorelib/path.h>
#include <stdcorelib/support/logging.h>
#include <stdcorelib/support/systempaths.h>
#include <stdcorelib/system.h>

#include <dsspapiv1/Controllers.h>
#include <dsspapp/ArchComposition.h>
#include <dsspapp/InferenceComposition.h>
#include <dsspruntime/ServerConfig.h>
#include <dsspmedia/Resources.h>

namespace dssp {

    namespace {

        stdc::LogCategory log("dssp.cli.Serve");

        int runServe(const stdc::cli::ParseResult &result) {
            runtime::ConfigValues options, environment;
            std::filesystem::path applicationDir;
            std::optional<std::filesystem::path> temporaryDir;
            try {
                environment = stdc::system::environment();
                applicationDir = stdc::system::application_directory();
                temporaryDir = stdc::system::SystemPaths::writableDirectory(stdc::system::SystemPaths::TempDirectory);
                for (const auto name : {"--host", "--port", "--package-dir", "--execution-provider", "--device-index", "--silence-threshold", "--plugin-dir", "--runtime-path"}) {
                    if (const auto value = result.valueForOption<std::string>(name)) {
                        options.emplace(name, *value);
                    }
                }
            } catch (const std::exception &error) {
                log.stdcCritical("Cannot read startup configuration: %1", error.what());
                return 1;
            }
            if (!temporaryDir || temporaryDir->empty() || !temporaryDir->is_absolute()) {
                log.stdcCritical("Cannot determine the system temporary directory for HTTP request caching");
                return 1;
            }
            auto config = runtime::resolveServerConfig(options, environment, applicationDir);
            if (!config) {
                log.stdcCritical("Configuration error (%1): %2", config.error().option(), config.error().formatChain());
                return 1;
            }
            auto inference = initializeInference(*config, architecturePluginPaths(config->pluginDir));
            if (!inference) {
                log.stdcCritical("Inference initialization failed: %1", inference.error().formatChain());
                return 1;
            }
            const auto &resourceRoot = config->resourceDir;
            auto mediaResources = media::Resources::create(resourceRoot / "media" / "magic.mgc");
            if (!mediaResources) {
                log.stdcCritical("Media resource initialization failed: %1", mediaResources.error().formatChain());
                return 1;
            }
            const arch::ArchitectureServices services{*inference, *mediaResources, resourceRoot / "localization"};
            auto architectures = composeArchitectures(services);
            if (!architectures) {
                log.stdcCritical("Architecture initialization failed: %1", architectures.error().formatChain());
                return 1;
            }
            const auto lifetime = std::make_shared<http::RequestLifetimeRegistry>();
            auto &app = drogon::app();
            auto shutdown = [&app, lifetime] {
                lifetime->cancelAll();
                // Future runtime ownership must wait for its tasks before release.
                app.quit();
            };
            try {
                // Drogon also uses this path to cache large JSON request bodies.
                app.setUploadPath(stdc::path::to_utf8(*temporaryDir / "dssp" / "uploads"));
                api_v1::registerControllers(app, lifetime, *architectures);
                app.setConnectionCallback([lifetime](const trantor::TcpConnectionPtr &connection) { lifetime->connectionChanged(connection); });
                app.setTermSignalHandler(shutdown);
                app.setIntSignalHandler(shutdown);
                app.addListener(config->host, config->port);
                log.stdcInfo("Listening on %1:%2", config->host, config->port);
                app.run();
            } catch (const std::exception &error) {
                lifetime->cancelAll();
                log.stdcCritical("Drogon startup/run failed: %1", error.what());
                return 1;
            }
            lifetime->cancelAll();
            log.stdcInfo("Server stopped");
            return 0;
        }

    }

    stdc::cli::Command serveCommand = stdc::cli::Command{"serve", "Start the server"}
                                          .addHelpOption()
                                          .addOption(stdc::cli::Option({"--host"}, "Host to listen on (DSSP_HOST)").arg("host"))
                                          .addOption(stdc::cli::Option({"--port"}, "Nonzero listening port (DSSP_PORT)").arg("port"))
                                          .addOption(stdc::cli::Option({"--package-dir"}, "Installed package root, default user data/OpenVPI/DiffScope_packages (DSSP_PACKAGE_DIR)").arg("dir"))
                                          .addOption(stdc::cli::Option({"--execution-provider"}, "cpu, cuda, directml or coreml (DSSP_EXECUTION_PROVIDER)").arg("provider"))
                                          .addOption(stdc::cli::Option({"--device-index"}, "Native backend device index (DSSP_DEVICE_INDEX)").arg("index"))
                                          .addOption(stdc::cli::Option({"--silence-threshold"}, "Linear RMS threshold, default 0.02 (DSSP_SILENCE_THRESHOLD)").arg("threshold"))
                                          .addOption(stdc::cli::Option({"--plugin-dir"}, "Category plugin root (DSSP_PLUGIN_DIR)").arg("dir"))
                                          .addOption(stdc::cli::Option({"--runtime-path"}, "Explicit ONNX Runtime library (DSSP_RUNTIME_PATH)").arg("path"))
                                          .setHandler(runServe);

}
