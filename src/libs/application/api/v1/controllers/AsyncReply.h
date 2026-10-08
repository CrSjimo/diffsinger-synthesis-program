#ifndef DSSP_API_V1_CONTROLLERS_ASYNCREPLY_H
#define DSSP_API_V1_CONTROLLERS_ASYNCREPLY_H

#include <atomic>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <stdcorelib/support/logging.h>
#include <dssphttp/RequestLifetime.h>
#include <dssphttp/Response.h>

namespace dssp::api_v1::detail {
    inline stdc::LogCategory replyLog("dssp.api_v1.AsyncReply");
    // The selected implementation, callback and request context outlive the
    // handler. No controller, stack reference or raw request is captured.
    // Backend tasks must release their callback when finished or cancelled.
    class AsyncReply {
    public:
        AsyncReply(http::Callback callback, http::RequestContext context, std::shared_ptr<const void> implementation, std::string_view operation) : callback_(std::move(callback)), context_(std::move(context)), implementation_(std::move(implementation)), operation_(operation) {}
        bool claim() {
            if (completed_.test_and_set(std::memory_order_acq_rel)) {
                replyLog.stdcWarning("%1 operation=%2 discarded a duplicate backend completion", context_.logContext().description(), operation_);
                return false;
            }
            if (context_.stopRequested()) {
                return false;
            }
            return true;
        }
        void send(const drogon::HttpResponsePtr &response) const {
            if (!context_.stopRequested()) {
                // This callback now runs on an inference worker. Drogon's
                // synchronous exception handler cannot cover this boundary.
                try {
                    callback_(response);
                } catch (const std::exception &error) {
                    replyLog.stdcCritical("%1 operation=%2 HTTP response callback failed: %3", context_.logContext().description(), operation_, error.what());
                }
            }
        }

    private:
        const http::Callback callback_;
        const http::RequestContext context_;
        const std::shared_ptr<const void> implementation_;
        const std::string operation_;
        std::atomic_flag completed_ = ATOMIC_FLAG_INIT;
    };
}

#endif // DSSP_API_V1_CONTROLLERS_ASYNCREPLY_H
