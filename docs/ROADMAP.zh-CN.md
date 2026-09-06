# 路线图

[English](ROADMAP.md) · [当前状态（英文）](STATUS.md)

更新于 **2026-09-06**，以 v0.2 和最新用户验收为准。`[x]` 表示所述范围已完成验证，不代表全游戏通关。历史调查记录保留当时状态；当前进度以 [STATUS.md](STATUS.md) 为准。

状态：`[ ]` 计划／待完成 · `[~]` 进行中 · `[x]` 已在所述范围验证。

## 待发布：v0.2.1

候选版增加 F1 下一帧渲染捕获及自动 ZIP、SDL 多手柄与键盘混合输入及 E/R 扳机。本地构建和限定范围测试通过，托管发布验证与正式发布尚待完成。这不是 AMD 渲染修复。见[捕获](notes/render-state-capture.md)和[输入](notes/controller-input.md)。

## 最新里程碑：v0.2

v0.2 已发布，包含两套已核对版本、按版本提供的文本／语音选项、先导入后设置及自动读取已导入盘。对应 [Redump 11817：USA, Europe，0.0.0.3](https://redump.info/disc/11817) 与 [Redump 39111：Europe, Asia，0.0.0.4](https://redump.info/disc/39111)。导入仍严格核对 XEX 哈希，不代表完成整张 ISO 的 Redump 哈希比对。两版原管理器均通过 1 → 2 → 3 → 4 → 1 受控流程，章节交界剧情仍未覆盖。见 [v0.2 发布说明](RELEASE-v0.2.md)和[自动选盘](notes/disc-selection.md)。

v0.1 保留为首个 Windows 发布里程碑，包含导入器、首次设置、五语言界面、着色器索引／并行准备及对白、脚下投影、海报和窗口响应修复。见 [v0.1 发布说明](RELEASE-v0.1.md)。

**语音问题已解决，用户已确认验收。** XMA 续接包处理现在保留新帧，不改变采样率或音量。装甲车对白从 2,129 帧恢复到 4,062 帧，与原始音轨的时间斜率从 0.547 恢复为 1.000；另外 34 个多声道缓冲、4,264 帧解码无错误。其他场景的音频属于常规回归覆盖，不再将已解决的对白问题列为待修复。见[音频证据](notes/audio-output.md)。

## 近期优先事项

1. 定位偶发 GPU query/wait 故障和长时间运行稳定性。
2. 修复火焰受击格子及箱子破坏黑色，解决两个资源着色器编译失败。
3. 扩展两版章节交界、存档读回、遇敌和多语言回归。自动路径切换已实现，真实剧情衔接仍需覆盖。
4. 完成全屏／独占、鼠标、跨 DPI 验收。Map 13 阴影改善只做受控回归，不重开为已确认缺陷。

现代图形功能后续再做。**用户于 2026-09-06 暂停文本语言互补补丁研究**：无成品、无运行时代码改动、不在 v0.2 中；语音不在该研究范围。见[挂起研究](notes/text-language-patch.md)。

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
- [ ] Map 13 人物表面阴影回归：用户反馈改善并满意，受控回归待做，尚未证明与海报同因。
- [ ] 火焰受击黑红格子，以原机画面为参考。
- [x] Ring 外环：测试中正常变化，释放 RT 获得 Good 和 101 伤害，用户确认手柄操作正常。见[记录](notes/battle-ring-resource.md)。
- [ ] 第二地图箱子破坏特效黑色。
- [x] Debug 地图 ID／本地化名称：标题未知状态、Hypocenter/Gorge 和 2→3 切图已测。见[地图](notes/debug-map-info.md)。
- [~] 随时存档：非存档点槽 03 保存／重启读回、权限恢复、原生存档点和营地切换通过；F1 开关有隔离尺寸／回调测试，其他桌面和游戏流程待验。见[记录](notes/save-anywhere.md)。
- [~] 营地／窗口无响应：大小端临界区归属和递归修复通过四线程及选定流程测试，窗口消息泵修改已发布；独立 GPU query/wait 指针损坏仍待修。见[临界区](notes/critical-section-endian.md)与 [GPU 等待](notes/third-map-hang.md)。
- [x] 语音问题已解决并经用户确认，输出及对白时序修复已发布；营地→装甲车→城门流程不再出现此前稳定解码错误。其他场景音频属于常规回归，长时间运行稳定性单独跟踪。见[音频](notes/audio-output.md)。

## 阶段 4：现代化

优先兼容性和稳定性，未来图形功能尚无承诺发布日期。

- [x] 按逻辑线程数减一、最少一线程并行预编译；测试机 15 worker 将 2,000 个着色器准备时间从 53.4 秒降至 6.7 秒，成功产物逐字节一致。
- [x] 已知着色器启动准备、持久缓存和进度界面；早期 184 微码集合的热缓存复用和损坏 DXIL 恢复通过。见[着色器准备](notes/shader-preparation.md)。
- [x] 四盘发现 2,000 个微码，1,998 个编译成功；进度、缓存复用和独立 Map 12 实跑通过。
- [x] 内置 52 文件索引，未知布局保留扫描回退；本地发现阶段从 36.2 秒降至 1.1 秒，此耗时与编译耗时分开计算。
- [ ] 修复两个微码编译失败，覆盖其他容器／运行时变体及实际 PSO 预创建；新场景仍可能首次卡顿。
- [x] 保留原选项并替换设置，增加五语言界面、语言选择、FXAA 和等比例输出缩放。见[设置](notes/settings-menu.md)。
- [ ] 完成全屏／独占、鼠标和混合 DPI 桌面验收。
- [ ] 增加 FXAA 之外的抗锯齿选项，研究时域 AA。
- [ ] 更高内部渲染分辨率、渲染缩放和宽屏，修正 UI 布局。
- [ ] 研究 DLSS/FSR 等超分辨率；当前输出缩放并非时域超分，v0.2 的 DLSS 为禁用占位。
- [ ] 研究帧生成 FG；v0.2 为禁用占位。
- [ ] 解锁帧率，定位固定 30 fps 的逻辑。
- [ ] HDR 输出和色调映射。
- [ ] 更高分辨率阴影。
- [ ] 研究 SSAO，为 ReShade 提供干净深度缓冲。
- [ ] 实现并验证 Vulkan 游戏运行及 Steam Deck 支持。
- [x] 托管 Windows CI 发布 v0.1，包含导入器和首次运行设置。见[安装](INSTALLING.md)与[打包](notes/release-packaging.md)。

## 阶段 5：可选探索

- [ ] 屏幕空间 GI / SSR。
- [ ] 硬件光追阴影和反射。
- [ ] 高清贴图替换与 mod 加载器。
