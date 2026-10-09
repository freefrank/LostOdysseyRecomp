#include "log_collection.h"
#include <os/logger.h>
#include <version.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <thread>
#include <unordered_set>
#include <vector>
#include <fmt/format.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace os::log_collection
{
namespace
{
std::atomic<int> consent{-1};
constexpr const char* PreferenceFile = "log-collection.ini";
constexpr size_t MaxLineBytes = 2000;

char Lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }

// ASCII case-insensitive, as Windows compares paths.
size_t FindNoCase(std::string_view text, std::string_view needle, size_t from)
{
    if (needle.empty() || needle.size() > text.size()) return std::string_view::npos;
    for (size_t i = from; i + needle.size() <= text.size(); ++i)
    {
        size_t j = 0;
        while (j < needle.size() && Lower(text[i + j]) == Lower(needle[j])) ++j;
        if (j == needle.size()) return i;
    }
    return std::string_view::npos;
}

bool WordCharacter(char c) { return (c >= '0' && c <= '9') || (Lower(c) >= 'a' && Lower(c) <= 'z'); }

// Whole words only, so a short account name does not rewrite ordinary text.
void ReplaceAll(std::string& text, std::string_view from, std::string_view to)
{
    for (size_t at = FindNoCase(text, from, 0); at != std::string::npos;)
    {
        const size_t end = at + from.size();
        if ((at && WordCharacter(text[at - 1])) || (end < text.size() && WordCharacter(text[end])))
        {
            at = FindNoCase(text, from, at + 1);
            continue;
        }
        text.replace(at, from.size(), to);
        at = FindNoCase(text, from, at + to.size());
    }
}

// The folder after Users\ or home/ is an account name on every drive,
// including Proton's Z:\home\<name>.
void ReplaceAccountFolders(std::string& text)
{
    for (const std::string_view marker : {"\\users\\", "/users/", "\\home\\", "/home/"})
    {
        for (size_t at = FindNoCase(text, marker, 0); at != std::string::npos; at = FindNoCase(text, marker, at + 1))
        {
            const size_t start = at + marker.size();
            size_t end = start;
            while (end < text.size() && std::string_view("\\/ \"'").find(text[end]) == std::string_view::npos) ++end;
            if (end > start) text.replace(start, end - start, "<user>");
        }
    }
}

// Invalid sequences and control characters become '?', so the server's
// strict UTF-8 decoder accepts every request.
std::string ValidUtf8(std::string_view in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();)
    {
        const auto c = static_cast<unsigned char>(in[i]);
        if (c < 0x80)
        {
            out += (c < 0x20 && c != '\t') || c == 0x7F ? '?' : char(c);
            ++i;
            continue;
        }
        const size_t n = c >= 0xC2 && c <= 0xDF ? 2 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xF0 && c <= 0xF4 ? 4 : 0;
        bool ok = n && i + n <= in.size();
        for (size_t k = 1; ok && k < n; ++k) ok = (static_cast<unsigned char>(in[i + k]) & 0xC0) == 0x80;
        if (ok && n >= 3)
        {
            const auto next = static_cast<unsigned char>(in[i + 1]);
            ok = !(c == 0xE0 && next < 0xA0) && !(c == 0xED && next >= 0xA0) &&
                 !(c == 0xF0 && next < 0x90) && !(c == 0xF4 && next >= 0x90);
        }
        if (!ok)
        {
            out += '?';
            ++i;
            continue;
        }
        out.append(in.substr(i, n));
        i += n;
    }
    return out;
}

// Logger lines read "[  elapsed tXXXX] [note]  message"; crash reports start
// with "[crash]". Returns the line without its timestamp, or nothing.
std::optional<std::string_view> Selected(std::string_view line)
{
    if (line.starts_with("[crash]")) return line;
    const size_t close = line.find("] ");
    if (!line.starts_with('[') || close == std::string_view::npos) return std::nullopt;
    const auto rest = line.substr(close + 2);
    if (rest.starts_with("[note] ") || rest.starts_with("[error]")) return rest;
    if (rest.starts_with("[warn] "))
    {
        auto message = rest.substr(7);
        message.remove_prefix(std::min(message.find_first_not_of(' '), message.size()));
        if (message.starts_with("hang watch:")) return rest;
    }
    return std::nullopt;
}

