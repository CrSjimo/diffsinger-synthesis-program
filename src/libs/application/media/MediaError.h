#ifndef DSSP_MEDIA_MEDIAERROR_H
#define DSSP_MEDIA_MEDIAERROR_H

#include <dssperror/ErrorSet.h>

namespace dssp::media {
    class MediaError final : public ErrorType<MediaError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.media.resource"; }
    };

    class InvalidMediaError final : public ErrorType<InvalidMediaError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.media.invalid_input"; }
    };

    using MediaInputErrors = ErrorSet<InvalidMediaError, MediaError>;
}

#endif // DSSP_MEDIA_MEDIAERROR_H
