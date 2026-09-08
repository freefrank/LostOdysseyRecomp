#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::string Utf8(std::wstring_view value)
{
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), int(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (!size) return {};
    std::string result(size_t(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), int(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    std::filesystem::path marker, contextMarker;
    std::vector<std::wstring> received;
    for (int i = 1; arguments && i < count; ++i)
    {
        received.emplace_back(arguments[i]);
        if (std::wstring_view(arguments[i]) == L"--updated-marker" && i + 1 < count)
        {
            marker = arguments[++i];
            received.emplace_back(arguments[i]);
        }
        else if (std::wstring_view(arguments[i]) == L"--context-marker" && i + 1 < count)
        {
            contextMarker = arguments[++i];
            received.emplace_back(arguments[i]);
        }
    }
    if (arguments) LocalFree(arguments);
    if (!contextMarker.empty())
    {
        std::ofstream output(contextMarker, std::ios::binary | std::ios::trunc);
        output << "cwd=" << Utf8(std::filesystem::current_path().wstring()) << '\n';
        for (const auto &argument : received) output << "arg=" << Utf8(argument) << '\n';
        return output ? 0 : 3;
    }
    if (!marker.empty())
    {
        std::ofstream output(marker, std::ios::trunc);
        output << "updated process started\n";
        return output ? 0 : 3;
    }
    return 2;
}
