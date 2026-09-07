#pragma once
#include <string>
#include <string_view>

namespace debug_menu::translations
{
// Stable source keys let asynchronous guest status remain language independent.
// Only known tool strings are translated; game names and filesystem paths are data.
struct Entry { std::wstring_view key; const wchar_t* en; const wchar_t* zh; };
inline constexpr Entry Entries[] = {
    {L"F1 打开/关闭 · 本窗口不会暂停游戏", L"F1: show/hide. This window does not pause the game.", L"F1 打开/关闭 · 本窗口不会暂停游戏"},
    {L"常用 / Quick settings", L"Quick settings", L"常用设置"},
    {L"随时存档 / Save anywhere", L"Save anywhere", L"随时存档"},
    {L"开启后重新进入 System 菜单，再选择 Save。", L"Reopen the System menu, then select Save.", L"开启后重新进入 System 菜单，再选择 Save。"},
    {L"当前地图 / Map", L"Current map", L"当前地图"},
    {L"战斗 / Battle", L"Battle", L"战斗"},
    {L"当前战斗判胜 / Win battle", L"Win current battle", L"当前战斗判胜"},
    {L"取消请求", L"Cancel request", L"取消请求"},
    {L"一次性请求；在战斗判定点执行。", L"One-shot request, applied at a safe battle phase.", L"一次性请求；在战斗判定点执行。"},
    {L"人物传送 / Teleport（仅当前地图）", L"Teleport (current map only)", L"人物传送（仅当前地图）"},
    {L"记住当前位置", L"Remember position", L"记住当前位置"},
    {L"返回记录位置", L"Return to bookmark", L"返回记录位置"},
    {L"填入当前坐标", L"Use current position", L"填入当前坐标"},
    {L"传送到坐标", L"Teleport", L"传送到坐标"},
    {L"传送到此 POI", L"Go to POI", L"传送到此 POI"},
    {L"POI 仅含已加载区域；传送到达后仍会触发游戏事件。", L"POIs cover loaded areas; arrival can trigger game events.", L"POI 仅含已加载区域；到达后仍会触发游戏事件。"},
    {L"截取渲染状态 / Capture render state", L"Capture render state", L"截取渲染状态"},
    {L"截取下一完整帧；导出期间可能短暂停顿。", L"Capture the next full frame; exporting may briefly pause.", L"截取下一完整帧；导出期间可能短暂停顿。"},
    {L"Lost Odyssey — Debug Menu (F1)", L"Lost Odyssey — Debug Menu (F1)", L"失落的奥德赛 — 调试菜单 (F1)"},
    {L"当前地图 / Map: 加载中或尚未识别", L"Map: loading or not yet identified", L"当前地图：加载中或尚未识别"},
    {L"地图 ID / Map ID: ", L"Map ID: ", L"地图 ID："},
    {L"地图名称尚未加载 / Name unavailable", L"Map name unavailable", L"地图名称尚未加载"},
    {L"等待可控制的地图角色 / No controllable map character", L"No controllable map character", L"等待可控制的地图角色"},
    {L"坐标须为 −1000000 到 1000000 范围内的数值。", L"Coordinates must be between -1000000 and 1000000.", L"坐标须为 −1000000 到 1000000 范围内的数值。"},
    {L"落点 X %.1f  Y %.1f  Z %.1f\n距离 %.0f（游戏单位）", L"Target X %.1f  Y %.1f  Z %.1f\nDistance %.0f (game units)", L"落点 X %.1f  Y %.1f  Z %.1f\n距离 %.0f（游戏单位）"},
    {L"当前地图没有识别到 POI", L"No POIs identified on this map", L"当前地图没有识别到 POI"},
    {L"地图控制恢复后自动更新 POI", L"POIs update when map control resumes", L"地图控制恢复后自动更新 POI"},
    {L"当前没有可跳过的战斗 / No active battle", L"No active battle", L"当前没有可跳过的战斗"},
    {L"战斗中：可请求判胜 / Battle active", L"Battle active: victory can be requested", L"战斗中：可请求判胜"},
    {L"等待战斗空闲或回合边界… / Waiting for safe phase", L"Waiting for a safe battle phase…", L"等待战斗空闲或回合边界…"},
    {L"已进入胜利收尾 / Victory requested in game", L"Victory requested in game", L"已进入胜利收尾"},
    {L"请求已取消 / Cancelled", L"Request cancelled", L"请求已取消"},
    {L"等待游戏线程执行", L"Waiting for the game thread", L"等待游戏线程执行"},
    {L"场景已变化，已清除记录坐标", L"Scene changed; bookmark cleared", L"场景已变化，已清除记录坐标"},
    {L"仅可在可控制的地图中传送", L"Teleport requires a controllable map character", L"仅可在可控制的地图中传送"},
    {L"地图传送就绪", L"Map teleport ready", L"地图传送就绪"},
    {L"已记录当前地图坐标", L"Current map position saved", L"已记录当前地图坐标"},
    {L"POI 已失效，请重新选择", L"POI expired; select another", L"POI 已失效，请重新选择"},
    {L"坐标必须为有限数值且绝对值不超过 1000000", L"Coordinates must be finite and within +/-1000000", L"坐标必须为有限数值且绝对值不超过 1000000"},
    {L"POI 已卸载或坐标无效，请重新选择", L"POI unloaded or invalid; select another", L"POI 已卸载或坐标无效，请重新选择"},
    {L"无法分配传送参数", L"Cannot allocate teleport parameters", L"无法分配传送参数"},
    {L"传送触发场景变化，已清除记录坐标", L"Teleport changed scene; bookmark cleared", L"传送触发场景变化，已清除记录坐标"},
    {L"传送完成", L"Teleport complete", L"传送完成"},
    {L"目标被碰撞或游戏规则拒绝", L"Destination rejected by collision or game rules", L"目标被碰撞或游戏规则拒绝"},
    {L"等待可控制地图的更新", L"Waiting for controllable map update", L"等待可控制地图的更新"},
    {L"存档点", L"Save point", L"存档点"},
    {L"出口附近", L"Near exit", L"出口附近"},
    {L"机关互动点", L"Interactable", L"机关互动点"},
    {L"拾取 / 触碰点", L"Pickup / touch point", L"拾取 / 触碰点"},
    {L"地图入口", L"Map entrance", L"地图入口"},
    {L"等待下一完整帧 / Waiting for next frame", L"Waiting for next full frame", L"等待下一完整帧"},
    {L"当前构建不支持渲染捕获 / Renderer unavailable", L"Render capture unavailable in this build", L"当前构建不支持渲染捕获"},
    {L"正在截取 / Capturing: ", L"Capturing: ", L"正在截取："},
    {L"捕获失败：无法创建输出 / Cannot create capture output", L"Cannot create capture output", L"捕获失败：无法创建输出"},
    {L"正在压缩 ZIP / Compressing ZIP", L"Compressing ZIP", L"正在压缩 ZIP"},
    {L"ZIP 已保存 / ZIP saved: ", L"ZIP saved: ", L"ZIP 已保存："},
    {L"ZIP 失败，原始文件保留 / ZIP failed: ", L"ZIP failed; raw files retained: ", L"ZIP 失败，原始文件保留："},
    {L"导出不完整 / Incomplete: ", L"Export incomplete: ", L"导出不完整："},
    {L"Language", L"Language", L"语言"},
    {L"Could not save language. Check settings.ini permissions.", L"Could not save language. Check settings.ini permissions.", L"无法保存语言，请检查 settings.ini 写入权限。"},
};
inline const wchar_t* Text(const wchar_t* key, bool chinese)
{
    for (const auto& entry : Entries)
        if (entry.key == key) return chinese ? entry.zh : entry.en;
    return key;
}
inline std::wstring Capture(const std::wstring& status, bool chinese)
{
    // These are the only dynamic renderer prefixes; leave their path suffix intact.
    for (const auto* prefix : {L"正在截取 / Capturing: ", L"ZIP 已保存 / ZIP saved: ",
        L"ZIP 失败，原始文件保留 / ZIP failed: ", L"导出不完整 / Incomplete: "})
        if (status.starts_with(prefix))
            return std::wstring(Text(prefix, chinese)) + status.substr(std::wstring_view(prefix).size());
    return Text(status.c_str(), chinese);
}
inline std::wstring Poi(const std::wstring& label, bool chinese)
{
    const auto split = label.rfind(L' ');
    if (split == std::wstring::npos) return label;
    const auto category = label.substr(0, split);
    return std::wstring(Text(category.c_str(), chinese)) + label.substr(split);
}
} // namespace debug_menu::translations
