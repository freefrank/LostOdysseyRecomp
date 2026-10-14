package io.github.freefrank.lostodyssey;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Driver packages offered for download: the GitHub release feeds of the Turnip
 * builders that the Eden emulator's driver fetcher lists, read with the same
 * per-source display rules, plus Eden's recommendation table by Adreno model;
 * and, for Mali, the Mesa PanVK builds of panvk-kbase-android with a
 * recommendation by Mali architecture.
 */
final class GpuDriverCatalog {
    static final String USER_AGENT = "LostOdysseyRecomp-Android";
    static final int RELEASES_PER_SOURCE = 20;
    static final long CACHE_FRESH_MS = 60L * 60L * 1000L;
    static final String CACHE_DIRECTORY = "gpu_driver_catalog";

    /** One GitHub repository whose releases carry driver zips. */
    static final class Source {
        final String name, path;
        /** Show the release tag instead of its title (titles are generic there). */
        final boolean useTagName;
        /** Order releases by publish time instead of the feed order. */
        final boolean sortByPublishTime;
        /** The GPUs the packages are for; the page lists only the device's own. */
        final GpuDriverStore.Vendor vendor;
        /** Assets offered for download (lower case); other zips there are for other systems. */
        final String assetSuffix;
        /**
         * The recommended release, fetched by tag when it is no longer among the
         * newest RELEASES_PER_SOURCE (the source publishes almost daily); null if none.
         */
        final String recommendedTag;

        Source(String name, String path, boolean useTagName, boolean sortByPublishTime) {
            this(name, path, useTagName, sortByPublishTime, GpuDriverStore.Vendor.ADRENO, ".zip", null);
        }

        Source(String name, String path, boolean useTagName, boolean sortByPublishTime,
               GpuDriverStore.Vendor vendor, String assetSuffix, String recommendedTag) {
            this.name = name;
            this.path = path;
            this.useTagName = useTagName;
            this.sortByPublishTime = sortByPublishTime;
            this.vendor = vendor;
            this.assetSuffix = assetSuffix;
            this.recommendedTag = recommendedTag;
        }

        String releasesUrl() {
            return "https://api.github.com/repos/" + path + "/releases?per_page=" + RELEASES_PER_SOURCE;
        }

        String recommendedUrl() {
            return "https://api.github.com/repos/" + path + "/releases/tags/" + recommendedTag;
        }

        String cacheName() { return path.replace('/', '_') + ".json"; }

        String recommendedCacheName() { return path.replace('/', '_') + "-recommended.json"; }
    }

    /**
     * The PanVK release recommended for Mali: tested by its author on Mali-G57,
     * G610, G710, G615, G715 and Immortalis-G720 (later builds were retested on
     * one GPU only).
     */
    static final String PANVK_RECOMMENDED_TAG = "panvk-kbase-v0.1.0-rc3";

    /** Eden's sources and flags for Adreno, then the PanVK builds for Mali. */
    static final List<Source> SOURCES = Collections.unmodifiableList(Arrays.asList(
        new Source("Mr. Purple Turnip", "MrPurple666/purple-turnip", false, false),
        new Source("GameHub Adreno 8xx", "crueter/GameHub-8Elite-Drivers", false, false),
        new Source("KIMCHI Turnip", "K11MCH1/AdrenoToolsDrivers", true, true),
        new Source("Weab-Chan Freedreno", "Weab-chan/freedreno_turnip-CI", false, false),
        new Source("Whitebelyash Turnip", "whitebelyash/freedreno_turnip-CI", false, true),
        // Each release also has a glibc "-EMULATOR.zip" that cannot load on Android.
        new Source("PanVK-kbase (Mesa)", "zenithblue-oss/panvk-kbase-android", true, true,
            GpuDriverStore.Vendor.MALI, ".adpkg.zip", PANVK_RECOMMENDED_TAG)));

    static List<Source> sourcesFor(GpuDriverStore.Vendor vendor) {
        List<Source> result = new ArrayList<>();
        for (Source source : SOURCES) if (source.vendor == vendor) result.add(source);
        return result;
    }

    static final class Asset {
        final String name, url;
        final long size;

        Asset(String name, String url, long size) {
            this.name = name;
            this.url = url;
            this.size = size;
        }
    }

    static final class Release {
        final String title, tag, publishedAt;
        final boolean prerelease;
        final List<Asset> assets;
        boolean latest, recommended;

        Release(String title, String publishedAt, boolean prerelease, List<Asset> assets) {
            this(title, title, publishedAt, prerelease, assets);
        }

