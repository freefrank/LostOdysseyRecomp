# Issue #40 UI 资源定位与资产映射归档

本文档归档 Issue #40（手柄按键提示与 PlayStation 形状映射）排查过程中定位到的游戏 UI 包、纹理、字体和配置文件字节偏移。所有内容严格基于当前已有实测证据，区分确证、候选与已排除项。

相关英文简要入口见 [issue-40-ui-resource-map.md](issue-40-ui-resource-map.md)。

## 状态与范围说明（2026-09-26 更新）

### 宿主与客端实现及验收状态

- **宿主环境（Host Recomp UI）**：项目的 Settings 菜单、安装器（Installer UI）及调试覆盖层（Debug overlay）的手柄按键提示已完整支持 PlayStation 符号切换（包括 Cross/Circle/Square/Triangle、L1/R1/L2/R2 与 Options/Share），并通过了离线自动化测试。
- **客端环境（Guest Game Runtime）**：客端游戏运行时已实现自动 PlayStation 按键提示替换。纠正此前文档中关于“仅 common 包且客端未实现”的旧叙述。实现机制如下：
  - 基于 SDL 自动识别最近活动的控制器类型。
  - 在 GPU 上传路径中，通过对两张已确证控制器图集进行完整内容哈希识别：
    1. `rpmenurescommon_int.xxx` 中的 `Icon_Page_0`（hash 前缀 `8e181...`，BC3，256x128）
    2. 英文字体包中的 `Texture2D_1`（完整 SHA-256：`cfd30b830a2fbf5d12520ea5c6cc2a6a3a37d534f8f4c7f846fdc50520ba8229`，BC3，256x128）
  - 两张图集均通过真实 BC3 识别与 production upload fixture 验证。在原始 Xbox 360 提示纹理与 PlayStation 不可变替换纹理之间安全切换，受 GPU 停止 guard 与退休审阅保护。
  - 覆盖范围：ABXY 动作键、肩键（LB/RB/LT/RT 映射至 L1/R1/L2/R2）以及暂停菜单中的 Start/Select（Options/Create）。已修复暂停菜单 Start/Select 与肩键遗漏问题。
- **用户实机验收**：
  - 对应构建：`HEAD d677a48-dirty`，可执行文件 SHA-256 为 `1ae34ffd7eedcc415816c30b6e74b31d5ca2c0d936cd6aadbf72b49aeca709a4`。
  - 用户在暂停菜单与过场动画场景复查后，明确给出“验收通过”。本地未提交、未发布。
- **测试与验证边界**：
  - 本次复用既有 host、HID、glyph 与 GPU 测试套件及构建结果，不重复执行测试或构建。
  - 用户实机验收基于特定实体设备与测试场景，不外推至所有手柄硬件或全游戏全流程覆盖。
  - **Issue #40 关联范围**：Issue #40 同时提及了 Mod 支持（Mod support），本次仅完成 PlayStation 控制器自动按键提示替换，Mod 支持未在本次范围内实现，因此不宣称 Issue #40 全量完成。

---

## 资产包位置与字节偏移（Byte Offsets）

所有偏移量均以字节（Byte Offset）为单位记录，不使用光盘扇区号。源文件均位于 `LostOdysseyRecompLib/private/disc1/`。

本记录数据仅代表 `int`（国际版/英文）包实测结果，不宣称兼容简体中文（`sch`）、繁体中文（`chi`）或日文（`jpn`）包，亦不推断未测语言包的内容。权威位置与尺寸数据以 `out/issue40/ui-export/manifest.json` 及 `manifest.csv` 为准。

