#include "menu_overlay.h"
#include <cstdint>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include "../host_ui/rasterizer.h"
#include "../host_ui/widgets.h"
#include "../debug/teleport.h"
#include "../debug/map_info.h"
#include "../debug/map_poi.h"
#include "../debug/save_anywhere.h"
#include "../debug/battle_menu.h"
#include "../debug/translations.h"
#include "../gpu/renderer.h"
#include "../settings/config.h"

namespace debug_menu
{
    struct OverlayState
    {
        bool visible = false;
        int activeTab = 0; // 0: Overview/常用, 1: Teleport/传送
        int selectedRow = 0;
        bool chinese = false;

        // Teleport edit state
        int selectedAxis = 0; // 0: X, 1: Y, 2: Z
        float editCoordinates[3] = {0.0f, 0.0f, 0.0f};
        float stepSize = 100.0f;

        // POI list
        int selectedPoi = 0;
        std::vector<debug_menu::MapPoi> pois;
        uint64_t poiRevision = ~uint64_t(0);

        std::wstring statusMessage;
        std::chrono::steady_clock::time_point statusExpiry{};
    };

    static std::mutex g_overlayStateMutex;
    static std::mutex g_overlayActionMutex;
    static OverlayState g_overlayState;

    static OverlayState GetOverlayStateSnapshot()
    {
        std::lock_guard lock(g_overlayStateMutex);
        return g_overlayState;
    }

    static void SetOverlayStatus(std::wstring message)
    {
        std::lock_guard lock(g_overlayStateMutex);
        g_overlayState.statusMessage = std::move(message);
        g_overlayState.statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    }

    static void ToggleOverlayLocked()
    {
        bool opening = false;
        {
            std::lock_guard lock(g_overlayStateMutex);
            opening = !g_overlayState.visible;
        }

        const bool chinese = settings::GetConfig().debugLanguage == 1;
        const auto snapshot = opening ? debug_menu::GetTeleportSnapshot() : TeleportSnapshot{};
        {
            std::lock_guard lock(g_overlayStateMutex);
            g_overlayState.visible = opening;
            g_overlayState.chinese = chinese;
            if (opening && snapshot.available)
            {
                g_overlayState.editCoordinates[0] = snapshot.current.x;
                g_overlayState.editCoordinates[1] = snapshot.current.y;
                g_overlayState.editCoordinates[2] = snapshot.current.z;
            }
        }
        host_ui::SetGamePaused(opening);
    }

    void ToggleOverlay()
    {
        std::lock_guard actionLock(g_overlayActionMutex);
        ToggleOverlayLocked();
    }

    bool IsOverlayVisible()
    {
        std::lock_guard lock(g_overlayStateMutex);
        return g_overlayState.visible;
    }

    void UpdateOverlaySnapshot()
    {
        std::lock_guard actionLock(g_overlayActionMutex);
        {
            std::lock_guard lock(g_overlayStateMutex);
            if (!g_overlayState.visible) return;
        }
        auto snapshot = debug_menu::GetTeleportSnapshot();
        {
            std::lock_guard lock(g_overlayStateMutex);
            if (g_overlayState.visible)
            {
                if (!snapshot.status.empty() && g_overlayState.activeTab == 1 &&
                    std::chrono::steady_clock::now() >= g_overlayState.statusExpiry)
                {
                    g_overlayState.statusMessage = snapshot.status;
                }
                if (snapshot.available && snapshot.poiRevision != g_overlayState.poiRevision)
                {
                    g_overlayState.pois = std::move(snapshot.pois);
                    g_overlayState.poiRevision = snapshot.poiRevision;
                    if (g_overlayState.selectedPoi >= int(g_overlayState.pois.size()))
                        g_overlayState.selectedPoi = 0;
                }
            }
        }
    }

