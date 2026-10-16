package io.github.freefrank.lostodyssey;

import java.io.File;
import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.UUID;

/** A verified, reversible slot copy. Called only before the guest starts. */
final class SaveLocationMigration {
    private SaveLocationMigration() {}

    static final class Snapshot {
        final List<String> conflicts = new ArrayList<>();
        final String fingerprint;

        Snapshot(List<String> conflicts, String fingerprint) {
            this.conflicts.addAll(conflicts);
            this.fingerprint = fingerprint;
        }
    }

    static final class ConflictException extends IOException {
        final Snapshot snapshot;
        ConflictException(Snapshot snapshot) {
            super(String.join(", ", snapshot.conflicts));
            this.snapshot = snapshot;
        }
    }

    interface CopyCheck { void beforeCopy(File source, File target) throws IOException; }
    interface DirectorySync { void sync(File directory) throws IOException; }

    static Snapshot inspect(File source, File target) throws IOException {
        List<String> conflicts = new ArrayList<>();
        for (File slot : slots(source)) {
            File destination = new File(target, slot.getName());
            if (destination.exists() && !sameTree(slot, destination)
                    && new File(destination, SaveTransfer.SAVE_FILE).lastModified()
                        >= new File(slot, SaveTransfer.SAVE_FILE).lastModified())
                conflicts.add(slot.getName());
        }
        return new Snapshot(conflicts, fingerprint(source) + ":" + fingerprint(target));
    }

    static void migrate(File source, File target, String approvedFingerprint, boolean keepDestination)
            throws IOException {
        migrate(source, target, approvedFingerprint, keepDestination, (from, to) -> {}, directory -> {});
    }

    static void migrate(File source, File target, String approvedFingerprint, boolean keepDestination,
            CopyCheck check) throws IOException {
        migrate(source, target, approvedFingerprint, keepDestination, check, directory -> {});
    }

