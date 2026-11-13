package io.github.freefrank.lostodyssey;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.Gravity;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;

/** Moves ONE SDL SurfaceView to a public presentation window, never clones the game. */
final class TvOutputController implements DisplayManager.DisplayListener {
    interface Listener { void outputChanged(boolean external, int height); }
    private final Activity activity;
    private final ViewGroup phone;
    private final SurfaceView surface;
    private final ViewGroup.LayoutParams phoneLayout;
    private final DisplayManager displays;
    private final TvDisplayDiagnostics diagnostics;
    private final Listener listener;
    final SharedPreferences preferences;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable update = this::reconcile;
    private Presentation presentation;
    private FrameLayout tvRoot;
    private int rejectedDisplay = Display.INVALID_DISPLAY, appliedHeight;
    private boolean started, resumed, changing;
    private String status = "";
    private View phoneBlackout;
    private float previousBrightness;
    private boolean dimDismissed;

    TvOutputController(Activity activity, ViewGroup phone, SurfaceView surface,
                       TvDisplayDiagnostics diagnostics, Listener listener) {
        this.activity = activity;
        this.phone = phone;
        this.surface = surface;
        this.phoneLayout = surface.getLayoutParams();
        this.diagnostics = diagnostics;
        this.listener = listener;
        displays = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        preferences = activity.getSharedPreferences("tv_output", Context.MODE_PRIVATE);
    }

    boolean active() { return presentation != null && presentation.isShowing(); }
    int mode() { return TvModePolicy.mode(preferences.getInt("mode", TvModePolicy.AUTOMATIC)); }
    int height() { return TvModePolicy.height(preferences.getInt("height", 1080)); }
    String status() { return status; }

    void start() {
        if (started) return;
        started = true;
        if (displays != null) displays.registerDisplayListener(this, handler);
    }
    void resume() { resumed = true; schedule(); }
    void pause() { resumed = false; restorePhoneBrightness(); }
    void stop() {
        started = false;
        resumed = false;
        handler.removeCallbacks(update);
        if (displays != null) displays.unregisterDisplayListener(this);
        leave();
    }
    void settingsChanged() { rejectedDisplay = Display.INVALID_DISPLAY; dimDismissed = false; schedule(); }
    boolean hideTouch() { return active() && preferences.getBoolean("hide_touch", true); }
    void wakePhone() { dimDismissed = true; restorePhoneBrightness(); }
    private void schedule() {
        handler.removeCallbacks(update);
        if (started) handler.post(update);
    }

    private Display candidate() {
        if (displays == null) return null;
        Display fallback = null;
        int host = activity.getWindowManager().getDefaultDisplay().getDisplayId();
        for (Display display : displays.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            Display.Mode current = display.getMode();
            if (!display.isValid() || display.getDisplayId() == host || display.getDisplayId() == rejectedDisplay
                    || !TvModePolicy.useDisplay(mode(), display.getDisplayId() != Display.DEFAULT_DISPLAY,
                        (display.getFlags() & Display.FLAG_PRESENTATION) != 0,
                        TvModePolicy.wide(current.getPhysicalWidth(), current.getPhysicalHeight()))) continue;
            // Keep the current display when another monitor is attached.
            if (presentation != null && presentation.getDisplay().getDisplayId() == display.getDisplayId()) return display;
            if (fallback == null) fallback = display;
        }
        return fallback;
    }

    private void reconcile() {
        if (!started || changing || activity.isFinishing() || activity.isDestroyed()) return;
        Display target = candidate();
        if (presentation != null && (target == null
                || target.getDisplayId() != presentation.getDisplay().getDisplayId())) leave();
        if (target == null) {
            status = activity.getString(rejectedDisplay != Display.INVALID_DISPLAY ? R.string.tv_status_rejected
                : mode() == TvModePolicy.ORIGINAL_MIRROR || mode() == TvModePolicy.OFF
                    ? R.string.tv_status_mirroring : R.string.tv_status_no_display);
            return;
        }
        if (presentation == null && resumed) enter(target);
        else if (presentation != null) configure(target);
    }

