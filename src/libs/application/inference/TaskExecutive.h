#ifndef DSSP_INFERENCE_TASKEXECUTIVE_H
#define DSSP_INFERENCE_TASKEXECUTIVE_H

#include <functional>
#include <memory>
#include <utility>

#ifdef _WIN32
#    pragma push_macro("interface")
#    undef interface
#endif

#include <synthrt/Core/ContribExecutive.h>
#include <synthrt/Task/ITask.h>

#ifdef _WIN32
#    pragma pop_macro("interface")
#endif

#include <dsspinference/NativeCall.h>

namespace dssp::inference {
    // The resource adapter owns a native task without imposing an inference
    // category on it. FIFO and LRU remain in the independent resource library.
    class TaskExecutive final {
    public:
        template <class Executive>
        explicit TaskExecutive(std::unique_ptr<Executive> executive)
            : executive_(std::move(executive)),
              state_([task = static_cast<Executive *>(executive_.get())] { return task->state(); }),
              stop_([task = static_cast<Executive *>(executive_.get())] { return task->stop(); }),
              wait_([task = static_cast<Executive *>(executive_.get())] { return task->waitForFinished(); }) {}

        srt::ContribExecutive &native() const noexcept { return *executive_; }
        srt::ITask::State state() const noexcept { return state_(); }
        std::expected<void, ExecutionError> stop() { return nativeCall("Native task::stop", stop_); }
        std::expected<void, ExecutionError> waitForFinished() { return nativeCall("Native task::waitForFinished", wait_); }

    private:
        std::unique_ptr<srt::ContribExecutive> executive_;
        std::function<srt::ITask::State()> state_;
        std::function<srt::Expected<void>()> stop_;
        std::function<srt::Expected<void>()> wait_;
    };
}

#endif // DSSP_INFERENCE_TASKEXECUTIVE_H
