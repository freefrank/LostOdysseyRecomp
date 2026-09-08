#include <stdafx.h>
#include "first_run.h"
#include "config.h"
#include "desktop_ui.h"
#include "game_path.h"
#include "folder_picker.h"
#include "translations.h"
#include <array>
#include <fstream>
#ifdef _WIN32
#include <shlobj.h>
#endif

namespace settings
{
#ifdef _WIN32
namespace
{
struct SavedPathFile
{
    bool existed = false;
    std::string contents;
};

SavedPathFile ReadPathFile()
{
    SavedPathFile result;
    std::ifstream input("game-path.txt", std::ios::binary);
    if (!input) return result;
    result.existed = true;
    result.contents.assign(std::istreambuf_iterator<char>(input), {});
    return result;
}

bool ReplacePathFile(const std::filesystem::path &path)
{
    std::ofstream output("game-path.txt.tmp", std::ios::binary | std::ios::trunc);
    const auto utf8 = path.u8string();
    output.write(reinterpret_cast<const char *>(utf8.data()), std::streamsize(utf8.size()));
    output.put('\n');
    output.flush();
    if (!output) return false;
    output.close();
    return output && MoveFileExW(L"game-path.txt.tmp", L"game-path.txt",
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

bool RestorePathFile(const SavedPathFile &saved)
{
    if (!saved.existed)
    {
        return DeleteFileW(L"game-path.txt") || GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    // Keep this recovery copy if replacement fails so the user can restore the
    // prior path without reconstructing it from an error message.
    std::ofstream output("game-path.txt.rollback", std::ios::binary | std::ios::trunc);
    output.write(saved.contents.data(), std::streamsize(saved.contents.size()));
    output.close();
    if (!output) return false;
    return MoveFileExW(L"game-path.txt.rollback", L"game-path.txt",
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

struct Setup
{
    HWND window{}, note{}, title{}, brand{}, pathEdit{}, browse{}, start{}, cancel{}, status{};
    HWND labels[7]{}, boxes[6]{};
    HFONT font{}, titleFont{}, brandFont{};
    bool done = false, accepted = false, initializing = true, pathChanged = false;
    Config config = GetConfig();
    std::filesystem::path selectedRoot;
    std::filesystem::path *outputRoot = nullptr;
    std::vector<std::pair<uint32_t, uint32_t>> resolutions{
        {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

    int px(int value) const { return desktop_ui::Px(window, value); }

    HWND control(const wchar_t *type, const wchar_t *text, DWORD style,
                 int x, int y, int width, int height, int id = 0)
    {
        if (std::wcscmp(type, L"BUTTON") == 0) style |= BS_OWNERDRAW;
        HWND result = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
                                      px(x), px(y), px(width), px(height), window,
                                      reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
                                      GetModuleHandleW(nullptr), nullptr);
        desktop_ui::StyleControl(result, font);
        return result;
    }

    int selection(int index) const
    {
        return std::max(0, int(SendMessageW(boxes[index], CB_GETCURSEL, 0, 0)));
    }

    void items(int index, std::initializer_list<const wchar_t *> values, int selected)
    {
        SendMessageW(boxes[index], CB_RESETCONTENT, 0, 0);
        for (const wchar_t *value : values)
            SendMessageW(boxes[index], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        SendMessageW(boxes[index], CB_SETCURSEL, selected, 0);
    }

    const wchar_t *pick(const wchar_t *en, const wchar_t *tw, const wchar_t *jp,
                        const wchar_t *kr, const wchar_t *sc) const
    {
        const wchar_t *values[] = {en, tw, jp, kr, sc};
        return values[std::min(config.uiLanguage, 4u)];
    }

    void rebuildGameLanguages()
    {
        SendMessageW(boxes[1], CB_RESETCONTENT, 0, 0);
        for (const auto name : GameLanguageNames)
            SendMessageW(boxes[1], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        if (GameLanguageIds[GameLanguageIndex(config.gameLanguage)] != config.gameLanguage)
            config.gameLanguage = 1;
        SendMessageW(boxes[1], CB_SETCURSEL, GameLanguageIndex(config.gameLanguage), 0);
    }

    void translate()
    {
        config.uiLanguage = uint32_t(selection(0));
        const uint32_t language = config.uiLanguage;
        auto tr = [&](const wchar_t *en, const wchar_t *tw) { return Translate(language, en, tw); };
        SetWindowTextW(window, pick(L"Lost Odyssey — First-time setup", L"Lost Odyssey — 首次設定",
            L"Lost Odyssey — 初回設定", L"Lost Odyssey — 처음 실행 설정", L"Lost Odyssey — 首次设置"));
        SetWindowTextW(brand, L"LOST ODYSSEY");
        SetWindowTextW(title, pick(L"Ready your journey", L"準備旅程", L"旅の準備",
            L"여정 준비", L"准备旅程"));
        SetWindowTextW(note, pick(
            L"Choose the game data and initial display settings. Every choice can be reviewed before the game starts.",
            L"選擇遊戲資料與初始顯示設定；啟動前可再次檢查。",
            L"ゲームデータと初期表示設定を選択してください。開始前に確認できます。",
            L"게임 데이터와 초기 화면 설정을 선택하세요. 시작 전에 다시 확인할 수 있습니다.",
            L"选择游戏数据与初始显示设置；启动前可再次检查。"));
        const wchar_t *names[][5] = {
            {L"Interface language", L"介面語言", L"表示言語", L"인터페이스 언어", L"界面语言"},
            {L"Game language", L"遊戲語言", L"ゲーム言語", L"게임 언어", L"游戏语言"},
            {L"Game data", L"遊戲資料", L"ゲームデータ", L"게임 데이터", L"游戏数据"},
            {L"Graphics backend", L"圖形後端", L"グラフィックス API", L"그래픽 백엔드", L"图形后端"},
            {L"Output resolution", L"輸出解析度", L"出力解像度", L"출력 해상도", L"输出分辨率"},
            {L"Window mode", L"視窗模式", L"ウィンドウモード", L"창 모드", L"窗口模式"},
            {L"Anti-aliasing", L"抗鋸齒", L"アンチエイリアス", L"안티앨리어싱", L"抗锯齿"}};
        for (int i = 0; i < 7; ++i) SetWindowTextW(labels[i], names[i][language]);
        SetWindowTextW(browse, pick(L"Browse", L"瀏覽", L"参照", L"찾아보기", L"浏览"));
        SetWindowTextW(start, pick(L"Save and start", L"儲存並啟動", L"保存して開始",
            L"저장 후 시작", L"保存并启动"));
        SetWindowTextW(cancel, pick(L"Cancel", L"取消", L"キャンセル", L"취소", L"取消"));
        const int backend = selection(2), mode = selection(4), aa = selection(5);
        items(2, {L"Direct3D 12", L"Vulkan"}, backend);
        items(4, {tr(L"Windowed", L"視窗"), tr(L"Borderless fullscreen", L"無邊框全螢幕"),
                  tr(L"Exclusive fullscreen", L"獨佔全螢幕")}, mode);
        items(5, {tr(L"Off", L"關"), L"FXAA", L"SMAA", tr(L"TAA (Experimental)", L"TAA（實驗性）")}, aa);
    }

    void create()
    {
        desktop_ui::EnableDarkFrame(window);
        font = desktop_ui::Font(window, 16);
        titleFont = desktop_ui::Font(window, 30, FW_SEMIBOLD);
        brandFont = desktop_ui::Font(window, 15, FW_BOLD);
        brand = control(L"STATIC", L"", 0, 28, 32, 170, 28);
        title = control(L"STATIC", L"", 0, 250, 28, 500, 44);
        note = control(L"STATIC", L"", SS_LEFT, 250, 76, 500, 54);
        SendMessageW(brand, WM_SETFONT, reinterpret_cast<WPARAM>(brandFont), TRUE);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont), TRUE);
        for (int i = 0; i < 7; ++i)
            labels[i] = control(L"STATIC", L"", 0, 250, 150 + i * 48, 180, 28);
        boxes[0] = control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                           440, 144, 310, 240, 100);
        boxes[1] = control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                           440, 192, 310, 240, 101);
        pathEdit = control(L"EDIT", selectedRoot.c_str(), WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                           440, 240, 240, 31, 200);
        browse = control(L"BUTTON", L"", WS_TABSTOP, 688, 240, 62, 31, 201);
        for (int i = 2; i < 6; ++i)
            boxes[i] = control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                               440, 144 + (i + 1) * 48, 310, 240, 100 + i);
        for (const auto name : UiLanguageNames)
            SendMessageW(boxes[0], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        SendMessageW(boxes[0], CB_SETCURSEL, config.uiLanguage, 0);
        rebuildGameLanguages();
        items(2, {L"Direct3D 12", L"Vulkan"}, int(config.graphicsBackend));
        const auto resolution = std::pair{config.width, config.height};
        if (std::find(resolutions.begin(), resolutions.end(), resolution) == resolutions.end())
            resolutions.push_back(resolution);
        for (const auto &[width, height] : resolutions)
        {
            const auto text = std::to_wstring(width) + L" × " + std::to_wstring(height);
            SendMessageW(boxes[3], CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        }
        SendMessageW(boxes[3], CB_SETCURSEL,
                     std::find(resolutions.begin(), resolutions.end(), resolution) - resolutions.begin(), 0);
        items(4, {L"", L"", L""}, int(config.windowMode));
        items(5, {L"", L"", L"", L""}, int(config.antialiasing));
        status = control(L"STATIC", L"", 0, 250, 494, 500, 24);
        cancel = control(L"BUTTON", L"", WS_TABSTOP, 492, 532, 120, 38, IDCANCEL);
        start = control(L"BUTTON", L"", WS_TABSTOP, 620, 532, 130, 38, IDOK);
        translate();
        initializing = false;
        SetFocus(boxes[0]);
        RECT rect{0, 0, px(790), px(600)};
        AdjustWindowRectEx(&rect, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_CONTROLPARENT);
        SetWindowPos(window, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    }

    void choosePath()
    {
        const auto chosen = folder_picker::Choose(window,
            pick(L"Choose a folder containing default.xex (or its disc1 parent).",
            L"選擇包含 default.xex 的資料夾（或其 disc1 上層）。",
            L"default.xex を含むフォルダーを選択してください。",
            L"default.xex가 있는 폴더를 선택하세요.", L"选择包含 default.xex 的文件夹。"), selectedRoot);
        if (chosen.status == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return;
        if (FAILED(chosen.status))
        {
            SetWindowTextW(status, pick(L"Could not open the folder picker. Enter the game folder path directly.",
                L"無法開啟資料夾選擇器，請直接輸入遊戲資料夾路徑。",
                L"フォルダー選択を開けません。ゲームフォルダーのパスを直接入力してください。",
                L"폴더 선택기를 열 수 없습니다. 게임 폴더 경로를 직접 입력하세요.",
                L"无法打开文件夹选择器，请直接输入游戏文件夹路径。"));
            return;
        }
        const auto recognized = game_path::Recognize(chosen.path);
        if (!recognized)
        {
            SetWindowTextW(status, pick(L"That folder does not contain a usable default.xex.",
                L"該資料夾不包含可用的 default.xex。", L"有効な default.xex が見つかりません。",
                L"사용 가능한 default.xex가 없습니다.", L"该文件夹不包含可用的 default.xex。"));
            return;
        }
        selectedRoot = *recognized;
        SetWindowTextW(pathEdit, selectedRoot.c_str());
        ConfigureGameLanguages(selectedRoot / "default.xex");
        rebuildGameLanguages();
        pathChanged = true;
        SetWindowTextW(status, L"");
    }

    bool readAndValidatePath()
    {
        wchar_t text[32768]{};
        GetWindowTextW(pathEdit, text, int(std::size(text)));
        const auto recognized = game_path::Recognize(std::filesystem::path(text));
        if (!recognized)
        {
            SetWindowTextW(status, pick(L"Game data must resolve to a folder containing default.xex.",
                L"遊戲資料必須指向包含 default.xex 的資料夾。", L"default.xex を含むフォルダーを指定してください。",
                L"default.xex가 있는 폴더를 지정하세요.", L"游戏数据必须指向包含 default.xex 的文件夹。"));
            return false;
        }
        if (*recognized != selectedRoot)
        {
            selectedRoot = *recognized;
            ConfigureGameLanguages(selectedRoot / "default.xex");
            rebuildGameLanguages();
        }
        return true;
    }

    void save()
    {
        if (!readAndValidatePath()) return;
        config.uiLanguage = uint32_t(selection(0));
        config.gameLanguage = GameLanguageIds[size_t(selection(1))];
        config.graphicsBackend = GraphicsBackend(selection(2));
        const auto [width, height] = resolutions.at(size_t(selection(3)));
        config.width = width;
        config.height = height;
        config.windowMode = WindowMode(selection(4));
        config.antialiasing = uint32_t(selection(5));
        config.fxaa = config.antialiasing == 1;

        const auto previousPath = ReadPathFile();
        if (pathChanged && !ReplacePathFile(selectedRoot))
        {
            SetWindowTextW(status, pick(L"Could not save game-path.txt. Check folder permissions.",
                L"無法儲存 game-path.txt，請檢查資料夾權限。", L"game-path.txt を保存できません。",
                L"game-path.txt를 저장할 수 없습니다.", L"无法保存 game-path.txt，请检查文件夹权限。"));
            return;
        }
        if (!SaveConfig(config))
        {
            const bool restored = !pathChanged || RestorePathFile(previousPath);
            SetWindowTextW(status, restored
                ? pick(L"Could not save settings.ini. No path change was kept.",
                    L"無法儲存 settings.ini；遊戲路徑變更已復原。", L"settings.ini を保存できません。変更を元に戻しました。",
                    L"settings.ini를 저장할 수 없어 변경을 복원했습니다.", L"无法保存 settings.ini；游戏路径更改已恢复。")
                : pick(L"Settings were not saved and the path rollback failed. Restore game-path.txt.rollback before retrying.",
                    L"設定未儲存且路徑復原失敗；重試前請還原 game-path.txt.rollback。",
                    L"保存とパス復元に失敗しました。game-path.txt.rollback を復元してください。",
                    L"저장 및 경로 복원에 실패했습니다. game-path.txt.rollback을 복원하세요.",
                    L"设置未保存且路径恢复失败；重试前请恢复 game-path.txt.rollback。"));
            return;
        }
        if (outputRoot) *outputRoot = selectedRoot;
        accepted = true;
        DestroyWindow(window);
    }
};

LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto *setup = reinterpret_cast<Setup *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        setup = static_cast<Setup *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
        setup->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(setup));
    }
    if (!setup) return DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_CREATE) { setup->create(); return 0; }
    if (message == WM_COMMAND)
    {
        const int id = LOWORD(wparam);
        if (id == 100 && HIWORD(wparam) == CBN_SELCHANGE) setup->translate();
        if (id == 200 && HIWORD(wparam) == EN_CHANGE && !setup->initializing) setup->pathChanged = true;
        if (id == 201) setup->choosePath();
        if (id == IDOK) setup->save();
        if (id == IDCANCEL) DestroyWindow(window);
        return 0;
    }
    if (message == WM_DRAWITEM)
    {
        const auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(lparam);
        desktop_ui::DrawButton(item, item.CtlID == IDOK);
        return TRUE;
    }
    if (message == WM_CTLCOLORSTATIC)
    {
        const HWND control = reinterpret_cast<HWND>(lparam);
        HDC dc = reinterpret_cast<HDC>(wparam);
        if (control == setup->brand) SetTextColor(dc, desktop_ui::Accent);
        else if (control == setup->note || control == setup->status) SetTextColor(dc, desktop_ui::Muted);
        else SetTextColor(dc, desktop_ui::Text);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(control == setup->brand ? desktop_ui::CanvasBrush() : desktop_ui::SurfaceBrush());
    }
    if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX)
        return reinterpret_cast<LRESULT>(desktop_ui::ColorControl(reinterpret_cast<HDC>(wparam), true));
    if (message == WM_ERASEBKGND)
    {
        RECT client{};
        GetClientRect(window, &client);
        desktop_ui::Fill(reinterpret_cast<HDC>(wparam), client, desktop_ui::Surface);
        RECT rail{0, 0, setup->px(220), client.bottom};
        desktop_ui::Fill(reinterpret_cast<HDC>(wparam), rail, desktop_ui::Canvas);
        RECT accent{setup->px(216), 0, setup->px(220), client.bottom};
        desktop_ui::Fill(reinterpret_cast<HDC>(wparam), accent, desktop_ui::Accent);
        return TRUE;
    }
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (message == WM_DESTROY)
    {
        setup->done = true;
        PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace
#endif

bool FirstRunSetup(std::filesystem::path *gameRoot)
{
#ifdef _WIN32
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW type{};
    type.lpfnWndProc = Procedure;
    type.hInstance = GetModuleHandleW(nullptr);
    type.hIcon = LoadIconW(type.hInstance, L"IDI_LOST_ODYSSEY_RECOMP");
    type.lpszClassName = L"LostOdysseyFirstRun";
    type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    type.hbrBackground = desktop_ui::SurfaceBrush();
    RegisterClassW(&type);
    Setup setup;
    setup.outputRoot = gameRoot;
    setup.selectedRoot = gameRoot ? *gameRoot : game_path::Resolve(std::filesystem::current_path()).root;
    const bool hidden = getenv("LO_BACKGROUND") != nullptr;
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, type.lpszClassName, L"Lost Odyssey",
                                  WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 790, 600,
                                  nullptr, nullptr, type.hInstance, &setup);
    if (!window) return false;
    if (!hidden) ShowWindow(window, SW_SHOW);
    MSG message{};
    while (!setup.done && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (!setup.done) DestroyWindow(window);
    DeleteObject(setup.font);
    DeleteObject(setup.titleFont);
    DeleteObject(setup.brandFont);
    return setup.accepted;
#else
    (void)gameRoot;
    return SaveConfig(GetConfig());
#endif
}

bool FirstRunSetup()
{
    return FirstRunSetup(nullptr);
}
} // namespace settings
