#include <gtest/gtest.h>

#include <dssphttp/MediaType.h>

namespace dssp::tests {
    TEST(MediaType, NamesAcceptCaseVendorNamesAndStructuredSuffixes) {
        for (const auto text : {"audio/wav", "Audio/X-WAV", "application/vnd.example+json", "font/woff2"}) {
            SCOPED_TRACE(text);
            const auto result = http::matchesMediaTypeName(text);
            ASSERT_TRUE(result);
            EXPECT_TRUE(*result);
        }
        const auto longest = http::matchesMediaTypeName(std::string(127, 'a') + "/" + std::string(127, 'b'));
        ASSERT_TRUE(longest);
        EXPECT_TRUE(*longest);
        const auto tooLong = http::matchesMediaTypeName("audio/" + std::string(128, 'b'));
        ASSERT_TRUE(tooLong);
        EXPECT_FALSE(*tooLong);
    }

    TEST(MediaType, NamesRejectParametersWildcardsWhitespaceAndTrailingData) {
        for (const auto text : {"", "audio", "audio/", "/wav", "audio/wav;codecs=pcm", "audio/*", "*/wav", "*/*", "audio/wav,audio/flac", " audio/wav", "audio/wav ", "audio /wav", "audio/wav/extra", "audio/wav\n", "audio/\xC3\xA9"}) {
            SCOPED_TRACE(text);
            const auto result = http::matchesMediaTypeName(text);
            ASSERT_TRUE(result);
            EXPECT_FALSE(*result);
        }
        const auto nul = http::matchesMediaTypeName(std::string("audio/wav\0ignored", 17));
        ASSERT_TRUE(nul);
        EXPECT_FALSE(*nul);
    }

    TEST(MediaType, JsonContentTypeAcceptsHttpParametersAndRejectsMalformedValues) {
        for (const auto text : {"application/json", "Application/JSON; charset=utf-8", "\tapplication/json \t", "application/json; note=\"a,b;c\"", "application/json; note=\"a\\\"b\\\\c\"", "application/json;;"}) {
            SCOPED_TRACE(text);
            const auto result = http::matchesJsonContentType(text);
            ASSERT_TRUE(result);
            EXPECT_TRUE(*result);
        }
        for (const auto text : {"", "text/plain", "application/jsonp", "application/json; charset", "application/json;=x", "application/json; note=\"unclosed", "application/json; note=\"bad\nvalue\"", "application/json\r\n"}) {
            SCOPED_TRACE(text);
            const auto result = http::matchesJsonContentType(text);
            ASSERT_TRUE(result);
            EXPECT_FALSE(*result);
        }
    }
}
