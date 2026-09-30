#include "shared/frame_generation/core.h"
#include "shared/frame_generation/environment.h"
#include <cstdio>
#include <limits>
#include <cstdlib>
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL %u line %d: %s\n",checks,__LINE__,#x); return EXIT_FAILURE; } } while(false)
int main() {
    using namespace framegen;
    Config dlss{Provider::Dlss, Mode::Fixed, 1, 0};
    Capabilities caps{true, 3, true};
    CHECK(Select(dlss,caps).Enabled());
    CHECK(!Select({},caps).Enabled());
    CHECK(Select(dlss,{}).rejection == Rejection::Unavailable);
    auto mfg=dlss; mfg.mode=Mode::Dynamic; mfg.targetFrameRate=144;
    CHECK(Select(mfg,caps).config == mfg);
    CHECK(Select(mfg,{true,3,false}).rejection == Rejection::DynamicUnsupported);
    mfg.targetFrameRate=std::numeric_limits<float>::quiet_NaN();
    CHECK(Select(mfg,caps).rejection == Rejection::InvalidConfig);
    mfg.targetFrameRate=-1;
    CHECK(Select(mfg,caps).rejection == Rejection::InvalidConfig);
    mfg=dlss; mfg.generatedFrames=4;
    CHECK(Select(mfg,caps).rejection == Rejection::Multiplier);
    mfg.generatedFrames=0;
    CHECK(Select(mfg,caps).rejection == Rejection::InvalidConfig);
    // LO_FG_MULTIPLIER accepts the same 2x..6x range as the settings menu.
    CHECK(ParseEnvironment("dlss","fixed","2",nullptr).config.generatedFrames == 1);
    const auto six=ParseEnvironment("dlss","fixed","6",nullptr);
    CHECK(six.Enabled() && six.config.generatedFrames == kMaxMultiplier - 1);
    CHECK(ParseEnvironment("dlss","fixed","7",nullptr).error != nullptr);
    CHECK(ParseEnvironment("dlss","fixed","16",nullptr).error != nullptr);
    CHECK(ParseEnvironment("dlss","fixed","1",nullptr).error != nullptr);
    Config fsr{Provider::Fsr,Mode::Fixed,1,0};
    CHECK(Select(fsr,{true,1,false}).Enabled());
    fsr.mode=Mode::Dynamic;
    CHECK(!Select(fsr,{true,1,false}).Enabled());
    SwapchainOwner chain;
    CHECK(!chain.Acquire(Provider::Off));
    CHECK(chain.Acquire(Provider::Dlss));
    CHECK(!chain.Acquire(Provider::Fsr));
    CHECK(!chain.Release(Provider::Dlss,false));
    CHECK(!chain.Release(Provider::Fsr,true));
    CHECK(chain.Release(Provider::Dlss,true));
    CHECK(chain.Acquire(Provider::Fsr));
    InputLease lease;
    auto owner=std::make_shared<int>(42); std::weak_ptr<int> weak=owner;
    CHECK(!lease.Begin({}));
    CHECK(lease.Begin(owner)); owner.reset();
    CHECK(!weak.expired());
    CHECK(!lease.Begin(std::make_shared<int>(43)));
    CHECK(!lease.CancelRecorded(true,false,true));
    CHECK(!lease.Complete(1,true));
    CHECK(lease.SubmitStart());
    CHECK(!lease.CancelRecorded(true,true,true));
    CHECK(!lease.Submitted(false,1));
    CHECK(!lease.Submitted(true,0));
    CHECK(!weak.expired());
    CHECK(lease.Submitted(true,7));
    CHECK(!lease.Presented(false));
    CHECK(!lease.Complete(7,true));
    CHECK(lease.Presented(true));
    CHECK(!lease.Complete(7,false));
    CHECK(!lease.Complete(8,true));
    CHECK(lease.Complete(7,true));
    CHECK(weak.expired()); CHECK(!lease.Pending());
    CHECK(lease.Begin(std::make_shared<int>(1)));
    CHECK(lease.CancelRecorded(true,true,true));
    History history; HistoryKey key{1,1,2560,1440,1706,960,28,dlss};
    CHECK(history.NeedsReset(100,key,false)); history.Accepted(100,key);
    CHECK(!history.NeedsReset(101,key,false));
    CHECK(history.NeedsReset(102,key,false));
    CHECK(history.NeedsReset(101,key,true));
    auto changed=key; ++changed.width;
    CHECK(history.NeedsReset(101,changed,false));
    changed=key; ++changed.deviceEpoch;
    CHECK(history.NeedsReset(101,changed,false));
    changed=key; ++changed.temporalEpoch;
    CHECK(history.NeedsReset(101,changed,false));
    changed=key; changed.config.mode=Mode::Dynamic;
    CHECK(history.NeedsReset(101,changed,false));
    history.Accepted(UINT64_MAX,key);
    CHECK(history.NeedsReset(0,key,false));
    PresentStatistics stats;
    stats.RawPresent(true); CHECK(stats.actualPresents==1); CHECK(!stats.contiguous);
    stats.RawPresent(false); CHECK(stats.actualPresents==1);
    stats.Observe(true,true,true,2); CHECK(stats.generatedIntervals==0);
    stats.Observe(true,true,true,2); CHECK(stats.generatedIntervals==1);
    stats.Observe(false,true,true,0); stats.Observe(true,true,true,4);
    CHECK(stats.generatedIntervals==1); CHECK(stats.actualPresents==9);
    stats.Observe(true,true,true,3); CHECK(stats.generatedIntervals==2);
    stats.Observe(true,false,true,3); CHECK(stats.generatedIntervals==2);
    stats.RawPresent(true); CHECK(stats.actualPresents==13);
    stats.Observe(true,true,true,2); CHECK(stats.generatedIntervals==2);
    std::printf("PASS %u reusable FG core checks\n",checks);
}
