#ifndef DSSP_TESTS_TESTCOROUTINE_H
#define DSSP_TESTS_TESTCOROUTINE_H

#include <utility>

#include <drogon/utils/coroutine.h>

namespace dssp::tests {
    inline void startCoroutine(drogon::Task<> task) {
        drogon::async_run([task = std::move(task)]() mutable -> drogon::Task<> { co_await task; });
    }
}

#endif // DSSP_TESTS_TESTCOROUTINE_H
