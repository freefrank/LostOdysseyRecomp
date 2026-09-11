// External instruction-pointer sampler. Hits include waiting threads and are
// NOT CPU percentages. Target threads are resumed before any allocation or IO.
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if !defined(_M_X64) && !defined(__x86_64__)
#error This collector requires an x64 compiler target.
#endif

namespace {
volatile LONG interrupted = 0;
BOOL WINAPI control_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        InterlockedExchange(&interrupted, 1);
        return TRUE;
    }
    return FALSE;
}
struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE h = nullptr) : value(h) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};
struct Suspension {
    HANDLE thread;
    bool active;
    explicit Suspension(HANDLE h) : thread(h), active(SuspendThread(h) != DWORD(-1)) {}
    bool resume() {
        if (!active) return true;
        if (ResumeThread(thread) == DWORD(-1)) return false;
        active = false;
        return true;
    }
    ~Suspension() { if (active) ResumeThread(thread); }
};
std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
std::string quote(const std::string& text) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : text) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
            else out << c;
        }
    }
    out << '"';
    return out.str();
}
std::string hex(std::uint64_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << value;
    return out.str();
}
std::uint64_t ticks(FILETIME time) {
    return (std::uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}
double number(const std::wstring& text) {
    size_t end = 0;
    double value = std::stod(text, &end);
    if (end != text.size() || !std::isfinite(value)) throw std::runtime_error("Invalid numeric argument");
    return value;
}
struct Sample { DWORD tid; DWORD64 ip; double elapsed_ms; };
struct CpuTime { std::uint64_t created, first, last; };
struct Resolved {
    std::string module, symbol, source, bytes;
    DWORD64 base = 0, displacement = 0;
    DWORD line = 0;
};
struct Symbols {
    HANDLE process;
    bool initialized;
    explicit Symbols(HANDLE h, const std::wstring& search_path) : process(h) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
        initialized = SymInitializeW(h, search_path.c_str(), TRUE) != FALSE;
    }
    ~Symbols() { if (initialized) SymCleanup(process); }
};
Resolved resolve(HANDLE process, DWORD64 ip, bool symbols) {
    Resolved result;
    if (symbols) {
        IMAGEHLP_MODULEW64 module{};
        module.SizeOfStruct = sizeof(module);
        if (SymGetModuleInfoW64(process, ip, &module)) {
            result.module = utf8(module.LoadedImageName[0] ? module.LoadedImageName : module.ImageName);
            result.base = module.BaseOfImage;
        }
        alignas(SYMBOL_INFOW) unsigned char storage[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFOW*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
        symbol->MaxNameLen = MAX_SYM_NAME;
        if (SymFromAddrW(process, ip, &result.displacement, symbol))
            result.symbol = utf8(std::wstring(symbol->Name, symbol->NameLen));
        IMAGEHLP_LINEW64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD displacement = 0;
        if (SymGetLineFromAddrW64(process, ip, &displacement, &line)) {
            result.source = utf8(line.FileName);
            result.line = line.LineNumber;
        }
    }
    // Bound the read to the current committed region, including at page ends.
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQueryEx(process, reinterpret_cast<LPCVOID>(ip), &region, sizeof(region)) && region.State == MEM_COMMIT) {
        auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
        SIZE_T size = static_cast<SIZE_T>(std::min<DWORD64>(32, end - ip));
        unsigned char bytes[32]{};
        SIZE_T read = 0;
        ReadProcessMemory(process, reinterpret_cast<LPCVOID>(ip), bytes, size, &read);
        std::ostringstream out;
        for (SIZE_T i = 0; i < read; ++i) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(bytes[i]);
        result.bytes = out.str();
    }
    return result;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        DWORD pid = 0;
        double seconds = 10, interval_ms = 10;
        bool overwrite = false;
        std::filesystem::path output = L"asm-profile.json";
        for (int i = 1; i < argc; ++i) {
            std::wstring key = argv[i];
            if (key == L"--help" || key == L"-h") {
                std::cout << "lo_asm_profiler --pid PID [--seconds 10] [--interval-ms 10] [--output asm-profile.json] [--overwrite]\n"
                             "Samples every live thread, including waits. Hits are not CPU percentages.\n"
                             "Requires access to suspend/read the target. Does not launch or focus it.\n";
                return 0;
            }
            if (key == L"--overwrite") { overwrite = true; continue; }
            if (i + 1 >= argc) throw std::runtime_error("Missing option value");
            std::wstring value = argv[++i];
            if (key == L"--pid") {
                double parsed = number(value);
                if (parsed < 1 || parsed > MAXDWORD || std::floor(parsed) != parsed) throw std::runtime_error("Invalid PID");
                pid = static_cast<DWORD>(parsed);
            } else if (key == L"--seconds") seconds = number(value);
            else if (key == L"--interval-ms") interval_ms = number(value);
            else if (key == L"--output") output = value;
            else throw std::runtime_error("Unknown option: " + utf8(key));
        }
        if (!pid || pid == GetCurrentProcessId()) throw std::runtime_error("Specify a non-self --pid");
        if (seconds <= 0 || seconds > 3600 || interval_ms < 1 || interval_ms > 10000)
            throw std::runtime_error("--seconds must be (0,3600]; --interval-ms must be [1,10000]");
        Handle process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid));
        if (!process) throw std::runtime_error("OpenProcess failed: " + std::to_string(GetLastError()));
        USHORT machine = 0, native_machine = 0;
        if (!IsWow64Process2(process.value, &machine, &native_machine) ||
            (machine != IMAGE_FILE_MACHINE_AMD64 && !(machine == IMAGE_FILE_MACHINE_UNKNOWN && native_machine == IMAGE_FILE_MACHINE_AMD64)))
            throw std::runtime_error("Target must be an x64 process (Windows 10 1709 or newer required)");
        wchar_t executable[32768]{};
        DWORD length = 32768;
        if (!QueryFullProcessImageNameW(process.value, 0, executable, &length)) throw std::runtime_error("Cannot identify target executable");
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!GetProcessTimes(process.value, &created, &exited, &kernel, &user)) throw std::runtime_error("Cannot read target creation time");
        // Validate output before suspending any target thread.
        if (std::filesystem::exists(output)) {
            if (std::filesystem::equivalent(output, std::filesystem::path(executable))) throw std::runtime_error("Output cannot replace target executable");
            if (!overwrite) throw std::runtime_error("Output already exists; choose another path or use --overwrite");
        }
        std::ofstream out(output, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot open output file");
        SetConsoleCtrlHandler(control_handler, TRUE);
        std::vector<Sample> samples;
        samples.reserve(65536);
        std::map<DWORD, CpuTime> cpu;
        std::uint64_t attempted = 0, failed = 0, snapshot_failures = 0;
        bool target_exited = false, limit_reached = false, resume_failed = false;
        constexpr size_t max_samples = 2000000;
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        const auto deadline = start + std::chrono::duration<double>(seconds);
        auto next = start;
        std::cerr << "Sampling PID " << pid << " for " << seconds << " seconds; Ctrl+C stops and writes results.\n";
        while (Clock::now() < deadline && !InterlockedCompareExchange(&interrupted, 0, 0)) {
            if (WaitForSingleObject(process.value, 0) != WAIT_TIMEOUT) { target_exited = true; break; }
            Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0));
            if (!snapshot) { ++snapshot_failures; }
            else {
                THREADENTRY32 entry{};
                entry.dwSize = sizeof(entry);
                BOOL found = Thread32First(snapshot.value, &entry);
                while (found) {
                    if (entry.th32OwnerProcessID == pid) {
                        if (Clock::now() >= deadline || InterlockedCompareExchange(&interrupted, 0, 0)) break;
                        if (WaitForSingleObject(process.value, 0) != WAIT_TIMEOUT) { target_exited = true; break; }
                        ++attempted;
                        Handle thread(OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID));
                        if (!thread || GetProcessIdOfThread(thread.value) != pid) { ++failed; }
                        else {
                            FILETIME tc{}, te{}, tk{}, tu{};
                            if (GetThreadTimes(thread.value, &tc, &te, &tk, &tu)) {
                                auto total = ticks(tk) + ticks(tu);
                                auto it = cpu.find(entry.th32ThreadID);
                                if (it == cpu.end() || it->second.created != ticks(tc)) cpu[entry.th32ThreadID] = {ticks(tc), total, total};
                                else it->second.last = total;
                            }
                            CONTEXT context{};
                            context.ContextFlags = CONTEXT_CONTROL;
                            bool captured = false;
                            {
                                Suspension suspension(thread.value);
                                if (suspension.active) captured = GetThreadContext(thread.value, &context) != FALSE;
                                if (!suspension.resume()) { resume_failed = true; captured = false; }
                            }
                            // All target suspension scopes end before vector allocation.
                            if (captured) samples.push_back({entry.th32ThreadID, context.Rip, std::chrono::duration<double, std::milli>(Clock::now() - start).count()});
                            else ++failed;
                        }
                        if (samples.size() >= max_samples) { limit_reached = true; break; }
                        if (resume_failed) break;
                    }
                    found = Thread32Next(snapshot.value, &entry);
                }
            }
            if (limit_reached || resume_failed || target_exited) break;
            next += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double, std::milli>(interval_ms));
            // Do not burst to catch up after an expensive sweep. Sleep in short
            // chunks so Ctrl+C and process exit remain responsive.
            if (next < Clock::now()) next = Clock::now();
            while (Clock::now() < next && Clock::now() < deadline && !InterlockedCompareExchange(&interrupted, 0, 0))
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        double duration = std::chrono::duration<double>(Clock::now() - start).count();
        target_exited = target_exited || WaitForSingleObject(process.value, 0) != WAIT_TIMEOUT;
        std::cerr << "Captured " << samples.size() << " samples; resolving symbols.\n";
        Symbols symbols(process.value, std::filesystem::path(executable).parent_path().wstring());
        FILETIME code_snapshot_started{};
        GetSystemTimeAsFileTime(&code_snapshot_started);
        std::map<DWORD64, Resolved> resolved;
        for (const auto& sample : samples)
            if (!resolved.count(sample.ip)) resolved.emplace(sample.ip, resolve(process.value, sample.ip, symbols.initialized));
        out << "{\n  \"schema_version\":1,\n  \"pid\":" << pid
            << ",\n  \"executable\":" << quote(utf8(std::wstring(executable, length)))
            << ",\n  \"process_create_time_filetime\":" << quote(std::to_string(ticks(created)))
            << ",\n  \"duration_seconds\":" << duration << ",\n  \"requested_seconds\":" << seconds
            << ",\n  \"interval_ms\":" << interval_ms
            << ",\n  \"attempted_samples\":" << attempted << ",\n  \"failed_samples\":" << failed
            << ",\n  \"snapshot_failures\":" << snapshot_failures
            << ",\n  \"target_exited\":" << (target_exited ? "true" : "false")
            << ",\n  \"sample_limit_reached\":" << (limit_reached ? "true" : "false")
            << ",\n  \"resume_failed\":" << (resume_failed ? "true" : "false")
            << ",\n  \"symbols_initialized\":" << (symbols.initialized ? "true" : "false")
            << ",\n  \"code_snapshot_phase\":\"after_sampling\""
            << ",\n  \"code_bytes_timing\":\"post_capture\""
            << ",\n  \"module_mapping_timing\":\"post_capture; unloaded or reloaded modules may not match sampling time\""
            << ",\n  \"code_snapshot_started_filetime\":" << quote(std::to_string(ticks(code_snapshot_started)))
            << ",\n  \"sampling_method\":\"wall_clock_all_threads_suspend_context\",\n  \"thread_cpu_times\":[";
        bool first = true;
        for (const auto& item : cpu) {
            if (!first) out << ',';
            first = false;
            out << "{\"tid\":" << item.first << ",\"observed_cpu_seconds\":" << double(item.second.last - item.second.first) / 1e7 << '}';
        }
        out << "],\n  \"samples\":[\n";
        first = true;
        for (const auto& sample : samples) {
            const auto& r = resolved.at(sample.ip);
            if (!first) out << ",\n";
            first = false;
            out << "    {\"tid\":" << sample.tid << ",\"ip\":" << quote(hex(sample.ip))
                << ",\"elapsed_ms\":" << sample.elapsed_ms << ",\"module\":" << quote(r.module)
                << ",\"module_base\":" << quote(hex(r.base)) << ",\"symbol\":" << quote(r.symbol)
                << ",\"displacement\":" << quote(hex(r.displacement)) << ",\"source\":" << quote(r.source)
                << ",\"line\":" << r.line << ",\"bytes\":" << quote(r.bytes) << '}';
        }
        out << "\n  ]\n}\n";
        out.close();
        if (!out) throw std::runtime_error("Writing output failed");
        std::cerr << "Wrote " << samples.size() << " samples to " << output.string() << '\n';
        if (resume_failed) { std::cerr << "ResumeThread failed; collection stopped.\n"; return 2; }
        return samples.empty() ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
