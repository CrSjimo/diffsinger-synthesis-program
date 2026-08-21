#include "Serve.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

#include <stdcorelib/path.h>
#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <drogon/drogon.h>

#include <dssp/ServerConfig.h>

namespace dssp {

    namespace {

        void parseConfig(const stdc::cli::ParseResult &result) {
            const auto env = stdc::system::environment();

            if (const auto host = result.valueForOption<std::string>("--host")) {
                ServerConfig::setHost(*host);
            } else if (const auto it = env.find("DSSP_HOST"); it != env.end()) {
                ServerConfig::setHost(it->second);
            }

            if (const auto port = result.valueForOption<std::uint16_t>("--port")) {
                ServerConfig::setPort(*port);
            } else if (const auto it = env.find("DSSP_PORT"); it != env.end()) {
                ServerConfig::setPort(static_cast<std::uint16_t>(std::stoul(it->second)));
            }

            auto setPathFromCliOrEnv = [&](const std::string &option, const std::string &envName, void (*setter)(const std::filesystem::path &)) {
                std::filesystem::path value;
                if (const auto opt = result.valueForOption<std::string>(option)) {
                    value = stdc::path::from_utf8(*opt);
                } else {
                    const auto it = env.find(envName);
                    if (it != env.end()) {
                        value = stdc::path::from_utf8(it->second);
                    } else {
                        return;
                    }
                }
                setter(value);
            };
            setPathFromCliOrEnv("--package-dir", "DSSP_PACKAGE_DIR", ServerConfig::setPackageDir);
            setPathFromCliOrEnv("--game-model-dir", "DSSP_GAME_MODEL_DIR", ServerConfig::setGAMEModelDir);
            setPathFromCliOrEnv("--rmvpe-model-path", "DSSP_RMVPE_MODEL_PATH", ServerConfig::setRMVPEModelPath);
        }

        int runServe(const stdc::cli::ParseResult &result) {
            parseConfig(result);
            auto &app = drogon::app();
            app.addListener(ServerConfig::host(), ServerConfig::port());
            app.run();
            return 0;
        }

    }

    stdc::cli::Command serveCommand = stdc::cli::Command{"serve", "Start the server"}
        .addHelpOption()
        .addOption(stdc::cli::Option({"--host"}, "Host to listen on").arg("host"))
        .addOption(stdc::cli::Option({"--port"}, "Port to listen on").arg(stdc::cli::Argument("port").type<std::uint16_t>()))
        .addOption(stdc::cli::Option({"--package-dir"}, "Directory of packages").arg("dir"))
        .addOption(stdc::cli::Option({"--game-model-dir"}, "Directory of game models").arg("dir"))
        .addOption(stdc::cli::Option({"--rmvpe-model-path"}, "Path to the RMVPE model").arg("path"))
        .setHandler(runServe);

}
