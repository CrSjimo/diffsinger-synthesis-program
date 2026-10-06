#include <initializer_list>

#include <gtest/gtest.h>

#include <dsspinference/Display.h>
#include <dsspinference/MediaPath.h>

namespace dssp::tests {
    TEST(MediaPath, ExplicitEmptyStringDoesNotBecomeThePackageDirectory) {
        const auto path = inference::preserveEmptyMediaPaths(srt::DisplayText("/package"), srt::JsonValue(""));
        ASSERT_TRUE(path);
        EXPECT_TRUE(path->text().empty());
        EXPECT_TRUE(path->locales().empty());
    }

    TEST(MediaPath, NonemptyPathsRetainNativeResolution) {
        for (const auto declared : {".", "missing.wav", "clip.wav"}) {
            SCOPED_TRACE(declared);
            const srt::DisplayText resolved("/package/resolved", {{"ja", "/package/localized"}});
            const auto path = inference::preserveEmptyMediaPaths(resolved, srt::JsonValue(declared));
            ASSERT_TRUE(path);
            EXPECT_EQ(path->text(), resolved.text());
            EXPECT_EQ(path->text("ja"), resolved.text("ja"));
        }
    }

    TEST(MediaPath, LanguageMapsPreserveEmptySlotsAndNonemptyLocalizedResources) {
        const srt::DisplayText resolved("/package", {{"ja", "/package/ja-demo.wav"}, {"zh", "/package"}});
        const srt::JsonValue declared(srt::JsonObject{{"_", ""}, {"ja", "ja-demo.wav"}, {"zh", ""}});
        const auto path = inference::preserveEmptyMediaPaths(resolved, declared);
        ASSERT_TRUE(path);
        EXPECT_TRUE(path->text().empty());
        EXPECT_TRUE(path->text("zh").empty());
        EXPECT_EQ(path->text("ja"), "/package/ja-demo.wav");
        const auto defaultPath = inference::localize(*path, "de");
        ASSERT_TRUE(defaultPath);
        EXPECT_TRUE(defaultPath->empty());
        const auto japanese = inference::localize(*path, "ja-JP");
        ASSERT_TRUE(japanese);
        EXPECT_EQ(*japanese, "/package/ja-demo.wav");
        const auto chinese = inference::localize(*path, "zh-CN");
        ASSERT_TRUE(chinese);
        EXPECT_TRUE(chinese->empty());
    }
}
