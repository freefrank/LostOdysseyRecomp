# DLSS 5 NR 接入研究

日期：2026-10-08。源码、SDK、驱动和外部实现的核对结果，以及 Vulkan 实现的依据。

## 结论

- NR 是 Neural Rendering：放在超分之后、同分辨率的生成式图像阶段。它不是超分预设，也不是降噪器。
- NVIDIA 没有公开头文件，但官方调用方式是清楚的：驱动的 NGX core 用 feature 18（SDK 里的 `NVSDK_NGX_Feature_Reserved18`）创建 NR，并自己加载签名的 `nvngx_dlssnr.dll`。Streamline 的 `sl.dlss_nr` 插件走的也是这条路。
- 能拿到的只有社区改版，签名已失效，驱动不加载。所以 Vulkan 实现自己加载 DLL，调它导出的 `NVSDK_NGX_VULKAN_*`，做法同 Vapourkit。
- 不需要 D3D12 桥。在现有 NGX 会话里，SR evaluate 之后用 feature 18 再做一次 evaluate。
- 不打包 `nvngx_dlssnr.dll`，由用户自备。

## NR 本身

- NVIDIA 研究页：输入当前帧、引擎 MV、时序状态和风格参数，输出最高 4K 的生成帧，官方只支持 RTX 50。
- OpenDLSS-NR 复现了 310.8.0 版网络：71 个 block 的 U-net（shifted-window transformer + 底部全局 ViT），FP8 激活，141 MiB 权重。输入输出同分辨率，网络读的是 sRGB 编码的 LDR 图和上一帧重投影结果。
- 引擎侧只需要颜色、深度、MV。颜色必须是 0–1、带 sRGB 曲线的显示编码值，线性 HDR 会出错。
- 开销大。OpenDLSS-NR 在 4070 SUPER 上：1080p 7.8 ms，1440p 12.6 ms，4K 29.3 ms（这是复现版，不是 NVIDIA 运行库）。

## 官方调用方式

| 来源 | 内容 |
| --- | --- |
| Streamline 2.14.1 `sl_core_types.h:253` | `kFeatureDLSS_NR = 1004`，以及 Uplift 三个 buffer 类型（70–72）；公开包里没有 `sl_dlss_nr.h` 和 `sl.dlss_nr.dll` |
| DLSS SDK 310.9.1 `nvsdk_ngx_defs.h:224` | `NVSDK_NGX_Feature_Reserved18 = 18` |
| NVIDIA 签名的 `sl.dlss_nr.dll` 2.13.0.0（随 DLSS 5 Swapper 分发） | 源文件名 `dlss_nrBackendNGX.cpp`；通过 `sl.param.global.ngxContext` 走 NGX core；完整的 `DLSSNR.*` 参数名；报错文本是"check if you have a valid nvngx_dlssnr.dll or your driver supports DLSS-NR" |
| 驱动 616.56 的 `_nvngx.dll` | 含 "DLSSNR" feature 名；snippet 用 `nvLoadSignedLibraryW` 和 `WinVerifyTrust` 加载 |

`sl.dlss_nr.dll` 里的参数名：`DLSSNR.Width/Height/Hint.Render.Preset`；资源 `DLSSNR.Color/Output/Depth/MVec/ControlMask/Backbuffer/UI/UIAlpha/BidirectionalDistortionField`；每个资源各有 `SubrectBaseX/BaseY/Width/Height`；`DLSSNR.MVecScaleX/Y`、`DepthInverted`、`Intensity`、`LocalToneStrength`、`LocalStructureStrength`、`GlobalToneStrength`、`UseAutoMask`、`SkinStructureStrength`、`Style`、`Reset`、`UICorrection`。

NGX 按设置时的类型保存参数，读取时类型不一致会读到默认值。类型沿用 Vapourkit 的 Vulkan 实现：Width/Height、Style 用 unsigned；Enabled、Preset、DepthInverted、Reset、UseAutoMask、UICorrection 和 subrect 用 int；Intensity、各 Strength、MVecScale 用 float。

