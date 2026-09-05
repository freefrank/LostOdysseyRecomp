# GPU 当前说明（2026-09-05）

当前实现为 Xenos PM4/寄存器解释、microcode→HLSL→DXIL、plume D3D12 渲染。完整Vulkan运行尚未验证。历史实验已移至[GPU归档](../archive/2026-09-04/gpu.md)，其中被推翻的“遮挡已排除”等判断不能作为当前结论。

| 已验证的修复范围 | 依据 |
|---|---|
| 开场角色黑色剪影、物理别名 | [内存别名](physical-alias-rendering.md) |
| 索引对齐、后期尺寸、gamma/swizzle | [几何与resolve](rendering-index-and-resolve.md) |
| stencil、高光及D24清理端点 | [光影](lighting-stencil-depth-clear.md) |
| 战后实时演出白屏 | [格式转换](post-battle-whiteout.md) |
| 标题动态背景 | [packed mip](title-packed-mips.md) |

**未解决：**人物自阴影闪烁、遇敌影子缺失/闪烁、火焰黑红格子、Ring外环、箱子破坏黑色特效。缺失polygon offset是代码线索，尚无对应修复/视觉验证。遮挡计数仍近似，不宣称逐像素一致。

保持同一场景、镜头和环境开关，每次只改一个变量。火焰不能以Xenia异常输出作为正确基线。GPU停帧诊断见[营地调查](third-map-hang.md)；测试命令和缓存基线见[渲染验证](rendering-validation.md)。总体状态见[总表](../STATUS.md)。
