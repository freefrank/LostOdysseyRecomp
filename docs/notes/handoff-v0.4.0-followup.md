# v0.4.0 后续开发交接（2026-09-07）

## 2026-09-07：v0.4.0 正式发布

[v0.4.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.0) 已于 2026-09-07 21:56:34 UTC 从标签提交 `40362d78285d9ad829d2b0f6143d4fc54d3514d8` 发布，CI `34163445379` 成功。正式 ZIP 为 42,556,481 字节，SHA256 `92bf89f19d6eca3f6c2ec5c4a04d9ef372d0cadc9b3117546027e6e32e749cc1`；45 项 manifest 与 System32-only PATH 下安装器自测通过。正式 EXE SHA256 为 `6cbb1360c36a0492df78098a244c72bf4bba93e4f75052aab0e093c1ae30a09e`，匿名公开下载与受测 ZIP 一致。

两版均通过普通便携 game-path 路径启动，实际 EXE 和 DXC／DXIL 路径及哈希已核对。Map2 Auto 1080p／TAA3 达到有效 1920×1080，无分配回退及 error／fatal／critical 日志。各空发现缓存命中 52 文件、零扫描／CPX 回退，20,686 来源哈希与各版保留的完整扫描基准一致；亚洲／美欧分别读取 142,093,208／142,289,816 字节。编译缓存为此前任务副本，不作全冷性能对照。两张截图已检查，静态结果不证明时序伪影已修复；两个已知预编译失败仍保留。7 项用户文件及两份 seed 哈希不变，任务进程由 harness 结束（exit 1，不记作自然退出验证）。

证据见 `out/release-v0.4.0/{package-validation.json,installer-self-test.json,smoke-result.json,release-published.json,public-verification.json}` 及 [STATUS](../STATUS.md)。发布说明按用户要求在标签之后改为独立英文／简体中文区块，属于文案排版调整，未改变标签或产物内容；公开正文与当前 changelog 版本正文一致。新构建画质验收、Issue #5 原战斗和 Issue #6 原机器诊断仍未完成。下文旧提交前说明和冻结开发包记录保留各自时点，不作为当前发布状态。

## 2026-09-07 提交前状态说明

本次准备将 v0.4.0 开发内容合入 `main`，不创建 tag 或 Release。下方旧记录中的“未提交／未推送”描述各自产物交付时点，不代表后续 Git 状态；旧包的基础提交、dirty 身份和哈希保持原样，不因后续源码提交而改写。视觉验收、#5 原战斗复测和 #6 原机器诊断仍待完成。

## 本轮改动概括

本轮在 `c548b48` 基础上完成下列实现及限定验证；这些实现已纳入 v0.4.0 正式版；下文保留开发时的限定证据。

| 改动 | 已记录结果 |
|---|---|
| 两版 shader 索引 | 亚洲／美欧按资源身份自动匹配，无区域选项；各 52 个资源文件命中索引、CPX 零回退，20,686 个来源名称／哈希全等于完整扫描基准。未读内容不作完整性检查。 |
| 真实内部分辨率 | Auto 跟随输出、最高 4K；手动 720p／1080p／1440p／2160p。Map2 实际场景尺寸和图形页预览／回退／Keep 已作限定验证。 |
| TAA 相机探针 | 修正参考点落在无穷远边界造成的历史误拒绝；720p／Auto 4K 各 256／256 条记录复用历史，像素重投影、阈值及其他历史 guard 不变。 |
| Issue #5 | 拆分 `0x82AFA388`／`0x82AFD150` 两个缺失调用入口，56 项真实 dispatch 检查通过；原报告战斗待复测。 |
| Issue #6 | 补充启动分配失败阶段、OS 错误及内存诊断，186 项注入检查通过；未改变映射策略，原机器根因仍未知。 |
| Plume 依赖补丁 | 两处原生纹理创建失败返回 null；固定 HEAD 应用补丁后 Git 规范化内容与当前源一致，原有改动保留。 |

此前[开发包](../../out/v0.4.0-followup/packages-both-editions/LostOdysseyRecomp-windows-x64-c548b480-dev.zip)：ZIP SHA256 `ed8e0627…`，EXE SHA256 `c75946a3…`；45 项 manifest、安装器和两版 Map2 实跑核验通过，7 项用户文件保持。旧包和旧证据保留。

这些结果不代表整体启动／FPS 收益或全游戏画质验收；新版本视觉反馈、#5 原战斗和 #6 原机器仍待复查。旧 freeze 的独立窗口线程修复属于既有工作，本轮仅做关联检索。

## 详细验证与产物证据

### 当前续接：两版联合元数据（2026-09-07）

