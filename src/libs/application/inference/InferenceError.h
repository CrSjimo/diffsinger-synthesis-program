#ifndef DSSP_INFERENCE_INFERENCEERROR_H
#define DSSP_INFERENCE_INFERENCEERROR_H

#include <dssperror/Error.h>

namespace dssp::inference {

    class InitializationError final : public ErrorType<InitializationError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.inference.initialization"; }
    };
    class ResourceError final : public ErrorType<ResourceError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.inference.resource"; }
    };

}

#endif // DSSP_INFERENCE_INFERENCEERROR_H
