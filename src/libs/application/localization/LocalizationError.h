#ifndef DSSP_LOCALIZATION_LOCALIZATIONERROR_H
#define DSSP_LOCALIZATION_LOCALIZATIONERROR_H

#include <dssperror/Error.h>

namespace dssp::localization {
    class LocalizationError final : public ErrorType<LocalizationError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.localization"; }
    };
}

#endif // DSSP_LOCALIZATION_LOCALIZATIONERROR_H