亚洲和美欧裸 FPD／CPX 元数据已合并；按既有 FPI／FPD 身份匹配自动选择，无区域选项，也没有更改运行时匹配代码。CPX 表为 104 个 archive 布局、12,857 个 profile、275,186 个位置，原亚洲 profile 保留；两生成器新增可重复 `--additional-root`。两版各 52／52 布局与 52 个裸资源文件索引命中，零裸扫描；CPX 亚洲 10,060／美欧 10,198 索引命中，零回退。每版 20,686 个来源名称／逐 SHA256 等于保留的 strict 完整扫描基准，集合摘要均为 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`。Disc 2 入口自动发现兄弟盘并复用清单；元数据审计确认原 CPX／裸资源 profile 内容保留。

亚洲／美欧读取为 142,093,208／142,289,816 字节，美欧此前为 20,770,329,949 字节。12.537／12.631 秒属于本轮观测，可能与主构建并行，不作受控提速比例、完整启动或 FPS 结论。fixture 和主构建通过；证据在 `cpx-both-editions/{comparison.json,layout-audit.json,metadata-audit.json,fixtures.log}`。

联合索引新包为 `out/v0.4.0-followup/packages-both-editions/LostOdysseyRecomp-windows-x64-c548b480-dev.zip`，42,612,002 字节，SHA256 `ed8e0627f5398e2103f758009854ce29ee240f732565e66aeb2b19aa8b0295d5`，EXE SHA256 `c75946a3e83e7e1652f93646f332e51b19919b6a642cfd8ccda39a013cd3fe2e`。45 项 manifest、受测 EXE／编译器 DLL 与安装器 self-test 通过，无游戏／用户数据。

两版均实际运行新包 EXE `c75946a3…`，live 模块核实包内 DXC／DXIL；亚洲／美欧分别记录 13.482／13.367 秒发现、52 个裸资源文件索引命中、零扫描及 CPX 回退。相同 Map2 seed 的 Auto 1080p／AA3 场景均到达有效 1920×1080，无分配回退，两张静止截图已由主代理检查；每版实际清单全部 20,686 来源哈希与完整扫描基准一致。7 项用户文件、任务 seed 和 EXE 保持；任务进程均由 harness 结束，不记作自然退出验证。完整证据见[联合索引交付记录](../../out/v0.4.0-followup/cpx-both-editions/DELIVERY.md)和 `cpx-both-editions/runtime-summary.json`。这些发现时间不是受控完整启动／FPS 对照，也不扩大 #5 原战斗、全流程或玩家验收；该包产出时尚未提交、推送或发布。

### 此前版别验证：79308a15 包（2026-09-07）

用户要求验证亚洲及美欧版本。当前元数据匹配亚洲版四盘 52／52 个 archive 布局、美欧版 0／52；下方 30.015 → 13.280 秒及 98.55% 读取减少只适用于受测亚洲版。美欧版使用整包身份匹配与回退发现：裸资源 28 个索引／24 个扫描，CPX 7,401 个旧 SHA 索引／2,797 个完整回退；不能据此宣称相同快速路径收益。两版各自正常发现与 strict 完整扫描的 20,686 个来源名称／逐 SHA256 全等，集合摘要均为 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`。此次扫描与运行检查并行，耗时不作新性能对照。

两版都运行当前交付包 EXE `3aae46b8…`，live 模块确认包内 DXC／DXIL；日志分别识别 Asia/default（5 种游戏语言）和 USA/Europe（6 种）。相同 Map2 seed 的普通 Auto 1080p／AA3／30 FPS 静止检查均到达场景，有效内部尺寸与已审阅截图均为 1920×1080，无分配回退；每版实际清单内全部 20,686 来源哈希与 scanner 一致。7 项用户文件、任务 seed 和 EXE 保持，任务进程均由 harness 结束，不记作自然退出验证。

两版仍有相同的已知 `ps_78af7d75d932c582`／`vs_291187f5ef8ba74a` 预编译失败，不属于本次新漏提取。本次验证不涵盖原报告战斗、章节换盘、全语言／全游戏画质或新玩家验收。证据在 `out/v0.4.0-followup/edition-validation/` 的 `scanner/{layout-audit.json,comparison.json}`、`runtime-summary.json` 及两版 runtime 子目录；版别详情见[本次验证报告](../../out/v0.4.0-followup/edition-validation/REPORT.md)及[美欧版记录](europe-support.md)。本轮未修改源码、包或发布状态。

### 此前交付：CPX 已知布局直接提取（2026-09-07）

