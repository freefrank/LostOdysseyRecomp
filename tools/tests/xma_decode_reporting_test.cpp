#include "apu/xma_decode_reporting.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
unsigned checks = 0;

void Require(bool condition, const char* message)
{
    ++checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

using apu::xma::DecodeFailureReports;
using apu::xma::DecodeFailureStage;
using namespace std::chrono_literals;

void InitialBudgetAndLateRecurrence()
{
    DecodeFailureReports reports;
    const auto start = DecodeFailureReports::Clock::time_point{};
    for (uint64_t i = 1; i <= 16; ++i)
    {
        const auto r = reports.Observe(start);
        Require(r.ordinal == i, "monotonic failure ordinal");
        Require(r.emit && r.capture, "first 16 retain detailed report/capture eligibility");
        Require(r.suppressed == 0, "initial reports suppress nothing");
    }

    // This is the log-observability failure in Issue #55's evidence pattern:
    // the original process-wide counter cannot report a later occurrence.
    unsigned oldErrors = 0;
    for (unsigned i = 0; i < 16; ++i) (void)(oldErrors++ < 16);
    Require(!(oldErrors++ < 16), "old implementation hides failure 17 forever");

    const auto late = reports.Observe(start + 27h);
    Require(late.ordinal == 17, "late occurrence retains process-wide ordinal");
    Require(late.emit, "late occurrence after the initial 16 is reported");
    Require(!late.capture, "late warnings cannot expand the private capture budget");
    Require(late.suppressed == 0, "no invented suppressed events");
}

void BurstLimitsAndSuppressionSummary()
{
    DecodeFailureReports reports;
    const auto start = DecodeFailureReports::Clock::time_point{};
    for (unsigned i = 0; i < 16; ++i) (void)reports.Observe(start);
    for (unsigned i = 0; i < 3; ++i)
    {
        const auto r = reports.Observe(start + 1s);
        Require(!r.emit && !r.capture, "post-budget burst is bounded");
    }
    const auto early = reports.Observe(start + 10s - 1ns);
    Require(!early.emit, "interval does not reopen early");
    const auto boundary = reports.Observe(start + 10s);
    Require(boundary.emit, "interval reopens at exactly ten seconds");
    Require(boundary.suppressed == 4, "summary counts every suppressed observation");
    Require(boundary.ordinal == 21, "reported total includes suppressed observations");
    const auto next = reports.Observe(start + 20s);
    Require(next.emit && next.suppressed == 0, "suppressed count resets only after reporting");
}

void NoCaptureExpansionAcrossLongSessions()
{
    DecodeFailureReports reports;
    const auto start = DecodeFailureReports::Clock::time_point{};
    unsigned captures = 0;
    unsigned emissions = 0;
    for (unsigned i = 0; i < 1000; ++i)
    {
        const auto r = reports.Observe(start + std::chrono::seconds(i * 10));
        captures += r.capture;
        emissions += r.emit;
    }
    Require(captures == 16, "packet capture eligibility stays limited to 16");
    Require(emissions == 1000, "widely spaced late failures remain visible");
}

void StagesDoNotConfuseBackpressureWithInputDemand()
{
    Require(std::string_view(DecodeFailureStage(-11, -11)) == "send", "send EAGAIN is identified as send");
    Require(std::string_view(DecodeFailureStage(0, -11)) == "receive", "receive EAGAIN is identified as receive");
    Require(std::string_view(DecodeFailureStage(-22, -22)) == "send", "other send failure");
    Require(std::string_view(DecodeFailureStage(0, -22)) == "receive", "other receive failure");
    Require(std::string_view(DecodeFailureStage(0, 0)) == "frame-layout", "successful receive with unexpected frame layout");
}
}

int main()
{
    InitialBudgetAndLateRecurrence();
    BurstLimitsAndSuppressionSummary();
    NoCaptureExpansionAcrossLongSessions();
    StagesDoNotConfuseBackpressureWithInputDemand();
    std::cout << "PASS: " << checks << " checks; diagnostic reporting only, no game/audio playback validation\n";
}
