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
        std::weak_ptr<RequestLifetimeRegistry> registry;
        std::uint64_t id = 0;
        ~State() {
            if (const auto owner = registry.lock()) {
                owner->release(id);
            }
        }
    };
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
        context.state_->registry = weak_from_this();
        bool stop = false;
        {
            std::lock_guard lock(mutex_);
            context.state_->id = ++nextId_;
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
    void RequestLifetimeRegistry::release(std::uint64_t id) {
        std::lock_guard lock(mutex_);
        requests_.erase(id);
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
            state->source.request_stop();
        }
        if (!states.empty()) {
            log.stdcDebug("Disconnected client cancelled %1 requests", states.size());
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
            state->source.request_stop();
        }
    }
}
