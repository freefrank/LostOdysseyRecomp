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
    // Output layouts, numbered like settings::AudioOutput*. Surround passes
    // the guest's 5.1 channels through when the output device mixes at least
    // six, otherwise the stereo downmix is used; Matrix encodes 5.1 into
    // stereo for a receiver's surround decoding (see matrix_surround.h).
    enum class Output : uint32_t { Stereo, Surround, Matrix };
    void Init(Output output);
    // Live output change; a change to or from Surround reopens the device on
    // the driver thread between frames.
    void SetOutput(Output output);
    // Matrix surround: phase of the surrounds against the fronts, 0-180
    // degrees (90 is Pro Logic II). Applied from the next frame.
    void SetMatrixPhase(uint32_t degrees);
    // Speaker test noise (test_signal.h) in place of the game's sound. Call
    // it every frame while the test should play; it stops by itself 0.3 s
    // after the last call with true.
    void SetTestSignal(bool on);
    // Where the speaker test sound is along its circle (SpeakerPan::Position:
    // 0 front left ... 4 left surround, fractions while gliding), or a
    // negative value while no test plays.
    float TestSignalPosition();
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
