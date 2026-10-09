# Changelog / 更新日志

Brief release highlights, newest first. Dates are UTC. Technical validation is recorded in [development status](docs/STATUS.md); future plans are in the [roadmap](docs/ROADMAP.md).

按新到旧记录简短更新，日期采用 UTC。技术验证见[开发状态](docs/STATUS.md)，后续计划见[路线图](docs/ROADMAP.zh-CN.md)。

## Unreleased

### English

- Fixed a Vulkan crash a few seconds after startup on NVIDIA GPUs when ReShade's `dxgi.dll` is in the game folder; ReShade still works with Direct3D 12 (#323).
- With DLSS, FSR, XeSS or MetalFX on, a Render resolution above the output now supersamples: the upscaler outputs at that resolution and the picture is scaled down to the window or screen (#332).
- The HDR page's scene preview now follows the peak brightness with DLSS, FSR or XeSS on.
- New DLSS 5 neural rendering setting in Settings → Graphics on Windows with DLSS: Off or 1×–4× passes. Confirm on it opens a tuning page with model, intensity, tone, structure, skin and character mask, previewed live on the current scene. It needs an RTX GPU and your own nvngx_dlssnr.dll next to the game; the game does not include it.
- Settings → Graphics now lists upscaling, DLSS 5 neural rendering and frame generation together in one group.
- Mod Organizer 2 support: a game plugin lets MO2 manage the mods in the game's `mods/` folder. Mod folders reached through symbolic links now load too.
- The Settings menu now moves like the game's own menus: the cursor arrow slides between rows and sways while idle, the rows fade in one after another when it opens, prompts dim the screen behind them, and it fades out before closing (#151).
- Mod authors can export the game's textures (PNG with their mod keys) and CG movies from their own game data with `--export-assets`, or from a new Mod Organizer 2 tool.
- New Culling setting in Settings → Graphics, 0%–200%. 100% is the original. Lower values keep characters and objects at the screen edges visible until they are fully off screen; higher values hide them sooner (#342).

### 简体中文

- 修复游戏目录里有 ReShade 的 `dxgi.dll` 时，NVIDIA 显卡使用 Vulkan 启动几秒后崩溃的问题；ReShade 在 Direct3D 12 下仍可使用（#323）。
- 开启 DLSS、FSR、XeSS 或 MetalFX 时，高于输出的渲染分辨率现在会超采样：超分输出为该分辨率，再缩小到窗口或屏幕（#332）。
- 开启 DLSS、FSR 或 XeSS 时，HDR 页的场景预览现在会随峰值亮度变化。
- Windows 上开启 DLSS 时，设置 → 图形新增“DLSS 5 神经渲染”：关闭或 1×–4× 次。在这一行按确认会打开调整页，可调模型、强度、色调、结构、皮肤和角色遮罩，并在当前场景上实时预览。需要 RTX 显卡，并把自备的 nvngx_dlssnr.dll 放在游戏旁边，游戏不附带。
- 设置 → 图形里，超分、DLSS 5 神经渲染和帧生成现在排在同一组。
- 支持 Mod Organizer 2：游戏插件让 MO2 管理游戏 `mods/` 文件夹里的 Mod。通过符号链接放入的 Mod 文件夹现在也能加载。
- 设置菜单的动画现在和游戏原版菜单一致：光标箭头在行间滑动、停住时左右轻摆，打开时各行依次淡入，弹出提示时背后画面变暗，关闭前先淡出（#151）。
- Mod 作者可以用 `--export-assets` 从自己的游戏数据导出纹理（PNG，附 Mod key）和 CG 视频，也可以用新的 Mod Organizer 2 工具导出。
- 设置 → 图形新增“剔除”，0%–200%。100% 为原版。调低后，画面边缘的角色和物体会一直显示到完全离开画面；调高则更早隐藏（#342）。

## [v0.9.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.9.0) — 2026-10-08

### English

- New Aspect ratio setting in Settings → Graphics: Auto, 16:9, 21:9 or 4:3, with black bars when the screen has another shape. It replaces the Widescreen switch.

### 简体中文

- 设置 → 图形新增“画面比例”：自动、16:9、21:9 或 4:3，屏幕比例不同时加黑边。它取代了原来的“宽屏”开关。

## [v0.8.61](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.61) — 2026-10-08

### English

- The Settings menu now animates: it fades in when it opens, and tab switches, cursor moves and value changes ease instead of cutting (#151).
- New Motion blur and Dynamic shadows switches in Settings → Graphics (from boma's Xenia patches).
- New Button prompts setting in Settings → Gameplay: Auto, Xbox or PlayStation icons (#275).
- Settings menu reorganized: Vibration is on the Gameplay tab, the Language tab is now System and holds Import discs & DLC, and Dynamic shadows sits under Shadow resolution. TAA and higher frame rates are no longer marked experimental.
- Higher frame rate where walls hide most of the scene, such as the White Boa's Queen's Room.
- Higher frame rate in busy scenes on macOS.
- Upscaling costs less GPU time (#172).
- Faster rendering with the same image on every platform, most noticeable on Android and macOS.
- Fixed flickering boats at Experimental Staff Marine Division (#307).
- Controllers, audio and the game window moved to SDL 3, with support for newer controllers (#289).
- When startup hangs, the log shows where it stopped (#282).

### 简体中文

- 设置菜单加入动画：打开时淡入，切换分类、移动光标和修改选项时平滑过渡（#151）。
- 设置 → 图形新增“动态模糊”和“动态阴影”开关（来自 boma 的 Xenia 补丁）。
- 设置 → 游戏新增“按键提示”，可选自动、Xbox 或 PlayStation 图标（#275）。
- 设置菜单重新整理：“震动”移到“游戏”页，“语言”页改名为“系统”并放入“导入光盘与 DLC”，“动态阴影”移到“阴影分辨率”下方；TAA 和高帧率不再标为实验性。
- 大部分场景被墙挡住的地方（例如 White Boa 的 Queen's Room）帧率更高。
- macOS 繁重场景帧率更高。
- 超分占用的 GPU 时间更少（#172）。
- 各平台渲染更快，画面不变，Android 和 macOS 上最明显。
- 修复 Experimental Staff Marine Division 的船闪烁的问题（#307）。
- 手柄、音频和游戏窗口改用 SDL 3，支持更新的手柄（#289）。
- 启动卡住时，日志会记录卡在哪一步（#282）。

## [v0.8.53](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.53) — 2026-10-07

### English

- Vulkan: a pipeline first needed during play is linked from per-shader parts instead of compiled whole, so new maps, enemies and effects stutter less when their shaders were seen before.
- First visits to maps and battles stutter less: the pipelines a scene is known to use are built while it loads.
- DirectX 12: shorter stalls in the first frames of a new scene; when a pipeline is missing, pipelines that share its shaders are built on other CPU cores at the same time.
- The game downloads a list of the pipelines that maps, cutscenes and battles use, recorded ahead of time, so even first visits stutter less; it is kept up to date in the background.
- The F1 Debug Menu's fast-forward settings (on/off, Hold or Toggle, multiplier) are kept after a restart (#104).
- New **Vibration** slider in the Audio settings sets the controller rumble strength; at the minimum, rumble is off (#198).
- New Audio output setting (Settings > Audio): 5.1 surround sends the game's own 5.1 mix to 5.1 and 7.1 speaker setups (#174).
- Exclusive fullscreen was removed; the display mode is Windowed or Fullscreen (borderless), and a saved exclusive fullscreen choice starts in Fullscreen.
- A new GPU setting picks the graphics card to render with on PCs that have more than one; it applies after a restart (#202).
- A new Display setting picks the monitor the game uses, windowed or fullscreen; monitors of the same model are listed by number and position; after a switch the game asks to keep the new display and returns to the previous one after 5 seconds without an answer (#201).
- Keyboard: in the settings, Enter jumps to the Save button and a second Enter saves.
- Windows: Win+Shift+Left/Right moves the game to the next monitor, also in fullscreen, and a chosen Display setting follows it.
- The project has a Discord server: https://discord.gg/z2yPct6z2w
- Posts in the Discord #help forum are copied to GitHub Discussions (Q&A), so they can be read and searched without Discord.

### 简体中文

- Vulkan：游玩中第一次用到的管线改为由按着色器预编的部件链接而成，不再整体编译；着色器以前出现过时，新地图、新敌人和新特效的卡顿更短。
- 第一次进入地图和战斗时卡顿更少：场景已知会用到的管线在读盘时就建好。
- DirectX 12：新场景开头几帧的卡顿更短；缺少某个管线时，和它共用着色器的管线会同时在其他 CPU 核心上建好。
- 游戏会下载一份预先录好的地图、过场和战斗所用管线列表，第一次进入时卡顿也更少；这份列表会在后台自动更新。
- F1 调试菜单的快进设置（开关、按住或切换、倍率）重启后会保留（#104）。
- 声音设置新增**震动**滑块，可调整手柄震动强度，调到最小即关闭震动（#198）。
- 新增“音频输出”设置（设置 > 声音）：选择 5.1 环绕声后，游戏自带的 5.1 混音会输出到 5.1 和 7.1 扬声器（#174）。
- 移除独占全屏；显示模式只有窗口和全屏（无边框），之前保存为独占全屏的设置改为以全屏启动。
- 新增 GPU 设置，在有多张显卡的电脑上可选择用哪一张渲染，重启后生效（#202）。
- 新增显示器设置，可选择游戏在窗口或全屏下使用的显示器；同型号的多台显示器按编号和位置区分；切换后会询问是否保留新显示器，5 秒内未回应则回到之前的显示器（#201）。
- 键盘：设置中按 Enter 跳到“保存”按钮，再按一次 Enter 即保存。
- Windows：Win+Shift+左/右方向键可把游戏移到相邻显示器，全屏时也可以；已选择的显示器设置会随之更新。
- 项目开设了 Discord 服务器：https://discord.gg/z2yPct6z2w
- Discord #help 论坛的帖子会复制到 GitHub Discussions（Q&A），不用 Discord 也能查看和搜索。

## [v0.8.44](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.44) — 2026-10-06

### English

- FSR, DLSS and XeSS upscaling skip one full-resolution copy and blend per frame, which makes them a little faster at high output resolutions (#172).
- Android: the on-screen CTRL button can be moved in the touch-layout editor and follows the opacity setting (#253).
- Android: the CTRL button slides to the nearest screen edge when it is not used; tap the tab to bring it back.
- Android: the status and navigation bars no longer come back after the game starts or after a swipe; they hide again on their own (#199).
- The importer now explains an incomplete disc image: the file's size, how many bytes are missing and the likely cause, such as a copy cut at 4 GB by FAT32 storage; long scan errors wrap instead of being cut off (#251).

### 简体中文

- FSR、DLSS 和 XeSS 超分每帧少做一次全分辨率复制和合成，在高输出分辨率下略快一些（#172）。
- Android：屏幕上的 CTRL 按钮可以在触摸布局编辑里移动，透明度也跟随设置（#253）。
- Android：CTRL 按钮不用时会缩到最近的屏幕边缘，点一下边缘的小块即可恢复。
- Android：状态栏和导航栏不再在游戏启动后或下滑后一直留着，会自动再次隐藏（#199）。
- 导入器遇到不完整的光盘镜像时会说明文件大小、缺少多少字节和可能原因，例如被 FAT32 存储截到 4 GB；过长的扫描错误会自动换行，不再被截断（#251）。

## [v0.8.39](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.39) — 2026-10-06

### English

- Vulkan and DirectX 12 now keep compiled pipelines on disk, so a scene visited before and the startup pipeline preparation no longer wait for them to compile again.
- Android: Mesa drivers (Turnip) now keep their shader cache between starts.
- Pipelines learned since the last periodic save are no longer lost when the game exits.
- The log now reports pipelines compiled during play, per frame; `tools/pipeline_misses.py` sums them per map and battle.
- The shader cache no longer keeps a startup bundle or per-shader files once the shader pack is installed; existing installs clean up at startup.

### 简体中文

- Vulkan 和 DirectX 12 现在会把编译好的管线存到磁盘，再次进入去过的场景和启动时的管线准备不再等待重新编译。
- Android：Mesa 驱动（Turnip）的着色器缓存现在会在多次启动之间保留。
- 游戏退出时不再丢失上次定期保存之后学到的管线配方。
- 日志现在按帧记录游玩时编译的管线；`tools/pipeline_misses.py` 按地图和战斗汇总。
- 安装着色器包后，本地缓存不再保留启动 bundle 和逐文件着色器；已有安装会在启动时清理。

## [v0.8.37](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.37) — 2026-10-06

### English

- Fixed geometry outlines showing through white screen fades with ambient occlusion on (#237).
- CGI movies, including the title intro, now play in the selected voice language instead of Japanese (#220, #54).
- Android: the game now hides the status and navigation bars and draws under the display cutout (#199).
- Android: the game now renders on Mali GPUs (MediaTek Helio / Dimensity, Exynos), which cannot create BC-compressed textures (#214).
- Fixed being unable to move or open the menu after a random battle that started while breaking a box; the battle now waits until the item message is closed (#114).
- Debug menu (F1): new **Encounter Every Step** switch next to No Random Encounters.
- Debug menu (F1): new **Debug Event Room** button that jumps to the game's event-debug map (z0g_9); hold **LB** and press **Up** there for Scenario Jump.

### 简体中文

- 修复开启环境光遮蔽时，画面变白时透出几何轮廓的问题（#237）。
- CG 动画（包括标题画面的开场动画）现在按所选语音语言播放，不再固定为日语（#220、#54）。
- Android：游戏现在会隐藏状态栏和导航栏，并延伸到屏幕挖孔区域（#199）。
- Android：在不支持 BC 压缩纹理的 Mali GPU（MediaTek Helio / Dimensity、Exynos）上现在可以正常显示画面（#214）。
- 修复打破箱子时恰好遇敌、战斗后无法移动也打不开菜单的问题；现在战斗会等到关闭获得道具的提示后才开始（#114）。
- 调试菜单（F1）：不遇敌旁新增**一步一遇敌**开关。
- 调试菜单（F1）：新增**调试事件房**按钮，直接跳到游戏自带的事件调试图（z0g_9）；在那里按住 **LB** 再按**上**打开 Scenario Jump。

## [v0.8.30](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.30) — 2026-10-05

### English

- Fixed flickering floor lighting in Old Sorceress' Mansion battles and block shadows in the cutscene where Tolten becomes king, with TAA or upscaling (#212).
- The installation guide now has a table of contents and explains how to import Xenia and Xbox 360 (RGH) saves.
- Android: **CTRL → Saves** exports your saves to a ZIP file and imports saves from a PC, Xenia or the RGH save converter (#194).
- Android: the on-screen **LT** now starts fast-forward, like a controller's LT (#194).
- Steadier lighting and character edges in many cutscenes and some battles with TAA or upscaling.
- Enemies now fade in when you switch targets in battle instead of popping in (#219).
- New **Depth of field** (Off to 100%) and **Bloom** settings in Graphics; lowering depth of field keeps distant scenery sharp (#30, thanks @cngjd).
- The README now explains how to hide the minimap: hold **Back** (#85).
- Windows: the Vulkan backend no longer loads the Direct3D 12 runtime (#49).
- Windows (Direct3D 12): new **XeSS** upscaling option and **XeSS** frame generation (2×) (#192, thanks @frankzzz).
- Closing the game during shader preparation or a shader pack download no longer leaves large temporary files behind; leftovers from older versions are removed at startup.

### 简体中文

- 修复开启 TAA 或超分辨率时，Old Sorceress' Mansion 战斗中地面光照闪烁，以及 Tolten 即位过场中出现块状阴影的问题（#212）。
- 安装指南新增目录，并说明如何导入 Xenia 和 Xbox 360（RGH）存档。
- Android：**CTRL → Saves** 可以把存档导出成 ZIP，也能导入来自电脑、Xenia 或 RGH 存档转换器的存档（#194）。
- Android：屏幕上的 **LT** 现在也能开启快进，和手柄的 LT 一样（#194）。
- 开启 TAA 或超分辨率时，许多过场和部分战斗中的光照与角色边缘更稳定。
- 战斗中切换目标时，敌人现在会淡入，不再突然出现（#219）。
- 图形设置新增**景深**（关闭到 100%）和**泛光**选项；调低景深后远景不再模糊（#30，感谢 @cngjd）。
- README 新增隐藏小地图的说明：按住 **Back**（#85）。
- Windows：使用 Vulkan 后端时不再加载 Direct3D 12 运行库（#49）。
- Windows（Direct3D 12）：新增 **XeSS** 超分选项和 **XeSS** 插帧（2×）（#192，感谢 @frankzzz）。
- 在准备着色器或下载着色器包时关闭游戏，不再留下大型临时文件；旧版本留下的这类文件会在启动时清理。

## [v0.8.21](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.21) — 2026-10-04

### English

- Fixed sky flicker in Sea of Baus battles with TAA, FSR or DLSS (#203).
- Android: Adreno 6xx GPUs with the Turnip driver no longer stop at "descriptor/push-constant limits below renderer layout" (#185).
- Fixed the game closing without an error when loading a save with TAA on AMD GPUs under Proton (Direct3D 12) (#200).
- The game's display gamma adjustment for HDTVs is now applied, so blacks and contrast match Xenia without Expanded RGB range (#78, #179).
- New **Brightness / Gamma** page in Graphics: compare the game's default picture with your setting on the last game scene or a test pattern. The original calibration screen is a button on that page.
- The HDR calibration page switches between scene and test pattern with LB / RB, and shows the last game scene when opened from the game's menu.
- Turning HDR on or off applies right after saving, without a restart (frame generation still needs one).
- Android: HDR stays on after the app returns from the background.

### 简体中文

- 修复 Sea of Baus 战斗中开启 TAA、FSR 或 DLSS 时天空闪烁（#203）。
- Android：Adreno 6xx GPU 使用 Turnip 驱动时不再因“descriptor/push-constant limits below renderer layout”无法启动（#185）。
- 修复 AMD 显卡在 Proton 下（Direct3D 12）开启 TAA 读档时游戏无提示退出的问题（#200）。
- 现在会应用游戏针对高清电视的显示 gamma 调整，黑位和对比度与 Xenia 一致，无需开启“扩展 RGB 范围”（#78、#179）。
- 图像设置新增**亮度 / Gamma** 页面：在最后的游戏场景或测试图案上对照游戏默认画面和你的设置。原版亮度校准画面作为页面上的一个按钮保留。
- HDR 校准页改用 LB / RB 切换场景和测试图案，从游戏菜单进入时也会显示最后的游戏场景。
- 开关 HDR 保存后立即生效，无需重启（开启插帧时仍需重启）。
- Android：应用从后台返回后 HDR 不再失效。

## [v0.8.15](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.15) — 2026-10-04

### English

- Fixed flickering self-shadows on characters and objects in cutscenes when TAA or upscaling is on (#183).

### 简体中文

- 修复开启 TAA 或超分辨率时，过场动画中角色和物体自阴影闪烁的问题（#183）。

## [v0.8.10](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.10) — 2026-10-04

### English

- Fixed character and object shadows shaking and flickering on AMD GPUs, including the Steam Deck (#176).
- The shader packs change with this fix; the first start after updating offers to download the new packs.
- Android no longer closes silently when the GPU driver cannot run the game: it shows the reason, and on Qualcomm devices opens the GPU driver page, where a Turnip driver can be picked (#185).
- FSR and DLSS no longer recreate their working images every frame, which lowered the frame rate (#172).

### 简体中文

- 修复 AMD 显卡（包括 Steam Deck）上角色和物体阴影抖动、闪烁的问题（#176）。
- 着色器包随此修复更新，更新后第一次启动会提示下载新的着色器包。
- GPU 驱动无法运行游戏时，Android 版不再无提示地退出，而是显示原因；高通设备会打开 GPU driver 页面，可在那里改选 Turnip 驱动（#185）。
- FSR 和 DLSS 不再每帧重新创建工作图像，此前这会拉低帧率（#172）。

## [v0.8.7](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.7) — 2026-10-03

### English

- Android now checks for updates at startup and offers the new APK for download.
- Android now creates the `game` folder as soon as the app is opened, also on devices that start on the GPU driver page.
- Android can now read the game from an SD card, or from any folder chosen on the new **Game folder** page (**CTRL → Game folder**).
- Android can now import the game on the device from disc images or extracted discs (**Game folder → Import disc images…**).
- Android now writes its logs and crash reports to `Android/data/io.github.freefrank.lostodyssey/files/logs/`, where a PC can copy them over USB.
- The MetalFX settings texts are now translated into Japanese, Korean and Simplified Chinese.

### 简体中文

- Android 现在启动时检查更新，并提供新版 APK 下载。
- Android 现在一打开应用就建好 `game` 目录，先进入 GPU driver 页面的设备也一样。
- Android 现在可以从 SD 卡读取游戏，也可以在新的 **Game folder** 页面（**CTRL → Game folder**）选择任意文件夹。
- Android 现在可以在设备上从光盘镜像或已解出的光盘导入游戏（**Game folder → Import disc images…**）。
- Android 现在把日志和崩溃报告写到 `Android/data/io.github.freefrank.lostodyssey/files/logs/`，电脑可通过 USB 复制。
- MetalFX 相关设置文字现在有日文、韩文和简体中文翻译。

## [v0.8.6](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.6) — 2026-10-03

### English

- Fixed the pillar platforms in the Lunar Palace (Great Ancient Ruins, disc 4) rising on their own after a random battle, which left the pillars floating and the puzzle stuck (#171).
- Removed the v0.7.35 change that blocked object interactions while a battle was starting (#114); it caused the Lunar Palace problem above.

### 简体中文

- 修复 Lunar Palace（第 4 张光盘的大古代遗迹）中放着柱子的升降台在随机战斗后自行升起、柱子悬空、谜题无法继续的问题（#171）。
- 撤销 v0.7.35 加入的“战斗开始时禁止与物体互动”改动（#114）；上面的 Lunar Palace 问题正是它造成的。

## [v0.8.5](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.5) — 2026-10-03

### English

- Fixed battles with dialogue pausing for a long time between lines, or getting stuck, at 90 and 120 FPS (#148, #173).
- Added a **No Random Encounters** switch to the F1 Debug Menu, below Save Anywhere. Story battles still happen.
- The release now includes the experimental Android arm64 APK, built together with the other packages.
- Android on Qualcomm devices can now use a Mesa Turnip Vulkan driver. The device's own driver leaves the highlighted menu row's text invisible; Turnip draws it. A **GPU driver** page opens before the first game start and from **CTRL → GPU driver** while playing: download a driver from the same sources as the Eden emulator, or install a zip, and pick it. Changing the driver restarts the game. The Android app now has a launcher icon.
- Continue now finds a save whenever its `save.bin` is intact, including a renamed or copied slot folder and a slot whose `.lo-content` was damaged by a crash (#175).
- Saves are now written to disk before the game moves on, so a crash or power loss right after saving no longer wipes them (#175).

### 简体中文

- 修复 90／120 FPS 下带台词的战斗在台词之间长时间停顿甚至卡住的问题（#148、#173）。
- F1 调试菜单在随时存档下方新增“不遇敌”开关，剧情战斗仍会发生。
- 本版本起，实验性的 Android arm64 APK 随发布一起提供，和其他安装包一起构建。
- 高通设备上的 Android 版现在可以使用 Mesa Turnip Vulkan 驱动。设备自带的驱动会让菜单光标所在行的文字消失，Turnip 能正常显示。首次启动游戏前会打开 **GPU driver** 页面，游戏中也可从 **CTRL → GPU driver** 进入：从和 Eden 模拟器相同的来源下载驱动，或从 zip 安装，然后选用。切换驱动会重启游戏。Android 应用现在有了启动图标。
- 只要 `save.bin` 完好，“继续游戏”现在都能找到存档，包括被改名或复制过来的存档文件夹，以及 `.lo-content` 因崩溃损坏的存档（#175）。
- 存档现在会先写入磁盘再继续游戏，存档后立刻崩溃或断电不会再把存档清空（#175）。

## [v0.8.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.0) — 2026-10-03

### English

- HDR now works with every anti-aliasing mode (FXAA, SMAA, TAA) and every upscaler (DLSS, FSR, MetalFX), and with DLSS frame generation on Vulkan.
- Added Graphics settings for shadow resolution (1×, 2×, 4×) and experimental ambient occlusion (Off, SSAO, GTAO).
- Vulkan and Metal now share one shader pack, `portable_vk.lospv`. The first start after updating offers to download the new packs.
- Fixed the sun and its glare flashing through cliffs while sailing (#118).
- Added support for 8BitDo controllers such as the Ultimate 2 Wireless (#97, contributed by Xarishark).
- More Settings texts are translated into Japanese, Korean and Simplified Chinese.
- On Windows, a broken `OptiScaler.dll` no longer shows an error dialog at startup.
- The macOS app now requires macOS 15 or later.
- An experimental Android arm64 APK was added to this release after publication (see the Android section of the README).

### 简体中文

- HDR 现在可以和任一抗锯齿模式（FXAA、SMAA、TAA）、任一超分（DLSS、FSR、MetalFX）以及 Vulkan 上的 DLSS 插帧同时开启。
- 画面设置新增阴影分辨率（1×、2×、4×）和实验性环境光遮蔽（关闭、SSAO、GTAO）。
- Vulkan 和 Metal 现在共用一个着色器包 `portable_vk.lospv`。更新后第一次启动会提示下载新的着色器包。
- 修复开船时太阳和光晕穿过悬崖闪出来的问题（#118）。
- 支持 8BitDo 手柄，例如 Ultimate 2 Wireless（#97，由 Xarishark 贡献）。
- 设置菜单又有一批文字翻译为日语、韩语和简体中文。
- Windows 上，损坏的 `OptiScaler.dll` 不再在启动时弹出错误对话框。
- macOS 版本现在需要 macOS 15 或更高版本。
- 发布后补充上传了实验性的 Android arm64 APK（见 README 的 Android 一节）。

## [v0.7.35](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.35) — 2026-10-02

### English

- Added the first macOS package (Apple Silicon, Metal, experimental).
- Shaders compiled on your PC are now stored in one file per renderer, and the game offers to download precompiled shaders at startup.
- Added experimental HDR output for Windows, Linux and macOS.
- Screens taller than 16:9 (16:10, 3:2, 4:3) now show the 3D scene across the whole screen without bars.
- Direct3D 12 and Vulkan now run the game's occlusion queries, so effects like the sun's lens flare follow real visibility (#118).
- Vulkan DLSS frame generation now works like Direct3D 12; added experimental Vulkan FSR 3.1 and MetalFX frame generation.
- Fixed flicker in several skies, caves and cutscenes with TAA, FSR or DLSS (#121).
- Fixed the battle camera jumping at 90 and 120 FPS (#117) and DLSS being unavailable on NVIDIA GPUs in the Linux AppImage and Flatpak packages (#116).

### 简体中文

- 新增首个 macOS 安装包（Apple Silicon，Metal，实验性）。
- 本机编译的着色器现在每个渲染器只存一个文件，游戏也会在启动时询问是否下载预编译着色器。
- 新增 Windows、Linux 和 macOS 的实验性 HDR 输出。
- 比 16:9 更高的屏幕（16:10、3:2、4:3）现在让 3D 画面铺满整个屏幕，不再加黑边。
- Direct3D 12 和 Vulkan 现在会执行游戏的遮挡查询，太阳镜头光晕等效果会跟随真实可见度（#118）。
- Vulkan 的 DLSS 插帧现在与 Direct3D 12 一致；新增实验性的 Vulkan FSR 3.1 和 MetalFX 插帧。
- 修复多处天空、洞窟和过场动画在 TAA、FSR 或 DLSS 下的闪烁（#121）。
- 修复 90 和 120 FPS 下战斗镜头跳动（#117），以及 Linux AppImage 和 Flatpak 包在 NVIDIA 显卡上 DLSS 不可用的问题（#116）。

## [v0.7.25](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.25) — 2026-10-01

### English

- Added an experimental Apple Silicon macOS/Metal build.
- Added a Force RB Party Switch button to the F1 debug menu (#74); Save Anywhere is now off while the party is split.
- Fixed sky flicker with TAA and FSR in the Legacy of the Eastern Tribe area (#102).
- The Linux Flatpak update notice now explains how to install the new bundle.

### 简体中文

- 加入 Apple Silicon macOS／Metal 实验性构建。
- F1 调试菜单新增“强制开启 RB 换人”按钮（#74）；分队期间随时存档不再生效。
- 修复“东方部族的遗产”区域开启 TAA 或 FSR 时天空闪烁（#102）。
- Linux 的 Flatpak 更新提示现在会说明如何安装新的安装包。

## [v0.7.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.20) — 2026-09-30

### English

- Frame generation multiplier is capped at 6× (2× to 6×).
- Fixed a quit-to-desktop hang on Windows (#82).
- Improved performance on the default render path.
- Textures now load their mip chains (#87), which improves distant surfaces.

### 简体中文

- 插帧倍率上限为 6×（2× 到 6×）。
- 修复 Windows 上退出到桌面时卡死的问题（#82）。
- 提升默认渲染路径的性能。
- 纹理现在会加载 mip 链（#87），改善远处表面的显示。

## [v0.7.15 — 2026-09-29](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.15)

### English

- Added native 90/120 FPS targets and FreeSync / G-SYNC Compatible VRR pacing.
- Added an optional RGB Range expansion and a Hold/Toggle speed mode in F1 Cheats.
- Fixed the Hungry Man errand timer at higher frame rates.
- Portable installs now prefer the `game` directory beside the executable.

### 简体中文

- 新增原生 90／120 FPS 目标和 FreeSync／G-SYNC Compatible VRR 节奏控制。
- 新增可选的 RGB Range 扩展，以及 F1 Cheats 的按住／切换变速模式。
- 修复高帧率下 Hungry Man 差事计时。
- 便携式安装现在优先使用可执行文件旁的 `game` 目录。

## [v0.7.10 — 2026-09-28](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.10)

### English

- Updated the bundled Vulkan shaders and added a separate DX12 shader pack.

### 简体中文

- 更新内置 Vulkan 着色器，并新增独立的 DX12 着色器包。

## v0.7.9 — 2026-09-28

### English

- Added Windows D3D12 frame generation (Off/DLSS/FSR) in Graphics.
- Fixed Ubuntu 22.04 AppImage compatibility and improved Flatpak packaging.
- Simplified Windows updates, with rollback on failure.

### 简体中文

- 图像设置新增 Windows D3D12 插帧（关／DLSS／FSR）。
- 修复 Ubuntu 22.04 AppImage 兼容性，改进 Flatpak 打包。
- 简化 Windows 更新流程，失败时回滚。

## v0.7.3 — 2026-09-27

### English

- Reduced redundant D3D12 bindings for better performance.

### 简体中文

- 减少 D3D12 中的重复绑定以提升性能。

## v0.7.2 — 2026-09-27

### English

- Added experimental DLSS/DLAA and FSR upscaling on Windows D3D12.
- Fixed wrong DLAA resolution reporting.

### 简体中文

- Windows D3D12 新增实验性 DLSS／DLAA 与 FSR 超分。
- 修复 DLAA 报告分辨率错误的问题。

## v0.7.1 — 2026-09-26

### English

- Added official standalone Linux Flatpak packages and better PipeWire audio compatibility.
- Added in-game disc/DLC re-import with rollback on failure.

### 简体中文

- 新增官方 Linux Flatpak 独立安装包，改善 PipeWire 音频兼容性。
- 支持在游戏内重新导入光盘／DLC，失败时回滚。

## v0.7.0 — 2026-09-26

### English

- Added automatic PlayStation controller prompts in menus.
- Improved Vulkan DLSS/DLAA and FSR, with SMAA fallback when DLSS/DLAA is unavailable.
- Added the C++ Mod API and menu image/font-page replacement tools.

### 简体中文

- 菜单支持自动切换 PlayStation 手柄提示。
- 改进 Vulkan DLSS／DLAA 与 FSR，DLSS／DLAA 不可用时回退至 SMAA。
- 新增 C++ Mod API 和菜单图像／字体页面替换工具。

## v0.6.20 — 2026-09-25

### English

- Reduced flicker with TAA, FSR and DLSS in more scenes, including the exploration bridge.

### 简体中文

- 减少更多场景（包括探索场景桥面）在 TAA、FSR 和 DLSS 下的闪烁。

## v0.6.19 — 2026-09-25

### English

- Added live anisotropic filtering (Off to 16x) in Graphics, applied on save without a restart.
- Reordered DLSS and FSR quality choices and streamlined the settings menu.
- System menu now has "Quit to Desktop", and Settings > Game has "Quit to Main Menu".
- The debug Save Anywhere toggle is now remembered across sessions (#61).
- The mouse cursor now hides after 2 seconds of inactivity (#50); controller rumble is on by default; the Cheats sidebar uses LT/RT to switch categories.

### 简体中文

- 图形设置新增实时各向异性过滤（Off 到 16x），保存后生效，无需重启。
- 调整 DLSS 与 FSR 画质选项顺序，并精简设置菜单。
- 系统菜单新增“退出到桌面”，设置→游戏页新增“退出到主菜单”。
- 调试菜单的随时存档开关现在跨会话保存（#61）。
- 鼠标停顿 2 秒后自动隐藏（#50）；手柄震动默认开启；作弊菜单侧栏用 LT／RT 切换分类。

## v0.6.15 — 2026-09-24

### English

- Added experimental opt-in FSR upscaling on Windows and native Linux.
- Fixed screenshots showing swapped red and blue colors.
- Fixed DLSS SR and DLAA temporal history resetting spuriously after frame gaps.
- Fixed a crash when switching from DLSS Quality to DLAA.
- The DLSS menu status now shows `Active` only when DLSS output is really in use, with specific fallback reasons.

### 简体中文

- 增加实验性可选 FSR 超分，支持 Windows 与原生 Linux。
- 修复截图红蓝通道颠倒的问题。
- 修复 DLSS SR 与 DLAA 在帧间隔后时序历史被异常重置的问题。
- 修复从 DLSS Quality 切换到 DLAA 时的崩溃。
- DLSS 菜单状态现在仅在 DLSS 输出真正生效时显示为 `Active`，并给出具体回退原因。

## v0.6.11 — 2026-09-22

### English

- Added experimental native NVIDIA DLSS Super Resolution and DLAA, with NGX libraries included in the Windows and Linux packages.
- Settings -> Graphics now has **Upscaler** and **DLSS quality** (`Quality`, `Balanced`, `Performance`, `DLAA`) options.
- Removed the **Internal resolution** row from the menu.
- Menu lists longer than 11 rows now scroll.
- On the Graphics and Language tabs, Start (or Enter) jumps to **Save settings** without saving.

### 简体中文

- 增加实验性原生 NVIDIA DLSS 超分辨率与 DLAA，Windows 与 Linux 发布包内置 NGX 运行库。
- “设置” -> “图形”增加**缩放技术**与 **DLSS 质量**（质量、平衡、性能、DLAA）选项。
- 从菜单中移除“内部分辨率”行。
- 超过 11 行的菜单列表现在可滚动。
- 在“图形”与“语言”分页中，按 Start（或 Enter）跳到“保存设置”行而不保存。

## v0.6.7 — 2026-09-20

### English

- Added a **Widescreen** switch to Settings -> Graphics (Issue #17).
- Added 21:9 output resolution presets: 1720×720, 2560×1080, 3440×1440, 3840×1600 and 5120×2160.
- First-launch setup now offers the matching resolution presets.

### 简体中文

- “设置” -> “图形”增加**宽屏**开关（Issue #17）。
- 增加 21:9 输出分辨率预设：1720×720、2560×1080、3440×1440、3840×1600 与 5120×2160。
- 首次启动设置向导现在提供对应的分辨率预设。

## v0.6.6 — 2026-09-20

### English

- Added experimental native ultrawide (21:9) support (Issue #17); only 3440×1440 is supported.
- Fixed shadows at all aspect ratios and high internal resolutions.
- v0.6.6 was reissued on 2026-09-20 with the shadow fix; redownload it if you got the first package.
- On Linux, the AppImage updater now removes the old AppImage after a successful update and keeps it if the new one fails to run.

### 简体中文

- 增加实验性原生超宽屏（21:9）支持（Issue #17）；目前仅支持 3440×1440。
- 修复所有显示比例与高内部分辨率下的阴影问题。
- v0.6.6 已于 2026-09-20 重新发布以包含阴影修复；如果下载的是初版，请重新下载。
- Linux 上 AppImage 更新器在更新成功后删除旧版 AppImage，新版本无法运行时则保留旧版。

## v0.6.3 — 2026-09-19

### English

- Improved CPU performance during drawing by using sampled vertex-cache comparison for large buffers.
- Hardened the language menu (Issue #54): invalid language entries no longer change your selection.
- Fixed a possible file I/O locking problem (Issue #53).
- F1 render-state exports are now `.zip` on Windows and `.tar.gz` on Linux.

### 简体中文

- 对大缓冲使用采样式顶点缓存比较，降低绘制时的 CPU 开销。
- 加固语言菜单（Issue #54）：无效的语言条目不再改变你的选择。
- 修复一处可能的文件 I/O 锁问题（Issue #53）。
- F1 渲染状态导出在 Windows 上为 `.zip`，在 Linux 上为 `.tar.gz`。

## v0.6.2 — 2026-09-19

### English

- Applied the accepted TAA settings to the normal TAA path.
- Experimental motion-vector replay for TAA is now on by default; `LO_MV_ENABLE=0` turns it off.
- Improved performance with an index cache that reuses index fingerprints.

### 简体中文

- 将已接受的 TAA 设置应用到正常 TAA 路径。
- TAA 默认启用实验性运动矢量 replay，可用 `LO_MV_ENABLE=0` 关闭。
- 通过复用 index fingerprint 的 index cache 提升性能。

## v0.6.1 — 2026-09-18 / Published / 已发布

### English

- Windows and Linux now check for updates before importing game data.
- When a newer release exists, a prompt shows its release notes with **Install** and **Later**, and accepting updates and relaunches before import.

### 简体中文

- Windows 和 Linux 现在会在导入游戏资料前检查更新。
- 发现新版本时，提示显示发布说明以及“安装”和“稍后”，接受后在导入前更新并重新启动。

## v0.6.0 — 2026-09-18 / Published / 已发布

### English

- Fixed shader failures being treated as permanent, and shader/PSO preparation can now be cancelled.
- Fixed Linux updates so the previous AppImage is restored if the new one cannot start.
- Importer only publishes imported data after it was fully written, and the destination browser can create a new folder (button, `F2` or controller `Y`).

### 简体中文

- 修复 shader 暂时失败被当作永久失败的问题，并可取消 shader／PSO 准备。
- 修复 Linux 更新：新版本无法启动时恢复旧版 AppImage。
- 导入器仅在数据完整写入后才发布，目标目录页支持新建文件夹（按钮、`F2` 或手柄 `Y`）。

## v0.5.20 — 2026-09-17 / Published / 已发布

### English

- Fixed black light fixtures in Numara Castle's Philosopher's Chamber (Issue #38).
- Fixed TAA flicker on the stairs and save point in `f2358`.
- Added a portable Vulkan shader pack (`.lospv`) so the game starts without compiling shaders.
- Faster shader prebuilding, with `LO_SHADER_WORKERS` and `LO_PIPELINE_WORKERS` to set worker counts.
- The shader preparation screen can be skipped with ESC, Space or Controller B; `skip_shader_prebuild` in `settings.ini` disables it.
- Fixed debug overlay problems: pausing no longer hangs the game, close buttons no longer leak into the game, and Linux shows only the Vulkan backend.

### 简体中文

- 修复努玛拉城哲学者之间的黑光灯具问题（Issue #38）。
- 修复 `f2358` 场景阶梯与保存点处的 TAA 闪烁。
- 增加便携式 Vulkan 着色器包（`.lospv`），启动时无需编译着色器。
- 加快着色器预构建，可用 `LO_SHADER_WORKERS` 与 `LO_PIPELINE_WORKERS` 指定工作线程数。
- 着色器准备界面可按 ESC、空格键或手柄 B 键跳过；`settings.ini` 的 `skip_shader_prebuild` 可关闭该步骤。
- 修复调试浮层问题：暂停不再卡死游戏，关闭按键不再泄漏到游戏，Linux 仅显示 Vulkan 后端。

## Historical development checkpoints / 历史开发检查点

### English

- Added an in-game debug overlay (F1 or LB+RB) and a software-rendered settings menu that no longer depends on Windows GDI.
- Added Linux installer, updater and XDG user-path support; merged the importer and updater into `LostOdysseyRecomp.exe`; the SDL installer now imports discs and DLC together, with controller navigation.

### 简体中文

- 新增游戏内调试浮层（F1 或 LB+RB），设置菜单改为软件渲染，不再依赖 Windows GDI。
- 新增 Linux 安装器、更新器与 XDG 用户路径支持；导入器与更新器合并进 `LostOdysseyRecomp.exe`；SDL 安装器可一并导入光盘与 DLC，并支持手柄导航。

## v0.5.14 — 2026-09-16 / Published / 已发布

### English

- The main binary now includes the installer and updater, with safer DLC import and more reliable scan and retry.
- Added a Linux x64 AppImage alongside the Windows x64 ZIP.
- Replaced the Linux AppImage with one that includes the missing `AppRun`, fixing the launch error.

### 简体中文

- 主程序现已内置安装器与更新器，DLC 导入更安全，扫描与重试更可靠。
- 新增 Linux x64 AppImage，与 Windows x64 ZIP 并列提供。
- 已替换缺少 `AppRun` 的 Linux AppImage，修复启动报错。

## v0.5.13 — 2026-09-14 / Published / 已发布

### English

- Added **Alt+Enter** to switch between **Windowed** and **Borderless** modes.

### 简体中文

- 新增 **Alt+Enter**，在 **Windowed** 与 **Borderless** 模式间切换。

## v0.5.12 — 2026-09-14 / Published / 已发布

### English

- Fixed USA/Europe FMV and event subtitles showing English in other languages. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27).

### 简体中文

- 修复 USA/Europe FMV 与事件字幕在其他语言下显示英文的问题。[#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27)。

## v0.5.11 — 2026-09-14 / Published / 已发布

### English

- Added startup and GPU failure diagnostics to help investigate Issues [#6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) and [#22](https://github.com/freefrank/LostOdysseyRecomp/issues/22).

### 简体中文

- 增加启动与 GPU 失败诊断，用于排查 [#6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) 与 [#22](https://github.com/freefrank/LostOdysseyRecomp/issues/22)。

## v0.5.10 — 2026-09-13 / Published / 已发布

### English

- Fixed more Vulkan TAA flicker cases.

### 简体中文

- 修复更多 Vulkan TAA 闪烁情况。

## v0.5.9 — 2026-09-13 / Published / 已发布

### English

- Improved Vulkan performance with depth-clear coalescing and fewer redundant descriptor bindings.

### 简体中文

- 通过合并深度清除并减少重复 descriptor 绑定，提升 Vulkan 性能。

## v0.5.8 — 2026-09-13 / Published / 已发布

### English

- Extended TAA jitter coverage to more main-camera shaders.

### 简体中文

- 扩展 TAA jitter 对更多主相机 shader 的覆盖。

## v0.5.7 — 2026-09-13 / Published / 已发布

### English

- The updater can recover from an updater-only install, bad local metadata or a missing game executable, then asks whether to launch the game (default **No**).
- Fixed lighting flicker in the reported scene with TAA and added a bloom prefilter fix.

### 简体中文

- 更新器可从仅有更新器的安装、损坏的本地 metadata 或缺少游戏可执行文件的状态恢复，随后询问是否启动游戏（默认**否**）。
- 修复报告场景中 TAA 下的光影闪烁，并加入 bloom prefilter 修复。

## v0.5.6-hotfix1 development record / 开发记录

This development record was never released separately; its scope was included in v0.5.7. / 本开发记录从未单独发布；其范围已纳入 v0.5.7。

### English

- Updater recovery and TAA flicker fixes later released in v0.5.7.

### 简体中文

- 更新器恢复与 TAA 闪烁修复，后于 v0.5.7 发布。

## v0.5.6 — 2026-09-13 / 已发布

### English

- Issue #16: fixed the freeze in the `xf_shd_aniflz.freeze` particle-material case.
- Reduced stutter in D3D12 and the renderer.

### 简体中文

- Issue #16：修复 `xf_shd_aniflz.freeze` particle-material 情况下的卡死。
- 降低 D3D12 与渲染器卡顿。

## v0.5.4 — 2026-09-11

### English

- Fixed installer window dragging.

### 简体中文

- 修复安装器窗口拖动。

## v0.5.3 — 2026-09-10

### English

- Fixed updater handling of ZIP packages with a root-directory entry.

### 简体中文

- 修复更新器处理带根目录条目的 ZIP 包。

## v0.5.2 — 2026-09-10

### English

- Manual F1 render captures now include the original shader microcode.

### 简体中文

- 手动 F1 渲染捕获现附带原始 shader 微码。

## v0.5.1 — 2026-09-10

### English

- Double-click `LostOdysseyUpdater.exe` beside the game to update without launching the game; it starts the game after a successful update.

### 简体中文

- 在游戏旁双击 `LostOdysseyUpdater.exe` 即可更新，无需先启动游戏；更新成功后会启动游戏。

## v0.5.0 — 2026-09-09

### English

- Added Windows Vulkan alongside D3D12, with automatic fallback.
- Discs and DLC are recognized automatically from files, folders or mixed selections; the game starts directly from the executable.
- Refreshed the installer, updater, first-run setup and Debug Menu; graphics settings save in one click, with Now/Later when a restart is needed.
- Fixed the Issue #12 crash and DLC directory filtering; Xenia saves can be copied directly to Recomp; added DPI-correct window sizing and Alt+Enter fullscreen switching.

### 简体中文

- 新增 Windows Vulkan，与 D3D12 并存，支持自动回退。
- 从文件、文件夹或混合选择中自动识别光盘与 DLC；可直接运行游戏程序。
- 改进安装器、更新器、首次设置和 Debug Menu；图形设置单击保存，需要重启时可选“现在”或“稍后”。
- 修复 Issue #12 的崩溃与 DLC 目录过滤；支持直接复制 Xenia 存档到 Recomp；窗口按 DPI 正确确定大小，并加入 Alt+Enter 全屏切换。

## [v0.4.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.2) — 2026-09-08

### English

- Fixed the Uhra Council cutscene crash.
- Corrected several game logic bugs from PowerPC translation.
- Added battle TAA coverage; F1 captures now compress in the background.

### 简体中文

- 修复乌拉议会过场崩溃。
- 修正 PowerPC 翻译导致的若干游戏逻辑错误。
- 补充战斗 TAA 覆盖；F1 捕获现于后台压缩。

## Published / 已发布

### [v0.4.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.1) — 2026-09-08

#### English

- Fixed Map3 tire-shadow flicker with TAA enabled.
- F1 now captures three consecutive frames into one ZIP; `LO_DEBUG_CAPTURE_DRAW_STEPS=1` restores draw previews.

#### 简体中文

- 修复 Map3 轮胎在启用 TAA 时的阴影闪烁。
- F1 现连续捕获三帧合并为一个 ZIP；`LO_DEBUG_CAPTURE_DRAW_STEPS=1` 可恢复逐绘制预览。

### [v0.4.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.0) — 2026-09-07

#### English

- Added real internal resolution up to 4K (Auto, or manual 720p to 2160p), with preview and rollback.
- Added SMAA 1x and experimental TAA.
- Added saved 30/60 FPS controls; the 120 FPS option requires `LO_EXPERIMENTAL_120=1`.
- Faster shader discovery for both editions; `LO_SHADER_FULL_SCAN=1` forces a full rescan.
- Added Debug menu language switching; removed the unimplemented DLSS/frame-generation controls.

#### 简体中文

- 新增最高 4K 的真实内部分辨率（Auto 或手动 720p 至 2160p），支持预览与回退。
- 新增 SMAA 1x 与实验性 TAA。
- 新增可保存的 30／60 FPS 控制；120 FPS 选项需要 `LO_EXPERIMENTAL_120=1`。
- 两个版本的 shader 发现更快；`LO_SHADER_FULL_SCAN=1` 可强制完整重扫。
- 新增 Debug 菜单语言切换；移除尚未实现的 DLSS／帧生成控件。

### [v0.3.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.3.0) — 2026-09-07

- Shaders are discovered before gameplay and used pipelines are prepared in parallel on later launches. / 游戏开始前发现 shader，后续启动并行预创建用过的管线。

### [v0.2.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2) — 2026-09-07

- Fixed black/dark title, background and depth-of-field output on AMD; preserved Unicode Windows startup and save paths. / 修复 AMD 上标题、背景与景深全黑／偏黑；保留 Windows Unicode 启动与存档路径。

### [v0.2.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.1) — 2026-09-06

- Added F1 render-state capture; controllers and keyboard combine into player 1 with hotplug. / 增加 F1 渲染状态捕获；手柄与键盘合并到玩家 1，支持热插拔。

### [v0.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2) — 2026-09-06

- Added USA/Europe 0.0.0.3 support alongside Asian 0.0.0.4; text and voice choices follow the installed edition. / 增加欧美 0.0.0.3 支持，保留亚洲 0.0.0.4；文本与语音选项按安装版本提供。

### [v0.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.1) — 2026-09-06

- First experimental portable Windows x64 release with graphical importer and first-launch setup for the four-disc Asian edition. / 首个实验性便携 Windows x64 版本，含图形导入器及亚洲四盘版首次设置。
