# 光照 / 阴影续修（2026-09-04）

## 对照基线

上一轮 `out/render-final/shot_2400.png` 和 `shot_3000.png` 已修几何、gamma 与后期偏移，
但金属高光、暗部和 Xenia 不一致。本轮保留这些基线，不修改曝光或添加补光。

## 缺失的模板测试

第 2400 帧记录 1336 个实际 draw，其中 438 个 RB_DEPTHCONTROL.stencil_enable=1。
原 GetPipeline 仅设置 depth，并把管线 key 的 DEPTHCONTROL 截成低 7 位，丢掉模板比较和更新操作。
现在保留全控制字，并设置正反面比较、fail/zpass/zfail、read/write mask 和 reference。

同时修正寄存器地址：RB_STENCILREFMASK_BF=0x210C，正面 RB_STENCILREFMASK=0x210D；
原代码把 BF 错标为正面。对照本地 Xenia register_table.inc、registers.h、xenos.h。

plume D3D12 还有独立缺陷：setPipeline 会使用 pipeline.stencilRef，但构造函数没有从
RenderGraphicsPipelineDesc.stencilReference 赋值，导致 reference 恒为 0。已补齐该赋值，
并同步 tools/patches/plume-lostodyssey.patch；保留原有 readback 安全补丁。

## 光照缓冲清理时的 D24 溢出

本游戏用 depth-only rectangle 写 EDRAM 来清理颜色缓冲。移植版将 depth/stencil
合并成 32 位字，再用颜色格式解释它；这条路径会影响光照衰减纹理。

原公式 `uint32_t(depth * 16777215.0f + 0.5f)` 在 depth=1 时，用 float 舍入得到
0x1000000，而不是 0xFFFFFF；左移 8 位后所有深度位消失。
配合此前错误的 stencil reference 0，白色清理值变成黑色，光照被错误压低；
纠正模板 reference 为 255 后又变为纯红。这是中间实验 `render-light-stencil-ref/`
出现红色偏光的原因，不能把它当成最终修复结果。

新增 `gpu/depth_format.h` 的 PackDepth24Unorm：端点饱和，内部使用 double 中间值，
确保 1.0 打包为 0xFFFFFF，最终带 stencil=255 的字为 0xFFFFFFFF。

## 验证

`LoStencilTest` 不使用游戏资产，通过 plume 在真实 D3D12 渲染 8×1 图：
先只向左半区域写 stencil=3，然后分别以 EQUAL / NOT_EQUAL 画红色和蓝色。
GPU 回读左 4 个像素为红，右 4 个为蓝，PASS。
该测试会捕获 plume 丢失非零参考值的问题（旧实现会把所有像素画红）。

同一测试静态验证 D24 的 0、0.5、1.0、紧邻 1.0 的 float，以及完整白色清理字。
运行时已编译。最终实机复跑目录：`out/render-light-depthpack/`。

本场景未触发正反面不同模板 mask/reference 的告警；plume 当前接口只提供一组值，
该类其他场景仍需要独立支持。未改亮度参数，也未关闭深度或阴影。

## 实机结果

`out/render-light-depthpack/shot_2400.png` 的角色肩甲、腰带、士兵武器和铠甲高光恢复，
阴影区域保持暗部；中间实验的红色偏光消失。未调曝光、gamma 指数或材质参数。
此图与上一轮 `out/render-final/shot_2400.png` 及 Xenia 同机位目标列表对照。
待机动作有差异，不进行逐像素误差或性能比较。

第 3000 帧正面近景也确认肩甲、护腕、剑刃和腿甲高光恢复，暗部保留。
对照页：`out/xenia-comparison/comparison-lighting.html`。
最终运行日志未见 shader 编译错误或不同正反面 mask 警告；`git diff --check` 通过。
尚未宣称所有场景及每项阴影细节与 Xenia 完全一致。
