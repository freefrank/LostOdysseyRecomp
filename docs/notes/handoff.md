# 接手入口

## 2026-09-07（本地）taa-fix 开发分支接续：敌人消散仍待修复

用户要求将本次 TAA 改动提交到 `taa-fix` 分支；当前仍为本地 `0.4.2-dev`，未发布，玩家验收待完成。生产范围保持六条位置路径、draw 诊断及 17,287 项检查，没有新增敌人消散修复。四盘扫描的可跟踪摘要与后续覆盖方案见 [TAA 覆盖审查](taa-coverage-audit.md)；运行链接来源与 draw 覆盖统计仅为未实现设计，408 个归一化组不能直接当语义白名单。

用户已自行安装 `fe39a9c9…`；f5446–5448 新导出完整，54 份 raw resolve 检查通过，审查时无游戏进程。固定曝光下敌人胸甲、肩甲／武器黑面在最早颜色 resolve 已出现。新增 `4bd8985d84983b83` 以 c230 输出位置，在每帧 draw10–13／30–33 深度预处理漏覆盖，并与已支持本体层配对；它属于盘内 `adc97a079302f52e` 的新运行链接结果。消散 clip 阈值与本体同步，深度／材质投影不一致为优先候选。F1 每帧停顿 0.706–0.900 秒，尚无实际上传、正常时序 32 相位或 Off 证据，不能当成已证明的视觉根因。见[新诊断](shadow-texture-lod.md#enemy-death-f5446)与本地 `out/enemy-death-f5446/`。

下节“原安装七文件未变”只描述此前交付时的保护检查；当前 EXE 已由用户替换。山体有限实跑通过、原轮胎已验收及两个 AMD 报告分别挂起的状态不变。

## 2026-09-07（本地）战斗 TAA 实跑通过，玩家验收待完成

用户已授权修复。本地 `0.4.2-dev` 补齐六条已核对的战斗位置路径，保留全部相机／viewport／深度限制及 jitter 算法。17,287 项 CPU 检查通过，覆盖 32 相位、四种分辨率、三层位置负对照、蒙皮和原轮胎／d55 用例。新增 draw log 可跟随 resolve trace，`log_slot` 不授权 jitter。见[实现与边界](shadow-texture-lod.md#battle-taa-fix-dev)和 `out/battle-taa-fix/LoTemporalJitterTest.log`。

隔离 v0.4.1 源码候选排除 Issue #7 改动，构建及六项 CPU／GPU 用例通过；开发 ZIP `0bb58914…`／EXE `fe39a9c9…` 的 45 项哈希、归档和 importer 检查通过。已在 Map3 随机遇敌复现原山体，基线／修正版／Off 各覆盖 32 相位：基线 16 帧至少半区变黑，修正版和 Off 均为零；修正版三层 VP 32/32 一致，Off 224 条常量不变。三组观测时间戳均小于 250 ms，但 temporal summary 未覆盖采样帧，history reuse／gap 标志未知；跨进程相机动画不同。详见[实跑证据](shadow-texture-lod.md#battle-taa-runtime-dev)与 `out/battle-taa-fix/runtime-comparison.md`。

实际包 Map3 轮胎回归也已通过：352 条上传、64 组三层 VP 和 32 条 temporal summary 均通过；近远轮胎的 mask／depth 四组 ROI 与已验收 r2 按 32 相位逐字节一致，粒子影响的 source／TAA 颜色不作字节等同。当时七个原安装文件哈希未变，全部自有游戏进程已结束；该交付检查点尚无玩家验收、提交、推送或发布。本地入口与完整证据见 `out/battle-taa-fix/REPORT.md`。下节保留先前只诊断时的状态；原轮胎已验收和两个旧 AMD 报告分别挂起的结论保持。

四盘静态审查另已完成：22,935 份 HLSL（2,859 VS／20,076 PS）、408 个位置程序组；七个 observed VS、19 个阴影 PS 及平面投影／blur／fog 路径仍为候选。它们未加入本包修复，也不是已确认可见缺陷或全游戏验收。详见[扫描范围与边界](shadow-texture-lod.md#taa-four-disc-audit-20260907)及 `out/taa-whole-game-scan/REPORT.md`。

## 2026-09-07（本地）v0.4.1 新战斗地形闪烁

正式 v0.4.1 EXE `9e0e13d9…`、RTX 5080／2560×1440／AA3 的战斗地形出现大片黑亮切换。原 2871–2873 和现场补充 26786–26788 两组三帧导出完整；异常已存在于最早颜色 resolve，早于阴影遮罩与最终 TAA。已确认同几何深度层支持 jitter、材质／补光层漏覆盖，这是强候选，尚无实际上传常量、修正版 A/B 或 Off 对照。详见[战斗诊断](shadow-texture-lod.md#battle-terrain-flicker-v041)，本地证据为 `out/battle-flicker-20260907-f2871/`。

接续重点是同场景实际上传和颜色输出对照，保留现有深度／viewport 限制。捕获停顿超过 250 ms 会影响后续 jitter 条件，首帧坏、后两帧好不能作为无扰动时序对照。本轮只诊断和捕获，未改运行时代码、构建、更改 AA、替换 EXE 或提交发布；用户游戏保持运行。新 RTX 战斗缺陷待修复，原 Map3 轮胎验收保持已解决，两个旧 AMD 报告继续分别挂起，未证明同因。

## 2026-09-07 v0.5.0 Vulkan 与 DX11 TODO

用户确定下一主版本 v0.5.0 开发 Windows PC Vulkan 与 Direct3D 11 支持，已写入[中文路线图](../ROADMAP.zh-CN.md#v050-pc-graphics)／[English roadmap](../ROADMAP.md#v050-pc-graphics)，并补充[后端交接与 DX11 独立拆分](switch-vulkan-handoff.md)。本次仅记录待办，后端实现、实际包验证和玩家验收均未完成。先择取 PC Vulkan 可复用改动、保留 D3D12 基线，再独立推进 DX11；Switch rebase 及 Linux／Steam Deck 仍需各自的平台验收。下方合并数字和旧版本状态保留为历史记录，开发时重查；当前发布状态以 [STATUS](../STATUS.md) 为准。

## 2026-09-07 Switch 移植与 PC Vulkan 交接

朋友基于 v0.1（`2d9ce9f`）的 Switch 移植快照位于 `../LostOdysseyRecomp-main`，已评估：可编译 NRO 但默认为探针模式，不可玩；与当前 main 合并干跑有 9 个文件冲突。决定 Switch 代码暂不入主库，先在主库实现 PC Vulkan 后端，再让 Switch 分支 rebase。分叉证据、冲突清单、可复用部分和 Vulkan 工作拆分见[Switch 移植评估与 PC Vulkan 后端交接](switch-vulkan-handoff.md)。本条不改变下方既有交接的其他结论。

## 2026-09-06 当前交接

[v0.2.1 已正式发布](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.1)，包含 F1 渲染捕获自动 ZIP 与多手柄／键盘 E/R 输入；托管 CI、正式包验证及隔离启动／捕获均通过。参见[捕获](render-state-capture.md)与[输入](controller-input.md)，不代表 AMD 缺陷已修复；文本补丁仍暂停。

用户最终决定再次关闭本仓库 Gitea Actions；API PATCH `has_actions=false` 后 GET 已验证为 false。`git ls-remote origin main` 返回 `98b8fcc`，代码镜像继续保留，CI 与正式发布使用 GitHub。此前 `win-t640` 的 `windows-2022:host` 标签已修复并恢复接单，但 Release 16 与 test 17／18 均在 `setup-python@v5` 安装 Python 3.12.10 时失败，尚未进入 C++ 编译。T640 的 `Setup_20260906134712_Failed.txt` 记录 `0x80004005`，提示无法打开 engine process path 的句柄及初始化 engine section/state。本轮不再修复 Python 环境，不改变全局 runner 或其他仓库。GitHub [正式 CI 34053765472](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34053765472) 已全部成功，包括 LoHidTest；v0.2.1 正式发布不受影响。

最新公开版本为 v0.2.1，标签指向 `906d7c039f7e709c57af2c5278a43e259bfba8e5`，两远端标签均已确认。正式 ZIP 为 38,203,314 字节，SHA256 为 `cd583a6a28b47a1b2e7e31984052cd4eb6c953f5df333198f41540114eb4afb3`；44 项 manifest 哈希、安装器自测通过。冷缓存隔离运行 54 秒，德文标题菜单目视正常；frame 400 的 ZIP 共 60 项，每项 CRC／SHA256 校验通过，压缩后恢复约 30 fps。证据见 `out/release-v0.2.1/package-validation.json` 和 `out/release-v0.2.1/smoke/runtime.log`。此前 v0.2 的代码标签 `dcc9462`、发布记录提交 `b572d0a` 保留为历史。当前进度与近期 TODO 以 [STATUS](../STATUS.md) 和[中文路线图](../ROADMAP.zh-CN.md)为准；以下 2026-09-05 的构建名、PID、授权与实验安排保留作历史记录，不是新的执行指令。

v0.2 新增 USA/Europe `0.0.0.3` 兼容，保留 Europe/Asia `0.0.0.4`，对应用户确认的 [Redump #11817](https://redump.info/disc/11817) 与 [#39111](https://redump.info/disc/39111)。两版已通过自动选盘受控流程，章节交界剧情和完整通关仍待验证。

**跨版本文本补充补丁已由用户挂起。** 当前只有只读清单解析和资源定位，无可用补丁、无运行时或游戏数据修改。不要自动恢复；[研究记录](text-language-patch.md)保存已知结论。语音补充不在范围内。

发布状态文档已同步；README 已以两条 Redump 记录澄清名称。既有依赖修改继续由 tracked patches 管理，勿更改子模块指针。主开发程序、原始游戏数据和存档保持原状。

## 2026-09-05 历史交接

## 对白倍速修复（2026-09-05 最新结果）

用户已确认原始对白和修复后的游戏录音听感正常。XMA 跨包帧结束时曾额外跳过续接包中的新帧，导致对白被截短；现已保留这些帧，不改变采样率或音量。同段装甲车对白从 2129 帧恢复到完整 4062 帧，游戏录音相对原始音轨的时间斜率从 0.547 恢复为 1.000。另有 34 个多声道缓冲、4264 帧解码零错误。

本地验证构建 `135DCA79` 已安装，存档和配置文件校验未变。验证范围为该过场与上述音频样本；其他对白、战斗声音和循环子帧边界仍需覆盖。详细证据见 [音频记录](audio-output.md)。下方较早实验的未完成状态按各自日期理解。

先读[成果与交接报告](../WORK_REPORT_2026-09-05.md)、[状态总表](../STATUS.md)、[路线图](../ROADMAP.md)，再读对应专项证据。报告覆盖当前代码状态；日期化实验中的“下一步”和PID只代表当时现场。

## 当前优先事项

当前安装版本为 **135DCA79**，包含对白跨包修正、首次资源扫描与按逻辑线程数减一并行预编译、进度离屏绘制、窗口独立事件线程、深度清除分批、地面投影及Map12海报修正。Map13当前效果用户接受，火焰/箱子效果、PSO预热、两项shader翻译失败和独立GPU query/wait异常仍开放。旧条目中的PID和“待安装”仅表示当时状态。

较早本地构建 **14F09F15** 修正 mode5仅深度绘制误用前次像素着色器：旧PS导出深度，破坏地面投影模板。9693/f12611移动后缺失；14F0独立副本42672的f4738、f7128、f8354投影保留，最后一帧161个mode5绘制全部PS=0；新增GPU定向模板回归通过。营地多位置通过不等于所有地图/遇敌完成。启动分批清除修正仍保留，见[阴影证据](shadow-texture-lod.md)与[启动记录](startup-depth-clear-crash.md)。

Ring外环已在选定遇敌验证；随时存档后台保存/重载通过、桌面UI待验；音频错流修正完整营地至城门路线通过、全部对白待验。临界区死锁已有修正，独立的GPU query/wait异常仍开放。火焰和箱子效果未修好。

## 执行与版本约定

当前连续目标仅允许后台独立程序及save/profile副本，不操作桌面、不覆盖用户存档、不终止用户游戏，不自行commit/push。此前桌面与双推授权不适用于当前约束。本次用户明确要求代码提交双推，已形成2a4f47d/42c543f/c4fc9e4三个代码提交。两个子模块的工作树修改通过tools/patches保存，不需要推上游或更改gitlink。后续自主推送仍按当前授权范围判断。

30分钟heartbeat用于中断恢复，不表示持续有agent运行。任务是否运行需实时查询；不要把日志仍跑当作游戏正常。当前报告不宣布通关、全音频完整或所有阴影修复。私有产物留在ignored的out目录。

[专项索引](README.md) · [旧交接](../archive/2026-09-04/handoff.md) · [发布约定](../PUBLISHING.md)


窗口修正730E2653已编译并独立游戏阻塞测试通过，详见docs/notes/window-event-pump.md；用户40932仍旧FB版，重启存档确认待答。Map13连续帧已明确捕获暗亮交替，根因未定。


2026-09-05 海报修正 AC3B3A73 已完成并单独保存 out/poster-fixed.exe，未替换用户主程序。只读 float24 补光层保留绝对深度偏移，避免浅反向深度下 D32 ULP 偏移过小；基底/补光GPU顶点回放差74ULP，旧bias24ULP。相同exe开关AB各300帧，海报ROI暗像素>20的帧旧169/300、新0/300；GPU额外验证旧拒绝/新通过/前景遮挡保留。详见docs/notes/map12-poster-depth.md。所有研究游戏均自动退出；用户继续正常游戏，不重启。跨地图回归仍待后续，Map13按用户要求暂缓。


更正：用户明确没有在玩；此前“用户继续正常游戏”是助手误解。已核对主程序无进程，将已验证 AC3B3A73 exe/pdb 更新到主构建目录，安装后 SHA256 一致。未启动游戏，原 save/profile 未改动；旧730E2653仍保留在out/window-pump-fixed.exe。

2026-09-05 启动着色器准备 2BD8598E 已构建验证并安装到主目录，exe SHA256 核对一致，缓存 184 DXIL/source 已放入主构建工作目录。未启动游戏、未修改原始 save/profile。热准备174ms，仅source重建4500ms，损坏DXIL识别通过；两次准备后Map12 f100后无>150ms日志。详见 docs/notes/shader-preparation.md。用户进一步要求提前整理全游戏着色器，下一步优先 FPI/FPD 资源解析与四盘微码提取可行性；184只是已知样本，不代表全部。PSO预创建仍开放。

2026-09-05 首次资源预编译8FD77179已安装核对hash。四盘扫描2000个微码，1998成功2失败；空缓存扫描35.4s+编译54.1s，热缓存2118个准备2.075s。Map12首次仍104变体补编译和PSO卡顿，第二次无>150ms日志。扫描/编译进度界面与窗口关闭已实现；独立GDI绘制目视验证，后台跨进程截图黑图不能当视觉证据。主缓存保持原状态，用户下次启动自动扫描；未启动可见游戏、原save/profile未改、未提交推送。详见docs/notes/shader-preparation.md。

2026-09-05 多线程预编译7ABDBAFD已安装hash核对一致。按逻辑线程数-1(min1)启worker，本机16→15；同exe2000shader无DXIL对照总准备53441ms→6746ms，约7.9x。1998成功DXIL全部逐字节相同，2旧失败保留。热缓存2118个准备1970ms，完整profile副本Map12载入后运行10秒完成并自动退出。CP独占渲染器映射/计数/GPU对象，worker仅翻译编译与独立cache写入。资源扫描和PSO未并行。未启动可见游戏、未修改原save/profile、无提交推送。

2026-09-05 用户可见扫描界面文字闪烁：video进度窗口原先直接清屏再画字且每8ms可能重绘。64FDB4ED改为离屏完整绘制后单次BitBlt、UI限10Hz且阶段切换立即更新、仅尺寸改变时MoveWindow。编译链接通过；独立GDI连续300次绘制和暖机后句柄计数检查通过，buffered.png目视核对。用户41308仍运行7ABDBAFD且响应正常，不中断，不替换主exe；修正版out/progress-buffered.exe/.pdb待下一次安装/启动。可见动态闪烁仍待重启实测。
