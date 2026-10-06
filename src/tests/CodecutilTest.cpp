#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>

#include <dsspcodecutil/codecutil.h>

namespace dssp::tests {
    TEST(Codecutil, Base64EncodingMatchesRfc4648Vectors) {
        constexpr std::array<std::pair<std::string_view, std::string_view>, 7> vectors{{
            {"", ""},
            {"f", "Zg=="},
            {"fo", "Zm8="},
            {"foo", "Zm9v"},
            {"foob", "Zm9vYg=="},
            {"fooba", "Zm9vYmE="},
            {"foobar", "Zm9vYmFy"}
        }};
        for (const auto &[bytes, expected] : vectors) {
            SCOPED_TRACE(bytes);
            const codecutil::Base64EncodingView encoded(bytes);
            EXPECT_EQ(std::string(encoded.begin(), encoded.end()), expected);
            const codecutil::Base64EncodingView<codecutil::Base64Mode::URL> urlEncoded(bytes);
            EXPECT_EQ(std::string(urlEncoded.begin(), urlEncoded.end()), expected);
            const codecutil::Base64EncodingView<codecutil::Base64Mode::OmitEquals> unpadded(bytes);
            EXPECT_EQ(std::string(unpadded.begin(), unpadded.end()), expected.substr(0, expected.find('=')));
        }
    }

    TEST(Codecutil, Base64BinaryTailsPreserveAllBits) {
        constexpr std::array<std::tuple<std::string_view, std::string_view, std::string_view>, 4> vectors{{
            {std::string_view("\0;", 2), "ADs=", "ADs="},
            {"\xff", "/w==", "_w=="},
            {"\xfb\xff", "+/8=", "-_8="},
            {"abc\xfb\xff", "YWJj+/8=", "YWJj-_8="}
        }};
        for (const auto &[bytes, expected, expectedUrl] : vectors) {
            SCOPED_TRACE(expected);
            const codecutil::Base64EncodingView encoded(bytes);
            EXPECT_EQ(std::string(encoded.begin(), encoded.end()), expected);
            const codecutil::Base64EncodingView<codecutil::Base64Mode::URL> urlEncoded(bytes);
            EXPECT_EQ(std::string(urlEncoded.begin(), urlEncoded.end()), expectedUrl);
            const codecutil::Base64EncodingView<codecutil::Base64Mode::OmitEquals> unpadded(bytes);
            EXPECT_EQ(std::string(unpadded.begin(), unpadded.end()), expected.substr(0, expected.find('=')));
        }
    }
}
