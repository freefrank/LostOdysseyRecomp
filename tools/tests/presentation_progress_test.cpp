#include <gpu/presentation_progress.h>
#include <cstdio>
#include <cstdlib>
#include <thread>

static void Require(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

int main()
{
    namespace progress = gpu::presentation_progress;
    progress::Progress state;
    Require(state.Read().stage == progress::Stage::Idle, "initial stage");
    const auto earlyReturn = [&] {
        progress::Progress::Scope present(state, progress::Stage::Present);
        {
            progress::Progress::Scope prepare(state, progress::Stage::Prepare);
            prepare.Set(progress::Stage::ResizeDrain);
            Require(state.Read().stage == progress::Stage::ResizeDrain, "nested phase");
        }
        Require(state.Read().stage == progress::Stage::Present, "nested scope restores caller");
        return;
    };
    earlyReturn();
    Require(state.Read().stage == progress::Stage::Idle, "early exit restores idle");
    state.Submitted(17);
    Require(state.Read().pendingFence == 17 && state.Read().completed == 0,
        "accepted presentation is pending before QueuePresent returns");
    state.Completed();
    Require(state.Read().completed == 17 && state.Read().pendingFence == 0,
        "only completed fence retires pending presentation");
    state.Submitted(18);
    Require(state.Read().completed == 17 && state.Read().pendingFence == 18,
        "new submission does not claim completion");
    state.Completed();
    state.SwapchainChanged();
    Require(state.Read().swapchainGeneration == 1, "swapchain boundary counted");

    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (uint64_t serial = 19; serial < 100019; ++serial) {
            progress::Progress::Scope present(state, progress::Stage::Present);
            state.Policy((serial & 1) != 0, (serial & 1) != 0);
            state.Image(uint32_t(serial % 3));
            state.Submitted(serial);
            state.Completed();
        }
        done.store(true, std::memory_order_release);
    });
    uint64_t observations = 0;
    do {
        const auto snapshot = state.Read();
        Require(snapshot.loading == snapshot.vsync, "policy fields use one atomic observation");
        Require(snapshot.image == progress::NoImage || snapshot.image < 3, "valid image observation");
        Require(snapshot.submitted >= 18 && snapshot.submitted <= 100018, "valid serial observation");
        ++observations;
    } while (!done.load(std::memory_order_acquire));
    writer.join();
    Require(observations && state.Read().stage == progress::Stage::Idle, "concurrent reader and final idle");
    state.ResetSubmissions();
    state.Submitted(1);
    Require(state.Read().completed == 0 && state.Read().pendingFence == 1 &&
        state.Read().image == progress::NoImage, "replacement device starts a fresh submission lifetime");
}
