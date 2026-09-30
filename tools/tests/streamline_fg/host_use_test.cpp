#include "gpu/dlss_fg_host_use.h"
#include <cstdio>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool value, const char* reason) {
    ++checks;
    if (!value) throw std::runtime_error(reason);
}
}
int main() {
    try {
        using gpu::dlss_fg::HostInputUse;
        int list = 0, other = 0;
        HostInputUse use;
        Check(!use.Begin(nullptr), "missing list cannot own a recording");
        Check(use.Begin(&list), "begin unsubmitted input recording");
        Check(!use.Begin(&other), "recording cannot overwrite its owner");
        Check(!use.CanCancel(&other), "different command list cannot cancel this use");
        Check(!use.Completed(), "queue idle or missing serial is not SDK completion");
        Check(!use.Submitted(true, 1), "submission needs an attempted boundary");
        Check(!use.Canceled(&list, false, true, true), "failed native reset retains recording");
        Check(!use.Canceled(&list, true, false, true), "failed tag revocation retains recording");
        Check(!use.Canceled(&list, true, true, false), "unfinished producer copy retains recording");
        Check(use.Pending(), "all failed cancellations preserve the only owner");
        Check(use.Canceled(&list, true, true, true) && !use.Pending(), "three proofs cancel only unsubmitted input");
        Check(!use.Canceled(&list, true, true, true), "duplicate cancellation cannot release a new use");
        Check(use.Begin(&list) && use.SubmissionStarted(), "native submission attempt starts");
        Check(!use.CanCancel(&list), "attempted submit is irreversible even before native result");
        Check(!use.Submitted(false, 2) && use.Serial() == 0, "failed submit publishes no host serial");
        Check(!use.Canceled(&list, true, true, true) && !use.Completed(), "failed/uncertain submit cannot be retired");

        HostInputUse submitted;
        Check(submitted.Begin(&list) && submitted.SubmissionStarted(), "new successful use");
        Check(!submitted.SubmissionStarted(), "duplicate native attempt rejected");
        Check(!submitted.Submitted(true, 0), "zero serial cannot publish successful submission");
        Check(submitted.Submitted(true, 42) && submitted.Serial() == 42, "publish native successful serial");
        Check(!submitted.Submitted(true, 43) && submitted.Serial() == 42, "duplicate serial cannot overwrite use");
        Check(!submitted.Canceled(&list, true, true, true), "submitted commands never use cancellation path");
        // Production invokes Completed only after PresentQueueCompletion's
        // matching successful fence wait (covered separately by session test).
        Check(submitted.Completed() && !submitted.Pending(), "checked SDK drain retires submitted host use");
        Check(submitted.Begin(&other), "retirement permits the next command recording");
        HostInputUse off;
        Check(off.SubmissionStarted() && off.Submitted(true, 9) && off.Completed(), "FG-off host frame has no borrowed input");
        std::printf("host input ownership: %u checks passed\n", checks);
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