    void HandleInput(InputAction action)
    {
        std::lock_guard actionLock(g_overlayActionMutex);
        std::unique_lock stateLock(g_overlayStateMutex);
        if (!g_overlayState.visible) return;

        switch (action)
        {
        case InputAction::Cancel:
            stateLock.unlock();
            ToggleOverlayLocked();
            return;
        case InputAction::PrevTab:
            g_overlayState.activeTab = (g_overlayState.activeTab + 2 - 1) % 2;
            g_overlayState.selectedRow = 0;
            return;
        case InputAction::NextTab:
            g_overlayState.activeTab = (g_overlayState.activeTab + 1) % 2;
            g_overlayState.selectedRow = 0;
            return;
        case InputAction::Up:
            if (g_overlayState.selectedRow > 0)
                g_overlayState.selectedRow--;
            return;
        case InputAction::Down:
            if (g_overlayState.activeTab == 0)
            {
                if (g_overlayState.selectedRow < 4)
                    g_overlayState.selectedRow++;
            }
            else
            {
                if (g_overlayState.selectedRow < 7)
                    g_overlayState.selectedRow++;
            }
            return;
        default:
            break;
        }

        if (g_overlayState.activeTab == 0)
        {
            // Overview:
            // 0: Language (Toggle En / Zh)
            // 1: Render Capture
            // 2: Save Anywhere
            // 3: Win Battle
            // 4: Cancel Battle Request
            if (action == InputAction::Confirm || action == InputAction::Left || action == InputAction::Right)
            {
                switch (g_overlayState.selectedRow)
                {
                case 0:
                    g_overlayState.chinese = !g_overlayState.chinese;
                    {
                        const int language = g_overlayState.chinese ? 1 : 0;
                        const bool zh = g_overlayState.chinese;
                        stateLock.unlock();
                        if (!settings::SaveDebugLanguage(language))
                            SetOverlayStatus(zh ? L"保存语言配置失败" : L"Failed to save language setting");
                    }
                    return;
                case 1:
                    stateLock.unlock();
                    gpu::renderer::RequestDebugCapture();
                    SetOverlayStatus(g_overlayState.chinese ? L"已请求截取渲染状态" : L"Capture requested");
                    return;
                case 2:
                    stateLock.unlock();
                    debug_menu::SetSaveAnywhereEnabled(!debug_menu::SaveAnywhereEnabled());
                    return;
                case 3:
                {
                    const bool zh = g_overlayState.chinese;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestVictory();
                    SetOverlayStatus(accepted
                        ? (zh ? L"已提交判胜请求（等待安全阶段生效）" : L"Victory requested (pending)")
                        : (zh ? L"无法请求判胜（当前无活跃战斗）" : L"Cannot request victory (no active battle)"));
                    return;
                }
                case 4:
                {
                    const bool zh = g_overlayState.chinese;
                    stateLock.unlock();
                    debug_menu::CancelVictory();
                    SetOverlayStatus(zh ? L"已取消判胜请求" : L"Victory cancelled");
                    return;
                }
                }
            }
        }
        else if (g_overlayState.activeTab == 1)
        {
            // Teleport Tab:
            // 0: Remember Current Pos
            // 1: Restore Pos
            // 2: Fill Current Pos
            // 3: Select Axis (X/Y/Z) & Adjust with Left/Right
            // 4: Teleport to XYZ
            // 5: Step delta (+-100/+-500)
            // 6: POI Selection (Left/Right)
            // 7: Teleport to POI
            switch (g_overlayState.selectedRow)
            {
            case 0:
                if (action == InputAction::Confirm)
                {
                    const bool zh = g_overlayState.chinese;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestSavePosition();
                    SetOverlayStatus(accepted
                        ? (zh ? L"已提交记录坐标请求（等待生效）" : L"Save position requested (pending)")
                        : (zh ? L"无法记录坐标（当前不可用）" : L"Cannot save position (unavailable)"));
                    return;
                }
                break;
            case 1:
                if (action == InputAction::Confirm)
                {
                    const bool zh = g_overlayState.chinese;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestRestorePosition();
                    SetOverlayStatus(accepted
                        ? (zh ? L"已提交恢复坐标请求（等待生效）" : L"Restore position requested (pending)")
                        : (zh ? L"无法恢复坐标（未记录或不可用）" : L"Cannot restore position (no bookmark or unavailable)"));
                    return;
                }
                break;
            case 2:
                if (action == InputAction::Confirm)
                {
                    stateLock.unlock();
                    auto s = debug_menu::GetTeleportSnapshot();
                    if (s.available)
                    {
                        stateLock.lock();
                        g_overlayState.editCoordinates[0] = s.current.x;
                        g_overlayState.editCoordinates[1] = s.current.y;
                        g_overlayState.editCoordinates[2] = s.current.z;
                        g_overlayState.statusMessage = g_overlayState.chinese ? L"已填入当前角色坐标" : L"Position filled";
                        g_overlayState.statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                    }
                    else
                    {
                        SetOverlayStatus(g_overlayState.chinese ? L"无法获取当前角色坐标" : L"Cannot fill position (unavailable)");
                    }
                    return;
                }
                break;
            case 3: // Axis adjustment
                if (action == InputAction::Confirm)
                {
                    g_overlayState.selectedAxis = (g_overlayState.selectedAxis + 1) % 3;
                }
                else if (action == InputAction::Left)
                {
                    g_overlayState.editCoordinates[g_overlayState.selectedAxis] -= g_overlayState.stepSize;
                }
                else if (action == InputAction::Right)
                {
                    g_overlayState.editCoordinates[g_overlayState.selectedAxis] += g_overlayState.stepSize;
                }
                break;
            case 4:
                if (action == InputAction::Confirm)
                {
                    debug_menu::Position p{
                        g_overlayState.editCoordinates[0],
                        g_overlayState.editCoordinates[1],
                        g_overlayState.editCoordinates[2]
                    };
                    const bool zh = g_overlayState.chinese;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestTeleport(p);
                    SetOverlayStatus(accepted
                        ? (zh ? L"已提交传送请求（等待生效）" : L"Teleport requested (pending)")
                        : (zh ? L"传送请求被拒绝（坐标无效或不可用）" : L"Teleport request rejected (invalid or unavailable)"));
                    return;
                }
                break;
            case 5:
                if (action == InputAction::Confirm || action == InputAction::Right)
                {
                    g_overlayState.stepSize = (g_overlayState.stepSize == 100.0f) ? 500.0f : (g_overlayState.stepSize == 500.0f ? 10.0f : 100.0f);
                }
                else if (action == InputAction::Left)
                {
                    g_overlayState.stepSize = (g_overlayState.stepSize == 100.0f) ? 10.0f : (g_overlayState.stepSize == 10.0f ? 500.0f : 100.0f);
                }
                break;
            case 6: // POI selection
                if (!g_overlayState.pois.empty())
                {
                    if (action == InputAction::Left)
                    {
                        g_overlayState.selectedPoi = (g_overlayState.selectedPoi + int(g_overlayState.pois.size()) - 1) % int(g_overlayState.pois.size());
                    }
                    else if (action == InputAction::Right)
                    {
                        g_overlayState.selectedPoi = (g_overlayState.selectedPoi + 1) % int(g_overlayState.pois.size());
                    }
                }
                break;
            case 7:
                if (action == InputAction::Confirm && !g_overlayState.pois.empty())
                {
                    if (size_t(g_overlayState.selectedPoi) < g_overlayState.pois.size())
                    {
                        const auto id = g_overlayState.pois[g_overlayState.selectedPoi].id;
                        const bool zh = g_overlayState.chinese;
                        stateLock.unlock();
                        const bool accepted = debug_menu::RequestPoiTeleport(id);
                        SetOverlayStatus(accepted
                            ? (zh ? L"已提交 POI 传送请求（等待生效）" : L"POI teleport requested (pending)")
                            : (zh ? L"POI 传送请求被拒绝（不可用）" : L"POI teleport request rejected (unavailable)"));
                        return;
                    }
                }
                break;
            }
        }
    }

