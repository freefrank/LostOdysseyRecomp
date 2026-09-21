#include "gpu/vulkan_submission_state.h"
#include "gpu/dlss_submission_lifetime.h"
#include <cstdlib>
#include <iostream>

namespace { unsigned checks = 0;
void Check(bool ok) { ++checks; if (!ok) { std::cerr << "failed check " << checks << '\n'; std::exit(1); } }
}
int main() {
    using gpu::submission::VulkanState;
    for (int32_t error : {-1, -2, -4, -13}) for (bool failReset : {false, true}) {
        VulkanState state;
        gpu::dlss::SubmissionLifetime uses;
        unsigned resets = 0, submits = 0, waits = 0;
        uint64_t serial = 999; int32_t raw = 99;
        auto reset = [&] { ++resets; return 0; };
        auto submit = [&] { ++submits; return 0; };
        Check(!state.Stopped());
        const auto good = uses.Record();
        Check(state.SubmitBatch(reset, submit, serial, raw) && serial == 1 && raw == 0);
        Check(uses.Submit(good, serial));
        const auto excluded = uses.Record();
        Check(!state.SubmitBatch([&] { ++resets; return failReset ? error : 0; },
            [&] { ++submits; return error; }, serial, raw));
        Check(serial == 0 && raw == error && state.LastSubmission() == 1);
        Check(resets == 2 && submits == (failReset ? 1u : 2u));
        uses.Discard(excluded);
        Check(!uses.Empty()); // previous GPU work is still live
        const auto before = resets + submits;
        Check(!state.SubmitBatch(reset, submit, serial, raw));
        Check(resets + submits == before && serial == 0 && raw == error);
        // A later native wait may drain older successful work, but cannot make
        // the stopped device record/submit again or revive the failed batch.
        Check(state.WaitSubmitted([&] { ++waits; return 0; }));
        uses.CompleteThrough(state.LastSubmission());
        Check(uses.Empty() && state.Stopped() && waits == 1);
        Check(!state.SubmitBatch(reset, submit, serial, raw) && resets + submits == before);
        state.Stop(-4);
        Check(state.Lost() && state.Failure() == error);
    }
    VulkanState fenceFailure;
    gpu::dlss::SubmissionLifetime uses;
    const auto id = uses.Record(); uint64_t serial; int32_t raw;
    Check(fenceFailure.SubmitBatch([] { return 0; }, [] { return 0; }, serial, raw));
    Check(uses.Submit(id, serial));
    Check(!fenceFailure.WaitSubmitted([] { return -4; }));
    Check(!uses.Empty() && fenceFailure.Lost());
    uses.Discard(id); Check(!uses.Empty()); // failure is NOT completion
    uses.AbandonAfterDeviceLoss(); Check(uses.Empty());
    Check(fenceFailure.Stopped() && fenceFailure.LastSubmission() == 1);
    VulkanState recordingFailure;
    recordingFailure.Stop(0); // even a caller with a missing native code stops
    Check(recordingFailure.Stopped() && recordingFailure.Failure() != 0);
    std::cout << checks << " native submission failure checks passed\n";
}
