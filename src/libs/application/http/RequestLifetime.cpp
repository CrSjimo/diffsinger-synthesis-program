#include "RequestLifetime.h"

#include <vector>

#include <stdcorelib/support/logging.h>

namespace dssp::http {
    namespace {
        stdc::LogCategory log("dssp.http.RequestLifetime");
    }

    struct RequestContext::State {
        std::stop_source source;
        std::string acceptLanguage;
        RequestLog logContext;
        std::weak_ptr<RequestLifetimeRegistry> registry;
        std::uint64_t registrationId = 0;

        ~State() {
            if (const auto owner = registry.lock()) {
                owner->release(registrationId);
            }
        }
    };

    std::uint64_t RequestContext::id() const noexcept {
        return logContext().id();
    }

    const RequestLog &RequestContext::logContext() const noexcept {
        static const RequestLog empty;
        return state_ ? state_->logContext : empty;
    }

    std::stop_token RequestContext::cancellation() const noexcept {
        return state_ ? state_->source.get_token() : std::stop_token{};
    }

    bool RequestContext::stopRequested() const noexcept {
        return cancellation().stop_requested();
    }

    const std::string &RequestContext::acceptLanguage() const noexcept {
        static const std::string empty;
        return state_ ? state_->acceptLanguage : empty;
    }

    RequestContext RequestLifetimeRegistry::track(const drogon::HttpRequest &request) {
        RequestContext context;
        context.state_ = std::make_shared<RequestContext::State>();
        context.state_->acceptLanguage = request.getHeader("accept-language");
        context.state_->logContext = requestLog(request);
        context.state_->registry = weak_from_this();

        bool stop = false;
        {
            std::lock_guard lock(mutex_);
            context.state_->registrationId = ++nextId_;
            const auto connection = request.getConnectionPtr();
            const auto live = connection.lock();
            const std::weak_ptr<trantor::TcpConnection> empty;
            const bool hasConnection = connection.owner_before(empty) || empty.owner_before(connection);
            stop = closing_ || (live ? !live->connected() : hasConnection);
            if (!stop) {
                requests_.emplace(nextId_, Entry{connection, context.state_});
            }
        }

        if (stop) {
            context.state_->source.request_stop();
        }

        return context;
    }

    void RequestLifetimeRegistry::release(std::uint64_t registrationId) {
        std::lock_guard lock(mutex_);
        requests_.erase(registrationId);
    }

    void RequestLifetimeRegistry::connectionChanged(const trantor::TcpConnectionPtr &connection) {
        if (connection->connected()) {
            return;
        }

        std::vector<std::shared_ptr<RequestContext::State>> states;
        {
            std::lock_guard lock(mutex_);
            for (auto it = requests_.begin(); it != requests_.end();) {
                if (it->second.connection.lock() == connection) {
                    if (auto state = it->second.state.lock()) {
                        states.push_back(std::move(state));
                    }
                    it = requests_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Stop callbacks may synchronously release their registration. Never call
        // request_stop or destroy the last request state while holding this mutex.
        for (auto &state : states) {
            log.stdcTrace("%1 cancelling backend work after client disconnect", state->logContext.description());
            state->source.request_stop();
        }
    }

    void RequestLifetimeRegistry::cancelAll() {
        std::vector<std::shared_ptr<RequestContext::State>> states;
        {
            std::lock_guard lock(mutex_);
            closing_ = true;
            for (const auto &[id, entry] : requests_) {
                if (auto state = entry.state.lock()) {
                    states.push_back(std::move(state));
                }
            }
            requests_.clear();
        }

        for (auto &state : states) {
            log.stdcTrace("%1 cancelling backend work during server shutdown", state->logContext.description());
            state->source.request_stop();
        }
    }
}
