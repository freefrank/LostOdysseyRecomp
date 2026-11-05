package io.github.freefrank.lostodyssey;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Log;
import android.view.Display;
import android.view.HapticFeedbackConstants;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.net.Uri;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Dual-screen companion: a live, touchable panel for the second display of
 * dual-screen handhelds such as the AYN Thor. The panel is a WebView page from
 * assets/companion/ shown in a {@link Presentation}; it reads the game state the
 * native runtime samples (companion/companion.cpp) and holds gamepad buttons.
 *
 * <p>The Presentation window is never focusable, so controller and keyboard
 * events keep going to the game while the second screen is touched.
 */
final class DualScreenCompanion implements DisplayManager.DisplayListener {
    private static final String TAG = "LoCompanion";
    private static final String PREFS = "dual_screen_companion";
    /** Reserved WebView host: the page and the theme share one origin (fetch, localStorage). */
    private static final String HOST = "appassets.androidplatform.net";
    private static final String PAGE = "https://" + HOST + "/companion/index.html";
    /**
     * Theme built from the player's own game files by tools/companion_theme.py and copied to
     * Android/data/io.github.freefrank.lostodyssey/files/companion-theme/. The APK ships no game art.
     */
    static final String THEME_FOLDER = "companion-theme";
    /** Shared-storage folder for the game (custom folder) and the theme. */
    static final String SHARED_FOLDER = "LostOdyssey";
    static final String PREF_ENABLED = "enabled";

    static native String nativeState();
    static native void nativeButtons(int buttons, int leftTrigger, int rightTrigger);
    static native boolean nativeAction(String json);

    private final Activity activity;
    private final DisplayManager displays;
    private final Handler main = new Handler(Looper.getMainLooper());
    private CompanionPresentation presentation;
    private boolean started;

    DualScreenCompanion(Activity activity) {
        this.activity = activity;
        this.displays = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
    }

    static boolean isEnabled(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean(PREF_ENABLED, true);
    }