## 调用方检查

`nvngx_dlssnr.dll` 只接受模块名为 `nvngx.dll` 的调用方：它用 `RtlPcToFileHeader` 找到调用方模块，再用 `GetModuleFileNameW` 读名字。两种改版都还导入这两个函数，带着 `nvngx.dll` 字符串，检查仍在。

走驱动 core 时调用方就是驱动的 nvngx，自然通过。改版只能由程序自己调，社区有两种做法：Vapourkit 改 snippet 的 `GetModuleFileNameW` 导入，Dagherbou 另做一个名字含 `nvngx.dll` 的转发 DLL。我们用前一种：只在内存里改这一个导入槽，只对本 exe 模块报 `nvngx.dll`，其他查询照常交给系统，DLL 文件不动。

## 四个仓库

| 仓库 | 是什么 | 对我们的用处 |
| --- | --- | --- |
| [OpenDLSS-NR](https://github.com/maanHimself/OpenDLSS-NR) | MIT，C++/Vulkan 逐位复现网络；要 Ada 以上和 NVIDIA 专有扩展；权重自备 | 只做参考 |
| [dlss-unlocked](https://github.com/ShyVortex/dlss-unlocked) | 安装包：OptiScaler DLSSNR 分支 + 改过的 `nvngx_dlssnr.dll` + Streamline 2.14.1，给 20/30/40 系用 | 不照搬，它的 release 自带 NVIDIA 文件 |
| [DLSS5-Swapper](https://github.com/rakanki911/DLSS5-Swapper) | Electron 启动器，装 ReShade、DLSS5-Feeder、RenoDX、OptiScaler DLSS-NR | 它带的签名 `sl.dlss_nr.dll` 给出了官方参数名 |
| [DLSS-NR-on-AMD](https://github.com/danielblnc/DLSS-NR-on-AMD) | 闭源安装包，挂 FSR 3/4 路径，HIP 自写推理 | 闭源，无法集成 |

本机的几份 `nvngx_dlssnr.dll`（310.8.0.0）全部签名失效，分两种改版：`8270B350…`（Swapper 2.2.9、reblue，9 月 2 日起）和 `4B8D19BC…`（ryujinx、Downloads、nds，8 月 30 日）。两者在 GPU 架构分派代码和压缩的 kernel 数据上不同，都不是 NVIDIA 原版。NGX OTA 缓存里也没有 DLSSNR。测试用较新的 `8270B350…`。

## 实现

设置 → 图形的“DLSS 5 神经渲染”（`dlss_neural_rendering`，0 关闭，1–4 为 pass 数）打开，只在 Windows、开 DLSS 时显示。代码在 `gpu/dlss_ngx.cpp`（共用部分和 Vulkan）、`gpu/dlss_ngx_d3d12.cpp`（D3D12）和 `gpu/dlss_nr.h`：

- DLL 默认在 SR 运行库旁边（`nvngx_dlss.dll` 所在目录），也可以用 `LO_DLSS_NR_PATH` 指定完整路径。进程内只加载一次，不卸载。
- pass 数是 `SrConfig` 的一部分：改设置后 SR 和 NR 一起在排空点重建，feature 在一帧里创建，下一帧开始 evaluate，第一次带 Reset。输出尺寸变化同理。
- 多 pass：每帧连跑 N 次，每次以上一次的输出为输入，每个 pass 用自己的 feature（时序状态互不干扰），两张 FP16 图来回交替，同一帧所有 pass 用同一个 Reset。第二个 pass 起 `LocalToneStrength` 为 0，照 wilsjo2 的多 pass 实现的默认值，没针对本游戏调过。社区实现最多 3（解锁 30）或 10 次，NVIDIA 没有对应参数。
- MV 用渲染分辨率像素、`MVecScale = 1`，subrect 用渲染尺寸，和 SR 一致。
- 菜单帮助行显示状态：运行中、找不到 DLL、显卡/驱动不支持、出错停止（`gpu/dlss_nr_state.h`）。NR 的任何失败只关掉 NR，不影响 SR；下一次排空重建会再试一次。

Vulkan：

- 建设备时如果 DLL 在，就把 `VK_NVX_binary_import`、`VK_NVX_image_view_handle`、`VK_KHR_push_descriptor`、`VK_KHR_buffer_device_address` 并入 SR 的扩展列表；DLL 不在或设备缺任何一个就不开 NR，SR 和以前完全一样。游戏运行中才放入 DLL 的要重启。
- snippet 用 `Init_Ext2` 初始化。参数顺序以反汇编为准：`(…, GIPA, GDPA, Version, const NVSDK_NGX_Parameter*)`，和 SDK 头文件一致。第一版和社区实现把最后两个参数传反了（版本 0、指针 0x15），碰巧能跑；现在改为 `(NVSDK_NGX_Version_API, nullptr)`。
- SR evaluate 之后，在同一个隔离 list 里：SR 输出 blit 到 FP16 图，NR 各 pass evaluate，最后一个 pass 的结果 blit 回 SR 输出。每一步之间加全局 memory barrier，图像都留在 GENERAL。

D3D12：

- 导出 `NVSDK_NGX_D3D12_Init_Ext(appId, dataPath, device, Version, const NVSDK_NGX_Parameter*)`（反汇编确认，第 4 个参数按 32 位读）、`CreateFeature`、`EvaluateFeature`、`ReleaseFeature`、`Shutdown1`。资源用 `SetD3d12Resource`。
- D3D12 没有能转格式的 blit：SR 输出（RGBA8）先 `CopyResource` 到同格式的中转图，一个小 compute shader 转成 FP16 给 NR，结果再转回中转图、拷回 SR 输出（保留原 alpha）。shader 运行时由 DXC 编译（`SetComputeShaderCompiler`），描述符集固定绑定中转图和 FP16 图。
- 拷贝、转换和状态切换都走 plume，状态跟踪保持一致；NR 输入是 SHADER_READ，输出是 UAV，结束时 SR 输出回到 UAV/GENERAL。每次 NGX 调用后 `invalidateCachedNativeState()`，plume 才会重新绑定自己的 descriptor heap。

HDR：

- 开 HDR 输出时，DLSS 的输入仍是 8 位、显示编码的场景（`renderer.cpp` 里 DLSS 的 `qualifiedEncoding` 只会是 `Sdr`），高光由呈现阶段用超分前的 FP16 场景按亮度增益加回来。所以 NR 在 HDR 下照常运行，不需要额外的色调映射。
- `SrColorSpace::Linear` 的路径在本游戏里不会出现，遇到时跳过 NR 并记日志。

## 风险

- 官方只支持 RTX 50。20/30/40 系是否能用取决于用户手里的改版，我们不保证。
- 4K 下单帧开销可能有十几到几十毫秒。
- `nvngx_dlssnr.dll` 的再分发条款没找到。

## 来源

- [NVIDIA DLSS 5 研究页](https://research.nvidia.com/labs/adlr/DLSS5/)
- [Streamline](https://github.com/NVIDIA-RTX/Streamline)（`changelog.txt` 2.14.0：新增 `sl.dlss_nr` 插件）
- [DagorEngine streamline_adapter.cpp](https://github.com/GaijinEntertainment/DagorEngine/blob/master/prog/engine/drv/drv3d_commonCode/streamline_adapter.cpp)
- [Vapourkit vsdlssnr](https://github.com/Kim2091/vapourkit)、[Dagherbou/OptiScaler_DLSSNR](https://github.com/Dagherbou/OptiScaler_DLSSNR)（参数类型、MV 约定、caller 检查）
- [Guru3D：610.47 驱动的 DLSS-NR 配置项](https://guru3d.com/story/nvidia-geforce-61047-driver-quietly-adds-first-dlss-5-neural-rendering-profiles/)
- 上表四个仓库
