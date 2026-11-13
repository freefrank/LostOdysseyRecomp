package io.github.freefrank.lostodyssey;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.Process;
import android.system.Os;
import android.system.OsConstants;
import android.widget.Toast;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.HashMap;
import java.util.Map;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import org.json.JSONException;
import org.json.JSONObject;

/** Owns the sampling thread and private files; no network or storage permission. */
final class AndroidTester {
    private static AndroidTester instance;
    static synchronized AndroidTester get(Context context) {
        if (instance == null) instance = new AndroidTester(context.getApplicationContext());
        return instance;
    }
    private final Context context;
    private final File root;
    private final ScheduledExecutorService worker = Executors.newSingleThreadScheduledExecutor();
    private final Handler ui = new Handler(Looper.getMainLooper());
    private TesterTraceFiles trace;
    private File directory;
    private ScheduledFuture<?> timer;
    private long previousTime, previousCpu, lastHeadroom, lastSessionWrite, lastThreads, lastFrequencies, ticksPerSecond;
    private final Map<String, Tick> previousThreads = new HashMap<>();
    private double headroom = Double.NaN;
    private long[] status;
    private static final class Tick {
        final long value, time;
        Tick(long value, long time) { this.value = value; this.time = time; }
    }

    private AndroidTester(Context context) {
        this.context = context;
        root = new File(context.getFilesDir(), "android-diagnostics");
    }
    void start() { worker.execute(this::startOnWorker); }
    void stop() { worker.execute(this::finishOnWorker); }
    private void startOnWorker() {
        try {
            finishOnWorker();
            status = RuntimeActivity.nativeTesterStatus();
            if (status == null || status.length != 9 || status[4] == 0) throw new IOException("native recorder unavailable");
            if (!root.isDirectory() && !root.mkdirs()) throw new IOException("cannot create diagnostics directory");
            directory = new File(root, "session-" + status[4]);
            trace = new TesterTraceFiles(directory);
            TesterTraceFiles.writeText(new File(directory, "device.json"), deviceJson());
            previousTime = previousCpu = lastHeadroom = lastSessionWrite = lastThreads = lastFrequencies = 0;
            previousThreads.clear();
            try { ticksPerSecond = Os.sysconf(OsConstants._SC_CLK_TCK); }
            catch (RuntimeException unavailable) { ticksPerSecond = 0; }
            headroom = Double.NaN;
            saveSession(true);
            removeOlderSessions();
            long period = status[1] == 2 ? 250 : 1000;
            timer = worker.scheduleAtFixedRate(this::sampleOnWorker, 0, period, TimeUnit.MILLISECONDS);
        } catch (IOException | RuntimeException error) {
            RuntimeActivity.nativeTesterStop();
            closeQuietly();
            RuntimeActivity.nativeTesterCollectorFinished();
            showError("Diagnostics could not start: " + error.getMessage());
        }
    }
    private void sampleOnWorker() {
        if (trace == null) return;
        try {
            status = RuntimeActivity.nativeTesterStatus();
            if (status == null || status.length != 9) throw new IOException("native recorder unavailable");
            String frames = RuntimeActivity.nativeTesterDrain();
            if (frames == null) throw new IOException("could not read native trace");
            trace.appendFrames(frames, status[2] != 0);
            long now = System.nanoTime(); // same CLOCK_MONOTONIC basis as the native steady clock on Android
            long cpu = Process.getElapsedCpuTime();
            String percent = previousTime > 0 && now > previousTime && cpu >= previousCpu
                ? String.format(Locale.US, "%.2f", (cpu - previousCpu) * 100000000.0 / (now - previousTime)) : "";
            trace.appendCpu(now + "," + cpu + "," + percent + "," + Runtime.getRuntime().availableProcessors() + "\n");
            previousTime = now; previousCpu = cpu;
            if (status[3] != 0) sampleThermal(now);
            if (status[1] > 0 && (lastThreads == 0 || now - lastThreads >= 1_000_000_000L)) {
                sampleThreads(now); lastThreads = now;
            }
            if (lastFrequencies == 0 || now - lastFrequencies >= 10_000_000_000L) {
                sampleFrequencies(now); lastFrequencies = now;
            }
            if (status[0] == 0 || lastSessionWrite == 0 || now - lastSessionWrite >= 5_000_000_000L) {
                saveSession(status[0] != 0);
                lastSessionWrite = now;
            }
            if (status[0] == 0) finishOnWorker();
        } catch (IOException | RuntimeException error) {
            RuntimeActivity.nativeTesterStop();
            closeQuietly();
            RuntimeActivity.nativeTesterCollectorFinished();
            showError("Diagnostics stopped: " + error.getMessage());
        }
    }
    private void sampleThermal(long now) throws IOException {
        String thermalStatus = "", battery = "", thermalHeadroom = "";
        PowerManager power = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        try {
            if (power != null && Build.VERSION.SDK_INT >= 29) thermalStatus = Integer.toString(power.getCurrentThermalStatus());
        } catch (RuntimeException unavailable) { }
        try {
            if (power != null && Build.VERSION.SDK_INT >= 30 && (lastHeadroom == 0 || now - lastHeadroom >= 10_000_000_000L)) {
                headroom = power.getThermalHeadroom(0); lastHeadroom = now;
                if (Double.isFinite(headroom)) thermalHeadroom = String.format(Locale.US, "%.3f", headroom);
            }
        } catch (RuntimeException unavailable) { lastHeadroom = now; }
        try {
            Intent intent = context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            if (intent != null && intent.hasExtra(BatteryManager.EXTRA_TEMPERATURE))
                battery = String.format(Locale.US, "%.1f", intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) / 10.0);
        } catch (RuntimeException unavailable) { }
        trace.appendThermal(now + "," + thermalStatus + "," + thermalHeadroom + "," + battery + "\n");
    }
    private void sampleThreads(long now) throws IOException {
        File[] tasks = new File("/proc/self/task").listFiles();
        if (tasks == null) return;
        Map<String, Tick> next = new HashMap<>();
        StringBuilder lines = new StringBuilder();
        int examined = 0;
        for (File task : tasks) {
            if (++examined > 256 || !task.getName().matches("[0-9]+")) continue;
            try (BufferedReader in = new BufferedReader(new InputStreamReader(
                    new FileInputStream(new File(task, "stat")), StandardCharsets.UTF_8))) {
                TesterThreadStats stat = TesterThreadStats.parse(in.readLine());
                if (stat == null) continue;
                String key = task.getName() + ":" + stat.startTicks;
                Tick prior = previousThreads.get(key);
                String percent = prior != null && ticksPerSecond > 0 && now > prior.time && stat.ticks >= prior.value
                    ? String.format(Locale.US, "%.2f", (stat.ticks - prior.value) * 100000000000.0 / ticksPerSecond / (now - prior.time)) : "";
                lines.append(now).append(',').append(task.getName()).append(',').append(stat.name).append(',')
                    .append(stat.state).append(',').append(stat.ticks).append(',');
                if (ticksPerSecond > 0) lines.append(ticksPerSecond);
                lines.append(',').append(percent).append('\n');
                next.put(key, new Tick(stat.ticks, now));
            } catch (IOException | SecurityException unavailable) { /* A thread may exit during the scan. */ }
        }
        previousThreads.clear(); previousThreads.putAll(next);
        trace.appendThreads(lines.toString());
    }
    private void sampleFrequencies(long now) throws IOException {
        StringBuilder lines = new StringBuilder();
        int cores = Math.min(64, Runtime.getRuntime().availableProcessors());
        for (int i = 0; i < cores; ++i) {
            File file = new File("/sys/devices/system/cpu/cpu" + i + "/cpufreq/scaling_cur_freq");
            try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.US_ASCII))) {
                String frequency = in.readLine();
                if (frequency != null && frequency.matches("[0-9]+"))
                    lines.append(now).append(',').append(i).append(',').append(frequency).append('\n');
            } catch (IOException | SecurityException unavailable) { /* Unreadable on many phones. */ }
        }
        trace.appendFrequencies(lines.toString());
    }
    private void finishOnWorker() {
        if (timer != null) { timer.cancel(false); timer = null; }
        if (trace == null) return;
        try {
            status = RuntimeActivity.nativeTesterStatus();
            String frames = RuntimeActivity.nativeTesterDrain();
            if (frames == null) throw new IOException("could not read native trace");
            trace.appendFrames(frames, status != null && status[2] != 0);
            saveSession(false);
            trace.close(); trace = null;
            RuntimeActivity.nativeTesterCollectorFinished();
        } catch (IOException | RuntimeException error) {
            closeQuietly();
            RuntimeActivity.nativeTesterCollectorFinished();
            showError("Could not finish diagnostics: " + error.getMessage());
        }
    }
    private void closeQuietly() {
        if (timer != null) { timer.cancel(false); timer = null; }
        if (trace != null) try { trace.close(); } catch (IOException ignored) { }
        trace = null;
    }
    private void showError(String message) { ui.post(() -> Toast.makeText(context, message, Toast.LENGTH_LONG).show()); }
    private String deviceJson() throws IOException {
        try {
            String version = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
            String gpu = RuntimeActivity.nativeTesterGpu();
            String driver = RuntimeActivity.nativeTesterGpuDriver();
            String soc = Build.VERSION.SDK_INT >= 31 ? Build.SOC_MODEL : Build.HARDWARE;
            return new JSONObject().put("manufacturer", Build.MANUFACTURER).put("model", Build.MODEL)
                .put("android_release", Build.VERSION.RELEASE).put("android_api", Build.VERSION.SDK_INT)
                .put("soc", soc)
                .put("abi", Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : JSONObject.NULL)
                .put("app_version", version).put("vulkan_device", gpu == null || gpu.isEmpty() ? JSONObject.NULL : gpu)
                .put("vulkan_driver_raw", driver == null || driver.isEmpty() ? JSONObject.NULL : driver).toString(2) + "\n";
        } catch (Exception error) { throw new IOException("device metadata", error); }
    }
    private JSONObject settings() throws IOException, JSONException {
        Set<String> allowed = new HashSet<>(Arrays.asList("frame_rate", "internal_resolution", "antialiasing",
            "width", "height", "shadow_resolution", "dynamic_shadows", "ambient_occlusion", "anisotropic_filtering",
            "depth_of_field", "bloom", "motion_blur", "vsync", "scaling_quality", "upscaler", "fsr_quality", "culling"));
        JSONObject result = new JSONObject();
        File ini = new File(new File(context.getFilesDir(), "config"), "settings.ini");
        if (!ini.isFile()) return result;
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(ini), StandardCharsets.UTF_8))) {
            for (String line; (line = reader.readLine()) != null;) {
                int equals = line.indexOf('=');
                if (equals > 0 && allowed.contains(line.substring(0, equals).trim()))
                    result.put(line.substring(0, equals).trim(), line.substring(equals + 1).trim());
            }
        }
        return result;
    }
    private void saveSession(boolean recording) throws IOException {
        if (status == null || directory == null) return;
        try {
            JSONObject json = new JSONObject().put("clock", "CLOCK_MONOTONIC nanoseconds")
                .put("started_ns", status[4]).put("duration_ns", status[5])
                .put("recording", recording).put("profile", new String[]{"standard", "advanced", "intensive"}[(int)Math.max(0, Math.min(2, status[1]))])
                .put("automatic_slow_detection", status[2] != 0).put("thermal_monitoring", status[3] != 0)
                .put("target_fps_at_start", status[7]).put("slow_interval_threshold_ns", status[8])
                .put("dropped_frame_samples", status[6]).put("settings", settings())
                .put("gpu_timing_scope", "completed timestamped queue batches; cumulative, not per-frame GPU time")
                .put("xma_timing_scope", "cumulative send/receive wall time, advanced and intensive only; includes scheduling")
                .put("pipeline_timing_scope", "synchronous inline creation attempts only; excludes background jobs and cache hits")
                .put("movie_timing_scope", "guest frame update and guest PTS, not isolated video decode or A/V sync")
                .put("unavailable", "GPU utilization/frequency, CPU/GPU temperature, display latency, actual underruns, isolated CGI decode and A/V sync");
            TesterTraceFiles.writeText(new File(directory, "session.json"), json.toString(2) + "\n");
        } catch (JSONException error) { throw new IOException("session metadata", error); }
    }
    private File[] sessions() {
        File[] files = root.listFiles((dir, name) -> name.startsWith("session-"));
        if (files == null) return new File[0];
        Arrays.sort(files, Comparator.comparingLong(File::lastModified));
        return files;
    }
    private void removeOlderSessions() {
        File[] files = sessions();
        for (int i = 0; i < files.length - 3; ++i) {
            File[] children = files[i].listFiles();
            if (children != null) for (File child : children) if (child.isFile()) child.delete();
            files[i].delete();
        }
    }
    void export(OutputStream out) throws IOException {
        try {
            worker.submit(() -> {
                if (trace != null) sampleOnWorker();
                File[] sessions = sessions();
                if (sessions.length == 0) throw new IOException("No diagnostic session available");
                List<File> logs = PlayerLogs.exportable(PlayerLogs.directory(context));
                TesterTraceFiles.export(sessions[sessions.length - 1], logs, out);
                return null;
            }).get();
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt(); throw new IOException("Export interrupted", error);
        } catch (ExecutionException error) { throw new IOException("Export failed", error.getCause()); }
    }
}
