#include <array>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/support/logging.h>
#include <stdcorelib/system.h>

#include <dsspapp/Logger.h>
#include <dsspcli/DumpDevices.h>
#include <dsspcli/Serve.h>

namespace {

    int configureLogging(const stdc::cli::ParseResult &result) {
        using dssp::LogLevel;
        using namespace std::literals::string_view_literals;

        static constexpr std::array levels{
            std::pair{"trace"sv, LogLevel::Trace},
            std::pair{"debug"sv, LogLevel::Debug},
            std::pair{"success"sv, LogLevel::Success},
            std::pair{"info"sv, LogLevel::Info},
            std::pair{"warning"sv, LogLevel::Warning},
            std::pair{"critical"sv, LogLevel::Critical},
            std::pair{"fatal"sv, LogLevel::Fatal},
        };

        auto configured = result.valueForOption<std::string>("--log-level");
        if (!configured) {
            const auto environment = stdc::system::environment();
            if (const auto it = environment.find("DSSP_LOG_LEVEL"); it != environment.end()) {
                configured = it->second;
            }
        }

        const bool verbose = result.option("--verbose").has_value();
        const bool quiet = result.option("--quiet").has_value();
        auto selected = std::pair{"info"sv, LogLevel::Info};
        std::string warning;
        if (configured) {
            bool valid = false;
            for (const auto &entry : levels) {
                if (entry.first == *configured) {
                    selected = entry;
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                warning = "Invalid log level '" + *configured + "'; falling back to info. ";
            }
        } else if (verbose) {
            selected = {"debug"sv, LogLevel::Debug};
        } else if (quiet) {
            selected = {"warning"sv, LogLevel::Warning};
        }

        if (static_cast<int>(configured.has_value()) + verbose + quiet > 1) {
            warning += "Multiple logging options set. ";
        }

        if (!warning.empty()) {
            // Report the decision before a critical/fatal threshold can hide it.
            const stdc::LogCategory log("dssp.Main");
            log.stdcWarning("%1Using log level '%2'.", warning, selected.first);
        }

        dssp::setLogLevel(selected.second);
        return 0;
    }

}

int main(int argc, char **argv) {
    using namespace dssp;

    installLogger();

    // stdcorelib's command-line/system APIs form the outer library boundary.
    try {
        auto mainCommand = stdc::cli::Command{stdc::system::application_name(), DSSP_DESCRIPTION}
            .addVersionOption(DSSP_VERSION)
            .addHelpOption(true, false)
            .addOption(stdc::cli::Option({"--log-level"}, "Log level (DSSP_LOG_LEVEL), default info").arg("trace|debug|success|info|warning|critical|fatal").recursive())
            .addOption(stdc::cli::Option({"-V", "--verbose"}, "Set log level to debug").recursive())
            .addOption(stdc::cli::Option({"-q", "--quiet"}, "Set log level to warning").recursive())
            .setPreHandler(configureLogging)
            .addCommand(dumpDevicesCommand)
            .addCommand(serveCommand);

        stdc::cli::Parser parser(mainCommand);
        return parser.invoke(stdc::system::command_line_arguments().vec());
    } catch (const std::exception &error) {
        const stdc::LogCategory log("dssp.Main");
        log.stdcCritical("Command-line processing failed: %1", error.what());
        return 1;
    }
}
