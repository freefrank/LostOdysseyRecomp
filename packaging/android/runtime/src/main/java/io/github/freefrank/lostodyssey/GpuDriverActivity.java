package io.github.freefrank.lostodyssey;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * GPU driver page: pick the system Vulkan driver or an installed Mesa package
 * (Turnip on Qualcomm, PanVK on Mali), choose how it loads, and download
 * packages from the device's sources in {@link GpuDriverCatalog}.
 *
 * It is the launcher activity. On a Qualcomm or Mali device it shows once,
 * before the first game start, and again after a start with a custom driver
 * that never reached the game; otherwise it hands straight over to the game.
 * The CTRL dialog's "GPU driver" button opens it while playing; changing the
 * driver or its loading mode then restarts the process, because the driver is
 * bound before the Vulkan instance.
 */
public final class GpuDriverActivity extends Activity {
    static final String EXTRA_FROM_GAME = "from_game";
    /** The renderer could not start with the selected driver; the value is the reason. */
    static final String EXTRA_GRAPHICS_FAILURE = "graphics_failure";
    private static final int REQUEST_OPEN_PACKAGE = 1;
    private static final String MALI_GPUS = "Mali-G57, G68, G77, G78 and Mali-G610 or newer (including Immortalis)";

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private boolean fromGame;
    private String originalSelection;
    /** Loading mode of the driver the game runs with, to tell a mode change that needs a restart. */
    private String originalMode;
    private String failedDriver;
    private String graphicsFailure;
    private GpuDriverStore.Vendor vendor;
    /** For Mali: the GPU name, from sysfs or from the reported failure; null when unknown. */
    private String gpuName;
    private GpuDriverCatalog.MaliSupport maliSupport = GpuDriverCatalog.MaliSupport.UNKNOWN;
    private LinearLayout installedSection, sourcesSection;
    private Button primaryButton;
    private TextView statusLine;
    private final List<RadioButton> choices = new ArrayList<>();
    private String pendingSelection;
    private boolean loadingFeeds;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PlayerLogs.installCrashHandler(this);
        // Make the game folders before the first start so they can be filled over USB.
        GameStorage.prepare(this);
        fromGame = getIntent().getBooleanExtra(EXTRA_FROM_GAME, false);
        graphicsFailure = getIntent().getStringExtra(EXTRA_GRAPHICS_FAILURE);
        vendor = GpuDriverStore.vendor();
        gpuName = GpuDriverStore.gpuModel();
        if (vendor == GpuDriverStore.Vendor.MALI) {
            // The failure reason starts with the GPU name the renderer saw.
            if (GpuDriverCatalog.maliArch(gpuName) == 0) gpuName = GpuDriverCatalog.maliName(graphicsFailure);
            maliSupport = GpuDriverCatalog.maliSupport(GpuDriverCatalog.maliArch(gpuName));
        }
        if (!fromGame) {
            // Shows the previous run's logs over USB, complete even after a crash.
            PlayerLogs.publish(this);
            if (!GpuDriverStore.supported()) {
                // No custom drivers here: say why the game closed instead of
                // starting it into the same failure again.
                if (graphicsFailure != null) showUnsupportedDriver(); else launchGame();
                return;
            }
            failedDriver = GpuDriverStore.takeFailedBoot(this);
            if (failedDriver == null && graphicsFailure == null && GpuDriverStore.choiceMade(this)) {
                launchGame();
                return;
            }
        }
        originalSelection = GpuDriverStore.selected(this);
        pendingSelection = originalSelection;
        GpuDriverStore.Installed active = GpuDriverStore.selectedDriver(this);
        originalMode = active != null ? GpuDriverStore.loadMode(this, active) : null;
        // Mali has one source: show its releases with the recommended one at once.
        for (GpuDriverCatalog.Source source : GpuDriverCatalog.sourcesFor(vendor)) {
            if (source.recommendedTag != null) expandedSources.add(source.path);
        }
        setContentView(buildPage());
        refreshInstalled();
        if (maliSupport != GpuDriverCatalog.MaliSupport.UNSUPPORTED) loadFeeds(false);
    }

    private boolean isMali() { return vendor == GpuDriverStore.Vendor.MALI; }

    /** Whether the driver the game runs with now loads in another mode than when the page opened. */
    private boolean activeModeChanged() {
        GpuDriverStore.Installed active = GpuDriverStore.selectedDriver(this);
        return active != null && active.id().equals(originalSelection) && originalMode != null
            && !originalMode.equals(GpuDriverStore.loadMode(this, active));
    }

    private void showUnsupportedDriver() {
        new AlertDialog.Builder(this)
            .setTitle("GPU driver not supported")
            .setMessage("This device's Vulkan driver cannot run the game: " + graphicsFailure
                + ".\n\nThe log is in Android/data/io.github.freefrank.lostodyssey/files/logs/.")
            .setPositiveButton("Close", (dialog, which) -> finish())
            .setOnCancelListener(dialog -> finish())
            .show();
    }

    private void launchGame() {
        // Without game data the game folder page comes first.
        startActivity(new Intent(this, GameStorage.hasGame(this) ? RuntimeActivity.class
                                                                 : GameFolderActivity.class));
        finish();
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private TextView text(String value, float sizeSp, boolean bold) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sizeSp);
        view.setTextColor(Color.WHITE);
        if (bold) view.setTypeface(Typeface.DEFAULT_BOLD);
        view.setPadding(0, dp(4), 0, dp(4));
        return view;
    }

    private TextView dim(String value) {
        TextView view = text(value, 13f, false);
        view.setTextColor(Color.argb(255, 190, 200, 210));
        return view;
    }

    private View buildPage() {
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setBackgroundColor(Color.rgb(18, 24, 30));
        int padding = dp(20);
        page.setPadding(padding, padding, padding, padding);

        page.addView(text("GPU driver", 24f, true));
        if (isMali()) {
            addMaliIntroduction(page);
        } else {
            String model = gpuName;
            int number = GpuDriverCatalog.adrenoModel(model);
            StringBuilder about = new StringBuilder("GPU: ").append(model != null ? model : "unknown");
            if (number > 0) about.append(" · Eden recommends: ").append(GpuDriverCatalog.edenRecommendation(number));
            about.append("\nVerified with this game on Adreno 750: ").append(GpuDriverStore.VERIFIED_DRIVER);
            page.addView(dim(about.toString()));
            page.addView(dim("The Qualcomm driver drops the text on the highlighted menu row. "
                + "A Mesa Turnip package fixes it. Downloads come from the same GitHub "
                + "sources as the Eden emulator; the game restarts when the driver changes."));
        }
        String notice = null;
        if (graphicsFailure != null) {
            GpuDriverStore.Installed selected = GpuDriverStore.selectedDriver(this);
            if (selected != null) {
                notice = "The last start with \"" + selected.metadata.name + "\" could not start the renderer: "
                    + graphicsFailure + (isMali() ? ". Switch its loading mode or select another driver."
                                                  : ". Select another driver.");
            } else if (!isMali()) {
                notice = "The system GPU driver cannot run the game: " + graphicsFailure
                    + ". Download a Turnip driver below, select it and start the game.";
            } else {
                notice = "The Mali system driver cannot run the game: " + graphicsFailure + ". "
                    + (maliSupport == GpuDriverCatalog.MaliSupport.UNSUPPORTED
                        ? "No driver can run the game on this GPU."
                        : "Download PanVK-kbase below, select it and start the game.");
            }
        } else if (failedDriver != null) {
            notice = "The last start with \"" + failedDriver + "\" did not reach the game."
                + (failedDriver.equals(GpuDriverStore.SYSTEM_DRIVER_NAME) ? ""
                                                                   : " The system driver is selected again.")
                + (isMali() && !failedDriver.equals(GpuDriverStore.SYSTEM_DRIVER_NAME)
                    ? " To try again, switch the driver's loading mode or pick another release." : "");
        }
        if (notice != null) {
            TextView warning = text(notice, 14f, true);
            warning.setTextColor(Color.rgb(255, 196, 80));
            page.addView(warning);
        }

        page.addView(text("Installed", 18f, true));
        installedSection = new LinearLayout(this);
        installedSection.setOrientation(LinearLayout.VERTICAL);
        page.addView(installedSection);

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        actions.setPadding(0, dp(8), 0, dp(8));
        primaryButton = new Button(this);
        primaryButton.setOnClickListener(v -> onPrimary());
        actions.addView(primaryButton);
        Button fromFile = new Button(this);
        fromFile.setText("Install from file");
        fromFile.setOnClickListener(v -> openPackage());
        actions.addView(fromFile);
        Button refresh = new Button(this);
        refresh.setText("Refresh");
        refresh.setOnClickListener(v -> loadFeeds(true));
        actions.addView(refresh);
        page.addView(actions);
        statusLine = dim("");
        page.addView(statusLine);

        sourcesSection = new LinearLayout(this);
        sourcesSection.setOrientation(LinearLayout.VERTICAL);
        if (maliSupport != GpuDriverCatalog.MaliSupport.UNSUPPORTED) {
            page.addView(text("Download", 18f, true));
            page.addView(sourcesSection);
        }

        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.rgb(18, 24, 30));
        scroll.setFillViewport(true);
        scroll.addView(page, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT));
        return scroll;
    }

    private void addMaliIntroduction(LinearLayout page) {
        String gpu = gpuName != null ? gpuName : "Mali (model unknown)";
        if (maliSupport == GpuDriverCatalog.MaliSupport.UNSUPPORTED) {
            String name = GpuDriverCatalog.maliName(gpuName);
            page.addView(dim("GPU: " + gpu));
            TextView none = text("No usable driver for this GPU" + (name != null ? " (" + name + ")" : "") + ".", 15f, true);
            none.setTextColor(Color.rgb(255, 196, 80));
            page.addView(none);
            page.addView(dim("The game needs a " + MALI_GPUS + ". The system driver stays selectable, "
                + "but the game does not start with it on this GPU."));
            return;
        }
        StringBuilder about = new StringBuilder("GPU: ").append(gpu)
            .append("\nRecommended: PanVK-kbase ").append(GpuDriverCatalog.PANVK_RECOMMENDED_TAG);
        if (maliSupport == GpuDriverCatalog.MaliSupport.UNTESTED) about.append(" (not yet tested on this GPU by its author)");
        page.addView(dim(about.toString()));
        page.addView(dim("Arm's Mali driver lacks features the game needs, so the game does not start with the "
            + "system driver; it stays selectable. PanVK-kbase is an experimental open-source Mesa driver for "
            + MALI_GPUS + ". Its author supports Android 15 and newer; Android 13 and 14 may not work"
            + (android.os.Build.VERSION.SDK_INT < GpuDriverStore.PANVK_LOWEST_API
                ? ", and this device's Android is too old for it." : ".")
            + " Downloads come from its GitHub releases; the game restarts when the driver changes."));
        page.addView(dim("Loading mode: \"direct\" opens the driver on its own and lets it show the picture; "
            + "\"libadrenotools\" puts it under Android's Vulkan loader. If the game does not start with one, "
            + "try the other."));
    }

    private void refreshInstalled() {
        installedSection.removeAllViews();
        choices.clear();
        List<GpuDriverStore.Installed> installed = GpuDriverStore.installed(this);
        boolean pendingExists = pendingSelection.isEmpty();
        for (GpuDriverStore.Installed driver : installed) pendingExists |= driver.id().equals(pendingSelection);
        if (!pendingExists) pendingSelection = GpuDriverStore.SYSTEM_DRIVER;
        addChoice(GpuDriverStore.SYSTEM_DRIVER, GpuDriverStore.SYSTEM_DRIVER_NAME,
            isMali() ? "The device's own Mali driver; the game does not start with it"
                     : "The Vulkan driver shipped with this device", null);
        for (GpuDriverStore.Installed driver : installed) {
            addChoice(driver.id(), driver.metadata.name, driver.metadata.summary(), driver);
        }
        updatePrimary();
    }

    private void addChoice(String id, String title, String summary, GpuDriverStore.Installed driver) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        RadioButton radio = new RadioButton(this);
        radio.setChecked(id.equals(pendingSelection));
        radio.setOnClickListener(v -> {
            pendingSelection = id;
            for (RadioButton other : choices) other.setChecked(other == radio);
            updatePrimary();
        });
        choices.add(radio);
        row.addView(radio);
        LinearLayout labels = new LinearLayout(this);
        labels.setOrientation(LinearLayout.VERTICAL);
        labels.addView(text(title, 15f, true));
        if (summary != null && !summary.isEmpty()) labels.addView(dim(summary));
        labels.setOnClickListener(v -> radio.performClick());
        row.addView(labels, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        if (driver != null) {
            // Loading mode: PanVK packages (and any package on Mali) can load
            // directly or through libadrenotools; Turnip keeps libadrenotools only.
            if (isMali() || driver.metadata.isPanVk()) {
                Button mode = new Button(this);
                mode.setAllCaps(false);
                boolean direct = GpuDriverStore.LOAD_ICD.equals(GpuDriverStore.loadMode(this, driver));
                mode.setText(direct ? "Mode: direct" : "Mode: libadrenotools");
                mode.setOnClickListener(v -> {
                    boolean nowDirect = GpuDriverStore.LOAD_ICD.equals(GpuDriverStore.loadMode(this, driver));
                    GpuDriverStore.setLoadMode(this, driver,
                        nowDirect ? GpuDriverStore.LOAD_ADRENOTOOLS : GpuDriverStore.LOAD_ICD);
                    refreshInstalled();
                });
                row.addView(mode);
            }
            Button remove = new Button(this);
            remove.setText("Remove");
            remove.setOnClickListener(v -> {
                GpuDriverStore.remove(this, driver);
                if (pendingSelection.equals(driver.id())) pendingSelection = GpuDriverStore.SYSTEM_DRIVER;
                refreshInstalled();
                renderSources();
            });
            row.addView(remove);
        }
        installedSection.addView(row);
    }

    /** From the game, nothing to apply: the same driver in the same loading mode. */
    private boolean unchanged() {
        return pendingSelection.equals(originalSelection) && !activeModeChanged();
    }

    private void updatePrimary() {
        if (!fromGame) primaryButton.setText("Start game");
        else if (unchanged()) primaryButton.setText("Back to game");
        else primaryButton.setText("Apply and restart");
    }

    private void onPrimary() {
        if (!fromGame) {
            GpuDriverStore.select(this, pendingSelection);
            launchGame();
            return;
        }
        if (unchanged()) { finish(); return; }
        new AlertDialog.Builder(this)
            .setTitle("Restart the game?")
            .setMessage("The driver is chosen when the game starts, so the game restarts now. "
                + "Progress since the last save is lost.")
            .setPositiveButton("Restart", (dialog, which) -> {
                GpuDriverStore.select(this, pendingSelection);
                Intent intent = new Intent(this, RuntimeActivity.class);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                startActivity(intent);
                // The pending activity outlives the process; Android starts a new one for it.
                Runtime.getRuntime().exit(0);
            })
            .setNegativeButton("Cancel", null)
            .show();
    }

    // Catalog ---------------------------------------------------------------

    private final List<GpuDriverCatalog.Feed> feeds = new ArrayList<>();

    private void loadFeeds(boolean forceRefresh) {
        if (loadingFeeds) return;
        loadingFeeds = true;
        statusLine.setText(forceRefresh ? "Refreshing sources…" : "Loading sources…");
        feeds.clear();
        renderSources();
        final File cacheDir = getCacheDir();
        final List<GpuDriverCatalog.Source> sources = GpuDriverCatalog.sourcesFor(vendor);
        new Thread(() -> {
            for (GpuDriverCatalog.Source source : sources) {
                GpuDriverCatalog.Feed feed = GpuDriverCatalog.fetch(cacheDir, source, forceRefresh);
                mainHandler.post(() -> {
                    if (isFinishing()) return;
                    feeds.add(feed);
                    renderSources();
                });
            }
            mainHandler.post(() -> {
                loadingFeeds = false;
                if (!isFinishing()) statusLine.setText("");
            });
        }, "gpu-driver-catalog").start();
    }

    /** Sources whose release list is unfolded; only one with a recommended release starts unfolded. */
    private final Set<String> expandedSources = new HashSet<>();

    private void renderSources() {
        sourcesSection.removeAllViews();
        // The recommended package first, as one button (PanVK on Mali).
        for (GpuDriverCatalog.Feed feed : feeds) {
            for (GpuDriverCatalog.Release release : feed.releases) {
                if (!release.recommended) continue;
                for (GpuDriverCatalog.Asset asset : release.assets) {
                    if (GpuDriverStore.isInstalled(this, asset.name)) continue;
                    Button button = new Button(this);
                    button.setAllCaps(false);
                    button.setText("Download recommended: " + release.title
                        + (asset.size > 0 ? String.format(Locale.ROOT, " (%.1f MB)", asset.size / 1048576.0) : ""));
                    button.setOnClickListener(v -> download(asset));
                    sourcesSection.addView(button, new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                }
            }
        }
        for (GpuDriverCatalog.Feed feed : feeds) {
            final boolean expanded = expandedSources.contains(feed.source.path);
            String header = (expanded ? "▾ " : "▸ ") + feed.source.name + " (" + feed.source.path + ")"
                + " · " + feed.releases.size() + (feed.releases.size() == 1 ? " release" : " releases");
            if (feed.fromCache) header += " · cached list";
            TextView headerView = text(header, 16f, true);
            headerView.setPadding(0, dp(10), 0, dp(10));
            headerView.setOnClickListener(v -> {
                if (expanded) expandedSources.remove(feed.source.path);
                else expandedSources.add(feed.source.path);
                renderSources();
            });
            sourcesSection.addView(headerView);
            if (feed.error != null) {
                TextView error = dim("Could not load: " + feed.error);
                error.setTextColor(Color.rgb(255, 150, 150));
                sourcesSection.addView(error);
            }
            if (!expanded) continue;
            if (feed.releases.isEmpty() && feed.error == null) sourcesSection.addView(dim("No driver packages."));
            for (GpuDriverCatalog.Release release : feed.releases) {
                String title = release.title + (release.recommended ? " · Recommended" : "")
                    + (release.latest ? " · Latest" : "") + (release.prerelease ? " · pre-release" : "");
                TextView releaseTitle = text(title, 14f, false);
                releaseTitle.setPadding(dp(12), dp(6), 0, 0);
                sourcesSection.addView(releaseTitle);
                TextView date = dim(release.date());
                date.setPadding(dp(12), 0, 0, 0);
                sourcesSection.addView(date);
                LinearLayout buttons = new LinearLayout(this);
                buttons.setOrientation(LinearLayout.VERTICAL);
                buttons.setPadding(dp(12), 0, 0, 0);
                for (GpuDriverCatalog.Asset asset : release.assets) {
                    Button button = new Button(this);
                    boolean installed = GpuDriverStore.isInstalled(this, asset.name);
                    button.setText(installed ? asset.name + " · installed" : asset.name
                        + (asset.size > 0 ? String.format(Locale.ROOT, " (%.1f MB)", asset.size / 1048576.0) : ""));
                    button.setEnabled(!installed);
                    button.setAllCaps(false);
                    button.setOnClickListener(v -> download(asset));
                    buttons.addView(button, new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
                }
                sourcesSection.addView(buttons);
            }
        }
    }

    private void download(GpuDriverCatalog.Asset asset) {
        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(20), dp(10), dp(20), 0);
        TextView label = text(asset.name, 14f, false);
        ProgressBar bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(1000);
        bar.setIndeterminate(true);
        content.addView(label);
        content.addView(bar);
        AlertDialog dialog = new AlertDialog.Builder(this).setTitle("Downloading")
            .setView(content).setCancelable(false).create();
        dialog.show();
        final File cacheDir = new File(getCacheDir(), "gpu_driver_download");
        new Thread(() -> {
            String failure = null;
            GpuDriverStore.Installed result = null;
            File zip = null;
            try {
                zip = GpuDriverCatalog.download(asset, cacheDir, (done, total) -> mainHandler.post(() -> {
                    if (total > 0) {
                        bar.setIndeterminate(false);
                        bar.setProgress((int) (done * 1000L / total));
                    }
                    label.setText(String.format(Locale.ROOT, "%s · %.1f MB", asset.name, done / 1048576.0));
                }));
                result = GpuDriverStore.install(this, zip);
            } catch (IOException | RuntimeException e) {
                failure = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
            } finally {
                //noinspection ResultOfMethodCallIgnored
                if (zip != null) zip.delete();
            }
            final String error = failure;
            final GpuDriverStore.Installed installed = result;
            mainHandler.post(() -> {
                dialog.dismiss();
                if (isFinishing()) return;
                if (installed != null) {
                    pendingSelection = installed.id();
                    Toast.makeText(this, "Installed " + installed.metadata.name, Toast.LENGTH_SHORT).show();
                } else {
                    new AlertDialog.Builder(this).setTitle("Download failed")
                        .setMessage(error).setPositiveButton("OK", null).show();
                }
                refreshInstalled();
                renderSources();
            });
        }, "gpu-driver-download").start();
    }

    // Manual install ----------------------------------------------------------

    private void openPackage() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/zip");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "application/zip", "application/octet-stream" });
        try {
            startActivityForResult(intent, REQUEST_OPEN_PACKAGE);
        } catch (RuntimeException e) {
            Toast.makeText(this, "No file picker available", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_OPEN_PACKAGE || resultCode != RESULT_OK || data == null
                || data.getData() == null) return;
        Uri uri = data.getData();
        String name = displayName(uri);
        File copy = new File(new File(getCacheDir(), "gpu_driver_download"), GpuDriverStore.slug(name) + ".zip");
        String error = null;
        GpuDriverStore.Installed installed = null;
        try {
            //noinspection ResultOfMethodCallIgnored
            copy.getParentFile().mkdirs();
            try (InputStream input = getContentResolver().openInputStream(uri);
                 OutputStream output = new FileOutputStream(copy)) {
                if (input == null) throw new IOException("cannot open " + name);
                byte[] chunk = new byte[65536];
                long total = 0;
                for (int read; (read = input.read(chunk)) > 0; ) {
                    total += read;
                    if (total > GpuDriverStore.MAX_LIBRARY_BYTES) throw new IOException("package too large");
                    output.write(chunk, 0, read);
                }
            }
            installed = GpuDriverStore.install(this, copy);
        } catch (IOException | RuntimeException e) {
            error = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
        } finally {
            //noinspection ResultOfMethodCallIgnored
            copy.delete();
        }
        if (installed != null) {
            pendingSelection = installed.id();
            Toast.makeText(this, "Installed " + installed.metadata.name, Toast.LENGTH_SHORT).show();
        } else {
            new AlertDialog.Builder(this).setTitle("Not a driver package")
                .setMessage(error).setPositiveButton("OK", null).show();
        }
        refreshInstalled();
        renderSources();
    }

    private String displayName(Uri uri) {
        String name = null;
        try (android.database.Cursor cursor = getContentResolver().query(uri,
                new String[] { android.provider.OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
        } catch (RuntimeException ignored) {
            // fall back to the last path segment
        }
        if (name == null || name.isEmpty()) name = uri.getLastPathSegment();
        return name == null || name.isEmpty() ? "driver.zip" : name;
    }
}
