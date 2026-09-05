> **历史归档 / Historical snapshot — 2026-09-04.** 保留当时的调查和旧假设，不代表当前状态。先读[当前状态](../../STATUS.md)和[接手入口](../../notes/handoff.md)。

# 接手入口（2026-09-04）

## 当前状态

后台攻略推进新修复：宽printf资源名称语言后缀、82B146A8遗漏switch，两项联合回归已正常完成戒指战斗并进入峡谷，见 [戒指调查](../../notes/battle-ring-resource.md) 与 [攻略测试](../../notes/walkthrough-testing.md)。F1人物传送已实现，后台位移/记录点返回/恢复行走与菜单禁用已验证，见 [传送说明](../../notes/debug-teleport.md)。Gorge保存/读回尚未完成。


用户确认 debug 判胜可跳过战斗、修复后手动存档成功；独立进程已从存档副本进入地图，见 [存档调查](../../notes/save-storage.md)。后续遇敌的主角 T 姿势与攻击停滞已修复：正常开场 82B003EC 将持久资源 11→0，debug 判胜补齐此转换；0x139 对遗留存档做限定恢复。独立副本已自然完成战斗并覆盖保存。敌人地图姿态和光影闪烁仍待查，见 [遇敌调查](../../notes/encounter-animation.md)。用户正在使用电脑，只允许后台运行及进程内测试输入，不使用前台窗口或系统键鼠。

Windows D3D12 已验证标题、菜单、开场战斗、攻击目标选择及攻击后镜头切换。
最新续修已通过首场战斗后的实时演出并进入重型坦克战斗，见 [战后白屏修复](../../notes/post-battle-whiteout.md)。
本轮依次修复：

| 问题 | 根因与实现 | 详细记录 |
|---|---|---|
| 标题动态背景纯黑 | 小尺寸纹理 level 0 位于 packed mip 尾部，上传忽略块偏移 | [标题背景](../../notes/title-packed-mips.md) |
| 角色黑色剪影 | A/C 物理地址未共享，客体读不到 GPU 遮挡结果；E 别名偏移一页 | [物理内存别名](../../notes/physical-alias-rendering.md) |
| 角色网格破面 | 16 位索引 DMA 起点被错误按 4 字节对齐 | [几何与后期](../../notes/rendering-index-and-resolve.md) |
| 后期轮廓偏移 | 428×242 的纹理按 448×242 行距资源采样 | [几何与后期](../../notes/rendering-index-and-resolve.md) |
| 材质颜色偏差 | 缺失纹理 gamma、fetch swizzle 和 resolve R/B 配套解释；补齐 ALU 并行读语义 | [着色器与纹理](../../notes/rendering-index-and-resolve.md) |
| 金属高光缺失 | stencil 状态未接通、plume 参考值丢失、D24 清理端点溢出 | [光影](../../notes/lighting-stencil-depth-clear.md) |
| 战后演出整屏白色 | 场景恢复后以另一格式继续叠加光照，draw 前缺少 EDRAM 内容转换 | [战后白屏](../../notes/post-battle-whiteout.md) |

Shader cache 版本为 v19。保持正常深度、客体遮挡开关及景深，没有用曝光或强制颜色写掩码补偿。
GPU 遮挡计数仍是已有的近似实现，不能当作真实硬件查询结果。

## 验证入口

- [构建、回归测试与实机复现](../../notes/rendering-validation.md)
- [依赖补丁应用方式](../../../tools/patches/README.md)
- [Xenia 对照条件和原始观察](../../notes/xenia-render-comparison.md)
- 本地最新证据：`out/render-light-depthpack/shot_2400.png`、`shot_3000.png`。
- 本地对照页：`out/xenia-comparison/comparison-lighting.html`。
- 最新战后演出证据：`out/cg-white-transfer/`；对照页 `out/xenia-comparison/comparison-cg.html`。

`out/` 中的日志、游戏截图、捕获数据不随仓库分发。角色待机动作不同，当前对照不支持逐像素一致或性能提升的结论。

## 下一步

主角火焰受击在 Xenia 中也有 glitch（用户已确认），不能把该效果当作正确基线；需要独立取证或原机参考。攻击力 debug menu 已登记在 [需求说明](../../debug-menu-requirements.md)，尚未实现。

1. 继续以 Xenia 同场景对照检查粒子、光晕、阴影边缘和后处理，保留本轮基线，每次只改一个变量。EDRAM 转换现在默认 draw + resolve；read 是历史错误行为对照。
2. 验证重型坦克战斗结束、更后续场景及存档；目前未验证通关。
3. plume 只提供一组 stencil reference/read/write mask；当前场景没有启用双面且两组值不同的 draw，其他场景需要继续检查。
4. 完整 Linux/Vulkan 运行、音频真解码、WMV 和四盘合并仍未完成，见 [路线图](../../ROADMAP.md)。

## 代码地图

路径相对于 `LostOdysseyRecomp/`：

- `main.cpp`：加载客体、启动 GPU/APU 和重编译入口。
- `kernel/guest_address_space.cpp`、`memory.cpp`：共享物理映射和页分配；`xex_loader.cpp`：镜像与导入。
- `gpu/command_processor.cpp`：PM4、寄存器、DRAW_INDX、resolve 和同步。
- `gpu/renderer.cpp`：plume D3D12、EDRAM、资源缓存、管线状态及诊断捕获。
- `gpu/shader/xenos_translator.cpp`：Xenos microcode → HLSL；`dxc_compiler.cpp`：DXIL 编译。
- `gpu/depth_format.h`：D24 打包边界处理。
- `gpu/video.cpp`、`hid/hid.cpp`：呈现、截图及自动输入。
- `apu/audio.cpp`、`apu/xma.cpp`：当前音频占位实现。

## 历史调查的阅读顺序

此前调查停在蒙皮 draw 普查。续接发现相对寻址候选的
75 次 mode 4 draw 中有 74 次关闭颜色写入，不能把它们称为 75 次角色颜色绘制。
随后追踪客体提交条件，确认 GPU 写 A 别名、GetData 读 C 别名才是黑色剪影根因。

[gpu.md](../../notes/gpu.md) 按时间保留早期分析，其中“遮挡查询已排除”等判断已经被后续证据推翻。
以本页和各专项笔记中的最终验证为准，不再重复禁用遮挡或强开 mask 的实验。
项目逆向技能入口为 `tools/reverse-skill/README_AI.md`；Ghidra 导出脚本为 `tools/ghidra/ExportFunctions.java`。

人物传送已完成首轮后台验证：实际位移、记录点返回、输入绝对坐标、恢复普通行走、游戏菜单/戒指教学/战斗禁用均通过，见[传送记录](../../notes/debug-teleport.md)。重启使用最终运行时后按F1；所有本轮游戏副本位于out/，原始save/profile保持不变。

POI扩展已完成：F1传送区增加当前已加载地图的兴趣点列表，含存档、出入口、机关和拾取点，显示落点及距离。后台 `out/poi-landing-verified/` 已验证Hypocenter的20点枚举、机关附近碰撞搜索、返回存档点、出口附近、游戏菜单禁用及旧编号拒绝。最终构建 `out/poi-final-build.log` 通过；窗口未前台打开，未验证全游戏地图。结构偏移、候选落点策略、日志时间和限制见[传送记录](../../notes/debug-teleport.md)。
