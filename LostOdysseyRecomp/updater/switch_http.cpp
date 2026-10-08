#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include "http.h"

// Nintendo Switch: no network transfers. The updater, the shader pack download
// and the pipeline corpus download are all skipped on the console; these keep
// the shared code linking and fail cleanly if anything calls them.
namespace updater
{
bool ReadResponse(std::string_view, size_t, std::string&, std::string& error)
{
    error = "network access is not available on Nintendo Switch";
    return false;
}

bool DownloadFile(std::string_view, const std::filesystem::path&, uint64_t,
                  const DownloadProgress&, std::string& error, bool& cancelled)
{
    cancelled = false;
    error = "network access is not available on Nintendo Switch";
    return false;
}

bool Download(std::string_view, const std::filesystem::path&, uint64_t,
              ProgressWindow&, std::string& error, bool& cancelled)
{
    cancelled = false;
    error = "network access is not available on Nintendo Switch";
    return false;
}
}
#endif