    // Render debug overlay onto a 1280x720 pixel buffer (alpha composited over frozen frame)
    void RenderOverlay(host_ui::Rasterizer& r)
    {
        const OverlayState state = GetOverlayStateSnapshot();
        if (!state.visible) return;

        bool zh = state.chinese;
        auto tr = [zh](const wchar_t* key) { return debug_menu::translations::Text(key, zh); };

        // Dim background slightly to focus attention on debug overlay
        r.FillRect(0, 0, r.width, r.height, host_ui::MakeColor(140, 0, 0, 0));

        // Draw Main Dialog Panel (Centered: 760 x 540)
        int panelX = (1280 - 760) / 2;
        int panelY = (720 - 540) / 2;
        int panelW = 760;
        int panelH = 540;

        host_ui::DrawPanel(r, panelX, panelY, panelW, panelH, host_ui::MakeColor(235, 20, 22, 26), host_ui::MakeColor(255, 75, 85, 95));

        // Header Title
        std::wstring headerTitle = zh ? L"Lost Odyssey — 调试菜单 (F1 / LB+RB)" : L"Lost Odyssey — Debug Menu (F1 / LB+RB)";
        host_ui::DrawHeader(r, panelX, panelY, panelW, 36, headerTitle);

        // Tab buttons
        int tabY = panelY + 44;
        host_ui::DrawButton(r, panelX + 20, tabY, 140, 30, zh ? L"常用 / Overview" : L"Overview", state.activeTab == 0, state.activeTab == 0);
        host_ui::DrawButton(r, panelX + 170, tabY, 140, 30, zh ? L"传送 / Teleport" : L"Teleport", state.activeTab == 1, state.activeTab == 1);

        // Status message at bottom of panel
        int footerY = panelY + panelH - 32;
        r.DrawHLine(panelX, footerY - 6, panelW, host_ui::MakeColor(255, 60, 65, 75));
        std::wstring help = zh ? L"方向键/左摇杆: 导航   A/Enter: 确定   B/Esc: 关闭   LB/RB: 切页"
                               : L"D-Pad/Stick: Nav   A/Enter: Confirm   B/Esc: Close   LB/RB: Tab";
        r.DrawWString(panelX + 20, footerY, help, host_ui::MakeColor(255, 170, 175, 185));

        int contentX = panelX + 25;
        int contentY = tabY + 42;
        int rowH = 34;

        if (state.activeTab == 0)
        {
            // Overview Content:
            // 0: Language
            std::wstring langText = std::wstring(zh ? L"界面语言: 简体中文" : L"Language: English");
            host_ui::DrawButton(r, contentX, contentY + 0 * rowH, 360, 28, langText, state.selectedRow == 0);

            // 1: Render Capture
            std::wstring capText = zh ? L"截取渲染状态 (Capture)" : L"Capture render state";
            host_ui::DrawButton(r, contentX, contentY + 1 * rowH, 360, 28, capText, state.selectedRow == 1);

            // Status of capture
            std::wstring capStat = gpu::renderer::DebugCaptureStatus();
            if (!capStat.empty())
                r.DrawWString(contentX + 375, contentY + 1 * rowH + 6, capStat, host_ui::MakeColor(255, 200, 200, 100));

            // 2: Save Anywhere
            bool saveOn = debug_menu::SaveAnywhereEnabled();
            std::wstring saveText = (zh ? L"随时存档: " : L"Save Anywhere: ") + std::wstring(saveOn ? (zh ? L"【已启用】" : L"[ON]") : (zh ? L"【已关闭】" : L"[OFF]"));
            host_ui::DrawButton(r, contentX, contentY + 2 * rowH, 360, 28, saveText, state.selectedRow == 2);

            // Map info display
            auto mapInfo = debug_menu::GetMapInfo();
            std::wstring mapText = zh ? L"当前地图: " : L"Current Map: ";
            if (mapInfo.available)
            {
                mapText += mapInfo.name + L" (" + std::to_wstring(mapInfo.id) + L")";
            }
            else
            {
                mapText += zh ? L"未知" : L"Unknown";
            }
            r.DrawWString(contentX, contentY + 3 * rowH + 6, mapText, host_ui::MakeColor(255, 180, 210, 240));

            // 3: Win Battle
            std::wstring winText = zh ? L"当前战斗判胜 (Win Battle)" : L"Win Battle";
            host_ui::DrawButton(r, contentX, contentY + 4 * rowH, 240, 28, winText, state.selectedRow == 3);

            // 4: Cancel Victory
            std::wstring cancelWinText = zh ? L"取消判胜请求" : L"Cancel Victory Request";
            host_ui::DrawButton(r, contentX + 255, contentY + 4 * rowH, 200, 28, cancelWinText, state.selectedRow == 4);

            const wchar_t* bStat = debug_menu::Status();
            if (bStat && *bStat)
                r.DrawWString(contentX, contentY + 5 * rowH + 6, bStat, host_ui::MakeColor(255, 220, 180, 120));
        }
        else if (state.activeTab == 1)
        {
            // Teleport Content
            // 0: Save Pos, 1: Restore Pos, 2: Fill Pos
            host_ui::DrawButton(r, contentX, contentY + 0 * rowH, 220, 28, zh ? L"记住当前位置" : L"Remember Position", state.selectedRow == 0);
            host_ui::DrawButton(r, contentX + 235, contentY + 0 * rowH, 220, 28, zh ? L"返回记录位置" : L"Restore Position", state.selectedRow == 1);
            host_ui::DrawButton(r, contentX + 470, contentY + 0 * rowH, 220, 28, zh ? L"填入当前坐标" : L"Fill Coordinates", state.selectedRow == 2);

            // Current coordinates display
            auto posSnap = debug_menu::GetTeleportSnapshot();
            std::wstring curPosStr = zh ? L"角色实时坐标: " : L"Player Position: ";
            if (posSnap.available)
            {
                wchar_t buf[128];
                swprintf(buf, 128, L"X: %.2f  Y: %.2f  Z: %.2f", posSnap.current.x, posSnap.current.y, posSnap.current.z);
                curPosStr += buf;
            }
            else
            {
                curPosStr += zh ? L"不可用" : L"Unavailable";
            }
            r.DrawWString(contentX, contentY + 1 * rowH + 6, curPosStr, host_ui::MakeColor(255, 180, 220, 180));

            // 3: Editable XYZ
            wchar_t coordBuf[128];
            const wchar_t* axisNames[] = {L"X", L"Y", L"Z"};
            swprintf(coordBuf, 128, L"目标坐标 [%ls]: X: %.1f,  Y: %.1f,  Z: %.1f  (◄/► 微调)",
                     axisNames[state.selectedAxis],
                     state.editCoordinates[0],
                     state.editCoordinates[1],
                     state.editCoordinates[2]);
            host_ui::DrawButton(r, contentX, contentY + 2 * rowH, 500, 28, coordBuf, state.selectedRow == 3);

            // 4: Teleport to XYZ
            host_ui::DrawButton(r, contentX + 515, contentY + 2 * rowH, 180, 28, zh ? L"传送到目标坐标" : L"Teleport to XYZ", state.selectedRow == 4);

            // 5: Step Size
            wchar_t stepBuf[64];
            swprintf(stepBuf, 64, zh ? L"微调步长: ±%.0f (◄/► 切换)" : L"Step Size: ±%.0f (◄/► Switch)", state.stepSize);
            host_ui::DrawButton(r, contentX, contentY + 3 * rowH, 300, 28, stepBuf, state.selectedRow == 5);

            // 6 & 7: POI
            r.DrawWString(contentX, contentY + 4 * rowH + 4, zh ? L"地图兴趣点 (POI):" : L"Points of Interest (POI):", host_ui::MakeColor(255, 230, 230, 230));

            std::wstring poiName = zh ? L"无可用 POI" : L"No POIs available";
            if (!state.pois.empty() && size_t(state.selectedPoi) < state.pois.size())
            {
                const auto& p = state.pois[state.selectedPoi];
                wchar_t pBuf[128];
                swprintf(pBuf, 128, L"[%d/%d] %ls (%.0f, %.0f, %.0f)",
                         state.selectedPoi + 1, int(state.pois.size()),
                         p.label.c_str(), p.position.x, p.position.y, p.position.z);
                poiName = pBuf;
            }
            host_ui::DrawButton(r, contentX, contentY + 5 * rowH, 500, 28, poiName, state.selectedRow == 6);
            host_ui::DrawButton(r, contentX + 515, contentY + 5 * rowH, 180, 28, zh ? L"传送到此 POI" : L"Teleport to POI", state.selectedRow == 7);
        }

        if (!state.statusMessage.empty())
        {
            r.DrawWString(contentX, footerY - 24, state.statusMessage, host_ui::MakeColor(255, 120, 220, 150));
        }
    }
}
