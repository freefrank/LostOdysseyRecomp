# 路线图

[English](ROADMAP.md) · [当前状态](STATUS.md) · [更新日志](../CHANGELOG.md) · [历史路线图快照](archive/ROADMAP.zh-CN-2026-09-10.md)

`[ ]` 待完成 · `[~]` 进行中 · `[x]` 在所述范围内已有证据。[公开维护者 Project](https://github.com/users/freefrank/projects/3) 是当前工作项的事实来源。本镜像只保留方向、未完成事项和验证边界；实现、玩家验收和发布状态彼此独立。

v0.6.20 已于 2026-09-25T21:15:07Z 从 tag/source commit `be842b91d7367fd198074b1b8d3c1bc3ef4372a6` 公开发布。Release CI [36188596414](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36188596414) 已成功完成。Windows ZIP（243,700,717 字节）与 Linux AppImage（251,005,432 字节）打包核验通过并包含 .sha256 sidecar；未新增 hash 下载核验。

## 交付

[v0.6.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.20) 是当前公开发布版本，整合了屏幕采样抖动候选批次、后期 e810 trial、时序抖动暂缓候选后续、Hybrid 相机／深度运动回退、有界 DLSS 尺寸恢复、离线 F1 捕获诊断及捕获 CLI 维护：
- [x] **v0.6.20 发布：**从 `be842b91d7367fd198074b1b8d3c1bc3ef4372a6` 发布；Release CI 36188596414 已成功完成，Windows ZIP（243,700,717 字节）与 Linux AppImage（251,005,432 字节）打包核验通过并附带 sidecar；未新增 hash 下载核验。源码有界证据及运行时／玩家验收边界仍见[状态记录](STATUS.md)。
- [x] **v0.6.19 发布：**从 `1b2ea66` 发布；Release CI 36092250520、包 hash、sidecar、GitHub digest 与匿名资源下载核验均通过。Issue #35 与 #61 已关闭。源码有界证据及运行时／玩家验收边界仍见[状态记录](STATUS.md)。
- [x] **各向异性过滤（AF）实时选项：**图形设置中提供实时各向异性过滤选项（关、2x、4x、8x、16x），通过不可变 sampler table generation 与 GPU 完成后安全回收避免 UAF 和描述符竞态（Issue #35）。Windows 与 Linux CPU 构建及 softwareVulkan Actions 运行 36079262997 通过，合并后 `settings_interaction` 1/1 通过。全游戏实体 GPU 结论与实机画质仍待验证。随 v0.6.19 发布；Issue #35 在 GitHub 上已关闭。
- [x] **系统“退出到桌面”：**系统菜单中提供“退出到桌面”选项（Issue #35），匹配 `0x822E256C` 与 `0x822E26B8` 两个真实 guest 调用点并修复 UTF-16 NUL 截断复制长度。v4 用户明确验证退出成功。随 v0.6.19 发布；Issue #35 在 GitHub 上已关闭。
- [x] **“随时保存”偏好持久化：**在 debug 配置中记住“随时保存”（Save Anywhere）选择，默认保持 `false`（Issue #61）。CPU 跨进程配置持久化验证通过；单项实体机实测仍待进行。随 v0.6.19 发布；Issue #61 在 GitHub 上已关闭。
- [x] **精简缩放与抗锯齿预设：**在设置菜单中整合抗锯齿与缩放预设（Performance、Balanced、Quality、DLAA、NativeAA）；通过缓存菜单背景避免重复整屏软件滤波（无实测端到端延迟结论）。经由菜单交互回归测试套件（1/1）验证通过。随 v0.6.19 发布。
- [x] **Settings 确认返回主菜单：**新增 Settings 确认返回主菜单处理，等待原生 Settings 关闭后再请求 Title。本轮 Windows 上 LoMenuFlowTest 8 组测试通过，合并独立 `settings_interaction` 1/1 另行记录；未进行实机单项运行。随 v0.6.19 发布。
- [x] **分类金手指手柄导航：**debug 金手指菜单支持手柄 LT/RT 肩键边沿触发切换类别，并提供中英文底部导航提示。用户此前已在实体手柄上验收。随 v0.6.19 发布。
- [x] **输入与手柄改进：**手柄震动运行期 XInput motor 状态保持（非跨会话持久化偏好）、鼠标闲置自动隐藏及游戏运行时禁用 IME 输入法干扰。随 v0.6.19 发布。

- [x] **v0.6.15 发布：**从 `6eef30d` 发布；Release CI 36044604844、pack hash 门禁、Windows ZIP 与 Linux AppImage 打包均通过并内置便携式 Vulkan 着色器包（28,527 个着色器）。
- [x] **v0.6.3 发布：**从 `93bdbc1` 发布；Release CI 35476569158、包 hash、sidecar、GitHub digest 和 Windows manifest 核验均通过。源码有界证据及运行时／玩家验收边界仍见[状态记录](STATUS.md)。
- [x] **v0.6.2 发布：**已接受的 Uhra 4K Vulkan TAA 策略和实验性几何运动矢量 replay 已进入 Windows/Linux 发布包。CI、包交付和公开 sidecar 已通过核验；更广场景、1080p internal 到 4K output 的移动相机覆盖、Linux 原生 GPU、Steam Deck 和 D3D12 replay PSO 验收仍开放。

[v0.6.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.0) 已于 2026-09-18T15:21:34Z 从 `4b4b6c617172d43c7a73477881263e6e542d6cdf` 公开发布。[Release CI 35359206991](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35359206991) 已通过审计、Windows Release 和 Linux Release 门槛。公开发布包含 Windows 与 Linux 发布包、独立便携式 Vulkan 着色器包及 SHA-256 校验文件。Windows ZIP、Linux AppImage 和 shader pack 的 hash 分别为 `7ebcad6c2660ea6bcce801df3e6eb0f9bb6faef87e5108898618c1144031d161`、`46c1c10e9dbaf0dc5db490aa3fd2f5b195eea8ccdbd7e35a0996f978292d3901` 和 `387a23b9328b8136847d48b37b574fddd600a526eb837807fbb08b758c6de4d9`。原生 Linux GPU、Steam Deck、AppImage 更新事务和全流程游戏仍待验证。

[v0.5.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) 作为上一里程碑保留记录。其 Release CI、包来源和六个公开资产均通过核验；其中引入的便携式 shader pack 与在线 PowerPC 源码编译继续包含在 v0.6.0 中。历史提交、hash 和验证边界见 [STATUS](STATUS.md)。

<a id="v070-frame-generation"></a>
<a id="v070-upscaling"></a>
## v0.7.0 计划

- [x] **原生 Vulkan DLSS 超分辨率（SR）与 DLAA：**v0.6.11 正式打包了实验性原生 Vulkan DLSS 超分辨率（SR）与 DLAA，并在 Windows 与 Linux 发布包中内置官方 NVIDIA NGX 运行库。提交 `0625923` 修复了时序历史时钟推进与长间隔抖动丢失（BR-01），并通过按值快照消除了能力查询并发数据竞争（BR-02）。交付提交 `871685fdd75b84c00bb9ebb685f36d90169eb82d` 实现了图形菜单稳定行索引枚举（`GraphicsRow` 0..10）与真实渲染器执行反馈闭环（BR-03），细化回退状态并在 checked Vulkan 提交成功后才报告 `Active`；增加了设置保存时的结构化状态日志与参数记录；F1 渲染状态捕获的 `screenshot.bmp` 改进为提交给呈现系统的最终交换链画面，并与 `guest-frontbuffer.bmp` 同在 `XE_SWAP` 三帧归档完成；修复了从 DLSS Quality 切换至 DLAA 时外部 depth 纹理先于已退休 stencil 视图销毁引起的访问违规（UAF）崩溃；同步 NGX Evaluate 捕获（包含 raw、preview 与 dlss-evaluations.json 元数据）契约测试通过。用户实机验证成功完成 Quality->DLAA 切换并导出帧 2961–2963 无崩溃（2887 帧实机已 Submitted）。在插帧与 macOS 推迟至 v0.8.0 后，原生 Vulkan DLSS 超分辨率／DLAA 已通过用户明确验收。记录用户验收，维持已有测试与硬件验证边界。保持现有 DLSS/DLAA 基线为必要要求。详细记录见 [Native DLSS Validation](notes/native-dlss-validation.md) 与 [Bug Report](notes/post-v0.6.7-bug-report.zh-CN.md)。
- [x] **P1：可运行的 FSR 3.1.4 Vulkan 超分版本（Windows 与原生 Linux）：**G002/P1 已完成。FSR 3.1.4 Vulkan SR 已在提交 `87a1691` 中交付并通过 Windows 与原生 Linux 验证。证据范围与限制见[当前 FSR 交接](notes/fsr-dlss-fg-codex-handoff.zh-CN.md)和[历史记录](notes/fsr-dlss-fg-codex-history.zh-CN.md)。这不代表 Steam Deck 硬件等价。
- [x] **P2：FSR 画质调优与多平台硬件验收：**G003/P2 交付范围已随 [v0.6.15](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.15) 正式发布。在插帧与 macOS 推迟至 v0.8.0 后，当前 v0.7.0 交付范围内的 FSR 3.1.4（P2）多平台画质与运行验证已通过用户明确验收。用户已明确验收 Windows 画面；Linux 端经 psvita distrobox psbuild 验证 AMD 8060S RADV 游戏链接及 `LoFsrAdapterGpuTest --transient-only` 通过，用户确认 Linux 验证通过，补证任务已取消且无新增硬件测试。源码已合入并由 commit `6eef30d257f2e14ce30a546217574a0dc74fad69` 推送至 main、FSR 分支与 tag；Release CI [36044604844](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36044604844) 通过 Windows、Linux、FSR inputs、pack hash 及 FidelityFX 许可证门禁。生产路径 `Controller::RecordIsolated` 对 NaN 帧间隔正确拒绝并返回 `InputUnavailable` 与 `useId 0`，保持 context 处于 Ready；下一有效帧恢复 SDK dispatch 并触发 gap reset，读回中心蓝色通道 128->96 证实采用新输入。已发布 Windows ZIP 与 Linux AppImage 及对应 sha256 校验文件。记录用户验收通过，基于已文档化的测试与硬件验证范围。用户接受 APEX 15W 作为本轮 Deck 代理，不代表 Steam Deck 硬件等价。详见[当前 FSR 交接](notes/fsr-dlss-fg-codex-handoff.zh-CN.md)、[修复记录](notes/fsr-repair-2026-09-24.zh-CN.md)与[历史证据](notes/fsr-dlss-fg-codex-history.zh-CN.md)。

<a id="v080-frame-generation-macos"></a>
## v0.8.0 计划

- [~] **P0：通用超分／插帧契约与 Streamline 共存架构评估：**通用时序超分契约已实现，已有记录的定向 CPU 8/8 与 frame-plan 1/1 检查通过。随插帧推迟至 v0.8.0，未完成的 Streamline 共存架构评估及 P0 Gate 1 门禁（当前 NOT PASSED）移入 v0.8.0 承接，不标记完成。独立实验 Streamline/NGX 探针已有源码和一次前台诊断运行：二进制 `f47431b7746cdf54d847d7008547c0d7af0bce8a579f7596cccd5d95334c9efa` 报告了生成帧区间，但因 Vulkan validation errors 以 exit 1 结束。这是失败运行的诊断证据，不是通过验证的生成帧、物理显示证明、生产级 FG 交付或 Gate 1 验收。P0 尚未完成。详见[探针记录](../tools/tests/streamline_fg/README.md)。
- [ ] **P3：帧生成呈现基础设施与资源租约：**计划中的生产阶段，尚未开始实现。随插帧推迟至 v0.8.0。工作树中已有未经验证的 Streamline/NGX 探针源码，但这不代表生产 hook、HUDless/UI 分离、资源租约、Reflex 接线或帧生成已实现。
- [ ] **P4：Windows Vulkan 固定 2× DLSS 帧生成：**随插帧推迟至 v0.8.0。在运行时支持的 RTX 硬件上交付 Windows Vulkan 下固定 2× DLSS-G，覆盖 DLSS Quality+FG、DLAA+FG 及 FSR+FG 组合；具备切场景、过场影片、暂停、窗口重建下的安全停用与恢复，关闭时安全回到基线路径。要求真实生成帧呈现核验与解耦的游戏节奏；尚未开始。
- [ ] **延期帧生成积压项（D3D12 FG、FSR FG 与动态多帧生成）：**延期 FG 积压项保留原优先级，作为未来规划而非 v0.8.0 必交付项。历史计划中的更广泛目标（D3D12 2× DLSS-G，以及 D3D12 与 Vulkan 2× FSR Frame Generation）仍作为未来积压需求完整保留，动态多帧生成（MFG）亦由新方案明确延期；本轮迭代暂缓实施，不作静默删除或取消。见历史 [v0.7.0 计划](notes/v0.7.0-frame-generation-plan.md)。
- [ ] **macOS 发布规划：**随用户决定推迟至 v0.8.0 发布目标。当前仅为路线图目标，尚无 macOS 构建、后端、包、兼容性验证、玩家验收或发布产物。

<a id="v050-pc-graphics"></a>
<a id="下一主版本v050--pc-vulkan-与-direct3d-11"></a>
## PC 图形方向

D3D12 仍是可用基线。Windows Vulkan 已有 RTX 5080 实机场景的界定验证，与更广 GPU 和全游戏覆盖分开。Direct3D 11、Linux/Steam Deck 和 Switch 是彼此独立的平台工作，未声明完成或发布日期。Linux 首可玩范围已实现：在宿主 Mesa + SDL2 + X11/XWayland 上，以 `./LostOdysseyRecomp --game <disc>` 启动 Vulkan-only 未打包 ELF。WSL2 Manjaro + Mesa Dozen 运行已进入 1280x720 Vulkan 窗口，用户观看后关闭并正常退出；原生 Linux ICD、Steam Deck 和更广游戏流程覆盖仍开放。Linux 安装器／导入器、XDG/Flatpak 用户路径映射、POSIX libcurl 更新器以及 Flatpak 清单已实现；Linux AppImage 打包（`tools/package_appimage.py`）已随 [v0.5.20 发布版](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20)正式发布并内置便携式 Vulkan 着色器包（`shaders/portable_vk.lospv`）。menu 分支以纯软件跨平台设置菜单光栅化与游戏内调试浮层替代了 Win32 独占对话框。现已支持快速按需 Linux 构建（`tools/build_linux.sh` / `tools/build_wsl.bat`）与跨平台便携式 Vulkan 着色器包（`.lospv`，在 WSL2 Linux 下实测 28,482 个着色器 1.2 秒零编译启动）。原生 Steam Deck sniper 运行时打包、Flathub 提交以及全游戏实测通关仍待后续跟进。见 [Linux 移植评估](notes/linux-port-evaluation-2026-09-13.md)。

<a id="近期优先事项"></a>
## 当前事项与验收边界

- [~] **Issue #49 窗口坐标初始化排查：**保留为维护者活跃排查事项（freefrank 确认：“still gonna dig deeper later.”，2026-09-24T23:11:46Z）；旧系统环境下渲染器初始化前 `SetThreadDpiAwarenessContext(PMv2)` 返回 null 导致启动终止。不按 unsupported 关闭。
- [~] **Issue #37 PortForge 启动器集成：**已获授权推进实现；当前处于调研与适配准备阶段，正并行核对外部 `.forge.json` 清单合同及本地路径，尚无代码或验收。原生 Linux AppImage 发布包已独立发布并受支持。
- [~] **Issue #54 与 #55 语音与过场反馈：**语音及过场语言反馈保持开放；Issue #54 在 v0.6.3 语音菜单修复后近期仍有盘 3 等日文过场复现；Issue #55 主体已改善，但 Obsidian Miasma 单句人声缺失待存档复现（非全盘终 Boss 人声缺失）。
- [ ] **Issue #30、#40、#48 体验与功能积压：**景深滑块/开关（#30）、PS 手柄按键提示与 Mod 文档（#40）、晕动症视角防抖与动态模糊开关（#48）保持为待办积压。
- [x] **Issue #53 盘 2 加载卡死：**已于 2026-09-19 关闭并在 v0.6.3 中发布，保持关闭，不重开。
- [x] **v0.5.4 发布：**已于 2026-09-11T01:36:30Z 从 `2ad94d418bb0478417ab9589109f1f685ed92eb3` 公开发布；CI 34550200618 通过。44,237,061-byte ZIP 的 SHA-256 为 `104ced8b60c16cd1b9013543a3940c9ed8d7cf904c3d05a6a8ef8d591f51d218`；包来源、版本、全部 50 个文件 hash/CRC 及四个匿名资源下载均通过。本条只记录发布交付；各项运行时和玩家验收边界仍见下文。
- [x] **移除 PPC 预编译同步并改为在线全源码编译：**在 v0.5.20（commit `9a1617a`、`693042d`、`03f0d9f`）中，PowerPC 预编译静态库缓存与远程同步机制（`LO_PREBUILT_PPC_DIR`、`ppc_sync.py`、`ppc_prebuilt.py`）已彻底移除。Windows 与 Linux 构建均统一在 CI 及本地 Release 构建中从源码在线编译 `LostOdysseyRecompLib` 客户机 PowerPC 代码。消除了平台专属静态库缓存契约，并为未来架构（如 ARM64）铺平道路。早期 direct-main 同步与 fingerprint 审计证据保留为历史。
- [x] **便携式 Vulkan 着色器包（.lospv）与 Linux AppImage 发布：**可重定位便携式 Vulkan 着色器包（`.lospv`）已实现，包含 SPIR-V 字节码 SHA-256 去重与分块 Zstandard 压缩（28,482 个着色器压缩为 169.9 MB），最初随 [v0.5.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) 正式发布。维护合并将测试 capture 与 Windows 分发中的 45 个重编译着色器合入基线包（28,482 -> 28,527；产物 `out/merged-shaders/portable_vk.lospv`，178,332,830 字节，SHA-256 `b486c87d121968bcec67fae6bc1aa7926378455281bc8b2a221409b8c06c6e2b`），`verify-runtime` 验证全部 payload 及运行时兼容。着色器包内置于 Windows x64 ZIP 与 Linux x64 AppImage 并随 [v0.6.15](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.15) 正式发布（源码 commit `6eef30d257f2e14ce30a546217574a0dc74fad69`，Release CI [36044604844](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36044604844) 固定 pack hash 门禁通过；按用户需求不发布独立 Vulkan bundle）。详见 [STATUS](STATUS.md)。
- [ ] **D3D12 内置便携式着色器包（.lospv）：**计划放到 v0.6.15 之后的下一版本（具体版本号未定）。复用现有便携包 Reader/Writer、去重及 LoShaderPackTool 基础设施，支持 D3D12/DXIL 后端专属契约、容器校验、渲染器运行时加载与 Windows 打包。目标减少运行时 HLSL->DXIL 编译开销（排除驱动私有 PSO 缓存）；实现与验证尚未开始。
- [ ] **抗锯齿选项整合与独立实验性 TAA 移除：**计划放到 v0.6.19 之后的下一版本（具体版本号未定）。取消独立 experimental TAA 用户选项，保留关闭（Off）、FXAA、空间型 SMAA、DLSS 与 FSR（时域超分）；FXAA 与 SMAA 路径关闭时域 jitter 与自研 TAA 累积；DLSS 与 FSR 保留必要公共 jitter、深度及运动矢量输入处理；旧配置中的 TAA 迁移到稳定空间 AA 模式（具体默认选项待实现决定）。背景：用户在 capture `render-17903110123692464-f2548.zip`（capture header 声明为 v0.6.19，非可执行文件哈希认证；Vulkan，RTX 5080，4K）报告地面闪烁；f2548 结构检查已完成，具体地面 draw/shader、闪烁阶段和根因尚未确定。验收需分别检查 DLSS 与 FSR 的地面闪烁，不能宣称移除 TAA 即修复。实现与验证尚未开始。
- [~] **GitHub Issue 自动分流：**`main` 至 `600b08e` 已部署 Issue-opened 分流、`@codex` 评论处理及只读、受版本控制的第一方源码检索，供已配置 API 分析。secret 已配置，BOM 处理已修正。19 个 triage/mention 测试和 7 个 retrieval 测试通过；本地合成 mention 与真实 Issue #27／源码／API 中文预演成功且未 POST。hosted workflow-dispatch [34887742345](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34887742345) 成功，但普通路径没有 `comment_id`，不能证明真实 `issue_comment` 事件或公开回复。首次真实 `@codex` 公开回复、分流质量、修复、验证、玩家验收和发布仍待完成。
- [x] **PPC 0.5.6 fingerprint 审核：**[Release CI 34726533463](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34726533463) 已通过 v0.5.6 的 hosted PPC 消费与发布。runner key `d8919775…` 获取不可变私有 cache `a6cd91ea…`，恢复并校验库 `ba3e4c4d…`，没有生成 PPC 编译或新库链接。在 v0.5.6 当次发布时，其公开非预发布版本为 Latest；当前 Latest 为 v0.5.8。四个公开资产均通过匿名 HTTP 200、大小和 hash 核验。producer `50b8ad…`/`6a6ed031…` 和五项行尾／十四项 symlink 的表示差异证明保留为历史。`lo.ppcAutoSync=false`；源层 key 规范化尚未实现。
- [x] **安装器拖动闪退：**v0.5.4 已公开包含本地验收的 `PostMessageW` 修复，针对 v0.5.3 安装器拖动卡顿／退出。message-only HWND DragDispatch case 通过 1/1；旧 fixture 仍受未变的前台 setup 断言限制。用户于 2026-09-10 验收报告的拖动路径；更广安装器交互覆盖仍为独立事项。
- [x] **更新器简化：**v0.5.7 已从 954d0e17 发布，CI 34743383193 通过。PPC key 3260d975 复用经审核的 v0.5.6 原始库，未改 PPC 代码。公开 ZIP、50 个 manifest payload、独立 updater 和两个 sidecar 均已完成匿名交付核验。负责人于 2026-09-15 关闭更新器验收等待；公开 v0.5.7 包含 GitHub 二进制更新器，具备验证、回滚和保留用户数据能力。真实网络更新交易覆盖保持为独立回归项。
- [x] **可选 shader 收集：**v0.5.3 已发布紧凑的 opt-in 诊断和 schema 3 绑定证据；Worker schema 1–4 与私有归档、账本已集成。紧凑收集使用 32 帧／180 秒 CPU 窗口、最多 24 个配对和 8 个绑定，限制在 32 KiB 内。18 个账本案例仍待复核。schema 3 只覆盖 e810 的两对绑定；新候选仍需程序复核及足够的最终绑定、生产者时序和 jitter 证据。画面修复、玩家验收、更广 GPU 覆盖和全部提交绘制的覆盖审计仍待完成。
- [x] **Capture 导出超时：**两次 F1 ZIP 归档均在约 60 秒后超时。archive worker 现改为在把子进程视为超时前最多等待 180 秒（`60000` → `180000` ms）；既有进程、job object 和 `Optimal` 压缩行为保持不变。v0.5.3 EXE 已成功链接并核对 `build.json` 来源，安装替换后的 SHA256 为 `CF70EA663ED230334145E1135CA97A58DFE3A34C65ED7D43E9E428C41B53270B`；旧 EXE／metadata 已备份至 `out/f1-zip-180s/backup`。整体构建最终因源码依赖 DLL 缺失而在复制 DXC 时失败；未重编译，仅从安装目录复用 DLL 补齐 build 输出。v0.5.4 安装包及公开下载核验已通过。负责人于 2026-09-15 关闭剩余超时恢复等待。
- [x] **4K TAA 与地面／阴影反馈：**17468–17470 capture 来自早于四路径 c7 修复的较早 EXE。它不重开后续已验收的四路径 Sol 修复，不定位新根因，也不建立更广画面覆盖。负责人于 2026-09-15 关闭剩余验收等待；保留有界 c7 验收和已发布 v0.5.8 的 TAA 路径修复，更广场景与硬件覆盖继续通过 Project 跟踪。
- [x] **坎托族魔装兵光照闪烁：**v0.5.7 已从 954d0e17 发布，CI 34743383193 通过且全部安装包交付核验完成。用户已验收第五版在报告遭遇中不闪；不代表所有场景或完整游戏覆盖。若复发则重新打开本项。
- [x] **PPC 生成源码一致性检查：**生成树 provenance 子范围已完成：`ppc_codegen generate` 产出 247 个 C++ 单元，含 843 个 `.u32`、零个 `.u64` selector，246 个指令流哈希保持不变。7 个 guard fixture 通过，覆盖旧输出、input/tool/context drift 和 return-0-without-output recovery；历史 3,258／109／843 表-44,523 证据已复用而未重跑。CMake 配置及编译前检查依赖已核验；更广 PPC 契约审计仍开放。Issue #14 调查明确区分原始历史 v0.5.0 日志（LR `829DFDCC`、CTR `829DFEF0`）和第二份历史 v0.4.2 日志（LR `823CB53C`、CTR `0`）。后者的 guest virtual-call 位置 `823CB538` 与 Issue #12 material-call 分析相似，但没有对象释放现场可证明同一根因。`gc_render_flush.cpp` 已存在于 `17e3ab7`；尚无新的运行时验证、当前版本复现、报告者验收或发布证据。
- [x] **新增游戏流程调查（#15、#16）：**#15 的 source-v0.5.4 日志在用户关闭进程前保持 60 fps 渲染和音频运行；同一存档之后可加载，第二次存档未挂起。其目录名 `0.5.0` 不是版本证据。两项 compiler DLL hash 与 v0.5.4 manifest 相符，其余组件仍未知。更新器调查发现 `StageArchive` 漏将 `manifest.json` 纳入 `ApplyPlan`，更新后会残留旧清单。其关联修复已验证：当前 worktree 的 clang-cl `/O2 /MT` 定向编译成功，`--manifest-transaction` 三场景零失败，覆盖 plan 往返、替换后 post-apply 回滚、manifest 替换后注入失败回滚和 manifest 篡改拒绝。原 updater-only 验证未运行 helper／游戏，也不构成玩家验收；实现现已包含在公开 v0.5.6 中。up-to-date 路径仍只对比版本而非每个已安装文件 hash，故混装 DLL／资源尚未排除，Issue #15 挂起根因仍未知。#16 实现和有界本地验证已完成，等待报告者验证。最终 branch-native-r1 EXE `f2015ad7…` 在 D3D12／亚洲 Disc 3 从 user09 通过 final-freeze-01，未跳过国王冻结。1527.792s，raw `0A33A8C0` 使用缺失 particle-shader fallback；稳定内存匹配 `xf_shd_aniflz.freeze`、GUID `0fd4ca6d4bb67581c9c9f5b8f0af62c0`、子表 count 16 和 particle shader 0。180 张 PNG 全部完成（shot45508 冻结、45823 对白、47673 外景）；1733.599s 回到 map229，shot53430-53517 显示 30 tick 向下输入后的角色移动和镜头变化。本次无坐标 telemetry。两个 native checkpoint 均可重读，原 seed 未变。报告者／玩家验收和 Vulkan／其他区域覆盖仍待完成。实现随后已提交、推送并包含在公开 v0.5.6 中；报告者验收保持独立。
- [x] **#14–#16 本地合并记录：**本地提交 `2019cd017ab939d0b728cec340f7835c2082e615` 记录本任务的 triage、updater-manifest 修复及 particle compatibility 修复／证据，已包含在从 `7124f4b…` 发布的 v0.5.6 中。[Release CI 34726533463](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34726533463)、包来源／50 个成员 hash 及四个匿名公开下载检查均通过。各项调查、报告者验收与覆盖边界仍以上述内容为准。
- [x] **#16 main 0.5.6 回放：**正常 main Release 构建（`d50c240d…`，173.782 s）在 D3D12／本地亚洲 Disc 3 从 user09 通过完整目标回放，无诊断对象或 binary bridge。原生 F1 两次判胜；冻结未跳过，1759.793 s 在 raw `0a54ce40` 命中 fallback，剧情继续，map229／菜单／30 tick 移动通过，360 张截图完成。修复已包含在公开 v0.5.6；CI／包／公开下载只核验交付，未重复游戏。报告者／玩家验收、全游戏、Vulkan 和其他区域证据仍待完成。较早 branch-native-r1 v0.5.4 通过记录保持独立。
- [x] **渲染输出／DPI 呈现诊断：**修正后的 D3D12 DPI144 路径已在 Project 中记录；Vulkan、发布和玩家验收仍待完成。
- [x] **全屏 underscan 诊断：**修正后的原生／D3D12 尺寸路径已在 Project 中记录；实际全屏行为、宽高比区别、Vulkan、发布和玩家验收仍待完成。
- [x] **Alt+Enter 窗口／全屏切换：**v0.5.13 已从 `545af9be` 发布，Release CI `34908617463` 已通过；44,304,038-byte 公开 ZIP 的 SHA-256 为 `a993071c24f7324a3ae3a0dbe24688afc60450d3da9f1a78fbf532c69f65bacd`。实际本地游戏路径已在 Vulkan、RTX 5080 下验收左 Alt+Enter 和右 Alt+Enter（包含 AltGr／VK_MENU 修复），原生 fixture 覆盖了保存窗口位置和 pending-transition。实机验收未使用公开安装包；实机运行未单独重新测量位置恢复与防连发，完整游戏验收仍未完成。
- [x] **超宽屏 (21:9) 与 FOV 布局（Issue #17）：**已随 v0.6.7 正式公开发布（2026-09-20T20:09:28Z，源提交 `f92c24da03816b4c0c7664fbc8589169a205b555`，Release CI 35533399325；Windows ZIP `37232481…` 与 Linux AppImage `de99f9ee…` 均通过 SHA-256 sidecar、GitHub API 摘要、4 项资产公开 HTTP 200 下载核验，Windows 清单版本为 v0.6.7 且 dirty=false）。基于 v0.6.6 初版原生 Hor+ 超宽屏及已验收的阴影映射修复（mode 4+5 深度光栅化与缓存高度修正），图形设置菜单在输出分辨率上方新增宽屏开关，开启后提供 1720×720、2560×1080、3440×1440、3840×1600、5120×2160 五档 21:9 预设（关闭切回 16:9，按最近高度映射且等距取高档，保持保存／取消原路径，旧宽屏配置自动推导），首启列表与五语言本地化已同步。菜单 hook 流测试（初始 3440 推导、五档正向循环、2160 开关、取消重开、保存应用）、离线渲染截图布局可读性及增量构建均通过（10/10 构建成功）。用户运行最新构建验收当前宽屏交付确认无问题，Issue #17 现已正式关闭。不宣称全分辨率或跨 GPU 穷尽覆盖，未来若有新回归或场景反馈再行跟进。
- [ ] **呈现与输入后续事项：**混合 DPI、鼠标、Exclusive DXGI 全屏和 underscan 仍开放。判断用户报告的 underscan 时应保留正常宽高比黑边。
- [ ] **D3D12 motion replay PSO 创建（`0x80070057`）：**在 motion replay 自动验证中，异步生成的 DXIL replay shader 编译成功，但 D3D12 `CreateGraphicsPipelineState` 返回 `E_INVALIDARG 0x80070057`，日志反复记录 `MV pipeline allocation failed`（`runtime-639254046562050769.log`），导致 `replay=0 ready=false consume=false`。Vulkan 后端已验证成功并达到 `replay>0 ready=true consume=true`。后续修复需定位 D3D12 PSO 描述符中的无效字段或状态不兼容，使 D3D12 下 replay PSO 正常创建并完成自动场景回放消费，且不回归 Vulkan 路径。
- [ ] **游戏流程与稳定性：**后续推进、存档读回、遇敌、长时间稳定性、其余渲染反馈和跨 GPU／正式包证据仍为待办。既有界定修复不代表可完整通关。
- [x] **Issue #6 与 #22 启动诊断：**v0.5.11 已于 2026-09-14T02:09:50Z 从 `624729c` 公开发布，[CI 34797755460](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34797755460) 成功。44,304,683-byte ZIP 的 SHA-256 为 `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132`；干净 provenance、50 个 payload hash/CRC 和四个匿名公开资产检查均通过。既有选定日志 fixture 和有界 D3D12/Vulkan 日志核对复用而未重跑。两个 GitHub Issue 截至 2026-09-18 均为 CLOSED；根因和原报告者验收仍未记录。#6 的 v0.5.6 关联仅为历史。见 [v0.5.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.11)。
- [x] **Issue #27 西班牙语 FMV 字幕：**source/tag `36e574b` 包含 v0.5.12 修复：过场动画注册表按当前语言查询时，为 GameLanguage ID 1–9 恢复原始表后缀。2026-09-14，用户在 USA/Europe Disc 1 测试路径、Vulkan 3840×2160 下，验收了西班牙语（`game_language=5`）开场 FMV／过场动画字幕。[v0.5.12](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.12) 已于 2026-09-14T21:34:13Z 发布；[Release CI 34895591364](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34895591364) 已通过，使用 `LostOdysseyRecomp-build-inputs` 中的预构建 PPC。2026-09-15 原报告者确认西班牙语开场 FMV 字幕（Vulkan 与 Direct3D 12），德语、法语、意大利语 FMV 覆盖亦已确认。Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) 已关闭。完整事件覆盖和全游戏验证仍未核验。
- [x] **汇编级性能分析：**外部 Win64 工具现已记录 wall-clock RIP 快照，解析 DbgHelp PDB 符号／源码位置，并生成 Capstone x64 HTML/JSON 热点、线程 CPU 时间／筛选和函数 self 样本排名。7 个定向报告用例、MSVC Release 构建和一次合成端到端采集均通过。2026-09-11 两次隔离实机采集使用已发布 v0.5.4 EXE `3c3b4073…`：user00 走图 5,092 样本／0 失败／最忙线程 8.11 s CPU，user01 城市走图 10,736 样本／0 失败／最忙线程 7.27 s CPU；均为 `xenon_scr.fpd`，约 36–44 fps、1,900–2,300 draws/frame、frontbuffer 1280×720。最忙线程为无符号 EXE 样本、`NtWaitForSingleObject` 与 AMD/D3D12 的混合。负责人于 2026-09-15 关闭剩余工具验收等待；外部 Win64 采样器随 v0.5.4 发布，存在实机采集，GPU 指令分析保持在范围外。见[实机采集](notes/asm-profiler-gameplay.md)。其 PPC 注释是生成源码上下文，不是精确 guest PC；请求的采样间隔也不代表实际频率。
- [~] **性能与 shader 启动：**本地 `main` 提交 `ae287f2` 记录了已完成的有界 Hidden 城市子项：41.8241 ms vertex 卡顿来自 `std::unordered_map` 插入／rehash，而非 `CopySwapped`；新的 65,536 项预留 dense metadata cache 使用 16 候选 LRU 淘汰，保持 1280×720 D3D12 AA=3、画质、双槽和 arena/wait 策略；`LoVertexCacheTest` 一次通过 3,569,548 项检查。最终同 EXE 运行仅把 driver input／screenshot-request 文件移至 TEMP，观测到的 post-present 最大值从 412.9283→0.4193 ms，但未证明文件系统原因。1,790 个城市帧的平均为 59.651463 FPS；固定 1,201 帧窗口为 59.918376 FPS、1% low 54.494611、超 16.67 ms 为 50.374688%、最大 present 22.5913 ms、最大 vertex 2.2981 ms、零 rehash 且无 draw 超预算。平均≥58 的目标已达到；全城 1% low 45.989346 和 43.0117 ms 最大 present 保留城市入口 load/bind 尾部。这是接近 60 的路线证据，不是每帧锁 60、严格 S4／全游戏通过或玩家验收。实现已包含在公开 v0.5.6 中，托管 CI、包及公开下载核验均通过；上述性能测量保留历史二进制身份；原存档未变，本地 EXE SHA-256 为 `9D9460248FEB72AC7239ABD40AC1DA6619847F176CF4AA38AD6F725C6923852B`。见[vertex-stage 报告](../out/perf-ring/vertex-stage/REPORT.md)。
  **v0.5.9 发布与 Project 归属：**v0.5.9 于 2026-09-13T19:50:00Z 从 `d26ee8b021784d7232b5319d816227867f98050d` 发布；CI 34778434518、包来源、全部 50 个 payload hash 与四个匿名公开下载均通过。该持续事项的 Project Release 字段为 v0.5.9。较早 candidate 测量已明确标为发布前历史，不是干净 release benchmark。4K60、Vulkan 1080p60 @15 W 与玩家验收仍待完成。

  Vulkan v0.5.9 已发布：深度清除及绘制状态/缓存改动复用已有验证；CI 34778434518 和四个公开下载文件均核验通过。固定 4K/60 W 结果为 7.50 → 43.48 FPS；binding-only 后续不证明整体收益。4K60、1080p60@15W 及玩家验收仍待完成。详见 [Vulkan 验证记录](notes/vulkan-depth-clear-performance-2026-09-13.md)。

   **Card A 城市实验（已发布 v0.5.11）：**指南仍仅为指南，未实施运行时。已对 published v0.5.11／source `0.5.11`（`624729c`）完成 Hidden 1280×720 D3D12 城市测量：`exhausted_resource_class=null`；fence wait、nested Flush、descriptor/upload/arena split、GPU queue、slot、limit 和 ring 均未表明有耗尽资源类或城市瓶颈。这不是运行时改动、版本改动、新 GitHub Release、CPU 性能验收或玩家验收。见 [Card A 证据](notes/cpu-card-a-city-2026-09-14.md)。

   **Card B 城市实验（已发布 v0.5.11）：**同一 Hidden 1280×720 D3D12 城市 profile 已用 `LO_VERTEX_TIMING=1` 取得：1,846 个城市帧；`match_ms` 均值 0.1535 ms（占 0.3616 ms stage sum 的 42%）。B1/B2/B3 均维持不实施：没有 length histogram、v0.5.9+ texture-chain snapshot／仍热的 lookup，也没有新的 guest-spin 证据。不要扩展 SIMD、再写 hash mix 或扩展 `poll_wait`。这不是运行时实施、发布、CPU 性能验收或玩家验收。见 [Card B 证据](notes/cpu-card-b-city-2026-09-14.md)。

   **Card C 准备门控（已发布 v0.5.11）：**同一城市日志加上既有 prepare worker 的静态清点表明启动 shader/pipeline prepare 已存在（bundle 2248 ms，pipeline 21 ms／4 workers／222 recipes）。per-frame 剩余（`copy_ms` 0.0091、lookup 0.051/0.03）低于排队／复制开销。新 Parallel Prepare 缺快照／所有权／join／取消／串行回退证据。`implement=false`；不要再加第三套准备池。Card D 已单独测过，仍默认关。这不是运行时实施、发布、CPU 性能验收或玩家验收。见 [Card C 证据](notes/cpu-card-c-prepare-gate-2026-09-14.md)。

   **Card D 3C6T 城市实验（已发布 v0.5.11）：**Hidden 1280×720 D3D12 AA3 60-cap 城市窗口使用进程亲和性 `0x3F`（3 个物理核心／6 个硬件线程），取得 1,370 个城市帧，超预算 0%，`fence_wait_ms` 均值 0.0008／最大 0.1336，GPU queue 均值 0.8162／最大 1.8092，`draw_ms` 均值 3.805／p95 4.556／最大 13.65，descriptor/upload/arena split 为零。默认钉核 `implement=false`；这只是实验，不是 Steam Deck、15 W、1080p60@15W 或玩家验收。见 [Card D 证据](notes/cpu-card-d-3c6t-city-2026-09-14.md)。

   **R3 CPU 等待路径现代化检查点（未发布的 `deck` 分支）：**实现 `LostOdysseyRecomp/notified_wait.h` 条件变量谓词／截止期等待辅助；`kernel/imports.cpp` 中 Event、Semaphore、Mutant 的有限等待从 200 µs 轮询转为条件变量等待；`gpu/command_processor.{h,cpp}` 增加写指针更新与关闭通知，以 500 µs 有界等待替代原 200 次 yield 循环并保留事件泵送。直接 fixture `LoNotifiedWaitTest` 通过（预通知、提前唤醒、截止期、CP 写指针唤醒）；既有 `LoPollWaitTest` 通过无回归；Windows 完整构建、diff check 及 Linux 原生构建／codegen 通过。Radeon 8060S 原生 Vulkan 在 15 W STAPM / 25 W Fast / 20 W Slow 下创建／调整 1280×720，稳定运行 90 秒无报错；60 W 启动 shader 准备完成 28,484 条（28,482 就绪，2 确定性失败）。前期稳定窗口 58.46–58.58 FPS，后期复杂场景 37–40 FPS（**不宣称锁定 60 FPS**）。无玩家视觉验收，未提交、推送或发布。详见[指南](notes/cpu-performance-optimization-guide.md)与[状态记录](STATUS.md)。


- [~] **v0.5.8 TAA 与 CPU 纳入：**[v0.5.8](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.8) 已于 2026-09-13T15:11:11Z 从 6e6f11cf 公开发布；[Release CI 34764115203](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34764115203) 通过。干净 source-0.5.8 包已通过 ZIP CRC、全部 50 个 payload hash，以及四个公开资产的匿名核验（bytes、SHA、sidecar 和 API digest）。其中包含 TAA cde8b50 解决及 SIMD／LO_QUERY_TRACE presence cache 修复。TAA 原场景视觉验收和更广的性能／shader 启动调查仍待完成；历史 v0.5.6 有界城市发布证据及既有 40 W 窗口均保留其原始身份。

- [ ] **索引缓存命中复制优化：**exact-content index cache 命中后仍会把已转换的 index 数据复制到 scratch。先对真实热点和实际收益做 profile，再评估免复制或引用路径；必须保留源字节验证、upload 生命周期、缓存替换及引用失效安全。此项仅为未来优化，尚未实现、构建或 benchmark。
- [ ] **Issue #57 低成本顶点缓存失效：**当前大顶点缓存命中使用头尾片段和跨区采样，以降低 CPU 比较成本。采样范围之外的合成修改仍可能漏检，但目前没有任何已知游戏 bug 由此行为引起。只有在有可靠的写入／失效机制以及实机复现或有界运行时证据后再重开；小顶点和 index buffer 的精确校验必须保留。当前采样策略 fixture 通过 3,668,957 项检查；尚未建立游戏或发布二进制性能结果。
- [ ] **重复实例运动配对的低成本实现：**当前按提交顺序配对重复实例，保证相邻帧 Nth-to-Nth 的运动匹配，不能简单删除这层逻辑。只有 profile 证明成本明显后，才探索保持等价行为的低开销实现，并在同一 Uhra 场景用 MV tracked/matched/replay 计数、画面和 FPS A/B 验证。此项待办，尚未实现或测试。

- [x] **Vulkan 像素着色器 LoopEnd 谓词退出：**v0.5.7 已从 954d0e17 发布，CI 34743383193 通过且全部安装包交付核验完成。既有有界 LoopEnd guard、64-lane GPU/reference 和 cache 证据保留。负责人于 2026-09-15 关闭实机 GPU 验收等待；LoopEnd 谓词退出修复已随 v0.5.7 发布。复杂循环与跨场景覆盖仍待完成。
- [x] **相邻重复 resolve copy 消除：**v0.5.7 已从 954d0e17 发布，CI 34743383193 通过且全部安装包交付核验完成。既有 CPU 与 D3D12/Vulkan FP16 fixture 证据保留。负责人于 2026-09-15 关闭实机命中等待；相邻 resolve copy 消除已随 v0.5.7 发布。跨场景命中覆盖仍待完成。
- [x] **Issue #38 努玛拉城黑光灯具修复：**`menu` 分支 commit `7484518` 在 `renderer.cpp` 与 `xenos_translator.cpp` 中对无符号 EDRAM 格式（格式 0、1、2、3、10、12，含 7e3 格式 `COLOR_2_10_10_10_FLOAT`）严格截断下限至 `0.0`，杜绝加法光照混合写入负数导致后续色调映射 `log2` 计算产生 NaN 与全屏黑洞。`cache.h` 着色器缓存 `Version` 提升至 23。渲染对比确认菱形黑斑消除，灯具结构、光照与光晕恢复；`LoMenuRenderTest` 编译并通过。已包含在 [v0.5.20 发布版](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) 中。全游戏通关与全场景玩家视觉验收仍待完成。
- [x] **`f2358` 场景 TAA 抖动闪烁修复：**`menu` 分支 commit `7484518` 在 `temporal_scene.h` 的 `PositionVPSlot` 中补充注册遗漏的静态场景与光照顶点着色器（`0x69e9adcf2e1b6887`、`0x6a8c2c78737dc94c`、`0xa20d6099a44e2cd5` 映射至 Slot 7，`0x6761469677f921c6` 映射至 Slot 8）。消除了 `f2358` 场景中阶梯与保存点光球处的帧间相机抖动相位不匹配闪烁。`LoTemporalJitterTest` 通过 2,319,037 项检查，画面对比确认帧间抖动相位对齐。已包含在 [v0.5.20 发布版](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) 中。更广场景覆盖与玩家视觉验收继续跟踪。
- [x] **着色器预构建并发优化与即时跳过：**基于宿主物理内存与 CPU 线程数动态缩放 DXC 与管线工作线程（`HostWorkerCap`），宿主内存 >= 8 GB 时自动使用 `logicalThreads - 1` 线程，低内存环境（< 8 GB）保留 4 线程安全上限。移除 Windows 落盘 `MOVEFILE_WRITE_THROUGH` 强行刷盘，利用写缓冲提升写入吞吐。引入核心着色器优先编译机制。增加启动预构建按键跳过支持（ESC / 空格 / 手柄 B 键）及 `settings.ini` 中的 `skip_shader_prebuild` 配置。已随 [v0.5.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) 发布。
- [x] **游戏内调试浮层与跨平台设置菜单光栅化：**已自 `menu` 分支完整合入 [v0.5.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) 发布版本。替代 Win32 专属调试对话框，通过交换链直接混合呈现游戏内浮层，支持手柄与键盘全功能导航。设置菜单采用纯软件 1280x720 光栅化，消除了高分辨率下的 CPU 光栅化卡顿。完整解决代码审查 R1 至 R17 缺陷项（协作式暂停安全点、按键释放隔离、解耦输入与呈现泵、呈现凭据传递、截图缓冲区校验等）。测试套件 `LoHidTest`、`LoHostUiCompositeTest`、`LoDebugOverlayTest`、`LoDebugMenuInteractionTest` 及 `LoMenuRenderTest` 均编译通过。

<a id="当前反馈与回归"></a>
## 保留的有效验证范围

用户于 2026-09-08 接受了有限的 D3D12/Vulkan 实机场景边界。四条 c7 路径在一个 Ghost Town 场景中约 11.37 秒内获得验收；这不是连续视频、跨场景、跨 GPU 或全游戏覆盖。此前已发布修复及其证据继续见[STATUS.md](STATUS.md)、[CHANGELOG.md](../CHANGELOG.md)和历史快照。

## 导航

以[Project](https://github.com/users/freefrank/projects/3)查看工作项状态、依赖和详细未完成需求；以[STATUS.md](STATUS.md)查看当前验证与发布证据；以[CHANGELOG.md](../CHANGELOG.md)查看已发布改动；以[历史路线图](archive/ROADMAP.zh-CN-2026-09-10.md)查看保留的详细历史。

<!-- 保留兼容锚点，供既有 Project 和 notes 链接使用。 -->
<a id="已发布里程碑v042--修复与验证"></a>
<a id="阶段-1产出可编译代码"></a>
<a id="阶段-2进入主菜单"></a>
<a id="阶段-3推进完整通关"></a>
<a id="阶段-4现代化"></a>
<a id="阶段-5可选探索"></a>
