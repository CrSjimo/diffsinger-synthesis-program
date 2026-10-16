#ifndef DSSP_APP_LOGTIMESTAMP_H
#define DSSP_APP_LOGTIMESTAMP_H

#include <chrono>
#include <string>

namespace dssp::detail {
    std::string formatLogTimestamp(std::chrono::system_clock::time_point now);
    // Local time minus UTC in seconds, including daylight saving time.
    std::chrono::seconds logTimezoneOffset(std::chrono::system_clock::time_point now);
}

#endif // DSSP_APP_LOGTIMESTAMP_H
