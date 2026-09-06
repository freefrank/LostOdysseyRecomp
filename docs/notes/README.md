# 逆向与验证笔记

先读[当前状态](../STATUS.md)和[接手入口](handoff.md)。日期化实验保留证据，不自动代表当前功能支持。过期GPU、内核和旧交接已移到[归档](../archive/README.md)。

最新汇总：[2026-09-05成果与交接](../WORK_REPORT_2026-09-05.md)。

## 现有文档

| 专项笔记 |
|---|
| [Map12 海报黑斑与深度偏移](map12-poster-depth.md) |
| [窗口事件线程与无响应修正](window-event-pump.md) |
| [启动深度清除崩溃](startup-depth-clear-crash.md) |
| [首次启动着色器准备与覆盖边界（2026-09-05）](shader-preparation.md) |
| [音频输出与对白修复（2026-09-05）](audio-output.md) |
| [戒指战斗资源名缺少语言后缀（2026-09-04）](battle-ring-resource.md) |
| [临界区大小端与线程死锁（2026-09-05）](critical-section-endian.md) |
| [Debug 地图 ID 和本地化名称（2026-09-05）](debug-map-info.md) |
| [Debug 人物传送：逆向依据与验证边界（2026-09-04）](debug-teleport.md) |
| [随机遇敌动画与战斗停滞调查（2026-09-04，主角与战斗停滞已修复，敌人待查）](encounter-animation.md) |
| [GPU 当前说明（2026-09-05）](gpu.md) |
| [接手入口（2026-09-05）](handoff.md) |
| [内核 HLE 当前说明（2026-09-05）](kernel.md) |
| [光照 / 阴影续修（2026-09-04）](lighting-stencil-depth-clear.md) |
| [火焰受击亮暗帧与灯光参数（2026-09-05）](fire-hit-rendering.md) |
| [Polygon offset 接入与验证边界（2026-09-05）](polygon-offset.md) |
| [阴影纹理零 LOD 翻译（2026-09-05）](shadow-texture-lod.md) |
| [角色黑色剪影：物理地址别名与遮挡查询（2026-09-04）](physical-alias-rendering.md) |
| [战后演出白屏与 EDRAM 格式切换（2026-09-04）](post-battle-whiteout.md) |
| [重编译配置笔记](recomp.md) |
| [角色破面与后期重影调查（2026-09-04）](rendering-index-and-resolve.md) |
| [渲染验证（2026-09-04）](rendering-validation.md) |
| [手动存档失败（2026-09-04）](save-storage.md) |
| [随时存档开关与后台回归（2026-09-05）](save-anywhere.md) |
| [第三地图卡住调查（2026-09-04）](third-map-hang.md) |
| [标题动态背景恢复（2026-09-04）](title-packed-mips.md) |
| [攻略路线与后台推进测试（2026-09-04）](walkthrough-testing.md) |
| [Xenia 实机画面对照（2026-09-04）](xenia-render-comparison.md) |
| [游戏数据来源与 XEX](xex.md) |

记录地址范围、还原结构、可重复验证和未决问题；不要创建只有规划名称、没有实际内容的索引项。