    private void enter(Display display) {
        changing = true;
        Presentation next = new Presentation(activity, display, android.R.style.Theme_Material_NoActionBar_Fullscreen);
        try {
            Window window = next.getWindow();
            if (window == null) throw new WindowManager.InvalidDisplayException("No presentation window");
            // The phone remains the SDL Activity and input focus owner. A focusable
            // Presentation would pause SDL and freeze its USB HID manager.
            window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            tvRoot = new FrameLayout(next.getContext());
            tvRoot.setBackgroundColor(Color.BLACK);
            next.setContentView(tvRoot);
            next.setOnDismissListener(dialog -> {
                if (presentation != next) return;
                rejectedDisplay = display.getDisplayId();
                leave();
                diagnostics.event("presentation dismissed by system; automatic retry suppressed");
            });
            // Validate before detaching the phone's surface. An OEM may reject show().
            next.show();
            presentation = next;
            dimDismissed = false;
            window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
            detach();
            appliedHeight = 0;
            configure(display);
            tvRoot.addView(surface, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT, Gravity.CENTER));
            tvRoot.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> fitSurface());
            tvRoot.post(this::fitSurface);
            diagnostics.event("presentation active display=" + display.getDisplayId() + " single-game-surface=true");
        } catch (WindowManager.InvalidDisplayException | SecurityException | IllegalArgumentException e) {
            rejectedDisplay = display.getDisplayId();
            if (presentation == next) leave();
            else next.dismiss();
            status = activity.getString(R.string.tv_status_rejected);
            diagnostics.event("presentation rejected: " + e);
        } finally {
            changing = false;
        }
    }

    private void configure(Display display) {
        if (presentation == null) return;
        int height = height();
        int preferred = 0;
        double best = Double.POSITIVE_INFINITY;
        for (Display.Mode mode : display.getSupportedModes()) {
            double score = TvModePolicy.modeScore(mode.getPhysicalWidth(), mode.getPhysicalHeight(), mode.getRefreshRate(), height);
            if (score < best) { best = score; preferred = mode.getModeId(); }
        }
        Window window = presentation.getWindow();
        if (window != null) {
            WindowManager.LayoutParams attributes = window.getAttributes();
            if (attributes.preferredDisplayModeId != preferred) {
                attributes.preferredDisplayModeId = preferred;
                window.setAttributes(attributes);
                diagnostics.event("preferred TV mode=" + preferred + " (request only; Android may ignore)");
            }
        }
        listener.outputChanged(true, height);
        if (appliedHeight != height) {
            appliedHeight = height;
            surface.getHolder().setFixedSize(TvModePolicy.width(height), height);
            diagnostics.event("TV render buffer requested=" + TvModePolicy.width(height) + "x" + height);
        }
        applyConsole();
        status = activity.getString(R.string.tv_status_active, TvModePolicy.width(height), height);
    }

    private void fitSurface() {
        if (!active() || tvRoot == null || surface.getParent() != tvRoot) return;
        int[] size = TvModePolicy.fit(tvRoot.getWidth(), tvRoot.getHeight());
        if (size[0] <= 0 || size[1] <= 0) return;
        ViewGroup.LayoutParams params = surface.getLayoutParams();
        if (params.width == size[0] && params.height == size[1]) return;
        surface.setLayoutParams(new FrameLayout.LayoutParams(size[0], size[1], Gravity.CENTER));
        diagnostics.event("TV view=" + size[0] + "x" + size[1] + " window=" + tvRoot.getWidth() + "x" + tvRoot.getHeight());
    }

    void surfaceReady() {
        if (active() && Build.VERSION.SDK_INT >= 30 && surface.getHolder().getSurface().isValid())
            surface.getHolder().getSurface().setFrameRate(60f, android.view.Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
    }

    private void detach() {
        if (surface.getParent() instanceof ViewGroup) ((ViewGroup)surface.getParent()).removeView(surface);
    }

    private void applyConsole() {
        if (!active() || !resumed || dimDismissed || !preferences.getBoolean("dim_phone", false)) {
            restorePhoneBrightness();
            return;
        }
        if (phoneBlackout != null) return;
        previousBrightness = activity.getWindow().getAttributes().screenBrightness;
        WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
        attributes.screenBrightness = 0f; // Public per-window minimum, NOT panel power-off.
        activity.getWindow().setAttributes(attributes);
        phoneBlackout = new View(activity);
        phoneBlackout.setBackgroundColor(Color.BLACK);
        phoneBlackout.setKeepScreenOn(true);
        phoneBlackout.setOnTouchListener((view, event) -> { wakePhone(); return true; });
        phone.addView(phoneBlackout, new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        diagnostics.event("phone software blackout active; physical-panel-off=false");
    }

    private void restorePhoneBrightness() {
        if (phoneBlackout == null) return;
        phone.removeView(phoneBlackout);
        phoneBlackout = null;
        WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
        attributes.screenBrightness = previousBrightness;
        activity.getWindow().setAttributes(attributes);
        diagnostics.event("phone brightness restored");
    }

    private void leave() {
        Presentation previous = presentation;
        restorePhoneBrightness();
        if (previous == null) return;
        presentation = null; // Dismiss callback must not trigger another migration.
        detach();
        listener.outputChanged(false, 0);
        surface.getHolder().setSizeFromLayout();
        phone.addView(surface, 0, phoneLayout);
        surface.requestFocus();
        tvRoot = null;
        appliedHeight = 0;
        previous.dismiss();
        diagnostics.event("phone surface and render preferences restored");
    }

    @Override public void onDisplayAdded(int id) { schedule(); }
    @Override public void onDisplayChanged(int id) { schedule(); }
    @Override public void onDisplayRemoved(int id) {
        if (rejectedDisplay == id) rejectedDisplay = Display.INVALID_DISPLAY;
        schedule();
    }
}
