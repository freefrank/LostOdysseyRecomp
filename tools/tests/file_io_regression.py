#!/usr/bin/env python3
"""Exercise production file APIs without the guest runtime or private game data.

The guest endian types and API bodies are real. Handle lookup, diagnostics and
completion delivery use fixtures; host read errors use a real stdio stream.
Seek/flush/resize errors are injected at their host-call boundary. This is not
an import-bridge or gameplay integration test.

Run: python3 tools/tests/file_io_regression.py --cxx g++ --sanitize
"""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def between(text: str, begin: str, end: str) -> str:
    if text.count(begin) != 1 or text.count(end) != 1:
        raise RuntimeError(f"Production extraction boundaries changed: {begin!r}, {end!r}")
    start, finish = text.index(begin), text.index(end)
    if finish <= start:
        raise RuntimeError("Production extraction boundaries reversed")
    return text[start:finish]


COMMON = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <type_traits>
#include <vector>
#include <unistd.h>
#include "byteswap.h"
// EXTRACTED_GUEST_TYPES
namespace modding::text_overlay {
struct Range { uint64_t offset = 0; std::shared_ptr<const std::vector<uint8_t>> bytes; };
}
constexpr uint64_t PPC_MEMORY_SIZE = 0x100000000ull;
constexpr uint32_t STATUS_SUCCESS = 0, STATUS_END_OF_FILE = 0xC0000011u,
    STATUS_INVALID_HANDLE = 0xC0000008u, STATUS_NOT_IMPLEMENTED = 0xC0000002u,
    STATUS_INVALID_PARAMETER = 0xC000000Du, STATUS_ACCESS_DENIED = 0xC0000022u,
    STATUS_NO_MORE_FILES = 0x80000006u;
