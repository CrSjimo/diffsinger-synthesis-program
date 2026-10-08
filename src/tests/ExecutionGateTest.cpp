#include <chrono>
#include <future>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include <dsspresource/ExecutionGate.h>

namespace dssp::tests {
    using namespace std::chrono_literals;
    namespace {
        std::future<resource::PermitResult> acquire(const std::shared_ptr<resource::ExecutionGate> &gate, std::stop_token stop = {}) {
            const auto promise = std::make_shared<std::promise<resource::PermitResult>>();
            auto result = promise->get_future();
            drogon::async_run([gate, stop, promise]() -> drogon::Task<> { promise->set_value(co_await gate->acquire(stop)); });
            return result;
        }
    }

    TEST(ExecutionGate, PermitsAreMoveOnlyAndWaitersKeepFifoOrder) {
        const auto created = resource::ExecutionGate::create("test-admission");
        ASSERT_TRUE(created);
        const auto gate = *created;
        auto first = acquire(gate);
        ASSERT_EQ(first.wait_for(1s), std::future_status::ready);
        std::optional<resource::PermitResult> held(first.get());
        ASSERT_TRUE(*held);
        auto second = acquire(gate);
        auto third = acquire(gate);
        EXPECT_EQ(second.wait_for(0ms), std::future_status::timeout);
        EXPECT_EQ(third.wait_for(0ms), std::future_status::timeout);
        held.reset();
        ASSERT_EQ(second.wait_for(1s), std::future_status::ready);
        held.emplace(second.get());
        ASSERT_TRUE(*held);
        EXPECT_EQ(third.wait_for(0ms), std::future_status::timeout);
        held.reset();
        ASSERT_EQ(third.wait_for(1s), std::future_status::ready);
        auto final = third.get();
        ASSERT_TRUE(final);
    }

    TEST(ExecutionGate, PendingCancellationCompletesWithoutWaitingForThePermit) {
        const auto created = resource::ExecutionGate::create("test-cancel-admission");
        ASSERT_TRUE(created);
        const auto gate = *created;
        auto first = acquire(gate);
        ASSERT_EQ(first.wait_for(1s), std::future_status::ready);
        auto held = first.get();
        ASSERT_TRUE(held);
        std::stop_source stop;
        auto pending = acquire(gate, stop.get_token());
        stop.request_stop();
        ASSERT_EQ(pending.wait_for(0ms), std::future_status::ready);
        EXPECT_FALSE(pending.get());
        EXPECT_FALSE(held->cancellation().stop_requested());
    }

    TEST(ExecutionGate, RunningCancellationRequestsStopButRetainsAdmission) {
        const auto created = resource::ExecutionGate::create("test-running-admission");
        ASSERT_TRUE(created);
        const auto gate = *created;
        std::stop_source stop;
        auto first = acquire(gate, stop.get_token());
        ASSERT_EQ(first.wait_for(1s), std::future_status::ready);
        std::optional<resource::PermitResult> held(first.get());
        ASSERT_TRUE(*held);
        auto pending = acquire(gate);
        stop.request_stop();
        EXPECT_TRUE((*held)->cancellation().stop_requested());
        EXPECT_EQ(pending.wait_for(0ms), std::future_status::timeout);
        held.reset();
        ASSERT_EQ(pending.wait_for(1s), std::future_status::ready);
        EXPECT_TRUE(pending.get());
    }

    TEST(ExecutionGate, ClosingCancelsWaitersAndRejectsFurtherAdmission) {
        const auto created = resource::ExecutionGate::create("test-close-admission");
        ASSERT_TRUE(created);
        const auto gate = *created;
        auto first = acquire(gate);
        ASSERT_EQ(first.wait_for(1s), std::future_status::ready);
        std::optional<resource::PermitResult> held(first.get());
        ASSERT_TRUE(*held);
        auto pending = acquire(gate);
        gate->close();
        EXPECT_TRUE((*held)->cancellation().stop_requested());
        ASSERT_EQ(pending.wait_for(0ms), std::future_status::ready);
        EXPECT_FALSE(pending.get());
        held.reset();
        gate->shutdown();
        auto rejected = acquire(gate);
        ASSERT_EQ(rejected.wait_for(0ms), std::future_status::ready);
        EXPECT_FALSE(rejected.get());
    }
}
