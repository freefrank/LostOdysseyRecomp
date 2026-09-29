#pragma once

#include <cstdint>

// Calendar conversion for RtlTimeFieldsToTime / RtlTimeToTimeFields in plain
// integer arithmetic (proleptic Gregorian, UTC). The C library's timegm and
// gmtime_r take a lock and check the time zone database on every call on
// macOS; the game converts file times in bulk while loading (about 1.3 s of a
// disc switch on an M1 Pro). Out-of-range fields normalize the way timegm does.
namespace time_fields
{
    struct Civil
    {
        int64_t year = 1970;
        int64_t month = 1;   // 1-12 after normalization
        int64_t day = 1;     // 1-31
        int64_t hour = 0, minute = 0, second = 0;
        int64_t weekday = 4; // 0 = Sunday
    };

    constexpr int64_t FloorDiv(int64_t a, int64_t b)
    {
        return a / b - ((a % b != 0) && ((a < 0) != (b < 0)));
    }

    // Days since 1970-01-01 (Howard Hinnant's days_from_civil).
    constexpr int64_t DaysFromCivil(int64_t year, int64_t month, int64_t day)
    {
        // Carry the month into the year first, as timegm does.
        year += FloorDiv(month - 1, 12);
        month -= FloorDiv(month - 1, 12) * 12;
        year -= month <= 2;
        const int64_t era = FloorDiv(year, 400);
        const int64_t yoe = year - era * 400;
        const int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    constexpr Civil CivilFromDays(int64_t days)
    {
        days += 719468;
        const int64_t era = FloorDiv(days, 146097);
        const int64_t doe = days - era * 146097;
        const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const int64_t mp = (5 * doy + 2) / 153;
        Civil civil;
        civil.day = doy - (153 * mp + 2) / 5 + 1;
        civil.month = mp < 10 ? mp + 3 : mp - 9;
        civil.year = yoe + era * 400 + (civil.month <= 2);
        return civil;
    }

    // Seconds since the Unix epoch; fields may be out of range (timegm semantics).
    constexpr int64_t SecondsFromFields(int64_t year, int64_t month, int64_t day,
        int64_t hour, int64_t minute, int64_t second)
    {
        return DaysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
    }

    constexpr Civil FieldsFromSeconds(int64_t seconds)
    {
        const int64_t days = FloorDiv(seconds, 86400);
        const int64_t rest = seconds - days * 86400;
        Civil civil = CivilFromDays(days);
        civil.hour = rest / 3600;
        civil.minute = rest / 60 % 60;
        civil.second = rest % 60;
        civil.weekday = FloorDiv(days + 4, 7) * -7 + days + 4; // 1970-01-01 was a Thursday
        return civil;
    }
}