        Release(String title, String tag, String publishedAt, boolean prerelease, List<Asset> assets) {
            this.title = title;
            this.tag = tag;
            this.publishedAt = publishedAt;
            this.prerelease = prerelease;
            this.assets = assets;
        }

        String date() {
            return publishedAt.length() >= 10 ? publishedAt.substring(0, 10) : publishedAt;
        }
    }

    /** Result of reading one source: releases, or an error, possibly from the cache. */
    static final class Feed {
        final Source source;
        final List<Release> releases;
        final String error;
        final boolean fromCache;

        Feed(Source source, List<Release> releases, String error, boolean fromCache) {
            this.source = source;
            this.releases = releases;
            this.error = error;
            this.fromCache = fromCache;
        }
    }

    interface Progress { void update(long done, long total); }

    private GpuDriverCatalog() {}

    /**
     * Releases with at least one asset ending in the source's suffix (a zip).
     * Prereleases are listed but never "latest"; the first stable release in
     * the final order is marked latest, and the source's recommended tag is
     * marked recommended.
     */
    static List<Release> parseReleases(String json, Source source) throws JSONException {
        JSONArray array = new JSONArray(json);
        List<Release> releases = new ArrayList<>();
        for (int i = 0; i < array.length(); ++i) {
            JSONObject object = array.getJSONObject(i);
            List<Asset> assets = new ArrayList<>();
            JSONArray assetArray = object.optJSONArray("assets");
            for (int j = 0; assetArray != null && j < assetArray.length(); ++j) {
                JSONObject asset = assetArray.getJSONObject(j);
                String name = asset.optString("name", "");
                String url = asset.optString("browser_download_url", "");
                if (!name.toLowerCase(Locale.ROOT).endsWith(source.assetSuffix) || !url.startsWith("https://")) continue;
                assets.add(new Asset(name, url, asset.optLong("size", 0)));
            }
            if (assets.isEmpty()) continue;
            String tag = object.optString("tag_name", "").trim();
            String title = source.useTagName ? tag : object.optString("name", "");
            if (title.trim().isEmpty()) title = object.optString("tag_name", "release");
            releases.add(new Release(title.trim(), tag, object.optString("published_at", ""),
                object.optBoolean("prerelease", false), assets));
        }
        return order(releases, source);
    }

    /**
     * Adds the recommended release (read on its own when the newest releases no
     * longer include it) unless the list already has it, then orders the list.
     */
    static List<Release> withRecommended(List<Release> releases, List<Release> recommended, Source source) {
        List<Release> result = new ArrayList<>(releases);
        for (Release candidate : recommended) {
            boolean present = false;
            for (Release release : result) present |= release.tag.equals(candidate.tag);
            if (!present) result.add(candidate);
        }
        return order(result, source);
    }

    static boolean hasRecommended(List<Release> releases, Source source) {
        if (source.recommendedTag == null) return true;
        for (Release release : releases) if (release.tag.equals(source.recommendedTag)) return true;
        return false;
    }

    private static List<Release> order(List<Release> releases, Source source) {
        if (source.sortByPublishTime) {
            Collections.sort(releases, (a, b) -> b.publishedAt.compareTo(a.publishedAt));
        }
        boolean latestFound = false;
        for (Release release : releases) {
            release.latest = !latestFound && !release.prerelease;
            latestFound |= release.latest;
            release.recommended = release.tag.equals(source.recommendedTag);
        }
        return releases;
    }

    private static final Pattern ADRENO = Pattern.compile("Adreno\\D*(\\d+)", Pattern.CASE_INSENSITIVE);

