#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <dsinfer/Inference/InferenceDriverPlugin.h>
#include <gtest/gtest.h>
#include <synthrt/Core/SynthUnit.h>
#include <wolf/Session/LinguistSession.h>

#include <dssparchdiffsinger/DiffSinger.h>
#include <dssparchdiffsinger/Factory.h>
#include <dsspcodecutil/codecutil.h>
#include <dssptests/TestFiles.h>

namespace dssp::tests {
    namespace {
        constexpr char gif[] = "GIF89a\x01\x00\x01\x00\x80\x00\x00\x00\x00\x00\xff\xff\xff\x2c\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x02\x44\x01\x00\x3b";

        void writeSingerPackage(const TestFiles &files, std::string_view directory, std::string_view version) {
            const std::filesystem::path root(directory);
            files.write(root / "desc.json", "{\"$version\":\"1.0\",\"id\":\"example/singers\",\"version\":\"" + std::string(version) + R"(","runtimeLevel":1,"contributions":{
                "inference":[{"id":"acoustic","path":"acoustic.json"},{"id":"vocoder","path":"vocoder.json"},{"id":"g2p","path":"g2p.json"}],
                "linguist":[{"id":"eng","path":"eng.json"}],
                "singer":[{"id":"first","path":"first.json"},{"id":"second","path":"second.json"}]}})");
            files.write(root / "acoustic.json", R"({"interface":"org.openvpi.dsinfer.inference.Acoustic","variant":"onnx","level":1,
                "exports":{"varianceControls":["energy"],"transitionControls":["gender","velocity"]},
                "configuration":{"model":"${root}/missing-acoustic.onnx","phonemes":"phonemes.json","parameters":["energy","gender","velocity"]}})");
            files.write(root / "vocoder.json", R"({"interface":"org.openvpi.dsinfer.inference.Vocoder","variant":"onnx","level":1,
                "exports":{},"configuration":{"model":"${root}/missing-vocoder.onnx","pitchControllable":true}})");
            files.write(root / "phonemes.json", R"({"a":0,"SP":1,"AP":2})");
            files.write(root / "g2p.json", R"({"interface":"org.openvpi.wolf.inference.G2P","variant":"pipe-chain","level":1,
                "exports":{"languages":[{"language":"eng","scheme":"test"}],"symbols":["a"]},
                "configuration":{"formatVersion":1,"steps":[{"step":"fallback","params":{}}]}})");
            files.write(root / "eng.json", R"({"interface":"org.openvpi.wolf.linguist.WolfLinguist","variant":"wolf","level":1,
                "language":"eng","scheme":"test","name":{"_":"English","ja":"英語"},
                "exports":{"phonemes":["a"]},"configuration":{},"imports":[{"role":"linguist/g2p","ref":":inference/g2p"}]})");
            for (const auto id : {"first", "second"}) {
                const std::string languageRole = std::string_view(id) == "first" ? "language/eng" : "custom/english";
                files.write(root / (std::string(id) + ".json"), R"({"interface":"org.openvpi.dsinfer.singer.DiffSinger","variant":"openvpi","level":1,
                    "name":{"_":"Singer","zh":"歌手"},"avatar":{"_":"pixel.bin","zh":"missing-image.bin"},"demoAudio":"", "configuration":{"dict":"unused-dictionary.txt"},
                    "languages":{"eng":")" + languageRole + R"("},"defaultLanguage":"eng","imports":[
                        {"role":"singer/acoustic","ref":":inference/acoustic","options":{}},
                        {"role":"singer/vocoder","ref":":inference/vocoder","options":{}},
                        {"role":")" + languageRole + R"(","ref":":linguist/eng","options":{}}]})");
            }
            files.write(root / "pixel.bin", std::string_view(gif, sizeof(gif) - 1));
        }
    }

    class SingerDiscoveryTest : public ::testing::Test {
    protected:
        void SetUp() override {
            writeSingerPackage(files, "one", "1");
            writeSingerPackage(files, "two", "2");
            inference::EnvironmentOptions options;
            options.packageRoots = {files.path()};
            const std::filesystem::path plugins(DSSP_TEST_PLUGIN_DIR);
            options.pluginPaths = arch_diffsinger::pluginPaths(plugins);
            options.pluginPaths.push_back({"inference", plugins / "dsinfer" / "inferenceinterpreters"});
            options.pluginPaths.push_back({"inference", plugins / "wolf" / "inferenceinterpreters"});
            options.pluginPaths.push_back({"linguist", plugins / "wolf" / "linguistproviders"});
            const auto host = inference::Environment::create(options);
            ASSERT_TRUE(host) << (host ? "" : host.error().formatChain());
            environment = *host;
            ASSERT_TRUE(environment->problems().empty()) << (environment->problems().empty() ? "" : environment->problems().front().error.formatChain());
            const auto mediaResources = media::Resources::create(std::filesystem::path(DSSP_TEST_RESOURCE_DIR) / "media" / "magic.mgc");
            ASSERT_TRUE(mediaResources);
            const auto created = arch_diffsinger::create({environment, *mediaResources, std::filesystem::path(DSSP_TEST_RESOURCE_DIR) / "localization"});
            ASSERT_TRUE(created) << (created ? "" : created.error().formatChain());
            implementation = *created;
        }
        TestFiles files;
        std::shared_ptr<inference::Environment> environment;
        arch::ArchitecturePtr implementation;
    };

    TEST_F(SingerDiscoveryTest, MissingModelsDoNotPreventDiscoveryAndVersionsHaveDistinctStableIdentities) {
        ASSERT_EQ(environment->packages().size(), 2U);
        const auto singers = implementation->singers({});
        ASSERT_TRUE(singers);
        ASSERT_EQ(singers->size(), 4U);
        EXPECT_TRUE(environment->unit().runtimeServices(ds::InferenceDriverPlugin::IID).empty());
        const auto *diffsinger = dynamic_cast<const arch_diffsinger::DiffSinger *>(implementation.get());
        ASSERT_NE(diffsinger, nullptr);
        for (const auto &singer : *singers) {
            SCOPED_TRACE(singer.id);
            EXPECT_EQ(singer.arch, "diffsinger");
            EXPECT_EQ(singer.defaultLanguage, "eng");
            ASSERT_EQ(singer.languages.size(), 1U);
            EXPECT_EQ(singer.languages.at("eng").name, "English");
            EXPECT_TRUE(singer.languages.at("eng").defaultLyric.empty());
            EXPECT_FALSE(singer.archSpecificInfo["hasDuration"].asBool());
            EXPECT_EQ(singer.archSpecificInfo["languageCapabilities"]["eng"]["maxDepth"].asString(), "pronunciation");
            const auto *record = diffsinger->catalog().find(singer.id);
            ASSERT_NE(record, nullptr);
            EXPECT_EQ(environment->languages().probe(record->ref, "eng").readiness, wolf::Readiness::Cold);
            EXPECT_FALSE(std::filesystem::exists(record->package.path() / "missing-acoustic.onnx"));
            const auto single = implementation->singer({singer.id, {"zh-CN"}});
            ASSERT_TRUE(single);
            ASSERT_TRUE(*single);
            EXPECT_EQ((**single).name, "歌手");
            const auto japanese = implementation->singer({singer.id, {"ja-JP"}});
            ASSERT_TRUE(japanese);
            ASSERT_TRUE(*japanese);
            EXPECT_EQ((**japanese).languages.at("eng").name, "英語");
        }
        const auto repeated = arch_diffsinger::SingerCatalog::discover(environment);
        ASSERT_TRUE(repeated);
        EXPECT_EQ((*repeated)->singers().size(), singers->size());
        for (const auto &singer : *singers) {
            ASSERT_NE((*repeated)->find(singer.id), nullptr);
            EXPECT_EQ((*repeated)->find(singer.id)->info.group, singer.group);
        }
    }

    TEST_F(SingerDiscoveryTest, GroupsReflectAvailableStagesAndSharedEffectivePipelines) {
        const auto groups = implementation->groups({});
        ASSERT_TRUE(groups);
        ASSERT_EQ(groups->size(), 2U);
        for (const auto &group : *groups) {
            EXPECT_FALSE(group.mixable);
            EXPECT_EQ(group.pronunciationMode, arch::PronunciationMode::Full);
            EXPECT_EQ(group.phonemeMode, arch::PhonemeMode::Skip);
            EXPECT_TRUE(std::holds_alternative<arch::DirectParameter>(group.parameterPipeline.at("pitch")));
            EXPECT_TRUE(group.parameterPipeline.contains("gender"));
            EXPECT_TRUE(group.parameterPipeline.contains("velocity"));
            EXPECT_TRUE(group.parameterPipeline.contains("tone_shift"));
            EXPECT_FALSE(group.parameterPipeline.contains("expressiveness"));
            const auto members = implementation->groupSingers({group.id, {}});
            ASSERT_TRUE(members);
            ASSERT_EQ(members->size(), 2U);
            EXPECT_NE((*members)[0].id, (*members)[1].id);
            for (const auto &singer : *members) {
                EXPECT_EQ(singer.group, group.id);
            }
            const auto single = implementation->group({group.id, {}});
            ASSERT_TRUE(single);
            ASSERT_TRUE(*single);
            EXPECT_EQ((**single).id, group.id);
        }
    }

    TEST_F(SingerDiscoveryTest, MediaUsesRawBytesAndNegotiationWhileAbsentOptionalResourcesAreEmpty) {
        const auto singers = implementation->singers({});
        ASSERT_TRUE(singers);
        ASSERT_FALSE(singers->empty());
        arch::SingerMediaQuery query{{singers->front().id, {}}, {{"IMAGE/GIF"}, {"DATA"}}};
        const auto avatar = implementation->avatar(query);
        ASSERT_TRUE(avatar);
        ASSERT_TRUE(avatar->starts_with("data:image/gif;base64,"));
        const std::string_view encoded(*avatar);
        const auto payload = encoded.substr(encoded.find(',') + 1);
        const codecutil::Base64DecodingView decoded(payload);
        EXPECT_EQ(std::string(decoded.begin(), decoded.end()), std::string(gif, sizeof(gif) - 1));
        const auto background = implementation->background(query);
        ASSERT_TRUE(background);
        EXPECT_TRUE(background->empty());
        const auto demo = implementation->demoAudio(query);
    ASSERT_TRUE(demo) << (demo ? "" : demo.error().asError().formatChain());
        EXPECT_TRUE(demo->empty());
        query.media.acceptableFormats = {"image/png"};
        const auto format = implementation->avatar(query);
        ASSERT_FALSE(format);
        EXPECT_NE(format.error().get_if<arch::InvalidInputError>(), nullptr);
        query.media.acceptableFormats.clear();
        query.media.acceptableSchemes = {"https"};
        const auto scheme = implementation->avatar(query);
        ASSERT_FALSE(scheme);
        EXPECT_NE(scheme.error().get_if<arch::InvalidInputError>(), nullptr);
        query.media.acceptableSchemes.clear();
        query.singer.display.acceptLanguage = "zh-CN";
        const auto missingLocalizedResource = implementation->avatar(query);
        ASSERT_FALSE(missingLocalizedResource);
        EXPECT_NE(missingLocalizedResource.error().get_if<arch::OperationError>(), nullptr);
    }

    TEST_F(SingerDiscoveryTest, ConcurrentReadersUseTheCommittedSingerSnapshot) {
        std::atomic<unsigned> failures = 0;
        std::vector<std::jthread> workers;
        for (unsigned i = 0; i < 8; ++i) {
            workers.emplace_back([&] {
                for (unsigned j = 0; j < 16; ++j) {
                    const auto singers = implementation->singers({j % 2 ? "zh" : "ja"});
                    if (!singers || singers->size() != 4U) {
                        ++failures;
                    }
                }
            });
        }
        workers.clear();
        EXPECT_EQ(failures.load(), 0U);
    }

    TEST_F(SingerDiscoveryTest, MovingAnInstallationDoesNotChangeSingerOrGroupIdentities) {
        TestFiles relocated;
        writeSingerPackage(relocated, "moved", "1");
        inference::EnvironmentOptions options;
        options.packageRoots = {relocated.path()};
        const std::filesystem::path plugins(DSSP_TEST_PLUGIN_DIR);
        options.pluginPaths = arch_diffsinger::pluginPaths(plugins);
        options.pluginPaths.push_back({"inference", plugins / "dsinfer" / "inferenceinterpreters"});
        options.pluginPaths.push_back({"inference", plugins / "wolf" / "inferenceinterpreters"});
        options.pluginPaths.push_back({"linguist", plugins / "wolf" / "linguistproviders"});
        const auto host = inference::Environment::create(options);
        ASSERT_TRUE(host);
        const auto catalog = arch_diffsinger::SingerCatalog::discover(*host);
        ASSERT_TRUE(catalog);
        ASSERT_EQ((*catalog)->singers().size(), 2U);
        for (const auto &[id, singer] : (*catalog)->singers()) {
            const auto original = implementation->singer({id, {}});
            ASSERT_TRUE(original);
            ASSERT_TRUE(*original);
            EXPECT_EQ((**original).group, singer.info.group);
        }
    }
}
