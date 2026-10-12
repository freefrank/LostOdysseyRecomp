package io.github.freefrank.lostodyssey;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Game folder page: where the game data is read from. The app folder in
 * {@code Android/data} needs no permission but phone file managers cannot
 * write into it, so a folder anywhere on the internal storage or an SD card
 * can be picked instead. The native runtime opens files by path, so a picked
 * folder needs the "All files access" permission (storage permission before
 * Android 11).
 *
 * It opens instead of the game when no {@code disc1/default.xex} is found,
 * and from the CTRL dialog while playing; changing the folder then restarts.
 * "Import disc images" picks a disc image in the system file picker and
 * starts the runtime's own importer on it, which reads ISOs or extracted
 * discs from shared storage and writes the game folder.
 */
public final class GameFolderActivity extends Activity {
    static final String EXTRA_FROM_GAME = "from_game";
    private static final int REQUEST_PICK_FOLDER = 1;
    private static final int REQUEST_STORAGE_PERMISSION = 2;
    private static final int REQUEST_PICK_DISCS = 3;

    private boolean fromGame;
    private File originalCustom;
    /** What to continue with once the storage permission is granted. */
    private enum Pending { NONE, PICK, IMPORT }
    private Pending afterPermission = Pending.NONE;
    private LinearLayout page;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // A cancelled import reopens this page as the first one of a new process.
        PlayerLogs.installCrashHandler(this);
        super.onCreate(savedInstanceState);
        fromGame = getIntent().getBooleanExtra(EXTRA_FROM_GAME, false);
        // A finished import into another folder becomes the custom folder.
        GameStorage.adoptImportedRoot(this);
        originalCustom = GameStorage.customRoot(this);
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
        // Returning from the system permission page.
        if (afterPermission != Pending.NONE && canReadSharedStorage()) {
            Pending pending = afterPermission;
            afterPermission = Pending.NONE;
            if (pending == Pending.PICK) pickFolder();
            else pickDiscImages();
        }
        render();
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
        page.addView(text("Game folder", 24f, true));

        File disc1 = GameStorage.disc1(this);
        boolean found = GameStorage.hasDefaultXex(disc1);
        TextView status = text(found ? "Game found: " + disc1.getParent()
            : "No game found. Put the imported game folder (disc1/default.xex, disc2-disc4 "
                + "beside it) in one of the places below.", 15f, true);
        status.setTextColor(found ? Color.rgb(140, 220, 140) : Color.rgb(255, 196, 80));
        page.addView(status);

        page.addView(text("App folder", 18f, true));
        StringBuilder folders = new StringBuilder();
        for (File volume : GameStorage.volumes(this))
            folders.append(new File(volume, "game").getAbsolutePath()).append('\n');
        page.addView(dim(folders.toString().trim()));
        page.addView(dim("No permission needed. Copy the game here from a PC over USB; "
            + "file managers on the device usually cannot write into Android/data. "
            + "Uninstalling the app deletes this folder."));

        page.addView(text("Custom folder", 18f, true));
        File custom = GameStorage.customRoot(this);
        if (custom == null) {
            page.addView(dim("Not set."));
        } else {
            page.addView(dim(custom.getAbsolutePath()));
            if (!canReadSharedStorage()) {
                TextView warning = dim("All files access is off, so the game cannot read this folder.");
                warning.setTextColor(Color.rgb(255, 196, 80));
                page.addView(warning);
            } else if (!GameStorage.hasDefaultXex(new File(custom, "disc1"))) {
                TextView warning = dim("disc1/default.xex is missing in this folder.");
                warning.setTextColor(Color.rgb(255, 196, 80));
                page.addView(warning);
            }
        }
        page.addView(dim("Any folder on the internal storage or an SD card, for example one "
            + "filled with a file manager. Pick the folder that holds disc1. "
            + "Needs the \"All files access\" permission."));

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        actions.setPadding(0, dp(8), 0, dp(8));
        actions.addView(button("Choose folder…", v -> chooseFolder()));
        if (custom != null) {
            actions.addView(button("Use app folder", v -> {
                GameStorage.setCustomRoot(this, null);
                render();
            }));
        }
        page.addView(actions);

        page.addView(text("Import", 18f, true));
        page.addView(dim("Import the game on this device from disc images (.iso), for example in "
            + "Download, on an SD card or a USB drive. Pick one disc image; the other discs in "
            + "the same folder are found with it. For extracted discs pick default.xex. The "
            + "importer writes to " + GameStorage.importRoot(this).getAbsolutePath() + " unless "
            + "you pick another destination; the four discs need about 20 GB. Needs \"All files "
            + "access\"."));
        page.addView(button("Import disc images…", v -> {
            if (canReadSharedStorage()) pickDiscImages();
            else askForStorage(Pending.IMPORT);
        }));

