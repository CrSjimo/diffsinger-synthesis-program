#ifndef DSSP_APP_INFERENCECOMPOSITION_H
#define DSSP_APP_INFERENCECOMPOSITION_H

#include <dsspinference/Environment.h>
#include <dsspruntime/ServerConfig.h>

namespace dssp {
    std::expected<std::shared_ptr<inference::Environment>, inference::InitializationError> initializeInference(const runtime::ServerConfig &, std::vector<inference::PluginSearchPath> architecturePaths);
}

#endif // DSSP_APP_INFERENCECOMPOSITION_H