template<class T> T RoundUp(T value, T alignment) { return (value + alignment - 1) & ~(alignment - 1); }
struct MockMemory {
    std::array<uint8_t, 0x10000> bytes{};
    void* Translate(size_t address) {
        if (address >= bytes.size()) throw std::runtime_error("unexpected translation");
        return bytes.data() + address;
    }
    uint32_t MapVirtual(const void*) { return 0x8000; }
} g_memory;
void* MmGetHostAddress(uint32_t address) { return g_memory.Translate(address); }
namespace kernel::wait {
struct Target { virtual ~Target() = default; };
struct Event : Target { Event(bool, bool) {} };
}
struct KernelObject {
    virtual ~KernelObject() = default;
    virtual kernel::wait::Target* WaitTarget() { return nullptr; }
};
namespace io_diagnostics {
enum class Stage { Destroyed, TransferDone, CompletionPublished };
struct Object {};
bool Enabled() { return false; }
template<class... T> void RecordEvent(T...) {}
struct Request {
    template<class... T> Request(T...) {}
    template<class... T> void Acquired(T...) {}
    void SetRequestedOffset(uint64_t) {}
    void SetResolvedOffset(uint64_t) {}
    void SetResult(uint32_t, uint32_t = 0) {}
    void SetStage(Stage) {}
    void LockWaiting() {}
    void LockAcquired() {}
    void LockReleased() {}
};
}
uint32_t IoGuestPcr() { return 0; }
#define IO_TEST_STAGE(...) ((void)0)
#define LOG_KERNEL(...) ((void)0)
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
static bool failSeek = false, failFlush = false, failResize = false;
static unsigned readCalls = 0, resizeCalls = 0;
static int HostSeek(FILE* stream, int64_t offset, int origin) {
    if (failSeek) { errno = EIO; return -1; }
    return fseeko(stream, off_t(offset), origin);
}
static size_t HostRead(void* buffer, size_t size, size_t count, FILE* stream) {
    ++readCalls;
    return fread(buffer, size, count, stream);
}
static int HostFlush(FILE* stream) {
    if (failFlush) { errno = ENOSPC; return EOF; }
    return fflush(stream);
}
static int HostResize(int fd, off_t length) {
    ++resizeCalls;
    if (failResize) { errno = EIO; return -1; }
    return ftruncate(fd, length);
}
#define _fseeki64 HostSeek
#define fread HostRead
#define fflush HostFlush
#define ftruncate HostResize
#define LOG_WARNING(...) ((void)0)
// FileHandle syncs save files before closing; the fixture flushes through HostFlush.
struct FileSystem {
    static bool SyncFile(FILE* file) { return fflush(file) == 0; }
    static std::string PathUtf8(const std::filesystem::path& path) { return path.string(); }
};
static std::filesystem::path g_gameRoot;
static uint64_t ToFileTime(std::filesystem::file_time_type) { return 123; }
'''

FIXTURES = r'''
static std::shared_ptr<FileHandle> currentHandle;
template<class T> std::shared_ptr<T> GetKernelObject(uint32_t handle) {
    return handle == 1 ? currentHandle : nullptr;
}
static unsigned apcs = 0, events = 0;
static uint32_t completedStatus = 0, completedBytes = 0;
void EnqueueUserApc(uint32_t, uint32_t, uint32_t, uint32_t) { ++apcs; }
static XIO_STATUS_BLOCK* completionBlock = nullptr;
void KernelSignalEventHandle(uint32_t) {
    ++events;
    if (completionBlock) {
        completedStatus = completionBlock->Status;
        completedBytes = completionBlock->Information;
    }
}
std::string GuestAnsiString(XANSI_STRING* value) {
    return value && value->Buffer.get() ? std::string(value->Buffer.get(), value->Length) : "";
}
'''

TESTS = r'''
#undef fread
#undef fflush
#undef ftruncate
static unsigned checks = 0;
static void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
static void Canary(const std::array<uint8_t, 128>& bytes, size_t start) {
    Check(std::all_of(bytes.begin() + start, bytes.end(), [](auto b) { return b == 0xA5; }),
        "API wrote outside actual output bytes");
}
static void FixedQuery(bool volume, unsigned type, unsigned size) {
    alignas(8) std::array<uint8_t, 128> bytes;
    XIO_STATUS_BLOCK iosb{};
    for (unsigned length = 0; length <= size + 1; ++length) {
        bytes.fill(0xA5);
        auto status = volume ? NtQueryVolumeInformationFile(1, &iosb, bytes.data(), length, type)
                             : NtQueryInformationFile(1, &iosb, bytes.data(), length, type);
        Check(status == (length < size ? kStatusInfoLengthMismatch : STATUS_SUCCESS), "fixed query length status");
        Check(uint32_t(iosb.Status) == status, "fixed query completion status");
        Check(uint32_t(iosb.Information) == (length < size ? 0u : size), "fixed query completion byte count");
        Canary(bytes, length < size ? 0 : size);
    }
    auto status = volume ? NtQueryVolumeInformationFile(1, &iosb, nullptr, size, type)
                         : NtQueryInformationFile(1, &iosb, nullptr, size, type);
    Check(status == STATUS_INVALID_PARAMETER && uint32_t(iosb.Information) == 0, "null fixed query output rejected");
}
static void VariableQuery(bool volume) {
    const unsigned header = volume ? 12 : 4;
    const std::string payload = volume ? "FATX" : "\\" + currentHandle->path.filename().string();
    const unsigned type = volume ? unsigned(FileFsAttributeInformation) : unsigned(FileNameInformation);
    alignas(8) std::array<uint8_t, 128> bytes;
    XIO_STATUS_BLOCK iosb{};
    for (unsigned length = 0; length <= header + payload.size() + 1; ++length) {
        bytes.fill(0xA5);
        const auto status = volume ? NtQueryVolumeInformationFile(1, &iosb, bytes.data(), length, type)
                                   : NtQueryInformationFile(1, &iosb, bytes.data(), length, type);
        const auto expected = length < header ? kStatusInfoLengthMismatch
                              : length < header + payload.size() ? kStatusBufferOverflow : STATUS_SUCCESS;
        const unsigned written = length < header ? 0 : std::min<unsigned>(length, header + payload.size());
        Check(status == expected && uint32_t(iosb.Status) == status, "variable query status");
        Check(uint32_t(iosb.Information) == written, "variable query reports actual bytes");
        Canary(bytes, written);
        if (written) {
            Check(uint32_t(*reinterpret_cast<be<uint32_t>*>(bytes.data() + header - 4)) == payload.size(),
                "variable query preserves full required payload length");
            Check(!memcmp(bytes.data() + header, payload.data(), written - header), "variable query copies available prefix");
        }
    }
    const auto status = volume ? NtQueryVolumeInformationFile(1, &iosb, nullptr, header, type)
                               : NtQueryInformationFile(1, &iosb, nullptr, header, type);
    Check(status == STATUS_INVALID_PARAMETER && uint32_t(iosb.Information) == 0, "null variable query output rejected");
}
static void FileFixture() {
    currentHandle = std::make_shared<FileHandle>();
    currentHandle->file = tmpfile();
    Check(currentHandle->file != nullptr, "create real file fixture");
    Check(fwrite("ABCDEF", 1, 6, currentHandle->file) == 6 && fflush(currentHandle->file) == 0, "write fixture");
    currentHandle->size = 6;
    currentHandle->position = 2;
    currentHandle->writable = true;
    currentHandle->path = "fixture.bin";
}
static void Complete(uint32_t status, XIO_STATUS_BLOCK& iosb, uint32_t expected, unsigned count, unsigned oldEvents) {
    Check(status == expected && uint32_t(iosb.Status) == expected && uint32_t(iosb.Information) == count,
        "read and completion status/count disagree");
    Check(events == oldEvents + 1 && completedStatus == expected && completedBytes == count,
        "event signalled before publishing actual status");
}
static void ReadCases(bool scatter) {
    FileFixture();
    XIO_STATUS_BLOCK iosb{}; completionBlock = &iosb;
    be<uint64_t> offset = 0, segments[] = {0x1000, 0x3000};
    std::array<char, 8> output{};
    auto read = [&](unsigned length, be<uint64_t>* requested = nullptr) {
        return scatter ? NtReadFileScatter(1, 2, 4, 8, &iosb, segments, length, requested)
                       : NtReadFile(1, 2, 4, 8, &iosb, output.data(), length, requested);
    };
    for (uint64_t bad : {UINT64_MAX, uint64_t(std::numeric_limits<int64_t>::max()) + 1}) {
        offset = bad;
        const auto oldEvents = events, oldReads = readCalls, oldApcs = apcs;
        Complete(read(2, &offset), iosb, STATUS_INVALID_PARAMETER, 0, oldEvents);
        Check(currentHandle->position == 2 && readCalls == oldReads && apcs == oldApcs, "invalid offset mutated/read file");
    }
    failSeek = true; offset = 4;
    auto oldEvents = events, oldReads = readCalls;
    Complete(read(2, &offset), iosb, kStatusIoDeviceError, 0, oldEvents);
    Check(currentHandle->position == 2 && readCalls == oldReads, "seek failure read from old file position");
    failSeek = false;
    offset = 0xFFFFFFFFFFFFFFFEull; oldEvents = events;
    Complete(read(2, &offset), iosb, STATUS_SUCCESS, 2, oldEvents);
    Check(currentHandle->position == 4, "file pointer sentinel not respected");
    Check(scatter ? memcmp(g_memory.bytes.data() + 0x1000, "CD", 2) == 0 : memcmp(output.data(), "CD", 2) == 0,
        "positioned read returned wrong data");
    offset = 4; oldEvents = events;
    Complete(read(8, &offset), iosb, STATUS_SUCCESS, 2, oldEvents);
    Check(currentHandle->position == 6, "partial EOF read position");
    oldEvents = events;
    Complete(read(8), iosb, STATUS_END_OF_FILE, 0, oldEvents);
    oldEvents = events; oldReads = readCalls;
    Complete(read(0), iosb, STATUS_SUCCESS, 0, oldEvents);
    Check(readCalls == oldReads && currentHandle->position == 6, "zero-length read changed position");
    oldEvents = events;
    auto status = scatter ? NtReadFileScatter(1, 2, 0, 0, &iosb, nullptr, 0, nullptr)
                          : NtReadFile(1, 2, 0, 0, &iosb, nullptr, 0, nullptr);
    Complete(status, iosb, STATUS_SUCCESS, 0, oldEvents);
    Check(currentHandle->position == 6, "zero-length null buffer changed position");
    oldEvents = events;
    status = scatter ? NtReadFileScatter(1, 2, 0, 0, &iosb, nullptr, 1, nullptr)
                     : NtReadFile(1, 2, 0, 0, &iosb, nullptr, 1, nullptr);
    Complete(status, iosb, STATUS_INVALID_PARAMETER, 0, oldEvents);
    const auto oldApcs = apcs;
    status = scatter ? NtReadFileScatter(1, 0, 4, 8, nullptr, nullptr, 0, nullptr)
                     : NtReadFile(1, 0, 4, 8, nullptr, nullptr, 0, nullptr);
    Check(status == STATUS_SUCCESS && apcs == oldApcs, "APC queued with missing status block");
    if (scatter) {
        for (uint64_t badPointer : {uint64_t(0), PPC_MEMORY_SIZE, PPC_MEMORY_SIZE - 1}) {
            segments[0] = badPointer; offset = 0; oldEvents = events;
            Complete(read(2, &offset), iosb, STATUS_INVALID_PARAMETER, 0, oldEvents);
        }
        // PVOID64 carries a 32-bit guest pointer; a sign-extended high word is not an error.
        segments[0] = 0xFFFFFFFF00001000ull; offset = 0; oldEvents = events;
        memset(g_memory.bytes.data() + 0x1000, 0, 2);
        Complete(read(2, &offset), iosb, STATUS_SUCCESS, 2, oldEvents);
        Check(memcmp(g_memory.bytes.data() + 0x1000, "AB", 2) == 0, "sign-extended segment pointer not truncated");
        segments[0] = 0x1000;
    }
    // A real stream opened write-only makes fread set ferror, not feof.
    fclose(currentHandle->file);
    currentHandle->file = fopen("write-only.bin", "wb");
    Check(currentHandle->file != nullptr, "open write-only error fixture");
    currentHandle->position = 2; offset = 0; oldEvents = events;
    Complete(read(2, &offset), iosb, kStatusIoDeviceError, 0, oldEvents);
    Check(ferror(currentHandle->file) && !feof(currentHandle->file) && currentHandle->position == 2,
        "real read failure treated as EOF or changed logical position");
    completionBlock = nullptr;
}
static void InvalidHandleCases() {
    XIO_STATUS_BLOCK iosb{};
    iosb.Status = 0xA5A5A5A5u; iosb.Information = 0xA5A5A5A5u;
    auto* unreadableOffset = reinterpret_cast<be<uint64_t>*>(uintptr_t(1));
    Check(NtReadFile(0, 0, 0, 0, &iosb, nullptr, 1, unreadableOffset) == STATUS_INVALID_HANDLE,
        "read did not reject invalid handle before offset access");
    Check(NtReadFileScatter(0, 0, 0, 0, &iosb, nullptr, 1, unreadableOffset) == STATUS_INVALID_HANDLE,
        "scatter did not reject invalid handle before offset access");
    Check(NtQueryInformationFile(0, &iosb, nullptr, 8, FilePositionInformation) == STATUS_INVALID_HANDLE &&
        NtSetInformationFile(0, &iosb, nullptr, 8, FilePositionInformation) == STATUS_INVALID_HANDLE &&
        NtQueryVolumeInformationFile(0, &iosb, nullptr, 8, FileFsDeviceInformation) == STATUS_INVALID_HANDLE,
        "information API accepted invalid handle");
    Check(uint32_t(iosb.Status) == 0xA5A5A5A5u && uint32_t(iosb.Information) == 0xA5A5A5A5u,
        "invalid handle rejection changed caller output");
}
struct ErrorCookie {
    std::vector<uint8_t> bytes;
    uint64_t position = 0, failAfter = UINT64_MAX;
};
static ssize_t CookieRead(void* opaque, char* destination, size_t count) {
    auto& cookie = *static_cast<ErrorCookie*>(opaque);
    if (cookie.position >= cookie.failAfter) { errno = EIO; return -1; }
    if (cookie.position >= cookie.bytes.size()) return 0;
    count = std::min<uint64_t>(count, cookie.bytes.size() - cookie.position);
    count = std::min<uint64_t>(count, cookie.failAfter - cookie.position);
    memcpy(destination, cookie.bytes.data() + cookie.position, count);
    cookie.position += count;
    return ssize_t(count);
}
static int CookieSeek(void* opaque, off64_t* offset, int origin) {
    auto& cookie = *static_cast<ErrorCookie*>(opaque);
    const int64_t base = origin == SEEK_CUR ? int64_t(cookie.position)
                       : origin == SEEK_END ? int64_t(cookie.bytes.size()) : 0;
    if (*offset < -base) { errno = EINVAL; return -1; }
    cookie.position = base + *offset;
    *offset = off64_t(cookie.position);
    return 0;
}
static int CookieClose(void* opaque) { delete static_cast<ErrorCookie*>(opaque); return 0; }
static void MultiPageReadCases(bool scatter) {
    for (bool error : {false, true}) {
        FileFixture();
        fclose(currentHandle->file);
        currentHandle->file = nullptr;
        auto* cookie = new ErrorCookie;
        cookie->bytes.resize(error ? 8192 : 4098);
        cookie->failAfter = error ? 4113 : UINT64_MAX;
        for (size_t i = 0; i < cookie->bytes.size(); ++i) cookie->bytes[i] = uint8_t(i * 7);
        cookie_io_functions_t functions{};
        functions.read = CookieRead; functions.seek = CookieSeek; functions.close = CookieClose;
        currentHandle->file = fopencookie(cookie, "rb", functions);
        if (!currentHandle->file) { delete cookie; throw std::runtime_error("fopencookie fixture failed"); }
        XIO_STATUS_BLOCK iosb{}; completionBlock = &iosb;
        be<uint64_t> offset = 0, segments[] = {0x1000, 0x3000};
        std::array<uint8_t, 8192> output{};
        const auto oldEvents = events, oldApcs = apcs;
        const auto status = scatter ? NtReadFileScatter(1, 2, 4, 8, &iosb, segments, 8192, &offset)
                                    : NtReadFile(1, 2, 4, 8, &iosb, output.data(), 8192, &offset);
        const unsigned count = error ? 4113 : 4098;
        Complete(status, iosb, error ? kStatusIoDeviceError : STATUS_SUCCESS, count, oldEvents);
        Check(currentHandle->position == count, "partial transfer did not advance by actual bytes");
        Check(apcs == oldApcs + (error ? 0 : 1), "APC delivery changed for failed read");
        Check(!scatter || memcmp(g_memory.bytes.data() + 0x1000, cookie->bytes.data(), 4096) == 0,
            "first scatter page bytes");
        Check(scatter ? memcmp(g_memory.bytes.data() + 0x3000, cookie->bytes.data() + 4096, count - 4096) == 0
                      : memcmp(output.data(), cookie->bytes.data(), count) == 0, "partial transfer bytes");
        completionBlock = nullptr;
    }
}
static uint64_t HostSize() {
    Check(fseeko(currentHandle->file, 0, SEEK_END) == 0, "seek end for resize validation");
    return uint64_t(ftello(currentHandle->file));
}
static void SetCases() {
    FileFixture();
    XIO_STATUS_BLOCK iosb{}; be<uint64_t> size = 3;
    for (unsigned type : {FilePositionInformation, FileEndOfFileInformation, FileAllocationInformation}) {
        for (unsigned length = 0; length < 8; ++length) {
            const auto status = NtSetInformationFile(1, &iosb, &size, length, type);
            Check(status == kStatusInfoLengthMismatch && uint32_t(iosb.Status) == status && uint32_t(iosb.Information) == 0,
                "short input accepted by set API");
            Check(currentHandle->position == 2 && currentHandle->size == 6 && HostSize() == 6,
                "short input mutated position or file size");
        }
        Check(NtSetInformationFile(1, &iosb, nullptr, 8, type) == STATUS_INVALID_PARAMETER, "null set input rejected");
    }
    size = UINT64_MAX;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileEndOfFileInformation) == STATUS_INVALID_PARAMETER &&
        currentHandle->size == 6 && HostSize() == 6, "unrepresentable resize changed file");
    Check(NtSetInformationFile(1, &iosb, &size, 8, FilePositionInformation) == STATUS_INVALID_PARAMETER &&
        currentHandle->position == 2, "unrepresentable position accepted");
    size = 3; currentHandle->writable = false;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileEndOfFileInformation) == STATUS_ACCESS_DENIED &&
        currentHandle->size == 6 && HostSize() == 6, "readonly resize reported success");
    currentHandle->writable = true; failFlush = true;
    const auto oldResizes = resizeCalls;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileEndOfFileInformation) == 0xC000007Fu &&
        resizeCalls == oldResizes && currentHandle->size == 6 && HostSize() == 6, "flush failure resized file or updated size");
    failFlush = false; failResize = true;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileEndOfFileInformation) == kStatusIoDeviceError &&
        currentHandle->size == 6 && HostSize() == 6, "failed truncate updated logical size");
    failResize = false;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileEndOfFileInformation) == STATUS_SUCCESS &&
        currentHandle->size == 3 && HostSize() == 3, "successful shrink failed");
    size = 9;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileEndOfFileInformation) == STATUS_SUCCESS &&
        currentHandle->size == 9 && HostSize() == 9, "successful extend failed");
    size = 4096;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileAllocationInformation) == STATUS_SUCCESS &&
        currentHandle->size == 9 && HostSize() == 9, "allocation hint changed EOF");
    currentHandle->writable = false;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FileAllocationInformation) == STATUS_ACCESS_DENIED,
        "readonly allocation hint accepted");
    size = 1;
    Check(NtSetInformationFile(1, &iosb, &size, 8, FilePositionInformation) == STATUS_SUCCESS &&
        currentHandle->position == 1, "successful position update failed");
}
static void DirectoryCases() {
    std::filesystem::create_directory("directory");
    std::ofstream("directory/a.bin") << "a";
    std::ofstream("directory/b.bin") << "bb";
    currentHandle = std::make_shared<FileHandle>();
    currentHandle->path = "directory"; currentHandle->isDirectory = true;
    // Deterministic order; directory iterator order is host-dependent.
    currentHandle->entries = {std::filesystem::directory_entry("directory/a.bin"),
        std::filesystem::directory_entry("directory/b.bin")};
    currentHandle->enumerated = true;
    alignas(8) std::array<uint8_t, 128> bytes;
    XIO_STATUS_BLOCK iosb{};
    bytes.fill(0xA5);
    Check(NtQueryDirectoryFile(1, 0, 0, 0, &iosb, bytes.data(), 1, nullptr, 0) == kStatusBufferOverflow &&
        currentHandle->nextEntry == 0 && uint32_t(iosb.Information) == 0, "overflow consumed directory entry");
    Canary(bytes, 0);
    for (unsigned index = 0; index != 2; ++index) {
        bytes.fill(0xA5);
        Check(NtQueryDirectoryFile(1, 0, 0, 0, &iosb, bytes.data(), bytes.size(), nullptr, 0) == STATUS_SUCCESS,
            "expanded directory query failed");
        const auto* info = reinterpret_cast<const X_FILE_DIRECTORY_INFORMATION*>(bytes.data());
        Check(std::string(info->fileName, info->fileNameLength) == (index ? "b.bin" : "a.bin"),
            "directory retry skipped original entry");
        Check(uint32_t(info->fileIndex) == index + 1, "directory file index changed");
        Canary(bytes, iosb.Information);
    }
    Check(NtQueryDirectoryFile(1, 0, 0, 0, &iosb, bytes.data(), bytes.size(), nullptr, 0) == STATUS_NO_MORE_FILES,
        "directory enumeration completion failed");
    currentHandle->searchPattern = "b.*"; currentHandle->nextEntry = 0;
    Check(NtQueryDirectoryFile(1, 0, 0, 0, &iosb, bytes.data(), 1, nullptr, 0) == kStatusBufferOverflow &&
        currentHandle->nextEntry == 1, "filtered directory match consumed on overflow");
    Check(NtQueryDirectoryFile(1, 0, 0, 0, &iosb, nullptr, bytes.size(), nullptr, 0) == STATUS_INVALID_PARAMETER &&
        currentHandle->nextEntry == 1, "null directory output consumed item");
}
// Language-pack ranges: bytes over the file, a gap of zeros past its end and a tail.
static void RangeReadCases(bool scatter) {
    FileFixture();
    using modding::text_overlay::Range;
    std::vector<uint8_t> tail(4096);
    for (size_t i = 0; i < tail.size(); ++i) tail[i] = uint8_t(i * 13 + 1);
    currentHandle->ranges = {Range{0, std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{'x', 'y'})},
                             Range{2048, std::make_shared<const std::vector<uint8_t>>(tail)}};
    currentHandle->fileSize = 6;
    currentHandle->size = 2048 + tail.size();
    std::vector<uint8_t> expected(currentHandle->size, 0);
    memcpy(expected.data(), "xyCDEF", 6);
    memcpy(expected.data() + 2048, tail.data(), tail.size());
    XIO_STATUS_BLOCK iosb{}; completionBlock = &iosb;
    be<uint64_t> offset = 0, segments[] = {0x1000, 0x3000};
    std::array<uint8_t, 8192> output{};
    g_memory.bytes.fill(0xA5);
    auto oldEvents = events;
    auto status = scatter ? NtReadFileScatter(1, 2, 4, 8, &iosb, segments, 8192, &offset)
                          : NtReadFile(1, 2, 4, 8, &iosb, output.data(), 8192, &offset);
    Complete(status, iosb, STATUS_SUCCESS, unsigned(expected.size()), oldEvents);
    Check(currentHandle->position == expected.size(), "range read did not stop at the extended size");
    Check(scatter ? !memcmp(g_memory.bytes.data() + 0x1000, expected.data(), 4096) &&
                        !memcmp(g_memory.bytes.data() + 0x3000, expected.data() + 4096, expected.size() - 4096)
                  : !memcmp(output.data(), expected.data(), expected.size()), "range read bytes");
    offset = expected.size();
    oldEvents = events;
    status = scatter ? NtReadFileScatter(1, 2, 4, 8, &iosb, segments, 4096, &offset)
                     : NtReadFile(1, 2, 4, 8, &iosb, output.data(), 4096, &offset);
    Complete(status, iosb, STATUS_END_OF_FILE, 0, oldEvents);
    completionBlock = nullptr;
}
int main(int argc, char** argv) try {
    Check(argc == 2, "fixture directory argument missing");
    std::filesystem::current_path(argv[1]);
    FileFixture();
    FixedQuery(false, FileInternalInformation, 8);
    FixedQuery(false, FilePositionInformation, 8);
    FixedQuery(false, FileStandardInformation, sizeof(X_FILE_STANDARD_INFORMATION));
    FixedQuery(false, FileBasicInformation, sizeof(X_FILE_BASIC_INFORMATION));
    FixedQuery(false, FileNetworkOpenInformation, sizeof(X_FILE_NETWORK_OPEN_INFORMATION));
    FixedQuery(false, FileAlignmentInformation, 4);
    FixedQuery(false, FileModeInformation, 4);
    FixedQuery(true, FileFsVolumeInformation, 18);
    FixedQuery(true, FileFsSizeInformation, 24);
    FixedQuery(true, FileFsDeviceInformation, 8);
    VariableQuery(false); VariableQuery(true);
    InvalidHandleCases();
    ReadCases(false); ReadCases(true);
    MultiPageReadCases(false); MultiPageReadCases(true);
    RangeReadCases(false); RangeReadCases(true);
    SetCases(); DirectoryCases();
    currentHandle.reset();
    std::cout << "PASS production file I/O: " << checks << " checks; buffer bounds, partial output, seek/read/resize errors, EOF, scatter, directory retry\n";
    return 0;
} catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
'''


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    source = (ROOT / "LostOdysseyRecomp/kernel/io/file_system.cpp").read_text()
    xbox = (ROOT / "tools/XenonRecomp/XenonUtils/xbox.h").read_text()
    guest_types = between(xbox, "template<typename T>\nstruct be", "template<typename TGuest>\nstruct HostObject")
    guest_types += between(xbox, "typedef struct _XANSI_STRING", "typedef struct _XOBJECT_ATTRIBUTES")
    guest_types += between(xbox, "typedef struct _XIO_STATUS_BLOCK", "typedef struct _XOVERLAPPED")
    helpers = between(source, "    constexpr uint32_t kStatusInfoLengthMismatch", "    std::filesystem::path g_gameRoot;")
    schemas = between(source, "    // Guest structures (big-endian in guest memory).", "    uint64_t ToFileTime(")
    handle = between(source, "struct FileHandle : KernelObject", "void FileSystem::TraceHandleClose(")
    lock = between(source, "class FileIoLock\n", "// Semantics follow Xenia")
    queue_apc = between(source, "static void QueueIoApc(", "namespace\n{")
    reads = between(source, "// Reads a handle that has language-pack ranges", "uint32_t NtReadFile(")
    reads += between(source, "uint32_t NtReadFile(", "uint32_t NtWriteFile(")
    query_set_directory = between(source, "uint32_t NtQueryInformationFile(", "uint32_t NtQueryFullAttributesFile(")
    scatter = between(source, "uint32_t NtReadFileScatter(", "GUEST_FUNCTION_HOOK(__imp__NtCreateFile,")
    cpp = COMMON.replace("// EXTRACTED_GUEST_TYPES", guest_types) + helpers + schemas + handle + lock + FIXTURES + queue_apc + reads + query_set_directory + scatter + TESTS
    with tempfile.TemporaryDirectory(prefix="lo-file-io-") as temporary:
        out = args.out.resolve() if args.out else Path(temporary)
        out.mkdir(parents=True, exist_ok=True)
        (out / "file_io.cpp").write_text(cpp)
        flags = ["-std=c++20", "-pthread", "-I" + str(ROOT / "tools/XenonRecomp/XenonUtils")]
        flags += ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitize else ["-O2"]
        subprocess.run([args.cxx, *flags, str(out / "file_io.cpp"), "-o", str(out / "file_io")], check=True, timeout=120)
        fixture = out / "fixtures"
        fixture.mkdir(exist_ok=True)
        return subprocess.run([str(out / "file_io"), str(fixture)], timeout=30).returncode


if __name__ == "__main__":
    raise SystemExit(main())
