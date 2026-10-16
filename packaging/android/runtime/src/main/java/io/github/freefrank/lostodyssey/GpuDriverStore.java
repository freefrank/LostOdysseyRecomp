package io.github.freefrank.lostodyssey;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Installed custom Vulkan drivers (Mesa Turnip and PanVK packages) and the
 * player's choice.
 *
 * Each package is extracted into its own directory under files/gpu_driver/, so
 * switching drivers only changes which directory the native loader is pointed
 * at. The package format is the libadrenotools one shared with other emulators:
 * a flat zip holding meta.json plus the driver .so named by "libraryName".
 */
final class GpuDriverStore {
    static final String PREFERENCES = "gpu_driver";
    static final String KEY_SELECTED = "selected";
    static final String KEY_CHOICE_MADE = "choice_made";
    static final String KEY_BOOT_PENDING = "boot_pending";
    /** Per-package loading mode, suffixed with the package id. */
    static final String KEY_LOAD_MODE = "load_mode:";
    static final String SYSTEM_DRIVER = "";
    static final String SYSTEM_DRIVER_NAME = "System GPU driver";
    static final String DIRECTORY = "gpu_driver";
    static final String META_FILE = "meta.json";
    static final long MAX_META_BYTES = 500_000L;
    static final long MAX_LIBRARY_BYTES = 256L << 20;
    /** The driver the menu text fix was verified with on an Adreno 750 (2026-10-03). */
    static final String VERIFIED_DRIVER = "KIMCHI Turnip v26.0.0 R8";
    /** meta.json "backend" of the Mesa PanVK packages for Mali (panvk-kbase-android). */
    static final String BACKEND_PANVK = "panvk-kbase";
    /**
     * PanVK packages declare Android 15 (API 35), but their library loads from
     * Android 13 on; the maintainer chose to allow those versions without
     * promising them (the GPU driver page says so).
     */
    static final int PANVK_LOWEST_API = 33;

    /** How the native runtime loads a package (LO_VK_CUSTOM_DRIVER_MODE). */
    static final String LOAD_ADRENOTOOLS = "adrenotools";
    static final String LOAD_ICD = "icd";

    /** The GPU family, which decides the driver sources and the page's wording. */
    enum Vendor { ADRENO, MALI, OTHER }

    /** Parsed meta.json of one package. */
    static final class Metadata {
        final String name, description, author, vendor, driverVersion, libraryName;
        final String packageVersion, abi, backend;
        final int minApi;

        Metadata(String name, String description, String author, String vendor,
                 String driverVersion, int minApi, String libraryName) {
            this(name, description, author, vendor, driverVersion, minApi, libraryName, "", "", "");
        }

        Metadata(String name, String description, String author, String vendor, String driverVersion,
                 int minApi, String libraryName, String packageVersion, String abi, String backend) {
            this.name = name;
            this.description = description;
            this.author = author;
            this.vendor = vendor;
            this.driverVersion = driverVersion;
            this.minApi = minApi;
            this.libraryName = libraryName;
            this.packageVersion = packageVersion;
            this.abi = abi;
            this.backend = backend;
        }

        /** Requires name and libraryName; the descriptive fields are optional. */
        static Metadata parse(String json) throws JSONException {
            JSONObject object = new JSONObject(json);
            String name = object.getString("name").trim();
            String library = object.getString("libraryName").trim();
            if (name.isEmpty()) throw new JSONException("empty name");
            if (library.isEmpty() || library.contains("/") || library.contains("\\")
                    || library.startsWith(".")) {
                throw new JSONException("invalid libraryName");
            }
            return new Metadata(name, object.optString("description", ""),
                object.optString("author", ""), object.optString("vendor", ""),
                object.optString("driverVersion", ""), object.optInt("minApi", 0), library,
                object.optString("packageVersion", "").trim(), object.optString("abi", "").trim(),
                object.optString("backend", "").trim());
        }

        boolean isPanVk() { return BACKEND_PANVK.equals(backend); }

        /** PanVK presents through its own Android surface; Turnip needs the system loader. */
        String defaultLoadMode() { return isPanVk() ? LOAD_ICD : LOAD_ADRENOTOOLS; }

        /** A glibc (Linux) build, e.g. a PanVK "-EMULATOR.zip", cannot load on Android. */
        boolean isForOtherSystem() {
            return !abi.isEmpty() && !abi.toLowerCase(Locale.ROOT).contains("android");
        }

