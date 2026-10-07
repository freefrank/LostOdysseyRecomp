package io.github.freefrank.lostodyssey;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Window;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import java.io.File;
import org.libsdl.app.SDLActivity;

/** Experimental game runtime. User-supplied discs live in app-owned storage. */
public final class RuntimeActivity extends SDLActivity {
    /** Starts the native importer (into {@link GameStorage#importRoot}) instead of the game. */
    static final String EXTRA_IMPORT = "import";
    /** A start that stays in the foreground this long counts as reached for the driver choice. */
    private static final long BOOT_SETTLED_MS = 15000;

    private TouchControlsView touchControls;
    /** Why the renderer could not start with the selected driver; set from native code. */
    private volatile String graphicsFailure;
    private final Handler bootHandler = new Handler(Looper.getMainLooper());
    private final Runnable bootSettled = () -> {
        GpuDriverStore.clearBootPending(this);
        PlayerLogs.publish(this);
    };

    static native void nativeSetTouchInput(int buttons, int leftTrigger, int rightTrigger,
                                           int leftX, int leftY, int rightX, int rightY);
    static native boolean nativeHasConnectedController();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        PlayerLogs.installCrashHandler(this);
        super.onCreate(savedInstanceState);
        hideSystemBars();
        // SDLActivity.onCreate posts setWindowStyle(false), which shows the bars
        // again once this method returns; hide after that command has run, and
        // whenever the bars come back later (SDL window-style changes, a swipe).
        bootHandler.post(this::hideSystemBars);
        ViewCompat.setOnApplyWindowInsetsListener(getWindow().getDecorView(), (view, insets) -> {
            if (insets.isVisible(WindowInsetsCompat.Type.systemBars())) scheduleHideSystemBars();
            return insets;
        });
        GameStorage.prepare(this);
        GpuDriverStore.markBootPending(this);
        if (mLayout != null && !mBrokenLibraries) {
            touchControls = new TouchControlsView(this);
            mLayout.addView(touchControls, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT));
            nativeSetTouchInput(0, 0, 0, 0, 0, 0, 0);
        }
    }

    /**
     * Called by the native runtime before its main returns when no Vulkan
     * device the renderer can use was created (video.cpp).
     */
    @SuppressWarnings("unused")
    void reportGraphicsFailure(String reason) {
        graphicsFailure = reason;
    }

    /** Opens the GPU driver page over the game (CTRL dialog). */
    void openGpuDriverPage() {
        Intent intent = new Intent(this, GpuDriverActivity.class);
        intent.putExtra(GpuDriverActivity.EXTRA_FROM_GAME, true);
        startActivity(intent);
    }

    /** Opens the game folder page over the game (CTRL dialog). */
    void openGameFolderPage() {
        Intent intent = new Intent(this, GameFolderActivity.class);
        intent.putExtra(GameFolderActivity.EXTRA_FROM_GAME, true);
        startActivity(intent);
    }

    /** Opens the saves page (export and import) over the game (CTRL dialog). */
    void openSavesPage() {
        Intent intent = new Intent(this, SaveTransferActivity.class);
        intent.putExtra(SaveTransferActivity.EXTRA_FROM_GAME, true);
        startActivity(intent);
    }

    @Override
    protected void onPause() {
        if (touchControls != null) touchControls.onHostPause();
        // Leaving the game is when a player connects USB to fetch the log.
        PlayerLogs.publish(this);
        super.onPause();
    }

    /**
     * The game always fills the screen (#199): status and navigation bars stay
     * hidden (a swipe shows them briefly) and the picture extends under the
     * display cutout. SDL only does this when the native side switches to a
     * fullscreen window mode, and the default display mode never does.
     */
    private void hideSystemBars() {
        Window window = getWindow();
        if (window == null) return;
        WindowCompat.setDecorFitsSystemWindows(window, false);
        if (Build.VERSION.SDK_INT >= 28 /* Android 9 (P) */) {
            WindowManager.LayoutParams attributes = window.getAttributes();
            int cutoutMode = Build.VERSION.SDK_INT >= 30 /* Android 11 (R) */
                ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            if (attributes.layoutInDisplayCutoutMode != cutoutMode) {
                attributes.layoutInDisplayCutoutMode = cutoutMode;
                window.setAttributes(attributes);
            }
        }
        WindowInsetsControllerCompat controller =
            WindowCompat.getInsetsController(window, window.getDecorView());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        controller.hide(WindowInsetsCompat.Type.systemBars());
    }

    /** Bars shown by a swipe stay for a moment, like Android's own transient bars. */
    private static final long HIDE_BARS_AFTER_MS = 2500;
    private final Runnable hideSystemBarsRunnable = this::hideSystemBars;

    private void scheduleHideSystemBars() {
        bootHandler.removeCallbacks(hideSystemBarsRunnable);
        bootHandler.postDelayed(hideSystemBarsRunnable, HIDE_BARS_AFTER_MS);
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemBars();
        if (touchControls != null) touchControls.onHostResume();
        bootHandler.removeCallbacks(bootSettled);
        bootHandler.postDelayed(bootSettled, BOOT_SETTLED_MS);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (!hasFocus && touchControls != null) touchControls.clearTouches();
        super.onWindowFocusChanged(hasFocus);
        // Dialogs and the pages opened over the game can bring the bars back.
        if (hasFocus) hideSystemBars();
    }

    @Override
    protected void onDestroy() {
        if (touchControls != null) touchControls.clearTouches();
        bootHandler.removeCallbacks(bootSettled);
        bootHandler.removeCallbacks(hideSystemBarsRunnable);
        // A normal exit is not a failed start, however short it was.
        GpuDriverStore.clearBootPending(this);
        super.onDestroy();
        // A cancelled or failed import returns from the native main instead of
        // starting the game. Go back to the game folder page in a new process,
        // since the native runtime runs only once per process.
        if (isFinishing() && getIntent().getBooleanExtra(EXTRA_IMPORT, false)) {
            Intent intent = new Intent(this, GameFolderActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            startActivity(intent);
            Runtime.getRuntime().exit(0);
        }
        // The same driver would fail again on the next start, and the launcher
        // goes straight to the game once a driver was chosen: show the GPU
        // driver page with the reason instead (#185). Devices without custom
        // drivers get the reason in a dialog.
        if (isFinishing() && graphicsFailure != null) {
            Intent intent = new Intent(this, GpuDriverActivity.class);
            intent.putExtra(GpuDriverActivity.EXTRA_GRAPHICS_FAILURE, graphicsFailure);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            startActivity(intent);
            Runtime.getRuntime().exit(0);
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL3", "main" };
    }

    @Override
    protected String[] getArguments() {
        boolean importing = getIntent().getBooleanExtra(EXTRA_IMPORT, false);
        File game = importing ? GameStorage.importRoot(this) : GameStorage.disc1(this);
        // Custom Vulkan driver (libadrenotools): the hook libraries sit in the
        // extracted native library directory, the chosen package in its own folder.
        nativeSetenv("LO_NATIVE_LIB_DIR", getApplicationInfo().nativeLibraryDir + "/");
        // Logs go where a PC can copy them over USB; the device line names the phone.
        nativeSetenv("LO_LOG_DIR", PlayerLogs.directory(this).getAbsolutePath());
        nativeSetenv("LO_ANDROID_DEVICE", PlayerLogs.deviceDescription());
        // Mesa drivers (Turnip) keep compiled pipelines on disk only with a cache
        // directory; Android gives them none. Other drivers ignore these.
        nativeSetenv("MESA_SHADER_CACHE_DIR", new File(getCacheDir(), "mesa_shader_cache").getAbsolutePath());
        nativeSetenv("MESA_SHADER_CACHE_MAX_SIZE", "256M");
        GpuDriverStore.Installed driver = GpuDriverStore.selectedDriver(this);
        if (driver != null) {
            nativeSetenv("LO_CUSTOM_DRIVER_DIR", driver.directory.getAbsolutePath() + "/");
            nativeSetenv("LO_VK_CUSTOM_DRIVER", driver.metadata.libraryName);
        }
        // Debug launches can select an isolated fixture without changing saves.
        if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
            String override = getIntent().getStringExtra("game_root");
            if (override != null && !override.isEmpty()) game = new File(override);
            // Expose the existing renderer diagnostics to ADB on development APKs.
            // A fresh process is required when changing these native switches.
            // Every LO_* extra is forwarded: `am start --es LO_DEBUG_CAPTURE_SWAP 3000`.
            Bundle extras = getIntent().getExtras();
            if (extras != null) {
                for (String name : extras.keySet()) {
                    if (!name.startsWith("LO_")) continue;
                    Object value = extras.get(name);
                    if (value != null) nativeSetenv(name, String.valueOf(value));
                }
            }
        }
        if (importing) {
            //noinspection ResultOfMethodCallIgnored
            game.mkdirs();
            return new String[] { "--game", game.getAbsolutePath(), "--install", "--quiet-kernel" };
        }
        return new String[] { "--game", game.getAbsolutePath(), "--quiet-kernel" };
    }
}
