# Radeon 8060S 偏黑画面调试交接

日期：2026-09-06。目标：在实际 Radeon 8060S 机器上找到第一道低分辨率后处理输出全黑的原因，并用同场景捕获验证修复。

## 当前结论与边界

两个捕获的景深合成把正常场景混入全黑的模糊缓冲，足以解释偏黑和大片纯黑区域。**还没有确定模糊缓冲为什么全黑，也没有已验收的修复。** 不应据此判定 AMD CPU 故障、AMD 独有问题或驱动缺陷。

本次分析工作区 HEAD 为 `98b8fccf5f0e43771c7a54b34ede43ca5453e58c`；它不是捕获机器构建身份。捕获中的 CPU 型号、游戏 EXE 哈希和构建提交尚未确定。已完成捕获分析与独立 D3D12 对照程序；未修改游戏运行时代码、安装构建、启动游戏、提交、推送或发布。用户要求转到 8060S 机器继续 debug。

## 应携带的文件

原始 ZIP 位于分析机器的用户 Downloads 目录，请原样带到 8060S 机器：

- `render-17887420369060564-f1991.zip`
- `render-17887420553625558-f2163.zip`

分析产物目录为仓库根目录下的 `out/amd-render-audit/`。`out` 被 Git 忽略，仅同步 Git 不会携带这些证据。交接包名称为 `8060s-handoff.zip`，原始两个 ZIP 需另外携带；解压后以交接包根目录作为以下命令的工作目录。

| 文件或目录 | 用途 |
| --- | --- |
| `handoff.md` | 本文的便携副本 |
| `report.md`、`composite-comparison.png` | 第一轮证据报告与合成前后对比 |
| `blur_probe.exe`、`blur_probe.cpp` | 独立 D3D12 对照程序与源码 |
| `prepare_probe.py`、`build_probe.bat` | 从原始解压捕获生成输入、原分析机编译命令 |
| `probe-f1991/`、`probe-f2163/` | 已准备好的两帧输入，可直接运行，无需 Python |
| `probe-results.json` | RTX 5080 和 WARP 四次对照结果 |
| `run-probe.ps1`、`baseline-5080.txt` | 一键运行四组对照、打包验证的参考日志 |
| `original-captures.json` | 原始两个 ZIP 的 SHA256 清单 |
| `resolve-sheet-f1991.png`、`resolve-sheet-f2163.png` | 各 resolve 的概览 |
| `dxcompiler.dll`、`dxil.dll` | 与程序一起携带的 DXC 运行依赖 |

每个 `probe-f*/` 的输入是 `vs.hlsl`、`ps.hlsl`、`vs.bin`、`ps.bin`、`shared.bin`、`vb.bin`、`scene.bin`。便携包排除原分析机的输出二进制；运行后会生成 `probe-gpu.bin`、`probe-warp.bin`，重复运行会覆盖同名文件。先复制整个交接目录作为本机实验副本，保留收到的原包。捕获和场景像素只用于私下调试，不应加入公开仓库。

`prepare_probe.py` 需要完整解压目录 `render-*-f1991/`、`render-*-f2163/` 与脚本并列；便携包有现成输入时无需重跑。`build_probe.bat` 使用原机器的 VS 2022 BuildTools 和 LLVM 绝对路径，并依赖仓库中的 `LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp`；它不是可直接在任意目录运行的便携构建脚本。需要改探针时，回到仓库根目录，按本机工具链调整路径后编译。

## 捕获证据索引

两包都记录 `AMD Radeon(TM) 8060S Graphics`，`driver_raw=9007200763840469`。保留此原始值；另在目标机记录可读驱动版本，不凭此字段推测版本号。

| 检查点 | f1991 | f2163 |
| --- | --- | --- |
| 非黑场景 resolve，地址 `0x09fa0000` | seq09 | seq10 |
| 首次九采样降采样 draw | 1465 | 1221 |
| 五张全黑低分辨率 resolve | seq10–14 | seq11–15 |
| 景深合成 draw | 1483 | 1239 |
| 离线合成平均 RGB 误差，8-bit 数值单位 | 0.114923 | 0.046877 |

首次降采样 VS 为 `2f6bbed8149a7804`，PS 为 `7c260eacff1d681d`；景深合成 PS 为 `b4b4d54a7a2d6b96`。跨次运行 draw 序号可能改变，优先按 shader 对、输入地址及通道顺序定位。

已核验：

