#include <chrono>
#include <memory>

#include <gtest/gtest.h>
#include <unicode/timezone.h>

#include "../app/LogTimestamp.h"

namespace dssp::tests {
    namespace {
        // ICU's default zone is process-global, so these tests have a separate
        // executable and restore it afterwards. No system setting is changed.
        class LogTimestampTest : public testing::Test {
        protected:
            void SetUp() override {
                previous_.reset(icu::TimeZone::createDefault());
            }

            void TearDown() override {
                icu::TimeZone::adoptDefault(previous_.release());
            }

            void zone(const char *value) {
                auto *selected = icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(value));
                ASSERT_NE(selected, nullptr);
                icu::TimeZone::adoptDefault(selected);
            }

            static auto utc(int y, unsigned m, unsigned d, int h, int min, int sec, int ms = 0) {
                using namespace std::chrono;
                return sys_days{year{y} / month{m} / day{d}} + hours{h} + minutes{min} + seconds{sec} + milliseconds{ms};
            }

        private:
            std::unique_ptr<icu::TimeZone> previous_;
        };

        TEST_F(LogTimestampTest, UtcPreservesMilliseconds) {
            zone("UTC");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 6, 1, 5, 45, 12, 123)), "2024-06-01T05:45:12.123+00:00");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 6, 1, 5, 45, 12, 7)), "2024-06-01T05:45:12.007+00:00");
        }

        TEST_F(LogTimestampTest, PositiveOffsetCrossesYearBoundary) {
            zone("Asia/Shanghai");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2023, 12, 31, 20, 0, 0)), "2024-01-01T04:00:00.000+08:00");
        }

        TEST_F(LogTimestampTest, NegativeOffsetCrossesYearBoundary) {
            zone("America/Los_Angeles");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 1, 1, 3, 0, 0)), "2023-12-31T19:00:00.000-08:00");
        }

        TEST_F(LogTimestampTest, FractionalHourOffsets) {
            zone("Asia/Kolkata");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 6, 1, 5, 45, 12)), "2024-06-01T11:15:12.000+05:30");
            zone("GMT-03:30");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 6, 1, 5, 45, 12)), "2024-06-01T02:15:12.000-03:30");
            zone("Asia/Kathmandu");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 6, 1, 5, 45, 12)), "2024-06-01T11:30:12.000+05:45");
        }

        TEST_F(LogTimestampTest, SpringForwardUsesOffsetAtTheLoggedInstant) {
            zone("America/New_York");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 3, 10, 6, 59, 59, 999)), "2024-03-10T01:59:59.999-05:00");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 3, 10, 7, 0, 0)), "2024-03-10T03:00:00.000-04:00");
        }

        TEST_F(LogTimestampTest, FallBackDistinguishesRepeatedLocalTimes) {
            zone("America/New_York");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 11, 3, 5, 30, 0)), "2024-11-03T01:30:00.000-04:00");
            EXPECT_EQ(detail::formatLogTimestamp(utc(2024, 11, 3, 6, 30, 0)), "2024-11-03T01:30:00.000-05:00");
        }
    }
}
