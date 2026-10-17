#ifndef DSSP_RESOURCE_TASKPROGRESS_H
#define DSSP_RESOURCE_TASKPROGRESS_H

#include <functional>
#include <utility>

namespace dssp::resource {
    struct TaskUpdate {
        enum class State {
            Queue,
            Progress
        };
        State state;
        double value = 0;
    };

    // Callbacks may run concurrently. Observers must not throw or block execution.
    class TaskProgress {
    public:
        TaskProgress() = default;
        explicit TaskProgress(std::function<void(TaskUpdate)> callback) : m_callback(std::move(callback)) {}

        void queue() const {
            if (m_callback) {
                m_callback({TaskUpdate::State::Queue});
            }
        }

        void advance(double value) const {
            if (m_callback) {
                m_callback({TaskUpdate::State::Progress, value});
            }
        }

    private:
        std::function<void(TaskUpdate)> m_callback;
    };
}

#endif // DSSP_RESOURCE_TASKPROGRESS_H
