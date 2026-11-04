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
#include "soft_reset.h"
#include "../debug/translations.h"
#include "../gpu/renderer.h"
#include "../settings/config.h"
#include "cheat_overlay.h"
#include "controller_hint.h"

// hid.h also declares guest XAMINPUT types; the overlay only needs this host state.
namespace hid { bool UsesPlayStationPrompts(); }

namespace debug_menu
{
    using translations::Pick;

    struct OverlayState
    {
        bool visible = false;
        int activeTab = 0; // 0: Overview, 1: Teleport, 2: Cheats
        int selectedRow = 0;
        uint32_t language = translations::English;
        bool resetConfirmation = false;
        bool resetYes = false;
        cheat_overlay::Model cheatsPage;

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

        const auto config = settings::GetConfig();
        const uint32_t language = config.debugLanguage;
        const auto snapshot = opening ? debug_menu::GetTeleportSnapshot() : TeleportSnapshot{};
        {
            std::lock_guard lock(g_overlayStateMutex);
            g_overlayState.visible = opening;
            g_overlayState.language = language;
            g_overlayState.cheatsPage.menuAnimationRate = config.menuAnimationRate;
            g_overlayState.cheatsPage.Dismiss();
            g_overlayState.resetConfirmation = false;
            g_overlayState.resetYes = false;
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

    void PollResetNotice()
    {
        const wchar_t* refusal = soft_reset::TakeRefusal();
        if (!refusal) return;
        std::lock_guard actionLock(g_overlayActionMutex);
        if (!IsOverlayVisible())
            ToggleOverlayLocked();
        {
            std::lock_guard lock(g_overlayStateMutex);
            g_overlayState.activeTab = 0;
            g_overlayState.selectedRow = 4;
            g_overlayState.resetConfirmation = false;
            g_overlayState.resetYes = false;
            g_overlayState.cheatsPage.Dismiss();
        }
        SetOverlayStatus(refusal);
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

        if (g_overlayState.resetConfirmation)
        {
            if (action == InputAction::Cancel)
                g_overlayState.resetConfirmation = false;
            else if (action == InputAction::Left || action == InputAction::Right)
                g_overlayState.resetYes = !g_overlayState.resetYes;
            else if (action == InputAction::Confirm)
            {
                g_overlayState.resetConfirmation = false;
                if (g_overlayState.resetYes)
                {
                    stateLock.unlock();
                    if (soft_reset::Request())
                        ToggleOverlayLocked();
                    else
                        SetOverlayStatus(soft_reset::Status());
                }
            }
            return;
        }

        if (g_overlayState.activeTab == 2)
        {
            std::optional<cheat_overlay::Nav> nav;
            switch (action)
            {
            case InputAction::Up: nav = cheat_overlay::Nav::Up; break;
            case InputAction::Down: nav = cheat_overlay::Nav::Down; break;
            case InputAction::Left: nav = cheat_overlay::Nav::Left; break;
            case InputAction::Right: nav = cheat_overlay::Nav::Right; break;
            case InputAction::Confirm: nav = cheat_overlay::Nav::Confirm; break;
            case InputAction::Cancel: nav = cheat_overlay::Nav::Cancel; break;
            case InputAction::PrevCategory: nav = cheat_overlay::Nav::PrevCategory; break;
            case InputAction::NextCategory: nav = cheat_overlay::Nav::NextCategory; break;
            default: break;
            }
            if (nav)
            {
                const auto before = fast_forward::GetStatus();
                const unsigned beforeMenuRate = g_overlayState.cheatsPage.menuAnimationRate;
                if (!g_overlayState.cheatsPage.Input(*nav, g_overlayState.language))
                {
                    stateLock.unlock();
                    ToggleOverlayLocked();
                    return;
                }
                // Keep the player's fast-forward rows across restarts (#104).
                const auto after = fast_forward::GetStatus();
                const unsigned afterMenuRate = g_overlayState.cheatsPage.menuAnimationRate;
                if (after.enabled != before.enabled || after.mode != before.mode || after.button != before.button || after.multiplier != before.multiplier ||
                    after.keepAudioPitch != before.keepAudioPitch)
                {
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    if (!settings::SaveFastForward(after.enabled, after.mode == fast_forward::Mode::Toggle ? 1 : 0,
                                                   after.multiplier, after.keepAudioPitch, uint32_t(after.button)))
                    {
                        fast_forward::SetKeepAudioPitch(before.keepAudioPitch);
                        fast_forward::SetButton(before.button);
                        std::lock_guard lock(g_overlayStateMutex);
                        g_overlayState.cheatsPage.notice = Pick(language, L"Failed to save fast-forward setting", L"保存变速设置失败", L"No se ha podido guardar el avance rápido");
                    }
                }
                else if (afterMenuRate != beforeMenuRate)
                {
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    if (!settings::SaveMenuAnimationRate(afterMenuRate))
                    {
                        std::lock_guard lock(g_overlayStateMutex);
                        g_overlayState.cheatsPage.menuAnimationRate = beforeMenuRate;
                        g_overlayState.cheatsPage.notice = Pick(language, L"Failed to save menu animation setting", L"保存菜单动画设置失败", L"No se ha podido guardar la animación de menús");
                    }
                }
                return;
            }
        }

        switch (action)
        {
        case InputAction::Cancel:
            stateLock.unlock();
            ToggleOverlayLocked();
            return;
        case InputAction::PrevTab:
            g_overlayState.activeTab = (g_overlayState.activeTab + 3 - 1) % 3;
            g_overlayState.selectedRow = 0;
            g_overlayState.cheatsPage.Dismiss();
            return;
        case InputAction::NextTab:
            g_overlayState.activeTab = (g_overlayState.activeTab + 1) % 3;
            g_overlayState.selectedRow = 0;
            g_overlayState.cheatsPage.Dismiss();
            return;
        case InputAction::Up:
            if (g_overlayState.selectedRow > 0)
                g_overlayState.selectedRow--;
            return;
        case InputAction::Down:
            // Overview ends at row 6; Teleport ends at row 8 (Debug Event Room).
            if (g_overlayState.selectedRow < (g_overlayState.activeTab == 0 ? 6 : 8))
                g_overlayState.selectedRow++;
            return;
        default:
            break;
        }

        if (g_overlayState.activeTab == 0)
        {
            // Overview:
            // 0: Language (English / Chinese / Spanish)
            // 1: Render Capture
            // 2: Save Anywhere
            // 3: Force RB party switch
            // 4: Return to title
            // 5: Win Battle
            // 6: Cancel Battle Request
            if (action == InputAction::Confirm || action == InputAction::Left || action == InputAction::Right)
            {
                switch (g_overlayState.selectedRow)
                {
                case 0:
                    // English, Simplified Chinese, Spanish; Left goes back.
                    g_overlayState.language = (g_overlayState.language +
                        (action == InputAction::Left ? translations::LanguageCount - 1 : 1)) % translations::LanguageCount;
                    {
                        const uint32_t language = g_overlayState.language;
                        stateLock.unlock();
                        if (!settings::SaveDebugLanguage(language))
                            SetOverlayStatus(Pick(language, L"Failed to save language setting", L"保存语言配置失败", L"No se ha podido guardar el idioma"));
                    }
                    return;
                case 1:
                    stateLock.unlock();
                    gpu::renderer::RequestDebugCapture();
                    SetOverlayStatus(Pick(g_overlayState.language, L"Capture requested", L"已请求截取渲染状态", L"Captura pedida"));
                    return;
                case 2:
                    stateLock.unlock();
                    debug_menu::SetSaveAnywhereEnabled(!debug_menu::SaveAnywhereEnabled());
                    return;
                case 3:
                {
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    debug_menu::RequestPartySwitch();
                    SetOverlayStatus(Pick(language, L"RB party switch enabled (split-party areas only)", L"已开启 RB 换人（仅分队区域有效）", L"Cambio de grupo con RB activado (solo en zonas con el grupo dividido)"));
                    return;
                }
                case 4:
                    if (action == InputAction::Confirm)
                    {
                        g_overlayState.resetConfirmation = true;
                        g_overlayState.resetYes = false;
                    }
                    return;
                case 5:
                {
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestVictory();
                    SetOverlayStatus(accepted
                        ? Pick(language, L"Victory requested (pending)", L"已提交判胜请求（等待安全阶段生效）", L"Victoria pedida (pendiente)")
                        : Pick(language, L"Cannot request victory (no active battle)", L"无法请求判胜（当前无活跃战斗）", L"No se puede pedir la victoria (no hay combate activo)"));
                    return;
                }
                case 6:
                {
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    debug_menu::CancelVictory();
                    SetOverlayStatus(Pick(language, L"Victory cancelled", L"已取消判胜请求", L"Victoria cancelada"));
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
            // 8: Debug Event Room (map jump to z0g_9_scrw)
            switch (g_overlayState.selectedRow)
            {
            case 0:
                if (action == InputAction::Confirm)
                {
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestSavePosition();
                    SetOverlayStatus(accepted
                        ? Pick(language, L"Save position requested (pending)", L"已提交记录坐标请求（等待生效）", L"Guardar la posición pedido (pendiente)")
                        : Pick(language, L"Cannot save position (unavailable)", L"无法记录坐标（当前不可用）", L"No se puede guardar la posición (no disponible)"));
                    return;
                }
                break;
            case 1:
                if (action == InputAction::Confirm)
                {
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestRestorePosition();
                    SetOverlayStatus(accepted
                        ? Pick(language, L"Restore position requested (pending)", L"已提交恢复坐标请求（等待生效）", L"Volver a la posición pedido (pendiente)")
                        : Pick(language, L"Cannot restore position (no bookmark or unavailable)", L"无法恢复坐标（未记录或不可用）", L"No se puede volver a la posición (no hay posición recordada o no disponible)"));
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
                        g_overlayState.statusMessage = Pick(g_overlayState.language, L"Position filled", L"已填入当前角色坐标", L"Posición rellenada");
                        g_overlayState.statusExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                    }
                    else
                    {
                        SetOverlayStatus(Pick(g_overlayState.language, L"Cannot fill position (unavailable)", L"无法获取当前角色坐标", L"No se puede rellenar la posición (no disponible)"));
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
                    const uint32_t language = g_overlayState.language;
                    stateLock.unlock();
                    const bool accepted = debug_menu::RequestTeleport(p);
                    SetOverlayStatus(accepted
                        ? Pick(language, L"Teleport requested (pending)", L"已提交传送请求（等待生效）", L"Teletransporte pedido (pendiente)")
                        : Pick(language, L"Teleport request rejected (invalid or unavailable)", L"传送请求被拒绝（坐标无效或不可用）", L"Teletransporte rechazado (no válido o no disponible)"));
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
                        const uint32_t language = g_overlayState.language;
                        stateLock.unlock();
                        const bool accepted = debug_menu::RequestPoiTeleport(id);
                        SetOverlayStatus(accepted
                            ? Pick(language, L"POI teleport requested (pending)", L"已提交 POI 传送请求（等待生效）", L"Teletransporte al PDI pedido (pendiente)")
                            : Pick(language, L"POI teleport request rejected (unavailable)", L"POI 传送请求被拒绝（不可用）", L"Teletransporte al PDI rechazado (no disponible)"));
                        return;
                    }
                }
                break;
            case 8:
                if (action == InputAction::Confirm)
                {
                    stateLock.unlock();
                    const wchar_t* refusal = debug_menu::RequestDebugEventRoom();
                    // Guest threads stay paused while the menu is open.
                    SetOverlayStatus(refusal ? refusal : L"已提交跳转调试事件房请求（关闭菜单后生效）");
                    return;
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

        const uint32_t language = state.language;
        const bool playStation = hid::UsesPlayStationPrompts();

        // Dim background slightly to focus attention on debug overlay
        r.FillRect(0, 0, r.width, r.height, host_ui::MakeColor(140, 0, 0, 0));

        // Draw Main Dialog Panel (Centered: 760 x 540)
        int panelW = 760;
        int panelH = 540;
        int panelX = (1280 - panelW) / 2;
        int panelY = (720 - panelH) / 2;

        host_ui::DrawPanel(r, panelX, panelY, panelW, panelH, host_ui::MakeColor(235, 20, 22, 26), host_ui::MakeColor(255, 75, 85, 95));

        // Header Title (Centered in header bar by DrawHeader)
        std::wstring headerTitle = controller_hint::ShoulderLabels(
            Pick(language, L"Lost Odyssey — Debug Menu (F1 / LB+RB)", L"Lost Odyssey — 调试菜单 (F1 / LB+RB)", L"Lost Odyssey — Menú de depuración (F1 / LB+RB)"), playStation);
        host_ui::DrawHeader(r, panelX, panelY, panelW, 36, headerTitle);

        // Centered Tab buttons
        int tabW = 160;
        int tabGap = 20;
        int totalTabsW = tabW * 3 + tabGap * 2;
        int tabStartX = panelX + (panelW - totalTabsW) / 2;
        int tabY = panelY + 44;
        host_ui::DrawButton(r, tabStartX, tabY, tabW, 30, Pick(language, L"Overview", L"常规", L"General"), state.activeTab == 0, state.activeTab == 0);
        host_ui::DrawButton(r, tabStartX + tabW + tabGap, tabY, tabW, 30, Pick(language, L"Teleport", L"传送", L"Teletransporte"), state.activeTab == 1, state.activeTab == 1);
        host_ui::DrawButton(r, tabStartX + (tabW + tabGap) * 2, tabY, tabW, 30, Pick(language, L"Cheats", L"辅助 / 修改", L"Trucos"), state.activeTab == 2, state.activeTab == 2);

        // Footer at bottom of panel
        int footerY = panelY + panelH - 32;
        r.DrawHLine(panelX, footerY - 6, panelW, host_ui::MakeColor(255, 60, 65, 75));
        const wchar_t* nav = state.resetConfirmation
            ? Pick(language, L"Left/Right: Choose", L"左/右: 选择", L"Izquierda/Derecha: elegir")
            : Pick(language, L"D-Pad/Stick: Nav", L"方向键/摇杆: 导航", L"Cruceta: mover");
        const wchar_t* accept = Pick(language, state.activeTab == 2 ? L"/Enter: OK" : L"/Enter: Confirm", L"/Enter: 确定",
            L"/Enter: confirmar");
        const wchar_t* back = Pick(language, L"/Esc: Back", L"/Esc: 返回", L"/Esc: atrás");
        const std::wstring tabs = state.resetConfirmation ? L"" : controller_hint::ShoulderLabels(
            state.activeTab == 2 ? Pick(language, L"LB/RB: Tab  LT/RT: Category", L"LB/RB: 切页  LT/RT: 类别",
                                        L"LB/RB: pestaña  LT/RT: categoría")
                                 : Pick(language, L"LB/RB: Tab", L"LB/RB: 切页", L"LB/RB: pestaña"), playStation);
        constexpr int gap = 13;
        const int helpW = r.MeasureWString(nav) + controller_hint::Width(r, playStation, accept)
            + controller_hint::Width(r, playStation, back) + r.MeasureWString(tabs) + 3 * gap;
        const uint32_t helpColor = host_ui::MakeColor(255, 170, 175, 185);
        int helpX = panelX + (panelW - helpW) / 2;
        helpX += r.DrawWString(helpX, footerY, nav, helpColor) + gap;
        helpX = controller_hint::Draw(r, helpX, footerY, playStation, hid::prompts::Face::A, accept, helpColor) + gap;
        helpX = controller_hint::Draw(r, helpX, footerY, playStation, hid::prompts::Face::B, back, helpColor) + gap;
        r.DrawWString(helpX, footerY, tabs, helpColor);

        int contentY = tabY + 42;

        if (state.activeTab == 0)
        {
            // Overview Content (Centered: 520px column width)
            int btnW = 520;
            int btnX = panelX + (panelW - btnW) / 2;
            int rowH = 38;

            // 0: Language
            std::wstring langText = Pick(language, L"Language: English", L"界面语言: 简体中文", L"Idioma: Español");
            host_ui::DrawButton(r, btnX, contentY + 0 * rowH, btnW, 30, langText, state.selectedRow == 0);

            // 1: Render Capture
            std::wstring capText = Pick(language, L"Capture Render State", L"截取渲染状态", L"Capturar el estado de renderizado");
            host_ui::DrawButton(r, btnX, contentY + 1 * rowH, btnW, 30, capText, state.selectedRow == 1);

            // Status of capture (Centered under capture button if active)
            std::wstring rawCapStat = gpu::renderer::DebugCaptureStatus();
            if (!rawCapStat.empty())
            {
                std::wstring capStat = debug_menu::translations::Capture(rawCapStat, language);
                int capStatW = r.MeasureWString(capStat);
                r.DrawWString(panelX + (panelW - capStatW) / 2, contentY + 1 * rowH + 34, capStat, host_ui::MakeColor(255, 200, 200, 100));
            }

            // 2: Save Anywhere
            bool saveOn = debug_menu::SaveAnywhereEnabled();
            std::wstring saveText = saveOn ? Pick(language, L"Save Anywhere: ON", L"随时存档: 开启", L"Guardado libre: SÍ")
                                           : Pick(language, L"Save Anywhere: OFF", L"随时存档: 关闭", L"Guardado libre: NO");
            // 3: Force RB party switch (beside Save Anywhere, same 520px row)
            const int saveGap = 16;
            const int saveBtnW = (btnW - saveGap) / 2;
            host_ui::DrawButton(r, btnX, contentY + 2 * rowH + 8, saveBtnW, 30, saveText, state.selectedRow == 2);
            host_ui::DrawButton(r, btnX + saveBtnW + saveGap, contentY + 2 * rowH + 8, saveBtnW, 30,
                Pick(language, L"Force RB Party Switch", L"强制开启 RB 换人", L"Forzar cambio con RB"), state.selectedRow == 3);

            // 4: Return to title
            host_ui::DrawButton(r, btnX, contentY + 3 * rowH + 12, btnW, 30,
                translations::Text(L"Return to title", language), state.selectedRow == 4);

            // Map info display (Centered)
            auto mapInfo = debug_menu::GetMapInfo();
            std::wstring mapText = Pick(language, L"Current Map: ", L"当前地图: ", L"Mapa actual: ");
            if (mapInfo.available)
            {
                mapText += mapInfo.name + L" (" + std::to_wstring(mapInfo.id) + L")";
            }
            else
            {
                mapText += Pick(language, L"Unknown", L"未知", L"Desconocido");
            }
            int mapTextW = r.MeasureWString(mapText);
            r.DrawWString(panelX + (panelW - mapTextW) / 2, contentY + 4 * rowH + 16, mapText, host_ui::MakeColor(255, 180, 210, 240));

            // 5: Win Battle & 6: Cancel Victory (Side by side, centered total 520px)
            int battleGap = 16;
            int battleBtnW = (btnW - battleGap) / 2; // 252
            std::wstring winText = Pick(language, L"Win Current Battle", L"当前战斗判胜", L"Ganar el combate actual");
            host_ui::DrawButton(r, btnX, contentY + 5 * rowH + 16, battleBtnW, 30, winText, state.selectedRow == 5);

            std::wstring cancelWinText = Pick(language, L"Cancel Victory Request", L"取消判胜请求", L"Cancelar la victoria");
            host_ui::DrawButton(r, btnX + battleBtnW + battleGap, contentY + 5 * rowH + 16, battleBtnW, 30, cancelWinText, state.selectedRow == 6);

            const wchar_t* rawBStat = debug_menu::Status();
            if (rawBStat && *rawBStat)
            {
                std::wstring bStat = debug_menu::translations::Text(rawBStat, language);
                int bStatW = r.MeasureWString(bStat);
                r.DrawWString(panelX + (panelW - bStatW) / 2, contentY + 6 * rowH + 22, bStat, host_ui::MakeColor(255, 220, 180, 120));
            }

            // Issue #74: party-switch permission is runtime-only and is not restored by a load.
            if (state.selectedRow == 4)
            {
                const wchar_t* help[] = {
                    translations::Text(L"Return to title after confirmation; unavailable in battle.", language),
                    translations::Text(L"Unsaved progress will be lost", language),
                };
                for (int line = 0; line < 2; ++line)
                    r.DrawWString(panelX + (panelW - r.MeasureWString(help[line])) / 2,
                        contentY + 7 * rowH + 30 + line * 22, help[line], host_ui::MakeColor(255, 240, 190, 90));
            }
            else if (saveOn)
            {
                const wchar_t* warning[] = {
                    Pick(language, L"Save Anywhere also saves where the game normally forbids it.", L"随时存档可在原作不允许存档的时刻保存。", L"El guardado libre también guarda donde el juego no suele permitirlo."),
                    Pick(language, L"Not offered while the party is split; the button above restores RB on older saves.", L"分队探索期间不提供随时存档；旧存档丢失换人时用上方按钮恢复。", L"No con el grupo dividido; el botón de arriba devuelve RB a partidas antiguas."),
                };
                for (int line = 0; line < 2; ++line)
                {
                    const int warningW = r.MeasureWString(warning[line]);
                    r.DrawWString(panelX + (panelW - warningW) / 2, contentY + 7 * rowH + 30 + line * 22, warning[line],
                        host_ui::MakeColor(255, 240, 190, 90));
                }
            }
        }
        else if (state.activeTab == 1)
        {
            // Teleport Content (Centered 700px grid)
            int gridW = 700;
            int gridX = panelX + (panelW - gridW) / 2; // panelX + 30
            int colGap = 14;

            // Line 0 (Rows 0, 1, 2: Save, Restore, Fill)
            int colW = (gridW - colGap * 2) / 3; // 224
            host_ui::DrawButton(r, gridX, contentY + 0, colW, 30, Pick(language, L"Remember Position", L"记住当前位置", L"Recordar la posición"), state.selectedRow == 0);
            host_ui::DrawButton(r, gridX + colW + colGap, contentY + 0, colW, 30, Pick(language, L"Restore Position", L"返回记录位置", L"Volver a la posición"), state.selectedRow == 1);
            host_ui::DrawButton(r, gridX + (colW + colGap) * 2, contentY + 0, colW, 30, Pick(language, L"Fill Coordinates", L"填入当前坐标", L"Rellenar coordenadas"), state.selectedRow == 2);

            // Line 1: Current coordinates display (Centered)
            auto posSnap = debug_menu::GetTeleportSnapshot();
            std::wstring curPosStr = Pick(language, L"Player Position: ", L"角色实时坐标: ", L"Posición del jugador: ");
            if (posSnap.available)
            {
                wchar_t buf[128];
                swprintf(buf, 128, L"X: %.2f  Y: %.2f  Z: %.2f", posSnap.current.x, posSnap.current.y, posSnap.current.z);
                curPosStr += buf;
            }
            else
            {
                curPosStr += Pick(language, L"Unavailable", L"不可用", L"No disponible");
            }
            int curPosW = r.MeasureWString(curPosStr);
            r.DrawWString(panelX + (panelW - curPosW) / 2, contentY + 42, curPosStr, host_ui::MakeColor(255, 180, 220, 180));

            // Line 2 (Row 3: Editable XYZ, Row 4: Teleport to Target)
            wchar_t coordBuf[128];
            const wchar_t* axisNames[] = {L"X", L"Y", L"Z"};
            swprintf(coordBuf, 128, Pick(language, L"Target [%ls]: X: %.1f  Y: %.1f  Z: %.1f  (◄/► Adjust)",
                                         L"目标坐标 [%ls]: X: %.1f  Y: %.1f  Z: %.1f  (◄/► 微调)",
                                         L"Destino [%ls]: X: %.1f  Y: %.1f  Z: %.1f  (◄/► ajustar)"),
                     axisNames[state.selectedAxis],
                     state.editCoordinates[0],
                     state.editCoordinates[1],
                     state.editCoordinates[2]);
            int editW = 510;
            int actionW = gridW - editW - colGap; // 176
            host_ui::DrawButton(r, gridX, contentY + 70, editW, 30, coordBuf, state.selectedRow == 3);
            host_ui::DrawButton(r, gridX + editW + colGap, contentY + 70, actionW, 30, Pick(language, L"Teleport to Target", L"传送到目标坐标", L"Ir al destino"), state.selectedRow == 4);

            // Line 3 (Row 5: Step Size, spanning grid width centered)
            wchar_t stepBuf[64];
            swprintf(stepBuf, 64, Pick(language, L"Step Size: ±%.0f (◄/► Switch)", L"微调步长: ±%.0f (◄/► 切换)",
                                       L"Tamaño del paso: ±%.0f (◄/► cambiar)"), state.stepSize);
            host_ui::DrawButton(r, gridX, contentY + 112, gridW, 30, stepBuf, state.selectedRow == 5);

            // Line 4: POI Header (Centered)
            std::wstring poiHdr = Pick(language, L"Points of Interest (POI):", L"地图兴趣点:", L"Puntos de interés (PDI):");
            int poiHdrW = r.MeasureWString(poiHdr);
            r.DrawWString(panelX + (panelW - poiHdrW) / 2, contentY + 154, poiHdr, host_ui::MakeColor(255, 230, 230, 230));

            // Line 5 (Row 6: POI Selection, Row 7: Teleport to POI)
            std::wstring poiName = Pick(language, L"No POIs available", L"无可用兴趣点", L"No hay PDI disponibles");
            if (!state.pois.empty() && size_t(state.selectedPoi) < state.pois.size())
            {
                const auto& p = state.pois[state.selectedPoi];
                std::wstring pLabel = debug_menu::translations::Poi(p.label, language);
                wchar_t pBuf[128];
                swprintf(pBuf, 128, L"[%d/%d] %ls (%.0f, %.0f, %.0f)",
                         state.selectedPoi + 1, int(state.pois.size()),
                         pLabel.c_str(), p.position.x, p.position.y, p.position.z);
                poiName = pBuf;
            }
            host_ui::DrawButton(r, gridX, contentY + 182, editW, 30, poiName, state.selectedRow == 6);
            host_ui::DrawButton(r, gridX + editW + colGap, contentY + 182, actionW, 30, Pick(language, L"Teleport to POI", L"传送到此兴趣点", L"Ir al PDI"), state.selectedRow == 7);

            // Line 6 (Row 8: map jump to the game's event-debug room)
            host_ui::DrawButton(r, gridX, contentY + 224, gridW, 30, Pick(language, L"Debug Event Room", L"调试事件房", L"Sala de eventos de depuración"), state.selectedRow == 8);
            const std::wstring roomHint = Pick(language, L"Jumps to z0g_9; hold LB and press Up there for Scenario Jump", L"跳到 z0g_9；在那里按住 LB 再按上打开 Scenario Jump", L"Va a z0g_9; allí mantén LB y pulsa arriba para Scenario Jump");
            const std::wstring hint = controller_hint::ShoulderLabels(roomHint, playStation);
            r.DrawWString(panelX + (panelW - r.MeasureWString(hint)) / 2, contentY + 260, hint, host_ui::MakeColor(255, 170, 175, 185));
        }
        else if (state.activeTab == 2)
        {
            cheat_overlay::Render(r, state.cheatsPage, language, panelX + 30, contentY, panelW - 60, playStation);
        }

        // Status message at bottom of panel (Centered)
        if (state.activeTab != 2 && !state.statusMessage.empty())
        {
            std::wstring dispMsg = debug_menu::translations::Text(state.statusMessage.c_str(), language);
            int msgW = r.MeasureWString(dispMsg);
            r.DrawWString(panelX + (panelW - msgW) / 2, footerY - 26, dispMsg, host_ui::MakeColor(255, 120, 220, 150));
        }

        if (state.resetConfirmation)
        {
            r.FillRect(panelX + 2, contentY - 3, panelW - 4, footerY - contentY - 6,
                host_ui::MakeColor(190, 10, 12, 16));
            constexpr int width = 560, height = 200;
            const int x = panelX + (panelW - width) / 2, y = contentY + 65;
            host_ui::DrawPanel(r, x, y, width, height, host_ui::MakeColor(255, 24, 31, 42));
            host_ui::DrawHeader(r, x, y, width, 36, translations::Text(L"Return to title", language));
            const wchar_t* warning = translations::Text(L"Unsaved progress will be lost", language);
            r.DrawWString(x + (width - r.MeasureWString(warning)) / 2, y + 66,
                warning, host_ui::MakeColor(255, 240, 190, 90));
            constexpr int buttonWidth = 220, gap = 24;
            const int buttonX = x + (width - 2 * buttonWidth - gap) / 2;
            host_ui::DrawButton(r, buttonX, y + 122, buttonWidth, 38,
                Pick(language, L"No", L"否", L"No"), !state.resetYes);
            host_ui::DrawButton(r, buttonX + buttonWidth + gap, y + 122, buttonWidth, 38,
                Pick(language, L"Yes", L"是", L"Sí"), state.resetYes);
        }
    }
}
