#include "Resources.h"

#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <mutex>
#include <vector>

#include <magic.h>
#include <stdcorelib/path.h>
#include <stdcorelib/support/logging.h>

#include <dsspcodecutil/codecutil.h>

namespace dssp::media {
    namespace {
        stdc::LogCategory log("dssp.media.Resources");
        std::expected<std::vector<char>, MediaError> readBytes(const std::filesystem::path &path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) {
                return std::unexpected{MediaError("Cannot open the declared media resource: " + stdc::path::to_utf8(path))};
            }

            const auto size = static_cast<std::streamoff>(file.tellg());
            if (size < 0 || static_cast<std::uintmax_t>(size) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) || static_cast<std::uintmax_t>(size) > std::vector<char>{}.max_size()) {
                return std::unexpected{MediaError("Cannot determine a supported media resource size: " + stdc::path::to_utf8(path))};
            }

            std::vector<char> bytes(static_cast<std::size_t>(size));
            file.seekg(0);
            if (!bytes.empty() && !file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
                return std::unexpected{MediaError("Cannot read the complete media resource: " + stdc::path::to_utf8(path))};
            }

            return bytes;
        }

        MediaError magicError(magic_t cookie, std::string_view operation) {
            const auto *message = magic_error(cookie);
            return MediaError("Media format library failed", std::make_shared<const ExternalError>(message ? message : "No libmagic diagnostic", NativeDiagnostic{"libmagic", std::string(operation), "errno", magic_errno(cookie)}));
        }
    }

    class Resources::Impl {
    public:
        // libmagic may retain the user-supplied compiled database buffer.
        std::vector<char> dictionary;
        magic_t cookie = nullptr;
        mutable std::mutex mutex;
        ~Impl() {
            if (cookie) {
                magic_close(cookie);
            }
        }
    };

    std::expected<std::shared_ptr<Resources>, MediaError> Resources::create(const std::filesystem::path &dictionary) {
        try {
            auto contents = readBytes(dictionary);
            if (!contents) {
                return std::unexpected{MediaError("Cannot read the deployed MIME database", contents.error().clone())};
            }

            auto impl = std::make_unique<Impl>();
            impl->dictionary = std::move(*contents);
            impl->cookie = magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR);
            if (!impl->cookie) {
                return std::unexpected{MediaError("Cannot create the MIME detector")};
            }

            void *buffer = impl->dictionary.data();
            auto size = impl->dictionary.size();
            // Reading through filesystem::path supports Unicode Windows paths;
            // no process code page or system-wide magic database is required.
            if (magic_load_buffers(impl->cookie, &buffer, &size, 1) != 0) {
                return std::unexpected{magicError(impl->cookie, "magic_load_buffers")};
            }

            log.stdcDebug("MIME database ready: %1 (%2 bytes)", stdc::path::to_utf8(dictionary), impl->dictionary.size());
            return std::shared_ptr<Resources>(new Resources(std::move(impl)));
        } catch (const std::exception &error) {
            return std::unexpected{MediaError("Media resource initialization failed", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"filesystem/libmagic", "initialize media resources", "exception"}))};
        }
    }

    Resources::Resources(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

    Resources::~Resources() = default;

    std::expected<std::string, MediaError> Resources::mime(std::span<const char> bytes, Kind kind) const {
        if (bytes.empty()) {
            return std::unexpected{MediaError("The declared media resource is empty")};
        }

        std::string result;
        {
            const std::lock_guard lock(impl_->mutex);
            const auto *type = magic_buffer(impl_->cookie, bytes.data(), bytes.size());
            if (!type) {
                return std::unexpected{magicError(impl_->cookie, "magic_buffer")};
            }
            result = type;
        }
        if (!result.starts_with(kind == Kind::Image ? "image/" : "audio/")) {
            return std::unexpected{MediaError("The declared resource has no recognizable format of the required media kind (detected " + result + ")")};
        }

        return result;
    }

    std::expected<Resource, MediaError> Resources::read(const std::filesystem::path &path, Kind kind) const {
        try {
            auto bytes = readBytes(path);
            if (!bytes) {
                return std::unexpected{std::move(bytes.error())};
            }

            auto type = mime(*bytes, kind);
            if (!type) {
                return std::unexpected{std::move(type.error())};
            }

            const codecutil::Base64EncodingView encoded(*bytes);
            if (log.isLevelEnabled(stdc::Logger::Trace)) {
                log.stdcTrace("Media resource %1: type=%2, bytes=%3", stdc::path::to_utf8(path), *type, bytes->size());
            }

            return Resource{*type, "data:" + *type + ";base64," + std::string(encoded.begin(), encoded.end())};
        } catch (const std::exception &error) {
            return std::unexpected{MediaError("Media resource read failed", std::make_shared<const ExternalError>(error.what(), NativeDiagnostic{"filesystem/libmagic", "read media resource", "exception"}))};
        }
    }
}
