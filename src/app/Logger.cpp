#include "Logger.h"
#include "LogTimestamp.h"

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

        std::string formatRecord(int level, const stdc::LogContext &context, const std::string_view &message) {
            return std::format("[{}] [{}] [{}]: {}", detail::formatLogTimestamp(std::chrono::system_clock::now()), context.category ? context.category : "", severityName(level), message);
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
