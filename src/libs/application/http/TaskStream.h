#ifndef DSSP_HTTP_TASKSTREAM_H
#define DSSP_HTTP_TASKSTREAM_H

#include <chrono>
#include <memory>

#include <dssphttp/RequestLifetime.h>
#include <dssphttp/Response.h>
#include <dsspresource/TaskProgress.h>

namespace dssp::http {
    class TaskStream final {
    public:
        static std::shared_ptr<TaskStream> open(const drogon::HttpRequest &, const Callback &, RequestContext, std::chrono::seconds heartbeatInterval);
        resource::TaskProgress progress() const;
        void finish(const drogon::HttpResponsePtr &) const;

    private:
        struct State;
        explicit TaskStream(std::shared_ptr<State>);
        std::shared_ptr<State> m_state;
    };
}

#endif // DSSP_HTTP_TASKSTREAM_H
