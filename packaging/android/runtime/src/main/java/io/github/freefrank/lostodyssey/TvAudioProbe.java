package io.github.freefrank.lostodyssey;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.Looper;

/** Manual audible probe. Its route is evidence for THIS track, never for SDL's stream. */
final class TvAudioProbe {
    private static boolean playing;
    private TvAudioProbe() {}
    static void play(Context context, TvDisplayDiagnostics diagnostics) {
        if (playing) return;
        final AudioTrack track;
        AudioTrack created = null;
        try {
            short[] pcm = new short[48000 * 2];
            for (int i = 0; i < 48000; ++i) {
                int channel = i < 24000 ? 0 : 1;
                pcm[i * 2 + channel] = (short)(6000 * Math.sin(2 * Math.PI * (channel == 0 ? 440 : 660) * i / 48000));
            }
            created = new AudioTrack.Builder()
                .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                .setAudioFormat(new AudioFormat.Builder().setSampleRate(48000)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).setEncoding(AudioFormat.ENCODING_PCM_16BIT).build())
                .setTransferMode(AudioTrack.MODE_STATIC).setBufferSizeInBytes(pcm.length * 2).build();
            AudioManager manager = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
            if (manager != null) for (AudioDeviceInfo device : manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
                if (device.getType() == AudioDeviceInfo.TYPE_HDMI || device.getType() == AudioDeviceInfo.TYPE_HDMI_ARC
                        || device.getType() == AudioDeviceInfo.TYPE_HDMI_EARC) {
                    diagnostics.event("audio-probe preferred HDMI id=" + device.getId()
                        + " accepted=" + created.setPreferredDevice(device));
                    break;
                }
            }
            if (created.write(pcm, 0, pcm.length) != pcm.length)
                throw new IllegalStateException("Incomplete probe audio buffer");
            created.play();
            track = created;
            playing = true;
        } catch (IllegalArgumentException | IllegalStateException | UnsupportedOperationException e) {
            if (created != null) created.release();
            diagnostics.event("audio-probe failed: " + e);
            return;
        }
        Handler handler = new Handler(Looper.getMainLooper());
        handler.postDelayed(() -> {
            AudioDeviceInfo route = track.getRoutedDevice();
            diagnostics.event("audio-probe actual-route=" + (route == null ? "unknown"
                : "id=" + route.getId() + " type=" + route.getType() + " name=" + route.getProductName())
                + "; SDL game route must be checked separately");
        }, 250);
        handler.postDelayed(() -> {
            track.release();
            playing = false;
        }, 1250);
    }
}
