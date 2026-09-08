#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace updater
{
enum class ProgressPhase { Verifying, CheckingPackage, Ready };
class ProgressWindow
{
public:
    explicit ProgressWindow(uint32_t language);
    ~ProgressWindow();
    ProgressWindow(const ProgressWindow &) = delete;
    ProgressWindow &operator=(const ProgressWindow &) = delete;

    void SetProgress(uint64_t completed, uint64_t total, std::wstring_view detail);
    void SetDownloadProgress(uint64_t completed, uint64_t total);
    void SetPhase(ProgressPhase phase);
    void SetPhase(std::wstring_view detail);
    bool Cancelled();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace updater
