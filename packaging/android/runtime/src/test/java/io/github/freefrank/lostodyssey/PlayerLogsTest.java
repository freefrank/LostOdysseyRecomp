package io.github.freefrank.lostodyssey;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class PlayerLogsTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();

    private static List<String> names(List<File> files) {
        List<String> result = new ArrayList<>();
        for (File file : files) result.add(file.getName());
        return result;
    }

    @Test
    public void scannableListsOnlyLogFiles() throws IOException {
        File logs = temporary.newFolder("logs");
        for (String name : new String[] { "runtime-1700000000000000.log", "shader-1700000000000000.jsonl",
                "native-stderr.log", "native-stderr.previous.log", "java-crash-1700000000000.txt", "README.txt",
                "tv-display.log", "runtime-.log", "notes.txt", "runtime-12.log.tmp" })
            assertTrue(new File(logs, name).createNewFile());
        assertTrue(new File(logs, "runtime-5.log").mkdir());

        assertEquals(Arrays.asList("README.txt", "java-crash-1700000000000.txt",
                "native-stderr.log", "native-stderr.previous.log", "runtime-1700000000000000.log",
                "shader-1700000000000000.jsonl", "tv-display.log"),
            names(PlayerLogs.scannable(logs)));
    }

    @Test
    public void scannableOfMissingFolderIsEmpty() {
        assertTrue(PlayerLogs.scannable(new File(temporary.getRoot(), "absent")).isEmpty());
    }

    @Test
    public void tvDiagnosticsAreIncludedInPlayerExports() throws IOException {
        File logs = temporary.newFolder("tv-export");
        assertTrue(new File(logs, "tv-display.log").createNewFile());
        assertTrue(new File(logs, "notes.txt").createNewFile());
        assertEquals(Arrays.asList("tv-display.log"), names(PlayerLogs.exportable(logs)));
    }

    @Test
    public void staleJavaCrashesKeepsNewestByTimestamp() throws IOException {
        File logs = temporary.newFolder("logs");
        // 900 sorts after 1000 by name; the numeric timestamp must decide.
        for (String stamp : new String[] { "900", "1000", "1100", "1200" })
            assertTrue(new File(logs, "java-crash-" + stamp + ".txt").createNewFile());
        assertTrue(new File(logs, "runtime-1.log").createNewFile());

        assertEquals(Arrays.asList("java-crash-900.txt", "java-crash-1000.txt"),
            names(PlayerLogs.staleJavaCrashes(logs, 2)));
        assertTrue(PlayerLogs.staleJavaCrashes(logs, 4).isEmpty());
    }

    @Test
    public void crashReportNamesDeviceThreadAndCause() {
        String report = PlayerLogs.crashReport(new Thread("SDLThread"),
            new IllegalStateException("boom", new UnsatisfiedLinkError("libmain.so")), "AYN Thor");
        assertTrue(report.contains("device: AYN Thor"));
        assertTrue(report.contains("thread: SDLThread"));
        assertTrue(report.contains("IllegalStateException: boom"));
        assertTrue(report.contains("Caused by: java.lang.UnsatisfiedLinkError: libmain.so"));
    }
}
