#include <atomic>
#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include <dsspresource/TaskExecutor.h>

namespace dssp::tests {
    using namespace std::chrono_literals;

    TEST(TaskExecutor, PendingCancellationCompletesBeforeWorkerBecomesAvailable) {
        const auto created = resource::TaskExecutor::create("test-tasks", 1);
        ASSERT_TRUE(created);
        const auto executor = *created;
        const auto entered = std::make_shared<std::promise<void>>();
        auto started = entered->get_future();
        const auto release = std::make_shared<std::promise<void>>();
        auto gate = release->get_future().share();
        const auto first = std::make_shared<std::promise<resource::TaskResult>>();
        auto firstResult = first->get_future();
        executor->submit({[entered, gate](std::stop_token) { entered->set_value(); gate.wait_for(1s); }, [first](auto result) { first->set_value(std::move(result)); }});
        EXPECT_EQ(started.wait_for(1s), std::future_status::ready);

        std::stop_source stop;
        const auto ran = std::make_shared<std::atomic<bool>>(false);
        const auto cancelled = std::make_shared<std::promise<resource::TaskResult>>();
        auto cancelledResult = cancelled->get_future();
        executor->submit({[ran](std::stop_token) { *ran = true; }, [cancelled](auto result) { cancelled->set_value(std::move(result)); }}, stop.get_token());
        stop.request_stop();
        const auto ready = cancelledResult.wait_for(1s);
        EXPECT_EQ(ready, std::future_status::ready);
        if (ready == std::future_status::ready) {
            const auto result = cancelledResult.get();
            EXPECT_FALSE(result);
            if (!result) EXPECT_NE(result.error().get_if<resource::TaskCancelledError>(), nullptr);
        }

        EXPECT_FALSE(ran->load());
        EXPECT_EQ(firstResult.wait_for(0ms), std::future_status::timeout);
        release->set_value();
        EXPECT_EQ(firstResult.wait_for(1s), std::future_status::ready);
        executor->shutdown();
    }

    TEST(TaskExecutor, ShutdownAfterTaskCompletionWakesIdleWorkers) {
        for (unsigned iteration = 0; iteration < 64; ++iteration) {
            SCOPED_TRACE(iteration);
            const auto created = resource::TaskExecutor::create("test-shutdown", 1);
            ASSERT_TRUE(created);
            const auto executor = *created;
            const auto completed = std::make_shared<std::promise<resource::TaskResult>>();
            auto result = completed->get_future();
            executor->submit({[](std::stop_token) {}, [completed](auto reply) { completed->set_value(std::move(reply)); }});
            EXPECT_EQ(result.wait_for(1s), std::future_status::ready);
            // Race shutdown against the worker returning to its idle wait.
            executor->shutdown();
        }
    }

    TEST(TaskExecutor, IndependentJobsRunConcurrentlyAndShutdownStopsRunningWork) {
        const auto created = resource::TaskExecutor::create("test-tasks", 2);
        ASSERT_TRUE(created);
        const auto executor = *created;
        const auto firstEntered = std::make_shared<std::promise<void>>();
        const auto secondEntered = std::make_shared<std::promise<void>>();
        auto firstStarted = firstEntered->get_future();
        auto secondStarted = secondEntered->get_future();
        const auto stopped = std::make_shared<std::promise<resource::TaskResult>>();
        auto stoppedResult = stopped->get_future();
        executor->submit({[firstEntered](std::stop_token stop) {
            const auto cancelled = std::make_shared<std::promise<void>>();
            auto done = cancelled->get_future();
            std::stop_callback callback(stop, [cancelled] { cancelled->set_value(); });
            firstEntered->set_value();
            done.wait_for(5s);
        }, [stopped](auto result) { stopped->set_value(std::move(result)); }});
        executor->submit({[secondEntered](std::stop_token) { secondEntered->set_value(); }, [](auto) {}});
        EXPECT_EQ(firstStarted.wait_for(5s), std::future_status::ready);
        EXPECT_EQ(secondStarted.wait_for(5s), std::future_status::ready);
        executor->shutdown();
        ASSERT_EQ(stoppedResult.wait_for(5s), std::future_status::ready);
        const auto result = stoppedResult.get();
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().get_if<resource::TaskCancelledError>(), nullptr);

        bool ran = false, completed = false;
        executor->submit({[&](std::stop_token) { ran = true; }, [&](auto reply) { EXPECT_FALSE(reply); completed = true; }});
        EXPECT_FALSE(ran);
        EXPECT_TRUE(completed);
    }
}