正常发现改为完整的小型 FPI 摘要＋FPD 名称／大小＋extent offset／size 布局绑定，跳过已知空包、non-CPX extent 和已成功提取的重复 profile；含 shader 的首个包只读取 header／table／所需 block，全部微码校验后才发布来源。未读块及同尺寸重复副本修改不在此契约的检查范围内。未知布局回到整包读取／SHA256 身份路径，仍匹配 profile 时可用索引；未知内容或提取失败完整回退。renderer 的 `LO_SHADER_FULL_SCAN=1` 禁用各索引并以 strict 每次完整扫描；v5 direct／strict 清单与旧 v4 分离。见 [shader 准备说明](shader-preparation.md)。

正式亚洲版串行应用冷缓存对照为 30.015 → 13.280 秒（减少 55.75%），资源读取 9,781,918,469 → 142,093,208 字节（减少 98.55%），解码量相同；20,686 个来源名称／逐 SHA256 全等，集合摘要 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`。候选暖发现 1.654 秒，只读 10,616,568 字节来源校验。两组顺序执行且没有并行构建，未清空 OS 缓存，不是完整启动或 FPS 测量；旧 49.578 秒及构建并行的 13.264 秒初测只作诊断。证据见 `cpx-direct/{baseline-final-cold.log,candidate-final-cold.log,candidate-final-warm.log,source-comparison-final.json,fixtures.log}`。

主程序构建通过，EXE SHA256 `3aae46b8d171cec4f977733a5a5f288d62d51aff7a1f0eeefbb098399509ebae`。该 EXE 使用亚洲版，在空资源发现缓存、复用任务自有 DXIL 缓存下实际启动，发现为 13.335 秒／142,093,208 字节／20,686 来源，52 个裸资源索引文件、零扫描／CPX 回退。Map2 普通 Auto 1080p／AA3／30 FPS 静止检查得到有效内部尺寸 1920×1080，无分配回退，1920×1080 截图已由主代理检查。运行缓存中的 20,686 个清单来源逐哈希仍与基线一致；另有运行来源，总计 22,954，不能把整个运行缓存数量视为 CPX 清单产物。7 项用户文件哈希不变；自有进程由 harness 结束，exit 1 不记作自然退出验证。

新 ZIP 位于 `out/v0.4.0-followup/packages-cpx-direct/LostOdysseyRecomp-windows-x64-c548b480-dev.zip`，41,787,161 字节，SHA256 `79308a1508edc459360d8b0791329e719cd5bb75a100ab57a61fab5f23059cef`。45 项 manifest、包内 EXE／DXC／DXIL 与受测 build 一致，安装器 self-test 返回 0，无游戏或用户数据。见[本次交付记录](../../out/v0.4.0-followup/cpx-direct/DELIVERY.md)、`cpx-direct/runtime-validation.json`、`cpx-direct-runtime/result.json` 和 `package-cpx-direct-audit.json`。旧包保留，仍未提交、推送、发布或新增玩家验收；以下 TAA、freeze、v1–v3 及早期记录保留各自证据范围。

用户补充“所有场景都看不出 upscale 效果”，并明确要求真正的内部分辨率最高到 4K，或默认跟随输出；随后授权自主开发 CPX 索引、内部分辨率及 Issue #5／#6。本轮工作基于本地 `c548b48`，尚未提交、推送或发布，公开下载仍为 v0.3.0。新版本画质收益尚未获用户验收。

### 收尾续接：相机历史误拒绝修复（2026-09-07）

新增精确 HistoryOwner 诊断后，两段各 256 对相机记录共定位 158 次旧 `camera_discontinuity`，全部来自 .001 参考点的 `InvalidWorldW`；Auto 4K 中 114 对前后 VP 逐位相同，证明静止也可能误拒绝。当前反向深度约定下，.001 接近投影无穷远边界；float32 相机系数使固定点落到无效 W 一侧。两次诊断使用同 seed 和 24 engine-tick 输入脉冲，但 readback／dt 和终点不同，不能声称逐帧相同轨迹。

生产 continuity 与诊断共用正 W 区间选点，普通无 pole 相机保留原深度；像素 `Reproject`、有效性阈值、四分之一屏幕切镜限制以及其他历史身份 guard 不变。默认 CPU 141 项通过；保留的 512 对相机输入共 3,215 项通过，复现全部旧拒绝并通过修正后的 continuity，含真实静止矩阵、连续移动、720p／1080p／4K 和无效 W／真实切镜负控。根因及合同见 `quality/taa-camera-pole-review.md`，CPU 证据为 `quality/temporal-camera-pole-fix-cpu.log`、`quality/taa-fixed-golden-cmake.log`；独立 CPU 未暴露的 Windows `near`／`far` 宏冲突已通过局部变量重命名修正，原失败日志保留。

新 EXE SHA256 为 `017c2ba16f3bd56251a7770e7ace6f42617ad740345503ac4ea02ce5239c31d5`，主程序、选定目标及时域 GPU fixture 通过。`taa-fixed-auto4k` 与 `taa-fixed-native720` 在同 4K 输出、普通 AA3／30 FPS、相同 seed 和实际 24 active engine ticks 下，各记录 256／256 帧 ready／completed／history reused、无拒绝 gate、零 jitter misses；松开输入后仍分别有 157／140 帧复用。各 source／TAA／depth 均有 12 帧，分别为 3840×2160／1280×720，无 allocation／CPU 回退。两组最终位置不同，不能声称相同轨迹、速度或 FPS 对照；相机探针误拒绝已在此限定范围闭环，完整运动画质、对象运动、更广场景和新版本用户验收不由这些检查证明。`taa-fixed-runtime-validation.json` 汇总上述证据及 7 项用户文件哈希不变；所有测试进程由 harness 结束，exit 1 不记作自然退出验证。

该次 TAA 修复开发包为 `out/v0.4.0-followup/packages-taa-fixed/LostOdysseyRecomp-windows-x64-c548b480-dev.zip`，41,418,024 字节，SHA256 `f88ea47ced4a6556f184cdf432919bf467d9e0ab28d87819604dca108ab7f094`；45 项 manifest、EXE 和两份编译 DLL 与受测 build 一致，安装器 self-test 返回 0，无游戏／用户数据。主代理读取 Auto 4K 进程模块，确认使用 build 自带的 DXC／DXIL；证据为 `package-taa-fixed-audit.json`。旧 `packages-final` ZIP 保持原样；新包仍为含未提交改动的本地开发包，未 commit／push／release。

Plume 两处纹理创建失败返回 null 已同步到主仓库补丁，原有 hunk 保留；从固定子模块 HEAD 的隔离 index／object store 应用后，Git 规范化 blob 与当前源一致，原始混合换行差异单独记录。XenonRecomp 补丁核验也通过，真实子模块源／index／HEAD 未改变。见 [依赖补丁说明](../../tools/patches/README.md) 和 `patch-sync-validation.json`。

### 既有 freeze 修复与本轮关联检索

2026-09-07 历史 freeze 对照澄清：旧 Map12／Map13 的“窗口未响应但画面仍更新”已有[窗口事件线程记录](window-event-pump.md)，相关营地／GPU 等待调查见 [third-map-hang](third-map-hang.md)；尚未从原 session 找到精确用户原话。当前 `video.cpp:274` 保留独立 `jthread`，本轮重读的受控 `WM_NULL` 证据为旧基线 6／6 超时、修正版 6／6 响应。Issue #5 的两处边界拆分（`config.toml:118–121`）及 56 项真实 dispatch 检查也已重新核对，但不能据此解释所有旧 freeze。本轮检索 `out` 下 1,705 个 `.log`（不跟随 junction，排除 `game*`／`generated*`）及原安装日志，未找到 `ctr=82afa388`／`82afd150` 警告；缺少匹配不否定用户报告，也不证明与 #5 同根因。本次仅检索和澄清，无代码变更、新实机验证、新用户验收或提交／发布。

### 此前交付与调查：v1–v3 证据保留

下节保留本次收尾前的构建、失败窗口和待办措辞；相机误拒绝及最新产物状态以上节为准。

| 工作 | 实现与证据边界 |
|---|---|
| CPX 定位索引 | 已实现完整压缩包 SHA256 身份校验、已知空包免解码、按 shader 所在独立块提取、未知／修改包完整扫描回退及去重免重复比较读取。独立应用冷缓存对照均输出 20,686 个同名且逐 SHA256 一致的来源；发现阶段 74.457 → 35.484 秒，解码字节减少 94.82%。未清空 Windows 文件缓存，不包含 DXIL 编译、PSO 或整体启动耗时。见[详细验证](shader-preparation.md#2026-09-07cpx-内置定位索引本地未发布)。 |
| 内部分辨率 | 设置和渲染实现已接入：Auto 按实际输出内的 16:9 区域选取，最高 3840×2160；手动 720p／1080p／1440p／2160p，输出分辨率独立。212 项配置／菜单检查、五语言 720p／4K 共 10 张八行 GDI 图，以及 37 项尺寸规则、16 个翻译 shader 的 DXC 编译通过。36 项 GPU 数值采样及非法宽度失败／随后正常分配恢复通过。build-v2 同 Map2／AA3／1080p 输出下的静止和移动实测，已验证实际 720p、Auto 1080p、4K 场景／深度／TAA 来源及历史复用，无 CPU／分配回退；最终 build-v3 Auto 4K 输出、八行图形页、预览／15.017 秒回退／Keep、实际 1440p 场景返回和同进程重开也已通过。新版本用户验收与更多场景／硬件仍待完成。 |
| [Issue #5](https://github.com/freefrank/LostOdysseyRecomp/issues/5) | 原函数范围吞并了间接调用入口 `0x82AFA388`，并发现同类入口 `0x82AFD150`。本地四盘镜像的边界指令一致；已拆分配置并重新生成入口，生成库构建通过。56 项真实生成 dispatch／分支／script cursor 检查通过。原报告的 USA, Europe 战斗存档未提供，仍待对应场景复测；不宣称整场战斗或报告版本已验收。 |
| [Issue #6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) | 附件仅 5 行，在游戏加载前报告 4 GiB guest 地址空间失败；没有失败阶段或 OS 错误码，尚无证据认定和 #5 同根因。已增加阶段、错误码／文本、映射上下文及系统内存诊断，保留原映射行为。186 项注入失败检查及真实 alias 分配／释放检查通过；原机器根因和恢复仍未知。 |

尺寸合同保留 guest 地址、pitch 与逻辑纹理尺寸，宿主颜色／深度、viewport／scissor、resolve 与 AA 输入使用物理尺寸。4K 内容为 3840×2160；常见 1280×736 带 padding 目标对应 3840×2208 存储，额外行不属于显示内容。方形 EDRAM 目标按分配策略保留原始 texel，超尺寸目标也回退原生；`LO_RESOLVE_READBACK` 明确强制原生内部尺寸，以保留 CPU guest 回写布局。

整合主程序和选定测试目标已构建成功，`build-v1` EXE SHA256 为 `F7A1BE32F483BF229F450B03B0B55B00F4FC72F5C1D6A865E90792A3F650DDF4`；presentation／temporal GPU fixture 通过，但不代表新的游戏场景画质验收。原生 720p 的应用启动实测中，CPX 发现为 34,132 ms、20,686 来源、10,060 命中／0 回退、20,339 重复包及 9,781,918,469 资源读取字节，与独立扫描对照一致；当时后续 DXIL 首次编译仍在进行。

实机场景对照使用 `build-v2`（SHA256 `15c7ea464501b448ffbf2b1dfd3c94980509e8cfcbd2762210375f801d1e5f72`）与相同 Map2 seed，在 AA3、30 FPS、1080p 输出下分别测试原生 720p、Auto 1080p、内部 4K。三组 source／depth／TAA trace 分别为 1280×720、1920×1080、3840×2160，记录 ready／history reuse、零 jitter misses，未触发 CPU／分配回退。主代理检查静止／移动输出：细杆和地表细节提高，UI 位置一致；天气和粒子状态不同，不能以整帧 MSE 证明收益。这些采集含 readback，不用于帧率收益结论。各组由主代理结束，原 seed 和 EXE 哈希不变；证据为 `native720-v2`、`auto1080-v2`、`internal4k-v2` 下的 `result.json`、日志与 captures。

首个 build-v1 曾因新增附件尺寸相等 guard 持续丢弃每帧 32 个 draw。该 guard 已移除，scissor 显式限制于两个附件的共同范围，build-v2 不再出现该持续丢弃。旧截图的透明保护效果不能单独证明该 guard 的视觉因果。最终 `build-v3` SHA256 为 `44bfd437c630817b2bcdff82f06344b9723b7ffc6ff06cccb6468cd032b86058`，另外修正 Plume 原生纹理创建失败返回非空包装的问题；非法 16385×1 宽度请求返回 null、随后 1×1 正常分配及 36 项 GPU 数值采样通过。这是无效请求检查，不是 OOM 压力测试。其 Auto 4K 实际输出／菜单验证已在下述范围完成。

时域日志复用并非每帧持续发生：`runtime-validation.json` 汇总原生 720p 为 175／189 条、Auto 1080p 为 161／176 条、手动 4K 为 246／256 条、最终 Auto 4K 为 108／256 条。最终窗口有 f1265–1405 连续 141 帧及另外 7 帧未复用，epoch／gap／ready／completed 保持正常、jitter misses 为零，也没有尺寸或目标分配变化。拒绝持续到输入释放之后，不能解释为正常按键期间拒绝。只读审计确认这是 CPU 整帧 HistoryOwner gate，现有 reason=0 属于 SceneObservation，未记录具体拒绝 gate 或前后 VP／raster／halfPixel／depth allocation，尚未找到直接因 4K 缩紧的阈值。单独 TODO：补精确历史拒绝诊断，按固定轨迹比较 720p／4K，原因确认前不放宽 guard；不阻塞已验证的内部尺寸交付，也不宣称 TAA 全面修复、持续累积或完整动画像素验收。该汇总同时确认四组程序／seed 哈希保持，以及用户目录的 7 个受保护文件逐 hash 不变。

最终 v3 的 `auto4k-final/result.json` 确认正常 AA3、3840×2160 输出和 Auto 模式，静止／移动 trace 的 source、TAA、depth 都为 3840×2160。实际简中八行图形页 `menu-validation/state-17.png` 无裁切；Auto → 1440p 预览时 `state-33.ini` 与 `before.ini` 逐字节相同。日志 191.960 秒生效，206.977 秒回退 Auto，间隔 15.017 秒，`state-35.png` 显示已还原。再次 Apply + Keep 后，`state-59.png` 显示已保存，INI 为 `internal_resolution=1440` 且 `debug_language=0` 保留。退出菜单的 `state-83.png` 为实际 Map2，`trace_f11737..11740` 三种 surface 均为 2560×1440；同进程重开的 `state-93.png` 仍显示 1440p。harness 主动终止进程（exit 1），seed／EXE 哈希不变；不记作程序自然退出或全游戏验收。

本轮开发 ZIP 为 `out/v0.4.0-followup/packages-final/LostOdysseyRecomp-windows-x64-c548b480-dev.zip`，41,415,021 字节，SHA256 `e3d5dd6ecec00d407677e871f80ec300377c38f8c936b838484b055eb20e3ee8`。45 项 manifest 文件哈希与依赖白名单检查通过，包内 EXE 与最终 v3 一致，安装指南逐字节匹配修正后的 `docs/INSTALLING.md`，`InstallGame --self-test` 返回 0，未打入游戏或用户数据；证据 `package-final-audit.json`。`c548b480-dev` 记录本地基础提交及开发包身份，包含本轮未提交改动，不是该 clean commit 的正式 Release。公开下载仍为 v0.3.0，未新增 commit／push／release 或玩家验收。补充运行最终 ZIP 解压的同 SHA256 EXE，进程模块确认加载包内且与 manifest 哈希一致的 `dxcompiler.dll`／`dxil.dll`，复制 Map2 存档在普通 Auto 1080p／AA3／30 FPS 设置下通过静止与短移动检查，source／TAA／depth 各 12 帧为 1920×1080、无 allocation fallback，7 项用户文件复核不变；证据为 `package-final-smoke/result.json`、`package-final-smoke/loaded-shader-libraries.json` 和 `package-final-audit.json`，使用任务自有暖缓存，不用于冷启动或 FPS 结论，ZIP 哈希保持不变。

本地证据集中在 `out/v0.4.0-followup/`：`cpx/validation.md`、`cpx/source-comparison.json`、`settings/cpu-test.log`、`issues/bounds-image-audit.json`、`issues/memory/validation.md`。用户 EXE、save、profile、settings、game-path 的保护清单为 `user-baseline.json`；本轮实机验证使用与用户安装隔离的产物和任务自有缓存；v2 场景对照共用该任务缓存，不属于冷 I/O 基准。当前进度以本节及 [STATUS](../STATUS.md) 为准。

下文保留开发开始前的同日交接。其“本次仅整理”“CPX 仍完整解码”“内部仍为 720p”及 v3 清单说明描述当时版本；本轮资源清单已经升为 v4，并增加来源列表完整性摘要。旧 PID、旧构建及旧验证窗口不代表当前运行状态。

## 历史交接：开发开始前

## 当前结论与优先级

**用户最新反馈：切换缩放质量和 TAA，肉眼看不出区别。画质收益尚未获用户验收。** 既有构建、执行链和限定像素验证继续有效，但完成标记不能代替用户实际场景的效果；下一轮重开该场景的有效性与可感知画质验收。

优先推进两项：① CPX shader 内置定位索引，减少首次资源发现；② 用户实际场景的 Standard/High、SMAA/TAA 诊断与同场景 A/B。最新待办在本地 `task_plan.md` 末尾；历史计划、旧 `handoff.md` 和旧 PID 不构成当前执行指令。

本次仅整理交接，没有实现、构建、运行游戏或提交。用户此前要求子代理实现、主代理审阅；下一轮按明确文件所有权分工，主代理整合。单 GPU 同时只交给一个代理，运行前后记录程序路径、PID、配置、缓存和清理责任，其他代理推进 CPU／源码／文档工作。

## 版本、用户目录与保护范围

- 当前提交：`c548b4802a3e96cdcfd93d0b5a30f68317be3b73`（`c548b48`，67 文件），仅本地提交，未 push／发布。正式公开版仍为 v0.3.0；此前 `c` 授权不包含推送或发布。
- 已验证 0.4.0-dev EXE SHA256：`81752F19551DC111F6083CCCCAF383BA7F863ED5AB657592B7FB9F3DA3E682EF`；本次再次读取用户 EXE，哈希一致。
- 本地 ZIP：`out/v0.4.0-delivery/packages/LostOdysseyRecomp-windows-x64-239e8516-dev.zip`，38,439,812 字节，SHA256 `1A3C51D84F8A46F43C26CECEC4579EC305BF489F0C09647EF63A87FFEA3F06AB`。
- ZIP 的 `239e8516-dev` 是打包时脏源码身份；不能因后来提交而改写 manifest／冻结身份，或将它描述为从当前提交重新构建的正式包。见本地 [交付说明](../../out/v0.4.0-delivery/README.md) 与 [验证记录](../../out/v0.4.0-delivery/package-verification.json)。

用户实际目录来自此前进程 63860／60472 的可执行文件路径，本次已按字面路径读取目录、设置与日志：

```text
D:\Mihoyo\LostOdysseyRecomp-windows-x64-2cd56708-dev`哈哈
```

