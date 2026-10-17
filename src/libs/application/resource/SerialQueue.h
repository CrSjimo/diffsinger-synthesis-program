#ifndef DSSP_RESOURCE_SERIALQUEUE_H
#define DSSP_RESOURCE_SERIALQUEUE_H

#include <functional>
#include <memory>
#include <stop_token>
#include <string>

namespace dssp::resource {

    // The dispatcher must eventually execute accepted work off the submitting
    // thread. Queue callbacks must not throw; dependency adapters translate errors.
    class SerialQueue final {
    public:
        using Dispatch = std::function<void(std::function<void()>)>;
        struct Task {
            std::function<void(std::stop_token)> run;
            // Exactly once, outside queue locks. A queued cancellation completes
            // immediately; a running cancellation completes after run() returns.
            std::function<void(bool cancelled)> complete;
            // An asynchronous task holds its FIFO position until finish is
            // called. It must call finish exactly once, even after cancellation.
            std::function<void(std::stop_token, std::function<void()> finish)> runAsync;
        };

        explicit SerialQueue(Dispatch, std::string name = {});
        ~SerialQueue();
        SerialQueue(const SerialQueue &) = delete;
        SerialQueue &operator=(const SerialQueue &) = delete;
        void submit(Task, std::stop_token = {});
        void close();
        // The owner waits before destroying its executor or resource factories.
        // Never call wait() from this queue's own callbacks.
        void wait();

    private:
        struct State;
        std::shared_ptr<State> m_state;
    };
}

#endif // DSSP_RESOURCE_SERIALQUEUE_H
