#include <debug/loading_state.h>
#include <cstdio>
#include <cstdlib>

int main() {
    unsigned checks = 0, failures = 0;
    const auto expect = [&](bool actual, bool expected, const char* name) {
        ++checks;
        if (actual != expected) { ++failures; std::printf("FAIL: %s\n", name); }
    };
    using debug_menu::loading_state::Loading;
    for (uint8_t slot = 0; slot < 6; ++slot) {
        expect(Loading(0, slot, 1), false, "queued request keeps player pacing");
        expect(Loading(0, slot, 2), true, "accepted request lifts pacing");
        expect(Loading(3, slot, 2), true, "package work remains active");
        expect(Loading(4, slot, 2), false, "activation restores pacing");
        expect(Loading(5, slot, 2), false, "presentation keeps player pacing");
        expect(Loading(6, slot, 2), false, "completion keeps player pacing");
        expect(Loading(0, slot, 0), false, "cancelled request restores pacing");
        expect(Loading(0, slot, -1), false, "failed request restores pacing");
    }
    expect(Loading(0, 6, 2), false, "idle loader keeps player pacing");
    expect(Loading(0, 255, 2), false, "invalid slot keeps player pacing");
    expect(Loading(255, 0, 2), false, "invalid stage keeps player pacing");
    std::printf("loading state: %u/%u checks passed\n", checks - failures, checks);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
