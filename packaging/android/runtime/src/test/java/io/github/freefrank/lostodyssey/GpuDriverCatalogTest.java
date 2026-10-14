package io.github.freefrank.lostodyssey;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.util.List;
import org.junit.Test;

public class GpuDriverCatalogTest {
    private static final GpuDriverCatalog.Source PLAIN =
        new GpuDriverCatalog.Source("Plain", "a/b", false, false);
    private static final GpuDriverCatalog.Source TAGGED =
        new GpuDriverCatalog.Source("Tagged", "a/c", true, true);

    private static String release(String name, String tag, String published, boolean prerelease, String... assets) {
        StringBuilder json = new StringBuilder("{\"name\":\"" + name + "\",\"tag_name\":\"" + tag
            + "\",\"published_at\":\"" + published + "\",\"prerelease\":" + prerelease + ",\"assets\":[");
        for (int i = 0; i < assets.length; ++i) {
            if (i > 0) json.append(',');
            json.append("{\"name\":\"").append(assets[i]).append("\",\"size\":1234,\"browser_download_url\":\"https://x/")
                .append(assets[i]).append("\"}");
        }
        return json.append("]}").toString();
    }

    @Test
    public void keepsOnlyZipAssetsAndDropsEmptyReleases() throws Exception {
        String json = "[" + release("Driver 2", "v2", "2026-02-01T00:00:00Z", false, "turnip.zip", "notes.txt")
            + "," + release("Source only", "v1", "2026-01-01T00:00:00Z", false, "source.tar.gz") + "]";
        List<GpuDriverCatalog.Release> releases = GpuDriverCatalog.parseReleases(json, PLAIN);
        assertEquals(1, releases.size());
        assertEquals("Driver 2", releases.get(0).title);
        assertEquals(1, releases.get(0).assets.size());
        assertEquals("turnip.zip", releases.get(0).assets.get(0).name);
        assertEquals("https://x/turnip.zip", releases.get(0).assets.get(0).url);
        assertEquals("2026-02-01", releases.get(0).date());
    }

    @Test
    public void latestIsTheFirstStableReleaseAfterSorting() throws Exception {
        String json = "[" + release("old", "v1", "2026-01-01T00:00:00Z", false, "a.zip")
            + "," + release("beta", "v3-rc", "2026-03-01T00:00:00Z", true, "b.zip")
            + "," + release("new", "v2", "2026-02-01T00:00:00Z", false, "c.zip") + "]";
        List<GpuDriverCatalog.Release> releases = GpuDriverCatalog.parseReleases(json, TAGGED);
        assertEquals("v3-rc", releases.get(0).title);
        assertEquals("v2", releases.get(1).title);
        assertEquals("v1", releases.get(2).title);
        assertFalse(releases.get(0).latest);
        assertTrue(releases.get(1).latest);
        assertFalse(releases.get(2).latest);
    }

    @Test
    public void plainSourcesKeepFeedOrderAndTitles() throws Exception {
        String json = "[" + release("", "v9", "2026-01-01T00:00:00Z", false, "a.zip")
            + "," + release("Named", "v8", "2026-05-01T00:00:00Z", false, "b.zip") + "]";
        List<GpuDriverCatalog.Release> releases = GpuDriverCatalog.parseReleases(json, PLAIN);
        assertEquals("v9", releases.get(0).title);
        assertEquals("Named", releases.get(1).title);
        assertTrue(releases.get(0).latest);
    }

    @Test
    public void rejectsNonHttpsAssets() throws Exception {
        String json = "[{\"name\":\"x\",\"tag_name\":\"x\",\"assets\":[{\"name\":\"a.zip\",\"browser_download_url\":\"http://x/a.zip\"}]}]";
        assertTrue(GpuDriverCatalog.parseReleases(json, PLAIN).isEmpty());
    }

    @Test
    public void parsesAdrenoModelNumbers() {
        assertEquals(750, GpuDriverCatalog.adrenoModel("Adreno750v2"));
        assertEquals(740, GpuDriverCatalog.adrenoModel("Adreno (TM) 740"));
        assertEquals(830, GpuDriverCatalog.adrenoModel("Adreno830"));
        assertEquals(0, GpuDriverCatalog.adrenoModel("Mali-G78"));
        assertEquals(0, GpuDriverCatalog.adrenoModel(null));
    }

