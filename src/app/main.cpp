#include <exception>

#include <stdcorelib/support/commandline.h>
#include <stdcorelib/support/logging.h>
#include <stdcorelib/system.h>

#include <dsspapp/Logger.h>
#include <dsspcli/DumpDevices.h>
#include <dsspcli/Serve.h>

int main(int argc, char **argv) {
    using namespace dssp;

    installLogger();

    // stdcorelib's command-line/system APIs form the outer library boundary.
    try {
        auto mainCommand = stdc::cli::Command{stdc::system::application_name(), DSSP_DESCRIPTION}
            .addVersionOption(DSSP_VERSION)
            .addHelpOption(true, false)
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
