package io.github.freefrank.lostodyssey;

import android.app.Activity;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.hardware.input.InputManager;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.SurfaceHolder;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Public-API evidence. A presentation display is not proof of a wired HDMI connection. */
final class TvDisplayDiagnostics implements DisplayManager.DisplayListener, InputManager.InputDeviceListener {
    static final String FILE_NAME = "tv-display.log";
    private final Activity activity;
    private final File file;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final DisplayManager displays;
    private final AudioManager audio;
    private final InputManager input;
    private boolean started;
    private final AudioDeviceCallback audioCallback = new AudioDeviceCallback() {
        @Override public void onAudioDevicesAdded(AudioDeviceInfo[] added) { snapshot("audio-added"); }
        @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] removed) { snapshot("audio-removed"); }
    };

    TvDisplayDiagnostics(Activity activity, File directory) {
        this.activity = activity;
        file = new File(directory, FILE_NAME);
        displays = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        audio = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);
        input = (InputManager) activity.getSystemService(Context.INPUT_SERVICE);
    }

    void start() {
        if (started) return;
        started = true;
        if (displays != null) displays.registerDisplayListener(this, handler);
        if (input != null) input.registerInputDeviceListener(this, handler);
        if (audio != null) audio.registerAudioDeviceCallback(audioCallback, handler);
        event("session manufacturer=" + Build.MANUFACTURER + " model=" + Build.MODEL
            + " sdk=" + Build.VERSION.SDK_INT + " physical-panel-off=unsupported-by-this-app");
        snapshot("start");
    }

    void stop() {
        if (!started) return;
        started = false;
        if (displays != null) displays.unregisterDisplayListener(this);
        if (input != null) input.unregisterInputDeviceListener(this);
        if (audio != null) audio.unregisterAudioDeviceCallback(audioCallback);
        event("session stopped");
    }

    synchronized void event(String message) {
        Log.i("LOTV", message);
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) return;
        // Bounded, directly exportable diagnostic file. Never log every rendered frame.
        boolean append = file.length() < 1024 * 1024;
        try (PrintWriter out = new PrintWriter(new OutputStreamWriter(
                new FileOutputStream(file, append), StandardCharsets.UTF_8))) {
            out.println(System.currentTimeMillis() + " " + message);
        } catch (java.io.IOException e) {
            Log.w("LOTV", "Cannot write TV diagnostics", e);
        }
    }

    void surface(String reason, SurfaceHolder holder, int width, int height, Display display) {
        event("surface " + reason + " display=" + (display == null ? -1 : display.getDisplayId())
            + " callback=" + width + "x" + height + " frame=" + holder.getSurfaceFrame()
            + " valid=" + holder.getSurface().isValid());
    }

    void snapshot(String reason) {
        event("snapshot " + reason + " activityDisplay="
            + activity.getWindowManager().getDefaultDisplay().getDisplayId());
        if (displays != null) for (Display d : displays.getDisplays()) {
            event("display id=" + d.getDisplayId() + " name=" + d.getName() + " flags=" + d.getFlags()
                + " state=" + d.getState() + " valid=" + d.isValid() + " current=" + d.getMode()
                + " supported=" + Arrays.toString(d.getSupportedModes()));
        }
        // Available outputs only. This does NOT establish the route of SDL's audio stream.
        if (audio != null) for (AudioDeviceInfo d : audio.getDevices(AudioManager.GET_DEVICES_OUTPUTS))
            event("audio-available id=" + d.getId() + " type=" + d.getType() + " name=" + d.getProductName()
                + " channels=" + Arrays.toString(d.getChannelCounts()));
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d != null && !d.isVirtual() && (d.supportsSource(InputDevice.SOURCE_GAMEPAD)
                    || d.supportsSource(InputDevice.SOURCE_JOYSTICK)))
                event("controller id=" + id + " name=" + d.getName() + " sources=" + d.getSources());
        }
    }

    @Override public void onDisplayAdded(int id) { snapshot("display-added:" + id); }
    @Override public void onDisplayChanged(int id) { snapshot("display-changed:" + id); }
    @Override public void onDisplayRemoved(int id) { snapshot("display-removed:" + id); }
    @Override public void onInputDeviceAdded(int id) { snapshot("input-added:" + id); }
    @Override public void onInputDeviceChanged(int id) { snapshot("input-changed:" + id); }
    @Override public void onInputDeviceRemoved(int id) { snapshot("input-removed:" + id); }
}
