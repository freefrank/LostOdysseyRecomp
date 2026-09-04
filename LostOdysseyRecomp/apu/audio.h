#pragma once

// XAudio render driver emulation. The game registers a callback that the
// driver invokes whenever it wants another frame of 256 samples x 6 channels
// (48 kHz, 32-bit float, planar); the game answers with
// XAudioSubmitRenderDriverFrame. Until the SDL audio path lands the frames
// are consumed and discarded, but the callback cadence keeps the game's
// audio thread alive.

#define XAUDIO_SAMPLES_HZ 48000
#define XAUDIO_NUM_CHANNELS 6
#define XAUDIO_NUM_SAMPLES 256

namespace apu
{
    void Init();
    void RegisterClient(uint32_t callback, uint32_t param);
    void UnregisterClient();
    void SubmitFrame(const void* samples);
}