| 容器文件 | 包路径 / 资源标识 | 字节偏移（Offset） | 长度（Length） | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| `xenon_loc.fpd` | `rpmenurescommon_int.xxx` | `25069568` | `892105` | 通用菜单 UI 资源包 |
| `xenon_loc.fpd` | `rpfontscommon_int.xxx` | `23947264` | `616018` | 字体包（解压后 `871782` 字节） |
| `xenon_loc.fpd` | `rpmenuresbattle_int.xxx` | `24854528` | `213267` | 战斗界面 UI 资源包 |
| `xenon_loc.fpd` | `rpmenuresfield_int.xxx` | `25962496` | `168576` | 野外界面 UI 资源包 |
| `xenon_sys.fpd` | `bin/xenon/sys/enginefonts.xxx` | `15126528` | `104030` | 引擎系统字体包 |
| `lo.fpd` | `rpgame/config/xenon/cooked/coalesced.ini` | `0` | `209499` | 包含 RPInput 按键绑定（如 `XboxTypeS_A=XPad_A`，报告记录） |

*注：`coalesced.ini` 中的输入绑定记录来自既往排查报告，本地当前未保留解压独立 ini 副本，按报告记录归档。*

---

## 批量导出规模与权威记录（ui-export）

最新的批量导出通过标准化工具完成，生成了权威清单 `out/issue40/ui-export/manifest.json` 与 `manifest.csv`，记录了每个对象的实际容器偏移、导出索引、格式与尺寸。

### 导出统计数据

- **筛选范围**：覆盖全部 4 张光盘（disc1 至 disc4），筛选出 560 个包条目（其中 `xenon_sys.fpd` 72 个、`xenon_vfx.fpd` 8 个、`xenon_loc.fpd` 480 个）。
- **生成图像**：导出 931 个独立 PNG 图像（其中包含 351 个字体页 PNG）。
- **缩略图表**：生成 82 张分组 Contact Sheets。总入口为 `out/issue40/ui-export/sheets/INDEX.txt`。
- **解析成功率**：
  - 9 个不同包因布局格式暂不支持导致 36 次包级失败（4 张光盘上相同的 9 个包，如 `battlemenu.xxx`、`rpgameover.xxx`、`rpnavi.xxx` 等）。
  - 对于所有成功解析的包内对象，**解码失败为 0**（`failed_objects: 0`）。

---

## `rpmenurescommon` 包导出清单（32 Exports）

`rpmenurescommon_int.xxx` 包含 32 个导出项（索引 0 至 31），涵盖材质、材质表达式、重定向及 2D 纹理对象。

### 导出项结构概览

- **材质与着色器**：
  - `[0] MatBrightness`（Material，offset 13528，size 577）
  - `[1]..[3]` 材质表达式（DestColor, Multiply, ScalarParameter）
  - `[4] MatColorReverse`（Material，offset 14665，size 1066）
  - `[5]..[15]` 材质表达式（Lerp, Multiply, OneMinus, Scalar/VectorParameter）
  - `[16] ColorReverse`（ObjectRedirector，offset 18491，size 16）
  - `[31] SeekFreeShaderCache`（ShaderCache，offset 737976，size 154129）
- **核心 2D 纹理（Texture2D，依权威清单记录）**：

