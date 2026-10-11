#include <atomic>

#include <gtest/gtest.h>

#include <dsspresource/WorkerPool.h>

namespace dssp::tests {
    TEST(WorkerPool, ShutdownDrainsAcceptedWorkAndRejectsNewWork) {
        std::atomic<unsigned> completed = 0;
        const auto created = resource::WorkerPool::create("test-workers", 4);
        ASSERT_TRUE(created);
        const auto &pool = *created;
        for (unsigned i = 0; i < 128; ++i) {
            ASSERT_TRUE(pool->dispatch([&] { ++completed; }));
        }

        pool->shutdown();
        EXPECT_EQ(completed.load(), 128U);
        EXPECT_FALSE(pool->dispatch([&] { ++completed; }));
        pool->shutdown();
        EXPECT_EQ(completed.load(), 128U);
    }

    TEST(WorkerPool, DestructionCompletesAcceptedWork) {
        std::atomic<unsigned> completed = 0;
        {
            const auto created = resource::WorkerPool::create("test-workers", 1);
            ASSERT_TRUE(created);
            for (unsigned i = 0; i < 128; ++i) {
                ASSERT_TRUE((*created)->dispatch([&] { ++completed; }));
            }
        }

        EXPECT_EQ(completed.load(), 128U);
    }
}
