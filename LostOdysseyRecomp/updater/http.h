#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace updater
{
class ProgressWindow;

bool ReadResponse(std::string_view url, size_t limit, std::string &body, std::string &error);
bool Download(std::string_view url, const std::filesystem::path &destination, uint64_t expectedSize,
              ProgressWindow &progress, std::string &error, bool &cancelled);
}
