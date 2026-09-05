# 接手入口（2026-09-04）

## 当前状态

Windows D3D12 已验证标题、菜单、开场战斗、攻击目标选择及攻击后镜头切换。
本轮依次修复：

| 问题 | 根因与实现 | 详细记录 |
|---|---|---|
| 角色黑色剪影 | A/C 物理地址未共享，客体读不到 GPU 遮挡结果；E 别名偏移一页 | [物理内存别名](physical-alias-rendering.md) |
| 角色网格破面 | 16 位索引 DMA 起点被错误按 4 字节对齐 | [几何与后期](rendering-index-and-resolve.md) |
| 后期轮廓偏移 | 428×242 的纹理按 448×242 行距资源采样 | [几何与后期](rendering-index-and-resolve.md) |
| 材质颜色偏差 | 缺失纹理 gamma、fetch swizzle 和 resolve R/B 配套解释；补齐 ALU 并行读语义 | [着色器与纹理](rendering-index-and-resolve.md) |
| 金属高光缺失 | stencil 状态未接通、plume 参考值丢失、D24 清理端点溢出 | [光影](lighting-stencil-depth-clear.md) |

Shader cache 版本为 v19。保持正常深度、客体遮挡开关及景深，没有用曝光或强制颜色写掩码补偿。
GPU 遮挡计数仍是已有的近似实现，不能当作真实硬件查询结果。

## 验证入口

- [构建、回归测试与实机复现](rendering-validation.md)
- [依赖补丁应用方式](../../tools/patches/README.md)
- [Xenia 对照条件和原始观察](xenia-render-comparison.md)
- 本地最新证据：`out/render-light-depthpack/shot_2400.png`、`shot_3000.png`。
- 本地对照页：`out/xenia-comparison/comparison-lighting.html`。

`out/` 中的日志、游戏截图、捕获数据不随仓库分发。角色待机动作不同，当前对照不支持逐像素一致或性能提升的结论。

## 下一步

1. 继续以 Xenia 同场景对照检查阴影边缘、局部光照和后处理，保留本轮基线，每次只改一个变量。
2. 验证战斗结束、后续场景及存档；目前未验证通关。
3. plume 只提供一组 stencil reference/read/write mask；当前场景没有启用双面且两组值不同的 draw，其他场景需要继续检查。
4. 完整 Linux/Vulkan 运行、音频真解码、WMV 和四盘合并仍未完成，见 [路线图](../ROADMAP.md)。

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

[gpu.md](gpu.md) 按时间保留早期分析，其中“遮挡查询已排除”等判断已经被后续证据推翻。
以本页和各专项笔记中的最终验证为准，不再重复禁用遮挡或强开 mask 的实验。

项目逆向技能入口为 `tools/reverse-skill/README_AI.md`；Ghidra 导出脚本为 `tools/ghidra/ExportFunctions.java`。
