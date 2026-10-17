#ifndef DSSP_TESTS_LOGCAPTURE_H
#define DSSP_TESTS_LOGCAPTURE_H

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <stdcorelib/support/logging.h>

namespace dssp::tests {
    class LogCapture {
    public:
        struct Record {
            int level;
            std::string category;
            std::string message;
        };

        LogCapture() : m_previous(stdc::Logger::logCallback()) {
            std::lock_guard lock(m_mutex);
            m_active = this;
            stdc::Logger::setLogCallback(&capture);
        }

        ~LogCapture() {
            std::lock_guard lock(m_mutex);
            stdc::Logger::setLogCallback(m_previous);
            m_active = nullptr;
        }

        LogCapture(const LogCapture &) = delete;
        LogCapture &operator=(const LogCapture &) = delete;
        std::vector<Record> records(std::string_view category, int level) const {
            std::lock_guard lock(m_mutex);
            std::vector<Record> result;
            for (const auto &record : m_records) {
                if (record.category == category && record.level == level) {
                    result.push_back(record);
                }
            }
            return result;
        }

        std::vector<Record> accessRecords() const {
            return records("dssp.http.Access", stdc::Logger::Information);
        }

    private:
        static void capture(int level, const stdc::LogContext &context, const std::string_view &message) {
            std::lock_guard lock(m_mutex);
            if (m_active) {
                m_active->m_records.push_back({level, context.category ? context.category : "", std::string(message)});
            }
        }

        inline static std::mutex m_mutex;
        inline static LogCapture *m_active = nullptr;
        const stdc::Logger::LogCallback m_previous;
        std::vector<Record> m_records;
    };
}

#endif // DSSP_TESTS_LOGCAPTURE_H
