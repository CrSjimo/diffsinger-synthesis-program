#ifndef DSSP_MEDIA_RESOURCES_H
#define DSSP_MEDIA_RESOURCES_H

#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include <dsspmedia/MediaError.h>

namespace dssp::media {
    enum class Kind { Image, Audio };

    struct Resource {
        std::string mime;
        std::string dataUrl;
    };

    class Resources final {
    public:
        static std::expected<std::shared_ptr<Resources>, MediaError> create(const std::filesystem::path &dictionary);
        ~Resources();
        Resources(const Resources &) = delete;
        Resources &operator=(const Resources &) = delete;

        std::expected<std::string, MediaError> mime(std::span<const char>, Kind) const;
        std::expected<Resource, MediaError> read(const std::filesystem::path &, Kind) const;

    private:
        class Impl;
        explicit Resources(std::unique_ptr<Impl>);
        std::unique_ptr<Impl> impl_;
    };
}

#endif // DSSP_MEDIA_RESOURCES_H
