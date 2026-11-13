package io.github.freefrank.lostodyssey;

import android.content.Intent;
import android.app.AlertDialog;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.Window;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

/** Game runtime. User-supplied discs live in app-owned storage. */
public final class RuntimeActivity extends SDLActivity {
    /** Matches the SDL_SendAndroidMessage command in settings/menu.cpp. */
    private static final int COMMAND_OPEN_GAME_FOLDER = 0x8000;
    private static final int COMMAND_OPEN_TV_SETTINGS = 0x8001;
    /** Starts the native importer (into {@link GameStorage#importRoot}) instead of the game. */
    static final String EXTRA_IMPORT = "import";
    /** Files and folders picked in the system file picker, which the importer scans at once. */
    static final String EXTRA_IMPORT_SOURCES = "import_sources";
    /** A start that stays in the foreground this long counts as reached for the driver choice. */
    private static final long BOOT_SETTLED_MS = 15000;

    private TouchControlsView touchControls;
    private TvDisplayDiagnostics tvDiagnostics;
    private TvOutputController tvOutput;
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
    static native void nativeSetTvOutput(boolean external, int height);

    /** SDLSurface caches its display. Refresh it after migration to the TV. */
    private final class RuntimeSurface extends SDLSurface {
        RuntimeSurface(Context context) { super(context); }
        @Override protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            if (getDisplay() != null) mDisplay = getDisplay();
        }
        @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            if (getDisplay() != null) mDisplay = getDisplay();
            super.surfaceChanged(holder, format, width, height);
            if (tvDiagnostics != null) tvDiagnostics.surface("changed", holder, width, height, getDisplay());
            if (tvOutput != null) tvOutput.surfaceReady();
        }
        @Override public void surfaceDestroyed(SurfaceHolder holder) {
            if (tvDiagnostics != null) tvDiagnostics.surface("destroyed", holder, 0, 0, getDisplay());
            super.surfaceDestroyed(holder);
        }
    }

    @Override protected SDLSurface createSDLSurface(Context context) { return new RuntimeSurface(context); }

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
            if (insets.isVisible(WindowInsetsCompat.Type.statusBars())
                    || insets.isVisible(WindowInsetsCompat.Type.navigationBars())) {
                if (touchControls != null) touchControls.revealCtrl();
                scheduleHideSystemBars();
            }
            return insets;
        });
        GameStorage.prepare(this);
        tvDiagnostics = new TvDisplayDiagnostics(this, PlayerLogs.directory(this));
        tvDiagnostics.start();
        GpuDriverStore.markBootPending(this);
        if (mLayout != null && !mBrokenLibraries) {
            touchControls = new TouchControlsView(this);
            mLayout.addView(touchControls, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT,
                RelativeLayout.LayoutParams.MATCH_PARENT));
            nativeSetTouchInput(0, 0, 0, 0, 0, 0, 0);
            if (!getIntent().getBooleanExtra(EXTRA_IMPORT, false)) {
                tvOutput = new TvOutputController(this, mLayout, mSurface, tvDiagnostics, (external, height) -> {
                    nativeSetTvOutput(external, height);
                    if (touchControls != null) touchControls.setTvControlsHidden(external && tvOutput.hideTouch());
                    if (external) {
                        mLayout.setFocusableInTouchMode(true);
                        mLayout.requestFocus();
                    }
                });
                tvOutput.start();
            }
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

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        InputDevice device = event.getDevice();
        if (tvOutput != null && tvOutput.active() && device != null
                && (device.supportsSource(InputDevice.SOURCE_GAMEPAD) || device.supportsSource(InputDevice.SOURCE_JOYSTICK))
                && SDLActivity.handleKeyEvent(mSurface, event.getKeyCode(), event, null)) return true;
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP
                && !event.isCanceled() && touchControls != null) touchControls.revealCtrl();
        // Keep SDL's existing Back mapping/consumption, including gesture-generated keys.
        return super.dispatchKeyEvent(event);
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (tvOutput != null && tvOutput.active() && event.isFromSource(InputDevice.SOURCE_JOYSTICK)
                && mSurface.dispatchGenericMotionEvent(event)) return true;
        return super.dispatchGenericMotionEvent(event);
    }

    void openTvSettings() {
        if (tvOutput != null) {
            tvOutput.wakePhone();
            TvSettingsDialog.show(this, tvOutput, tvDiagnostics);
        }
    }

    @Override
    public void onBackPressed() {
        if (touchControls != null) touchControls.revealCtrl();
        // Preserve SDL's fallback behavior on Android versions that call this directly.
        super.onBackPressed();
    }

    /** Opens the GPU driver page over the game (CTRL dialog). */
    void openGpuDriverPage() {
        Intent intent = new Intent(this, GpuDriverActivity.class);
        intent.putExtra(GpuDriverActivity.EXTRA_FROM_GAME, true);
        startActivity(intent);
    }

    @Override
    protected boolean onUnhandledMessage(int command, Object param) {
        if (command == COMMAND_OPEN_TV_SETTINGS) {
            openTvSettings();
            return true;
        }
        if (command == COMMAND_OPEN_GAME_FOLDER) {
            openGameFolderPage();
            return true;
        }
        return super.onUnhandledMessage(command, param);
    }

    /** Opens the game folder page over the game (CTRL dialog or native settings). */
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
        if (tvOutput != null) tvOutput.pause();
        if (tvDiagnostics != null) tvDiagnostics.event("runtime pause");
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
        updateSystemBarsBehavior();
        controller.hide(WindowInsetsCompat.Type.systemBars());
    }

    void updateSystemBarsBehavior() {
        // Transient bars can stay "hidden" in app insets even while drawn. Use
        // normal swipe-revealed bars in hide-all mode so their visibility can
        // restore CTRL; the existing timer hides the bars again after 2.5 s.
        WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView())
            .setSystemBarsBehavior(touchControls != null && touchControls.hidesAllControls()
                ? WindowInsetsControllerCompat.BEHAVIOR_DEFAULT
                : WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
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
        if (tvOutput != null) tvOutput.resume();
        if (tvDiagnostics != null) tvDiagnostics.snapshot("runtime-resume");
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
        if (tvOutput != null) tvOutput.stop();
        if (tvDiagnostics != null) tvDiagnostics.stop();
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
        // The importer's drive list: SD cards and USB drives (apps cannot list /storage).
        nativeSetenv("LO_STORAGE_VOLUMES", TextUtils.join(":", GameStorage.removableRoots(this)));
        // Mesa drivers (Turnip) keep compiled pipelines on disk only with a cache
        // directory; Android gives them none. Other drivers ignore these.
        nativeSetenv("MESA_SHADER_CACHE_DIR", new File(getCacheDir(), "mesa_shader_cache").getAbsolutePath());
        nativeSetenv("MESA_SHADER_CACHE_MAX_SIZE", "256M");
        GpuDriverStore.Installed driver = GpuDriverStore.selectedDriver(this);
        if (driver != null) {
            nativeSetenv("LO_CUSTOM_DRIVER_DIR", driver.directory.getAbsolutePath() + "/");
            nativeSetenv("LO_VK_CUSTOM_DRIVER", driver.metadata.libraryName);
            nativeSetenv("LO_VK_CUSTOM_DRIVER_MODE", GpuDriverStore.loadMode(this, driver));
            if (GpuDriverStore.needsAdreno650SyncDraw(GpuDriverStore.gpuModel(), driver.metadata)) {
                nativeSetenv("LO_VK_ADRENO650_SYNC_DRAW", "1");
            }
            // PanVK refuses the Mali GPUs Mesa marks experimental without this.
            if (driver.metadata.isPanVk()) nativeSetenv("PAN_I_WANT_A_BROKEN_VULKAN_DRIVER", "1");
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
            List<String> arguments = new ArrayList<>(Arrays.asList(
                "--game", game.getAbsolutePath(), "--install", "--quiet-kernel"));
            String[] sources = getIntent().getStringArrayExtra(EXTRA_IMPORT_SOURCES);
            if (sources != null) for (String source : sources) arguments.addAll(Arrays.asList("--install-source", source));
            return arguments.toArray(new String[0]);
        }
        // SDL calls this before nativeRunMain. Never migrate in an Activity lifecycle
        // callback: the guest may still be writing even while another page is visible.
        SaveLocationState.Run saves = SaveLocationStore.prepare(this);
        nativeSetenv("LO_SAVE_DIR", saves.root.getAbsolutePath());
        nativeSetenv("LO_SAVE_SHARED", saves.usesSharedStorage(SaveTransfer.saveRoot(getFilesDir())) ? "1" : "0");
        if (saves.fallback || saves.conflict || saves.error != null) runOnUiThread(() -> {
            if (isFinishing() || isDestroyed()) return;
            Context strings = SaveLocationStrings.context(this);
            int reason = saves.conflict ? R.string.save_location_conflict
                : saves.fallback ? R.string.save_location_fallback : R.string.save_location_failed;
            new AlertDialog.Builder(this).setTitle(strings.getString(R.string.save_location_warning_title))
                .setMessage(strings.getString(reason) + "\n\n"
                    + strings.getString(R.string.save_location_current, saves.root.getAbsolutePath()))
                .setPositiveButton(strings.getString(R.string.save_location_ok), null).show();
        });
        return new String[] { "--game", game.getAbsolutePath(), "--quiet-kernel" };
    }
}