    @Test
    public void mirrorsEdenRecommendationTable() {
        assertEquals("Unsupported", GpuDriverCatalog.edenRecommendation(0));
        assertEquals("KIMCHI Latest", GpuDriverCatalog.edenRecommendation(32));
        assertEquals("Mr. Purple EOL-24.3.4", GpuDriverCatalog.edenRecommendation(610));
        assertEquals("Mr. Purple T19", GpuDriverCatalog.edenRecommendation(650));
        assertEquals("KIMCHI 25.2.0_r5", GpuDriverCatalog.edenRecommendation(710));
        assertEquals("Mr. Purple T23", GpuDriverCatalog.edenRecommendation(750));
        assertEquals("GameHub Adreno 8xx", GpuDriverCatalog.edenRecommendation(830));
        assertEquals("Unsupported", GpuDriverCatalog.edenRecommendation(900));
    }

    @Test
    public void sourcesMatchEden() {
        List<GpuDriverCatalog.Source> adreno = GpuDriverCatalog.sourcesFor(GpuDriverStore.Vendor.ADRENO);
        assertEquals(5, adreno.size());
        assertEquals("K11MCH1/AdrenoToolsDrivers", adreno.get(2).path);
        assertTrue(adreno.get(2).useTagName);
        assertEquals("https://api.github.com/repos/K11MCH1/AdrenoToolsDrivers/releases?per_page=20",
            adreno.get(2).releasesUrl());
        for (GpuDriverCatalog.Source source : adreno) {
            assertEquals(".zip", source.assetSuffix);
            assertNull(source.recommendedTag);
        }
    }

    @Test
    public void maliHasOnlyThePanVkSource() {
        List<GpuDriverCatalog.Source> mali = GpuDriverCatalog.sourcesFor(GpuDriverStore.Vendor.MALI);
        assertEquals(1, mali.size());
        GpuDriverCatalog.Source panvk = mali.get(0);
        assertEquals("zenithblue-oss/panvk-kbase-android", panvk.path);
        assertEquals(".adpkg.zip", panvk.assetSuffix);
        assertEquals(GpuDriverCatalog.PANVK_RECOMMENDED_TAG, panvk.recommendedTag);
        assertEquals("https://api.github.com/repos/zenithblue-oss/panvk-kbase-android/releases/tags/panvk-kbase-v0.1.0-rc3",
            panvk.recommendedUrl());
        assertTrue(GpuDriverCatalog.sourcesFor(GpuDriverStore.Vendor.OTHER).isEmpty());
    }

    private static final GpuDriverCatalog.Source PANVK = GpuDriverCatalog.sourcesFor(GpuDriverStore.Vendor.MALI).get(0);

    /** The asset set of a PanVK-kbase release. */
    private static String panvkRelease(String tag, String published) {
        return release("PanVK-kbase " + tag, tag, published, true,
            "libvulkan_panfrost-android-aarch64.so", "panfrost_icd.aarch64.json", "SHA256SUMS", "g57.png",
            "PanVK-Kbase-Android-g615-v11-csf-" + tag + "-5a07217f.adpkg.zip",
            "PanVK-Kbase-g615-v11-csf-" + tag + "-5a07217f-EMULATOR.zip");
    }

    @Test
    public void panvkOffersOnlyTheAndroidPackageAndMarksTheRecommendedRelease() throws Exception {
        String json = "[" + panvkRelease("panvk-kbase-v0.1.0-rc3", "2026-10-11T15:00:00Z")
            + "," + panvkRelease("panvk-kbase-v0.1.0-rc3-lowmem", "2026-10-13T13:33:51Z") + "]";
        List<GpuDriverCatalog.Release> releases = GpuDriverCatalog.parseReleases(json, PANVK);
        assertEquals(2, releases.size());
        assertEquals("panvk-kbase-v0.1.0-rc3-lowmem", releases.get(0).title);
        for (GpuDriverCatalog.Release release : releases) {
            assertEquals(1, release.assets.size());
            assertTrue(release.assets.get(0).name.endsWith(".adpkg.zip"));
            assertFalse(release.latest); // every PanVK release is a pre-release
        }
        assertFalse(releases.get(0).recommended);
        assertTrue(releases.get(1).recommended);
        assertTrue(GpuDriverCatalog.hasRecommended(releases, PANVK));
    }

