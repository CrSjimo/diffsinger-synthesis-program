#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>

#include <gtest/gtest.h>
#include <synthrt/Core/ContribImportBinding.h>
#include <wolf/Api/Inferences/G2P/1/G2PApiL1.h>

#include <dsspinference/Environment.h>
#include <dssptests/TestLanguagePackage.h>

namespace dssp::tests {
    using namespace std::chrono_literals;
    namespace G2P = wolf::Api::G2P::L1;
    using G2pResult = inference::ExecutionResult<std::unique_ptr<G2P::G2PResult>>;

    namespace {
        struct ExecutionControl {
            std::mutex mutex;
            std::condition_variable wake;
            std::vector<std::string> starts;
            unsigned active = 0;
            unsigned maximumActive = 0;
            std::atomic<unsigned> created = 0;
            std::atomic<unsigned> stopped = 0;
            bool block = false;
            bool released = false;
        };
        // A native contract test double lets cancellation races be controlled
        // without sleeps, external models, or edits to any upstream library.
        class ControlledExecutive final : public G2P::G2PExecutive {
        public:
            ControlledExecutive(srt::InferenceSpec &spec, std::shared_ptr<ExecutionControl> control) : G2PExecutive(spec), control_(std::move(control)) { ++control_->created; }
            srt::Expected<void> initialize(const G2P::G2PInitArgs &) override { return {}; }
            srt::Expected<std::unique_ptr<G2P::G2PResult>> start(const G2P::G2PStartInput &input) override {
                state_ = srt::ITask::Running;
                if (input.lyrics.front() == "throw") {
                    throw std::runtime_error("simulated native failure");
                }
                {
                    std::unique_lock lock(control_->mutex);
                    ++control_->active;
                    control_->maximumActive = std::max(control_->maximumActive, control_->active);
                    control_->starts.push_back(input.lyrics.front());
                    control_->wake.notify_all();
                    if (control_->block) {
                        control_->wake.wait_for(lock, 5s, [&] { return stopped_ || control_->released; });
                    }
                    --control_->active;
                }
                auto result = std::make_unique<G2P::G2PResult>();
                for (const auto &lyric : input.lyrics) {
                    G2P::G2PWordOutput word;
                    word.pronunciation = lyric;
                    result->words.push_back(std::move(word));
                }
                state_ = stopped_ ? srt::ITask::Canceled : srt::ITask::Succeeded;
                return result;
            }
            srt::Expected<void> startAsync(std::shared_ptr<const G2P::G2PStartInput> input, AsyncCallback complete) override {
                complete(start(*input));
                return {};
            }
            srt::ITask::State state() const noexcept override { return state_; }
            srt::Expected<void> stop() override {
                {
                    const std::lock_guard lock(control_->mutex);
                    stopped_ = true;
                }
                ++control_->stopped;
                control_->wake.notify_all();
                return {};
            }
            srt::Expected<void> waitForFinished() override { return {}; }

        private:
            std::shared_ptr<ExecutionControl> control_;
            std::atomic<srt::ITask::State> state_ = srt::ITask::Idle;
            std::atomic<bool> stopped_ = false;
        };
        G2pResult awaitResult(std::future<G2pResult> result) {
            const auto ready = result.wait_for(5s);
            EXPECT_EQ(ready, std::future_status::ready);
            if (ready != std::future_status::ready) {
                return fail<inference::ExecutionErrors>(inference::ExecutionError("Test inference timed out"));
            }
            return result.get();
        }
    }

