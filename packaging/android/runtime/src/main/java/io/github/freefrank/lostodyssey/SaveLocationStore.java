package io.github.freefrank.lostodyssey;

import android.content.Context;
import android.content.SharedPreferences;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import java.io.File;
import java.io.FileDescriptor;
import java.io.IOException;

/** Android preferences and the immutable save root selected for this process's guest. */
final class SaveLocationStore {
    private static final String PREFERENCES = "save_location";
    private static SaveLocationState.Run run;
    private static SaveLocationState.Run transferRun;

    private SaveLocationStore() {}

    static SaveLocationState load(Context context) {
        SharedPreferences preferences = context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
        SaveLocationState state = new SaveLocationState();
        state.sharedRoot = preferences.getString("shared_root", null);
        state.pendingRoot = preferences.getString("pending_root", null);
        state.approvedFingerprint = preferences.getString("approved_fingerprint", null);
        state.keepDestination = preferences.getBoolean("keep_destination", false);
        state.fallbackDirty = preferences.getBoolean("fallback_dirty", false);
        return state;
    }

    static void save(Context context, SaveLocationState state) {
        // Synchronous: the native guest must not start before the chosen root is recorded.
        if (!context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).edit()
                .putString("shared_root", state.sharedRoot)
                .putString("pending_root", state.pendingRoot)
                .putString("approved_fingerprint", state.approvedFingerprint)
                .putBoolean("keep_destination", state.keepDestination)
                .putBoolean("fallback_dirty", state.fallbackDirty).commit())
            throw new IllegalStateException("Cannot save the save-location setting");
    }

    static synchronized SaveLocationState.Run prepare(Context context) {
        if (run == null) {
            SaveLocationState state = load(context);
            SaveLocationState.Run prepared = state.prepare(SaveTransfer.saveRoot(context.getFilesDir()),
                SaveLocationStore::syncDirectory);
            save(context, state);
            run = prepared;
        }
        return run;
    }

    static synchronized boolean guestStarted() { return run != null; }

    static synchronized File currentRoot(Context context) {
        if (run != null) return run.root;
        if (transferRun != null) return transferRun.root;
        SaveLocationState state = load(context);
        File privateRoot = SaveTransfer.saveRoot(context.getFilesDir());
        File root = state.sharedRoot == null || state.fallbackDirty ? privateRoot : new File(state.sharedRoot);
        String error = null;
        if (state.sharedRoot != null && !state.fallbackDirty) {
            try { SaveLocationMigration.probe(root, SaveLocationStore::syncDirectory); }
            catch (IOException | RuntimeException e) {
                root = privateRoot;
                state.fallbackDirty = true;
                error = e.getMessage();
                save(context, state);
            }
        }
        // ZIP import can reopen this page in a fresh process without starting the
        // guest. Check storage there too, but never apply a pending switch here.
        transferRun = new SaveLocationState.Run(root, state.fallbackDirty, false, error);
        return root;
    }

    static synchronized SaveLocationState.Run currentRun() { return run != null ? run : transferRun; }

    static void request(Context context, File sharedRoot) {
        SaveLocationState state = load(context);
        state.pendingRoot = sharedRoot == null ? "" : sharedRoot.getAbsolutePath();
        state.approvedFingerprint = null;
        save(context, state);
    }

    static void approve(Context context, String fingerprint, boolean keepDestination) {
        SaveLocationState state = load(context);
        state.approvedFingerprint = fingerprint;
        state.keepDestination = keepDestination;
        save(context, state);
    }

    private static void syncDirectory(File directory) throws IOException {
        FileDescriptor descriptor = null;
        try {
            descriptor = Os.open(directory.getAbsolutePath(), OsConstants.O_RDONLY, 0);
            Os.fsync(descriptor);
        } catch (ErrnoException e) {
            throw new IOException("Cannot sync save directory: " + directory, e);
        } finally {
            if (descriptor != null) {
                try { Os.close(descriptor); }
                catch (ErrnoException e) { throw new IOException("Cannot close " + directory, e); }
            }
        }
    }
}
