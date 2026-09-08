#include "update.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

#include <array>
#include <fstream>

namespace updater
{
std::string Sha256File(const std::filesystem::path &path, std::string &error)
{
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0, bytes = 0, digestSize = 0;
    std::vector<unsigned char> object, digest;
    auto cleanup = [&] {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    };
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize), &bytes, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&digestSize),
                          sizeof(digestSize), &bytes, 0) < 0)
    {
        cleanup();
        error = "Windows SHA256 provider is unavailable";
        return {};
    }
    object.resize(objectSize);
    digest.resize(digestSize);
    if (BCryptCreateHash(algorithm, &hash, object.data(), DWORD(object.size()), nullptr, 0, 0) < 0)
    {
        cleanup();
        error = "could not create SHA256 context";
        return {};
    }
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        cleanup();
        error = "could not open file for SHA256";
        return {};
    }
    std::array<char, 64 * 1024> buffer{};
    while (input)
    {
        input.read(buffer.data(), buffer.size());
        const auto count = input.gcount();
        if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(count), 0) < 0)
        {
            cleanup();
            error = "could not update SHA256";
            return {};
        }
    }
    if (!input.eof() || BCryptFinishHash(hash, digest.data(), DWORD(digest.size()), 0) < 0)
    {
        cleanup();
        error = "could not finish SHA256";
        return {};
    }
    cleanup();
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2);
    for (const auto value : digest)
    {
        result.push_back(hex[value >> 4]);
        result.push_back(hex[value & 15]);
    }
    return result;
#else
    (void)path;
    error = "SHA256 is not implemented for this platform";
    return {};
#endif
}
} // namespace updater