    /** Adreno model number from "Adreno750v2" or "Adreno (TM) 750"; 0 when unknown. */
    static int adrenoModel(String gpuModel) {
        if (gpuModel == null) return 0;
        Matcher matcher = ADRENO.matcher(gpuModel);
        if (!matcher.find()) return 0;
        try {
            return Integer.parseInt(matcher.group(1));
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    /** Eden's recommendation for an Adreno model number (its table, unchanged). */
    static String edenRecommendation(int model) {
        if (model < 10) return "Unsupported";
        if (model < 100) return "KIMCHI Latest";
        if (model < 600) return "Unsupported";
        if (model < 640) return "Mr. Purple EOL-24.3.4";
        if (model < 700) return "Mr. Purple T19";
        if (model <= 710) return "KIMCHI 25.2.0_r5";
        if (model < 800) return "Mr. Purple T23";
        if (model < 900) return "GameHub Adreno 8xx";
        return "Unsupported";
    }

    // Mali -------------------------------------------------------------------

    /** What PanVK-kbase offers a Mali GPU, by its Mesa architecture number. */
    enum MaliSupport {
        /** Valhall v9 to 5th Gen v13: tested by PanVK-kbase's author on most of them. */
        SUPPORTED,
        /** Mali G1 (v14): built by PanVK-kbase but not run on hardware yet. */
        UNTESTED,
        /**
         * Midgard and Bifrost: no PanVK-kbase build, and Bifrost's PanVK has
         * four descriptor sets where the renderer binds five.
         */
        UNSUPPORTED,
        /** The GPU name is unknown or not a Mali one. */
        UNKNOWN
    }

    private static final Pattern MALI = Pattern.compile("(?:mali|immortalis)[- ]?([tg])[- ]?(\\d+)",
        Pattern.CASE_INSENSITIVE);

    /**
     * Mesa's Mali architecture number (PAN_ARCH) from a GPU name such as
     * "Mali-G615 6 cores r1p3 0xB8A3", "Mali-G720-Immortalis MC12",
     * "Immortalis-G925" or "Mali-G1-Ultra MC12"; 0 when unknown.
     */
    static int maliArch(String gpuName) {
        if (gpuName == null) return 0;
        Matcher matcher = MALI.matcher(gpuName);
        if (!matcher.find()) return 0;
        int model;
        try {
            model = Integer.parseInt(matcher.group(2));
        } catch (NumberFormatException e) {
            return 0;
        }
        if (matcher.group(1).equalsIgnoreCase("t")) return model < 760 ? 4 : 5; // Midgard
        switch (model) {
            case 1: return 14;                                        // G1-Pro/Premium/Ultra
            case 71: case 72: return 6;                               // Bifrost
            case 31: case 51: case 52: case 76: return 7;             // Bifrost
            case 57: case 68: case 77: case 78: return 9;             // Valhall, job manager
            case 310: case 510: case 610: case 710: return 10;        // Valhall, CSF
            case 615: case 715: return 11;
            case 620: case 720: return 12;                            // 5th Gen
            case 625: case 725: case 925: return 13;
            default: return 0;
        }
    }

    static MaliSupport maliSupport(int arch) {
        if (arch >= 9 && arch <= 13) return MaliSupport.SUPPORTED;
        if (arch == 14) return MaliSupport.UNTESTED;
        if (arch >= 4 && arch <= 7) return MaliSupport.UNSUPPORTED;
        return MaliSupport.UNKNOWN;
    }

    /** "Mali-G52" (the part of a longer name that names the GPU), or null. */
    static String maliName(String text) {
        if (text == null) return null;
        Matcher matcher = MALI.matcher(text);
        return matcher.find() ? matcher.group().replace(' ', '-') : null;
    }

    /**
     * Reads one source and, when its newest releases no longer include the
     * recommended one, that release by its tag.
     */
    static Feed fetch(File cacheRoot, Source source, boolean forceRefresh) {
        Feed feed = fetchList(cacheRoot, source, forceRefresh);
        if (hasRecommended(feed.releases, source)) return feed;
        File cache = new File(new File(cacheRoot, CACHE_DIRECTORY), source.recommendedCacheName());
        String json = null;
        if (!forceRefresh && cache.isFile() && System.currentTimeMillis() - cache.lastModified() < CACHE_FRESH_MS) {
            json = readCache(cache);
        }
        try {
            if (json == null) {
                json = get(source.recommendedUrl(), "application/vnd.github+json");
                //noinspection ResultOfMethodCallIgnored
                cache.getParentFile().mkdirs();
                try (OutputStream output = new FileOutputStream(cache)) {
                    output.write(json.getBytes(StandardCharsets.UTF_8));
                }
            }
        } catch (IOException e) {
            json = readCache(cache);
        }
        if (json == null) return feed;
        try {
            List<Release> recommended = parseReleases("[" + json + "]", source);
            return new Feed(source, withRecommended(feed.releases, recommended, source), feed.error, feed.fromCache);
        } catch (JSONException e) {
            return feed;
        }
    }

    private static String readCache(File cache) {
        if (!cache.isFile()) return null;
        try (InputStream input = new FileInputStream(cache)) {
            byte[] bytes = new byte[(int) Math.min(cache.length(), 8L << 20)];
            int offset = 0;
            for (int read; offset < bytes.length && (read = input.read(bytes, offset, bytes.length - offset)) > 0; ) offset += read;
            return new String(bytes, 0, offset, StandardCharsets.UTF_8);
        } catch (IOException e) {
            return null;
        }
    }

    /** Reads one source's release list, serving a cached copy when it is fresh or the network fails. */
    private static Feed fetchList(File cacheRoot, Source source, boolean forceRefresh) {
        File cacheDirectory = new File(cacheRoot, CACHE_DIRECTORY);
        File cache = new File(cacheDirectory, source.cacheName());
        boolean cacheFresh = cache.isFile() && System.currentTimeMillis() - cache.lastModified() < CACHE_FRESH_MS;
        if (cacheFresh && !forceRefresh) {
            Feed cached = fromCache(source, cache, null);
            if (cached != null) return cached;
        }
        String error;
        try {
            String json = get(source.releasesUrl(), "application/vnd.github+json");
            List<Release> releases = parseReleases(json, source);
            //noinspection ResultOfMethodCallIgnored
            cacheDirectory.mkdirs();
            try (OutputStream output = new FileOutputStream(cache)) {
                output.write(json.getBytes(StandardCharsets.UTF_8));
            }
            return new Feed(source, releases, null, false);
        } catch (IOException | JSONException e) {
            error = e.getMessage() == null ? e.getClass().getSimpleName() : e.getMessage();
        }
        Feed cached = fromCache(source, cache, error);
        return cached != null ? cached : new Feed(source, Collections.emptyList(), error, false);
    }

    private static Feed fromCache(Source source, File cache, String error) {
        String json = readCache(cache);
        if (json == null) return null;
        try {
            return new Feed(source, parseReleases(json, source), error, true);
        } catch (JSONException e) {
            return null;
        }
    }

    private static String get(String url, String accept) throws IOException {
        HttpURLConnection connection = open(url, accept);
        try {
            int code = connection.getResponseCode();
            if (code / 100 != 2) throw new IOException(describe(connection, code));
            try (InputStream input = connection.getInputStream()) {
                StringBuilder text = new StringBuilder();
                byte[] chunk = new byte[16384];
                for (int read; (read = input.read(chunk)) > 0; ) {
                    text.append(new String(chunk, 0, read, StandardCharsets.UTF_8));
                    if (text.length() > (8 << 20)) throw new IOException("response too large");
                }
                return text.toString();
            }
        } finally {
            connection.disconnect();
        }
    }

    private static HttpURLConnection open(String url, String accept) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
        connection.setInstanceFollowRedirects(true);
        connection.setConnectTimeout(15000);
        connection.setReadTimeout(30000);
        connection.setRequestProperty("User-Agent", USER_AGENT);
        connection.setRequestProperty("Accept", accept);
        return connection;
    }

    /** "HTTP 403, GitHub rate limit resets at 14:05" or "HTTP 500". */
    static String describe(HttpURLConnection connection, int code) {
        String remaining = connection.getHeaderField("X-RateLimit-Remaining");
        String reset = connection.getHeaderField("X-RateLimit-Reset");
        if ((code == 403 || code == 429) && "0".equals(remaining) && reset != null) {
            try {
                long seconds = Long.parseLong(reset.trim());
                return "HTTP " + code + ", GitHub rate limit resets at "
                    + String.format(Locale.ROOT, "%tR", seconds * 1000L);
            } catch (NumberFormatException ignored) {
                // fall through to the plain code
            }
        }
        return "HTTP " + code;
    }

    /** Downloads an asset into a file, reporting byte progress; the file is removed on failure. */
    static File download(Asset asset, File directory, Progress progress) throws IOException {
        //noinspection ResultOfMethodCallIgnored
        directory.mkdirs();
        File target = new File(directory, GpuDriverStore.slug(asset.name) + ".zip");
        HttpURLConnection connection = open(asset.url, "application/octet-stream");
        try {
            int code = connection.getResponseCode();
            if (code / 100 != 2) throw new IOException(describe(connection, code));
            long total = connection.getContentLengthLong();
            if (total <= 0) total = asset.size;
            long done = 0;
            try (InputStream input = connection.getInputStream();
                 OutputStream output = new FileOutputStream(target)) {
                byte[] chunk = new byte[65536];
                for (int read; (read = input.read(chunk)) > 0; ) {
                    output.write(chunk, 0, read);
                    done += read;
                    if (done > GpuDriverStore.MAX_LIBRARY_BYTES) throw new IOException("package too large");
                    if (progress != null) progress.update(done, total);
                }
            }
            if (done == 0) throw new IOException("empty download");
            return target;
        } catch (IOException | RuntimeException e) {
            //noinspection ResultOfMethodCallIgnored
            target.delete();
            throw e;
        } finally {
            connection.disconnect();
        }
    }
}
