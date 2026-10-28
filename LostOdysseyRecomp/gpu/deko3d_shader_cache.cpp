// LostOdysseyRecomp (2026-10-27): deko3d DKSH caches (deko3d_shader_cache.h).
//
// deko3d_shaders.lodk (tools/deko3d/pack_dkcache.py):
//   header  'LODK', version 1, count, 0                        (4 x u32)
//   index   count x {u64 key, u64 offset, u32 stored, u32 size} sorted by key
//   data    each DKSH, zstd-compressed (stored < size) or as is (stored == size)
// deko3d_runtime.dkcache: records {'DKS1', size, u64 key, DKSH[size]} (the PC tool's own output format).
#include "deko3d_shader_cache.h"

#if defined(LO_SWITCH_DEKO3D)
#include <plume_deko3d_api.h>
#include <os/logger.h>

#include <zstd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace lo::deko3d {
    namespace {
        constexpr uint32_t kPackMagic = 0x4B444F4C;    // "LODK"
        constexpr uint32_t kRecordMagic = 0x31534B44;  // "DKS1"

        struct PackEntry {
            uint64_t key;
            uint64_t offset;
            uint32_t stored;
            uint32_t size;
        };
        static_assert(sizeof(PackEntry) == 24);

        std::mutex g_mutex;
        FILE* g_pack = nullptr;
        std::vector<PackEntry> g_index;
        FILE* g_runtime = nullptr;
        std::unordered_map<uint64_t, std::vector<uint8_t>> g_runtimeEntries;
        uint64_t g_packHits = 0, g_runtimeHits = 0, g_misses = 0, g_stored = 0;

        bool LoadPack(const std::filesystem::path& path) {
            g_pack = std::fopen(path.c_str(), "rb");
            if (!g_pack) return false;
            uint32_t header[4]{};
            if (std::fread(header, 4, 4, g_pack) != 4 || header[0] != kPackMagic || header[1] != 1) {
                LOG_WARNING("deko3d: {} is not a version 1 shader pack", path.string());
                std::fclose(g_pack);
                g_pack = nullptr;
                return false;
            }
            g_index.resize(header[2]);
            if (header[2] && std::fread(g_index.data(), sizeof(PackEntry), header[2], g_pack) != header[2]) {
                LOG_WARNING("deko3d: {} is cut short", path.string());
                g_index.clear();
                std::fclose(g_pack);
                g_pack = nullptr;
                return false;
            }
            return true;
        }

        void LoadRuntime(const std::filesystem::path& path) {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (FILE* f = std::fopen(path.c_str(), "rb")) {
                long good = 0;
                for (;;) {
                    uint32_t head[2];
                    uint64_t key;
                    if (std::fread(head, 4, 2, f) != 2 || head[0] != kRecordMagic || head[1] > (8u << 20) ||
                        std::fread(&key, 8, 1, f) != 1)
                        break;
                    std::vector<uint8_t> dksh(head[1]);
                    if (std::fread(dksh.data(), 1, dksh.size(), f) != dksh.size())
                        break;
                    g_runtimeEntries[key] = std::move(dksh);
                    good = std::ftell(f);
                }
                std::fclose(f);
                // A record cut short by a power-off: the file is truncated after the last whole one.
                if (std::filesystem::file_size(path, ec) != uint64_t(good) && !ec)
                    std::filesystem::resize_file(path, uint64_t(good), ec);
            }
            g_runtime = std::fopen(path.c_str(), "ab");
        }

        bool Lookup(uint64_t key, std::vector<uint8_t>& dksh) {
            std::lock_guard lock(g_mutex);
            if (auto it = g_runtimeEntries.find(key); it != g_runtimeEntries.end()) {
                dksh = it->second;
                ++g_runtimeHits;
                return true;
            }
            const auto it = std::lower_bound(g_index.begin(), g_index.end(), key,
                [](const PackEntry& e, uint64_t k) { return e.key < k; });
            if (it == g_index.end() || it->key != key || !g_pack) {
                ++g_misses;
                return false;
            }
            std::vector<uint8_t> stored(it->stored);
            if (fseeko(g_pack, off_t(it->offset), SEEK_SET) != 0 || std::fread(stored.data(), 1, stored.size(), g_pack) != stored.size()) {
                ++g_misses;
                return false;
            }
            if (it->stored == it->size)
                dksh = std::move(stored);
            else {
                dksh.resize(it->size);
                const size_t n = ZSTD_decompress(dksh.data(), dksh.size(), stored.data(), stored.size());
                if (ZSTD_isError(n) || n != it->size) {
                    ++g_misses;
                    return false;
                }
            }
            ++g_packHits;
            return true;
        }

        void Store(uint64_t key, const std::vector<uint8_t>& dksh) {
            std::lock_guard lock(g_mutex);
            g_runtimeEntries[key] = dksh;
            if (!g_runtime) return;
            const uint32_t head[2] = { kRecordMagic, uint32_t(dksh.size()) };
            std::fwrite(head, 4, 2, g_runtime);
            std::fwrite(&key, 8, 1, g_runtime);
            std::fwrite(dksh.data(), 1, dksh.size(), g_runtime);
            // On the card now: a HOME close must not lose what took the console 100 ms each to compile.
            std::fflush(g_runtime);
            fsync(fileno(g_runtime));
            if (++g_stored <= 8 || g_stored % 64 == 0)
                LOG_INFO("deko3d: {} shaders compiled on the console so far (pack hits {}, cache hits {}, misses {})",
                    g_stored, g_packHits, g_runtimeHits, g_misses);
        }
    }

    void InstallShaderCache(const std::filesystem::path& packPath, const std::filesystem::path& runtimePath) {
        {
            std::lock_guard lock(g_mutex);
            const bool pack = LoadPack(packPath);
            LoadRuntime(runtimePath);
            if (pack)
                LOG_INFO("deko3d: shader pack {} with {} shaders; {} compiled earlier on the console", packPath.string(),
                    g_index.size(), g_runtimeEntries.size());
            else
                LOG_WARNING("deko3d: no shader pack at {} ({} compiled earlier on the console): every new shader is "
                    "compiled now, about 0.1 s each", packPath.string(), g_runtimeEntries.size());
        }
        plume::SetDeko3DShaderCache(Lookup, Store);
    }
}
#else
namespace lo::deko3d {
    void InstallShaderCache(const std::filesystem::path&, const std::filesystem::path&) {}
}
#endif
