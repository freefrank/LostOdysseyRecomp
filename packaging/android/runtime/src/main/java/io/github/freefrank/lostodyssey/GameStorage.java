package io.github.freefrank.lostodyssey;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.MediaScannerConnection;
import android.os.Build;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Where the user's game data lives: {@code Android/data/<package>/files/game/}
 * on the internal shared storage or on an SD card.
 *
 * Android creates that folder only when the app first asks for it, and a PC
 * sees new folders over USB (MTP) only after the media scanner has indexed a
 * file inside them. The launcher therefore creates the game folder on every
 * volume and scans a README in each, so it is there to copy into. Empty disc
 * folders are not made: the importer and the disc lookup would take them for
 * a damaged installation.
 */
final class GameStorage {
    static final String README = "README.txt";
    private static final String README_TEXT =
        "Copy the imported game data here: the folders disc1 to disc4 from the importer,\n"
        + "so that disc1/default.xex exists, and DLC in dlc/<content-id>.\n"
        + "The game uses the first storage (internal or SD card) whose disc1 has default.xex,\n"
        + "otherwise this folder on the internal storage.\n";

    private GameStorage() {}

    /** The {@code files} folder of every mounted shared-storage volume, internal first. */
    static List<File> volumes(Context context) {
        List<File> result = new ArrayList<>();
        File[] dirs = context.getExternalFilesDirs(null);
        if (dirs != null) for (File dir : Arrays.asList(dirs)) if (dir != null) result.add(dir);
        return result;
    }

