# 路线图

[English](ROADMAP.md) · [当前状态（英文）](STATUS.md)

更新于 **2026-09-08**，涵盖 v0.4.2 发布状态。`[x]` 表示所述范围已完成验证，不代表全游戏通关。历史调查记录保留当时状态；当前发布与验证记录见 [STATUS.md](STATUS.md)。

状态：`[ ]` 计划／待完成 · `[~]` 进行中 · `[x]` 已在所述范围验证。

工作项状态和优先级由[维护者公开 Project](https://github.com/users/freefrank/projects/3) 管理。本路线图是仓库内的公开中文镜像，由 [project_manager](agents/project-management.md) 按需同步；实现、验证、玩家验收与发布证据分别保留。

## 当前进度与下一步 — 2026-09-08

- **版本与交付：**`0.4.2` 后共有 16 个功能批次已本地提交：原有 13 个为源码 `0.4.3` 至 `0.4.15`，另有 0.4.16 安装器（`b094a1a`）、0.4.17 更新器（`9c2dc76`）和 0.4.18 Debug UI（`294df07`）。这些不是构建或公开 Release；此前合并报告记录 61 次编译、2 次链接和 1 个包。见[产物身份与验证](STATUS.md#current-milestone-development)。发布基线仍为 [v0.4.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.2)。
- **v0.5.0 工作：**共跟踪 27 项：22 项 Done、1 项 In Progress、2 项 Todo、2 项 Awaiting validation。安装器、更新器与 Debug Menu 本轮 UI 改进均已完成限定验证，分别记为本地源码 0.4.16、0.4.17、0.4.18。新增游戏主窗口按实际像素尺寸、不随桌面 DPI 缩放的需求仅记录为待办。玩家验收与发布状态另行保留。
- **P1——等待验证：**[Issue #12](https://github.com/freefrank/LostOdysseyRecomp/issues/12) 已确定为 GC 与渲染对象生命周期竞争；源码修复已从 `claude-issue12` 本地合并到 `0.5.0`，提交 `9cefb0d`。官方 v0.4.2 的 run09/10 捕获旧材质归属；加延迟的 run13 记录 5 次悬空绘制，run14 在 GC 前排空并移除旧 proxy 后为 0 次、无崩溃。run14 使用链式探针、人工延迟并排除 `poll_wait`，当前正式构建、正常时序交花和直接相关性能覆盖仍待完成；源码暂留 `0.4.18`，目标仍为 v0.5.0，尚未发布或取得本修复的玩家验收。报告者的独立火把 workaround 已记录，不算本补丁验收。见[根因报告](notes/issue12-root-cause.md)和[交接文档](notes/issue12-handoff-2026-09-09.md)。
- **下一步——开发：**Direct3D 11 可行性和后端保留为 v0.5.0 之外的未来工作，未指定下个版本或日期。用户已确认 CPU Vulkan 对照通过；此前停止的采集未保留配对指标，不补写新的 benchmark 数字。
- **下一步——证据或验收：**原版风格设置仍使用近似的系统字体／程序纹理；冻结安装器与 DPI 覆盖仍开放；两项保留的 shader compiler error 已被缓存，并未修复。玩家验收单独保留。
- **保持挂起或等待证据：**Issue #9 已按项目用户验收关闭，不声称原报告者确认或两个症状存在共同根因。AMD 阴影调查和敌人消散闪烁保留原有待办或暂停状态，本次整理不自动恢复。Issue #10 记录 v0.5.0 之外未来的 FSR 工作；Issue #11 已完成用户确认的 Xenia → recomp 直接复制读取；反向兼容与转换不在此边界内。

## 已发布里程碑：v0.4.2 — 修复与验证

以下修复及有界验证已完成。原报告者确认和更广流程推进单独保留为后续工作。

- [x] 修复本机复现的议会崩溃：字宽 switch 分派使用客体低 32 位。843 个表的合约核对、109 项生成代码检查和旧索引负对照通过。见[议会根因](notes/issue7-cutscene-crash.md)。
- [x] 修正另外九类 PPC 标量、寻址与分支翻译错误，并同步受跟踪的依赖补丁。重新生成的代码通过 3,258 项原生检查；冻结的旧生成器在相同输入中有 1,533 项失败。见[实现与未覆盖范围](notes/recompiler-width-audit.md)。
- [x] 核验精确的 `v0.4.1-issue7-semantics-r2` 候选版：完整通过议会剧情、恢复主街移动、原生保存及独立重启读档。实测路径使用亚洲版 Disc 1、英文、FXAA、1280x720 和 30 FPS。构建／源码审计及开发 ZIP 全部 47 个成员的哈希和 CRC 通过；不代表完整通关。见[实景与包验证](notes/issue7-cutscene-crash.md#2026-09-07-follow-up-semantics-implementation)。
- [ ] 由 Issue #7 原报告者使用其存档与硬件确认结果；本机复现不证明与其未记录异常完全相同。Issue 已关闭，本项跟踪剩余确认。
- [ ] 使用原生存档继续后续章节、其他版本及硬件回归，记录新增语义修复实际自然到达的路径，包括 BLRL；已验证的本机议会修复仅在复发时重新打开。
- [x] 六条战斗 TAA 路径通过 17,287 项 CPU 检查及限定的 Map3 32 相位场景／轮胎回归；后台 ZIP 和三日志保留通过独立用例与导出实跑。相关结果属于各自候选版，见[验证边界](STATUS.md)。
- [x] 从 `2ed7a2d` 发布 v0.4.2：CI 34192600181、manifest／CRC、内嵌版本、哈希和匿名下载核验通过。正式包仅做构建与产物检查，复用既有功能证据，不重复启动或游戏测试。见[发布核验](STATUS.md)。
- [ ] 继续排查敌人消散闪烁；本次六路径修复未解决该报告。

## 已发布里程碑：v0.4.1

1. [x] 修正已核对的 TAA 补光／深度偏移及阴影重建坐标不一致。本地实现、整合构建及选定 CPU／GPU 检查通过，见[验证范围](notes/shadow-texture-lod.md)。
2. [x] 第一候选 `6d3bc037…` 原场景验收失败后，补齐两个遗漏的场景深度材质层。r2 的整合构建、8,192 项 CPU jitter 检查及同位置 Map3、TAA／60 FPS／Auto 1440p 实际绘制检查通过；近处轮胎 ROI 平均亮度波动下降，单像素仍有变化。见[对照与边界](notes/shadow-texture-lod.md)。
3. [x] 完成 r2 原 Map3 位置的 TAA／AA Off 对照及玩家视觉验收。用户确认启用 TAA 后轮胎不闪，运行中的 EXE 与 r2 一致。本机 Map3 缺陷已解决；其他地图、运动场景和硬件列为回归，第一候选失败记录继续保留。

[v0.4.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.1) 已于 2026-09-08 00:03:50 UTC 从 `eb43f10` 发布，包含已验收的 TAA 修复及带日志的三帧导出。发布 CI 34171122945、全部 45 项 manifest、安装器自测、8 项无图形启动路径及匿名下载核验通过。正式包检查未重做 GPU 或玩家视觉验收；相关范围保留在上方 r2 证据中。独立 AMD 首战与另一 RX 9060 XT 报告继续分别挂起。见[发布证据](STATUS.md)。

<a id="v050-pc-graphics"></a>

## 下一主版本：v0.5.0 — PC Vulkan

按用户要求于 **2026-09-07** 记录，Windows Vulkan 功能已在受限的本机 RTX 5080 范围完成：`0.4.15` 源码树是历史 Vulkan／package 验证检查点（未发布开发构建；公开基线仍为 v0.4.2），当前源码为 `0.5.0`；22,933 个 shader corpus 成功，另有两个保留的 DXIL 同样失败项；Map3 Off、Map2 SMAA/TAA 与 D3D12 对照、调整大小／重启、Map12 TAA／历史和缓存恢复均通过。Map12 的 256 帧中有 253 帧 ready/completed/reused；三次 capture stall reset 后恢复，jitter misses 为零。三帧 raw 已保存，生产 host archive 核验 273 个条目；该 host 完成不代表原 game worker 自然退出。DX11 保留为 v0.5.0 之外的未来工作，并保留可用的 D3D12 基线。跨 GPU 与全游戏回归另行保留。Linux／Steam Deck 和 Switch 需要独立的平台工作与验收。见 [Vulkan 报告](../out/v0.5.0/vulkan/REPORT.md)。

源码 `0.4.19` 已完成 D3D12／Vulkan 的后端选择和类型化缓存隔离。选择策略定义能力下限、有界回退、清理及明确的 DX11 Unsupported 行为；缓存 envelope 绑定 backend、format、compiler、translator、options、variant，并拒绝过期或损坏的 success 文件。焦点化 policy、lifecycle、cold/warm/restart 与 framing 检查通过，唯一 runtime build exit 0。保留的 DXBC 识别及 DX11 失败行为不表示实现或验证 DX11 runtime。D3D12／Vulkan 场景验证范围继续保留 2026-09-08 的用户肉眼 Done／Validated；DX11 runtime、fixture、场景对照和 F1 capture 是 v0.5.0 之外的未来工作。更广跨 GPU 覆盖等待用户反馈，不是 v0.5.0 发布前要求。

1. [x] 实现类型化的 D3D12／Vulkan 选择与能力策略，提供明确的 DX11 Unsupported、有限回退和部分启动状态清理。焦点化 policy 检查覆盖 21 个 staged failure／exception case；RTX 5080 native-device lifecycle probe 创建／释放所需 D3D12／Vulkan 对象，不创建窗口、不启动 guest、不提交、不绘制或呈现。DX11 runtime 支持仍独立。
2. [x] Vulkan 第一批：平台无关的着色器、线程生命周期、内存接口及拷贝／交换链修正已在所述 Windows Vulkan 范围完成。当前 guest 时基、D3D12 深度清除修复及子模块加补丁流程保持；Switch 专属改动留在其分支。
3. [x] Vulkan 第二批：DXIL／SPIR-V 编译和着色器创建覆盖 guest 微码与当前内置 pass。实际 Vulkan corpus 记录 22,933 个成功；余下两个失败项也在保留 DXIL 对照中失败，不计作 Vulkan 回归。
4. [x] 将 D3D12／Vulkan success cache 绑定到类型化的 backend／format／compiler／translator／options／variant 身份及 digest-checked envelope。焦点化检查覆盖身份、损坏、legacy 拒绝、cold/warm/restart 复用及 framing；隔离 cold 编译有 2 次实际 DXC 调用，warm/restart 为 0。保留的 DXBC framing 识别不表示 DX11 编译或 runtime 支持。
5. [x] Vulkan 第三批：Windows SDL／WSI、运行时选择、DXC 和 loader／volk 依赖、descriptor／constant 布局、资源屏障、同步、resolve、回读与捕获已在所述 Windows Vulkan 范围完成。
6. [x] Vulkan 的 presentation、SMAA、TAA／历史缓冲、内部分辨率及独立 UI 路径已在所述范围完成，包含窗口大小调整和重置。Map12 有受限的历史／capture-stall 恢复证据；更广场景与硬件回归另行保留。
7. [ ] **未来工作，v0.5.0 之外：**按现有渲染抽象验证 DX11 可行性及最低能力：评估 DXBC／Shader Model 5 编译、逐 draw 资源／采样器重映射、slot／UAV 限制、几何着色器与同步回读。选定设计前记录不支持项和回退方案；当前后端与着色器格式接口没有 DX11／DXBC 实现。
8. [ ] **未来工作，v0.5.0 之外：**实现并接通 DX11 后端的编译、缓存、资源绑定、渲染、呈现、SMAA／TAA、缩放／UI 及捕获全路径，完成状态与 Vulkan 独立跟踪。
9. [x] 用户已于 2026-09-08 肉眼验收受限的原生存档 D3D12／Vulkan Map2／Map3／Map12 实景与对照边界。DX11 验证、跨 GPU 回归和全游戏覆盖仍开放；不表示支持 DX11。
10. [x] 完成受限的 native D3D12／Vulkan lifecycle 验证：source `0.4.20` lifecycle 修复后，隐藏 640×360→800×450 window/swapchain cycle、同进程 re-init、failure cleanup、外部 SDL reference 保留、受控 restart 及 SDL_QUIT close 均通过。fixture 不运行 guest、draw、present、capture、save/profile/settings-file 或音频路径；此前用户场景验收只复用为未改渲染行为证据。DX11 runtime 及更广硬件／lifecycle 验证仍是独立 Todo。
11. [ ] 在能获得的 AMD、NVIDIA 和 Intel 硬件上采集实际包的画面与稳定性证据，记录驱动、版本、场景和设置。未获得的硬件或场景标记待验，玩家验收与自动检查分开记录。
12. [ ] **等待验证：**历史 source `0.5.0` 的本地开发候选 build/package 检查已完成：build 4497 和 package 47942 均 exit 0；49 files／42 notices 的 ZIP 通过 CRC、manifest hashes、无重复名、DXC 2407、source provenance 和顶层 PE import 检查。hosted CI、匿名下载和公开 Release 仍待完成；DX11 及广泛 AMD／Intel 覆盖是未来／用户反馈工作，发布兼容范围只以已建立证据为准。

### v0.5.0 体验优化待办

用户于 2026-09-08 提出。以下是与上方 PC 后端规划并列、优先 Windows 的独立需求；12 项均已完成本轮实现及必要的限定验证，玩家验收和发布证据仍按各项所述范围分别保留。详见[体验优化需求](notes/v0.5.0-qol-requirements.md)。每实现一个功能并完成必要验证，版本增加 `0.0.1`，不要求创建 Release 或强制玩家验收；v0.5.0 仍是目标里程碑，累计超过该版本不人为进位 minor。历史 source `0.5.0` 的本地开发候选及其 ZIP 身份如上记录，v0.4.15 ZIP 保留为历史证据；已本地提交的 source 0.4.3–0.4.15 序列不改写这些产物。

1. [x] 现代化安装器以识别支持的多种目录结构、ISO、已解压目录和 XEX 输入；可选 MD5／SHA-256 身份识别，并美化界面但不削弱兼容性核对。源码与合成桌面流程已验证；冻结安装器／DPI 包覆盖仍待完成。已本地提交为 `b094a1a98be441e24b8a14f2c02a2fd60020629a`（source 0.4.16）。
2. [x] `game-path.txt` 为空时，在配置／根目录解析 `default.xex`，或在导入根目录下解析 `disc1`；并回退相邻的 `game/disc1`、`game`、EXE 和 EXE 父目录下的 `game`。实际 main 启动路径已到达 parser 并按预期提前退出。
3. [x] 基于原游戏图标改造 EXE 图标，并突出 recomp 身份。ICO 资源、resource 与安装器接线、PE 编译及实际 SDL 图标资源匹配已通过。
4. [x] EXE 无需 BAT／CMD 即可直接启动：以自身目录为稳定资源基准并保留显式路径优先。支持的真实路径形式为 `default.xex`、`disc1`、相邻 `game/disc1`、`game`、EXE 和 EXE 父目录下的 `game`；实际 main 启动路径检查已在 guest／GPU 执行前通过。
5. [x] 将首启配置独立为界面／模块并现代化美化，不需要额外 EXE。实际 `IFileOpenDialog` picker 覆盖超长路径和 Unicode 选择、取消、调用方 COM 状态保留及不兼容 apartment 错误。冻结安装器／DPI 覆盖、真实零售输入、玩家验收和发布另行保留。
6. [x] 通过顺序启动 bundle 和按身份键控的确定性失败缓存，修复启动时重复 shader 检测与编译。原安装目录的 v0.4.16 有 22,972 个有效已编译 binary 和两个反复被拒绝的 source，并非大规模重复编译。本地 source `0.4.17` runtime update 后，对同一原缓存进行了两次 D3D12 prepare-only 启动，均在 guest 启动前退出：迁移首轮 10.781 s 初始化并有 2 次实际 DXC 拒绝；后续 warm 启动 3.634 s 初始化，metadata snapshot 77 ms、bundle load 2,397 ms、22,972 个 ready module、2 个 cached failure、source 内容读取／translation／进程范围实际 DXC 均为 0，288 个 PSO 在 18 ms 内 ready。首轮仅新增 bundle 和两个 failure record；第二轮未改 cache entry，5 个受保护用户文件与既有 cache metadata 不变。build 与 `LoShaderStartupCacheTest` 通过。这是受限的 D3D12 preparation-only 证据：不表示修复两项 compiler error，也不证明 Vulkan、gameplay、其他平台、玩家验收或 Release。见[启动复发报告](../out/v0.5.0/shader-startup-recurrence/REPORT.md)。
7. [x] 核实启动 shader 准备在实际启动路径中使用逻辑线程数减一、最少一线程，并覆盖恢复行为。生产队列 fixture 及实际 Map12 的 16 logical／15 worker 启动采样已通过；更广场景验收另行保留。
8. [x] 现代化、轻量化 Debug Menu，同时保留诊断作用。窄 fixture 覆盖 child-focus 下 F1／Esc／Gamepad B 隐藏及 busy／failure 恢复；r5 Map2 实跑显示正确地图和坐标的真实 Debug 窗口，guest 菜单继续运行。更广诊断流程、玩家验收和发布另行保留。已本地提交为 `294df076e09c7a8b41c6b530a58ed59404ae24e0`（source 0.4.18）。
9. [x] 先评估以宿主设置菜单替换游戏内置设置的可行性，再选择实现方案。受限的宿主设置路径已实现并验证：重开后原亮度返回值不变。
10. [x] 设置菜单复刻原游戏风格，同时兼容首启所需的现代交互。Root 审阅实际 r5 Map2 替换 overlay：System → Settings、RB 两次至 Graphics 及 Display Mode 选择均通过；1280×720 和 1920×1200 fixture 通过。系统字体和程序纹理均为近似，玩家美术验收、更广交互覆盖和发布另行保留。
11. [x] 保存后需要重启的设置应提示立即／稍后重启，绝不强制立即重启。取消保持设置哈希不变；受限的“立即重启”交接已使父进程退出并让子进程回到 Map2。玩家视觉验收及正常子进程退出证据另行保留。
12. [x] 检查 GitHub 是否有较新的匹配 binary；存在时自动下载并更新：先支持 Windows，保留存档／设置／游戏数据，校验包，在进程退出后替换、失败回滚，离线不阻挡启动，并为未来 macOS／Linux AppImage 选包预留架构。core/helper/progress/main-entry 检查及 v0.4.15 开发 ZIP identity gate 已通过：47 个 manifest 文件、runtime/helper、三个 EXE 图标 payload、DXC 2407 pair、license、provenance、README 及不含游戏／源码／PDB／用户数据的 allowlist 均匹配。不声称公开 Release 或玩家验收；包检查未启动 payload，也未重做 transaction／GPU 验证。已本地提交为 `9c2dc761a4c775b3ffc8b01429f11bd42f0c4f94`（source 0.4.17）。
13. [ ] 游戏主窗口与呈现区域按所选实际像素分辨率确定尺寸，不随 Windows DPI 比例再次缩放。相同分辨率在 100%／125%／150%／200% 缩放下应保持相同像素尺寸；辅助设置、安装器、更新器及 Debug 窗口继续适配 DPI。本项仅记录需求，尚未实现或验证，不增加版本号。

## 已发布里程碑与剩余覆盖：v0.4.0

[v0.4.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.0) 已于 2026-09-07 21:56:34 UTC 从 `40362d78` 发布，发布 CI 和正式包检查通过。两个已核对版本均通过正式包的隔离 Map2 启动检查，Auto 1080p／TAA 及包内编译器依赖已核实。此为限定静态场景检查，不代表全游戏或新增玩家画质验收。 已包含 v0.3.0 标签之后的测试工具。详见[当前发布证据](STATUS.md)。

此前 v0.4.0 检查点完成了所记录的实现与限定验证：文字／UI、Debug、空间 AA／缩放、实验性 TAA、采样 60 FPS 行为及 DLSS/FSR 可行性研究。更多流程列为回归，无新增用户验收。见[实现检查点与验收标准](notes/v0.4.0-development.md)。相关实现已包含在 v0.4.0 中，下方验证和验收边界继续保留。

1. [x] 通过按输出分辨率绘制宿主设置页及支持场景的 UI 前 AA 改善文字清晰度。同一真实 Navigation 帧重放、生产 GPU 控制和 v7 最终 AA 跳过记录，验证后续 UI 移除了重复 AA；实际七行图形页布局也已通过。该 v7 构建的 guest UI 仍为 720p；不支持场景／CPU 回退及更多字形／语言质量列为独立回归覆盖。
2. [x] 增加独立英文／简体中文 Debug menu：fixture、启动及实际 Map2 教学状态 EN → SC → EN 检查通过，包含动态地图／无战斗／等待可控角色文本和语言恢复。Capture render state 保持最上方；其他战斗／状态／布局变体列为回归，不宣称新增用户验收。
3. [x] 增加 SMAA 1x：上游 HIGH 三阶段／LUT 通过选定 GPU 及实际 UI 前处理检查。实际图形页选择、Keep 和同进程重开通过，包含从 SMAA 切换至 TAA；更多参考／场景对照列为回归。
4. [x] 增加实验性相机重投影 TAA：正常 AA3 设置、场景深度、jitter、稳定网格历史／重置及最终 AA 跳过通过 v7 有界静止／移动验证，实际菜单选择／Keep／重开也已通过。不支持路径使用 SMAA；原生对象运动保留为后续工作，更广场景质量列为回归覆盖。
5. [x] 自动匹配实际输出并提供标准／双线性与高／双三次质量：选定 GPU／配置及真实帧重放检查通过。实际七行图形页验证了预览不写盘、15 秒恢复 High、Keep 保存 Standard 及同进程关闭／重开。该 v7 空间滤波改动保持 guest 尺寸固定；真正的内部分辨率现已列入下方明确需求，进程重启／显示覆盖继续列为回归。
6. [x] 实现 60 FPS 并完成限定正确速度验证：采样移动／对白、正常 Apply／Keep／重开及 59.79827 FPS 静止营地通过。自然 30／60 战斗样本也显示 Ring 时间／进度及长按超时序列一致，观察到的 1.399／1.415 秒阶段边界差异在采样粒度内。精准释放／Perfect、伤害和更广流程列为回归；不宣称全游戏锁 60，未验证的可选 120 候选可延期。
7. [x] 完成 DLSS/FSR 可行性研究：官方契约、实际场景／深度／相机／jitter 证据、API／硬件／分发风险及分阶段实施路线已记录。原生对象／骨骼运动、实测曝光／色彩空间语义、更广场景覆盖及供应商后端保留为后续工程。

用户反馈已有闭源实现能在更高帧率保持正确游戏速度，目前未提供名称和具体帧率覆盖，作为可行性线索保留。其他实现超过 60 FPS 的限制不代表 120 FPS 不可行，但用户最新反馈允许在 120 FPS 困难时仅交付 60 FPS。既有阴影调查和无关待办维持原状态，不纳入本次新增范围。

### 2026-09-07 新增后续需求

用户反馈所有场景均看不出 upscale 改善，此前也反馈切换 TAA 看不出差别，并明确要求真正的内部分辨率最高到 4K，优先跟随输出。既有执行链检查在对应构建范围内继续有效；新构建的可感知改善仍待用户复查。记录中的开发构建以 `c548b48` 为基础并包含后续改动；后续实现已包含在 v0.4.0 中。见[后续交接](notes/handoff-v0.4.0-followup.md)。

1. [x] 亚洲／美欧裸 FPD 与 CPX 联合元数据按导入资源身份自动匹配，无区域选项。两版各 52 个资源文件命中索引、CPX 零回退，20,686 个来源名称／哈希与保留的 strict 完整扫描一致；从 Disc 2 进入可复用同组兄弟盘清单。旧 profile、未知／提取失败回退、未读内容边界及显式完整扫描保持；fixture 与主构建通过。读取量分别为 142,093,208／142,289,816 字节，观测 12.537／12.631 秒不作为受控提速、启动或 FPS 结论。新包通过两版 Map2 Auto 1080p 检查，包内依赖与来源哈希已核实，用户数据保持；原报告战斗／全游戏验收不在此范围。旧单版计时保留为[历史证据](notes/shader-preparation.md)。
2. [x] 增加最高 3840×2160 的真实内部分辨率 Auto 跟随输出及手动 720p／1080p／1440p／2160p，与输出和空间滤波质量独立。八行五语言设置实现通过 212 项 CPU／菜单检查和 10 张 GDI 图；37 项尺寸检查及 16 个翻译 shader 的 DXC 编译通过。GPU 采样与非法分配后恢复检查通过。固定 1080p 输出／AA3 的 Map2 静止／移动实测验证原生 720p、Auto 1080p 和 4K 来源／深度／TAA 输入及部分记录的历史复用，没有 CPU／分配回退；已检查的细杆和地表细节改善。build-v3 的 Auto 4K 输出、中文八行图形页、预览不写盘、15.017 秒回退、1440p Keep、实际 1440p 场景返回及同进程重开通过。此前 v3 Auto 4K 窗口复用为 108／256 条，已定位的相机参考点错误由下项处理；新版本用户画质验收及更广场景／硬件仍待验证。
3. [x] 修复 Issue #5 缺失的间接调用入口 `0x82AFA388` 及同类 `0x82AFD150`。四盘边界审计、重新生成、整合构建及 56 项真实生成 dispatch／分支／script cursor 检查通过。[报告者随后提供 USA/Europe 存档并确认同一遇敌已通过](https://github.com/freefrank/LostOdysseyRecomp/issues/5#issuecomment-5576195692)，Issue 已关闭；其他遇敌和版本继续回归。
4. [x] 增加 Issue #6 启动分配失败诊断：保留原始 OS 错误、失败操作、映射和内存上下文。186 项注入检查及真实 alias 分配／释放通过，没有改变映射合同。[维护者以 v0.4.2 修复说明关闭 Issue](https://github.com/freefrank/LostOdysseyRecomp/issues/6#issuecomment-5580296598)；原机器恢复仍待确认，附件日志不证明和 #5 同根因。

5. [x] 修正已定位的 TAA 相机历史误拒绝：固定 .001 参考点可能落在投影无穷远边界，VP 不变也会失败。正 W 区间选点保留像素重投影、有效性阈值、四分之一屏幕限制及其他历史 guard；默认 CPU 共 141 项，512 对保留记录重放共 3,215 项通过，并复现全部 158 次旧拒绝。整合构建及时域 GPU fixture 通过；原生 720p 和 Auto 4K Map2 实测各 256／256 次历史复用，包含输入释放后 140／157 帧，无拒绝 gate 或 jitter misses，source／TAA／depth 尺寸匹配。相同 24 active engine-tick 输入的终点不同，不作为相同轨迹或 FPS 对照；更广 TAA 运动画质和用户视觉验收另行保留。

## 已发布里程碑：v0.3.0

- [x] CPX/FPI 发现、校验后的 XEX 来源、有限 VS 变体及已记录管线预创建已实现并完成本地验证；见 [shader 证据](notes/shader-preparation.md)。
- [x] [v0.3.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.3.0) 已于 UTC 2026-09-07 06:55:19 从未改动标签 `fba7ae4` 正式发布；发布 CI 34091301175、正式包／安装器及短时首战验证通过。不隐含新增玩家画面／卡顿验收，标签后的测试工具仍为 Unreleased。

## 历史里程碑：v0.2.2

[v0.2.2 已正式发布](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2)，包含已验收 AMD resolve 修复及 Unicode 启动／存档路径修复。CI、正式包校验、8 项启动路径及中文工作目录 Map 12 隔离验证通过。Issue #4 完整游戏崩溃仍未复现；见[发布说明](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2)。

## 历史里程碑：v0.2.1

[v0.2.1 已发布](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.1)，增加 F1 下一帧渲染捕获及自动 ZIP、SDL 多手柄与键盘混合输入及 E/R 扳机。托管 CI（含 LoHidTest）、正式包哈希、安装器及隔离渲染／捕获验证均通过；实体手柄和游戏中切换仍待覆盖。这不是 AMD 渲染修复。见[捕获](notes/render-state-capture.md)和[输入](notes/controller-input.md)。

## 历史里程碑：v0.2

v0.2 已发布，包含两套已核对版本、按版本提供的文本／语音选项、先导入后设置及自动读取已导入盘。对应 [Redump 11817：USA, Europe，0.0.0.3](https://redump.info/disc/11817) 与 [Redump 39111：Europe, Asia，0.0.0.4](https://redump.info/disc/39111)。导入仍严格核对 XEX 哈希，不代表完成整张 ISO 的 Redump 哈希比对。两版原管理器均通过 1 → 2 → 3 → 4 → 1 受控流程，章节交界剧情仍未覆盖。见 [v0.2 发布说明](RELEASE-v0.2.md)和[自动选盘](notes/disc-selection.md)。

v0.1 保留为首个 Windows 发布里程碑，包含导入器、首次设置、五语言界面、着色器索引／并行准备及对白、脚下投影、海报和窗口响应修复。见 [v0.1 发布说明](RELEASE-v0.1.md)。

**语音问题已解决，用户已确认验收。** XMA 续接包处理现在保留新帧，不改变采样率或音量。装甲车对白从 2,129 帧恢复到 4,062 帧，与原始音轨的时间斜率从 0.547 恢复为 1.000；另外 34 个多声道缓冲、4,264 帧解码无错误。其他场景的音频属于常规回归覆盖，不再将已解决的对白问题列为待修复。见[音频证据](notes/audio-output.md)。

## 近期优先事项

以下保留既有兼容性和回归待办；上方里程碑区分已发布工作与后续开发。

1. [ ] 凯姆首战身体阴影调查在按要求复查 v0.4.0 并完成最后一分钟核对后，于 2026-09-07 继续挂起。Radeon 8060S 上的正式包测试未确认复现 RX 9060 XT 报告，尚无根因或修复；恢复调查需要报告者的视频、日志、设置及正常／异常捕获。Map 13 仍未解决，两项报告的关联未知。见[证据与边界](notes/kaim-body-shadow-v040.md)。
2. [ ] 排查攻击动画卡顿，采集帧耗时、着色器编译日志并做冷／暖缓存对照。用户回复将其归因于着色器编译，但尚无采样证据确认原因。
3. [~] 研究文字模糊或乱码，与原机和 Xenia 对照，进行中；这与已暂停的文本语言互补补丁是不同任务。
4. [ ] 回归喷火阶段阴影闪烁。同一玩家反馈该现象似乎消失，仅算单次观察，不宣称全面修复；火焰受击格子仍是单独开放的问题。
5. [x] **v0.5.0 — 游戏／DLC 自动导入已实现：**已本地提交的 source [`0.4.15`](https://github.com/freefrank/LostOdysseyRecomp/commit/f77d943261e14105d17c880ffa40c171b4191dcc) 批次可将失落的奥德赛 CON/LIVE/PIRS STFS 包导入共享 DLC 目录，支持事务解包、重复／冲突保护及运行时枚举／打开／读取；它移除了手动 DLC／Game Discs 模式选择，对选取的文件、目录和混合来源自动分类、统一复核并导入。15 项直接受影响检查首次全部通过（13 项新增识别／扫描／复核／分派检查，2 项既有 GUI 检查复用）；未改动的 DLC parser、交易和运行时链路保留 22 项 Python 与两个无窗口原生模式证据。真实 DLC 奖励、区域、版本兼容性与用户验收仍待完成。该批次已本地提交，本地包未发布。保留的 v0.5.0 与 0.5.1 ZIP 是历史产物，不限制本里程碑继续纳入功能。
6. [ ] 增加独立 shader 日志与 TAA 覆盖审计：分离 shader 诊断和 runtime 摘要，保留最近三组会话日志及按版本累计的覆盖清单，并让 F1 导出包含 shader 日志快照。发现、整理未覆盖路径无需每次导出；未知路径只记录，闪烁修复仍需同场景验证。此方案尚未实现，见[日志与覆盖 TODO](notes/taa-coverage-audit.md#shader-log-coverage-todo)。
7. [ ] 分析 [Issue #9](https://github.com/freefrank/LostOdysseyRecomp/issues/9)：v0.4.2、Ryzen 9950X3D / RTX 5090 上报告 CPU 温度高和 TAA 纹理闪烁。两个症状均未验证，不假定同一根因，也不直接合并到此前的敌人消失报告。
8. [x] 在固定的 v0.4.15 D3D12 Map2 配置中定位 Windows 稳态 CPU 热点（Ryzen 7 9800X3D／RTX 5080、60 FPS、SMAA、720p internal、配置 1080p output）。45.0001416 秒内进程使用 110.875 CPU 秒（整机 CPU 15.3993%；2.46388 个逻辑核当量）；112,344 个 on-CPU 样本无丢失事件、20 个缺栈。承载 GPU query 轮询和 guest 共享值零超时轮询的线程分别占进程 CPU 计时的 40.389% 与 39.994%（合计 80.3833%）；两条实际路径分别占进程 on-CPU 样本 36.5618% 与 33.0948%（合计 69.6566%），路径份额是样本分布而非 CPU 计时归因。GPU WorkerMain 占进程 CPU 计时 16.164%，其中 `getenv_nolock` 为该线程 exclusive 样本的 24.6787%。该结果只隔离单场景 D3D12 路径，不诊断温度、不证明后端回归或全游戏行为。GPU query 等待策略、零超时轮询及每 draw 环境变量查询是在 profiling 阶段提出的候选；后续实现见下一条。见[热点报告](../out/v0.5.0/cpu-hotspots/report/HOTSPOTS.html)和[CPU 对比](../out/v0.5.0/cpu-comparison/report/REPORT.html)。
9. [~] 由该 profile 实施一批 CPU 效率修复：仅在已测 GPU query 和共享值轮询路径使用有界等待，并将四个禁用 capture 的环境值移出每 draw 热路径缓存。源码 `0.4.16` 构建成功，新的 `LoPollWaitTest` 通过。有效的新 D3D12 Map2 对比未重跑 v0.4.15：60.0003575 秒内均值为 4.3033598%／0.6885376 个逻辑核当量，保留的 v0.4.15 为 15.6200169%／2.4992027；3,599 个 PresentMon 事件在单 swapchain 上完整覆盖 CPU 窗口，采样后受控输入仍留在 Map2。该结果是整个批次的单次结果，不归因于单项改动，也不代表全游戏。首次 Vulkan CPU 窗口与 PresentMon 零重叠而排除；替代采集在开始前被用户物理 Escape 停止。Vulkan 对比、玩家验收及发布仍待完成。见[优化报告](../out/v0.5.0/cpu-optimization/REPORT.md)。

原有事项继续排队：偶发 GPU query/wait 故障及长时间稳定性；火焰受击格子／箱子特效和两个资源着色器失败；两版章节交界、存档读回、遇敌和多语言回归；全屏／独占、鼠标及跨 DPI 验收。Map 13 身体／环境阴影明暗闪烁仍开放；已验收的脚下投影修复继续常规回归。

## v0.2.2 交付状态 — 本地 2026-09-06

- [x] AMD resolve 修复 `43ce0e53` 已获用户验收，并以 `ab0d038` 合并；NVIDIA RTX 5080 定向检查及人工验收通过。
- [x] Unicode 启动／存档路径修复 `7dbb668` 已以 `04dd7d0` 合并；8 项启动路径及 8 组存储测试通过。Issue #4 完整游戏崩溃仍未复现，路径测试不代表游戏内验收。
- [x] `v0.2.2` 已于 UTC 2026-09-07 03:11:10 正式发布，标签 `f03efe370d444db1a8a9c1213c240da697f58504` 已双推，CI `34077788392` 全通过。正式包 CRC／44 项 manifest、安装器自测、8 项启动路径及中文工作目录 Map 12 验证通过。

本次要求的抗锯齿、缩放和帧率工作已列入 v0.4.0。**用户于 2026-09-06 暂停文本语言互补补丁研究**：无成品、无运行时代码改动、不在 v0.2 中；语音不在该研究范围。见[挂起研究](notes/text-language-patch.md)。

## 阶段 0：准备

- [x] 建立仓库骨架和子模块。
- [x] 使用 `tools/god_extract.py` 提取四盘，存入不跟踪的 `LostOdysseyRecompLib/private/disc1..4`。
- [x] 确认支持的 XEX 为 Europe, Asia 版本 4 和 USA, Europe 版本 3；TU 不在已核对集合内；按可执行文件详情和哈希匹配，不仅依赖区域标签。见 [XEX](notes/xex.md)。
- [x] XenonAnalyse 生成初版 841 张跳转表；安装 Ghidra 12.1.3 和 XEXLoaderWV，无界面导入 `default.xex`。
- [x] Xenia Canary 实跑开场战斗，保存相同机位对照。见[对比记录](notes/xenia-render-comparison.md)。

## 阶段 1：产出可编译代码

- [x] 填写 TOML：保存／恢复地址、无效指令、80 条显式函数边界及 setjmp/longjmp。
- [x] XenonRecomp 第 15 轮生成零错误、零未实现指令；补丁补齐 30 条指令。
- [x] 251 个生成的 C++ 文件使用 clang-cl 22、`/O2` 编译通过。MSVC 不作为受支持的编译器。
- [ ] 完成全部 XenosRecomp 着色器翻译并记录不支持的指令；仍有两个资源着色器预编译失败。

## 阶段 2：进入主菜单

- [x] 修正小纹理 level-0 packed-mip 偏移，恢复动态标题背景。见[记录](notes/title-packed-mips.md)。
- [x] 实现线程、同步、内存、文件系统和 XAM 用户档案 HLE；标题循环实跑通过，60 fps swap 不代表游戏逻辑已解锁帧率。
- [x] 接通 plume/D3D12，正确显示 Press START。见 [GPU](notes/gpu.md)。
- [x] 接通 Xenia FFmpeg XMAFRAMES 与 SDL 48 kHz 双声道音频；修复循环终点、游标归属、文件读取竞争、命令寄存器和对白跳帧。共享句柄测试从 8,000 次读取中 932 次错误降为零，连续 32 次 MMIO kick/clear 通过；用户确认语音问题解决。其他场景继续常规回归，开场 WMV 播放单独待修。见[音频](notes/audio-output.md)。
- [x] SDL 手柄输入及键盘回退。
- [ ] 避免输入法干扰游戏按键：游戏控制期间关闭 SDL 文本输入／IME，仅在真正文本输入场景启用，不改变用户全局输入法。验证中文输入法下 Z/X/A/S 和 F1、焦点切换及键盘／手柄切换。用户已确认另一台电脑切英文输入法后恢复操作；v0.2.1 尚未修复。见[输入记录](notes/controller-input.md)。
- [x] 开发期间默认关闭震动，`LO_CONTROLLER_RUMBLE=1` 可启用。
- [x] 通过登录、存储检查，进入标题、主菜单和设置。

## 阶段 3：推进完整通关

- [~] 后续遇敌：修复开场资源遗留导致的主角 T 姿势和攻击停滞；独立验证普通攻击、反击和自然胜利，敌人姿态及闪烁待查。见[遇敌动画](notes/encounter-animation.md)。
- [x] Windows debug 判胜调用原游戏胜利阶段和结果初始化，用户确认可跳过战斗。
- [ ] 火焰受击异常需原机参考；Xenia 同样异常，不能单独作为目标。
- [ ] Debug menu 增加可恢复、不写入存档的主角攻击力／伤害调整。见[需求](debug-menu-requirements.md)。
- [~] 修复异步存档完成、缩略图 ABI、持久化枚举、NT 写入和 CREATE_ALWAYS；手动保存、覆盖及独立进程读回通过，完整兼容性待验。见[存储](notes/save-storage.md)。
- [x] 自动读取已导入盘并重载原索引；两版存储路径和原管理器换盘测试均通过。见[证据](notes/disc-selection.md)。
- [ ] 验证两版真实章节交界剧情及之后存档读回；受控管理器测试不代表完整推进。
- [ ] 逐一验证过场、战斗、千年之梦和大地图。
- [~] 独立存档按攻略推进：Hypocenter 残骸／Ram、戒指教学、自然胜利、早期 Wasteland 遇敌、Gorge 营地、装甲车剧情和城门控制已有局部证据；用户已到 Map 13，尚未通关。见[攻略测试](notes/walkthrough-testing.md)与[戒指](notes/battle-ring-resource.md)。
- [x] 修正物理地址别名，恢复开场人物材质：A/C 共享，E 偏移一页；Windows/Linux 别名测试及场景验证通过。见[记录](notes/physical-alias-rendering.md)。
- [x] 修正 16 位索引对齐、resolve 尺寸和纹理解码，解决开场破面、后续轮廓偏移及 gamma 缺失；数值测试和场景对照通过。见[记录](notes/rendering-index-and-resolve.md)。
- [x] 接通 stencil、修复 D3D12 参考值和 D24 清理，恢复高光。见[记录](notes/lighting-stencil-depth-clear.md)。
- [x] 绘制前转换 EDRAM，修复首战后白屏，恢复场景和炮口火焰，进入重型坦克战斗。见[记录](notes/post-battle-whiteout.md)。
- [ ] 修复其余崩溃并完整通关；此前 RPBattle__Scene memset 截断崩溃已修复。见[重编译](notes/recomp.md)与[交接](notes/handoff.md)。
- [x] 同地图传送、坐标记录／返回和轴向调整；原生传送、恢复行走及游戏菜单中拒绝操作已测。见[传送](notes/debug-teleport.md)。
- [x] 自动枚举当前地图存档、出口、机关和拾取 POI；Hypocenter 落点、出口附近及旧编号拒绝已测。见[传送](notes/debug-teleport.md)。

## 当前反馈与回归

- [x] 脚下投影：修复 mode-5 stencil 绘制残留像素着色器；营地移动和 GPU 测试通过，用户确认基本修复；继续跨地图／遇敌回归。见[阴影](notes/shadow-texture-lod.md)。
- [x] Map 12 海报黑斑：polygon-offset 修复通过 300 帧 A/B 和浅深度遮挡测试，其他地图及近裁剪面继续回归。见[海报](notes/map12-poster-depth.md)。
- [ ] Map 13 身体／环境阴影明暗闪烁按后续用户反馈仍开放；已验收脚下投影属于另一项修复，尚未证明与海报同因。
- [ ] 火焰受击黑红格子，以原机画面为参考。
- [x] Ring 外环：测试中正常变化，释放 RT 获得 Good 和 101 伤害，用户确认手柄操作正常。见[记录](notes/battle-ring-resource.md)。
- [ ] 第二地图箱子破坏特效黑色。
- [x] Debug 地图 ID／本地化名称：标题未知状态、Hypocenter/Gorge 和 2→3 切图已测。见[地图](notes/debug-map-info.md)。
- [~] 随时存档：非存档点槽 03 保存／重启读回、权限恢复、原生存档点和营地切换通过；F1 开关有隔离尺寸／回调测试，其他桌面和游戏流程待验。见[记录](notes/save-anywhere.md)。
- [~] 营地／窗口无响应：大小端临界区归属和递归修复通过四线程及选定流程测试，窗口消息泵修改已发布；独立 GPU query/wait 指针损坏仍待修。见[临界区](notes/critical-section-endian.md)与 [GPU 等待](notes/third-map-hang.md)。
- [x] 语音问题已解决并经用户确认，输出及对白时序修复已发布；营地→装甲车→城门流程不再出现此前稳定解码错误。其他场景音频属于常规回归，长时间运行稳定性单独跟踪。见[音频](notes/audio-output.md)。

## 阶段 4：现代化

兼容性和稳定性仍是验收要求。SMAA/TAA、按分辨率缩放与速度正确、稳定的 60 FPS 已列入 v0.4.0 目标；120 FPS 为可延期的可选目标。DLSS/FSR 输入可行性列为研究；下方其他功能继续后续评估，均无承诺发布日期。

- [x] 按逻辑线程数减一、最少一线程并行预编译；测试机 15 worker 将 2,000 个着色器准备时间从 53.4 秒降至 6.7 秒，成功产物逐字节一致。
- [x] 已知着色器启动准备、持久缓存和进度界面；早期 184 微码集合的热缓存复用和损坏 DXIL 恢复通过。见[着色器准备](notes/shader-preparation.md)。
- [x] 四盘发现 2,000 个微码，1,998 个编译成功；进度、缓存复用和独立 Map 12 实跑通过。
- [x] 内置 52 文件索引，未知布局保留扫描回退；本地发现阶段从 36.2 秒降至 1.1 秒，此耗时与编译耗时分开计算。
- [x] v0.3.0 已发布扩展 CPX/XEX 发现、有限 VS 变体和已记录管线预创建；在已记录场景范围确认实际 draw 复用。
- [ ] 修复两个微码编译失败，补充其余来源／变体覆盖；尚未实现仅凭资源准备全部首用 PSO，新场景仍可能首次卡顿。
- [x] 保留原选项并替换设置，增加五语言界面、语言选择、FXAA 和等比例输出缩放。见[设置](notes/settings-menu.md)。
- [ ] 完成全屏／独占、鼠标和混合 DPI 桌面验收。
- [x] SMAA 1x 和实验性相机重投影 TAA 已实现，并在上方所述有界实机及菜单选择／Keep／重开范围通过验证；更广 TAA 画质列为回归覆盖。
- [x] 为 v0.4.0 增加自动输出分辨率匹配和标准／高质量：选定检查及实际七行图形页预览／回退／Keep／同进程重开通过；后续已实现最高 4K 的真实 Auto／手动内部分辨率，并完成上方记录的限定验证。
- [ ] 宽屏／FOV 变化及正确 UI 布局作为后续工作。
- [x] 完成 v0.4.0 的 DLSS/FSR 时域输入可行性研究，涵盖实际证据、缺失输入及未来接入门槛。尚无供应商后端，v0.4.0 设置页已移除 v0.3.0 的 DLSS 禁用占位。
- [ ] 帧生成 FG 作为后续研究；v0.4.0 设置页已移除旧发布版本中的禁用控件。
- [x] 实现 60 FPS，并完成 v0.4.0 的限定移动／对白、UI／营地及 Ring 核心计时／超时检查；精准 Ring 释放和更广流程列为回归，不宣称全游戏锁 60 或可选 120 候选已验证。
- [ ] HDR 输出和色调映射。
- [ ] 更高分辨率阴影。
- [ ] 研究 SSAO，为 ReShade 提供干净深度缓冲。
- [ ] 完成 [v0.5.0 PC Vulkan 里程碑](#v050-pc-graphics)；DX11、Linux／Steam Deck 及其验收均为未来工作。
- [x] 托管 Windows CI 发布 v0.1，包含导入器和首次运行设置。见[安装](INSTALLING.md)与[打包](notes/release-packaging.md)。

## 阶段 5：可选探索

- [ ] 屏幕空间 GI / SSR。
- [ ] 硬件光追阴影和反射。
- [ ] 高清贴图替换与 mod 加载器。