        /** The Android API level installing needs; PanVK's declared 35 is relaxed to 33. */
        int requiredApi() { return isPanVk() ? Math.min(minApi, PANVK_LOWEST_API) : minApi; }

        /**
         * The package version when it says more than a format counter: PanVK's
         * "0.1.0-rc3" (every PanVK build has the same name), not Turnip's "1".
         */
        String displayVersion() {
            return packageVersion.isEmpty() || packageVersion.matches("\\d+") ? "" : packageVersion;
        }

        String summary() {
            StringBuilder text = new StringBuilder(displayVersion());
            if (!driverVersion.isEmpty()) text.append(text.length() > 0 ? " · " : "").append(driverVersion);
            if (!author.isEmpty()) text.append(text.length() > 0 ? " · " : "").append(author);
            if (!description.isEmpty()) text.append(text.length() > 0 ? "\n" : "").append(description);
            return text.toString();
        }
    }

    /** One extracted package on disk. */
    static final class Installed {
        final File directory;
        final Metadata metadata;

        Installed(File directory, Metadata metadata) {
            this.directory = directory;
            this.metadata = metadata;
        }

        String id() { return directory.getName(); }
        File library() { return new File(directory, metadata.libraryName); }
    }

    /** Thrown for a package that is not a usable driver. */
    static final class InvalidPackageException extends IOException {
        InvalidPackageException(String message) { super(message); }
    }

    private GpuDriverStore() {}

    /**
     * Custom drivers need arm64 and API 28+ (libadrenotools' linker namespaces)
     * on a Qualcomm KGSL or an Arm Mali kbase GPU node.
     */
    static boolean supported() {
        return isArm64(Build.SUPPORTED_ABIS) && Build.VERSION.SDK_INT >= 28 && vendor() != Vendor.OTHER;
    }

    static Vendor vendor() {
        if (new File("/dev/kgsl-3d0").exists()) return Vendor.ADRENO;
        if (new File("/dev/mali0").exists()) return Vendor.MALI;
        return Vendor.OTHER;
    }

    static boolean isArm64(String[] abis) {
        if (abis == null) return false;
        for (String abi : abis) if ("arm64-v8a".equals(abi)) return true;
        return false;
    }

