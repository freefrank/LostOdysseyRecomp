# 安装 Lost Odyssey Recomp

[English](INSTALLING.md)

下载对应平台的安装包，从自己的光盘导入游戏，更新时保留存档。下载包不含游戏文件；启动需要 Disc 1。

## 目录

- [Windows 快速开始](#windows-快速开始)
- [导入游戏数据](#automatic-content-import)
- [追加或替换光盘和 DLC](#追加或替换光盘和-dlc)
- [Linux 安装包](#running-on-linux)
  - [AppImage](#appimage)
  - [Flatpak](#flatpak)
- [macOS（Apple Silicon）](#macos)
- [Android](#android)
- [首次设置和普通设置](#首次设置和普通设置)
  - [着色器准备](#shader-preparation)
- [文件位置](#file-locations)
- [导入 Xenia 或 Xbox 360 存档](#importing-saves)
  - [从 Xenia 导入](#xenia-saves)
  - [从 Xbox 360（RGH）导入](#console-saves)
- [命令行参数](#命令行参数)
  - [游戏目录的查找顺序](#游戏目录的查找顺序)
- [更新和保留个人数据](#更新和保留个人数据)
- [报告启动或画面问题](#报告启动或画面问题)

## Windows 快速开始

需要 Windows x64 和支持 AVX 的 CPU。默认使用 Direct3D 12，也可以在设置中改用 Vulkan。

1. 从[最新发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/latest)下载 `LostOdysseyRecomp-windows-x64-v0.9.22.zip`。
2. 把整个 ZIP 解压到可写目录，不要放在 `Program Files` 下。
3. 运行 `LostOdysseyRecomp.exe`。找不到游戏时会打开导入器。
4. 选择语言和图形选项。游戏可能会先询问是否下载预编译着色器，见[着色器准备](#shader-preparation)。

<a id="automatic-content-import"></a>

## 导入游戏数据

在导入器的来源页面选择 **Files** 选取文件，或选择 **Folder** 扫描文件夹。导入器会自己识别光盘和 DLC；检查识别结果后确认即可。

<a id="supported-sources"></a>

支持的来源：

- 解压后的游戏文件夹，或其中的 `default.xex`；
- ISO 镜像；
- Games on Demand（GOD）数据：header 文件、对应的 `.data` 文件夹，或包含多张光盘的文件夹。

支持的版本（Title ID `4D5307FA`）：

| 版本 | Version | Disc 1–4 的 Media ID |
|---|---:|---|
| Asian multilingual | 4 | `39F7D748`、`0EF8CEA8`、`309E3386`、`7B21A91D` |
| USA/Europe | 3 | `368DE6DD`、`1888BE4E`、`6DD59D08`、`0C0E80B5` |

两套版本的光盘不能混用。其他区域版本、Title Update 和修改过的 XEX 不受支持。四张光盘都导入后，游戏会自动换盘。

## 追加或替换光盘和 DLC

从**设置 → 系统 → 导入光盘与 DLC**（Settings → System → Import discs & DLC）重新打开导入器。选择要添加或替换的光盘或 DLC，检查结果后确认。其他光盘、存档和设置保持不变。DLC 可以直接选择，也可以放在扫描的文件夹里；下载包不含 DLC。

导入器只复制文件，不会移动或修改原文件；确认游戏可用前请保留原始文件。导入取消或失败时重新导入即可。

在目标目录页，选择 **New folder**、按 **F2** 或手柄 **Y** 可新建文件夹。游戏默认导入到 `game/disc1`–`game/disc4`，DLC 在 `game/dlc/`；选择其他文件夹时由 `game-path.txt` 记住位置。

<a id="running-on-linux"></a>

## Linux 安装包

Linux 使用 Vulkan。Steam Deck 和其他 Linux 硬件的测试还不多。从源码构建见 [BUILDING.md](BUILDING.md)。

### AppImage

下载 `LostOdysseyRecomp-linux-x64-v0.9.22.AppImage` 后运行：

```bash
chmod +x LostOdysseyRecomp-linux-x64-v0.9.22.AppImage
./LostOdysseyRecomp-linux-x64-v0.9.22.AppImage
```

可以在导入器里导入游戏，也可以直接指定游戏文件夹启动：

```bash
./LostOdysseyRecomp-linux-x64-v0.9.22.AppImage --game /path/to/game
```

AppImage 把存档和设置放在当前用户的目录，见[文件位置](#file-locations)。

### Flatpak

先从 Flathub 安装一次 Freedesktop 26.08 运行时：

```bash
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.freedesktop.Platform//26.08
```

再安装并运行下载的 bundle：

```bash
flatpak --user install --bundle LostOdysseyRecomp-linux-x64-v0.9.22.flatpak
flatpak run io.github.freefrank.LostOdysseyRecomp
```

导入器可以读取电脑上任意位置的 dump，包括 `/media`、`/run/media` 和 `/mnt`。更新时下载新的 bundle，按同样方法安装。

<a id="macos"></a>

## macOS（Apple Silicon）

需要 macOS 15 或更高版本的 Apple Silicon Mac。应用未经公证，macOS 会拦截首次启动。

1. 下载 `LostOdysseyRecomp-macos-arm64-v0.9.22.dmg` 并打开。
2. 把 `LostOdysseyRecomp.app` 拖到 Applications 链接上，然后推出磁盘映像。
3. 从“应用程序”打开 `LostOdysseyRecomp`。macOS 第一次会拦截它。打开 **系统设置 → 隐私与安全性**，滚动到“安全性”，点击应用旁的 **仍要打开**（这个按钮只在启动被拦截后出现）。macOS 再次询问时点击 **打开**，可能需要输入密码。之后的启动不再需要批准。
4. 找不到游戏时会打开导入器，见[导入游戏数据](#automatic-content-import)。

也可以在终端运行 `xattr -dr com.apple.quarantine /Applications/LostOdysseyRecomp.app` 来批准应用，然后再次打开。

首次启动可能会询问是否下载 Metal 着色器，见[着色器准备](#shader-preparation)。更新时下载新的磁盘映像，替换“应用程序”中的应用；存档和设置在应用之外，会保留。目前只在一台 Mac 上运行过。

<a id="android"></a>

## Android

需要支持 Vulkan 的 64 位 Android 8.0 或更高版本设备，四张光盘约需 20 GB 空闲空间。

1. 在设备上下载 APK 并打开，按 Android 的提示允许来自浏览器或文件管理器的安装。先打开一次应用：它会在内部存储和每张 SD 卡上建好 `Android/data/io.github.freefrank.lostodyssey/files/game/` 文件夹。
2. 用以下三种方法之一把游戏放到设备上：
   - **在设备上导入：** **Game folder → Import disc images…** 可以从内部存储或 SD 卡导入光盘镜像（`.iso`）或已解出的光盘，完成后直接启动游戏。需要“所有文件访问”权限，页面会先请求。
   - **从电脑复制：** 先用电脑版导入游戏，再用 USB 文件传输模式连接设备，把 `disc1`–`disc4`（和 `dlc/`）复制到上面的 `game` 文件夹，使 `game/disc1/default.xex` 存在。
   - **使用其他文件夹：** 设备上的文件管理器一般写不进 `Android/data`，可以用它把光盘复制到其他任意位置，再在 **Game folder → Choose folder…** 选中包含 `disc1` 的文件夹。同样需要“所有文件访问”权限。
3. 打开应用。高通设备会先显示 **GPU driver** 页面，因为手机自带的驱动会让部分菜单文字不可见：在这里下载 Turnip 驱动（页面会显示推荐给你机型的版本），然后按 **Start game**。之后可从 **CTRL → GPU driver** 再进入。如果某个驱动无法运行游戏，游戏会回到这个页面并说明原因。
4. 用屏幕上的 **A** 键接受着色器下载。**B** 跳过并在设备上编译，需要几分钟。

游戏画面上有触摸按键。**CTRL** 可以调整触摸按键的大小、透明度和布局，也能进入 Saves、Game folder 和 GPU driver 页面。连接 USB 或蓝牙手柄后触摸按键自动隐藏。设置在游戏内的设置页面修改。

开启 **Automatic updates（自动更新）** 时，应用启动会检查新版本并提供 APK 下载。新 APK 直接覆盖安装，存档和设置保留。如果装过自己构建的 APK，需要先卸载。卸载应用会删除存档和 `Android/data` 里的游戏数据。

<a id="android-saves"></a>
**存档**保存在应用内部，文件管理器访问不到。**CTRL → Saves** 可以把所有存档位导出成一个 ZIP，保存到你选的位置（例如 Download），也可以导入包含存档文件夹的 ZIP：这里或电脑上导出的、打包 Xenia `userNN` 文件夹得到的，或者 [RGH 存档转换器](#console-saves)下载的 ZIP。卸载应用前请先导出存档。

<a id="android-logs"></a>
**日志**在 `Android/data/io.github.freefrank.lostodyssey/files/logs/`：`runtime-*.log`、`native-stderr.log`，应用本身出错时还有 `java-crash-*.txt`。游戏闪退或一直黑屏时，先再打开一次应用，然后用 USB 把这些文件复制到电脑，附到报告里。使用 adb 时，`adb logcat -s LostOdyssey` 可以实时看到同样的内容。

目前只在一台平板上测试过。

## 首次设置和普通设置

游戏以默认设置启动，之后在游戏内设置页面修改：在标题菜单按 Y（键盘 S），或在游戏中从营地菜单打开 系统 → 设置。存在存档里的选项（文字速度、字幕、语音和音乐音量等）只在游戏中显示。Windows 上运行 `LostOdysseyRecomp.exe --setup`，会在游戏启动前打开设置页面，选择界面语言、游戏语言和图形选项。需要重启的改动，设置页面会提示。

在图像设置中，**显示模式**可选窗口或全屏。电脑有多台显示器或多张显卡时，**显示器**选择用哪台显示器，**GPU** 选择用哪张显卡（GPU 的改动重启后生效）。切换显示器后，游戏会询问是否保留，5 秒内没有回应就回到原来的显示器。在 Windows 上，Win+Shift+左／右方向键也可以把游戏移到相邻的显示器。**画面比例**可选自动（画面填满窗口），或 16:9、21:9、4:3（画面保持该比例，屏幕比例不同时加上黑边）。在游戏设置中，**震动**调整震动强度。在声音设置中，**语音音量**单独调整过场对白的音量；原版里它跟随音效音量，战斗中的语音现在仍是如此。**音频输出**选择立体声或 5.1 环绕声；使用 5.1 前，请先在系统声音设置中把扬声器设为 5.1 或 7.1，否则游戏仍使用立体声。Windows 上这个选项只在 控制面板 → 声音 → 播放 中：选中设备，点“配置”，选 5.1 或 7.1 环绕。**矩阵环绕声**不需要设置扬声器：它把 5.1 混音编入立体声，交给 AV 功放的 Pro Logic II、Dolby Surround 或 Neural:X 模式还原；**后方角度**设置后方声道听起来所在的位置（90°–150°，默认 110°），选中这一行时，测试声音会在 5.1 布局图上绕着扬声器转圈。

界面语言和游戏语言是两套选项。USA/Europe 光盘有英、日、德、法、西、意语言；亚洲版有英、日、韩、繁中、简中。光盘里没有的游戏语言会回退到英语。

按 **F1** 或 **LB+RB** 打开调试菜单，见 [README](../README.zh-CN.md#调试菜单)。

<a id="shader-preparation"></a>

### 着色器准备

第一次用某个渲染器启动时，游戏会询问是否下载对应的预编译着色器，并显示下载大小。

- **下载 (A)** 从 GitHub 下载，可以省去数分钟的编译。**取消 (B)** 中止下载。
- **跳过 (B)** 改为在本机编译。游戏会记住这个选择，直到着色器更新为止。不做选择直接关闭窗口，下次启动会再次询问。

更新后如果着色器有变化，第一次启动会再次提示下载。离线时游戏不会询问，直接在本机编译。之后的启动会复用编译好的着色器。

<a id="file-locations"></a>

## 文件位置

| 安装包 | 位置 |
|---|---|
| Windows ZIP | 全部在 `LostOdysseyRecomp.exe` 旁边：`save/`、`profile/`、`cache/`、`logs/`、`settings.ini`、`game-path.txt`，导入的游戏在 `game/`。 |
| Linux AppImage | 存档、个人配置、缓存和游戏：`~/.local/share/lost-odyssey-recomp/`。设置：`~/.config/lost-odyssey-recomp/`。日志：`~/.local/state/lost-odyssey-recomp/logs/`。 |
| Linux Flatpak | 在 `~/.var/app/io.github.freefrank.LostOdysseyRecomp/` 下：存档、个人配置、缓存和游戏在 `data/`（沙盒内为 `/var/data`）；设置在 `config/lost-odyssey-recomp/`；日志在 `.local/state/lost-odyssey-recomp/logs/`。 |
| macOS | 存档、个人配置、缓存、游戏和设置：`~/Library/Application Support/LostOdysseyRecomp/`。日志：`~/Library/Logs/LostOdysseyRecomp/logs/`。 |
| Android | 游戏：`Android/data/io.github.freefrank.lostodyssey/files/game/`（或在 **Game folder** 页面选择的文件夹）。日志：`Android/data/io.github.freefrank.lostodyssey/files/logs/`。存档和设置保存在应用内部；用 **CTRL → Saves** 导出和导入存档。 |

渲染捕获保存在 `captures/`，Mod 放在 `mods/`：Windows ZIP 都在程序旁边；其他安装包的捕获在设置目录，Mod 在数据目录。下载的着色器放在 `shaders/`，位置和 Mod 相同。用 Mod Organizer 2 管理 Mod 见 [Mod Organizer 2](wiki/Mod-Organizer-2.md)。

Linux 上可以用 `XDG_CONFIG_HOME`、`XDG_DATA_HOME` 和 `XDG_STATE_HOME` 改变 AppImage 的目录。放在可写目录里的 Linux 构建会像 Windows ZIP 一样把所有文件放在程序旁边。用 `--game` 启动 Windows ZIP 时，存档和设置跟随启动时所在的目录，请始终从同一个目录启动。

<a id="importing-saves"></a>

## 导入 Xenia 或 Xbox 360 存档

每个存档位是 `save/` 里的一个文件夹，例如 `save/user00/save.bin`；`save/` 的位置见[文件位置](#file-locations)。先关闭游戏并备份 `save/`。复制进去的存档会出现在游戏的读档列表里。Android 上改用 **CTRL → Saves** 导入包含存档文件夹的 ZIP（[详见](#android-saves)）。

<a id="xenia-saves"></a>

### 从 Xenia 导入

Xenia 的存档格式相同，不需要转换。

1. 找到 Xenia 的 `content` 文件夹：便携版 Xenia 在程序旁边，否则在 `Documents\Xenia\content`。Xenia Canary 默认是便携版，Xenia Manager 也这样安装，所以先在 Xenia 文件夹里找。
2. 在里面打开 `4D5307FA\00000001`。Xenia Canary 中间还有一层 16 位的个人档案文件夹：`content\<个人档案 ID>\4D5307FA\00000001`。
3. 其中每个 `userNN` 文件夹是一个存档，把需要的复制到 `save/`。

同名文件夹会替换那个存档位。要两个都保留，把复制过来的文件夹改成没用过的编号，例如 `user07`。

<a id="console-saves"></a>

### 从 Xbox 360（RGH）导入

主机上每个存档是一个文件，通常叫 `user00`，位于硬盘的 `Content\<个人档案 ID>\4D5307FA\00000001\`。先把它复制到电脑上，例如通过 FTP。

1. 用浏览器打开[存档转换器](https://freefrank.github.io/LostOdysseyRecomp/)。转换在本机完成，不会上传任何文件。
2. 选择存档文件或包含它的 ZIP，再选一个空的目标存档位。
3. 点击 **转换存档**（Convert save），下载生成的 ZIP。
4. Windows 上把 ZIP 解压到 `LostOdysseyRecomp.exe` 旁边。其他系统把 ZIP 里 `save` 文件夹中的 `userNN` 文件夹复制到你的 `save/`。

存档不能再转回主机。

## 命令行参数

| 参数 | 作用 |
| :--- | :--- |
| `--game <路径>` | 使用指定的游戏：包含 `default.xex` 或 `disc1/` 的文件夹，或 `default.xex` 文件。跳过导入器；找不到 `default.xex` 时报错退出。 |
| `--install` | 即使已经设置好游戏也打开导入器，完成后退出。 |
| `--setup` | 打开设置页面，然后进入游戏（Windows；其他平台只会保存当前设置）。 |
| `--setup-only` | 同 `--setup`，完成后退出。 |
| `--prepare-shaders-only` | 预先准备全部着色器，然后不启动游戏直接退出。 |
| `--quiet-kernel` | 日志中不记录内核跟踪行。 |

`--game <路径>` 要写成两个参数，`--game=<路径>` 和无法识别的参数都会被忽略。程序不会向控制台输出内容，请查看日志。

```bash
LostOdysseyRecomp.exe --game "D:\Games\Lost Odyssey"
./LostOdysseyRecomp-linux-x64-v0.9.22.AppImage --game ~/Games/LostOdyssey
flatpak run io.github.freefrank.LostOdysseyRecomp --game ~/Games/LostOdyssey
LostOdysseyRecomp.app/Contents/MacOS/LostOdysseyRecomp --game ~/Games/LostOdyssey
```

环境变量只在本次运行中覆盖已保存的设置：

| 变量 | 作用 |
| :--- | :--- |
| `LO_GRAPHICS_API` | Windows 上为 `d3d12` 或 `vulkan`。 |
| `LO_FPS` | 帧率上限，0 到 1000；`0` 表示不限制。 |
| `LO_FG_PROVIDER`、`LO_FG_MODE`、`LO_FG_MULTIPLIER`、`LO_FG_TARGET_FPS` | 插帧：`off`/`dlss`/`fsr`/`xess`；`off`/`fixed`/`dynamic`；2–6 倍；目标帧率（[详情](notes/vulkan-fg-fsr4-metalfx.md)）。 |
| `LO_OPTISCALER_PATH` | 仅 Windows：自备 `OptiScaler.dll` 的完整路径（[配置方法](notes/vulkan-fg-fsr4-metalfx.md#optional-optiscaler-loading-on-windows)）。 |
| `LO_NO_UPDATE` | 设为 `0` 以外的任何值即跳过更新检查。 |
| `LO_PROFILE_DIR`、`LO_SHADER_CACHE_DIR`、`LO_MODS_DIR` | 使用其他个人配置、着色器缓存或 Mod 目录。`LO_SHADER_CACHE_DIR` 设为空值会关闭着色器缓存。 |
| `LO_MODS` | `0` 或 `false` 关闭 Mod。 |
| `LO_LOG_FILE` | 把日志写到指定路径；设为 `0` 则不写日志文件。 |
| `LO_AUDIO_MUTE`、`LO_CONTROLLER_RUMBLE` | `LO_AUDIO_MUTE=1` 静音；`LO_CONTROLLER_RUMBLE=0` 关闭震动。 |
| `LO_TRACE_STARTUP` | 仅 Windows：设为 `1` 时日志会多记录启动细节：显卡适配器、加载进游戏的第三方软件（悬浮窗、录屏工具等），以及窗口与交换链耗时。 |

### 游戏目录的查找顺序

未指定 `--game` 时：

1. 读取 `game-path.txt`。
2. 没有这个文件时，在数据目录的 `game/` 中查找 `default.xex`（仅限按用户目录存放的安装包）。
3. 再依次检查程序旁的 `game/`、程序所在目录和上一级的 `../game`。
4. 都找不到时打开导入器。

## 更新和保留个人数据

游戏启动时会检查 GitHub 上的新版本；如果想自己检查，可在设置中关闭 **Automatic updates（自动更新）**。Windows 和 AppImage 可以自动更新；Flatpak、macOS 和 Android 需要安装新下载的包。

更新时保留以下内容：

- 存档、个人配置和 `settings.ini`；
- `logs/` 与着色器缓存；
- 放在程序旁边的 `game-path.txt` 和导入的游戏。

手动更新时先关闭游戏，另存一份存档和设置，并在确认新包可用前保留旧包。

## 报告启动或画面问题

附上最新的 `logs/runtime-<timestamp>.log`（Android 见 [Android 日志](#android-logs)），并写明安装包版本、图形后端、GPU 和驱动、游戏版本、光盘和场景。

如果游戏卡在启动阶段，10 秒后日志会记下卡住的步骤。请再用 `LO_TRACE_STARTUP=1` 运行一次，并一并附上那份日志。

画面问题请打开 **F1 → Overview → Capture render state（捕获渲染状态）**，确认后**关闭 F1** 让游戏继续渲染。重新打开 F1 查看归档保存的位置，检查内容后附上。
