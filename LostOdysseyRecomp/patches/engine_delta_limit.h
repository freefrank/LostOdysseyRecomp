#pragma once
#include <cstdlib>

// Longest frame delta the engine tick is given.
//
// A long stall in the frame loop (a scene load that finishes, a first-use
// shader compile, a machine that cannot keep its frame rate) reaches the game
// as one engine tick with a delta of 0.4 to 1 second. The Experimental Staff
// lifts (Operation Division to Upper Division, Disc 2) do not survive that: the
// player arrives at the bottom of the shaft, the scripted arrival does not
// carry them up, and they fall out of the world for good (the black screen of
// issue #17). Measured on the Switch, the first ticks after the arrival had
// deltas of 696, 453 and 1000 ms and the ride failed on every try; with the
// delta cut to 34 ms or to 250 ms the same ride worked on both lifts. The
// official macOS build reproduces it when its frames are slowed down, and
// rides fine at a steady 30 FPS. Which script step needs the short ticks has
// not been identified.
//
// The delta is in seconds, as the engine receives it in f1. A longer delta is
// cut to the limit: game time slips by the excess instead of the game running
// the whole stall in one step. LO_MAX_ENGINE_DELTA_MS changes the limit; 0 turns
// it off.
namespace engine_delta
{
inline constexpr double kDefaultMaxSeconds = 0.25;

// The limit in seconds; 0 means none. A negative, non-numeric or absent
// value selects the default.
inline double ParseMaxSeconds(const char* text)
{
    if (!text) return kDefaultMaxSeconds;
    char* end = nullptr;
    const double milliseconds = std::strtod(text, &end);
    if (end == text || *end != '\0' || !(milliseconds >= 0.0) || milliseconds > 3600000.0) return kDefaultMaxSeconds;
    return milliseconds / 1000.0;
}

inline double MaxSeconds()
{
    static const double value = ParseMaxSeconds(std::getenv("LO_MAX_ENGINE_DELTA_MS"));
    return value;
}

// NaN and negative deltas pass through unchanged: they are not stalls.
constexpr double Limit(double delta, double maxSeconds)
{
    return maxSeconds > 0.0 && delta > maxSeconds ? maxSeconds : delta;
}
}
