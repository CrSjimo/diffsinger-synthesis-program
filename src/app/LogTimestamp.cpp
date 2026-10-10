#include "LogTimestamp.h"

#include <format>
#include <memory>
#include <stdexcept>

#include <unicode/timezone.h>

namespace dssp::detail {
    std::string formatLogTimestamp(std::chrono::system_clock::time_point now) {
        using namespace std::chrono;
        // ICU's default time zone is detected from the host system. Query at
        // the UTC instant being logged, so DST transitions are unambiguous.
        const std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
        if (!zone) {
            throw std::bad_alloc();
        }
        UErrorCode status = U_ZERO_ERROR;
        int32_t rawOffset = 0, dstOffset = 0;
        zone->getOffset(duration<double, std::milli>(now.time_since_epoch()).count(), false, rawOffset, dstOffset, status);
        if (U_FAILURE(status)) {
            throw std::runtime_error(std::string("Cannot determine local time zone offset: ") + u_errorName(status));
        }
        const milliseconds shift{static_cast<int64_t>(rawOffset) + dstOffset};
        const auto local = now + shift;
        const auto localDay = floor<days>(local);
        const year_month_day date{localDay};
        const hh_mm_ss time{floor<milliseconds>(local - localDay)};
        const auto offset = duration_cast<minutes>(shift).count();
        const char sign = offset < 0 ? '-' : '+';
        const auto magnitude = offset < 0 ? -offset : offset;

        // Numeric formatting avoids libc++'s experimental time-zone database
        // and its separate chrono formatter support.
        return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}{}{:02d}:{:02d}",
                           static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                           time.hours().count(), time.minutes().count(), time.seconds().count(), time.subseconds().count(),
                           sign, magnitude / 60, magnitude % 60);
    }
}
