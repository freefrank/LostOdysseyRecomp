# 4K Vulkan shadow-loop GPU audit — 2026-09-12

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

以下“不是生产 translator 修复”等表述仅指最初诊断阶段；后续实施状态见下方“实施跟进”。

本文记录最初的单帧 RenderDoc 诊断与单 shader 离线探针，以及后续 `gpu-perf` 分支上的有界修复；实现已纳入已发布的 v0.5.7，游戏实景验收仍待完成。目标是定位 RTX 5080 在 3840×2160 Vulkan 下的高 GPU 负载。诊断使用同一发布 EXE 0.5.6，SHA-256 为 `1fff598e1a0872da2a7728ceb9921aa2e4a1bff0bd818c224827b84eaf3f5aaa`。正确捕获是 [`audit_frame12462.rdc`](../../out/gpu-profile-20260912/session-02/audit_frame12462.rdc)，大小 1,274,168,861 bytes；较早的 `audit_frame7922.rdc` 不是本结论的依据。

## 结论

捕获的主要热点是一个自适应 shadow-filter pixel shader。实际 shader 映射为 PS `a195c4db25859691`、VS `99c2b4b0960a9ccd`，命中 EID 3070、3100 和 3692。该 shader 的 guest `LoopEnd` 原始值为 `8000003f0008`：loop ID 31，启用 predicated break，condition 0。捕获所用 translator 遗漏了这两个字段，捕获 HLSL 因而没有对应的 `break`。

同一捕获的常量显示 `loopConst31=0xff`，即循环上限 255，`c253.x=8`。这个 shader 的有效追加样本最多 8 轮；predicate 失效时，剩余 247 轮只执行已失活的循环体，形成可定位的空转成本。该数字描述 shader 语义与上限，不等同于每 invocation 的实际内存事务，也不外推到其他 shader。

## 受控 ABBA 探针

在 `out` 中仅对该单 shader 追加 `if (!p0) break;`，用同一 DXC 重新编译。原 HLSL 的 baseline SPIR-V SHA-256 为 `c87777eacf2465516e9d8877d5a22a9fe187eb121b51f65a0fe3e4997608a40e`，与捕获字节一致；探针 SPIR-V 为 `5e1452e8545645d2cac8b25bc0d35e3d63a9f9679664560cc2788aa9ec4b0394`。这证明探针只改变了目标循环控制流，探针没有部署到生产代码、运行缓存或可执行文件。

游戏和其他 GPU 测试全部退出后，使用同一 RDC 做最终 ABBA。热点与总 event 时间如下；此前并行运行游戏时采集的 counter 不用于最终结论。这些是离线重放的逐事件时间，不直接换算为实际游戏 FPS 或功耗收益。

| 替换 | hotspot（两次） | 总 event（两次） |
|---|---:|---:|
| 原 shader | 7.810848 / 7.612 ms | 14.128256 / 13.925408 ms |
| `break` 探针 | 1.020768 / 1.468160 ms | 7.077600 / 7.140000 ms |

最终 3840×2160 原 shader 与探针 PNG 的字节 SHA-256 均为 `EE053AB9A8B14D136FD74CAC2F4B049D750C0471CBD510EE0CDFA6D65521A5A8`。这支持该 shader 的循环空转是本帧重要 GPU 成本来源，同时说明探针在这个捕获帧没有改变输出字节。

## 2026-09-12 实施跟进

后续在新分支 `gpu-perf` 中将该思路收敛为有界 translator 行为。`CanExitInactiveLoop` 只有在单个 structured loop 满足全部证明条件时才发出 per-lane break：循环体每条指令都被反向 predicate 屏蔽，`aL` 在整个 shader 中没有读取，且没有复杂跳转、嵌套循环或 implicit-LOD 操作。这是针对已识别模式的安全守卫，不能解释为通用 Xbox 64-wave 语义模拟。

正式 translator 生成的该 pixel shader SPIR-V SHA-256 为 `5e1452e8545645d2cac8b25bc0d35e3d63a9f9679664560cc2788aa9ec4b0394`，与前述离线探针完全一致；本节复用原有 ABBA 与 PNG 证据，没有重复 GPU 测量。shader cache 版本从 21 提升到 22，以拒绝旧二进制与启动 bundle，避免继续使用不含新控制流的缓存。

