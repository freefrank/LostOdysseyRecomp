package io.github.freefrank.lostodyssey;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class GameStorageTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();

    @Test
    public void createsTheGameFolderAndReadmeOnEveryVolumeOnce() throws IOException {
        File internal = temporary.newFolder("internal", "files");
        File card = temporary.newFolder("card", "files");
        List<String> written = GameStorage.createFolders(Arrays.asList(internal, card));
        assertEquals(2, written.size());
        for (File volume : Arrays.asList(internal, card)) {
            assertTrue(new File(volume, "game/" + GameStorage.README).isFile());
            // Empty disc folders would look like a damaged installation.
            assertFalse(new File(volume, "game/disc1").exists());
        }
        assertTrue(GameStorage.createFolders(Arrays.asList(internal, card)).isEmpty());
    }

    @Test
    public void picksTheVolumeThatHoldsDisc1() throws IOException {
        File internal = temporary.newFolder("internal", "files");
        File card = temporary.newFolder("card", "files");
        List<File> volumes = Arrays.asList(internal, card);
        GameStorage.createFolders(volumes);
        assertEquals(new File(internal, "game/disc1"), GameStorage.selectDisc1(null, volumes, internal));
        assertTrue(new File(card, "game/disc1").mkdirs());
        assertTrue(new File(card, "game/disc1/default.xex").createNewFile());
        assertEquals(new File(card, "game/disc1"), GameStorage.selectDisc1(null, volumes, internal));
        assertTrue(new File(internal, "game/disc1").mkdirs());
        assertTrue(new File(internal, "game/disc1/default.xex").createNewFile());
        assertEquals(new File(internal, "game/disc1"), GameStorage.selectDisc1(null, volumes, internal));
    }

    @Test
    public void volumeRootIsFourLevelsAboveTheAppFolder() {
        assertEquals(new File("/storage/1234-ABCD"),
            GameStorage.volumeRoot(new File("/storage/1234-ABCD/Android/data/io.github.freefrank.lostodyssey/files")));
    }

    @Test
    public void fallsBackToPrivateStorageWithoutVolumes() {
        File fallback = temporary.getRoot();
        assertEquals(new File(fallback, "game/disc1"), GameStorage.selectDisc1(null, Collections.emptyList(), fallback));
    }

    @Test
    public void customFolderWinsOnlyWhenItHoldsTheGame() throws IOException {
        File internal = temporary.newFolder("internal", "files");
        File custom = temporary.newFolder("Games", "LostOdyssey");
        List<File> volumes = Collections.singletonList(internal);
        GameStorage.createFolders(volumes);
        assertTrue(new File(internal, "game/disc1").mkdirs());
        assertTrue(new File(internal, "game/disc1/default.xex").createNewFile());
        assertEquals(new File(internal, "game/disc1"), GameStorage.selectDisc1(custom, volumes, internal));
        assertTrue(new File(custom, "disc1").mkdirs());
        assertTrue(new File(custom, "disc1/default.xex").createNewFile());
        assertEquals(new File(custom, "disc1"), GameStorage.selectDisc1(custom, volumes, internal));
    }

    @Test
    public void importsIntoTheCustomFolderElseTheInternalAppFolder() throws IOException {
        File internal = temporary.newFolder("internal", "files");
        File card = temporary.newFolder("card", "files");
        File custom = temporary.newFolder("Games");
        List<File> volumes = Arrays.asList(internal, card);
        assertEquals(new File(internal, "game"), GameStorage.importRoot(null, volumes, temporary.getRoot()));
        assertEquals(custom, GameStorage.importRoot(custom, volumes, temporary.getRoot()));
        assertEquals(new File(temporary.getRoot(), "game"),
            GameStorage.importRoot(null, Collections.emptyList(), temporary.getRoot()));
    }

    @Test
    public void adoptsANewImportOutsideTheAppFolders() throws IOException {
        File internal = temporary.newFolder("internal", "files");
        List<File> volumes = Collections.singletonList(internal);
        File record = new File(temporary.getRoot(), "game-path.txt");
        File outside = temporary.newFolder("Games", "LO");
        assertTrue(new File(outside, "disc1").mkdirs());
        assertTrue(new File(outside, "disc1/default.xex").createNewFile());
        java.nio.file.Files.write(record.toPath(), (outside.getAbsolutePath() + "\n").getBytes("UTF-8"));
        assertEquals(outside, GameStorage.importedRootToAdopt(record, 0, volumes));
        // Already seen: the user may have changed the folder since.
        assertEquals(null, GameStorage.importedRootToAdopt(record, record.lastModified(), volumes));
        // The app folder is found without a custom folder.
        File appGame = new File(internal, "game");
        assertTrue(new File(appGame, "disc1").mkdirs());
        assertTrue(new File(appGame, "disc1/default.xex").createNewFile());
        java.nio.file.Files.write(record.toPath(), appGame.getAbsolutePath().getBytes("UTF-8"));
        assertEquals(null, GameStorage.importedRootToAdopt(record, 0, volumes));
        assertEquals(null, GameStorage.importedRootToAdopt(new File(temporary.getRoot(), "missing"), 0, volumes));
    }

    @Test
    public void normalizesThePickedFolder() throws IOException {
        File game = temporary.newFolder("LO");
        File disc1 = new File(game, "disc1");
        assertTrue(disc1.mkdirs());
        assertEquals(null, GameStorage.normalizeGameRoot(game));
        assertTrue(new File(disc1, "default.xex").createNewFile());
        assertEquals(game, GameStorage.normalizeGameRoot(game));
        assertEquals(game, GameStorage.normalizeGameRoot(disc1));
        assertEquals(null, GameStorage.normalizeGameRoot(temporary.getRoot()));
    }

    @Test
    public void mapsPickerTreeIdsToPaths() {
        File primary = new File("/storage/emulated/0");
        assertEquals(new File(primary, "Games/LO"), GameStorage.pathForTreeDocumentId("primary:Games/LO", primary));
        assertEquals(primary, GameStorage.pathForTreeDocumentId("primary:", primary));
        assertEquals(new File("/storage/1234-ABCD/LO"), GameStorage.pathForTreeDocumentId("1234-ABCD:LO", primary));
        assertEquals(new File("/storage/emulated/0/Download/LO"),
            GameStorage.pathForTreeDocumentId("raw:/storage/emulated/0/Download/LO", primary));
        assertEquals(null, GameStorage.pathForTreeDocumentId("raw", primary));
        assertEquals(null, GameStorage.pathForTreeDocumentId(null, primary));
    }
}