| 导出索引 | 资源名称 | 尺寸 | 格式 | 证据分类与内容描述 |
| :--- | :--- | :--- | :--- | :--- |
| `[17]` | `BACKYARD` | 512x128 | BC3 (Format 7) | 确证：后院战斗相关界面元素 |
| `[18]` | `Dot` | 4x4 | A8R8G8B8 (Format 2) | 确证：单点基础填充纹理 |
| `[19]` | `FIELD_NAVI_ICON` | 128x64 | BC3 (Format 7) | 确证：野外小地图/罗盘图标 |
| `[20]` | `FIELD_NAVI_SKIN` | 256x256 | BC3 (Format 7) | 确证：野外导航罗盘底盘皮肤 |
| `[21]` | `Icon_Page_0` | 256x128 | BC3 (Format 7) | **确证：控制器图标图集 1**（含肩键/扳机/摇杆及底排 ABXY；运行时已用于 PS 纹理替换） |
| `[22]` | `LO_UI_PASS` | 512x128 | BC3 (Format 7) | 确证：密码盘与数字字符界面元素 |
| `[23]` | `Result` | 512x1024 | BC3 (Format 7) | 确证：战斗结算界面背景与框架 |
| `[24]` | `TUTORIAL` | 32x32 | BC3 (Format 7) | 确证：教程提示小标志图标 |
| `[25]` | `UI_FONT` | 128x64 | BC3 (Format 7) | 确证：特定菜单英文字符/符号 |
| `[26]` | `UI_MAIN_00` | 512x1024 | BC3 (Format 7) | 确证：主界面背景、各类别徽记与装饰框架 |
| `[27]` | `UI_MAIN_01` | 512x512 | BC3 (Format 7) | 确证：界面半透明底纹与材质底板 |
| `[28]` | `UI_MAIN_02` | 512x64 | BC3 (Format 7) | 确证：计量条与进度条图层 |
| `[29]` | `window` | 256x1024 | BC3 (Format 7) | 确证：通用对话窗口边框与切片九宫格背景 |
| `[30]` | `WM_NEW_CURSOR` | 128x64 | BC3 (Format 7) | 确证：世界地图与菜单导航光标指引箭头 |

---

## 按键提示排查证据：确证、候选与已排除

本地分析图片与切片存放于 `out/issue40/ui-export/images/` 及 `out/issue40/assets/`。所有游戏资源仅用于本地分析，严禁提交到 Git。

### 1. 确证项：控制器图标图集 1（`rpmenurescommon` 的 `Icon_Page_0`）
- **对象信息**：`rpmenurescommon_int.xxx` 导出项索引 21，尺寸 `256x128`，BC3 格式，清单条目 ID `#00695`。
- **图像位置**：`out/issue40/ui-export/images/disc1/bin/xenon/loc/int/menu/rpmenurescommon_int.xxx/0021_Icon_Page_0.png`，对应 Contact Sheet 为 `sheets/disc1/int/rpmenurescommon/001.png #00695`。
- **核验结论**：主代理直接核读该图像，清晰显示 Xbox 手柄符号：包括上中区域的肩键与扳机（LB、RB、LT、RT）、摇杆（LS、RS），以及底排的四色按键：A（绿）、B（红）、X（蓝）、Y（黄）。内容哈希前缀为 `8e181...`。
- **运行时应用**：客端渲染在 GPU 上传时完整识别此哈希，并安全切换为不可变 PlayStation 替换纹理。

### 2. 确证项：控制器图标图集 2（英文字体包中的 `Texture2D_1`）
- **对象信息**：`rpfontscommon_int.xxx` 导出项 `Texture2D_1`，尺寸 `256x128`，BC3 格式。
- **哈希确证**：完整 SHA-256 内容哈希为 `cfd30b830a2fbf5d12520ea5c6cc2a6a3a37d534f8f4c7f846fdc50520ba8229`。
- **运行时应用**：客端运行时通过真实 BC3 识别与 production upload fixture 确认其承载文字流与暂停界面中的按键提示，已纳入 PlayStation 替换路径。

### 3. 确证项：屏幕交互 A 键提示与暂停菜单提示
- **证据来源**：历史运行截屏 `out/critical-section-camp-01/shot_12213.ppm`（营地物品“Open”旁约 `(1053, 661)` 的 18x18 绿底 A 键）及暂停菜单界面。
- **实机覆盖**：最新修复补齐了暂停菜单 Start/Select 与肩键遗漏，已在真实硬件上经用户复查，暂停菜单与过场场景中的提示已正确呈现 PlayStation 符号并获验收。

