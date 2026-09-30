#ifndef _WIN32
#include "http.h"
#include "progress.h"

#include <curl/curl.h>
#include <fstream>
#include <string_view>

namespace updater
{
namespace
{
struct CurlGlobal
{
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

struct ResponseBuffer
{
    std::string &body;
    size_t limit;
    bool exceeded = false;
};

bool IsHttpsUrl(std::string_view url)
{
    constexpr std::string_view prefix = "https://";
    return url.size() >= prefix.size() && url.substr(0, prefix.size()) == prefix;
}

size_t Append(void *data, size_t size, size_t count, void *context)
{
    auto &response = *static_cast<ResponseBuffer *>(context);
    const size_t bytes = size * count;
    if (bytes > response.limit - response.body.size())
    {
        response.exceeded = true;
        return 0;
    }
    response.body.append(static_cast<const char *>(data), bytes);
    return bytes;
}

std::string CurlError(CURLcode code)
{
    return std::string("curl request failed: ") + curl_easy_strerror(code);
}

bool Request(std::string_view url, std::string &body, std::string &error, size_t limit)
{
    if (!IsHttpsUrl(url))
    {
        error = "update URL does not use HTTPS";
        return false;
    }
    static const CurlGlobal global;
    CURL *curl = curl_easy_init();
    if (!curl) { error = "could not initialize curl"; return false; }
    curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LostOdysseyRecomp-Updater/1.0");
    ResponseBuffer response{body, limit};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Append);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    char *effectiveUrl = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
    const bool finalIsHttps = effectiveUrl && IsHttpsUrl(effectiveUrl);
    curl_easy_cleanup(curl);
    if (response.exceeded) { error = "update response exceeded its size limit"; return false; }
    if (result != CURLE_OK) { error = CurlError(result); return false; }
    if (!finalIsHttps) { error = "update redirect resolved to non-HTTPS URL"; return false; }
    if (status != 200) { error = "update server returned HTTP " + std::to_string(status); return false; }
    return true;
}

struct DownloadContext
{
    std::ofstream &output;
    ProgressWindow &progress;
    uint64_t expectedSize;
    uint64_t total = 0;
    bool cancelled = false;
};

size_t WriteDownload(void *data, size_t size, size_t count, void *context)
{
    auto &download = *static_cast<DownloadContext *>(context);
    const uint64_t bytes = uint64_t(size) * uint64_t(count);
    download.output.write(static_cast<const char *>(data), std::streamsize(bytes));
    if (!download.output) return 0;
    download.total += bytes;
    download.progress.SetDownloadProgress(download.total, download.expectedSize);
    return size * count;
}

int TransferProgress(void *context, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    auto &download = *static_cast<DownloadContext *>(context);
    if (!download.progress.Cancelled()) return 0;
    download.cancelled = true;
    return 1;
}
}

bool ReadResponse(std::string_view url, size_t limit, std::string &body, std::string &error)
{
    body.clear();
    return Request(url, body, error, limit);
}

bool Download(std::string_view url, const std::filesystem::path &destination, uint64_t expectedSize,
              ProgressWindow &progress, std::string &error, bool &cancelled)
{
    cancelled = false;
    if (!IsHttpsUrl(url))
    {
        error = "update URL does not use HTTPS";
        return false;
    }
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) { error = "could not create update download"; return false; }
    static const CurlGlobal global;
    CURL *curl = curl_easy_init();
    if (!curl) { error = "could not initialize curl"; return false; }
    DownloadContext download{output, progress, expectedSize};
    curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LostOdysseyRecomp-Updater/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteDownload);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, TransferProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &download);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    char *effectiveUrl = nullptr;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
    const bool finalIsHttps = effectiveUrl && IsHttpsUrl(effectiveUrl);
    curl_easy_cleanup(curl);
    if (download.cancelled)
    {
        cancelled = true;
        error = "update cancelled by user";
        return false;
    }
    if (result != CURLE_OK) { error = CurlError(result); return false; }
    if (!finalIsHttps) { error = "update redirect resolved to non-HTTPS URL"; return false; }
    if (status != 200) { error = "update server returned HTTP " + std::to_string(status); return false; }
    output.flush();
    if (!output) { error = "could not write update download"; return false; }
    return true;
}
}
#endif