    class InferencePoolTest : public ::testing::Test {
    protected:
        void SetUp() override {
            writeLanguagePackage(files, "one");
            writeLanguagePackage(files, "two", {.version = "2"});
            inference::EnvironmentOptions options;
            options.packageRoots = {files.path()};
            const std::filesystem::path plugins(DSSP_TEST_PLUGIN_DIR);
            options.pluginPaths = {{"inference", plugins / "wolf" / "inferenceinterpreters"}, {"linguist", plugins / "wolf" / "linguistproviders"}};
            options.cacheGroups = {{"diffsinger-language", {1, 0ms, 60s}}, {"diffsinger-synthesis", {2, 0ms, 60s}}};
            const auto created = inference::Environment::create(options);
            ASSERT_TRUE(created) << (created ? "" : created.error().formatChain());
            host = *created;
            ASSERT_TRUE(host->problems().empty()) << (host->problems().empty() ? "" : host->problems().front().error.formatChain());
            ASSERT_EQ(host->packages().size(), 2U);
            const auto group = host->inferences("diffsinger-language");
            ASSERT_TRUE(group);
            pool = *group;
            spec = dynamic_cast<srt::InferenceSpec *>(host->packages().front().contribution("inference", "g2p"));
            ASSERT_NE(spec, nullptr);
        }
        void TearDown() override {
            if (host) {
                host->shutdown();
            }
        }
        inference::InferenceHandle controlled(std::string key, const std::shared_ptr<ExecutionControl> &control, inference::InferencePool *group = nullptr) {
            auto definition = inference::InferenceDefinition{key, key, {spec->package()}, [spec = spec, control]() -> std::expected<std::unique_ptr<srt::InferenceExecutive>, inference::ExecutionError> {
                return std::make_unique<ControlledExecutive>(*spec, control);
            }};
            const auto handle = (group ? group : pool)->bind(std::move(definition));
            EXPECT_TRUE(handle);
            return handle ? *handle : inference::InferenceHandle{};
        }
        std::future<G2pResult> submit(inference::InferenceHandle handle, std::string lyric, std::stop_token stop = {}, inference::InferencePool *group = nullptr) {
            auto input = std::make_shared<G2P::G2PStartInput>();
            input->lyrics.push_back(std::move(lyric));
            auto promise = std::make_shared<std::promise<G2pResult>>();
            auto result = promise->get_future();
            (group ? group : pool)->run<G2P::G2PExecutive>(std::move(handle), stop, [input](auto &executive) { return executive.start(*input); }, [promise](auto value) { promise->set_value(std::move(value)); });
            return result;
        }
        TestFiles files;
        std::shared_ptr<inference::Environment> host;
        inference::InferencePool *pool = nullptr;
        srt::InferenceSpec *spec = nullptr;
    };

    TEST_F(InferencePoolTest, QueueIsSharedByIdentityAndBothQueuedAndRunningCancellationAreIsolated) {
        auto control = std::make_shared<ExecutionControl>();
        control->block = true;
        const auto handle = controlled("shared", control);
        EXPECT_EQ(controlled("shared", control), handle);
        std::stop_source firstStop, secondStop;
        auto first = submit(handle, "first", firstStop.get_token());
        bool started = false;
        {
            std::unique_lock lock(control->mutex);
            started = control->wake.wait_for(lock, 5s, [&] { return !control->starts.empty(); });
        }
        EXPECT_TRUE(started);
        auto second = submit(handle, "second", secondStop.get_token());
        auto third = submit(handle, "third");
        secondStop.request_stop();
        const auto queued = awaitResult(std::move(second));
        ASSERT_FALSE(queued);
        EXPECT_NE(queued.error().get_if<inference::CancelledError>(), nullptr);
        firstStop.request_stop();
        {
            const std::lock_guard lock(control->mutex);
            control->released = true;
        }
        control->wake.notify_all();
        const auto running = awaitResult(std::move(first));
        ASSERT_FALSE(running);
        EXPECT_NE(running.error().get_if<inference::CancelledError>(), nullptr);
        const auto successor = awaitResult(std::move(third));
        ASSERT_TRUE(successor);
        EXPECT_EQ(control->starts, (std::vector<std::string>{"first", "third"}));
        EXPECT_EQ(control->maximumActive, 1U);
        EXPECT_EQ(control->stopped.load(), 1U);
        EXPECT_EQ(control->created.load(), 2U);
    }

    TEST_F(InferencePoolTest, NativeExceptionsBecomeExpectedErrorsAndFaultedInstancesAreRecreated) {
        const auto control = std::make_shared<ExecutionControl>();
        const auto handle = controlled("exception", control);
        const auto failed = awaitResult(submit(handle, "throw"));
        ASSERT_FALSE(failed);
        const auto *error = failed.error().get_if<inference::ExecutionError>();
        ASSERT_NE(error, nullptr);
        ASSERT_NE(error->cause(), nullptr);
        EXPECT_NE(error->formatChain().find("simulated native failure"), std::string::npos);
        EXPECT_EQ(pool->residentCount(), 0U);
        const auto next = awaitResult(submit(handle, "next"));
        ASSERT_TRUE(next);
        EXPECT_EQ(control->created.load(), 2U);
    }

