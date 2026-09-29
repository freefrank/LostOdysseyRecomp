// kernel/time_fields.h against the C library's timegm/gmtime (the previous
// implementation of RtlTimeFieldsToTime / RtlTimeToTimeFields).
#include <kernel/time_fields.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <initializer_list>

#ifdef _WIN32
#define timegm _mkgmtime
static void Gm(time_t t, tm* out) { gmtime_s(out, &t); }
#else
static void Gm(time_t t, tm* out) { gmtime_r(&t, out); }
#endif

static int failures = 0;
static void Check(bool ok, const char* what, long long a, long long b)
{
    if (!ok && failures++ < 20) std::printf("FAIL %s: %lld != %lld\n", what, a, b);
}

int main()
{
    static_assert(time_fields::DaysFromCivil(1970, 1, 1) == 0);
    static_assert(time_fields::DaysFromCivil(2000, 3, 1) == 11017);
    static_assert(time_fields::FieldsFromSeconds(0).weekday == 4);
    unsigned checks = 0;
    // Every day 1970-2099 at a few times of day, both directions.
    for (int64_t day = 0; day < 47482; ++day)
        for (int64_t second : {0LL, 1LL, 43199LL, 86399LL}) {
            const int64_t t = day * 86400 + second;
            tm c{};
            Gm(time_t(t), &c);
            const auto f = time_fields::FieldsFromSeconds(t);
            Check(f.year == c.tm_year + 1900 && f.month == c.tm_mon + 1 && f.day == c.tm_mday &&
                  f.hour == c.tm_hour && f.minute == c.tm_min && f.second == c.tm_sec && f.weekday == c.tm_wday,
                  "fields", t, f.year * 10000 + f.month * 100 + f.day);
            Check(time_fields::SecondsFromFields(c.tm_year + 1900, c.tm_mon + 1, c.tm_mday, c.tm_hour, c.tm_min, c.tm_sec) == t,
                  "seconds", time_fields::SecondsFromFields(c.tm_year + 1900, c.tm_mon + 1, c.tm_mday, c.tm_hour, c.tm_min, c.tm_sec), t);
            checks += 2;
        }
    // Out-of-range fields normalize like timegm (month 13, day 0/40, hour 25, ...).
    const int fields[][6] = {{2008, 13, 1, 0, 0, 0}, {2008, 0, 1, 0, 0, 0}, {2008, 2, 30, 0, 0, 0}, {2008, 3, 0, 0, 0, 0},
        {2007, 12, 40, 25, 61, 61}, {2001, 1, 1, 0, 0, 0}, {2100, 2, 29, 0, 0, 0}, {2000, 2, 29, 23, 59, 59},
        {1999, 24, 1, 0, 0, 0}, {2010, 6, 15, 12, 30, 45}, {2008, 1, 1, 0, 0, 0}, {2107, 12, 31, 23, 59, 59}};
    for (const auto& v : fields) {
        tm c{};
        c.tm_year = v[0] - 1900; c.tm_mon = v[1] - 1; c.tm_mday = v[2]; c.tm_hour = v[3]; c.tm_min = v[4]; c.tm_sec = v[5];
        const long long expected = (long long)timegm(&c);
        const long long got = time_fields::SecondsFromFields(v[0], v[1], v[2], v[3], v[4], v[5]);
        Check(got == expected, "normalize", got, expected);
        ++checks;
    }
    if (failures) { std::printf("time fields: %d of %u checks failed\n", failures, checks); return 1; }
    std::printf("time fields: %u checks passed\n", checks);
    return 0;
}
