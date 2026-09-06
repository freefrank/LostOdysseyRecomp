#include <stdafx.h>
#include "first_run.h"
#include "config.h"
#include "translations.h"
#include <array>

namespace settings {
#ifdef _WIN32
namespace {
struct Setup {
    HWND window{}, labels[7]{}, boxes[6]{}, start{}, note{};
    HFONT font{};
    bool done=false, accepted=false;
    Config config=GetConfig();
    int scale=96;
    std::vector<std::pair<uint32_t,uint32_t>> resolutions{{1280,720},{1600,900},{1920,1080},{2560,1440},{3840,2160}};
    int px(int n) const { return MulDiv(n,scale,96); }
    HWND control(const wchar_t* type, const wchar_t* text, DWORD style, int x,int y,int w,int h,int id) {
        auto result=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),window,
                                  reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        return result;
    }
    int selection(int n) { return int(SendMessageW(boxes[n],CB_GETCURSEL,0,0)); }
    void items(int n, std::initializer_list<const wchar_t*> values, int selected) {
        SendMessageW(boxes[n],CB_RESETCONTENT,0,0);
        for (auto text:values) SendMessageW(boxes[n],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
        SendMessageW(boxes[n],CB_SETCURSEL,selected,0);
    }
    void translate() {
        const auto language=uint32_t(std::max(selection(0),0));
        auto tr=[&](const wchar_t* en,const wchar_t* tw){return Translate(language,en,tw);};
        const wchar_t* titles[]={L"First-time setup",L"首次設定",L"初回設定",L"처음 실행 설정",L"首次设置"};
        const wchar_t* starts[]={L"Save and start",L"儲存並啟動",L"保存して開始",L"저장 후 시작",L"保存并启动"};
        const wchar_t* notes[]={L"Choose your settings before the game starts. You can change them later in Settings.",
            L"啟動前選擇設定，之後可在遊戲設定中調整。",L"ゲーム開始前に設定を選択してください。後から変更できます。",
            L"게임 시작 전에 설정을 선택하세요. 나중에 설정에서 변경할 수 있습니다.",L"启动前选择设置，之后可在游戏设置中调整。"};
        SetWindowTextW(window,titles[language]); SetWindowTextW(start,starts[language]); SetWindowTextW(note,notes[language]);
        const wchar_t* en[]={L"Interface language",L"Game language",L"Output resolution",L"Window mode",L"Anti-aliasing",L"DLSS / Frame generation"};
        const wchar_t* tw[]={L"介面語言",L"遊戲語言",L"輸出解析度",L"視窗模式",L"反鋸齒",L"DLSS / 影格生成"};
        for(int i=0;i<6;++i) SetWindowTextW(labels[i],tr(en[i],tw[i]));
        const int mode=std::max(selection(3),0), aa=std::max(selection(4),0);
        items(3,{tr(L"Windowed",L"視窗"),tr(L"Borderless fullscreen",L"無邊框全螢幕"),tr(L"Exclusive fullscreen",L"獨佔全螢幕")},mode);
        items(4,{tr(L"Off",L"關"),L"FXAA"},aa);
        const wchar_t* unavailable[]={L"Not available",L"尚未提供",L"未対応",L"사용 불가",L"暂不可用"};
        items(5,{unavailable[language]},0);
    }
    void create() {
        scale=GetDpiForWindow(window);
        font=CreateFontW(-px(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        note=control(L"STATIC",L"",0,24,20,530,48,20);
        for(int i=0;i<6;++i) {
            labels[i]=control(L"STATIC",L"",0,24,85+i*43,210,26,30+i);
            boxes[i]=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,245,80+i*43,305,240,100+i);
        }
        for(auto name:UiLanguageNames) SendMessageW(boxes[0],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        for(auto name:GameLanguageNames) SendMessageW(boxes[1],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
        SendMessageW(boxes[0],CB_SETCURSEL,config.uiLanguage,0);
        SendMessageW(boxes[1],CB_SETCURSEL,GameLanguageIndex(config.gameLanguage),0);
        const auto resolution=std::pair{config.width,config.height};
        if(std::find(resolutions.begin(),resolutions.end(),resolution)==resolutions.end()) resolutions.push_back(resolution);
        for(const auto& [w,h]:resolutions) {
            const auto text=std::to_wstring(w)+L" × "+std::to_wstring(h);
            SendMessageW(boxes[2],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));
        }
        SendMessageW(boxes[2],CB_SETCURSEL,std::find(resolutions.begin(),resolutions.end(),resolution)-resolutions.begin(),0);
        items(3,{L"",L"",L""},int(config.windowMode)); items(4,{L"",L""},config.fxaa?1:0);
        EnableWindow(boxes[5],FALSE);
        start=control(L"BUTTON",L"",WS_TABSTOP|BS_DEFPUSHBUTTON,330,355,220,36,IDOK);
        translate(); SetFocus(boxes[0]);
        RECT rect{0,0,px(580),px(415)};
        AdjustWindowRectEx(&rect,WS_CAPTION|WS_SYSMENU, FALSE,WS_EX_CONTROLPARENT);
        SetWindowPos(window,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER);
    }
    void save() {
        config.uiLanguage=selection(0); config.gameLanguage=GameLanguageIds[selection(1)];
        const auto [w,h]=resolutions.at(selection(2)); config.width=w; config.height=h;
        config.windowMode=WindowMode(selection(3)); config.fxaa=selection(4)==1;
        if(!SaveConfig(config)) {
            MessageBoxW(window,L"Could not save settings.ini. Choose a writable game folder.",L"Lost Odyssey",MB_OK|MB_ICONERROR);
            return;
        }
        accepted=true; DestroyWindow(window);
    }
};
LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* setup=reinterpret_cast<Setup*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        setup=static_cast<Setup*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        setup->window=window; SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(setup));
    }
    if(setup) {
        if(message==WM_CREATE) { setup->create(); return 0; }
        if(message==WM_COMMAND) {
            if(LOWORD(w)==100 && HIWORD(w)==CBN_SELCHANGE) setup->translate();
            if(LOWORD(w)==IDOK) setup->save();
            return 0;
        }
        if(message==WM_CLOSE) { DestroyWindow(window); return 0; }
        if(message==WM_DESTROY) {
            setup->done=true;
            // A synchronous close can arrive while GetMessage is waiting. Wake
            // that wait without leaving WM_QUIT for the game's later event loop.
            PostThreadMessageW(GetCurrentThreadId(),WM_NULL,0,0);
            return 0;
        }
    }
    return DefWindowProcW(window,message,w,l);
}
}
#endif
bool FirstRunSetup() {
#ifdef _WIN32
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW type{}; type.lpfnWndProc=Procedure; type.hInstance=GetModuleHandleW(nullptr);
    type.lpszClassName=L"LostOdysseyFirstRun"; type.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
    type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    RegisterClassW(&type);
    Setup setup;
    const bool hidden=getenv("LO_BACKGROUND")!=nullptr;
    auto window=CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,L"Lost Odyssey",WS_CAPTION|WS_SYSMENU,
                              CW_USEDEFAULT,CW_USEDEFAULT,580,450,nullptr,nullptr,type.hInstance,&setup);
    if(!window) return false;
    if(!hidden) ShowWindow(window,SW_SHOW);
    MSG message{};
    while(!setup.done && GetMessageW(&message,nullptr,0,0)>0) {
        if(message.message==WM_KEYDOWN && message.wParam==VK_RETURN) { setup.save(); continue; }
        if(message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE) { DestroyWindow(window); continue; }
        if(!IsDialogMessageW(window,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if(!setup.done) DestroyWindow(window);
    DeleteObject(setup.font);
    return setup.accepted;
#else
    return SaveConfig(GetConfig());
#endif
}
}
