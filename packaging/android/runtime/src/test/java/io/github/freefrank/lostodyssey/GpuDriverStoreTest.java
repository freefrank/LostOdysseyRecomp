package io.github.freefrank.lostodyssey;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import org.json.JSONException;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class GpuDriverStoreTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();

    private static final String META = "{\"schemaVersion\":1,\"name\":\"Turnip R8\",\"description\":\"d\","
        + "\"author\":\"KIMCHI\",\"packageVersion\":\"1\",\"vendor\":\"Mesa\",\"driverVersion\":\"Vulkan 1.4\","
        + "\"minApi\":0,\"libraryName\":\"vulkan.ad07xx.so\"}";

    private static byte[] zip(String[][] entries) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            for (String[] entry : entries) {
                zip.putNextEntry(new ZipEntry(entry[0]));
                zip.write(entry[1].getBytes(StandardCharsets.UTF_8));
                zip.closeEntry();
            }
        }
        return bytes.toByteArray();
    }

    private GpuDriverStore.Installed install(byte[] zip, String name) throws IOException {
        return GpuDriverStore.installFromStream(new ByteArrayInputStream(zip), new File(temporary.getRoot(), name));
    }

    @Test
    public void parsesMetadataAndRequiresNameAndLibrary() throws Exception {
        GpuDriverStore.Metadata metadata = GpuDriverStore.Metadata.parse(META);
        assertEquals("Turnip R8", metadata.name);
        assertEquals("vulkan.ad07xx.so", metadata.libraryName);
        assertEquals("Vulkan 1.4 · KIMCHI\nd", metadata.summary());
        for (String bad : new String[] {
                "{\"libraryName\":\"a.so\"}", "{\"name\":\"x\"}", "{\"name\":\"\",\"libraryName\":\"a.so\"}",
                "{\"name\":\"x\",\"libraryName\":\"../a.so\"}", "not json" }) {
            try {
                GpuDriverStore.Metadata.parse(bad);
                fail(bad);
            } catch (JSONException expected) {
                // rejected
            }
        }
    }

    @Test
    public void extractsAValidPackage() throws Exception {
        GpuDriverStore.Installed driver = install(zip(new String[][] {
            { "meta.json", META }, { "vulkan.ad07xx.so", "ELF" } }), "pkg");
        assertEquals("Turnip R8", driver.metadata.name);
        assertEquals("pkg", driver.id());
        assertTrue(driver.library().isFile());
        assertTrue(new File(driver.directory, GpuDriverStore.META_FILE).isFile());
        assertEquals("Turnip R8", GpuDriverStore.read(driver.directory).metadata.name);
    }

    @Test
    public void rejectsPackagesWithoutTheNamedLibraryAndCleansUp() throws Exception {
        File directory = new File(temporary.getRoot(), "bad");
        try {
            install(zip(new String[][] { { "meta.json", META }, { "other.so", "ELF" } }), "bad");
            fail();
        } catch (GpuDriverStore.InvalidPackageException expected) {
            assertTrue(expected.getMessage().contains("vulkan.ad07xx.so"));
        }
        assertFalse(directory.exists());
    }

    @Test
    public void rejectsNestedEntriesAndMissingMetadata() throws Exception {
        try {
            install(zip(new String[][] { { "sub/meta.json", META }, { "vulkan.ad07xx.so", "ELF" } }), "nested");
            fail();
        } catch (GpuDriverStore.InvalidPackageException expected) {
            assertTrue(expected.getMessage().contains("unexpected entry"));
        }
        try {
            install(zip(new String[][] { { "vulkan.ad07xx.so", "ELF" } }), "nometa");
            fail();
        } catch (GpuDriverStore.InvalidPackageException expected) {
            assertTrue(expected.getMessage().contains("meta.json"));
        }
    }

    @Test
    public void rejectsPackagesNeedingANewerApi() throws Exception {
        // Build.VERSION.SDK_INT is 0 in the JVM stubs, so any positive minApi is too new.
        String meta = META.replace("\"minApi\":0", "\"minApi\":27");
        try {
            install(zip(new String[][] { { "meta.json", meta }, { "vulkan.ad07xx.so", "ELF" } }), "api");
            fail();
        } catch (GpuDriverStore.InvalidPackageException expected) {
            assertTrue(expected.getMessage().contains("API 27"));
        }
    }

    /** meta.json of the PanVK-kbase 0.1.0-rc3 Android package (trimmed). */
    private static final String PANVK_META = "{\"schemaVersion\":1,\"name\":\"PanVK Kbase G615\","
        + "\"author\":\"panvk-kbase-android\",\"packageVersion\":\"0.1.0-rc3\",\"vendor\":\"Mesa\","
        + "\"driverVersion\":\"Mesa 26.3.0-devel\",\"minApi\":35,\"description\":\"Experimental\","
        + "\"libraryName\":\"libvulkan_panfrost.so\",\"abi\":\"android-aarch64-bionic\",\"backend\":\"panvk-kbase\"}";

    @Test
    public void panvkMetadataLoadsDirectlyAndAllowsAndroid13() throws Exception {
        GpuDriverStore.Metadata panvk = GpuDriverStore.Metadata.parse(PANVK_META);
        assertTrue(panvk.isPanVk());
        assertEquals(GpuDriverStore.LOAD_ICD, panvk.defaultLoadMode());
        assertEquals(33, panvk.requiredApi());
        assertFalse(panvk.isForOtherSystem());
        assertEquals("0.1.0-rc3 · Mesa 26.3.0-devel · panvk-kbase-android\nExperimental", panvk.summary());

        GpuDriverStore.Metadata turnip = GpuDriverStore.Metadata.parse(META.replace("\"minApi\":0", "\"minApi\":27"));
        assertFalse(turnip.isPanVk());
        assertEquals(GpuDriverStore.LOAD_ADRENOTOOLS, turnip.defaultLoadMode());
        assertEquals(27, turnip.requiredApi());
        assertEquals("", turnip.displayVersion()); // Turnip's "1" is a format counter
        assertEquals(30, GpuDriverStore.Metadata.parse(PANVK_META.replace("\"minApi\":35", "\"minApi\":30")).requiredApi());
    }

    @Test
    public void readsTheEntryNamedMetaJsonAmongOtherJsonFiles() throws Exception {
        // PanVK packages carry MANIFEST.json, SOURCE.json and others next to meta.json.
        GpuDriverStore.Installed driver = install(zip(new String[][] {
            { "libvulkan_panfrost.so", "ELF" }, { "MANIFEST.json", "{\"files\":[]}" },
            { "meta.json", PANVK_META.replace("\"minApi\":35", "\"minApi\":0") },
            { "runtime-feature-matrix.json", "{}" }, { "SHA256SUMS.txt", "x" } }), "panvk");
        assertEquals("PanVK Kbase G615", driver.metadata.name);
        assertTrue(driver.library().isFile());
        assertEquals("PanVK Kbase G615", GpuDriverStore.read(driver.directory).metadata.name);
        // Packages without an entry of that name still use their first .json entry.
        GpuDriverStore.Installed other = install(zip(new String[][] {
            { "driver.json", META }, { "vulkan.ad07xx.so", "ELF" } }), "other");
        assertEquals("Turnip R8", other.metadata.name);
    }

    @Test
    public void rejectsGlibcBuilds() throws Exception {
        String glibc = PANVK_META.replace("android-aarch64-bionic", "linux-aarch64-glibc").replace("\"minApi\":35", "\"minApi\":0");
        try {
            install(zip(new String[][] { { "meta.json", glibc }, { "libvulkan_panfrost.so", "ELF" } }), "glibc");
            fail();
        } catch (GpuDriverStore.InvalidPackageException expected) {
            assertTrue(expected.getMessage().contains("not for Android"));
        }
        assertFalse(new File(temporary.getRoot(), "glibc").exists());
    }

    @Test
    public void slugsAreFilesystemSafe() {
        assertEquals("Turnip_v26.0.0_R8", GpuDriverStore.slug("Turnip_v26.0.0_R8.zip"));
        assertEquals("a_b_c", GpuDriverStore.slug("dir/a b:c.ZIP"));
        assertEquals("hidden", GpuDriverStore.slug("..hidden.zip"));
        assertEquals("driver", GpuDriverStore.slug(".zip"));
        assertEquals(64, GpuDriverStore.slug(new String(new char[100]).replace('\0', 'x') + ".zip").length());
    }

    @Test
    public void arm64Detection() {
        assertTrue(GpuDriverStore.isArm64(new String[] { "arm64-v8a", "armeabi-v7a" }));
        assertFalse(GpuDriverStore.isArm64(new String[] { "armeabi-v7a" }));
        assertFalse(GpuDriverStore.isArm64(null));
    }

    @Test
    public void enablesAdreno650OrderingWorkaroundForReportedTurnipPackages() throws Exception {
        GpuDriverStore.Metadata r8 = GpuDriverStore.Metadata.parse(META.replace("Turnip R8",
            "Mesa Turnip driver v26.0.0 - R8"));
        GpuDriverStore.Metadata t30 = GpuDriverStore.Metadata.parse(META.replace("Turnip R8",
            "Turnip Adreno Driver T30 (@Mr_Purple_666)").replace("\"vendor\":\"Mesa\"", "\"vendor\":\"\""));
        for (String model : new String[] { "Adreno650", "Adreno650v3", "Adreno 650", " adreno650V2 " }) {
            assertTrue(model, GpuDriverStore.needsAdreno650SyncDraw(model, r8));
            assertTrue(model, GpuDriverStore.needsAdreno650SyncDraw(model, t30));
        }
        assertTrue(GpuDriverStore.needsAdreno650SyncDraw("Adreno650v3",
            GpuDriverStore.Metadata.parse(META.replace("Turnip R8", "TURNIP R8"))));
    }

    @Test
    public void leavesOtherGpusAndDriverPackagesUnchanged() throws Exception {
        GpuDriverStore.Metadata turnip = GpuDriverStore.Metadata.parse(META);
        for (String model : new String[] { null, "", "Adreno6500", "Adreno640v2", "Adreno750v2",
                "Adreno650v", "Other Adreno650", "Adreno650 extra" }) {
            assertFalse(String.valueOf(model), GpuDriverStore.needsAdreno650SyncDraw(model, turnip));
        }
        assertFalse(GpuDriverStore.needsAdreno650SyncDraw("Adreno650v3", null));
        for (String name : new String[] { "Qualcomm driver", "NotTurnip R8", "Turnipish",
                "Mesa Turnip driver v26.0.0 - R8 SYSMEM", "Turnip (sysmem)", "PanVK Turnip compatibility" }) {
            assertFalse(name, GpuDriverStore.needsAdreno650SyncDraw("Adreno650v3",
                GpuDriverStore.Metadata.parse(META.replace("Turnip R8", name))));
        }
        assertFalse(GpuDriverStore.needsAdreno650SyncDraw("Adreno650v3",
            GpuDriverStore.Metadata.parse(PANVK_META.replace("PanVK Kbase G615", "Turnip compatibility"))));
    }
}
