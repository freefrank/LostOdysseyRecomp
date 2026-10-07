#pragma once

// XAudio render driver emulation. The game registers a callback that the
// driver invokes whenever it wants another frame of 256 samples x 6 channels
// (48 kHz, 32-bit float, planar); the game answers with
// XAudioSubmitRenderDriverFrame. Convert big-endian planar floats to stereo
// (or interleaved 5.1) PCM and queue them to SDL; bounded device buffering
// paces the callback.

#define XAUDIO_SAMPLES_HZ 48000
#define XAUDIO_NUM_CHANNELS 6
#define XAUDIO_NUM_SAMPLES 256

namespace apu
{
    // surround: pass the guest's 5.1 channels through when the output device
    // has at least six; otherwise the stereo downmix is used.
    void Init(bool surround);
    // Live output change; the driver thread reopens the device between frames.
    void SetSurround(bool surround);
    // Channels of the open device: 6 (5.1), 2 (stereo), or 0 without a device
    // or while a change is pending.
    uint32_t OutputChannels();
    void RegisterClient(uint32_t callback, uint32_t param);
    // Disables future calls and drains a call already acquired by the driver.
    // When called from that callback itself, it disables subsequent calls.
    void UnregisterClient();
    void SubmitFrame(const void* samples);
    void SetPaused(bool paused);
}