    @Test
    public void theRecommendedReleaseIsAddedWhenNewerOnesPushedItOut() throws Exception {
        List<GpuDriverCatalog.Release> newest = GpuDriverCatalog.parseReleases(
            "[" + panvkRelease("panvk-kbase-v0.1.2", "2026-11-02T00:00:00Z")
                + "," + panvkRelease("panvk-kbase-v0.1.1", "2026-11-01T00:00:00Z") + "]", PANVK);
        assertFalse(GpuDriverCatalog.hasRecommended(newest, PANVK));
        // The tag endpoint returns one object; fetch() wraps it into an array.
        List<GpuDriverCatalog.Release> recommended = GpuDriverCatalog.parseReleases(
            "[" + panvkRelease("panvk-kbase-v0.1.0-rc3", "2026-10-11T15:00:00Z") + "]", PANVK);
        List<GpuDriverCatalog.Release> merged = GpuDriverCatalog.withRecommended(newest, recommended, PANVK);
        assertEquals(3, merged.size());
        assertEquals("panvk-kbase-v0.1.0-rc3", merged.get(2).title);
        assertTrue(merged.get(2).recommended);
        // Already listed: not added twice.
        assertEquals(3, GpuDriverCatalog.withRecommended(merged, recommended, PANVK).size());
        // Sources without a recommended release never need the extra request.
        assertTrue(GpuDriverCatalog.hasRecommended(newest, PLAIN));
    }

    @Test
    public void mapsMaliNamesToArchitectures() {
        assertEquals(9, GpuDriverCatalog.maliArch("Mali-G57 MC2"));
        assertEquals(9, GpuDriverCatalog.maliArch("Mali-G78AE"));
        assertEquals(10, GpuDriverCatalog.maliArch("Mali-G710 MC7"));
        assertEquals(11, GpuDriverCatalog.maliArch("Mali-G615 6 cores r1p3 0xB8A3"));
        assertEquals(12, GpuDriverCatalog.maliArch("Mali-G720-Immortalis MC12"));
        assertEquals(13, GpuDriverCatalog.maliArch("Immortalis-G925 MC12"));
        assertEquals(14, GpuDriverCatalog.maliArch("Mali-G1-Ultra MC12"));
        assertEquals(7, GpuDriverCatalog.maliArch("Mali-G52 MC2"));
        assertEquals(6, GpuDriverCatalog.maliArch("Mali-G72 MP3"));
        assertEquals(5, GpuDriverCatalog.maliArch("Mali-T880 MP12"));
        assertEquals(4, GpuDriverCatalog.maliArch("Mali-T720"));
        assertEquals(0, GpuDriverCatalog.maliArch("Adreno (TM) 750"));
        assertEquals(0, GpuDriverCatalog.maliArch("Mali-G2-Ultra"));
        assertEquals(0, GpuDriverCatalog.maliArch(null));
    }

    @Test
    public void maliSupportFollowsTheArchitecture() {
        assertEquals(GpuDriverCatalog.MaliSupport.SUPPORTED, GpuDriverCatalog.maliSupport(9));
        assertEquals(GpuDriverCatalog.MaliSupport.SUPPORTED, GpuDriverCatalog.maliSupport(13));
        assertEquals(GpuDriverCatalog.MaliSupport.UNTESTED, GpuDriverCatalog.maliSupport(14));
        assertEquals(GpuDriverCatalog.MaliSupport.UNSUPPORTED, GpuDriverCatalog.maliSupport(7));
        assertEquals(GpuDriverCatalog.MaliSupport.UNSUPPORTED, GpuDriverCatalog.maliSupport(4));
        assertEquals(GpuDriverCatalog.MaliSupport.UNKNOWN, GpuDriverCatalog.maliSupport(0));
    }

    @Test
    public void findsTheMaliNameInAFailureReason() {
        assertEquals("Mali-G52", GpuDriverCatalog.maliName(
            "Mali-G52 MC2: bufferDeviceAddress and shaderInt64 required"));
        assertEquals("Mali-G57", GpuDriverCatalog.maliName("Mali G57 MC2: device creation failed"));
        assertNull(GpuDriverCatalog.maliName("device creation failed"));
        assertNull(GpuDriverCatalog.maliName(null));
    }
}
