#include <chrono>
#include <functional>
#include <memory>
#include <stop_token>
#include <vector>

#include <gtest/gtest.h>

#include <dsspresource/LruCache.h>
#include <dsspresource/SerialQueue.h>

namespace dssp::tests {
    using namespace std::chrono_literals;

    TEST(ResourceCache, EvictsLeastRecentlyUsedIdleEntriesAndPinsActiveLeases) {
        resource::LruCache<int, std::shared_ptr<int>> cache({2, 0ms, 60s});
        const auto first = std::make_shared<int>(1);
        cache.insert(1, first);
        cache.insert(2, std::make_shared<int>(2));
        ASSERT_TRUE(cache.acquire(1));
        cache.insert(3, std::make_shared<int>(3));
        EXPECT_FALSE(cache.acquire(2));
        EXPECT_TRUE(cache.acquire(1));
        EXPECT_TRUE(cache.acquire(3));

        auto pinnedOne = cache.acquire(1);
        auto pinnedThree = cache.acquire(3);
        ASSERT_TRUE(pinnedOne);
        ASSERT_TRUE(pinnedThree);
        auto pinnedFour = cache.insert(4, std::make_shared<int>(4));
        EXPECT_EQ(cache.size(), 3U);
        EXPECT_EQ(*pinnedOne->value(), 1);
        pinnedOne.reset();
        EXPECT_EQ(cache.size(), 2U);
        EXPECT_FALSE(cache.acquire(1));
    }

    TEST(ResourceCache, EvictionAndReplacementDoNotDestroyLeasedValues) {
        resource::LruCache<int, std::shared_ptr<int>> cache;
        std::weak_ptr<int> weakOld;
        {
            auto old = cache.insert(1, std::make_shared<int>(1));
            weakOld = old.value();
            auto replacement = cache.insert(1, std::make_shared<int>(2));
            EXPECT_EQ(*old.value(), 1);
            EXPECT_EQ(*replacement.value(), 2);
            cache.clear();
            EXPECT_EQ(cache.size(), 0U);
            EXPECT_FALSE(weakOld.expired());
        }
        EXPECT_TRUE(weakOld.expired());
    }

    TEST(ResourceCache, TimeoutIgnoresBusyEntriesAndZeroDisablesExpiryAndCountLimits) {
        resource::LruCache<int, int> cache({0, 1s, 60s});
        cache.insert(1, 1);
        auto busy = cache.insert(2, 2);
        cache.sweep(std::chrono::steady_clock::now() + 2s);
        EXPECT_FALSE(cache.acquire(1));
        EXPECT_TRUE(cache.acquire(2));
        resource::LruCache<int, int> unlimited({0, 0ms, 60s});
        for (int i = 0; i < 100; ++i) {
            auto lease = unlimited.insert(i, i);
        }
        unlimited.sweep(std::chrono::steady_clock::now() + 24h);
        EXPECT_EQ(unlimited.size(), 100U);
    }

    TEST(ResourceCache, CleanupRunsOutsideTheCacheMutex) {
        resource::LruCache<int, std::shared_ptr<int>> cache;
        bool released = false;
        auto value = std::shared_ptr<int>(new int(1), [&](int *pointer) {
            EXPECT_EQ(cache.size(), 0U);
            released = true;
            delete pointer;
        });
        cache.insert(1, std::move(value));
        cache.erase(1);
        EXPECT_TRUE(released);
    }

    TEST(ResourceCache, ALeaseOutlivesTheOwnerWithoutRetainingOtherIdleValues) {
        using Cache = resource::LruCache<int, std::shared_ptr<int>>;
        auto cache = std::make_unique<Cache>();
        auto lease = std::optional<Cache::Lease>{cache->insert(1, std::make_shared<int>(1))};
        const std::weak_ptr<int> pinned = lease->value();
        std::weak_ptr<int> idle;
        {
            auto entry = cache->insert(2, std::make_shared<int>(2));
            idle = entry.value();
        }
        cache.reset();
        EXPECT_TRUE(idle.expired());
        EXPECT_FALSE(pinned.expired());
        EXPECT_EQ(*lease->value(), 1);
        lease.reset();
        EXPECT_TRUE(pinned.expired());
    }

    TEST(ResourceQueue, FifoCancellationAndReentrantCompletionUseOneDrain) {
        std::vector<std::function<void()>> dispatched;
        resource::SerialQueue queue([&](auto task) { dispatched.push_back(std::move(task)); });
        std::vector<int> order;
        unsigned cancelled = 0;
        std::stop_source second;
        queue.submit({[&](auto) { order.push_back(1); }, [&](bool stopped) {
            EXPECT_FALSE(stopped);
            queue.submit({[&](auto) { order.push_back(4); }, [&](bool stopped) { EXPECT_FALSE(stopped); }});
        }});
        queue.submit({[&](auto) { ADD_FAILURE() << "Cancelled queued task ran"; }, [&](bool stopped) { EXPECT_TRUE(stopped); ++cancelled; }}, second.get_token());
        queue.submit({[&](auto) { order.push_back(3); }, [&](bool stopped) { EXPECT_FALSE(stopped); }});
        second.request_stop();
        EXPECT_EQ(cancelled, 1U);
        ASSERT_EQ(dispatched.size(), 1U);
        dispatched.front()();
        queue.wait();
        EXPECT_EQ(order, (std::vector<int>{1, 3, 4}));
        EXPECT_EQ(cancelled, 1U);
    }

    TEST(ResourceQueue, RunningCancellationReachesTheTaskAndDoesNotAffectItsSuccessor) {
        std::function<void()> dispatched;
        resource::SerialQueue queue([&](auto task) { dispatched = std::move(task); });
        std::stop_source first;
        unsigned stopped = 0, completed = 0;
        queue.submit({[&](std::stop_token token) {
            const std::stop_callback stop(token, [&] { ++stopped; });
            first.request_stop();
            EXPECT_TRUE(token.stop_requested());
        }, [&](bool cancelled) { EXPECT_TRUE(cancelled); ++completed; }}, first.get_token());
        queue.submit({[&](std::stop_token token) { EXPECT_FALSE(token.stop_requested()); }, [&](bool cancelled) { EXPECT_FALSE(cancelled); ++completed; }});
        ASSERT_TRUE(dispatched);
        dispatched();
        queue.wait();
        EXPECT_EQ(stopped, 1U);
        EXPECT_EQ(completed, 2U);
    }

    TEST(ResourceQueue, ClosedQueueAndAlreadyCancelledRequestsCompleteOnceWithoutRunning) {
        std::vector<std::function<void()>> dispatched;
        resource::SerialQueue queue([&](auto task) { dispatched.push_back(std::move(task)); });
        std::stop_source stop;
        stop.request_stop();
        unsigned completed = 0;
        const auto task = resource::SerialQueue::Task{[](auto) { ADD_FAILURE() << "Rejected task ran"; }, [&](bool cancelled) { EXPECT_TRUE(cancelled); ++completed; }};
        queue.submit(task, stop.get_token());
        queue.submit(task);
        queue.close();
        queue.submit(task);
        ASSERT_EQ(dispatched.size(), 1U);
        dispatched.front()();
        queue.wait();
        EXPECT_EQ(completed, 3U);
    }
}
