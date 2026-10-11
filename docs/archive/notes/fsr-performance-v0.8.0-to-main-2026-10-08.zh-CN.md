# FSR 性能对比：v0.8.0 到 main（#305）— 2026-10-08

[#172](https://github.com/freefrank/LostOdysseyRecomp/issues/172) 报告的是 v0.8.0 上 FSR 的性能。本记录在同一台机器、同一场景下，对比四个节点：

- v0.8.0：报告者使用的版本。
- v0.8.10：含 [#189](https://github.com/freefrank/LostOdysseyRecomp/pull/189)，SR 输出尺寸图像改为跨帧复用。
- v0.8.44：含 [#250](https://github.com/freefrank/LostOdysseyRecomp/pull/250)，削减 SR 前后的输出尺寸 pass。
- main `26ad776c`：含 [#305](https://github.com/freefrank/LostOdysseyRecomp/pull/305)，FSR encode 并入 composite，restore 重采样推迟。尚未发布。

## 条件

- 主机：psvita（AMD Radeon 8060S，RADV），通过 GE-Proton10-34 / vkd3d-proton 运行 Windows 版 Direct3D 12。
- 版本：v0.8.0、v0.8.10、v0.8.44 用各自发布包里的 exe 和 DLL；main 用本地 clang-cl 构建，其运行时代码与 `26ad776c` 相同。
- 场景：乌斯拉住宅区存档，站立不动，输出 3840x2160（隐藏窗口），帧率上限 120，GTAO 开，阴影 4 档。
- shader pack：v0.8.0 用 `portable_dx12-239f877563b6dc7a`，其余版本用 `portable_dx12-48cf14e3720d8a64`，都已确认命中。
- 采样：不限功耗，稳定 10 秒后取 10 秒窗口内 `LO_RENDER_TIMING` 的平均值；每格只跑一次。
- 噪声：同条件下两次运行的 GPU 时间一般相差 0.05–0.1 ms。12 W 那组没有跑。

## 结果

每格是帧率 / 每帧 GPU ms。"1080p 内部"指不开超分，用 1080p 内部分辨率双线性放大到 4K。

| 版本 | 1080p 内部（无超分） | FSR Performance（1080p） | FSR Quality（1440p） |
|---|---|---|---|
| v0.8.0 | 57.0 / 8.09 | 36.8 / 13.93 | 30.7 / 19.25 |
| v0.8.10 | 57.3 / 8.07 | 37.9 / 14.33 | 30.2 / 19.91 |
| v0.8.44 | 55.2 / 8.04 | 39.4 / 13.65 | 31.7 / 18.76 |
| main `26ad776c` | 57.6 / 7.95 | 40.1 / 13.18 | 32.7 / 17.92 |

- FSR Performance 相对 1080p 内部的帧率：v0.8.0 为 65%（慢 35%），main 为 70%（慢 30%）。
- 同一对比下多出的 GPU 时间：从 5.84 ms 降到 5.23 ms。
- 从 v0.8.0 到 main：FSR Performance 帧率 36.8 → 40.1，FSR Quality 帧率 30.7 → 32.7。

## 解读的边界

- 不限功耗时 psvita 的瓶颈在渲染线程：帧时间大于 GPU 时间，所以帧率同时反映 CPU 和 GPU 的开销。
- 报告者在 v0.8.0 上看到各 FSR 档位帧率相同。那是 #189 修复的每帧分配输出尺寸图像的开销，在 psvita/Proton 上没有表现出来：这里 v0.8.0 的 Performance 和 Quality 已经明显拉开。
- 剩下的差距主要是 FSR SDK 本身和运动矢量重绘，见 #250 和 #305 PR 说明里的分项拆解。
- 12 W 功耗上限下 #305 的单独收益记录在 [#305 PR 说明](https://github.com/freefrank/LostOdysseyRecomp/pull/305)：FSR Performance 51.66 → 48.64 ms。
- 没有在报告者的 RX 6600 / i7-8700K 上测试。

原始日志在 psvita `~/lo-i172/runs/HU-*`，测试脚本是 `~/lo-i172/fsrhist.sh` 和 `i172rel.py`，不随仓库分发。