- 最终 1280×720 resolve 与 `screenshot.bmp` 的 RGB 完全一致，最大通道差为 0。偏黑已存在于渲染缓冲。
- 每帧五张 448×242 FP16 后处理 resolve 的 RGB 和 alpha 都为 0；第一道的场景输入不是黑图。
- 第一通道记录的 viewport/scissor 为 428×242；深度、模板、剔除、混合关闭，颜色写入掩码全开。此处是捕获状态，仍需验证实际 D3D12 提交状态。
- 场景 FP16 和深度捕获没有 NaN/Inf。f2163 有 431 个负 RGB 分量，与受击效果相邻，但不足以解释大片黑区。
- 合成 shader 根据深度插值场景与模糊图，再施加约 `0.4545454383` 的 gamma 指数；另一附加后处理颜色图也为黑。
- 用捕获的场景、深度与常量计算合成，两帧每个通道误差都不超过 2/255。该计算未复刻 GPU 亚像素过滤，是数值近似。
- f2163 有 295128 个像素的计算模糊权重等于 1；293832 个输出像素变成纯黑，而合成前场景的 RGB 最大值大于 0.01。
- draw-drop 与 dummy-binding 计数为 0，不能据此证明资源内容或描述符正确。

## 先运行独立对照

在解压目录的 PowerShell 可直接运行 `./run-probe.ps1`，它会执行四组对照，将各组退出码和日志写入 `results.txt`；`baseline-5080.txt` 是原分析机参考结果。也可手动运行，逐条记录退出码：

```powershell
.\blur_probe.exe .\probe-f1991 *> .\8060s-f1991-gpu.log
$LASTEXITCODE
.\blur_probe.exe .\probe-f1991 warp *> .\8060s-f1991-warp.log
$LASTEXITCODE
.\blur_probe.exe .\probe-f2163 *> .\8060s-f2163-gpu.log
$LASTEXITCODE
.\blur_probe.exe .\probe-f2163 warp *> .\8060s-f2163-warp.log
$LASTEXITCODE
```

GPU 模式枚举 adapter 0，**先检查日志中的 Adapter 真的是 8060S**；多显卡时可能选错，需调整枚举再比较。WARP 应为 Microsoft Basic Render Driver。程序没有游戏启动行为，也不读取存档。

输出位于输入子目录，分别为 `probe-gpu.bin` 和 `probe-warp.bin`：448×242、RGBA FP16，每像素 8 字节，每行 3584 字节。只评价左侧 428×242 有效区域；右侧非绘制边界不用于图像差异判定。退出码 0 仅表示有效区出现非零 RGB 且未收集到错误，不是画面正确的证明；2 表示有效区全零，3 表示收集到 D3D12 验证错误，1 表示异常。日志若写 `debug validation unavailable`，不能称作验证层通过。

原分析机结果：

| 输入 | RTX 5080 非零 RGB 像素 | WARP 非零 RGB 像素 | 有效区总像素 |
| --- | --- | --- | --- |
| f1991 | 103465 | 103465 | 103576 |
| f2163 | 103568 | 103568 | 103576 |

四次退出码都是 0，D3D12 debug layer 均不可用；非零计数相同不代表输出逐像素相同。

**探针边界：**复用捕获 HLSL、寄存器重建的 VS/PS 常量和场景像素，但 `vb.bin` 是人工构造的全屏 quad，`shared.bin` 部分字段由脚本重建。它绕过游戏、plume、运行时资源绑定及原 DXIL 缓存。它不是原 draw 的精确重放；5080/WARP 有图只能说明这套受控输入可以产生非黑输出。

## 8060S 上的定位顺序

### 1. 按独立探针结果分流

- 8060S 全黑而同机 WARP 有图：优先收集 debug layer、实际编译 DXIL、驱动版本与输出，再将独立程序缩小；有无报错都先排查应用 API 使用，不能立即定性驱动 bug。
- 两者都有图：重点回到游戏实际 shader、几何、共享常量、描述符、资源状态及 resolve 路径；独立探针没有复现原运行时问题。
- 两者都黑或程序报错：先核对输入哈希、DLL、日志、编译/创建设备错误及有效区输出。此结果不能单独支持 AMD 硬件问题。

### 2. 核实实际加载的 DXIL，并隔离缓存做 A/B

当前 `renderer.cpp` 会重新生成 `entry.info.hlsl`，随后仍可能读入缓存 DXIL；捕获输出的是重新生成的 HLSL。**捕获 HLSL 不保证等于该次实际执行 DXIL 的来源。** 这是待查缺口，尚无证据证明缓存确实错配。