bool WritePreference(const char* name, const std::string& text)
{
    const std::string temporary = std::string(name) + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    file << text << '\n';
    file.close();
    if (!file) return false;
#ifdef _WIN32
    return MoveFileExA(temporary.c_str(), name, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    return std::rename(temporary.c_str(), name) == 0;
#endif
}

#ifdef _WIN32
// Beside the logs: the newest session already sent.
constexpr const char* SentFile = "log-collection-sent";
constexpr uintmax_t MaxReadBytes = 32u << 20;

std::string Json(std::string_view text)
{
    std::string out = "\"";
    for (const unsigned char c : text)
    {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) out += fmt::format("\\u{:04x}", c);
        else out += char(c);
    }
    return out + '"';
}

std::optional<uint64_t> SessionStamp(const std::filesystem::path& path)
{
    const auto name = path.filename().wstring();
    if (!name.starts_with(L"runtime-") || !name.ends_with(L".log") || name.size() <= 12 || name.size() > 32)
        return std::nullopt;
    std::string digits;
    for (const auto c : name.substr(8, name.size() - 12))
    {
        if (c < L'0' || c > L'9') return std::nullopt;
        digits += char(c);
    }
    uint64_t value = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (parsed.ec != std::errc{}) return std::nullopt;
    return value;
}

// Large debug logs keep their start (build, device) and their end (crash).
std::string ReadBounded(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (!file || ec) return {};
    const auto read = [&](uintmax_t offset, uintmax_t count) {
        std::string data(size_t(count), '\0');
        file.seekg(std::streamoff(offset));
        file.read(data.data(), std::streamsize(count));
        data.resize(size_t(file.gcount()));
        return data;
    };
    if (size <= MaxReadBytes) return read(0, size);
    constexpr uintmax_t head = 4u << 20;
    return read(0, head) + "\n" + read(size - (MaxReadBytes - head), MaxReadBytes - head);
}

struct Handle
{
    HINTERNET value{};
    ~Handle() { if (value) WinHttpCloseHandle(value); }
};

