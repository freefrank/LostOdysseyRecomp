#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace debug_menu::translations
{
// The debug menu's languages, as settings.ini's debug_language stores them.
enum Language : uint32_t { English, Chinese, Spanish, LanguageCount };
// Stable source keys let asynchronous guest status remain language independent.
// Only known tool strings are translated; game names and filesystem paths are data.
struct Entry { std::wstring_view key; const wchar_t* en; const wchar_t* zh; const wchar_t* es; };
inline constexpr Entry Entries[] = {
    {L"Diagnostics", L"Diagnostics", L"诊断", L"Diagnóstico"},
    {L"Position", L"Position", L"坐标", L"Posición"},
    {L"Points of interest", L"Points of interest", L"地图 POI", L"Puntos de interés"},
    {L"System → Save", L"System → Save", L"系统 → 存档", L"Sistema → Guardar"},
    {L"F1 打开/关闭 · 本窗口不会暂停游戏", L"F1: show/hide. This window does not pause the game.", L"F1 打开/关闭 · 本窗口不会暂停游戏", L"F1: mostrar/ocultar. Esta ventana no pausa el juego."},
    {L"常用 / Quick settings", L"Quick settings", L"常用设置", L"Ajustes rápidos"},
    {L"随时存档 / Save anywhere", L"Save anywhere", L"随时存档", L"Guardado libre"},
    {L"开启后重新进入 System 菜单，再选择 Save。", L"Reopen the System menu, then select Save.", L"开启后重新进入 System 菜单，再选择 Save。", L"Vuelve a abrir el menú Sistema y elige Guardar."},
    {L"当前地图 / Map", L"Current map", L"当前地图", L"Mapa actual"},
    {L"战斗 / Battle", L"Battle", L"战斗", L"Combate"},
    {L"当前战斗判胜 / Win battle", L"Win Current Battle", L"当前战斗判胜", L"Ganar el combate actual"},
    {L"取消请求", L"Cancel Request", L"取消请求", L"Cancelar la petición"},
    {L"一次性请求；在战斗判定点执行。", L"One-shot request, applied at a safe battle phase.", L"一次性请求；在战斗判定点执行。", L"Petición única; se aplica en una fase segura del combate."},
    {L"人物传送 / Teleport（仅当前地图）", L"Teleport (current map only)", L"人物传送（仅当前地图）", L"Teletransporte (solo el mapa actual)"},
    {L"记住当前位置", L"Remember Position", L"记住当前位置", L"Recordar la posición"},
    {L"返回记录位置", L"Restore Position", L"返回记录位置", L"Volver a la posición"},
    {L"填入当前坐标", L"Fill Coordinates", L"填入当前坐标", L"Rellenar coordenadas"},
    {L"传送到坐标", L"Teleport", L"传送到坐标", L"Teletransportar"},
    {L"传送到此 POI", L"Go to POI", L"传送到此 POI", L"Ir al PDI"},
    {L"POI 仅含已加载区域；传送到达后仍会触发游戏事件。", L"POIs cover loaded areas; arrival can trigger game events.", L"POI 仅含已加载区域；到达后仍会触发游戏事件。", L"Los PDI cubren las zonas cargadas; llegar puede activar eventos del juego."},
    {L"截取渲染状态 / Capture render state", L"Capture Render State", L"截取渲染状态", L"Capturar el estado de renderizado"},
    {L"截取下一完整帧；导出期间可能短暂停顿。", L"Capture the next full frame; exporting may briefly pause.", L"截取下一完整帧；导出期间可能短暂停顿。", L"Captura el siguiente fotograma completo; exportarlo puede pausar un momento."},
    {L"Lost Odyssey — Debug Menu (F1)", L"Lost Odyssey — Debug Menu (F1)", L"失落的奥德赛 — 调试菜单 (F1)", L"Lost Odyssey — Menú de depuración (F1)"},
    {L"当前地图 / Map: 加载中或尚未识别", L"Map: loading or not yet identified", L"当前地图：加载中或尚未识别", L"Mapa: cargando o aún sin identificar"},
    {L"地图 ID / Map ID: ", L"Map ID: ", L"地图 ID：", L"ID del mapa: "},
    {L"地图名称尚未加载 / Name unavailable", L"Map name unavailable", L"地图名称尚未加载", L"Nombre del mapa no disponible"},
    {L"等待可控制的地图角色 / No controllable map character", L"No controllable map character", L"等待可控制的地图角色", L"No hay ningún personaje controlable en el mapa"},
    {L"坐标须为 −1000000 到 1000000 范围内的数值。", L"Coordinates must be between -1000000 and 1000000.", L"坐标须为 −1000000 到 1000000 范围内的数值。", L"Las coordenadas deben estar entre -1000000 y 1000000."},
    {L"落点 X %.1f  Y %.1f  Z %.1f\n距离 %.0f（游戏单位）", L"Target X %.1f  Y %.1f  Z %.1f\nDistance %.0f (game units)", L"落点 X %.1f  Y %.1f  Z %.1f\n距离 %.0f（游戏单位）", L"Destino X %.1f  Y %.1f  Z %.1f\nDistancia %.0f (unidades del juego)"},
    {L"当前地图没有识别到 POI", L"No POIs identified on this map", L"当前地图没有识别到 POI", L"No se han identificado PDI en este mapa"},
    {L"地图控制恢复后自动更新 POI", L"POIs update when map control resumes", L"地图控制恢复后自动更新 POI", L"Los PDI se actualizan al recuperar el control en el mapa"},
    {L"当前没有可跳过的战斗 / No active battle", L"No active battle", L"当前没有可跳过的战斗", L"No hay ningún combate activo"},
    {L"战斗中：可请求判胜 / Battle active", L"Battle active: victory can be requested", L"战斗中：可请求判胜", L"Combate activo: se puede pedir la victoria"},
    {L"等待战斗空闲或回合边界… / Waiting for safe phase", L"Waiting for a safe battle phase…", L"等待战斗空闲或回合边界…", L"Esperando una fase segura del combate…"},
    {L"已进入胜利收尾 / Victory requested in game", L"Victory requested in game", L"已进入胜利收尾", L"Victoria pedida en el juego"},
    {L"请求已取消 / Cancelled", L"Request cancelled", L"请求已取消", L"Petición cancelada"},
    {L"等待游戏线程执行", L"Waiting for the game thread", L"等待游戏线程执行", L"Esperando al hilo del juego"},
    {L"场景已变化，已清除记录坐标", L"Scene changed; bookmark cleared", L"场景已变化，已清除记录坐标", L"La escena ha cambiado; se ha borrado la posición recordada"},
    {L"仅可在可控制的地图中传送", L"Teleport requires a controllable map character", L"仅可在可控制的地图中传送", L"El teletransporte necesita un personaje controlable en el mapa"},
    {L"地图传送就绪", L"Map teleport ready", L"地图传送就绪", L"Teletransporte en el mapa listo"},
    {L"已记录当前地图坐标", L"Current map position saved", L"已记录当前地图坐标", L"Posición en el mapa actual guardada"},
    {L"POI 已失效，请重新选择", L"POI expired; select another", L"POI 已失效，请重新选择", L"El PDI ha caducado; elige otro"},
    {L"坐标必须为有限数值且绝对值不超过 1000000", L"Coordinates must be finite and within +/-1000000", L"坐标必须为有限数值且绝对值不超过 1000000", L"Las coordenadas deben ser finitas y estar entre +/-1000000"},
    {L"POI 已卸载或坐标无效，请重新选择", L"POI unloaded or invalid; select another", L"POI 已卸载或坐标无效，请重新选择", L"El PDI se ha descargado o no es válido; elige otro"},
    {L"无法分配传送参数", L"Cannot allocate teleport parameters", L"无法分配传送参数", L"No se pueden reservar los parámetros del teletransporte"},
    {L"传送触发场景变化，已清除记录坐标", L"Teleport changed scene; bookmark cleared", L"传送触发场景变化，已清除记录坐标", L"El teletransporte ha cambiado de escena; se ha borrado la posición recordada"},
    {L"传送完成", L"Teleport complete", L"传送完成", L"Teletransporte completado"},
    {L"目标被碰撞或游戏规则拒绝", L"Destination rejected by collision or game rules", L"目标被碰撞或游戏规则拒绝", L"Destino rechazado por las colisiones o las reglas del juego"},
    {L"等待可控制地图的更新", L"Waiting for controllable map update", L"等待可控制地图的更新", L"Esperando una actualización del mapa controlable"},
    {L"地图名无效", L"Invalid map name", L"地图名无效", L"Nombre de mapa no válido"},
    {L"引擎未就绪，无法跳转地图", L"Map jump unavailable: no engine", L"引擎未就绪，无法跳转地图", L"Cambio de mapa no disponible: sin motor"},
    {L"无可控制角色，无法跳转地图", L"Map jump unavailable: no player", L"无可控制角色，无法跳转地图", L"Cambio de mapa no disponible: sin jugador"},
    {L"地图切换进行中", L"Map change pending", L"地图切换进行中", L"Cambio de mapa pendiente"},
    {L"地图跳转函数校验失败", L"Map jump unavailable: unexpected executable", L"地图跳转函数校验失败", L"Cambio de mapa no disponible: ejecutable inesperado"},
    {L"无法分配地图跳转参数", L"Cannot allocate map jump parameters", L"无法分配地图跳转参数", L"No se pueden reservar los parámetros del cambio de mapa"},
    {L"地图跳转已开始", L"Map jump started", L"地图跳转已开始", L"Cambio de mapa iniciado"},
    {L"游戏拒绝了地图跳转", L"The game refused the map jump", L"游戏拒绝了地图跳转", L"El juego ha rechazado el cambio de mapa"},
    {L"已提交跳转调试事件房请求（关闭菜单后生效）", L"Debug Event Room jump requested (runs after closing the menu)", L"已提交跳转调试事件房请求（关闭菜单后生效）", L"Viaje a la sala de eventos de depuración pedido (se hace al cerrar el menú)"},
    {L"调试事件房", L"Debug Event Room", L"调试事件房", L"Sala de eventos de depuración"},
    {L"存档点", L"Save point", L"存档点", L"Punto de guardado"},
    {L"出口附近", L"Near exit", L"出口附近", L"Cerca de una salida"},
    {L"机关互动点", L"Interactable", L"机关互动点", L"Elemento interactivo"},
    {L"拾取 / 触碰点", L"Pickup / touch point", L"拾取 / 触碰点", L"Objeto / punto de contacto"},
    {L"地图入口", L"Map entrance", L"地图入口", L"Entrada del mapa"},
    {L"等待下一完整帧 / Waiting for next frame", L"Waiting for next full frame", L"等待下一完整帧", L"Esperando el siguiente fotograma completo"},
    {L"当前构建不支持渲染捕获 / Renderer unavailable", L"Render capture unavailable in this build", L"当前构建不支持渲染捕获", L"La captura de renderizado no está disponible en esta compilación"},
    {L"正在截取 / Capturing: ", L"Capturing: ", L"正在截取：", L"Capturando: "},
    {L"捕获失败：无法创建输出 / Cannot create capture output", L"Cannot create capture output", L"捕获失败：无法创建输出", L"No se puede crear la salida de la captura"},
    {L"正在压缩 ZIP / Compressing ZIP", L"Compressing ZIP", L"正在压缩 ZIP", L"Comprimiendo el ZIP"},
    {L"ZIP 已保存 / ZIP saved: ", L"ZIP saved: ", L"ZIP 已保存：", L"ZIP guardado: "},
    {L"ZIP 失败，原始文件保留 / ZIP failed: ", L"ZIP failed; raw files retained: ", L"ZIP 失败，原始文件保留：", L"Error en el ZIP; se conservan los archivos sin comprimir: "},
    {L"导出不完整 / Incomplete: ", L"Export incomplete: ", L"导出不完整：", L"Exportación incompleta: "},
    {L"Language", L"Language", L"语言", L"Idioma"},
    {L"Return to title", L"Return to title", L"返回标题画面", L"Volver a la pantalla de título"},
    {L"Return to title after confirmation; unavailable in battle.", L"Return to title after confirmation; unavailable in battle.", L"确认后返回标题；战斗期间不可用。", L"Vuelve a la pantalla de título tras confirmar; no disponible en combate."},
    {L"Unsaved progress will be lost", L"Unsaved progress will be lost", L"未保存的进度将丢失", L"Se perderá el progreso no guardado"},
    {L"Return to title unavailable", L"Return to title unavailable", L"当前无法返回标题", L"No se puede volver a la pantalla de título"},
    {L"Not available during battle", L"Not available during battle", L"战斗期间不可用", L"No disponible en combate"},
    {L"Please wait for loading to finish", L"Please wait for loading to finish", L"请等待读取完成", L"Espera a que termine la carga"},
    {L"Please wait for saving to finish", L"Please wait for saving to finish", L"请等待保存完成", L"Espera a que termine el guardado"},
    {L"Close the save/load screen first", L"Close the save/load screen first", L"先关闭存档/读档画面", L"Cierra antes la pantalla de guardar/cargar"},
    {L"Already at the title screen", L"Already at the title screen", L"当前已在标题画面", L"Ya estás en la pantalla de título"},
    {L"Returning to title", L"Returning to title", L"正在返回标题", L"Volviendo a la pantalla de título"},
    {L"场景已变化，操作已取消", L"Scene changed; operation cancelled", L"场景已变化，操作已取消", L"La escena ha cambiado; operación cancelada"},
    {L"保存语言配置失败", L"Failed to save language setting", L"保存语言配置失败", L"No se ha podido guardar el idioma"},
    {L"已请求截取渲染状态", L"Capture requested", L"已请求截取渲染状态", L"Captura pedida"},
    {L"已提交判胜请求（等待安全阶段生效）", L"Victory requested (pending)", L"已提交判胜请求（等待安全阶段生效）", L"Victoria pedida (pendiente)"},
    {L"无法请求判胜（当前无活跃战斗）", L"Cannot request victory (no active battle)", L"无法请求判胜（当前无活跃战斗）", L"No se puede pedir la victoria (no hay combate activo)"},
    {L"已取消判胜请求", L"Victory cancelled", L"已取消判胜请求", L"Victoria cancelada"},
    {L"已提交记录坐标请求（等待生效）", L"Save position requested (pending)", L"已提交记录坐标请求（等待生效）", L"Guardar la posición pedido (pendiente)"},
    {L"无法记录坐标（当前不可用）", L"Cannot save position (unavailable)", L"无法记录坐标（当前不可用）", L"No se puede guardar la posición (no disponible)"},
    {L"已提交恢复坐标请求（等待生效）", L"Restore position requested (pending)", L"已提交恢复坐标请求（等待生效）", L"Volver a la posición pedido (pendiente)"},
    {L"无法恢复坐标（未记录或不可用）", L"Cannot restore position (no bookmark or unavailable)", L"无法恢复坐标（未记录或不可用）", L"No se puede volver a la posición (no hay posición recordada o no disponible)"},
    {L"已填入当前角色坐标", L"Position filled", L"已填入当前角色坐标", L"Posición rellenada"},
    {L"无法获取当前角色坐标", L"Cannot fill position (unavailable)", L"无法获取当前角色坐标", L"No se puede rellenar la posición (no disponible)"},
    {L"已提交传送请求（等待生效）", L"Teleport requested (pending)", L"已提交传送请求（等待生效）", L"Teletransporte pedido (pendiente)"},
    {L"传送请求被拒绝（坐标无效或不可用）", L"Teleport request rejected (invalid or unavailable)", L"传送请求被拒绝（坐标无效或不可用）", L"Teletransporte rechazado (no válido o no disponible)"},
    {L"已提交 POI 传送请求（等待生效）", L"POI teleport requested (pending)", L"已提交 POI 传送请求（等待生效）", L"Teletransporte al PDI pedido (pendiente)"},
    {L"POI 传送请求被拒绝（不可用）", L"POI teleport request rejected (unavailable)", L"POI 传送请求被拒绝（不可用）", L"Teletransporte al PDI rechazado (no disponible)"},
    {L"Save Point", L"Save point", L"存档点", L"Punto de guardado"},
    {L"Chest", L"Chest", L"宝箱 / 触碰点", L"Cofre"},
    {L"传送到目标坐标", L"Teleport to Target", L"传送到目标坐标", L"Ir al destino"},
    {L"传送到此兴趣点", L"Teleport to POI", L"传送到此兴趣点", L"Ir al PDI"},
    {L"无可用兴趣点", L"No POIs available", L"无可用兴趣点", L"No hay PDI disponibles"},
    {L"Ready", L"Ready", L"就绪", L"Listo"},
    // Notices the overlay sets itself, and the renderer's capture statuses (Capture below).
    {L"Failed to save fast-forward setting", L"Failed to save fast-forward setting", L"保存变速设置失败", L"No se ha podido guardar el avance rápido"},
    {L"Failed to save menu animation setting", L"Failed to save menu animation setting", L"保存菜单动画设置失败", L"No se ha podido guardar la animación de menús"},
    {L"RB party switch enabled (split-party areas only)", L"RB party switch enabled (split-party areas only)", L"已开启 RB 换人（仅分队区域有效）", L"Cambio de grupo con RB activado (solo en zonas con el grupo dividido)"},
    {L"归档已保存 / Archive saved: ", L"Archive saved: ", L"归档已保存：", L"Archivo guardado: "},
    {L" (原始目录清理失败 / Source cleanup failed)", L" (source cleanup failed)", L"（原始目录清理失败）", L" (no se ha podido limpiar el origen)"},
    {L"归档失败，原始文件保留 / Archive failed: ", L"Archive failed; raw files retained: ", L"归档失败，原始文件保留：", L"Error al archivar; se conservan los archivos sin comprimir: "},
    {L"正在截取 / Capturing ", L"Capturing ", L"正在截取 ", L"Capturando "},
    {L"捕获失败，已有文件保留 / Capture failed, files retained: ", L"Capture failed; files retained: ", L"捕获失败，已有文件保留：", L"Error en la captura; se conservan los archivos: "},
    {L"等待下一帧 / Waiting for frame ", L"Waiting for frame ", L"等待下一帧 ", L"Esperando el fotograma "},
    {L"后台压缩，可继续游戏 / Compressing capture in background", L"Compressing capture in background", L"后台压缩，可继续游戏", L"Comprimiendo la captura en segundo plano"},
    {L"后台保存不完整捕获 / Saving incomplete capture in background", L"Saving incomplete capture in background", L"后台保存不完整捕获", L"Guardando la captura incompleta en segundo plano"},
};
// The call site's own text in the menu's language (settings::Config::debugLanguage).
inline const wchar_t* Pick(uint32_t language, const wchar_t* en, const wchar_t* zh, const wchar_t* es)
{
    return language == Chinese ? zh : language == Spanish ? es : en;
}
inline const wchar_t* Text(const wchar_t* key, uint32_t language)
{
    if (!key) return L"";
    std::wstring_view sv(key);
    for (const auto& entry : Entries)
    {
        if (entry.key == sv || entry.en == sv || entry.zh == sv || entry.es == sv)
            return Pick(language, entry.en, entry.zh, entry.es);
    }
    return key;
}
inline std::wstring Capture(const std::wstring& status, uint32_t language)
{
    // These are the only dynamic renderer prefixes; leave their path suffix intact.
    for (const auto* prefix : {L"正在截取 / Capturing: ", L"ZIP 已保存 / ZIP saved: ",
        L"ZIP 失败，原始文件保留 / ZIP failed: ", L"导出不完整 / Incomplete: ", L"归档已保存 / Archive saved: ",
        L"归档失败，原始文件保留 / Archive failed: ", L"正在截取 / Capturing ", L"捕获失败，已有文件保留 / Capture failed, files retained: ",
        L"等待下一帧 / Waiting for frame "})
        if (status.starts_with(prefix))
        {
            auto rest = status.substr(std::wstring_view(prefix).size());
            constexpr std::wstring_view cleanup = L" (原始目录清理失败 / Source cleanup failed)";
            if (rest.ends_with(cleanup))
                rest = rest.substr(0, rest.size() - cleanup.size()) + Text(cleanup.data(), language);
            return std::wstring(Text(prefix, language)) + rest;
        }
    return Text(status.c_str(), language);
}
inline std::wstring Poi(const std::wstring& label, uint32_t language)
{
    const auto split = label.rfind(L' ');
    if (split == std::wstring::npos) return std::wstring(Text(label.c_str(), language));
    const auto category = label.substr(0, split);
    return std::wstring(Text(category.c_str(), language)) + label.substr(split);
}
} // namespace debug_menu::translations