反引号与“哈哈”在同一目录名，中间没有反斜杠，也没有 HTML 实体尾缀。PowerShell 使用单引号和 `-LiteralPath`；不要从消息转义文本猜路径。目录名里的旧版本片段不代表实际 EXE 版本。

交接时未发现运行中的 `LostOdysseyRecomp.exe`；上述 PID 仅是历史身份，下一轮重新发现。当前保存设置为 1920×1080、`antialiasing=3`（TAA）、`scaling_quality=1`（High）、`frame_rate=60`、`window_mode=0`，不能先归因于设置未保存。

保护用户目录的 `game`、`save`、`profile`、`settings.ini`、`game-path.txt` 和现有 `cache`；使用独立实验目录及存档／设置副本，变更前后核对哈希。冷缓存对照另建目录，不清理用户暖缓存，不覆盖可用 EXE。只清理本轮拥有的进程和临时产物，保留审阅证据。

仓库还有既有混合文档差异、`captures/`、根 `settings.ini` 及两个子模块的修改；不要还原、顺手提交或清空。选择边界见 [本地提交清单](../../out/v0.4.0-commit/docs-selection.md)。子模块适配已有 [受版本管理的补丁与说明](../../tools/patches/README.md)，不要将已应用补丁重复应用或重置子模块丢失修改。