    /**
     * The roots of the mounted SD cards and USB drives, for the importer's drive
     * list: apps cannot list {@code /storage}, so the native side cannot find them.
     */
    static List<File> removableRoots(Context context) {
        List<File> roots = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 30 /* Android 11 (R) */) {
            StorageManager storage = context.getSystemService(StorageManager.class);
            if (storage == null) return roots;
            for (StorageVolume volume : storage.getStorageVolumes()) {
                File directory = volume.getDirectory();
                if (!volume.isPrimary() && directory != null) roots.add(directory);
            }
            return roots;
        }
        // Before Android 11 only the app folders name the volumes; the first is the internal one.
        List<File> volumes = volumes(context);
        for (File files : volumes.subList(Math.min(1, volumes.size()), volumes.size())) {
            File root = volumeRoot(files);
            if (root != null) roots.add(root);
        }
        return roots;
    }

    /** {@code /storage/1234-ABCD} for the app folder {@code /storage/1234-ABCD/Android/data/<package>/files}. */
    static File volumeRoot(File appFiles) {
        File root = appFiles;
        for (int level = 0; level < 4 && root != null; ++level) root = root.getParentFile();
        return root;
    }

    /** Creates {@code game/} with a README on each volume; returns the README paths written. */
    static List<String> createFolders(List<File> volumes) {
        List<String> written = new ArrayList<>();
        for (File volume : volumes) {
            File game = new File(volume, "game");
            //noinspection ResultOfMethodCallIgnored
            game.mkdirs();
            File readme = new File(game, README);
            if (readme.exists() || !game.isDirectory()) continue;
            try (OutputStream output = new FileOutputStream(readme)) {
                output.write(README_TEXT.getBytes(StandardCharsets.UTF_8));
                written.add(readme.getAbsolutePath());
            } catch (IOException ignored) {
                // A read-only or removed card keeps its folders without the README.
            }
        }
        return written;
    }

    /** Makes the game folders and shows them to a PC connected over USB. */
    static void prepare(Context context) {
        adoptImportedRoot(context);
        List<String> written = createFolders(volumes(context));
        if (!written.isEmpty())
            MediaScannerConnection.scanFile(context.getApplicationContext(),
                written.toArray(new String[0]), null, null);
    }

    /**
     * The custom game folder when its {@code disc1} holds {@code default.xex}, else the
     * first volume whose {@code game/disc1} does, else the first volume's.
     */
    static File selectDisc1(File custom, List<File> volumes, File fallback) {
        if (custom != null && hasDefaultXex(new File(custom, "disc1"))) return new File(custom, "disc1");
        for (File volume : volumes) {
            File disc1 = new File(volume, "game/disc1");
            if (hasDefaultXex(disc1)) return disc1;
        }
        return new File(volumes.isEmpty() ? fallback : volumes.get(0), "game/disc1");
    }

    static File disc1(Context context) {
        return selectDisc1(customRoot(context), volumes(context), context.getFilesDir());
    }

    /** Where the importer writes: the custom folder when set, else the app folder on the internal storage. */
    static File importRoot(File custom, List<File> volumes, File fallback) {
        if (custom != null) return custom;
        return new File(volumes.isEmpty() ? fallback : volumes.get(0), "game");
    }

    static File importRoot(Context context) {
        return importRoot(customRoot(context), volumes(context), context.getFilesDir());
    }

    static boolean hasDefaultXex(File disc1) {
        return new File(disc1, "default.xex").isFile();
    }

    static boolean hasGame(Context context) {
        return hasDefaultXex(disc1(context));
    }

    // Custom folder -----------------------------------------------------------

    private static final String PREFERENCES = "game_storage";
    private static final String CUSTOM_ROOT = "custom_root";

    /** The chosen game folder (the one holding disc1), or null for the app folder. */
    static File customRoot(Context context) {
        String path = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
            .getString(CUSTOM_ROOT, null);
        return path == null || path.isEmpty() ? null : new File(path);
    }

    static void setCustomRoot(Context context, File root) {
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).edit()
            .putString(CUSTOM_ROOT, root != null ? root.getAbsolutePath() : null).apply();
    }

    // Importer result ---------------------------------------------------------

    private static final String IMPORTED_STAMP = "imported_stamp";

    /**
     * The native importer records its destination in {@code config/game-path.txt}
     * (the game folder holding disc1). A new record that points outside the app
     * folders becomes the custom folder, so the next start finds the game.
     */
    static File importedRootToAdopt(File gamePathFile, long lastStamp, List<File> volumes) {
        if (!gamePathFile.isFile() || gamePathFile.lastModified() == lastStamp) return null;
        String text;
        try {
            text = new String(Files.readAllBytes(gamePathFile.toPath()), StandardCharsets.UTF_8).trim();
        } catch (IOException e) {
            return null;
        }
        if (text.isEmpty()) return null;
        File root = normalizeGameRoot(new File(text));
        if (root == null) return null;
        for (File volume : volumes)
            if (root.getAbsoluteFile().equals(new File(volume, "game").getAbsoluteFile())) return null;
        return root;
    }

    static void adoptImportedRoot(Context context) {
        File record = new File(new File(context.getFilesDir(), "config"), "game-path.txt");
        SharedPreferences preferences =
            context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
        long stamp = preferences.getLong(IMPORTED_STAMP, 0);
        File root = importedRootToAdopt(record, stamp, volumes(context));
        if (record.lastModified() != stamp)
            preferences.edit().putLong(IMPORTED_STAMP, record.lastModified()).apply();
        if (root != null) setCustomRoot(context, root);
    }

    /**
     * The game folder for a folder the user picked: the folder itself when it holds
     * {@code disc1/default.xex}, its parent when the user picked {@code disc1}, else null.
     */
    static File normalizeGameRoot(File chosen) {
        if (chosen == null) return null;
        if (hasDefaultXex(new File(chosen, "disc1"))) return chosen;
        if (chosen.getName().equalsIgnoreCase("disc1") && hasDefaultXex(chosen)) {
            File parent = chosen.getParentFile();
            if (parent != null) return parent;
        }
        return null;
    }

    /**
     * Maps a document-tree id from the system folder picker to a file path:
     * {@code primary:Games/LO} lies under the internal shared storage and
     * {@code 1234-ABCD:Games/LO} under {@code /storage/1234-ABCD}. Other
     * providers (downloads, cloud) have no path and return null.
     */
    static File pathForTreeDocumentId(String documentId, File primaryRoot) {
        if (documentId == null) return null;
        int colon = documentId.indexOf(':');
        if (colon <= 0) return null;
        String volume = documentId.substring(0, colon);
        String relative = documentId.substring(colon + 1);
        // The downloads provider names folders by absolute path ("raw:/storage/…").
        if (relative.startsWith("/")) return new File(relative);
        File base = volume.equalsIgnoreCase("primary") ? primaryRoot : new File("/storage", volume);
        return relative.isEmpty() ? base : new File(base, relative);
    }
}
