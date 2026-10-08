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

## Vulkan 实现

在 `gpu/dlss_ngx.cpp` 的 `Controller` 里，`LO_DLSS_NR=1` 打开，只在 Windows：

- DLL 默认在 SR 运行库旁边（`nvngx_dlss.dll` 所在目录），也可以用 `LO_DLSS_NR_PATH` 指定完整路径。进程内只加载一次，不卸载。
- 建设备时把 `VK_NVX_binary_import`、`VK_NVX_image_view_handle`、`VK_KHR_push_descriptor`、`VK_KHR_buffer_device_address` 并入 SR 的扩展列表；设备缺任何一个就不开 NR，SR 照常。
- core 会话起来后，snippet 用 `Init_Ext2`（app id 0，显式传 `vkGetInstanceProcAddr`/`vkGetDeviceProcAddr`）初始化；参数袋取自 core 的 `GetCapabilityParameters`。关闭时先 snippet `Shutdown1`，再关 core。
- SR evaluate 之后，在同一个隔离 list 里：SR 输出 blit 到 FP16 输入图，NR evaluate 写 FP16 输出图，再 blit 回 SR 输出。之后的合成不变。每一步之间加全局 memory barrier，图像都留在 GENERAL。
- feature 在一帧里创建，下一帧开始 evaluate，第一次带 Reset。输出尺寸变化跟着 SR 的排空重建一起释放和重建。
- MV 用渲染分辨率像素、`MVecScale = 1`，subrect 用渲染尺寸，和 SR 一致。
- 场景颜色是 HDR 线性时跳过并记日志。
- NR 的任何失败只关掉 NR，不影响 SR。

D3D12 用同一套参数，只差资源类型，后续再做。

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
