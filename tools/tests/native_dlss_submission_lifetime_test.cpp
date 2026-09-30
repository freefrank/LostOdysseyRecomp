#include "gpu/dlss_submission_lifetime.h"

#include <cstdlib>
#include <iostream>

namespace {
unsigned checks = 0;
void Require(bool value, const char* message) {
    ++checks;
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}
int main() {
    gpu::dlss::SubmissionLifetime lifetime;
    Require(lifetime.Empty(), "cold session has no feature users");
    const auto first = lifetime.Record();
    const auto second = lifetime.Record();
    Require(first && second > first, "nonzero unique recording IDs");
    lifetime.CompleteThrough(0);
    Require(!lifetime.Empty(), "completion never releases unsubmitted recordings");
    Require(!lifetime.Submit(0, 1), "zero ID rejected");
    Require(!lifetime.Submit(first, 0), "zero serial rejected");
    Require(!lifetime.Submit(second + 1, 1), "unknown ID rejected");
    Require(lifetime.Submit(first, 10), "successful batch attaches fence");
    Require(lifetime.Submit(first, 10), "duplicate submit is idempotent");
    Require(!lifetime.Submit(first, 2), "duplicate cannot move to earlier fence");
    Require(!lifetime.Submit(first, 20), "duplicate cannot move to later fence");
    lifetime.Discard(first);
    Require(!lifetime.Empty(), "discard cannot release a submitted feature user");
    lifetime.Discard(second);
    lifetime.CompleteThrough(9);
    Require(!lifetime.Empty(), "feature retained before actual submission completion");
    lifetime.CompleteThrough(10);
    Require(lifetime.Empty(), "feature released at completion");
    Require(!lifetime.Submit(first, 11), "completed use cannot be revived");
    lifetime.Discard(first);
    Require(lifetime.Empty(), "late discard is harmless");
    const auto third = lifetime.Record();
    Require(third > second, "feature reconfiguration never reuses use IDs");
    lifetime.CompleteThrough(5);
    Require(!lifetime.Submit(third, 10), "out-of-order completion cannot rewind watermark");
    Require(!lifetime.Empty(), "invalid submission leaves recording retained");
    Require(lifetime.Submit(third, 11), "next successful queue submission");
    const auto fourth = lifetime.Record();
    Require(lifetime.Submit(fourth, 12), "two independent in-flight slots");
    lifetime.CompleteThrough(12);
    Require(lifetime.Empty(), "newest same-queue fence covers earlier submissions");
    const auto excluded = lifetime.Record();
    Require(lifetime.Submit(excluded, 13), "excluded NGX list still retains prefix fallback use");
    lifetime.Discard(excluded);
    Require(!lifetime.Empty(), "failed NGX recording waits for prefix fence");
    lifetime.CompleteThrough(13);
    Require(lifetime.Empty(), "fallback prefix completion permits feature retirement");
    const auto abandoned = lifetime.Record();
    lifetime.CompleteThrough(20);
    Require(!lifetime.Empty(), "unrelated completion never retires abandoned recording");
    lifetime.Discard(abandoned);
    Require(lifetime.Empty(), "discard of never-submitted batch permits retirement");
    std::cout << checks << " submission lifetime checks passed\n";
}