    static void setEnabled(Context context, boolean enabled) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putBoolean(PREF_ENABLED, enabled).apply();
    }

    /** Activity onStart: show the panel on the second display, if there is one. */
    void start() {
        if (started || displays == null) return;
        started = true;
        displays.registerDisplayListener(this, main);
        refresh();
    }

    /** Activity onStop: hide the panel and release every held button. */
    void stop() {
        if (!started) return;
        started = false;
        displays.unregisterDisplayListener(this);
        dismiss();
    }

    /** Releases held buttons, e.g. when the game loses focus. */
    void releaseButtons() {
        safeButtons(0, 0, 0);
    }

    private void refresh() {
        if (!started || activity.isFinishing() || !isEnabled(activity)) {
            dismiss();
            return;
        }
        if (presentation != null && presentation.isShowing()
                && presentation.getDisplay().isValid()) return;
        dismiss();
        for (Display display : candidates()) {
            CompanionPresentation candidate = new CompanionPresentation(activity, display);
            try {
                candidate.show();
                presentation = candidate;
                Log.i(TAG, "companion on display " + display.getDisplayId() + " (" + display.getName() + ")");
                return;
            } catch (WindowManager.InvalidDisplayException | IllegalStateException e) {
                Log.w(TAG, "display " + display.getDisplayId() + " refused the companion", e);
            }
        }
    }

    private void dismiss() {
        releaseButtons();
        if (presentation != null) {
            try {
                presentation.dismiss();
            } catch (RuntimeException ignored) {
                // The display may already be gone.
            }
            presentation = null;
        }
    }

    /**
     * Like Eden Duo's AuxPresentation: the union of presentation displays and all
     * displays (some handhelds list the second panel only under one of them),
     * physical panels before virtual or overlay ones.
     */
    @SuppressWarnings("deprecation") // getDefaultDisplay: minSdk 26
    private List<Display> candidates() {
        Set<Display> all = new LinkedHashSet<>();
        for (Display d : displays.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) all.add(d);
        for (Display d : displays.getDisplays()) all.add(d);
        List<Display> preferred = new ArrayList<>(), others = new ArrayList<>();
        int own = activity.getWindowManager().getDefaultDisplay().getDisplayId();
        for (Display d : all) {
            if (d.getDisplayId() == Display.DEFAULT_DISPLAY || d.getDisplayId() == own || !d.isValid()) continue;
            String name = String.valueOf(d.getName()).toLowerCase(Locale.ROOT);
            boolean overlay = name.contains("overlay") || name.contains("virtual") || name.contains("cast");
            boolean presentationFlag = (d.getFlags() & Display.FLAG_PRESENTATION) != 0;
            if (name.contains("screen-2") || name.contains("builtin") || name.contains("built-in")
                    || (!overlay && !presentationFlag)) preferred.add(d);
            else if (!overlay) others.add(d);
        }
        preferred.addAll(others);
        return preferred;
    }

    @Override public void onDisplayAdded(int displayId) { refresh(); }
    @Override public void onDisplayChanged(int displayId) { refresh(); }
    @Override public void onDisplayRemoved(int displayId) {
        if (presentation != null && presentation.getDisplay().getDisplayId() == displayId) dismiss();
        refresh();
    }

    /** /companion/theme/* from the theme folder, any other /companion/* from the APK assets. */
    private WebResourceResponse serve(Uri url) {
        if (url == null || !HOST.equals(url.getHost())) return null;
        List<String> parts = url.getPathSegments();
        if (parts.size() < 2 || !"companion".equals(parts.get(0))) return notFound();
        for (String part : parts) if (part.isEmpty() || part.equals("..") || part.contains("/")) return notFound();
        String name = parts.get(parts.size() - 1);
        try {
            InputStream data;
            if ("theme".equals(parts.get(1))) {
                if (parts.size() != 3) return notFound();
                File file = themeFile(name);
                if (file == null) return notFound();
                data = new FileInputStream(file);
            } else {
                data = activity.getAssets().open(TextUtils.join("/", parts));
            }
            return new WebResourceResponse(mime(name), mime(name).startsWith("text/") ? "utf-8" : null, data);
        } catch (IOException e) {
            return notFound();
        }
    }

    /**
     * The theme in the app folder, else in LostOdyssey/companion-theme on the shared storage
     * (beside a custom game folder; readable with the game's "All files access"). Some
     * devices do not let the app read files copied into Android/data over USB.
     */
    @SuppressWarnings("deprecation") // getExternalStorageDirectory: plain shared-storage path
    private File themeFile(String name) {
        File own = activity.getExternalFilesDir(null);
        File[] folders = {
            own != null ? new File(own, THEME_FOLDER) : null,
            new File(Environment.getExternalStorageDirectory(), SHARED_FOLDER + "/" + THEME_FOLDER),
        };
        for (File folder : folders) {
            if (folder == null) continue;
            File file = new File(folder, name);
            if (file.isFile() && file.canRead()) return file;
        }
        return null;
    }

    private static WebResourceResponse notFound() {
        WebResourceResponse response = new WebResourceResponse("text/plain", "utf-8", null);
        response.setStatusCodeAndReasonPhrase(404, "Not Found");
        return response;
    }

    private static String mime(String name) {
        if (name.endsWith(".html")) return "text/html";
        if (name.endsWith(".css")) return "text/css";
        if (name.endsWith(".js")) return "text/javascript";
        if (name.endsWith(".json")) return "application/json";
        if (name.endsWith(".png")) return "image/png";
        return "application/octet-stream";
    }

    private static void safeButtons(int buttons, int lt, int rt) {
        try {
            nativeButtons(buttons, lt, rt);
        } catch (UnsatisfiedLinkError ignored) {
            // libmain not loaded (broken libraries): nothing to release.
        }
    }

    /** The second-screen window: one WebView with the companion page. */
    private final class CompanionPresentation extends Presentation {
        private WebView web;

        CompanionPresentation(Activity owner, Display display) {
            super(owner, display);
        }

        @SuppressLint({"SetJavaScriptEnabled", "AddJavascriptInterface"})
        @SuppressWarnings("deprecation") // immersive flags: minSdk 26
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            Window window = getWindow();
            if (window != null) {
                // Touch only: key and controller focus stays with the game.
                window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                    | WindowManager.LayoutParams.FLAG_FULLSCREEN);
                window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
                window.setBackgroundDrawable(null);
            }
            web = new WebView(getContext());
            web.setBackgroundColor(Color.BLACK);
            web.setHapticFeedbackEnabled(true);
            web.setOverScrollMode(View.OVER_SCROLL_NEVER);
            WebSettings settings = web.getSettings();
            settings.setJavaScriptEnabled(true);
            settings.setDomStorageEnabled(true);
            settings.setAllowFileAccess(false);
            settings.setAllowContentAccess(false);
            settings.setMediaPlaybackRequiresUserGesture(true);
            settings.setTextZoom(100);
            web.setWebViewClient(new WebViewClient() {
                @Override
                public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
                    return serve(request.getUrl());
                }

                @Override
                public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                    return true; // the panel never navigates away
                }
            });
            web.setWebChromeClient(new WebChromeClient());
            web.addJavascriptInterface(new Bridge(), "LO");
            web.loadUrl(PAGE);
            setContentView(web);
        }

        @Override
        protected void onStop() {
            safeButtons(0, 0, 0);
            if (web != null) {
                web.removeJavascriptInterface("LO");
                web.destroy();
                web = null;
            }
            super.onStop();
        }

        /** Called from the page's JavaScript thread. */
        private final class Bridge {
            @JavascriptInterface
            public String state() {
                try {
                    return nativeState();
                } catch (UnsatisfiedLinkError e) {
                    return "{}";
                }
            }

            /** XInput button mask (0xF3FF) and trigger values held on the panel. */
            @JavascriptInterface
            public void buttons(int mask, int leftTrigger, int rightTrigger) {
                safeButtons(mask, leftTrigger, rightTrigger);
            }

            @JavascriptInterface
            public boolean action(String json) {
                try {
                    return nativeAction(json);
                } catch (UnsatisfiedLinkError e) {
                    return false;
                }
            }

            @JavascriptInterface
            public void haptic() {
                main.post(() -> {
                    if (web != null) web.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                });
            }

            @JavascriptInterface
            public String language() {
                return Locale.getDefault().toLanguageTag();
            }

            /** Turns the panel off until it is enabled again from the CTRL dialog. */
            @JavascriptInterface
            public void disable() {
                main.post(() -> {
                    setEnabled(activity, false);
                    refresh();
                });
            }
        }
    }
}
