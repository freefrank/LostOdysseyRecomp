package io.github.freefrank.lostodyssey;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Standalone JVM test; no Android SDK or emulator. */
public final class AndroidTesterTraceTest {
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        TesterThreadStats parsed = TesterThreadStats.parse("123 (Render (main)) R 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");
        check(parsed != null && parsed.name.equals("Render (main)") && parsed.ticks == 23 && parsed.startTicks == 19, "thread stat parser");
        check(TesterThreadStats.parse("123 malformed") == null, "unavailable thread stat");
        File dir = Files.createTempDirectory("lo-tester").toFile();
        TesterTraceFiles writer = new TesterTraceFiles(dir);
        TesterTraceFiles.writeText(new File(dir, "device.json"), "{}\n");
        TesterTraceFiles.writeText(new File(dir, "session.json"), "{}\n");
        for (int i = 1; i <= 50; ++i) {
            long time = i * 1_000_000_000L;
            writer.appendFrames(time + ",1000000000,1,1,1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1," + (i == 35 ? 1 : 0) + "\n", true);
        }
        writer.appendCpu("1,2,3,4\n");
        writer.appendThermal("1,2,,35.0\n");
        writer.close();
        File log = new File(dir, "runtime-123.log");
        Files.writeString(log.toPath(), "path /storage/emulated/0/private/file.txt\n");
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        TesterTraceFiles.export(dir, Collections.singletonList(log), bytes);
        Map<String,String> files = new HashMap<>();
        try (ZipInputStream zip = new ZipInputStream(new ByteArrayInputStream(bytes.toByteArray()))) {
            for (ZipEntry entry; (entry = zip.getNextEntry()) != null;)
                files.put(entry.getName(), new String(zip.readAllBytes(), StandardCharsets.UTF_8));
        }
        check(files.get("frame_timing.csv").startsWith(TesterTraceFiles.FRAME_HEADER), "frame header");
        check(files.get("summary.json").contains("\"p95_interval_ns\":1000000000"), "p95");
        String window = files.get("events/event-1.csv");
        check(window != null && window.contains("\n5000000000,") && window.contains("\n45000000000,"), "before/after window");
        check(!window.contains("\n4000000000,") && !window.contains("\n46000000000,"), "bounded event window");
        check(!files.get("runtime.log").contains("/storage/emulated/0/"), "redact shared path");
        check(files.get("thermal.csv").contains("1,2,,35.0"), "unavailable thermal data remains empty");
        System.out.println("PASS: Android tester trace export and event window (JVM)");
    }
}
