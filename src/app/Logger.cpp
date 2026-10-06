#include "Logger.h"

#include <chrono>
#include <cstdio>
#include <format>
#include <mutex>
#include <string>
#include <string_view>

#include <stdcorelib/console.h>
#include <stdcorelib/support/logging.h>

namespace dssp {

    namespace {

        // Serializes the level check, formatting and console write so records
        // from Drogon worker threads never interleave.
        std::mutex &loggerMutex() {
            static std::mutex mutex;
            return mutex;
        }

        // The logLevel configuration item, defaulting to Info.
        LogLevel &configuredLogLevel() {
            static LogLevel level = LogLevel::Info;
            return level;
        }

        // LogLevel is ordered like stdc::Logger::Level but starts at zero.
        constexpr int toStdcLevel(LogLevel level) {
            return static_cast<int>(level) + 1;
        }

        std::string_view severityName(int level) {
            switch (level) {
                case stdc::Logger::Trace:
                    return "TRACE";
                case stdc::Logger::Debug:
                    return "DEBUG";
                case stdc::Logger::Success:
                    return "SUCCESS";
                case stdc::Logger::Information:
                    return "INFO";
                case stdc::Logger::Warning:
                    return "WARNING";
                case stdc::Logger::Critical:
                    return "CRITICAL";
                case stdc::Logger::Fatal:
                    return "FATAL";
                default:
                    return "INFO";
            }
        }

        int severityColor(int level) {
            switch (level) {
                case stdc::Logger::Trace:
                    return stdc::console::lightblack;
                case stdc::Logger::Debug:
                    return stdc::console::cyan;
                case stdc::Logger::Success:
                    return stdc::console::lightgreen;
                case stdc::Logger::Information:
                    return stdc::console::nocolor;
                case stdc::Logger::Warning:
                    return stdc::console::yellow;
                case stdc::Logger::Critical:
                case stdc::Logger::Fatal:
                    return stdc::console::red;
                default:
                    return stdc::console::nocolor;
            }
        }

        // [2024-06-01T13:45:12.123+08:00], matching the local time with its
        // UTC offset that choruskit writes.
        std::string formatTimestamp() {
            using namespace std::chrono;
            const auto now = system_clock::now();
            const auto millis = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            const auto zoned = zoned_time{current_zone(), floor<seconds>(now)};
            const auto offset = duration_cast<minutes>(zoned.get_info().offset).count();
            const char sign = offset < 0 ? '-' : '+';
            const auto magnitude = offset < 0 ? -offset : offset;

            std::string timestamp = std::format("{:%FT%T}", zoned);
            timestamp += std::format(".{:03d}", static_cast<int>(millis.count()));
            timestamp += std::format("{}{:02d}:{:02d}", sign, magnitude / 60, magnitude % 60);
            return timestamp;
        }

        std::string formatRecord(int level, const stdc::LogContext &context, const std::string_view &message) {
            return std::format("[{}] [{}] [{}]: {}", formatTimestamp(), context.category ? context.category : "", severityName(level), message);
        }

        void handleLogRecord(int level, const stdc::LogContext &context, const std::string_view &message) {
            std::lock_guard lock(loggerMutex());
            if (level < toStdcLevel(configuredLogLevel())) {
                return;
            }
            const std::string record = formatRecord(level, context, message) + '\n';
            // stdcorelib emits the attributes only for a terminal and transcodes
            // for the Windows console.
            stdc::console::fputs(stdc::console::nostyle, severityColor(level), stdc::console::nocolor, record, stderr);
        }

    }

    void installLogger() {
        stdc::Logger::setLogCallback(&handleLogRecord);
    }

    void setLogLevel(LogLevel level) {
        std::lock_guard lock(loggerMutex());
        configuredLogLevel() = level;
    }

    LogLevel logLevel() {
        std::lock_guard lock(loggerMutex());
        return configuredLogLevel();
    }

}
