package io.github.freefrank.lostodyssey;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Saves and logs page (CTRL dialog): the saves live in the app's private storage,
 * which players cannot open, so they are exported to and imported from a ZIP file
 * through the system file picker. That needs no storage permission. The runtime
 * logs, which newer Android versions also hide from file managers, export the
 * same way for bug reports.
 */
public final class SaveTransferActivity extends Activity {
    static final String EXTRA_FROM_GAME = "from_game";
    private static final int REQUEST_EXPORT = 1;
    private static final int REQUEST_IMPORT = 2;
    private static final int REQUEST_EXPORT_LOGS = 3;

    private boolean fromGame;
    private boolean busy;
    private LinearLayout page;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        PlayerLogs.installCrashHandler(this);
        super.onCreate(savedInstanceState);
        fromGame = getIntent().getBooleanExtra(EXTRA_FROM_GAME, false);
        page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setBackgroundColor(Color.rgb(18, 24, 30));
        int padding = dp(20);
        page.setPadding(padding, padding, padding, padding);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.rgb(18, 24, 30));
        scroll.setFillViewport(true);
        scroll.addView(page, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT));
        // Android 15 draws apps edge to edge: keep the page clear of the status bar.
        scroll.setOnApplyWindowInsetsListener((view, insets) -> {
            view.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        });
        setContentView(scroll);
    }

    @Override
    protected void onResume() {
        super.onResume();
        render();
    }

    private File saveRoot() {
        return SaveTransfer.saveRoot(getFilesDir());
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

    private Button button(String label, View.OnClickListener listener) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setOnClickListener(listener);
        return button;
    }

    private void render() {
        page.removeAllViews();
        page.addView(text("Saves and logs", 24f, true));

        List<File> slots = SaveTransfer.listSlots(saveRoot());
        if (slots.isEmpty()) {
            page.addView(dim("No saves yet."));
        } else {
            DateFormat format = DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT);
            StringBuilder list = new StringBuilder();
            for (File slot : slots)
                list.append(slot.getName()).append("  ·  ")
                    .append(format.format(new Date(new File(slot, SaveTransfer.SAVE_FILE).lastModified())))
                    .append('\n');
            page.addView(dim(list.toString().trim()));
        }

        page.addView(text("Export", 18f, true));
        page.addView(dim("Saves every slot to one ZIP file, for example in Download. To play on a PC, "
            + "extract it beside LostOdysseyRecomp.exe, or copy its userNN folders into the save folder."));
        Button export = button("Export saves…", v -> startExport());
        export.setEnabled(!busy && !slots.isEmpty());
        page.addView(export);

        page.addView(text("Import", 18f, true));
        page.addView(dim("Reads a ZIP with save folders: one exported here or from a PC, a ZIP of "
            + "Xenia's userNN folders, or the download from the RGH save converter."));
        Button importButton = button("Import saves…", v -> startImport());
        importButton.setEnabled(!busy);
        page.addView(importButton);

        page.addView(text("Logs", 18f, true));
        page.addView(dim(PlayerLogs.debugLogEnabled(this)
            ? "Saves the logs of this run and the two before it to one ZIP file. Attach it to a bug report."
            : "Debug log is off, so the logs hold only warnings and errors. For a full log, turn on "
                + "Settings → System → Debug log, play until the problem happens, then export."));
        Button logs = button("Export logs…", v -> startLogExport());
        logs.setEnabled(!busy && !PlayerLogs.exportable(PlayerLogs.directory(this)).isEmpty());
        page.addView(logs);

        page.addView(button(fromGame ? "Back to game" : "Back", v -> finish()));
    }

    // Export ------------------------------------------------------------------

    private void startExport() {
        String name = "LostOdyssey-saves-"
            + new SimpleDateFormat("yyyyMMdd-HHmm", Locale.US).format(new Date()) + ".zip";
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType("application/zip")
            .putExtra(Intent.EXTRA_TITLE, name);
        launch(intent, REQUEST_EXPORT);
    }

    private void export(Uri uri) {
        List<File> slots = SaveTransfer.listSlots(saveRoot());
        runInBackground(() -> {
            try (OutputStream output = getContentResolver().openOutputStream(uri, "w")) {
                if (output == null) throw new IOException("cannot open the file");
                SaveTransfer.exportZip(slots, output);
            }
            return () -> Toast.makeText(this, "Exported " + slots.size() + " saves", Toast.LENGTH_LONG).show();
        }, "Export failed");
    }

    // Logs --------------------------------------------------------------------

    private void startLogExport() {
        String name = "LostOdyssey-logs-"
            + new SimpleDateFormat("yyyyMMdd-HHmm", Locale.US).format(new Date()) + ".zip";
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType("application/zip")
            .putExtra(Intent.EXTRA_TITLE, name);
        launch(intent, REQUEST_EXPORT_LOGS);
    }

    private void exportLogs(Uri uri) {
        List<File> files = PlayerLogs.exportable(PlayerLogs.directory(this));
        String version;
        try {
            version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (PackageManager.NameNotFoundException e) {
            version = "?";
        }
        String device = PlayerLogs.deviceDescription() + "\napp=" + version
            + "\ndebug_log=" + (PlayerLogs.debugLogEnabled(this) ? "on" : "off");
        runInBackground(() -> {
            try (OutputStream output = getContentResolver().openOutputStream(uri, "w")) {
                if (output == null) throw new IOException("cannot open the file");
                PlayerLogs.exportZip(files, device, output);
            }
            return () -> Toast.makeText(this, "Exported " + files.size() + " log files", Toast.LENGTH_LONG).show();
        }, "Export failed");
    }

    // Import ------------------------------------------------------------------

    private void startImport() {
        // File managers often label ZIPs as something else; accept any file.
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType("*/*")
            .putExtra(Intent.EXTRA_MIME_TYPES, new String[] { "application/zip",
                "application/x-zip-compressed", "application/octet-stream" });
        launch(intent, REQUEST_IMPORT);
    }

    private void read(Uri uri) {
        runInBackground(() -> {
            List<SaveTransfer.Slot> slots;
            try (InputStream input = getContentResolver().openInputStream(uri)) {
                if (input == null) throw new IOException("cannot open the file");
                slots = SaveTransfer.readZip(input);
            }
            return () -> confirmImport(slots);
        }, "Import failed");
    }

    private void confirmImport(List<SaveTransfer.Slot> slots) {
        List<String> existing = new ArrayList<>();
        StringBuilder names = new StringBuilder();
        for (SaveTransfer.Slot slot : slots) {
            if (new File(saveRoot(), slot.name).exists()) existing.add(slot.name);
            names.append(names.length() == 0 ? "" : ", ").append(slot.name);
        }
        if (existing.isEmpty()) {
            install(slots);
            return;
        }
        List<SaveTransfer.Slot> fresh = new ArrayList<>();
        for (SaveTransfer.Slot slot : slots) if (!existing.contains(slot.name)) fresh.add(slot);
        AlertDialog.Builder dialog = new AlertDialog.Builder(this)
            .setTitle("Replace saves?")
            .setMessage("The ZIP holds " + names + ".\n\nThese slots already have a save: "
                + String.join(", ", existing) + ". Replacing them cannot be undone; "
                + "export your saves first to keep a copy.")
            .setPositiveButton("Replace", (d, which) -> install(slots))
            .setNegativeButton("Cancel", null);
        if (!fresh.isEmpty()) dialog.setNeutralButton("Import only new", (d, which) -> install(fresh));
        dialog.show();
    }

    private void install(List<SaveTransfer.Slot> slots) {
        boolean replacing = false;
        for (SaveTransfer.Slot slot : slots) replacing |= new File(saveRoot(), slot.name).exists();
        boolean replaced = replacing;
        runInBackground(() -> {
            for (SaveTransfer.Slot slot : slots) SaveTransfer.install(slot, saveRoot());
            return () -> imported(slots.size(), replaced);
        }, "Import failed");
    }

    private void imported(int count, boolean replaced) {
        render();
        // New slots show up the next time the game lists its saves. A replaced
        // slot may still be held by the running game: restart to load it.
        if (!fromGame || !replaced) {
            Toast.makeText(this, "Imported " + count + " saves", Toast.LENGTH_LONG).show();
            return;
        }
        new AlertDialog.Builder(this)
            .setTitle("Restart the game?")
            .setMessage("Imported " + count + " saves. The running game may still use the old "
                + "save it replaced, so restart before loading. Progress since the last save is lost.")
            .setPositiveButton("Restart", (d, which) -> {
                Intent intent = new Intent(this, RuntimeActivity.class);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                startActivity(intent);
                // The pending activity outlives the process; Android starts a new one for it.
                Runtime.getRuntime().exit(0);
            })
            .setNegativeButton("Later", null)
            .show();
    }

    // Shared ------------------------------------------------------------------

    private void launch(Intent intent, int request) {
        try {
            startActivityForResult(intent, request);
        } catch (RuntimeException e) {
            Toast.makeText(this, "No file picker available", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        if (requestCode == REQUEST_EXPORT) export(data.getData());
        else if (requestCode == REQUEST_IMPORT) read(data.getData());
        else if (requestCode == REQUEST_EXPORT_LOGS) exportLogs(data.getData());
    }

    private interface Work { Runnable run() throws IOException; }

    /** File work off the UI thread; the returned step runs on it afterwards. */
    private void runInBackground(Work work, String failure) {
        busy = true;
        render();
        new Thread(() -> {
            Runnable next;
            try {
                next = work.run();
            } catch (IOException | RuntimeException e) {
                String reason = e.getMessage() != null ? e.getMessage() : e.toString();
                next = () -> new AlertDialog.Builder(this).setTitle(failure)
                    .setMessage(reason).setPositiveButton("OK", null).show();
            }
            Runnable done = next;
            runOnUiThread(() -> {
                busy = false;
                if (isFinishing() || isDestroyed()) return;
                render();
                done.run();
            });
        }, "save-transfer").start();
    }
}
