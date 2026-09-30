#include "probe_options.h"
#include <cstdio>

int main() {
    char program[] = "probe";
    char noActivate[] = "--no-activate";
    char explicitWait[] = "--explicit-input-wait";
    char unknown[] = "--unknown";
    char* defaults[] = {program};
    char* combined[] = {program, noActivate, explicitWait};
    char* reversed[] = {program, explicitWait, noActivate};
    char* duplicated[] = {program, explicitWait, explicitWait};
    char* invalid[] = {program, unknown};
    const auto normal = probe::ParseProbeOptions(1, defaults);
    const auto both = probe::ParseProbeOptions(3, combined);
    const auto bothReversed = probe::ParseProbeOptions(3, reversed);
    if (!normal || normal->noActivate || normal->explicitInputWait ||
        !both || !both->noActivate || !both->explicitInputWait ||
        !bothReversed || !bothReversed->noActivate || !bothReversed->explicitInputWait ||
        probe::ParseProbeOptions(3, duplicated) || probe::ParseProbeOptions(2, invalid)) {
        std::fputs("probe options test failed\n", stderr);
        return 1;
    }
    std::puts("probe options test passed");
}
