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
        LogCapture() : previous_(stdc::Logger::logCallback()) {
            std::lock_guard lock(mutex_);
            active_ = this;
            stdc::Logger::setLogCallback(&capture);
        }
        ~LogCapture() {
            std::lock_guard lock(mutex_);
            stdc::Logger::setLogCallback(previous_);
            active_ = nullptr;
        }
        LogCapture(const LogCapture &) = delete;
        LogCapture &operator=(const LogCapture &) = delete;
        std::vector<Record> records(std::string_view category, int level) const {
            std::lock_guard lock(mutex_);
            std::vector<Record> result;
            for (const auto &record : records_) {
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
            std::lock_guard lock(mutex_);
            if (active_) {
                active_->records_.push_back({level, context.category ? context.category : "", std::string(message)});
            }
        }
        inline static std::mutex mutex_;
        inline static LogCapture *active_ = nullptr;
        const stdc::Logger::LogCallback previous_;
        std::vector<Record> records_;
    };
}

#endif // DSSP_TESTS_LOGCAPTURE_H
