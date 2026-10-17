#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>
#include <stdcorelib/console.h>
#include <stdcorelib/support/json.h>
#include <stdcorelib/support/logging.h>
#include <unicode/timezone.h>

#include "../app/Logger.h"

namespace dssp::tests {
    namespace {
        class LoggerTest : public testing::Test {
        protected:
            void SetUp() override {
                m_callback = stdc::Logger::logCallback();
                m_level = logLevel();
                m_mode = logMode();
                m_color = stdc::console::get_color_mode();
                m_zone.reset(icu::TimeZone::createDefault());
                icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone("Asia/Shanghai"));
                stdc::console::set_color_mode(stdc::console::never);
                installLogger();
            }

            void TearDown() override {
                stdc::Logger::setLogCallback(m_callback);
                setLogLevel(m_level);
                setLogMode(m_mode);
                stdc::console::set_color_mode(m_color);
                icu::TimeZone::adoptDefault(m_zone.release());
            }

            static std::string output(int level, std::string_view message, const char *module = "dssp.Test") {
                testing::internal::CaptureStderr();
                stdc::Logger::logCallback()(level, stdc::LogContext(nullptr, 0, nullptr, module), message);
                return testing::internal::GetCapturedStderr();
            }

            static stdc::json::Value decode(const std::string &record) {
                EXPECT_EQ(std::count(record.begin(), record.end(), '\n'), 1);
                EXPECT_TRUE(record.ends_with('\n'));
                EXPECT_EQ(record.find('\x1b'), std::string::npos);
                stdc::json::ParseError error;
                auto value = stdc::json::Value::fromJson(record, false, &error);
                EXPECT_FALSE(static_cast<bool>(error)) << error.message();
                EXPECT_TRUE(value.isObject());
                return value;
            }

        private:
            stdc::Logger::LogCallback m_callback{};
            LogLevel m_level{};
            LogMode m_mode{};
            stdc::console::color_mode m_color{};
            std::unique_ptr<icu::TimeZone> m_zone;
        };

        TEST_F(LoggerTest, DefaultsToNormalAndInfo) {
            EXPECT_EQ(logMode(), LogMode::Normal);
            EXPECT_EQ(logLevel(), LogLevel::Info);
            const auto record = output(stdc::Logger::Information, "Hello");
            EXPECT_TRUE(record.starts_with('['));
            EXPECT_NE(record.find("+08:00] [dssp.Test] [INFO]: Hello\n"), std::string::npos);
        }

        TEST_F(LoggerTest, StructuredContainsTimestampOffsetModuleLevelAndMessage) {
            setLogMode(LogMode::Structured);
            using namespace std::chrono;
            const auto before = floor<milliseconds>(system_clock::now().time_since_epoch()).count();
            const auto record = decode(output(stdc::Logger::Warning, "Hello"));
            const auto after = floor<milliseconds>(system_clock::now().time_since_epoch()).count();
            EXPECT_EQ(record.toObject().size(), 5);
            EXPECT_TRUE(record["timestamp"].isInt());
            EXPECT_GE(record["timestamp"].toInt(), before);
            EXPECT_LE(record["timestamp"].toInt(), after);
            EXPECT_TRUE(record["timezone_offset"].isInt());
            EXPECT_EQ(record["timezone_offset"].toInt(), 28800);
            EXPECT_EQ(record["module"].toString(), "dssp.Test");
            EXPECT_EQ(record["level"].toString(), "WARNING");
            EXPECT_EQ(record["message"].toString(), "Hello");
        }

        TEST_F(LoggerTest, StructuredEscapesMultilineUnicodeAndControlCharacters) {
            setLogMode(LogMode::Structured);
            std::string message = "中文 \"quoted\" \\ path\nline\r\t\b\f\x1b[31m";
            message.push_back('\0');
            message += "tail";
            const auto record = decode(output(stdc::Logger::Information, message, "module\"\n\\"));
            EXPECT_EQ(record["message"].toString(), message);
            EXPECT_EQ(record["module"].toString(), "module\"\n\\");
        }

        TEST_F(LoggerTest, StructuredHandlesMissingModuleAndEmptyMessage) {
            setLogMode(LogMode::Structured);
            const auto record = decode(output(stdc::Logger::Information, "", nullptr));
            EXPECT_EQ(record["module"].toString(), "");
            EXPECT_EQ(record["message"].toString(), "");
        }

        TEST_F(LoggerTest, StructuredNeverAddsColorsEvenWhenForced) {
            stdc::console::set_color_mode(stdc::console::vt);
            EXPECT_NE(output(stdc::Logger::Warning, "Normal").find('\x1b'), std::string::npos);
            setLogMode(LogMode::Structured);
            const auto record = decode(output(stdc::Logger::Warning, "Structured"));
            EXPECT_EQ(record["message"].toString(), "Structured");
        }

        TEST_F(LoggerTest, StructuredSupportsEverySeverity) {
            setLogMode(LogMode::Structured);
            setLogLevel(LogLevel::Trace);
            for (const auto &[level, name] : {
                     std::pair{stdc::Logger::Trace, "TRACE"},
                     std::pair{stdc::Logger::Debug, "DEBUG"},
                     std::pair{stdc::Logger::Success, "SUCCESS"},
                     std::pair{stdc::Logger::Information, "INFO"},
                     std::pair{stdc::Logger::Warning, "WARNING"},
                     std::pair{stdc::Logger::Critical, "CRITICAL"},
                     std::pair{stdc::Logger::Fatal, "FATAL"},
                 }) {
                EXPECT_EQ(decode(output(level, "Message"))["level"].toString(), name);
            }
        }

        TEST_F(LoggerTest, BothModesRespectThresholdAndCanBeSwitched) {
            for (const auto mode : {LogMode::Structured, LogMode::Normal}) {
                setLogMode(mode);
                setLogLevel(LogLevel::Warning);
                EXPECT_TRUE(output(stdc::Logger::Information, "Hidden").empty());
                EXPECT_FALSE(output(stdc::Logger::Warning, "Visible").empty());
            }
        }
    }
}
