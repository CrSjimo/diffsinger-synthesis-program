#include "DemucsSpectralReference.h"
#include "SpectralTransform.h"
#include "TestFiles.h"

#include <dr_wav.h>

#include <chrono>
#include <cmath>
#include <future>
#include <thread>

#include <gtest/gtest.h>
#include <synthrt/Core/SynthUnit.h>

#include <dsspapiv1/ExtractionMapping.h>
#include <dsspapiv1/ResponseCodec.h>
#include <dsspcodecutil/codecutil.h>
#include <dsspextraction/SeparationTypes.h>
#include <dsspextraction/Service.h>
#include <dsspinference/ResourceIdentity.h>
#include <dsspmedia/Wave.h>

namespace dssp::tests {
    using namespace std::chrono_literals;
    namespace Sep = srtsepinfer::Api::Separation::L1;
    namespace {
        constexpr char Id[] = "example/separation=1.0:inference/demucs";
        std::string wave(int rate = 44100, int channels = 2) {
            std::vector<float> samples(501 * channels);
            for (std::size_t i = 0; i < samples.size(); ++i)
                samples[i] = (i % channels == 0) ? 0.25f : -0.125f;
            const drwav_data_format format{drwav_container_riff, DR_WAVE_FORMAT_IEEE_FLOAT, static_cast<drwav_uint32>(channels), static_cast<drwav_uint32>(rate), 32};
            void *bytes = nullptr;
            std::size_t size = 0;
            drwav writer{};
            EXPECT_TRUE(drwav_init_memory_write(&writer, &bytes, &size, &format, nullptr));
            EXPECT_EQ(drwav_write_pcm_frames(&writer, 501, samples.data()), 501);
            EXPECT_EQ(drwav_uninit(&writer), DRWAV_SUCCESS);
            const codecutil::Base64EncodingView encoded(std::span(static_cast<const std::uint8_t *>(bytes), size));
            auto result = "data:audio/wav;base64," + std::string(encoded.begin(), encoded.end());
            drwav_free(bytes, nullptr);
            return result;
        }
        class FakeSeparator final : public Sep::SeparationExecutive {
        public:
            FakeSeparator(srt::InferenceSpec &spec, bool invalid, std::shared_ptr<std::promise<void>> started)
                : SeparationExecutive(spec), invalid_(invalid), started_(std::move(started)) {}
            ~FakeSeparator() override {
                (void)stop();
                (void)waitForFinished();
            }

        protected:
            srt::Expected<std::unique_ptr<Sep::SeparationResult>> run(const Sep::SeparationStartInput &input) override {
                if (started_) {
                    started_->set_value();
                    const auto deadline = std::chrono::steady_clock::now() + 3s;
                    while (!cancelled() && std::chrono::steady_clock::now() < deadline)
                        std::this_thread::sleep_for(1ms);
                    if (cancelled())
                        return srtsepinfer::cancelledError();
                    return srt::Error(srtsepinfer::SeparationError::Internal, "Native cancellation did not arrive");
                }
                auto result = std::make_unique<Sep::SeparationResult>();
                for (const auto &id : {"drums", "bass", "other", "vocals"})
                    result->tracks.push_back({id, {input.audio.sampleRate, input.audio.channelCount, input.audio.samples, input.audio.startTime}});
                if (invalid_)
                    result->tracks.back().audio.samples.pop_back();
                return result;
            }

        private:
            bool invalid_;
            std::shared_ptr<std::promise<void>> started_;
        };
    }
    class SeparationTest : public ::testing::Test {
    protected:
        void SetUp() override {
            files.write("package/desc.json", R"({"$version":"1.0","id":"example/separation","version":"1","runtimeLevel":1,"contributions":{"inference":[{"id":"demucs","path":"demucs.json"}]}})");
            files.write("package/demucs.json", R"({"name":"Demucs","interface":"org.openvpi.srtsepinfer.inference.Separation","variant":"demucs","level":1,"exports":{"sampleRate":44100,"channelCount":2,"tracks":[{"id":"drums","description":{"_":"Drums","zh-CN":"鼓"}},{"id":"bass","description":"Bass"},{"id":"other","description":"Other"},{"id":"vocals","description":"Vocals"}]},"configuration":{"model":"missing.onnx","segmentSamples":343980,"fftSize":4096,"hopLength":1024,"overlap":0.25}})");
            inference::EnvironmentOptions options;
            options.packageRoots = {files.path()};
            options.pluginPaths = {{"inference", DSSP_TEST_SEPARATION_PLUGIN_DIR}};
            auto made = inference::Environment::create(options);
            ASSERT_TRUE(made);
            environment = *made;
            ASSERT_TRUE(environment->problems().empty()) << (environment->problems().empty() ? "" : environment->problems().front().error.formatChain());
        }
        void TearDown() override {
            if (service)
                service->shutdown();
            if (environment)
                environment->shutdown();
        }
        void install(bool invalid = false, std::shared_ptr<std::promise<void>> started = {}) {
            auto *category = dynamic_cast<srt::InferenceCategory *>(environment->unit().category("inference"));
            ASSERT_NE(category, nullptr);
            auto specs = category->inferences();
            ASSERT_EQ(specs.size(), 1);
            auto *spec = specs.front();
            auto pool = environment->inferences("extraction-separation");
            ASSERT_TRUE(pool);
            inference::InferenceDefinition definition;
            definition.key = inference::resourceReference(*spec) + "/default";
            definition.packages = {spec->package()};
            definition.create = [spec, invalid, started]() -> std::expected<std::unique_ptr<srt::InferenceExecutive>, inference::ExecutionError> { return std::make_unique<FakeSeparator>(*spec, invalid, started); };
            ASSERT_TRUE((*pool)->bind(std::move(definition)));
        }
        void start() {
            auto made = extraction::Service::create(environment);
            ASSERT_TRUE(made);
            service = *made;
        }
        auto submit(extraction::Input input, std::stop_token token = {}) {
            auto promise = std::make_shared<std::promise<extraction::ExtractionResult<extraction::SeparationOutput>>>();
            auto future = promise->get_future();
            service->separation(std::move(input), token, [promise](auto result) { promise->set_value(std::move(result)); });
            return future;
        }
        TestFiles files;
        std::shared_ptr<inference::Environment> environment;
        std::shared_ptr<extraction::Service> service;
    };

