#include <stdcorelib/support/commandline.h>
#include <stdcorelib/system.h>

#include <dssp/DumpDevices.h>
#include <dssp/Serve.h>

int main(int argc, char **argv) {
    using namespace dssp;

    stdc::cli::Parser parser(
        stdc::cli::Command{stdc::system::application_name(), DSSP_DESCRIPTION}
            .addVersionOption(DSSP_VERSION)
            .addHelpOption(true, false)
            .addCommand(dumpDevicesCommand)
            .addCommand(serveCommand)
    );

    return parser.invoke(stdc::system::command_line_arguments());
}