    TEST_F(InferencePoolTest, CacheGroupsApplyIndependentCapacitiesAndRejectForeignHandles) {
        const auto synthesis = host->inferences("diffsinger-synthesis");
        ASSERT_TRUE(synthesis);
        const auto control = std::make_shared<ExecutionControl>();
        const auto one = controlled("one", control);
        const auto two = controlled("two", control);
        ASSERT_TRUE(awaitResult(submit(one, "one")));
        ASSERT_TRUE(awaitResult(submit(two, "two")));
        EXPECT_EQ(pool->residentCount(), 1U);
        const auto synthOne = controlled("one", control, *synthesis);
        const auto synthTwo = controlled("two", control, *synthesis);
        ASSERT_TRUE(awaitResult(submit(synthOne, "one", {}, *synthesis)));
        ASSERT_TRUE(awaitResult(submit(synthTwo, "two", {}, *synthesis)));
        EXPECT_EQ((*synthesis)->residentCount(), 2U);
        EXPECT_EQ(pool->residentCount(), 1U);
        EXPECT_EQ(control->created.load(), 4U);
        ASSERT_TRUE(awaitResult(submit(one, "one")));
        EXPECT_EQ(control->created.load(), 5U);
        EXPECT_EQ(controlled("one", control), one);
        const auto wrongGroup = awaitResult(submit(one, "wrong", {}, *synthesis));
        ASSERT_FALSE(wrongGroup);
        EXPECT_NE(wrongGroup.error().get_if<inference::ExecutionError>(), nullptr);
    }

    TEST_F(InferencePoolTest, RealImportBindingsReuseTheSameExecutiveAndKeepVersionsAndRuntimeBindingsDistinct) {
        const auto imported = host->packages().front().contribution("linguist", "eng")->findImport("linguist/g2p");
        ASSERT_TRUE(imported);
        auto options = std::make_shared<G2P::G2PRuntimeOptions>("pipe-chain");
        options->binding = {"eng", "test"};
        const srt::JsonValue effective(srt::JsonObject{{"language", "eng"}, {"scheme", "test"}});
        const auto one = pool->bindImport<G2P::G2PExecutive, G2P::G2PInitArgs>(*imported, options, effective);
        ASSERT_TRUE(one);
        const auto repeated = pool->bindImport<G2P::G2PExecutive, G2P::G2PInitArgs>(*imported, options, effective);
        ASSERT_TRUE(repeated);
        EXPECT_EQ(*one, *repeated);
        auto changedOptions = std::make_shared<G2P::G2PRuntimeOptions>("pipe-chain");
        changedOptions->binding = {"eng", "other"};
        const auto changed = pool->bindImport<G2P::G2PExecutive, G2P::G2PInitArgs>(*imported, changedOptions, srt::JsonObject{{"language", "eng"}, {"scheme", "other"}});
        ASSERT_TRUE(changed);
        EXPECT_NE(*changed, *one);
        const auto otherImport = host->packages().back().contribution("linguist", "eng")->findImport("linguist/g2p");
        ASSERT_TRUE(otherImport);
        const auto otherVersion = pool->bindImport<G2P::G2PExecutive, G2P::G2PInitArgs>(*otherImport, options, effective);
        ASSERT_TRUE(otherVersion);
        EXPECT_NE(*otherVersion, *one);
        EXPECT_EQ(pool->residentCount(), 0U);
        const auto result = awaitResult(submit(*one, "read"));
        ASSERT_TRUE(result) << (result ? "" : result.error().asError().formatChain());
        ASSERT_EQ((*result)->words.size(), 1U);
        EXPECT_EQ((*result)->words.front().pronunciation, "b a b i");
        EXPECT_EQ(pool->residentCount(), 1U);
    }

    TEST_F(InferencePoolTest, ShutdownCancelsAcceptedWorkAndRejectsSubsequentBindingsAndTasks) {
        const auto control = std::make_shared<ExecutionControl>();
        control->block = true;
        const auto handle = controlled("shutdown", control);
        auto first = submit(handle, "running");
        {
            std::unique_lock lock(control->mutex);
            EXPECT_TRUE(control->wake.wait_for(lock, 5s, [&] { return !control->starts.empty(); }));
        }
        auto queued = submit(handle, "queued");
        host->shutdown();
        EXPECT_FALSE(awaitResult(std::move(first)));
        EXPECT_FALSE(awaitResult(std::move(queued)));
        EXPECT_FALSE(awaitResult(submit(handle, "after-shutdown")));
        EXPECT_FALSE(host->inferences("diffsinger-language"));
        EXPECT_EQ(pool->residentCount(), 0U);
    }
}
