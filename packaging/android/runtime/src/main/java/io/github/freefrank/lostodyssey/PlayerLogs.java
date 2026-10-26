package io.github.freefrank.lostodyssey;

import android.content.Context;
import android.media.MediaScannerConnection;
import android.os.Build;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/**
 * Runtime logs in {@code Android/data/<package>/files/logs/}, which a player
 * can copy to a PC over USB without root or adb.
 *
 * The native runtime writes there through {@code LO_LOG_DIR}. A PC sees new
 * files over USB (MTP) only after the media scanner has indexed them, and MTP
 * reports the size recorded at that scan, so the folder is scanned again on
 * every launch: the files of a run that crashed are complete after that.
 */
final class PlayerLogs {
    static final String DIRECTORY = "logs";
    static final String README = "README.txt";
    static final int JAVA_CRASHES_KEPT = 5;
    private static final String README_TEXT =
        "Lost Odyssey Recomp logs. For a bug report, attach the newest runtime-*.log,\n"
        + "native-stderr.log and any java-crash-*.txt. Open the app once after a crash\n"
        + "so a PC sees the complete files.\n";
    private static final Pattern LOG_FILE = Pattern.compile(
        "runtime-\\d+\\.log|shader-\\d+\\.jsonl|native-stderr(\\.previous)?\\.log|java-crash-\\d+\\.txt|README\\.txt|tv-display\\.log");
    private static final Pattern JAVA_CRASH = Pattern.compile("java-crash-(\\d+)\\.txt");
    private static final Pattern RUNTIME_LOG = Pattern.compile("runtime-(\\d+)\\.log");
    private static final Pattern EXPORTED = Pattern.compile(
        "native-stderr(\\.previous)?\\.log|java-crash-\\d+\\.txt|tv-display\\.log");
    /** Runtime logs in an export: this run and the two before it, for a crash in the last one. */
    static final int RUNTIME_LOGS_EXPORTED = 3;

    private PlayerLogs() {}

    static File directory(Context context) {
        File external = context.getExternalFilesDir(null);
        return new File(external != null ? external : context.getFilesDir(), DIRECTORY);
    }

    /** One line naming the phone, for the native log's LO_* switch list. */
    static String deviceDescription() {
        String soc = Build.VERSION.SDK_INT >= 31 ? Build.SOC_MANUFACTURER + " " + Build.SOC_MODEL : Build.HARDWARE;
        String abi = Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "?";
        return Build.MANUFACTURER + " " + Build.MODEL + " (" + Build.DEVICE + ") android=" + Build.VERSION.RELEASE
            + " sdk=" + Build.VERSION.SDK_INT + " soc=" + soc + " abi=" + abi;
    }

    /** The log files in {@code directory}, by name; other files and folders are left alone. */
    static List<File> scannable(File directory) {
        File[] files = directory.listFiles();
        List<File> result = new ArrayList<>();
        if (files == null) return result;
        for (File file : files)
            if (file.isFile() && LOG_FILE.matcher(file.getName()).matches()) result.add(file);
        Collections.sort(result, (a, b) -> a.getName().compareTo(b.getName()));
        return result;
    }

    /** Java crash reports beyond the newest {@code keep}, oldest first. */
    static List<File> staleJavaCrashes(File directory, int keep) {
        File[] files = directory.listFiles();
        List<File> crashes = new ArrayList<>();
        if (files != null) for (File file : files)
            if (file.isFile() && JAVA_CRASH.matcher(file.getName()).matches()) crashes.add(file);
        Collections.sort(crashes, (a, b) -> Long.compare(timestamp(a), timestamp(b)));
        return crashes.size() > keep ? crashes.subList(0, crashes.size() - keep) : Collections.emptyList();
    }

    private static long timestamp(File crash) {
        Matcher matcher = JAVA_CRASH.matcher(crash.getName());
        try {
            return matcher.matches() ? Long.parseLong(matcher.group(1)) : 0;
        } catch (NumberFormatException e) {
            return Long.MAX_VALUE;
        }
    }

    /**
     * The files a log export holds: the newest runtime logs, native stderr and the
     * Java crash reports. Shader logs stay out; they are large and rarely needed.
     */
    static List<File> exportable(File directory) {
        File[] files = directory.listFiles();
        List<File> runtime = new ArrayList<>();
        List<File> result = new ArrayList<>();
        if (files == null) return result;
        for (File file : files) {
            if (!file.isFile()) continue;
            if (RUNTIME_LOG.matcher(file.getName()).matches()) runtime.add(file);
            else if (EXPORTED.matcher(file.getName()).matches()) result.add(file);
        }
        Collections.sort(runtime, (a, b) -> Long.compare(runtimeStamp(b), runtimeStamp(a)));
        result.addAll(runtime.subList(0, Math.min(RUNTIME_LOGS_EXPORTED, runtime.size())));
        Collections.sort(result, (a, b) -> a.getName().compareTo(b.getName()));
        return result;
    }