## 优先一：CPX 内置定位索引

用户目录 `logs/runtime-1788796820756169.log` 第 54–56 行记录：

| 项目 | 实际结果 |
|---|---|
| 裸资源定位 | 52 indexed files，0 scanned files |
| CPX 工作量 | 30,399 packages，10,060 unique decoded |
| 读取／解码量 | 15,200,547,193 bytes read／7,668,866,392 decoded bytes |
| 资源发现 | 95,832 ms（95.832 秒），提取 20,686 shaders |
| 后续来源扩展 | 4 个静态 XEX、354 个固定 VS、1,891 个链接 VS；预编译列出 22,966 shaders |

`0 scanned` 只表示裸 FPD 没有回退全扫，不代表 CPX 没有扫描。现有内置索引覆盖裸 FPD shader；CPX 仍读取包、去重、解码并搜索包内内容。

预先生成并内置 CPX 包及包内 shader 的定位元数据，跳过不含 shader 的包和包内盲搜，并评估去重中的重复读取。只分发定位／身份元数据，所需内容仍从用户游戏资源提取；定位优化不等于免解压、免 shader 编译或免 PSO 创建。

相同资源版本、扫描范围和算法下，定位集合可复用，与显卡和存档无关；地区版本、补丁及资源修改按内容身份区分。索引命中必须校验身份，未知／修改版本保留正确回退；资源定位、DXIL 和 PSO 缓存不能混为同一种通用缓存。

