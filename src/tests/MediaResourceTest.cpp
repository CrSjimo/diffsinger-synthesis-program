#include <atomic>
#include <filesystem>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include <dsspcodecutil/codecutil.h>
#include <dsspmedia/Resources.h>
#include <dssptests/TestFiles.h>

namespace dssp::tests {
    namespace {
        constexpr char gif[] = "GIF89a\x01\x00\x01\x00\x80\x00\x00\x00\x00\x00\xff\xff\xff\x2c\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x02\x44\x01\x00\x3b";
        // A mono PCM WAV with one silent sample; no decoder/model is needed.
        constexpr char wav[] = "RIFF\x26\x00\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x44\xac\x00\x00\x88\x58\x01\x00\x02\x00\x10\x00" "data\x02\x00\x00\x00\x00\x00";
    }

    TEST(MediaResource, IdentifiesOriginalBytesAndPreservesThemInTheDataUrl) {
        const auto resources = media::Resources::create(std::filesystem::path(DSSP_TEST_RESOURCE_DIR) / "media" / "magic.mgc");
        ASSERT_TRUE(resources);
        TestFiles files;
        files.write("image.wav", std::string_view(gif, sizeof(gif) - 1));
        files.write("audio.gif", std::string_view(wav, sizeof(wav) - 1));
        for (const auto &[file, kind, expectedMime, bytes] : {
                 std::tuple{"image.wav", media::Kind::Image, "image/gif", std::string_view(gif, sizeof(gif) - 1)},
                 std::tuple{"audio.gif", media::Kind::Audio, "audio/x-wav", std::string_view(wav, sizeof(wav) - 1)}}) {
            const auto resource = (*resources)->read(files.path() / file, kind);
            ASSERT_TRUE(resource);
            EXPECT_EQ(resource->mime, expectedMime);
            const std::string_view url(resource->dataUrl);
            const auto encoded = url.substr(url.find(',') + 1);
            const codecutil::Base64DecodingView decoded(encoded);
            EXPECT_EQ(std::string(decoded.begin(), decoded.end()), bytes);
        }

        const auto wrongKind = (*resources)->read(files.path() / "image.wav", media::Kind::Audio);
        EXPECT_FALSE(wrongKind);
        files.write("unrecognized", "not a media file");
        EXPECT_FALSE((*resources)->read(files.path() / "unrecognized", media::Kind::Image));
        EXPECT_FALSE((*resources)->read(files.path() / "missing", media::Kind::Image));
    }

    TEST(MediaResource, ConcurrentDetectionProtectsTheLibmagicCookie) {
        const auto resources = media::Resources::create(std::filesystem::path(DSSP_TEST_RESOURCE_DIR) / "media" / "magic.mgc");
        ASSERT_TRUE(resources);
        std::atomic<unsigned> failures = 0;
        std::vector<std::jthread> workers;
        for (unsigned i = 0; i < 8; ++i) {
            workers.emplace_back([&] {
                for (unsigned j = 0; j < 16; ++j) {
                    const auto result = (*resources)->mime(std::span(gif, sizeof(gif) - 1), media::Kind::Image);
                    if (!result || *result != "image/gif") {
                        ++failures;
                    }
                }
            });
        }

        workers.clear();
        EXPECT_EQ(failures.load(), 0U);
    }
}