    private static long runtimeStamp(File log) {
        Matcher matcher = RUNTIME_LOG.matcher(log.getName());
        try {
            return matcher.matches() ? Long.parseLong(matcher.group(1)) : 0;
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    /**
     * Writes {@code files} and a device.txt naming the phone and app to one ZIP.
     * The running game flushes every log line, so its current log is complete
     * up to the export.
     */
    static void exportZip(List<File> files, String device, OutputStream output) throws IOException {
        ZipOutputStream zip = new ZipOutputStream(output);
        zip.putNextEntry(new ZipEntry("logs/device.txt"));
        zip.write((device + "\n").getBytes(StandardCharsets.UTF_8));
        zip.closeEntry();
        byte[] buffer = new byte[1 << 16];
        for (File file : files) {
            zip.putNextEntry(new ZipEntry("logs/" + file.getName()));
            try (InputStream input = new FileInputStream(file)) {
                for (int n; (n = input.read(buffer)) > 0; ) zip.write(buffer, 0, n);
            }
            zip.closeEntry();
        }
        zip.finish();
        zip.flush();
    }

    /** Whether settings.ini turns on Settings -> System -> Debug log (info lines in the runtime log). */
    static boolean debugLogEnabled(Context context) {
        File settings = new File(new File(context.getFilesDir(), "config"), "settings.ini");
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                new FileInputStream(settings), StandardCharsets.UTF_8))) {
            // The old debug_log key is ignored, as in settings/config.cpp.
            for (String line; (line = reader.readLine()) != null; )
                if (line.trim().equals("diagnostic_log=0")) return false;
        } catch (IOException ignored) {
            // No settings yet: the default is on.
        }
        return true;
    }

    /** Creates the folder and README, then shows every log file to a PC over USB. */
    static void publish(Context context) {
        File directory = directory(context);
        //noinspection ResultOfMethodCallIgnored
        directory.mkdirs();
        File readme = new File(directory, README);
        if (!readme.exists()) write(readme, README_TEXT);
        List<File> files = scannable(directory);
        if (files.isEmpty()) return;
        String[] paths = new String[files.size()];
        for (int i = 0; i < paths.length; ++i) paths[i] = files.get(i).getAbsolutePath();
        MediaScannerConnection.scanFile(context.getApplicationContext(), paths, null, null);
    }

    /** Writes uncaught Java exceptions to {@code java-crash-<ms>.txt}, then lets Android handle them. */
    static synchronized void installCrashHandler(Context context) {
        Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        if (previous instanceof CrashWriter) return;
        Thread.setDefaultUncaughtExceptionHandler(new CrashWriter(directory(context), previous));
    }

    static String crashReport(Thread thread, Throwable error, String device) {
        StringWriter text = new StringWriter();
        PrintWriter writer = new PrintWriter(text);
        writer.println("device: " + device);
        writer.println("thread: " + thread.getName());
        error.printStackTrace(writer);
        writer.flush();
        return text.toString();
    }

    private static final class CrashWriter implements Thread.UncaughtExceptionHandler {
        private final File directory;
        private final Thread.UncaughtExceptionHandler previous;

        CrashWriter(File directory, Thread.UncaughtExceptionHandler previous) {
            this.directory = directory;
            this.previous = previous;
        }

        @Override
        public void uncaughtException(Thread thread, Throwable error) {
            try {
                //noinspection ResultOfMethodCallIgnored
                directory.mkdirs();
                write(new File(directory, "java-crash-" + System.currentTimeMillis() + ".txt"),
                    crashReport(thread, error, deviceDescription()));
                for (File stale : staleJavaCrashes(directory, JAVA_CRASHES_KEPT))
                    //noinspection ResultOfMethodCallIgnored
                    stale.delete();
            } catch (Throwable ignored) {
                // The report is optional; Android's own handling must still run.
            }
            if (previous != null) previous.uncaughtException(thread, error);
        }
    }

    private static void write(File file, String text) {
        try (OutputStream output = new FileOutputStream(file)) {
            output.write(text.getBytes(StandardCharsets.UTF_8));
        } catch (IOException ignored) {
            // A missing README or report must not stop the game.
        }
    }
}