当前 `cache/shaders/resources.manifest` 在发现结束时立即写入，不等进程退出。复用依赖缓存目录、`resource-scanner-v3-cpx`、FPD/FPI 绝对路径／大小／mtime，并逐个核对 source 文件 hash；大小／mtime 指纹不等于完整资源校验。移动资源路径可能失效。

最新只读补充：`logs/runtime-1788797266098889.log` 在 3.170 秒记录 `20686 shaders (reused)`、0 indexed／0 scanned／0 bytes read、1,910 ms。这已证明该次资源发现缓存复用；旧待办“尚未复启验证”是此前状态。计数 0 bytes 不包含 source hash 校验读取，1.910 秒也不是整个启动耗时；此结果不证明首次 CPX 发现已优化或后续编译／PSO 没有成本。

验收：独立冷缓存基线／候选的 shader 集合与 hash 完全一致，比较真实读取量、解码量、发现时间，再测暖缓存及损坏／未知身份回退。UI 分开显示缓存验证、按索引提取、回退扫描、编译；修复 CPX 条目数被当作 MB 显示的问题。

源码入口：[resource_index_types.h](../../LostOdysseyRecomp/gpu/shader/resource_index_types.h)；[resource_scan.h](../../LostOdysseyRecomp/gpu/shader/resource_scan.h) 的裸索引约 75–108 行、缓存约 151–184 行、CPX 约 220–256 行、manifest 写入约 262–267 行；[renderer.cpp](../../LostOdysseyRecomp/gpu/renderer.cpp) 的发现调用；[video.cpp](../../LostOdysseyRecomp/gpu/video.cpp) 的进度显示。行号仅对应本次读到的版本。

