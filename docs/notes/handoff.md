# 接手入口

## 2026-09-06 当前交接

[v0.2.2 已正式发布](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2)，为最新正式版，包含 AMD resolve 初始化和 Unicode 启动／存档路径修复。AMD 与 NVIDIA 定向验证、NVIDIA 用户视觉验收通过；Issue #4 完整游戏崩溃尚未复现，不宣称已解决。见[发布说明](release-0.2.2.md)、[AMD 证据](amd-resolve-initialization.md)与[路径验证](save-path-unicode.md)。文本语言补丁仍暂停。

本次 v0.2.2 发布于 UTC 2026-09-07 03:11:10（本地 09-06），标签 `f03efe370d444db1a8a9c1213c240da697f58504` 已双推；[CI 34077788392](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34077788392) 全通过。正式 ZIP 38,205,388 字节，SHA256 `e91af2f49c03da48714731b07912767614d676be9d8887192647b217f2d29789`；CRC／44 项 manifest、安装器自测及正式 EXE 8 项启动路径检查通过。中文工作目录 RTX 5080 隔离运行 49.54 秒，swap 1200 目视确认为 Map 12；原有两项 shader 失败保留，无新增 error/fatal。测试已清理自身进程。原工作区证据：`out/release-v0.2.2/{published.json,package-validation.json,smoke-result.json,ci.log,smoke-存档/scene.png}`。

首战研究现场已结束：原工作区 `out/firstbattle-flicker-01/report.md` 记录 USA/Europe 实际首战 120 帧待机、600 帧攻击及 600 帧含 Magma Blast 的序列，另有 7 份完整 capture。未锁定新闪烁根因、未修改 runtime；捕获会扰动时序，不能归因攻击卡顿。自有进程 51728 已核对路径后停止释放 GPU，用户 review 优先级仍保留。

以下为 2026-09-06 较早的 Gitea/runner 观察，非本次服务复查：用户先选择关闭本仓库 Gitea Actions，API PATCH `has_actions=false` 后 GET 已确认；随后用户启动 Windows runner 并要求重开，现已 PATCH `has_actions=true` 并 GET 确认开启。`win-t640` 心跳为 `2026-09-06T19:24:52Z`，已在线，但标签仍只有 `windows-latest`／`windows`，无法匹配 7 个旧任务要求的 `windows-2022`；标签匹配待处理，历史队列记录保留。`git ls-remote` 已确认 Gitea 的 `main` 与 `v0.2.1` 完好，代码镜像继续双推。GitHub [正式 CI 34053765472](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34053765472) 全部步骤成功，包括 LoHidTest。

历史 v0.2.1 发布记录，标签指向 `906d7c039f7e709c57af2c5278a43e259bfba8e5`，两远端标签均已确认。正式 ZIP 为 38,203,314 字节，SHA256 为 `cd583a6a28b47a1b2e7e31984052cd4eb6c953f5df333198f41540114eb4afb3`；44 项 manifest 哈希、安装器自测通过。冷缓存隔离运行 54 秒，德文标题菜单目视正常；frame 400 的 ZIP 共 60 项，每项 CRC／SHA256 校验通过，压缩后恢复约 30 fps。证据见 `out/release-v0.2.1/package-validation.json` 和 `out/release-v0.2.1/smoke/runtime.log`。此前 v0.2 的代码标签 `dcc9462`、发布记录提交 `b572d0a` 保留为历史。当前进度与近期 TODO 以 [STATUS](../STATUS.md) 和[中文路线图](../ROADMAP.zh-CN.md)为准；以下 2026-09-05 的构建名、PID、授权与实验安排保留作历史记录，不是新的执行指令。

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
