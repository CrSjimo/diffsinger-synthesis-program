#include <CLI/CLI.hpp>

#include <dssp/DumpDevices.h>

namespace {
    constexpr const char *kAppDescription = "DSSP - DiffSinger Synthesis Program";
}

int main(int argc, char **argv) {
    using namespace dssp;

    CLI::App app{kAppDescription};
    app.require_subcommand();

    auto *dumpDevices = app.add_subcommand("dump-devices", "Dump information of all devices to JSON");
    dumpDevices->callback(runDumpDevices);

    CLI11_PARSE(app, argc, argv);
    return 0;
}