## 优先二：实际场景的 TAA 与缩放收益

`logs/runtime-1788797266098889.log` 初始 f0–255 的 temporal 记录均 `ready=false/completed=false`，此后没有该类记录；132.896 秒心跳为 1280×720 frontbuffer、319 draws/frame、60 FPS。代码的默认 temporal 日志只覆盖起始 256 帧，不能由此断言用户后续场景始终回退，也不能证明后续已启用 TAA。

Standard／High 当前仅为双线性／双三次空间重采样，guest 内部仍为 720p，不能增加原始场景细节。TAA 使用相机重投影，不满足场景条件时回退 SMAA，尚无原生对象／骨骼 motion vectors。

1. 复现用户实际场景与视角，确认正常菜单保存值、运行时分支及最终输出；不要只用强制实验开关演示另一路径。
2. 将 `LO_TEMPORAL_LOG_START_FRAME`、`LO_SCENE_AA_LOG_START_FRAME` 的有限诊断窗口移到目标场景；记录 ready／完成／fallback 原因、history reuse／reset 比例、jitter 和实际 source/output 尺寸，追到 TAA 结果是否真正用于最终显示。
3. 固定场景、输出分辨率和其他配置，分别做 Standard/High、SMAA/TAA A/B；观察静止文字／细线、相同相机运动中的闪烁、稳定性与拖影，保留原尺寸输出和有意义的局部图。
4. 区分分支未生效、场景覆盖不足、历史频繁失效和算法收益不足，再决定修复。配置成功、GPU dispatch、像素差分或放大差分图都不能代替可感知收益；完成后由主代理审阅并请用户复查该场景。
5. 性能窗口关闭截图／readback／PCM等会扰动时序的采集，画质证据与帧耗时证据分别记录；保留可用空间 AA 回退和基线。

