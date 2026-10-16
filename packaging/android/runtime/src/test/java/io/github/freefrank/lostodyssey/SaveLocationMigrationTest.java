package io.github.freefrank.lostodyssey;

import static org.junit.Assert.*;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class SaveLocationMigrationTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();
    private static final long TIME = 1_790_000_000_000L;

    private File root(String name) throws IOException { return temporary.newFolder(name, "save"); }

    private File save(File root, String slot, String contents, long time) throws IOException {
        File folder = new File(root, slot);
        if (!folder.isDirectory()) assertTrue(folder.mkdirs());
        File file = new File(folder, "save.bin");
        Files.write(file.toPath(), contents.getBytes(StandardCharsets.UTF_8));
        assertTrue(file.setLastModified(time));
        return file;
    }

    private String read(File root, String slot) throws IOException {
        return new String(Files.readAllBytes(new File(root, slot + "/save.bin").toPath()), StandardCharsets.UTF_8);
    }

    @Test public void copiesWindowsLayoutBothWaysAndKeepsEverySourceFile() throws IOException {
        File privateRoot = root("private"), shared = root("game");
        save(privateRoot, "user00", "first", TIME);
        Files.write(new File(privateRoot, "user00/.lo-content").toPath(), new byte[] { 1, 2, 3 });
        Files.write(new File(privateRoot, "user00/__thumbnail.png").toPath(), new byte[] { 4, 5 });
        SaveLocationMigration.migrate(privateRoot, shared, null, false);
        assertEquals("first", read(privateRoot, "user00"));
        assertEquals("first", read(shared, "user00"));
        assertArrayEquals(new byte[] { 1, 2, 3 }, Files.readAllBytes(new File(shared, "user00/.lo-content").toPath()));
        assertArrayEquals(new byte[] { 4, 5 }, Files.readAllBytes(new File(shared, "user00/__thumbnail.png").toPath()));
        assertEquals(TIME, new File(shared, "user00/save.bin").lastModified());
        save(shared, "user00", "played", TIME + 10000);
        SaveLocationMigration.migrate(shared, privateRoot, null, false);
        assertEquals("played", read(shared, "user00"));
        assertEquals("played", read(privateRoot, "user00"));
    }

    @Test public void newerDestinationRequiresSnapshotApproval() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "old", TIME);
        save(target, "user00", "new", TIME + 10000);
        try {
            SaveLocationMigration.migrate(source, target, null, false);
            fail("overwrote a newer save");
        } catch (SaveLocationMigration.ConflictException e) {
            assertEquals("user00", e.snapshot.conflicts.get(0));
            SaveLocationMigration.migrate(source, target, e.snapshot.fingerprint, false);
        }
        assertEquals("old", read(source, "user00"));
        assertEquals("old", read(target, "user00"));
    }

    @Test public void equalTimeWithDifferentContentsConflictsAndCanKeepDestination() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "phone", TIME);
        save(target, "user00", "pc", TIME);
        save(source, "user01", "new slot", TIME);
        SaveLocationMigration.Snapshot snapshot = SaveLocationMigration.inspect(source, target);
        assertEquals(1, snapshot.conflicts.size());
        SaveLocationMigration.migrate(source, target, snapshot.fingerprint, true);
        assertEquals("pc", read(target, "user00"));
        assertEquals("phone", read(source, "user00"));
        assertEquals("new slot", read(target, "user01"));
    }

    @Test public void changedSourceOrDestinationInvalidatesApproval() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "phone", TIME);
        save(target, "user00", "pc", TIME + 10000);
        String approved = SaveLocationMigration.inspect(source, target).fingerprint;
        save(target, "user00", "changed pc", TIME + 10000);
        try {
            SaveLocationMigration.migrate(source, target, approved, false);
            fail("accepted changed destination");
        } catch (SaveLocationMigration.ConflictException expected) { }
        approved = SaveLocationMigration.inspect(source, target).fingerprint;
        save(source, "user00", "changed phone", TIME);
        try {
            SaveLocationMigration.migrate(source, target, approved, false);
            fail("accepted changed source");
        } catch (SaveLocationMigration.ConflictException expected) { }
        assertEquals("changed pc", read(target, "user00"));
    }

    @Test public void partialCopyChangesNoDestinationSlot() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "new zero", TIME + 10000);
        save(source, "user01", "new one", TIME + 10000);
        save(target, "user00", "old zero", TIME);
        save(target, "user01", "old one", TIME);
        try {
            SaveLocationMigration.migrate(source, target, null, false, (from, to) -> {
                if (from.getParentFile().getName().equals("user01")) throw new IOException("disk full");
            });
            fail("accepted partial copy");
        } catch (IOException expected) { }
        assertEquals("old zero", read(target, "user00"));
        assertEquals("old one", read(target, "user01"));
        assertEquals("new zero", read(source, "user00"));
        assertEquals(1, target.getParentFile().list().length);
    }

    @Test public void publicationFailureRestoresPreviousSlots() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "new", TIME + 10000);
        save(target, "user00", "old", TIME);
        boolean[] failed = { false };
        try {
            SaveLocationMigration.migrate(source, target, null, false, (from, to) -> {}, directory -> {
                if (!failed[0] && directory.equals(target) && new File(target, "user00/save.bin").isFile()
                        && "new".equals(read(target, "user00"))) {
                    failed[0] = true;
                    throw new IOException("directory sync failed");
                }
            });
            fail("accepted publication failure");
        } catch (IOException expected) { }
        assertTrue(failed[0]);
        assertEquals("old", read(target, "user00"));
        assertEquals("new", read(source, "user00"));
    }

    @Test public void laterDestinationChangedDuringPublicationIsRetainedAndEarlierSlotsRollBack() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "new zero", TIME + 10000);
        save(source, "user01", "new one", TIME + 10000);
        save(target, "user00", "old zero", TIME);
        save(target, "user01", "old one", TIME);
        boolean[] changed = { false };
        try {
            SaveLocationMigration.migrate(source, target, null, false, (from, to) -> {}, directory -> {
                if (!changed[0] && directory.equals(target) && new File(target, "user00/save.bin").isFile()
                        && "new zero".equals(read(target, "user00"))) {
                    changed[0] = true;
                    save(target, "user01", "newer external sync", TIME + 20000);
                }
            });
            fail("overwrote concurrently updated destination");
        } catch (IOException expected) { }
        assertTrue(changed[0]);
        assertEquals("old zero", read(target, "user00"));
        assertEquals("newer external sync", read(target, "user01"));
        assertEquals("new one", read(source, "user01"));
    }

    @Test public void externalUpdateToPublishedSlotSurvivesRollbackInRecoveryFolder() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "new zero", TIME + 10000);
        save(source, "user01", "new one", TIME + 10000);
        save(target, "user00", "old zero", TIME);
        save(target, "user01", "old one", TIME);
        boolean[] changed = { false };
        String[] error = { null };
        try {
            SaveLocationMigration.migrate(source, target, null, false, (from, to) -> {}, directory -> {
                if (!changed[0] && directory.equals(target) && new File(target, "user00/save.bin").isFile()
                        && "new zero".equals(read(target, "user00"))) {
                    changed[0] = true;
                    save(target, "user00", "newer synced zero", TIME + 20000);
                    throw new IOException("publication interrupted after external update");
                }
            });
            fail("accepted interrupted publication");
        } catch (IOException expected) { error[0] = expected.getMessage(); }
        assertTrue(changed[0]);
        assertEquals("old zero", read(target, "user00"));
        assertEquals("old one", read(target, "user01"));
        assertEquals("new zero", read(source, "user00"));
        assertEquals("new one", read(source, "user01"));
        File[] recovery = target.getParentFile().listFiles(file -> file.getName().startsWith(".save-location-"));
        assertNotNull(recovery);
        assertEquals(1, recovery.length);
        assertEquals("newer synced zero", read(new File(recovery[0], "incoming"), "user00"));
        assertTrue(error[0].contains(recovery[0].getAbsolutePath()));
    }

    @Test public void laterSourceChangedDuringPublicationPreventsSwitchingToStaleCopy() throws IOException {
        File source = root("source"), target = root("target");
        save(source, "user00", "new zero", TIME + 10000);
        save(source, "user01", "new one", TIME + 10000);
        save(target, "user00", "old zero", TIME);
        save(target, "user01", "old one", TIME);
        boolean[] changed = { false };
        try {
            SaveLocationMigration.migrate(source, target, null, false, (from, to) -> {}, directory -> {
                if (!changed[0] && directory.equals(target) && new File(target, "user00/save.bin").isFile()
                        && "new zero".equals(read(target, "user00"))) {
                    changed[0] = true;
                    save(source, "user01", "source changed", TIME + 20000);
                }
            });
            fail("published a stale source");
        } catch (IOException expected) { }
        assertTrue(changed[0]);
        assertEquals("old zero", read(target, "user00"));
        assertEquals("old one", read(target, "user01"));
        assertEquals("source changed", read(source, "user01"));
    }

    @Test public void unwritableOrMissingFolderDoesNotCreateAnotherGameRoot() throws IOException {
        File source = root("source");
        save(source, "user00", "safe", TIME);
        File blocker = temporary.newFile("read-only-volume");
        File target = new File(blocker, "save");
        try {
            SaveLocationMigration.migrate(source, target, null, false);
            fail("accepted unwritable folder");
        } catch (IOException expected) { }
        File removed = new File(temporary.getRoot(), "removed-card/game/save");
        try {
            SaveLocationMigration.migrate(source, removed, null, false);
            fail("recreated removed game folder");
        } catch (IOException expected) { }
        assertFalse(removed.getParentFile().exists());
        assertEquals("safe", read(source, "user00"));
    }

    @Test public void probeRejectsUnsupportedDirectorySync() throws IOException {
        File target = root("target");
        try {
            SaveLocationMigration.probe(target, directory -> { throw new IOException("fsync unsupported"); });
            fail("accepted unsupported durability");
        } catch (IOException expected) { }
        assertEquals(0, target.list().length);
    }

    @Test public void successfulSwitchCommitsOnlyAfterVerifiedCopy() throws IOException {
        File privateRoot = root("private"), shared = root("game");
        save(privateRoot, "user00", "first", TIME);
        SaveLocationState state = new SaveLocationState();
        state.pendingRoot = shared.getAbsolutePath();
        assertEquals(shared, state.prepare(privateRoot).root);
        assertEquals(shared.getAbsolutePath(), state.sharedRoot);
        assertNull(state.pendingRoot);
        save(shared, "user00", "played", TIME + 10000);
        state.pendingRoot = "";
        assertEquals(privateRoot, state.prepare(privateRoot).root);
        assertNull(state.sharedRoot);
        assertNull(state.pendingRoot);
        assertEquals("played", read(privateRoot, "user00"));
        assertEquals("played", read(shared, "user00"));
    }

    @Test public void sharedWriteModeFollowsTheActualRunRoot() throws IOException {
        File privateRoot = root("private"), shared = root("game");
        SaveLocationState state = new SaveLocationState();
        assertFalse(state.prepare(privateRoot).usesSharedStorage(privateRoot));
        state.pendingRoot = shared.getAbsolutePath();
        assertTrue(state.prepare(privateRoot).usesSharedStorage(privateRoot));
        state.pendingRoot = "";
        assertFalse(state.prepare(privateRoot).usesSharedStorage(privateRoot));
    }

    @Test public void failedEnableRemainsPrivateAndFailedDisableRemainsShared() throws IOException {
        File privateRoot = root("private"), shared = root("game");
        save(privateRoot, "user00", "private newer", TIME + 10000);
        save(shared, "user00", "shared old", TIME);
        SaveLocationState enabling = new SaveLocationState();
        enabling.pendingRoot = new File(temporary.getRoot(), "missing/save").getAbsolutePath();
        assertEquals(privateRoot, enabling.prepare(privateRoot).root);
        assertNull(enabling.sharedRoot);
        assertNotNull(enabling.pendingRoot);

        SaveLocationState disabling = new SaveLocationState();
        disabling.sharedRoot = shared.getAbsolutePath();
        disabling.pendingRoot = "";
        SaveLocationState.Run run = disabling.prepare(privateRoot);
        assertTrue(run.conflict);
        assertEquals(shared, run.root);
        assertTrue(run.usesSharedStorage(privateRoot));
        assertEquals(shared.getAbsolutePath(), disabling.sharedRoot);
        assertEquals("", disabling.pendingRoot);
        assertEquals("private newer", read(privateRoot, "user00"));
    }

    @Test public void conflictingEnableDoesNotCommitSetting() throws IOException {
        File privateRoot = root("private"), shared = root("game");
        save(privateRoot, "user00", "private", TIME);
        save(shared, "user00", "pc newer", TIME + 10000);
        SaveLocationState state = new SaveLocationState();
        state.pendingRoot = shared.getAbsolutePath();
        SaveLocationState.Run run = state.prepare(privateRoot);
        assertEquals(privateRoot, run.root);
        assertTrue(run.conflict);
        assertFalse(run.usesSharedStorage(privateRoot));
        assertNull(state.sharedRoot);
        assertNotNull(state.pendingRoot);
        assertEquals("pc newer", read(shared, "user00"));
    }

    @Test public void missingSharedFolderUsesPrivateAndMergesFallbackProgressAfterReturn() throws IOException {
        File privateRoot = root("private");
        save(privateRoot, "user00", "backup", TIME);
        File game = new File(temporary.getRoot(), "card/game");
        File shared = new File(game, "save");
        SaveLocationState state = new SaveLocationState();
        state.sharedRoot = shared.getAbsolutePath();
        SaveLocationState.Run fallback = state.prepare(privateRoot);
        assertTrue(fallback.fallback);
        assertFalse(fallback.usesSharedStorage(privateRoot));
        assertTrue(state.fallbackDirty);
        assertEquals(privateRoot, fallback.root);
        assertEquals(shared.getAbsolutePath(), state.sharedRoot);
        assertFalse(game.exists());
        assertTrue(game.mkdirs());
        save(shared, "user00", "previous shared", TIME);
        save(privateRoot, "user00", "fallback progress", TIME + 10000);
        SaveLocationState.Run returned = state.prepare(privateRoot);
        assertEquals(shared, returned.root);
        assertTrue(returned.usesSharedStorage(privateRoot));
        assertFalse(state.fallbackDirty);
        assertEquals("fallback progress", read(shared, "user00"));
        assertEquals("fallback progress", read(privateRoot, "user00"));
    }

    @Test public void recoveryConflictKeepsUsingFallbackUntilExplicitChoice() throws IOException {
        File privateRoot = root("private"), shared = root("game");
        save(privateRoot, "user00", "fallback progress", TIME);
        save(shared, "user00", "pc newer", TIME + 10000);
        SaveLocationState state = new SaveLocationState();
        state.sharedRoot = shared.getAbsolutePath();
        state.fallbackDirty = true;
        SaveLocationState.Run run = state.prepare(privateRoot);
        assertTrue(run.conflict);
        assertEquals(privateRoot, run.root);
        assertFalse(run.usesSharedStorage(privateRoot));
        assertTrue(state.fallbackDirty);
        state.approvedFingerprint = SaveLocationMigration.inspect(privateRoot, shared).fingerprint;
        state.keepDestination = true;
        assertEquals(shared, state.prepare(privateRoot).root);
        assertFalse(state.fallbackDirty);
        assertEquals("pc newer", read(shared, "user00"));
        assertEquals("fallback progress", read(privateRoot, "user00"));
    }

    @Test public void cardRemovedDuringSwitchOffFallsBackWithoutCommittingSwitch() throws IOException {
        File privateRoot = root("private"), shared = root("game");
        save(privateRoot, "user00", "backup", TIME);
        save(shared, "user00", "shared progress", TIME + 10000);
        SaveLocationState state = new SaveLocationState();
        state.sharedRoot = shared.getAbsolutePath();
        state.pendingRoot = "";
        boolean[] removed = { false };
        SaveLocationState.Run run = state.prepare(privateRoot, directory -> {
            if (!removed[0] && directory.equals(privateRoot)) {
                removed[0] = true;
                assertTrue(shared.getParentFile().renameTo(new File(temporary.getRoot(), "removed-card")));
            }
        });
        assertTrue(removed[0]);
        assertTrue(run.fallback);
        assertEquals(privateRoot, run.root);
        assertTrue(state.fallbackDirty);
        assertEquals(shared.getAbsolutePath(), state.sharedRoot);
        assertEquals("", state.pendingRoot);
        assertEquals("backup", read(privateRoot, "user00"));
        assertEquals("shared progress", read(new File(temporary.getRoot(), "removed-card/save"), "user00"));
    }
}