    /**
     * "Adreno750v2" from the KGSL sysfs node, or the kbase one's
     * "Mali-G615 6 cores r1p3 0xB8A3"; null when neither is readable.
     */
    static String gpuModel() {
        for (String path : new String[] { "/sys/class/kgsl/kgsl-3d0/gpu_model", "/sys/class/misc/mali0/device/gpuinfo" }) {
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(new FileInputStream(path), StandardCharsets.UTF_8))) {
                String line = reader.readLine();
                if (line != null && !line.trim().isEmpty()) return line.trim();
            } catch (IOException | SecurityException ignored) {
                // try the next node
            }
        }
        return null;
    }

    /** Draw-ordering workaround for Adreno 650 Turnip hangs (#558), with less GPU overlap. */
    static boolean needsAdreno650SyncDraw(String model, Metadata metadata) {
        if (model == null || metadata == null || metadata.isPanVk()
                || !model.trim().matches("(?i)Adreno\\s*650(?:\\s*v\\d+)?")) return false;
        String name = metadata.name.toLowerCase(Locale.ROOT);
        // The R8 Sysmem package replaces Turnip's debug flags with forced sysmem,
        // so it cannot use this environment-controlled workaround.
        return name.matches("(?s).*\\bturnip\\b.*")
            && !name.matches("(?s).*\\b(?:panvk|sysmem)\\b.*");
    }

    /** LO_VK_CUSTOM_DRIVER_MODE for a package: the player's choice, else its default. */
    static String loadMode(Context context, Installed driver) {
        String mode = preferences(context).getString(KEY_LOAD_MODE + driver.id(), null);
        return LOAD_ICD.equals(mode) || LOAD_ADRENOTOOLS.equals(mode) ? mode : driver.metadata.defaultLoadMode();
    }

    /** Synchronous, like {@link #select}: a restart may follow at once. */
    static void setLoadMode(Context context, Installed driver, String mode) {
        preferences(context).edit().putString(KEY_LOAD_MODE + driver.id(), mode).commit();
    }

    /** Directory name for a package: the zip name without its extension, made filesystem safe. */
    static String slug(String packageName) {
        String base = packageName;
        int slash = Math.max(base.lastIndexOf('/'), base.lastIndexOf('\\'));
        if (slash >= 0) base = base.substring(slash + 1);
        if (base.toLowerCase().endsWith(".zip")) base = base.substring(0, base.length() - 4);
        StringBuilder result = new StringBuilder();
        for (char c : base.toCharArray()) {
            boolean keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
            result.append(keep ? c : '_');
            if (result.length() >= 64) break;
        }
        while (result.length() > 0 && result.charAt(0) == '.') result.deleteCharAt(0);
        return result.length() == 0 ? "driver" : result.toString();
    }

    static File root(Context context) { return new File(context.getFilesDir(), DIRECTORY); }

    static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE);
    }

    static boolean choiceMade(Context context) {
        return preferences(context).getBoolean(KEY_CHOICE_MADE, false);
    }

    /** Selected package id, or SYSTEM_DRIVER. */
    static String selected(Context context) {
        return preferences(context).getString(KEY_SELECTED, SYSTEM_DRIVER);
    }

    /** Synchronous: the caller may end the process right after (restart from the game). */
    static void select(Context context, String id) {
        preferences(context).edit().putString(KEY_SELECTED, id == null ? SYSTEM_DRIVER : id)
            .putBoolean(KEY_CHOICE_MADE, true).commit();
    }

    /** The selected package when it is still installed and complete, else null. */
    static Installed selectedDriver(Context context) {
        String id = selected(context);
        if (id.isEmpty()) return null;
        Installed driver = read(new File(root(context), id));
        return driver != null && driver.library().isFile() ? driver : null;
    }

    /** Synchronous: a crash moments later must still find the flag on disk. */
    static void markBootPending(Context context) {
        preferences(context).edit().putString(KEY_BOOT_PENDING, selected(context)).commit();
    }

    static void clearBootPending(Context context) {
        preferences(context).edit().remove(KEY_BOOT_PENDING).commit();
    }

    /**
     * The name of the driver whose last start never reached the game, or null.
     * A custom driver is replaced by the system driver so the player can always
     * get in; a failed start on the system driver only reopens the page, where
     * a custom driver can be picked (#185).
     */
    static String takeFailedBoot(Context context) {
        SharedPreferences preferences = preferences(context);
        String pending = preferences.getString(KEY_BOOT_PENDING, null);
        if (pending == null) return null;
        if (pending.isEmpty()) {
            preferences.edit().remove(KEY_BOOT_PENDING).commit();
            return SYSTEM_DRIVER_NAME;
        }
        preferences.edit().remove(KEY_BOOT_PENDING).putString(KEY_SELECTED, SYSTEM_DRIVER).commit();
        Installed driver = read(new File(root(context), pending));
        return driver != null ? driver.metadata.name : pending;
    }

    static List<Installed> installed(Context context) {
        List<Installed> result = new ArrayList<>();
        File[] directories = root(context).listFiles();
        if (directories == null) return result;
        for (File directory : directories) {
            Installed driver = read(directory);
            if (driver != null && driver.library().isFile()) result.add(driver);
        }
        Collections.sort(result, (a, b) -> a.metadata.name.compareToIgnoreCase(b.metadata.name));
        return result;
    }

    static Installed read(File directory) {
        File meta = new File(directory, META_FILE);
        if (!directory.isDirectory() || !meta.isFile() || meta.length() > MAX_META_BYTES) return null;
        try (InputStream input = new FileInputStream(meta)) {
            return new Installed(directory, Metadata.parse(readAll(input, MAX_META_BYTES)));
        } catch (IOException | JSONException ignored) {
            return null;
        }
    }

    static boolean isInstalled(Context context, String packageName) {
        Installed driver = read(new File(root(context), slug(packageName)));
        return driver != null && driver.library().isFile();
    }

    /** Extracts a package zip into its own directory and returns it; nothing is selected. */
    static Installed install(Context context, File zip) throws IOException {
        File directory = new File(root(context), slug(zip.getName()));
        try (InputStream input = new FileInputStream(zip)) {
            return installFromStream(input, directory);
        }
    }

    /**
     * Validates and extracts a package. Entries are flat; the metadata is the
     * entry named meta.json (PanVK packages carry several other .json files),
     * else the first .json entry; the library must be the file it names and
     * must be smaller than MAX_LIBRARY_BYTES. The directory is removed again on
     * failure.
     */
    static Installed installFromStream(InputStream input, File directory) throws IOException {
        deleteRecursively(directory);
        if (!directory.mkdirs()) throw new IOException("cannot create " + directory);
        String directoryPath = directory.getCanonicalPath() + File.separator;
        try {
            String metaJson = null, firstJson = null;
            List<String> extracted = new ArrayList<>();
            ZipInputStream zip = new ZipInputStream(input);
            for (ZipEntry entry; (entry = zip.getNextEntry()) != null; zip.closeEntry()) {
                if (entry.isDirectory()) continue;
                String name = entry.getName();
                if (name.contains("/") || name.contains("\\") || name.startsWith(".")) {
                    throw new InvalidPackageException("unexpected entry " + name);
                }
                File target = new File(directory, name);
                if (!target.getCanonicalPath().startsWith(directoryPath)) {
                    throw new InvalidPackageException("entry escapes package " + name);
                }
                if (name.equalsIgnoreCase(META_FILE)) {
                    metaJson = readAll(zip, MAX_META_BYTES);
                } else if (firstJson == null && name.toLowerCase(Locale.ROOT).endsWith(".json")) {
                    try {
                        firstJson = readAll(zip, MAX_META_BYTES);
                    } catch (InvalidPackageException tooLarge) {
                        firstJson = ""; // a large data file, not the metadata
                    }
                } else if (name.endsWith(".so")) {
                    copyBounded(zip, target, MAX_LIBRARY_BYTES);
                    extracted.add(name);
                }
            }
            String json = metaJson != null ? metaJson : firstJson != null && !firstJson.isEmpty() ? firstJson : null;
            if (json == null) throw new InvalidPackageException("no meta.json in package");
            Metadata metadata;
            try {
                metadata = Metadata.parse(json);
            } catch (JSONException e) {
                throw new InvalidPackageException("meta.json: " + e.getMessage());
            }
            if (metadata.isForOtherSystem()) {
                throw new InvalidPackageException("this is a build for " + metadata.abi
                    + ", not for Android; install the Android package (.adpkg.zip)");
            }
            if (metadata.requiredApi() > Build.VERSION.SDK_INT) {
                throw new InvalidPackageException("needs Android API " + metadata.requiredApi()
                    + ", this device has " + Build.VERSION.SDK_INT);
            }
            if (!extracted.contains(metadata.libraryName)) {
                throw new InvalidPackageException("package has no " + metadata.libraryName);
            }
            writeAll(json.getBytes(StandardCharsets.UTF_8), new File(directory, META_FILE));
            return new Installed(directory, metadata);
        } catch (IOException | RuntimeException e) {
            deleteRecursively(directory);
            throw e;
        }
    }

    static void remove(Context context, Installed driver) {
        deleteRecursively(driver.directory);
        SharedPreferences.Editor editor = preferences(context).edit().remove(KEY_LOAD_MODE + driver.id());
        if (selected(context).equals(driver.id())) editor.putString(KEY_SELECTED, SYSTEM_DRIVER);
        editor.commit();
    }

    static void deleteRecursively(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteRecursively(child);
        //noinspection ResultOfMethodCallIgnored
        file.delete();
    }

    private static String readAll(InputStream input, long limit) throws IOException {
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        byte[] chunk = new byte[8192];
        long total = 0;
        for (int read; (read = input.read(chunk)) > 0; ) {
            total += read;
            if (total > limit) throw new InvalidPackageException("meta.json larger than " + limit);
            buffer.write(chunk, 0, read);
        }
        return buffer.toString("UTF-8");
    }

    private static void writeAll(byte[] bytes, File target) throws IOException {
        try (OutputStream output = new FileOutputStream(target)) { output.write(bytes); }
    }

    private static void copyBounded(InputStream input, File target, long limit) throws IOException {
        try (OutputStream output = new FileOutputStream(target)) {
            byte[] chunk = new byte[65536];
            long total = 0;
            for (int read; (read = input.read(chunk)) > 0; ) {
                total += read;
                if (total > limit) throw new InvalidPackageException(target.getName() + " larger than " + limit);
                output.write(chunk, 0, read);
            }
        }
    }
}
