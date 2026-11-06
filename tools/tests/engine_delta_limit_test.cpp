#include <patches/engine_delta_limit.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

static void Require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main()
{
    using namespace engine_delta;
    // Normal frames at every supported rate, and slow ones, are never touched.
    for (const double delta : {1.0 / 120, 1.0 / 60, 1.0 / 30, 0.05, 0.1, 0.143, 0.25})
        Require(Limit(delta, kDefaultMaxSeconds) == delta, "frames up to the limit pass unchanged");
    // The stalls seen when the Experimental Staff lift arrival failed on the console are cut.
    for (const double delta : {0.453, 0.696, 1.0, 12.4})
        Require(Limit(delta, kDefaultMaxSeconds) == kDefaultMaxSeconds, "a stall is cut to the limit");
    Require(Limit(1.0, 0.0) == 1.0, "a zero limit turns it off");
    Require(Limit(0.0, kDefaultMaxSeconds) == 0.0, "a zero delta stays zero");
    Require(Limit(-0.5, kDefaultMaxSeconds) == -0.5, "a negative delta passes through");
    Require(std::isnan(Limit(std::numeric_limits<double>::quiet_NaN(), kDefaultMaxSeconds)), "NaN passes through");
    Require(Limit(std::numeric_limits<double>::infinity(), kDefaultMaxSeconds) == kDefaultMaxSeconds, "infinity is cut");

    Require(ParseMaxSeconds(nullptr) == kDefaultMaxSeconds, "unset selects the default");
    Require(ParseMaxSeconds("") == kDefaultMaxSeconds, "empty selects the default");
    Require(ParseMaxSeconds("abc") == kDefaultMaxSeconds, "text selects the default");
    Require(ParseMaxSeconds("100x") == kDefaultMaxSeconds, "trailing text selects the default");
    Require(ParseMaxSeconds("-5") == kDefaultMaxSeconds, "negative selects the default");
    Require(ParseMaxSeconds("nan") == kDefaultMaxSeconds, "NaN selects the default");
    Require(ParseMaxSeconds("1e9") == kDefaultMaxSeconds, "an absurd value selects the default");
    Require(ParseMaxSeconds("0") == 0.0, "0 turns the limit off");
    Require(ParseMaxSeconds("100") == 0.1, "milliseconds are converted to seconds");
    Require(ParseMaxSeconds("33.5") == 0.0335, "fractions are accepted");
    std::puts("engine_delta_limit_test ok");
    return 0;
}
