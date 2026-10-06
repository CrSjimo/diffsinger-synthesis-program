#ifndef DSSP_APP_LOGGER_H
#define DSSP_APP_LOGGER_H

namespace dssp {

    // Ordered from the most verbose to the least, matching stdcorelib's levels.
    enum class LogLevel {
        Trace,
        Debug,
        Success,
        Info,
        Warning,
        Critical,
        Fatal,
    };

    // Registers the console handler with stdcorelib's logging mechanism.
    void installLogger();

    // The logLevel configuration item, defaulting to Info.
    void setLogLevel(LogLevel level);
    LogLevel logLevel();

}

#endif // DSSP_APP_LOGGER_H