源码入口：[renderer.cpp](../../LostOdysseyRecomp/gpu/renderer.cpp) 的场景选取、jitter、Resolve 和有限日志；[temporal_scene.h](../../LostOdysseyRecomp/gpu/temporal_scene.h)、[temporal_history.h](../../LostOdysseyRecomp/gpu/temporal_history.h)、[temporal_aa.cpp](../../LostOdysseyRecomp/gpu/temporal_aa.cpp)；[presentation.cpp](../../LostOdysseyRecomp/gpu/presentation.cpp) 的空间质量与合成；[video.cpp](../../LostOdysseyRecomp/gpu/video.cpp) 的 `DrawComposited` 最终输出。

## 已有证据与范围边界

- [开发记录](v0.4.0-development.md) 保留实现、失败原型及限定验证：宿主原生文字、Debug 双语／Capture 顶部、SMAA、空间质量、真实 Graphics 预览／回退／Keep／重开。既有功能事实不因新反馈全部作废，但用户场景画质收益仍未验收。
- TAA v7：本地 `out/v0.4.0-temporal/integrated-user-taa-v7/{validation.md,audit.json}`，64 组静止／移动记录、55 reuse／9 reset、95 次处理与最终 skip；不是用户当前场景或全场景质量证明。文字证据见 `out/v0.4.0-text/clarity-delivery-v7.md`。
- 60 FPS：移动、对白／音频、菜单和营地验证已完成；营地约 29 秒为 59.798 FPS。最终 Ring 长按约 7 秒观察为 59.962 FPS，核心计时一致；未测精准释放／Perfect，不比较不同敌人数／HP 的整场伤害或时长。见 `out/v0.4.0-fps/battle-v7-validation.md`，不宣称全游戏锁 60。
- 120 FPS 可延期，不阻塞交付。[DLSS/FSR 可行性研究](temporal-upscaling-feasibility.md) 已保留官方契约与当前深度／相机／jitter 证据；供应商后端、真实对象 MV、曝光／色彩空间仍是后续工程，不能宣称已支持 DLSS/FSR。
- 按 [测试选择说明](../../tools/tests/README.md) 只运行受改动影响的检查，不因文档交接重跑已通过的全部测试。每轮记录实际程序／源码身份，保留失败与回退条件，按需同步已验证结果和用户验收。