    TEST_F(SeparationTest, ColdDiscoveryExportsLocalizedOrderedTracksWithoutLoadingModel) {
        start();
        auto list = service->extractors("zh-CN");
        ASSERT_TRUE(list);
        ASSERT_EQ(list->separation.size(), 1);
        EXPECT_TRUE(list->note.empty());
        EXPECT_TRUE(list->pitch.empty());
        const auto &info = list->separation.front();
        EXPECT_EQ(info.id, Id);
        ASSERT_EQ(info.tracks.size(), 4);
        EXPECT_EQ(info.tracks.front().name, "drums");
        EXPECT_EQ(info.tracks.front().description, "鼓");
        auto json = api_v1::toJson(api_v1::toDto(info));
        EXPECT_EQ(json["tracks"][0]["name"].asString(), "drums");
        EXPECT_EQ(json["tracks"][0]["description"].asString(), "鼓");
        auto pool = environment->inferences("extraction-separation");
        ASSERT_TRUE(pool);
        EXPECT_EQ((*pool)->residentCount(), 0);
        EXPECT_FALSE(service->extractor(extraction::Kind::Pitch, Id));
    }
    TEST_F(SeparationTest, PreservesStereoAndPartialTailInEveryWaveOutput) {
        install();
        start();
        auto future = submit({Id, wave()});
        ASSERT_EQ(future.wait_for(5s), std::future_status::ready);
        auto result = future.get();
        ASSERT_TRUE(result) << result.error().asError().formatChain();
        ASSERT_EQ(result->tracks.size(), 4);
        for (const auto &track : result->tracks) {
            auto decoded = media::readWaveDataUrl(track.audioUrl);
            ASSERT_TRUE(decoded);
            EXPECT_EQ(decoded->channelCount, 2);
            EXPECT_EQ(decoded->sampleRate, 44100);
            ASSERT_EQ(decoded->samples.size(), 1002);
            EXPECT_FLOAT_EQ(decoded->samples[1000], 0.25f);
            EXPECT_FLOAT_EQ(decoded->samples[1001], -0.125f);
        }
    }
    TEST_F(SeparationTest, RejectsWrongFormatBeforeOpeningModel) {
        start();
        for (const auto &input : std::vector<extraction::Input>{{Id, wave(48000)}, {Id, wave(44100, 1)}, {Id, "https://example.invalid/audio.wav"}, {"missing", wave()}}) {
            auto future = submit(input);
            ASSERT_EQ(future.wait_for(5s), std::future_status::ready);
            auto result = future.get();
            ASSERT_FALSE(result);
            EXPECT_NE(result.error().get_if<extraction::InvalidInputError>(), nullptr);
        }
        auto pool = environment->inferences("extraction-separation");
        ASSERT_TRUE(pool);
        EXPECT_EQ((*pool)->residentCount(), 0);
    }
    TEST_F(SeparationTest, RejectsMalformedNativeTrack) {
        install(true);
        start();
        auto future = submit({Id, wave()});
        ASSERT_EQ(future.wait_for(5s), std::future_status::ready);
        auto result = future.get();
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().get_if<extraction::OperationError>(), nullptr);
    }
    TEST_F(SeparationTest, CancellationReachesNativeSeparatorAndReturnsNoTracks) {
        auto started = std::make_shared<std::promise<void>>();
        auto entered = started->get_future();
        install(false, started);
        start();
        std::stop_source stop;
        auto future = submit({Id, wave()}, stop.get_token());
        ASSERT_EQ(entered.wait_for(5s), std::future_status::ready);
        stop.request_stop();
        ASSERT_EQ(future.wait_for(5s), std::future_status::ready);
        auto result = future.get();
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().get_if<extraction::CancelledError>(), nullptr);
    }
    TEST(DemucsSpectral, MatchesOfficialStftAndIstftIncludingBoundaryPadding) {
        constexpr std::size_t length = 8201, frames = 9, bins = 2048;
        std::vector<float> input(2 * length);
        for (int channel = 0; channel < 2; ++channel)
            for (std::size_t i = 0; i < length; ++i)
                input[channel * length + i] = static_cast<float>(std::sin(i * .013 + channel * .3) * .2 + (static_cast<int>((i * 17 + channel * 11) % 31) - 15) * .001);
        srtsepinfer::demucs::SpectralTransform transform;
        auto spectrum = transform.forward(input, 2);
        ASSERT_TRUE(spectrum);
        for (const auto &point : spectralReference)
            EXPECT_NEAR((*spectrum)[((point.channel * 2 + point.part) * bins + point.bin) * frames + point.frame], point.value, 3e-6);
        auto reconstructed = transform.inverse(*spectrum, 2, length);
        ASSERT_TRUE(reconstructed);
        for (const auto &point : audioReference)
            EXPECT_NEAR((*reconstructed)[point.channel * length + point.sample], point.value, 3e-6);
        EXPECT_FALSE(transform.forward(input, 2, [] { return true; }));
        EXPECT_FALSE(transform.inverse(*spectrum, 2, length, [] { return true; }));
    }
}
