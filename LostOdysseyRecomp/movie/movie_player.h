#pragma once
#include "movie_decoder.h"
#include <cstdint>
#include <memory>
#include <string_view>

// Cutscene replacement sessions. The guest keeps decoding, timing and drawing
// its own WMV (audio, subtitles, skip and end are unchanged); a session decodes
// the mod's MP4 and supplies the frame the renderer draws over the guest quad.
//
// Threads: StartReplacement on the game thread; StopReplacement and the clock
// calls on the guest render thread; FrameForDraw on the command processor thread.
namespace movie {
// The native WMV lifecycle, whether or not a mod provides replacement video.
// Read-only from any thread; contains no movie filename or scenario identifier.
#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)
bool GuestPlaying();
#endif
// guestMovie is the FLO_MOVIE object, name the video's file name stem (mv_00v).
// Resolves the mod asset and opens it on a decode thread; no-op without one.
void StartReplacement(uint32_t guestMovie, std::string_view name);
// After the guest's last draw of the movie.
void StopReplacement(uint32_t guestMovie);
// Non-zero while guestMovie has a replacement that has not failed.
uint32_t ReplacementSession(uint32_t guestMovie);

// Guest frame clock, render thread. Times are steady-clock milliseconds.
// False when the guest update can be skipped this time: its next frame is not
// due, and calling it would sleep the render thread until it is.
bool GuestUpdateDue(uint32_t session, double nowMs);
// The guest update returned a new frame with this presentation time.
void GuestFrameShown(uint32_t session, double ptsMs, double nowMs);
// The movie time to show now, in microseconds.
int64_t ReplacementTimeUs(uint32_t session, double nowMs);

struct DrawFrame {
    std::shared_ptr<const Nv12Frame> frame;
    uint64_t serial = 0; // unique per decoded picture
};
// The newest decoded frame at or before timeUs (the first frame before it).
// False before the first frame, after the stream ends, or on failure: the
// guest's own frame stays visible.
bool FrameForDraw(uint32_t session, int64_t timeUs, DrawFrame& out);

double NowMs();
}