### 4. 已排除项（Excluded）：`UI_MAIN_00` 彩色徽记
- **位置**：`UI_MAIN_00` 纹理坐标 `y=705, x=193..351`。
- **排查结论**：早期推测可能为 ABXY 彩色按键。切片分析（`Btn_Row1_1_193_705_30x30.png` 至 `Btn_Row1_5_322_705_29x30.png`）证实其为 5 个具有装饰花纹的彩色徽章图案，并非手柄功能按键，确凿排除。

### 5. 候选未确认项（Candidate - Unconfirmed）：`UI_MAIN_00` 白色圆环
- **位置**：`UI_MAIN_00` 纹理坐标 `y=737, x=257..323`。
- **排查结论**：切片分析（`Btn_Row2_4_257_737_20x20.png` 至 `Btn_Row2_7_323_737_20x20.png`）显示存在 4 个尺寸为 `20x20` 的白色空心圆形环。因内部无字母标识，暂列候选。

---

## 字体包范围（`rpfontscommon_int`）

基于权威清单 `manifest.json` 与已导出的图像文件，本次实际导出的字体资产范围如下：

- **`Maru23`**：共 1 页，尺寸 `512x512`，包含 199 个字符（`rpfontscommon_Maru23_p0.ppm` / `Maru23_Page0.png`），高度参数为 31。
- **`Abc`**：共 2 页，尺寸分别为 `256x256` 和 `256x128`，199 字符，高度参数 26。
- **`Arial18`**：共 3 页，前两页 `256x256`，第三页 `256x128`，199 字符，高度参数 33。
- **`BigNum`**：共 4 页，前三页 `256x256`，第四页 `256x128`，199 字符，高度参数 41。
- **`LocTit1`**：共 3 页，尺寸均为 `512x512`，199 字符，高度参数 70。
- **`LocTit2`**：共 1 页，尺寸 `512x512`，199 字符，高度参数 40。
- **`Meiyo18`**：共 1 页，尺寸 `512x256`，199 字符，高度参数 25。
- **`Meiyo26`**：共 4 页，尺寸均为 `256x256`，199 字符，高度参数 47。
- **`Texture2D_1`**：已确证为控制器提示图集 2，参与客端 PS 替换。
- **`Icon`**：未解出（解码报错：unsupported menu asset）。

---

## 可维护工具与复现流程（`tools/ui_assets/`）

仓库现已建立可维护工具目录 `tools/ui_assets/`，包含完整的 CMake 构建配置与导出流水线脚本。生产代码 `LostOdysseyRecomp/settings/menu_assets.cpp` 保持未修改状态。

### 真实复现命令

在配置好 `clang-cl`、Ninja、CMake 及 Python Pillow 的 Windows 环境下，于仓库根目录执行：

```bat
call tools\setup_windows.bat
cmake -S tools\ui_assets -B out\issue40\ui-export\build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl
cmake --build out\issue40\ui-export\build --target LoUiAssetDecoder
python -B tools\ui_assets\export.py --private LostOdysseyRecompLib\private --output out\issue40\ui-export
```

### 工具说明与合规规则

1. `export.py` 读取各盘片 `LO.fpi` 索引并挑选目标 UI、菜单、字体和系统资源包，调用 `LoUiAssetDecoder` 进行解码，再由 Pillow 输出透明 PNG 与分组 Contact Sheets。
2. 导出的图像、Contact Sheets 和清单均存放于 `out/issue40/ui-export/`，属于临时生成产物，**严禁提交到 Git**。
3. 源码 `tools/ui_assets/`（`CMakeLists.txt`、`decode.cpp`、`export.py`、`README.md`）已纳入版本维护。

---

## 结论与后续规划

1. **已完成与验收**：宿主界面与客端游戏运行时的自动 PlayStation 控制器提示替换已完整实现，并在本地构建上通过用户实机验收（涵盖动作键、肩键及暂停菜单 Start/Select）。
2. **范围界定**：用户验收不推及全硬件与全游戏场景；Issue #40 中提及的 Mod 支持未在本次范围内实施，后续如需 Mod 支持将独立建项推进。
