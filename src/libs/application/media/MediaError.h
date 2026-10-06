#ifndef DSSP_MEDIA_MEDIAERROR_H
#define DSSP_MEDIA_MEDIAERROR_H

#include <dssperror/Error.h>

namespace dssp::media {
    class MediaError final : public ErrorType<MediaError> {
    public:
        using ErrorType::ErrorType;
        std::string_view kind() const noexcept override { return "dssp.media.resource"; }
    };
}

#endif // DSSP_MEDIA_MEDIAERROR_H
