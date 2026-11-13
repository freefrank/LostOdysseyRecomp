#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

// Press-to-skip input latch for the battle intro. While armed, a new press of the
// game's cancel button fires once, and the guest never sees that press: the bit stays
// hidden until it has been up for ReleaseTime. A press inside that time restarts it.
// Nothing else ends the latch (no timer alone, no disarm, no focus loss), so a held
// button can never reach the first command menu as a fresh press.
// Sample and Hidden run under the caller's input lock; the other calls are atomic.
namespace hid::skip_latch
{
using Clock = std::chrono::steady_clock;
constexpr auto ReleaseTime = std::chrono::milliseconds(250);

class Latch
{
public:
    // Only while armed can a new press fire. A new arm needs a sample with the
    // button up first, and drops a press that nobody took.
    void SetArmed(bool armed)
    {
        if (!armed)
        {
            armed_.store(false);
            return;
        }
        if (armed_.load()) return;
        pressed_.store(false);
        rearmed_.store(true);
        armed_.store(true);
    }
    bool Armed() const { return armed_.load(); }

    // True once for each fired press.
    bool TakePress() { return pressed_.exchange(false); }

    // An input source was reset (focus loss, pad connect or disconnect, touch clear).
    // A release pause that runs starts again at the next sample; nothing ends early.
    void Restart() { restart_.store(true); }

    // False: Sample would change nothing (not armed and nothing hidden).
    bool Active() const { return hidden_ || armed_.load(); }
    bool Hidden() const { return hidden_; }
    uint16_t Bit() const { return bit_; }

    // One guest read. raw: every source merged, before any filter. buttons: the value
    // that the guest gets, after the filters; the latch removes its bit in place.
    // cancelBit: the guest's cancel bit now, used only when no press is hidden.
    // Returns true when this sample fired.
    bool Sample(uint16_t raw, uint16_t& buttons, uint16_t cancelBit, Clock::time_point now)
    {
        // armed_ first: an arm between the two reads then still clears ready_.
        const bool armed = armed_.load();
        if (rearmed_.exchange(false)) ready_ = false;
        if (restart_.exchange(false) && hidden_ && released_) releasedAt_ = now;
        bool fired = false;
        if (!hidden_)
        {
            if (!armed) return false;
            if (cancelBit != bit_)
            {
                bit_ = cancelBit;
                ready_ = false;
            }
            if (!(raw & bit_))
            {
                ready_ = true;
                return false;
            }
            // A press that a host menu took, or a button down since the arm or since
            // the last fire, never fires.
            if (!(buttons & bit_) || !ready_)
            {
                ready_ = false;
                return false;
            }
            hidden_ = true;
            released_ = false;
            ready_ = false;
            fired = true;
            pressed_.store(true);
        }
        else if (raw & bit_)
            released_ = false;
        else if (!released_)
        {
            released_ = true;
            releasedAt_ = now;
        }
        else if (now - releasedAt_ >= ReleaseTime)
        {
            // This sample has the bit up, so it also counts as the release that a new fire needs.
            hidden_ = false;
            ready_ = true;
        }
        buttons &= uint16_t(~bit_);
        return fired;
    }

private:
    std::atomic<bool> armed_{false}, rearmed_{false}, pressed_{false}, restart_{false};
    bool hidden_ = false, released_ = false, ready_ = false;
    uint16_t bit_ = 0;
    Clock::time_point releasedAt_{};
};
}