    static void migrate(File source, File target, String approvedFingerprint, boolean keepDestination,
            CopyCheck check, DirectorySync sync) throws IOException {
        if (source.getCanonicalFile().equals(target.getCanonicalFile())) return;
        if (!source.isDirectory()) throw new IOException("Save source is missing: " + source);
        probe(target, sync);
        Snapshot snapshot = inspect(source, target);
        if (!source.isDirectory()) throw new IOException("Save source is missing: " + source);
        if (approvedFingerprint != null && !approvedFingerprint.equals(snapshot.fingerprint))
            throw new ConflictException(snapshot);
        if (!snapshot.conflicts.isEmpty() && approvedFingerprint == null)
            throw new ConflictException(snapshot);
        Map<String, String> sourceFingerprints = new LinkedHashMap<>();
        Map<String, String> targetFingerprints = new LinkedHashMap<>();
        for (File slot : slots(source)) {
            sourceFingerprints.put(slot.getName(), fingerprint(slot));
            targetFingerprints.put(slot.getName(), fingerprint(new File(target, slot.getName())));
        }
        if (!snapshot.fingerprint.equals(inspect(source, target).fingerprint))
            throw new IOException("Saves changed before the copy; try again.");

        File staging = new File(target.getParentFile(), ".save-location-" + UUID.randomUUID());
        File incoming = new File(staging, "incoming");
        File previous = new File(staging, "previous");
        List<File> copying = new ArrayList<>();
        List<File> installed = new ArrayList<>();
        boolean rollbackFailed = false;
        boolean retainRecovery = false;
        try {
            mkdir(incoming);
            mkdir(previous);
            sync.sync(incoming);
            sync.sync(previous);
            sync.sync(staging);
            sync.sync(target.getParentFile());
            // Complete and verify every incoming slot before changing any destination slot.
            for (File slot : slots(source)) {
                File destination = new File(target, slot.getName());
                if ((sameTree(slot, destination)
                        && new File(destination, SaveTransfer.SAVE_FILE).lastModified()
                            >= new File(slot, SaveTransfer.SAVE_FILE).lastModified())
                        || (keepDestination && snapshot.conflicts.contains(slot.getName()))) continue;
                File staged = new File(incoming, slot.getName());
                copyTree(slot, staged, check, sync);
                if (!sameTree(slot, staged)) throw new IOException("Save copy verification failed: " + slot);
                copying.add(slot);
            }
            if (!snapshot.fingerprint.equals(inspect(source, target).fingerprint))
                throw new IOException("Saves changed during the copy; try again.");
            for (File slot : copying) {
                File destination = new File(target, slot.getName());
                File backup = new File(previous, slot.getName());
                if (!sourceFingerprints.get(slot.getName()).equals(fingerprint(slot)))
                    throw new IOException("Source save changed during the copy: " + slot);
                if (destination.exists() && !destination.renameTo(backup))
                    throw new IOException("Cannot back up " + destination);
                // Include this slot in rollback even if the second rename fails.
                installed.add(slot);
                // A sync client can update a later slot while an earlier one is being
                // published. Check the file actually moved aside, not a stale path check.
                if (!targetFingerprints.get(slot.getName()).equals(fingerprint(backup))
                        || !sourceFingerprints.get(slot.getName()).equals(fingerprint(slot)))
                    throw new IOException("Save changed before replacement: " + destination);
                sync.sync(previous);
                sync.sync(target);
                if (!new File(incoming, slot.getName()).renameTo(destination))
                    throw new IOException("Cannot install " + destination);
                sync.sync(incoming);
                sync.sync(target);
            }
        } catch (IOException | RuntimeException failure) {
            for (int i = installed.size() - 1; i >= 0; --i) {
                String name = installed.get(i).getName();
                File destination = new File(target, name);
                File backup = new File(previous, name);
                // Move the incoming copy aside before restoring the old one. Never delete it
                // when restoring fails: both versions remain available in the staging folder.
                if (destination.exists()) {
                    File movedAside = new File(incoming, name);
                    if (!destination.renameTo(movedAside)) {
                        rollbackFailed = true;
                        continue;
                    }
                    // A sync client may have updated an already published slot. Restore
                    // the original target, but retain that newer version for recovery.
                    try {
                        if (!sourceFingerprints.get(name).equals(fingerprint(movedAside)))
                            retainRecovery = true;
                    } catch (IOException | RuntimeException e) { retainRecovery = true; }
                }
                if (backup.exists() && !backup.renameTo(destination)) rollbackFailed = true;
                try { sync.sync(target); sync.sync(previous); sync.sync(incoming); }
                catch (IOException e) { rollbackFailed = true; }
            }
            if (rollbackFailed || retainRecovery)
                throw new IOException("Save copy failed; retained recovery copies in " + staging, failure);
            throw failure;
        } finally {
            if (!rollbackFailed && !retainRecovery) deleteTree(staging);
        }
    }

    /** Tests the operations used by save writes, including FUSE; canWrite() is insufficient. */
    static void probe(File root) throws IOException {
        probe(root, directory -> {});
    }

    static void probe(File root, DirectorySync sync) throws IOException {
        if (root.getParentFile() == null || !root.getParentFile().isDirectory())
            throw new IOException("Game folder is missing: " + root.getParent());
        mkdir(root);
        sync.sync(root.getParentFile());
        File temporary = new File(root, ".lo-save-probe-" + UUID.randomUUID());
        File renamed = new File(root, temporary.getName() + ".renamed");
        byte[] bytes = { 76, 79, 83, 65, 86, 69 };
        try {
            try (FileOutputStream output = new FileOutputStream(renamed)) {
                output.write(new byte[] { 0 });
                output.getFD().sync();
            }
            try (FileOutputStream output = new FileOutputStream(temporary)) {
                output.write(bytes);
                output.getFD().sync();
            }
            // ATOMIC_MOVE maps to rename(2) on Android; failure cannot fall back to a copy.
            Files.move(temporary.toPath(), renamed.toPath(), StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING);
            if (!Arrays.equals(bytes, Files.readAllBytes(renamed.toPath())))
                throw new IOException("Save write/rename verification failed: " + root);
            sync.sync(root);
        } finally {
            temporary.delete();
            renamed.delete();
        }
    }

    private static List<File> slots(File root) throws IOException {
        List<File> result = new ArrayList<>();
        if (!root.exists()) return result;
        for (File file : children(root)) {
            if (Files.isSymbolicLink(file.toPath())) throw new IOException("Linked save path: " + file);
            if (file.isDirectory() && SaveTransfer.validSlotName(file.getName())) result.add(file);
        }
        return result;
    }

