#ifndef DSSP_DEVICEERROR_H
#define DSSP_DEVICEERROR_H

#include <dssperror/Error.h>

namespace dssp::runtime {

    class DeviceError final : public ErrorType<DeviceError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.device"; }
    };

}

#endif // DSSP_DEVICEERROR_H