std::string Utf8(const wchar_t* text)
{
    if (!text || !*text) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string out(size_t(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::vector<Replacement> AccountReplacements()
{
    std::vector<Replacement> result;
    if (auto profile = Utf8(_wgetenv(L"USERPROFILE")); profile.size() > 3)
    {
        result.push_back({profile, "%USERPROFILE%"});
        std::replace(profile.begin(), profile.end(), '\\', '/');
        result.push_back({profile, "%USERPROFILE%"});
    }
    for (const auto& [name, placeholder] : {std::pair{L"USERNAME", "<user>"}, std::pair{L"COMPUTERNAME", "<host>"}})
        if (const auto value = Utf8(_wgetenv(name)); value.size() >= 3) result.push_back({value, placeholder});
    return result;
}

// LO_LOG_COLLECTION_URL points tests at a local `wrangler dev`.
bool Post(const std::string& body, std::string& outcome)
{
    std::wstring url = L"https://lo.dotslash.pro/v1/logs";
    if (const wchar_t* custom = _wgetenv(L"LO_LOG_COLLECTION_URL"); custom && *custom) url = custom;
    wchar_t host[256]{}, path[512]{};
    URL_COMPONENTS parts{sizeof(parts)};
    parts.lpszHostName = host;
    parts.dwHostNameLength = DWORD(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = DWORD(std::size(path));
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) { outcome = "invalid URL"; return false; }
    Handle session{WinHttpOpen(L"LostOdysseyRecomp-Log/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0)};
    if (!session.value) { outcome = fmt::format("WinHttpOpen {}", GetLastError()); return false; }
    WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000);
    Handle connection{WinHttpConnect(session.value, host, parts.nPort, 0)};
    Handle request{connection.value ? WinHttpOpenRequest(connection.value, L"POST", path, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr};
    if (!request.value) { outcome = fmt::format("WinHttpOpenRequest {}", GetLastError()); return false; }
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    if (!WinHttpSendRequest(request.value, L"Content-Type: application/json\r\n", DWORD(-1),
            const_cast<char*>(body.data()), DWORD(body.size()), DWORD(body.size()), 0) ||
        !WinHttpReceiveResponse(request.value, nullptr))
    {
        outcome = fmt::format("transport error {}", GetLastError());
        return false;
    }
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
    outcome = fmt::format("HTTP {}", status);
    return status == 200;
}

void Upload(const std::filesystem::path& directory, uint64_t current, const std::vector<Replacement>& replacements)
{
    uint64_t sent = 0;
    std::ifstream(directory / SentFile) >> sent;
    std::optional<std::pair<uint64_t, std::filesystem::path>> previous;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
    {
        const auto stamp = SessionStamp(it->path());
        if (stamp && *stamp > sent && *stamp < current && (!previous || *stamp > previous->first))
            previous.emplace(*stamp, it->path());
    }
    if (!previous) return;
    const auto text = Filter(ReadBounded(previous->second), replacements);
    if (text.empty()) return;
    const auto body = fmt::format(R"({{"schema":1,"build":{},"platform":"windows","text":{}}})", Json(lo_version::Source), Json(text));
    std::string outcome;
    if (!Enabled()) return;
    const bool accepted = Post(body, outcome);
    if (accepted) std::ofstream(directory / SentFile, std::ios::trunc) << previous->first << '\n';
    LOG_INFO("log collection: session {} ({} bytes) {}", previous->first, text.size(), outcome);
}
#endif
}

std::string Filter(std::string_view log, std::span<const Replacement> replacements, size_t limit)
{
    std::vector<std::string> lines;
    std::unordered_set<std::string_view> seen;
    size_t repeated = 0;
    for (size_t at = 0; at < log.size();)
    {
        size_t end = log.find('\n', at);
        if (end == std::string_view::npos) end = log.size();
        auto line = log.substr(at, end - at);
        at = end + 1;
        if (line.ends_with('\r')) line.remove_suffix(1);
        const auto key = Selected(line);
        if (!key) continue;
        // Notices are bounded where they are written and keep their order (a
        // map can be entered twice); repeated errors and warnings only count.
        if ((key->starts_with("[error]") || key->starts_with("[warn]")) && !seen.insert(*key).second)
        {
            ++repeated;
            continue;
        }
        std::string text(line.substr(0, MaxLineBytes));
        for (const auto& replacement : replacements)
            if (!replacement.from.empty()) ReplaceAll(text, replacement.from, replacement.to);
        ReplaceAccountFolders(text);
        lines.push_back(ValidUtf8(text));
    }
    if (repeated) lines.push_back(fmt::format("[note] log collection: {} repeated lines omitted", repeated));

    size_t total = 0;
    for (const auto& line : lines) total += line.size() + 1;
    std::string out;
    const auto append = [&](size_t first, size_t last) {
        for (size_t i = first; i < last; ++i) out.append(lines[i]).push_back('\n');
    };
    if (total <= limit)
    {
        append(0, lines.size());
        return out;
    }
    // A quarter from the start, the rest from the end, where crashes are.
    constexpr size_t markerBytes = 64;
    size_t head = 0, used = 0;
    while (head < lines.size() && used + lines[head].size() + 1 <= limit / 4) used += lines[head++].size() + 1;
    size_t tail = lines.size(), tailUsed = 0;
    while (tail > head && used + tailUsed + lines[tail - 1].size() + 1 + markerBytes <= limit)
        tailUsed += lines[--tail].size() + 1;
    append(0, head);
    out += fmt::format("[note] log collection: {} lines omitted\n", tail - head);
    append(tail, lines.size());
    return out;
}

bool Supported()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void Initialize()
{
    int value = -1;
    std::ifstream(PreferenceFile) >> value;
    consent = Supported() && (value == 0 || value == 1) ? value : -1;
}

int Consent() { return consent.load(std::memory_order_relaxed); }

bool Enabled() { return Consent() == 1; }

bool SetConsent(bool enabled)
{
    // Revocation takes effect even if the preference cannot be persisted.
    if (!enabled) consent = 0;
    if (!WritePreference(PreferenceFile, enabled ? "1" : "0")) return false;
    consent = enabled ? 1 : 0;
    return true;
}

const wchar_t* Label(uint32_t language)
{
    static constexpr const wchar_t* text[] = {L"Log collection", L"日誌收集", L"ログ収集", L"로그 수집", L"日志收集"};
    return text[std::min(language, 4u)];
}

const wchar_t* Message(uint32_t language)
{
    static constexpr const wchar_t* text[] = {
        L"Help fix bugs? When the game starts, it sends a filtered summary of the previous session's log to lo.dotslash.pro: "
        L"build, Windows version, GPU and driver, map names, errors, crash and hang reports, and TAA/jitter mismatch records with shader IDs. "
        L"Account names in folder paths are replaced; other folder names can remain. No saves, screenshots or personal or device identifiers. "
        L"Server records are deleted after 30 days; a private research archive keeps copies. Turn it off in Settings at any time. Enable log collection?",
        L"協助修正錯誤？遊戲啟動時，會把上一次遊玩日誌的篩選摘要傳送到 lo.dotslash.pro："
        L"版本、Windows 版本、GPU 與驅動、地圖名稱、錯誤、當機與卡住報告，以及含著色器 ID 的 TAA／抖動不匹配紀錄。"
        L"資料夾路徑中的帳戶名稱會被替換，其他資料夾名稱可能保留。不含存檔、截圖、個人或裝置識別碼。"
        L"伺服器記錄 30 天後刪除；私有研發歸檔會保留副本。可隨時在設定中關閉。啟用日誌收集？",
        L"不具合の修正に協力しますか？ゲーム起動時に、前回のプレイのログを絞り込んだ要約を lo.dotslash.pro に送信します："
        L"ビルド、Windows のバージョン、GPU とドライバー、マップ名、エラー、クラッシュとフリーズの報告、シェーダー ID 付きの TAA／ジッター不一致記録。"
        L"フォルダーパス内のアカウント名は置き換えますが、ほかのフォルダー名は残ることがあります。セーブ、スクリーンショット、個人・端末識別子は含みません。"
        L"サーバーの記録は 30 日後に削除され、非公開の研究用アーカイブにはコピーが残ります。設定でいつでも無効にできます。ログ収集を有効にしますか？",
        L"버그 수정에 참여하시겠습니까? 게임을 시작할 때 이전 플레이 로그를 걸러 낸 요약을 lo.dotslash.pro로 보냅니다: "
        L"빌드, Windows 버전, GPU와 드라이버, 맵 이름, 오류, 충돌·멈춤 보고서, 셰이더 ID가 포함된 TAA/지터 불일치 기록. "
        L"폴더 경로의 계정 이름은 바꿔서 보내며 다른 폴더 이름은 남을 수 있습니다. 저장 파일, 스크린샷, 개인·기기 식별자는 포함하지 않습니다. "
        L"서버 기록은 30일 후 삭제되며 비공개 연구용 보관소에는 사본이 남습니다. 설정에서 언제든 끌 수 있습니다. 로그 수집을 켤까요?",
        L"协助修复错误？游戏启动时，会把上一次游玩日志的筛选摘要发送到 lo.dotslash.pro："
        L"版本、Windows 版本、GPU 与驱动、地图名称、错误、崩溃与卡死报告，以及含着色器 ID 的 TAA／抖动不匹配记录。"
        L"文件夹路径中的账户名会被替换，其他文件夹名可能保留。不含存档、截图、个人或设备标识。"
        L"服务器记录 30 天后删除；私有研发归档会保留副本。可随时在设置中关闭。启用日志收集？"};
    return text[std::min(language, 4u)];
}

void PromptFirstRun(uint32_t language)
{
#ifdef _WIN32
    if (Consent() < 0 && !getenv("LO_BACKGROUND") && !getenv("LO_HEADLESS"))
    {
        const int choice = MessageBoxW(nullptr, Message(language), Label(language), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (!SetConsent(choice == IDYES)) LOG_WARNING("log collection: consent preference could not be saved");
    }
#else
    (void)language;
#endif
}

void StartUpload(const std::filesystem::path& currentLog)
{
#ifdef _WIN32
    const auto current = SessionStamp(currentLog);
    if (!Enabled() || !current || getenv("LO_BACKGROUND") || getenv("LO_HEADLESS")) return;
    try
    {
        // Startup work comes first; the request itself is a few kilobytes.
        std::thread([directory = currentLog.parent_path(), current = *current, replacements = AccountReplacements()] {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (!Enabled()) return;
            try { Upload(directory, current, replacements); }
            catch (...) {}
        }).detach();
    }
    catch (...) {}
#else
    (void)currentLog;
#endif
}
}