    private static File[] children(File root) throws IOException {
        File[] files = root.listFiles();
        if (files == null) throw new IOException("Cannot read " + root);
        Arrays.sort(files);
        return files;
    }

    private static void mkdir(File root) throws IOException {
        if (!root.isDirectory() && !root.mkdirs()) throw new IOException("Cannot create " + root);
        if (Files.isSymbolicLink(root.toPath())) throw new IOException("Linked save path: " + root);
    }

    private static void copyTree(File source, File target, CopyCheck check, DirectorySync sync) throws IOException {
        if (Files.isSymbolicLink(source.toPath())) throw new IOException("Linked save path: " + source);
        check.beforeCopy(source, target);
        if (source.isDirectory()) {
            mkdir(target);
            for (File file : children(source)) copyTree(file, new File(target, file.getName()), check, sync);
            sync.sync(target);
            sync.sync(target.getParentFile());
        } else {
            File temporary = new File(target.getParentFile(), ".copy-" + UUID.randomUUID());
            try {
                try (FileInputStream input = new FileInputStream(source);
                        FileOutputStream output = new FileOutputStream(temporary)) {
                    byte[] buffer = new byte[64 * 1024];
                    for (int n; (n = input.read(buffer)) > 0; ) output.write(buffer, 0, n);
                    if (!temporary.setLastModified(source.lastModified()))
                        throw new IOException("Cannot preserve save time: " + temporary);
                    output.getFD().sync();
                }
                if (!temporary.renameTo(target)) throw new IOException("Cannot rename " + temporary);
                sync.sync(target.getParentFile());
            } finally { temporary.delete(); }
        }
    }

    private static boolean sameTree(File first, File second) throws IOException {
        if (Files.isSymbolicLink(first.toPath()) || Files.isSymbolicLink(second.toPath()))
            throw new IOException("Linked save path: " + first + " or " + second);
        if (!first.exists() || !second.exists() || first.isDirectory() != second.isDirectory()) return false;
        if (first.isDirectory()) {
            File[] a = children(first), b = children(second);
            if (a.length != b.length) return false;
            for (int i = 0; i < a.length; ++i)
                if (!a[i].getName().equals(b[i].getName()) || !sameTree(a[i], b[i])) return false;
            return true;
        }
        if (first.length() != second.length()) return false;
        try (BufferedInputStream a = new BufferedInputStream(new FileInputStream(first));
                BufferedInputStream b = new BufferedInputStream(new FileInputStream(second))) {
            for (int value; (value = a.read()) != -1; ) if (value != b.read()) return false;
            return b.read() == -1;
        }
    }

    private static String fingerprint(File root) throws IOException { return fingerprint(root, true); }

    private static String fingerprint(File root, boolean times) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            hash(root, "", digest, times);
            StringBuilder result = new StringBuilder();
            for (byte value : digest.digest()) result.append(String.format(java.util.Locale.ROOT, "%02x", value & 255));
            return result.toString();
        } catch (NoSuchAlgorithmException e) { throw new AssertionError(e); }
    }

    private static void hash(File file, String relative, MessageDigest digest, boolean times) throws IOException {
        if (Files.isSymbolicLink(file.toPath())) throw new IOException("Linked save path: " + file);
        if (!file.exists()) { digest.update((byte) 0); return; }
        digest.update((file.isDirectory() ? "D:" : "F:").getBytes(StandardCharsets.UTF_8));
        digest.update(relative.getBytes(StandardCharsets.UTF_8));
        digest.update((byte) 0);
        if (file.isDirectory()) {
            for (File child : children(file)) hash(child, relative + "/" + child.getName(), digest, times);
        } else {
            if (times) digest.update(Long.toString(file.lastModified()).getBytes(StandardCharsets.UTF_8));
            digest.update(Long.toString(file.length()).getBytes(StandardCharsets.UTF_8));
            digest.update((byte) 0);
            try (FileInputStream input = new FileInputStream(file)) {
                byte[] buffer = new byte[64 * 1024];
                for (int n; (n = input.read(buffer)) > 0; ) digest.update(buffer, 0, n);
            }
        }
    }

    private static void deleteTree(File root) {
        File[] children = root.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        root.delete();
    }
}
