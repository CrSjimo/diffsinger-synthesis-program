#include "LogTimestamp.h"

#include <format>
#include <memory>
#include <stdexcept>

#include <unicode/timezone.h>

namespace dssp::detail {
    namespace {
        std::chrono::milliseconds localOffset(std::chrono::system_clock::time_point now) {
            // ICU's default time zone is detected from the host system. Query at
            // the UTC instant being logged, so DST transitions are unambiguous.
            const std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
            if (!zone) {
                throw std::bad_alloc();
            }
            UErrorCode status = U_ZERO_ERROR;
            int32_t rawOffset = 0, dstOffset = 0;
            zone->getOffset(std::chrono::duration<double, std::milli>(now.time_since_epoch()).count(), false, rawOffset, dstOffset, status);
            if (U_FAILURE(status)) {
                throw std::runtime_error(std::string("Cannot determine local time zone offset: ") + u_errorName(status));
            }
            return std::chrono::milliseconds{static_cast<int64_t>(rawOffset) + dstOffset};
        }

        std::string formatOffset(std::chrono::milliseconds shift) {
            const auto offset = std::chrono::duration_cast<std::chrono::minutes>(shift).count();
            const char sign = offset < 0 ? '-' : '+';
            const auto magnitude = offset < 0 ? -offset : offset;
            return std::format("{}{:02d}:{:02d}", sign, magnitude / 60, magnitude % 60);
        }
    }

    std::chrono::seconds logTimezoneOffset(std::chrono::system_clock::time_point now) {
        return std::chrono::duration_cast<std::chrono::seconds>(localOffset(now));
    }

    std::string formatLogTimestamp(std::chrono::system_clock::time_point now) {
        using namespace std::chrono;
        const auto shift = localOffset(now);
        const auto local = now + shift;
        const auto localDay = floor<days>(local);
        const year_month_day date{localDay};
        const hh_mm_ss time{floor<milliseconds>(local - localDay)};

        // Numeric formatting avoids libc++'s experimental time-zone database
        // and its separate chrono formatter support.
        return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}{}",
                           static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                           time.hours().count(), time.minutes().count(), time.seconds().count(), time.subseconds().count(),
                           formatOffset(shift));
    }
}
