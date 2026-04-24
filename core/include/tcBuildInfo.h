#pragma once

// =============================================================================
// Build information
// Values are injected as compile definitions by trussc_app() in CMake.
// Timestamps reflect the moment CMake last configured the project — rerun
// `cmake ..` (or the equivalent preset) to refresh.
//
// Note: date / time / year / month / day / hour / minute / second are in local
// time (the machine configuring the build). timestamp() is UTC Unix seconds.
// =============================================================================

#include <cstdint>

// Fallbacks for cases where the macros aren't defined (e.g. library-only
// builds that skip trussc_app()).
#ifndef TRUSSC_BUILD_DATE
    #define TRUSSC_BUILD_DATE "unknown"
#endif
#ifndef TRUSSC_BUILD_TIME
    #define TRUSSC_BUILD_TIME "unknown"
#endif
#ifndef TRUSSC_BUILD_DATETIME
    #define TRUSSC_BUILD_DATETIME "unknown"
#endif
#ifndef TRUSSC_BUILD_UNIX
    #define TRUSSC_BUILD_UNIX 0LL
#endif
#ifndef TRUSSC_BUILD_YEAR
    #define TRUSSC_BUILD_YEAR 0
#endif
#ifndef TRUSSC_BUILD_MONTH
    #define TRUSSC_BUILD_MONTH 0
#endif
#ifndef TRUSSC_BUILD_DAY
    #define TRUSSC_BUILD_DAY 0
#endif
#ifndef TRUSSC_BUILD_HOUR
    #define TRUSSC_BUILD_HOUR 0
#endif
#ifndef TRUSSC_BUILD_MINUTE
    #define TRUSSC_BUILD_MINUTE 0
#endif
#ifndef TRUSSC_BUILD_SECOND
    #define TRUSSC_BUILD_SECOND 0
#endif

namespace trussc {

struct BuildInfo {
    static constexpr const char* date()     { return TRUSSC_BUILD_DATE; }       // "2026-04-23"
    static constexpr const char* time()     { return TRUSSC_BUILD_TIME; }       // "15:30:45"
    static constexpr const char* dateTime() { return TRUSSC_BUILD_DATETIME; }   // "2026-04-23 15:30:45"
    static constexpr int64_t timestamp()    { return TRUSSC_BUILD_UNIX; }       // Unix seconds
    static constexpr int year()             { return TRUSSC_BUILD_YEAR; }
    static constexpr int month()            { return TRUSSC_BUILD_MONTH; }      // 1-12
    static constexpr int day()              { return TRUSSC_BUILD_DAY; }        // 1-31
    static constexpr int hour()             { return TRUSSC_BUILD_HOUR; }       // 0-23
    static constexpr int minute()           { return TRUSSC_BUILD_MINUTE; }     // 0-59
    static constexpr int second()           { return TRUSSC_BUILD_SECOND; }     // 0-59
};

} // namespace trussc