`LoShaderLoopTest` 已通过 11 个 guard case 和 12 个 D3D12 case，每个 case 覆盖 64 个 mixed-lane 输入，并与独立 CPU group-of-64 参考实现一致。9 个相关 guest shader 均通过 DXIL 与 SPIR-V 编译，其中 8 个采用保守 guard 优化，1 个保留原路径；cache test 已通过 99 项检查（证据：`out/gpu-profile-20260912/production-01/backend-cache-test.log`）。正式 translator 的 PS SPIR-V 与原探针完全一致，因此复用原 ABBA 与 PNG 证据，没有重复 GPU 测量。

同一分支的主程序 Release build 已成功，证据为 [`build.log`](../../out/gpu-profile-20260912/production-01/build.log)。resolve-copy 实现位于 renderer 与 `resolve_copy_policy.h`：仅去重同 batch 内完全相同的复制，并在 Draw、clear、transfer、分配、提交或外部访问时失效；metadata、layout 和 clear 副作用仍保留。`LoResolveCopyPolicyTest` 通过 30/30；既有 `LoResolveCopyGpuTest` 的 Vulkan 与 D3D12 记录均为每后端 3 allocation × 2 pass、`mismatch_pixels=0`，并验证 copy/skipped/逻辑 resolve 次数及 source-clear 强制重新 copy。`LO_RESOLVE_COPY_REUSE=0` 可关闭该路径，GPU stats/timing 会记录 `recorded` 与 `skipped`。

捕获中的 EID 5529/5532 重复复制为 66,355,200 bytes；尚未在修复 EXE 的实际游戏中验证命中，也未测量整帧收益。最终用户 EXE 和 `gpu-perf.cmd` 已准备但尚未启动游戏。已交付的诊断 EXE 是 `D:/Mihoyo/LostOdysseyRecomp-windows-x64/LostOdysseyRecomp-gpu-perf.exe`，SHA-256 为 `313CDD34712154A88FEEAF9C25D8AB1409705D327D8B0FD15252A52562C1C211`，来自包含既有 dirty 工作的 `0.5.6-hotfix1` 工作树，不代表干净发布产物；实现已推送于 [`4715b60`](https://github.com/freefrank/LostOdysseyRecomp/commit/4715b60)，并由 [`1d139c9`](https://github.com/freefrank/LostOdysseyRecomp/commit/1d139c9) 合并 `github/main` 的 `b39c2c9`，已纳入发布的 v0.5.7。

## 采集边界与后续工作

CPU 前采窗口为 GPU batch 平均 17.345 ms、CPU draw 5.587 ms；CPU 数字含注入开销，不能当作纯发布数据。该帧结构为 550 render passes、442 host draws、109 clears、17 copies；shadow loop 已定位为主要已证瓶颈，但 pass 拆分成本尚未量化。

[官方 Xenia 源码](https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/ucode.h)将该控制流定义为 guest 64-invocation group 级 predicated break。不能把本次 shader 的 per-lane `break` 直接照搬为通用生产 translator 修复；任何生产改动都需要按 shader/wave 语义另行设计，并进行匹配回放、图像和多场景验证。最初诊断阶段没有生产代码改动、没有新版本、没有提交或发布，也没有跨场景用户验收。

证据：[`ab-counters.json`](../../out/gpu-profile-20260912/ab-01/ab-counters.json)、[`original-a.png`](../../out/gpu-profile-20260912/ab-01/original-a.png)、[`break-a.png`](../../out/gpu-profile-20260912/ab-01/break-a.png)、[`constant-detail.json`](../../out/gpu-profile-20260912/focus-01/constant-detail.json)、[`hotspot-source-audit.md`](../../out/gpu-profile-20260912/analysis-01/hotspot-source-audit.md)、[`predicated-break-compile-identity.json`](../../out/gpu-profile-20260912/analysis-01/predicated-break-compile-identity.json)、[`shader-map-resolved.json`](../../out/gpu-profile-20260912/analysis-01/shader-map-resolved.json)。
