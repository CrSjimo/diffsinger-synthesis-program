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
        AsyncReply(http::Callback callback, http::RequestContext context, std::shared_ptr<const void> implementation, std::string_view operation) : m_callback(std::move(callback)), m_context(std::move(context)), m_implementation(std::move(implementation)), m_operation(operation) {}

        bool claim() {
            if (m_completed.test_and_set(std::memory_order_acq_rel)) {
                replyLog.stdcWarning("%1 operation=%2 discarded a duplicate backend completion", m_context.logContext().description(), m_operation);
                return false;
            }
            if (m_context.stopRequested()) {
                return false;
            }
            return true;
        }

        void send(const drogon::HttpResponsePtr &response) const {
            if (!m_context.stopRequested()) {
                // This callback now runs on an inference worker. Drogon's
                // synchronous exception handler cannot cover this boundary.
                try {
                    m_callback(response);
                } catch (const std::exception &error) {
                    replyLog.stdcCritical("%1 operation=%2 HTTP response callback failed: %3", m_context.logContext().description(), m_operation, error.what());
                }
            }
        }

    private:
        const http::Callback m_callback;
        const http::RequestContext m_context;
        const std::shared_ptr<const void> m_implementation;
        const std::string m_operation;
        std::atomic_flag m_completed = ATOMIC_FLAG_INIT;
    };
}

#endif // DSSP_API_V1_CONTROLLERS_ASYNCREPLY_H
