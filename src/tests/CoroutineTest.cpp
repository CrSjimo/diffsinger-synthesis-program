#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <dsspresource/Async.h>
#include <dsspresource/TaskExecutor.h>

namespace dssp::tests {
    using namespace std::chrono_literals;

    TEST(Coroutine, InlineCompletionTransfersMoveOnlyResultsAndIgnoresDuplicates) {
        std::promise<int> promise;
        auto result = promise.get_future();
        drogon::async_run([&]() -> drogon::Task<> {
            auto value = co_await resource::awaitCallback<std::unique_ptr<int>>([](auto complete) {
                complete(std::make_unique<int>(7));
                // The coroutine/awaiter may already be destroyed at this point.
                complete(std::make_unique<int>(8));
            });
            promise.set_value(*value);
        });
        ASSERT_EQ(result.wait_for(0ms), std::future_status::ready);
        EXPECT_EQ(result.get(), 7);
    }

    TEST(Coroutine, DeferredCompletionResumesOnceAndCanOutliveTheFrame) {
        std::function<void(int)> complete;
        std::promise<int> promise;
        auto result = promise.get_future();
        drogon::async_run([&]() -> drogon::Task<> {
            auto value = co_await resource::awaitCallback<int>([&](auto callback) { complete = std::move(callback); });
            promise.set_value(value);
        });
        ASSERT_TRUE(complete);
        EXPECT_EQ(result.wait_for(0ms), std::future_status::timeout);
        std::jthread worker([complete] { complete(42); });
        worker.join();
        ASSERT_EQ(result.wait_for(0ms), std::future_status::ready);
        EXPECT_EQ(result.get(), 42);
        complete(43);
    }

    TEST(Coroutine, BackgroundAdmissionResumesOnAWorkerAndReturnsCancellationAsExpected) {
        const auto created = resource::TaskExecutor::create("test-coroutine", 1);
        ASSERT_TRUE(created);
        const auto executor = *created;
        const auto caller = std::this_thread::get_id();
        std::promise<std::thread::id> promise;
        auto result = promise.get_future();
        drogon::async_run([executor, &promise]() -> drogon::Task<> {
            const auto ready = co_await executor->resume();
            EXPECT_TRUE(ready);
            promise.set_value(std::this_thread::get_id());
        });
        ASSERT_EQ(result.wait_for(1s), std::future_status::ready);
        EXPECT_NE(result.get(), caller);
        executor->shutdown();
        std::promise<bool> stopped;
        auto cancelled = stopped.get_future();
        drogon::async_run([executor, &stopped]() -> drogon::Task<> {
            const auto ready = co_await executor->resume();
            stopped.set_value(!ready && ready.error().get_if<resource::TaskCancelledError>());
        });
        ASSERT_EQ(cancelled.wait_for(0ms), std::future_status::ready);
        EXPECT_TRUE(cancelled.get());
    }
}
