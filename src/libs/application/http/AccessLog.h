#ifndef DSSP_HTTP_ACCESSLOG_H
#define DSSP_HTTP_ACCESSLOG_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

namespace drogon {
    class HttpAppFramework;
}

namespace dssp::http {
    // JSON quoting keeps values on one log line, including untrusted URL paths.
    std::string quoteLogValue(std::string_view value, std::size_t limit = 2048);

    class AccessLog;
    class RequestLog {
    public:
        struct State;
        std::uint64_t id() const noexcept;
        std::string description() const;
        void problem(std::string_view type, std::string_view reason) const;

    private:
        friend class AccessLog;
        friend RequestLog requestLog(const drogon::HttpRequest &request);
        std::shared_ptr<State> state_;
    };

    // Initialize on the request's event loop before dispatching asynchronous work.
    // Copies retain only diagnostic metadata, never the request body or connection.
    RequestLog requestLog(const drogon::HttpRequest &request);

    // Drogon's native advice hooks supply access records, independently of which
    // endpoints support cancellation. Disconnect/shutdown and reply race safely;
    // only one terminal record is emitted for each request.
    class AccessLog : public std::enable_shared_from_this<AccessLog> {
    public:
        void install(drogon::HttpAppFramework &app);
        void begin(const drogon::HttpRequest &request);
        void finish(const drogon::HttpRequest &request, const drogon::HttpResponse &response);
        void connectionChanged(const trantor::TcpConnectionPtr &connection);
        void shutdown();

    private:
        friend struct RequestLog::State;
        void release(std::uint64_t id);
        struct Entry {
            std::weak_ptr<trantor::TcpConnection> connection;
            std::weak_ptr<RequestLog::State> state;
        };

        std::mutex mutex_;
        std::map<std::uint64_t, Entry> requests_;
        bool closing_ = false;
    };
}

#endif // DSSP_HTTP_ACCESSLOG_H
