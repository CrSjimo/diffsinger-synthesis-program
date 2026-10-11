#ifndef DSSP_HTTP_REQUESTLIFETIME_H
#define DSSP_HTTP_REQUESTLIFETIME_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>

#include <drogon/HttpRequest.h>

#include <dssphttp/AccessLog.h>

namespace dssp::http {
    class RequestLifetimeRegistry;

    // Copy this context into asynchronous work to retain the registration. It does
    // not retain the HTTP body or the connection. Services register std::stop_callback
    // on cancellation() to stop their own tasks and retain their own runtime handles.
    class RequestContext {
    public:
        struct State;
        // Shares the access log ID; zero denotes an empty context.
        std::uint64_t id() const noexcept;
        const RequestLog &logContext() const noexcept;
        std::stop_token cancellation() const noexcept;
        bool stopRequested() const noexcept;
        const std::string &acceptLanguage() const noexcept;

    private:
        friend class RequestLifetimeRegistry;
        std::shared_ptr<State> state_;
    };

    class RequestLifetimeRegistry : public std::enable_shared_from_this<RequestLifetimeRegistry> {
    public:
        RequestContext track(const drogon::HttpRequest &request);
        void connectionChanged(const trantor::TcpConnectionPtr &connection);
        void cancelAll();

    private:
        friend struct RequestContext::State;
        void release(std::uint64_t registrationId);
        struct Entry {
            std::weak_ptr<trantor::TcpConnection> connection;
            std::weak_ptr<RequestContext::State> state;
        };

        std::mutex mutex_;
        std::map<std::uint64_t, Entry> requests_;
        std::uint64_t nextId_ = 0;
        bool closing_ = false;
    };
}

#endif // DSSP_HTTP_REQUESTLIFETIME_H
