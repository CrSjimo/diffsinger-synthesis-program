#include "AccessLog.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <vector>

#include <drogon/HttpAppFramework.h>
#include <json/writer.h>
#include <stdcorelib/support/logging.h>

namespace dssp::http {
    namespace {
        stdc::LogCategory log("dssp.http.Access");
        std::atomic<std::uint64_t> nextId{0};
        constexpr auto attributeName = "dssp.http.requestLog";
    }

    std::string quoteLogValue(std::string_view value, std::size_t limit) {
        auto end = std::min(value.size(), limit);
        const bool truncated = value.size() > limit;
        if (truncated) {
            // Do not split a UTF-8 code point at the byte limit.
            while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80) {
                --end;
            }
        }
        // JSONcpp's unescaped fast path assumes a NUL-terminated string even in
        // the length overload. A view must never expose bytes beyond its end.
        auto bounded = std::string(value.substr(0, end));
        if (truncated) {
            bounded += "...";
        }
        return Json::valueToQuotedString(bounded.c_str(), bounded.size());
    }

    struct RequestLog::State {
        const std::uint64_t id = nextId.fetch_add(1, std::memory_order_relaxed) + 1;
        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        const std::string method;
        const std::string path;
        const std::string client;
        std::weak_ptr<AccessLog> owner;
        std::atomic_flag completed = ATOMIC_FLAG_INIT;
        std::mutex problemMutex;
        std::string problemSummary;

        explicit State(const drogon::HttpRequest &request) : method(request.isHead() ? "HEAD" : request.methodString()), path(quoteLogValue(request.path())), client(request.peerAddr().toIpPort()) {}
        ~State() {
            if (const auto registry = owner.lock()) {
                registry->release(id);
            }
        }
        std::string description() const {
            return std::format("request={} method={} path={}", id, method, path);
        }
        void finish(std::string_view outcome, std::string_view status, std::string_view route = {}) {
            if (completed.test_and_set(std::memory_order_acq_rel)) {
                return;
            }
            if (const auto registry = owner.lock()) {
                registry->release(id);
            }
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            std::string problem;
            {
                std::lock_guard lock(problemMutex);
                problem = problemSummary;
            }
            // This is the send boundary, not proof of delivery to the client.
            log.stdcInfo("%1 status=%2 duration_ms=%3 client=%4 outcome=%5%6%7", description(), status, std::format("{:.3f}", elapsed), client, outcome, route.empty() ? std::string{} : " route=" + quoteLogValue(route), problem);
        }
    };

    RequestLog requestLog(const drogon::HttpRequest &request) {
        auto context = request.attributes()->get<RequestLog>(attributeName);
        if (!context.state_) {
            context.state_ = std::make_shared<RequestLog::State>(request);
            request.attributes()->insert(attributeName, context);
        }
        return context;
    }
    std::uint64_t RequestLog::id() const noexcept {
        return state_ ? state_->id : 0;
    }
    std::string RequestLog::description() const {
        return state_ ? state_->description() : "request=none";
    }
    void RequestLog::problem(std::string_view type, std::string_view reason) const {
        if (state_) {
            const auto summary = " error=" + quoteLogValue(type, 128) + " reason=" + quoteLogValue(reason, 1024);
            std::lock_guard lock(state_->problemMutex);
            state_->problemSummary = summary;
        }
    }

    void AccessLog::install(drogon::HttpAppFramework &app) {
        const auto self = shared_from_this();
        app.registerSyncAdvice([self](const drogon::HttpRequestPtr &request) -> drogon::HttpResponsePtr {
            self->begin(*request);
            return nullptr;
        });
        // Like Drogon's AccessLogger, observe the final send boundary, including
        // routing errors and responses produced by pre-routing advice (CORS).
        app.registerPreSendingAdvice([self](const drogon::HttpRequestPtr &request, const drogon::HttpResponsePtr &response) { self->finish(*request, *response); });
    }
    void AccessLog::begin(const drogon::HttpRequest &request) {
        const auto context = requestLog(request);
        bool closing;
        {
            std::lock_guard lock(mutex_);
            closing = closing_;
            if (!closing) {
                context.state_->owner = weak_from_this();
                requests_.emplace(context.id(), Entry{request.getConnectionPtr(), context.state_});
            }
        }
        log.stdcTrace("%1 client=%2 received", context.description(), context.state_->client);
        if (closing) {
            context.state_->finish("server_shutdown", "-");
        }
    }
    void AccessLog::finish(const drogon::HttpRequest &request, const drogon::HttpResponse &response) {
        const auto context = requestLog(request);
        const auto route = request.matchedPathPattern();
        context.state_->finish("response", std::to_string(static_cast<int>(response.statusCode())), route == request.path() ? std::string_view{} : route);
    }
    void AccessLog::release(std::uint64_t id) {
        std::lock_guard lock(mutex_);
        requests_.erase(id);
    }
    void AccessLog::connectionChanged(const trantor::TcpConnectionPtr &connection) {
        if (connection->connected()) {
            return;
        }
        std::vector<std::shared_ptr<RequestLog::State>> states;
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
        for (const auto &state : states) {
            state->finish("client_disconnected", "-");
        }
    }
    void AccessLog::shutdown() {
        std::vector<std::shared_ptr<RequestLog::State>> states;
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
        for (const auto &state : states) {
            state->finish("server_shutdown", "-");
        }
    }
}