        page.addView(primaryButton(found));
    }

    private void startImport(List<File> sources) {
        String[] paths = new String[sources.size()];
        for (int i = 0; i < paths.length; ++i) paths[i] = sources.get(i).getAbsolutePath();
        Runnable launch = () -> {
            Intent intent = new Intent(this, RuntimeActivity.class);
            intent.putExtra(RuntimeActivity.EXTRA_IMPORT, true);
            intent.putExtra(RuntimeActivity.EXTRA_IMPORT_SOURCES, paths);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
            startActivity(intent);
            // The native runtime runs once per process; a game in progress ends here.
            if (fromGame) Runtime.getRuntime().exit(0);
            finish();
        };
        if (!fromGame) { launch.run(); return; }
        new AlertDialog.Builder(this)
            .setTitle("Stop the game?")
            .setMessage("The importer replaces the running game. Progress since the last save is lost.")
            .setPositiveButton("Import", (dialog, which) -> launch.run())
            .setNegativeButton("Cancel", null)
            .show();
    }

    private Button primaryButton(boolean found) {
        boolean changed = !Objects.equals(originalCustom, GameStorage.customRoot(this));
        if (!fromGame) {
            Button start = button("Start game", v -> {
                startActivity(new Intent(this, RuntimeActivity.class));
                finish();
            });
            start.setEnabled(found);
            return start;
        }
        if (!changed) return button("Back to game", v -> finish());
        Button apply = button("Apply and restart", v -> new AlertDialog.Builder(this)
            .setTitle("Restart the game?")
            .setMessage("The game folder is chosen when the game starts, so the game restarts now. "
                + "Progress since the last save is lost.")
            .setPositiveButton("Restart", (dialog, which) -> {
                Intent intent = new Intent(this, RuntimeActivity.class);
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                startActivity(intent);
                // The pending activity outlives the process; Android starts a new one for it.
                Runtime.getRuntime().exit(0);
            })
            .setNegativeButton("Cancel", null)
            .show());
        // Restarting without game data would only reach the missing-game error.
        apply.setEnabled(found);
        return apply;
    }

    // Permission and picker ---------------------------------------------------

    private boolean canReadSharedStorage() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) return Environment.isExternalStorageManager();
        return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    private void chooseFolder() {
        if (canReadSharedStorage()) { pickFolder(); return; }
        askForStorage(Pending.PICK);
    }

    private void askForStorage(Pending next) {
        new AlertDialog.Builder(this)
            .setTitle("Allow file access")
            .setMessage("The game reads its files directly, so disc images and folders outside "
                + "the app folder need the \"All files access\" permission. Android opens the "
                + "setting now; turn it on for Lost Odyssey and come back.")
            .setPositiveButton("Open setting", (dialog, which) -> requestStorage(next))
            .setNegativeButton("Cancel", null)
            .show();
    }

    private void requestStorage(Pending next) {
        afterPermission = next;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
            } catch (RuntimeException e) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else {
            requestPermissions(new String[] { Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE }, REQUEST_STORAGE_PERMISSION);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        // onResume follows and continues to the picker when the permission was granted.
        if (requestCode == REQUEST_STORAGE_PERMISSION && !canReadSharedStorage()) afterPermission = Pending.NONE;
    }

    private void pickFolder() {
        try {
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), REQUEST_PICK_FOLDER);
        } catch (RuntimeException e) {
            Toast.makeText(this, "No folder picker available", Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * The file picker, not the folder picker: Android 11+ does not let the folder
     * picker choose Download or the root of an SD card, where disc images usually are.
     */
    private void pickDiscImages() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType("*/*")
            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
            // Device storage names files by path; the Downloads view may not.
            .putExtra(DocumentsContract.EXTRA_INITIAL_URI, DocumentsContract.buildDocumentUri(
                GameStorage.EXTERNAL_STORAGE_DOCUMENTS, "primary:" + Environment.DIRECTORY_DOWNLOADS));
        try {
            startActivityForResult(intent, REQUEST_PICK_DISCS);
        } catch (RuntimeException e) {
            Toast.makeText(this, "No file picker available", Toast.LENGTH_SHORT).show();
        }
    }

    private void discImagesPicked(Intent data) {
        List<Uri> uris = new ArrayList<>();
        if (data.getClipData() != null) {
            for (int i = 0; i < data.getClipData().getItemCount(); ++i) uris.add(data.getClipData().getItemAt(i).getUri());
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }
        List<File> picked = new ArrayList<>();
        for (Uri uri : uris) {
            File file = GameStorage.pathForDocument(this, uri);
            if (file == null || !file.isFile()) {
                new AlertDialog.Builder(this).setTitle("Cannot open this file")
                    .setMessage("The importer reads files from the internal storage, an SD card or a "
                        + "USB drive. Open the disc image from there in the picker's menu, not from "
                        + "cloud storage or Recent.")
                    .setPositiveButton("OK", null).show();
                return;
            }
            picked.add(file);
        }
        if (!picked.isEmpty()) startImport(GameStorage.importSources(picked));
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_PICK_DISCS) {
            if (resultCode == RESULT_OK && data != null) discImagesPicked(data);
            return;
        }
        if (requestCode != REQUEST_PICK_FOLDER || resultCode != RESULT_OK || data == null
                || data.getData() == null) return;
        String documentId;
        try {
            documentId = DocumentsContract.getTreeDocumentId(data.getData());
        } catch (RuntimeException e) {
            documentId = null;
        }
        File picked = GameStorage.pathForTreeDocumentId(documentId, Environment.getExternalStorageDirectory());
        File root = GameStorage.normalizeGameRoot(picked);
        if (root == null) {
            new AlertDialog.Builder(this).setTitle("No game in this folder")
                .setMessage(picked == null
                    ? "Pick a folder on the internal storage or an SD card."
                    : "Expected disc1/default.xex in " + picked.getAbsolutePath()
                        + ". Pick the folder that holds disc1.")
                .setPositiveButton("OK", null).show();
            return;
        }
        GameStorage.setCustomRoot(this, root);
        render();
    }
}