在 `device->createShader` 前记录最终 VS/PS DXIL 字节与 SHA256、shader hash、缓存命中/路径及编译器 DLL 哈希。先保留原缓存副本和基线；后续游戏试验使用独立 save/profile 与隔离缓存目录，一次只改变缓存条件。代码支持 `LO_SHADER_CACHE_DIR`；在实验启动环境中指定新的目录，不删除用户原缓存。旧缓存副本与新目录两组需使用同一 EXE、同一场景与设置，并分别记录实际加载 DXIL。亮度、gamma 调整或跳过景深只能作为诊断控制，不能当作根因修复。

### 3. 补齐首次降采样的真实输入

在目标 shader 对的实际 draw 处同时保存：

- 实际上传的顶点字节、索引、拓扑、base vertex、stride/offset 与 fetch 解码；既看 guest 快照，也看 GPU 上传/绑定结果。
- 最终 VS/PS 常量和完整 shared constants、根参数、SRV/sampler 描述符及其资源身份、格式、mip、swizzle、尺寸与状态。
- 实际 PSO、RTV、viewport/scissor、颜色掩码及相应 shader DXIL。

现有 `LO_GEOMETRY_CAPTURE_DIR` 的条件是 `vs->info.usesRelativeConstants`，**不会捕获本次 blur VS**；不能只设环境变量就认为几何证据齐全。需先把诊断筛选扩展到目标 shader 对或该通道。原实现写出的 stream 还是 CPU swapped 快照，不是上传堆回读。

### 4. 将“绘制失败”和“resolve 失败”分开

锁定首次降采样，记录同一通道的输入 SRV、绘制前 RT、绘制后 RT、resolve 前源 RT、resolve 后目标，注明资源 ID、子资源、尺寸、区域、barrier 与 fence。必要时使用目标机图形捕获工具检查实际命令流。

- 输入正常、绘制后 RT 全黑：查几何是否覆盖有效区、UV/常量、绑定的纹理内容、shader 与 PSO；可用固定颜色 PS 和简单采样 PS 分别验证覆盖与采样。
- 绘制后 RT 正常、resolve 后黑：查 resolve 源选择、格式、拷贝区域、资源别名、同步及后续覆盖。
- resolve 正常、下一通道采样黑：查 SRV 指向的资源/版本、描述符更新及资源生命周期。

不要仅比较帧尾五张黑图；目标是找到第一次从已知非零输入变为全零输出的位置。对照应保持同一存档、固定镜头、相同显示和游戏设置，一次修改一个条件。

## 源码入口

以下路径相对仓库根目录，源码需要另外的仓库 checkout，便携包不含完整仓库。行号对应上面的分析工作区 HEAD，后续变更请按函数和关键词查找：

| 文件 | 入口 |
| --- | --- |
| `LostOdysseyRecomp/gpu/renderer.cpp` | 577：`LO_SHADER_CACHE_DIR`；1139–1181：翻译 HLSL、读取缓存、编译及 `createShader` |
| 同上 | 1896：`DrawImpl`，跟踪常量/绑定/绘制；2329–2363：geometry capture 筛选与 CPU stream 快照 |
| 同上 | 2410–2420：捕获 HLSL 来自 `entry.info.hlsl` |
| 同上 | 2903：`ResolveOnGpu`；2961：`ResolveImpl`；3196：`FinishDebugCapture` |
| `LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp` | 探针和运行时的 HLSL 编译入口 |
| `out/amd-render-audit/prepare_probe.py` | 合成 quad 和 shared constants 的具体构造，可审计探针与真实 draw 的差异 |

## 回传与验收

回传四份探针日志、退出码、两帧 GPU/WARP 二进制输出，以及目标机 GPU/CPU、驱动版本、OS、EXE 与 DXC DLL SHA256、构建提交/本地 diff、游戏设置和缓存路径。若进入游戏调试，补充首次失败通道的真实输入、实际 DXIL、前后 RT/resolve 和同场景截图。

修复验收至少需要：8060S 上第一道降采样有效区恢复合理非黑内容、后续模糊链有效、景深合成不再把正常场景压成黑块，并检查有景深/无明显景深的场景。“程序退出 0”“缓存清空后启动成功”或“提高 gamma 看起来更亮”都不能替代上述验证。当前状态仍是根因待定位；没有修复发布可供宣称。

## 2026-09-06 当前澄清

以上保留调查历史。现已定位 placed RT 部分拷贝前缺完整初始化，并补齐 framebuffer 退役清理；本地技术审阅、GPU 回归、标题及最终读档随机战斗验证通过，运行时代码与该次验证一致。用户于 2026-09-06 要求提交至 `amd-fix` 开发分支；尚未发布，未授权推送或发布，不属于 v0.2.1，用户画面验收仍待确认；本轮未复测 NVIDIA。详见[当前结果与边界](amd-resolve-initialization.md)。
