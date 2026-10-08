#ifndef DSSP_ARCHDIFFSINGER_SYNTHESIS_H
#define DSSP_ARCHDIFFSINGER_SYNTHESIS_H

#include <dssparch/Architecture.h>
#include <dssparchdiffsinger/SingerCatalog.h>
#include <dsspresource/TaskExecutor.h>

namespace dssp::arch_diffsinger {
    // Preparation and result conversion run on ordinary task workers; only the
    // synchronous native start() contract runs on synthesis inference workers.
    void synthesizeDuration(std::shared_ptr<const SingerCatalog>, resource::TaskExecutor &, arch::DurationInput, std::stop_token, arch::Completion<arch::DurationOutput>);
    void synthesizeParameters(std::shared_ptr<const SingerCatalog>, resource::TaskExecutor &, arch::ParameterInput, std::stop_token, arch::Completion<arch::ParameterOutput>);
    void synthesizeAudio(std::shared_ptr<const SingerCatalog>, resource::TaskExecutor &, arch::AudioInput, std::stop_token, arch::Completion<arch::AudioOutput>);
}

#endif // DSSP_ARCHDIFFSINGER_SYNTHESIS_H
