package io.github.freefrank.lostodyssey;

import java.io.File;
import java.io.IOException;

/** Persistent switch state; the caller commits it after prepare returns, before the guest starts. */
final class SaveLocationState {
    String sharedRoot;
    // null: no requested change; empty: switch to private; otherwise: fixed game/save path.
    String pendingRoot;
    String approvedFingerprint;
    boolean keepDestination;
    boolean fallbackDirty;

    static final class Run {
        final File root;
        final boolean fallback;
        final boolean conflict;
        final String error;
        Run(File root, boolean fallback, boolean conflict, String error) {
            this.root = root;
            this.fallback = fallback;
            this.conflict = conflict;
            this.error = error;
        }

        boolean usesSharedStorage(File privateRoot) {
            return !fallback && !root.equals(privateRoot);
        }
    }

    File destination(File privateRoot) {
        if (pendingRoot != null) return pendingRoot.isEmpty() ? privateRoot : new File(pendingRoot);
        return sharedRoot != null ? new File(sharedRoot) : privateRoot;
    }

    Run prepare(File privateRoot) {
        return prepare(privateRoot, directory -> {});
    }

    Run prepare(File privateRoot, SaveLocationMigration.DirectorySync sync) {
        File current = sharedRoot == null ? privateRoot : new File(sharedRoot);
        boolean unavailable = false;
        String failure = null;
        if (sharedRoot != null) {
            try { SaveLocationMigration.probe(current, sync); }
            catch (IOException | RuntimeException e) { unavailable = true; failure = e.getMessage(); }
        }
        if (unavailable) fallbackDirty = true;
        // A previous fallback may contain newer progress. Keep using it until it has
        // been merged successfully, even when the card is readable again.
        File source = unavailable || fallbackDirty ? privateRoot : current;
        if (source.equals(privateRoot) && !privateRoot.isDirectory() && !privateRoot.mkdir())
            return new Run(privateRoot, unavailable || fallbackDirty, false,
                "Cannot create private save folder: " + privateRoot);
        File target = destination(privateRoot);
        boolean switching = pendingRoot != null;
        boolean recovering = !switching && fallbackDirty && !unavailable && sharedRoot != null;
        if ((switching || recovering) && !unavailable) {
            try {
                SaveLocationMigration.migrate(source, target, approvedFingerprint, keepDestination,
                    (from, to) -> {}, sync);
                if (switching) sharedRoot = pendingRoot.isEmpty() ? null : pendingRoot;
                pendingRoot = null;
                approvedFingerprint = null;
                fallbackDirty = false;
                return new Run(target, false, false, null);
            } catch (SaveLocationMigration.ConflictException e) {
                approvedFingerprint = null;
                return failedRun(privateRoot, source, true, e.getMessage(), sync);
            } catch (IOException | RuntimeException e) {
                approvedFingerprint = null;
                return failedRun(privateRoot, source, false, e.getMessage(), sync);
            }
        }
        return new Run(source, unavailable || fallbackDirty, false, failure);
    }

    private Run failedRun(File privateRoot, File source, boolean conflict, String error,
            SaveLocationMigration.DirectorySync sync) {
        if (sharedRoot != null && source.equals(new File(sharedRoot))) {
            try { SaveLocationMigration.probe(source, sync); }
            catch (IOException | RuntimeException e) {
                fallbackDirty = true;
                return new Run(privateRoot, true, conflict, error);
            }
        }
        return new Run(source, fallbackDirty, conflict, error);
    }
}
