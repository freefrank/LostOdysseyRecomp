#include "pipeline_corpus_download.h"
#include "shader_pack_index.h"
#include "http.h"

#include "../gpu/shader/portable_shader_pack_location.h"

#include <os/logger.h>
#include <os/stale_files.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace updater::shader_pack
{
namespace
{
namespace pack = xenos::portable_pack;
constexpr auto RecheckInterval = std::chrono::hours(24);

std::string PathUtf8(const std::filesystem::path &path)
{
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char *>(value.data()), value.size());
}

std::string Sha256File(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    xenos::resources::Sha256Incremental hash;
    std::vector<char> buffer(1u << 16);
    while (input)
    {
        input.read(buffer.data(), std::streamsize(buffer.size()));
        if (const auto count = input.gcount(); count > 0)
            hash.Update({reinterpret_cast<const uint8_t *>(buffer.data()), size_t(count)});
    }
    return input.eof() ? xenos::resources::Sha256Hex(hash.Finalize()) : std::string{};
}

std::string Check(bool automaticUpdates, bool forced)
{
    const auto directory = pack::InstallDirectory();
    const auto target = directory / CorpusFileName;
    const auto stamp = directory / "pipelines_corpus.checked";
    // A download cut short by a killed process leaves "<corpus>.download-<pid>".
    os::RemoveStaleSiblings(target, L".download-");
    std::error_code error;
    const bool present = std::filesystem::is_regular_file(target, error);
    if (present && !forced)
    {
        if (!automaticUpdates) return "present; automatic updates are off";
        const auto checked = std::filesystem::last_write_time(stamp, error);
        if (!error && std::filesystem::file_time_type::clock::now() - checked < RecheckInterval)
            return "present; checked within the last day";
    }
    const auto touch = [&] {
        std::filesystem::create_directories(directory, error);
        std::ofstream(stamp, std::ios::trunc);
    };

    const auto indexUrl = IndexUrl(std::getenv("LO_SHADER_PACK_INDEX_URL"));
    std::string body, failure;
    if (!ReadResponse(indexUrl, MaxIndexBytes, body, failure)) return "index unavailable: " + failure;
    const auto entries = ParseIndex(body, failure);
    if (!entries) return failure;
    const auto *entry = Select(*entries, CorpusRenderer, CorpusContract);
    if (!entry)
    {
        touch();
        return present ? "present; none published" : "none published";
    }
    if (present && Sha256File(target) == entry->sha256)
    {
        touch();
        return "up to date: " + PathUtf8(target);
    }
    if (entry->size > MaxCorpusBytes) return "the published corpus is larger than " + std::to_string(MaxCorpusBytes) + " bytes";

    std::filesystem::create_directories(directory, error);
    if (error) return "could not create " + PathUtf8(directory) + ": " + error.message();
    auto temp = target;
#ifdef _WIN32
    temp += ".download-" + std::to_string(_getpid());
#else
    temp += ".download-" + std::to_string(getpid());
#endif
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; if (!path.empty()) std::filesystem::remove(path, ignored); }
    } cleanup{temp};
    bool cancelled = false, oversize = false;
    if (!DownloadFile(AssetUrl(indexUrl, entry->file), temp, entry->size, [&](uint64_t completed, uint64_t) {
            oversize = completed > entry->size;
            return !oversize;
        }, failure, cancelled))
        return oversize ? "the server sent more than the " + std::to_string(entry->size) + " bytes the index lists"
                        : "download failed: " + failure;
    const auto size = std::filesystem::file_size(temp, error);
    if (error || size != entry->size)
        return "downloaded " + std::to_string(error ? 0 : size) + " bytes, the index lists " + std::to_string(entry->size);
    if (Sha256File(temp) != entry->sha256) return "the download does not match the published SHA-256";
    // An open corpus (the renderer reading it) can block the replacement on
    // Windows; the old file then stays and the next start tries again.
    std::filesystem::rename(temp, target, error);
    if (error) return "could not install " + PathUtf8(target) + ": " + error.message();
    cleanup.path.clear();
    touch();
    return std::string(present ? "updated: " : "installed: ") + PathUtf8(target) + " (" + std::to_string(entry->size) +
           " bytes, used from the next start)";
}
} // namespace

void StartCorpusDownload(bool automaticUpdates)
{
    const char *mode = std::getenv("LO_SHADER_PACK_DOWNLOAD");
    const std::string_view choice = mode ? mode : "";
    std::string skipped;
    if (choice == "0") skipped = "check disabled by LO_SHADER_PACK_DOWNLOAD=0";
    else if (std::getenv("LO_HEADLESS")) skipped = "check skipped: headless";
    else if (const char *corpus = std::getenv("LO_PIPELINE_CORPUS"); corpus && *corpus)
        skipped = "check skipped: LO_PIPELINE_CORPUS is set";
    else if (std::getenv("LO_BACKGROUND") && choice != "1") skipped = "check skipped: background run";
    if (!skipped.empty())
    {
        LOG_INFO("pipeline corpus: {}", skipped);
        return;
    }
    // LO_SHADER_PACK_DOWNLOAD=1 checks now even when the corpus was checked recently.
    std::thread([automaticUpdates, forced = choice == "1"] {
        try
        {
            LOG_INFO("pipeline corpus: {}", Check(automaticUpdates, forced));
        }
        catch (const std::exception &exception)
        {
            LOG_WARNING("pipeline corpus: check failed: {}", exception.what());
        }
    }).detach();
}
}
