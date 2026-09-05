# 战后演出白屏与 EDRAM 格式切换（2026-09-04）

## 复现与证据

用户报告首场战斗后的实时演出整屏白色，同时战斗光效与 Xenia 不完整。
`out/cg-white-baseline/` 保存本轮复现：第 4500 帧仍是战斗，第 4800、5100 帧采样全白。
自动输入在原开场序列后，从 2800 到 16000 每隔 120 swap 按 A；LO_AUTO_PULSE=6，默认 30 fps。

`out/cg-white-trace/` 在第 4800 帧开启逐 draw、resolve 和 shader 导出。
逐阶段结果：

- 首次场景 resolve 到 0x9fa0000 的红色范围为 0..0.714。
- 场景按 fixed 2_10_10_10 保存后，游戏在同一 EDRAM 基址 0x2d0 绘制阴影衰减遮罩。
- 第 1191 个提交 draw 从 fmt54 的 0x9fa0000 取样，以 fmt2 恢复场景。
- 下一批材质 draw 改用 fmt12（7e3）并采用加法混合，需要读取刚恢复的目标数据。
- 原默认仅在 resolve 前转换 EDRAM 格式，draw 切换时只更新 owner，保留旧 7e3 视图中的白色清理值。
- 随后 HDR resolve 红色最小 14.4141、最大 32.5、均值 28.941；最终合成被钳为纯白。

因此必须检查 draw 开始时的目标内容，而不能仅修最终 tone mapping 或降低曝光。
本地 Xenia `render_target_cache.cc` 的 Update 在绘制前调用 ChangeOwnership 并安排内容转移。

## 按需抓帧

`LO_CAPTURE_REQUEST=<文件>`：在文本文件中写入新的非零整数，于下一帧触发已有 draw trace、
逐 draw 和 resolve 导出。目录由 `LO_DUMP_RESOLVE_DIR` 指定，逐 draw 间隔由
`LO_DUMP_DRAW_EVERY` 指定。默认不启用；文件只读取，按帧边界检查，输出文件包含帧号。
这用于定位更晚的过场，不需要每次重启猜测帧号。
已用独立短跑验证连续请求 1、2 分别触发第 1、114 帧导出；文件留在
`out/capture-request-check/`，两次输出没有覆盖。

## 修复及验证

默认改为在 draw 和 resolve 前都进行格式转换；`LO_EDRAM_TRANSFER=read` 保留为旧行为对照开关。
没有修改曝光、遮挡、深度或跳过阴影。该转换目前仍限于已有的三种 32 位颜色存储类；
更完整的跨基址、跨 pitch、深度/颜色共享仍有待实现，不能据此宣称全部 EDRAM 行为正确。

单变量 A/B 的对照版 `out/cg-white-transfer/`：第 4800 帧恢复坦克及双侧炮口火焰；
HDR 场景最终红色均值约 0.626，最终前台图均值约 0.514，不再全白。
第 5400 帧进入 Heavy Tank Right/Left Unit 目标选择，第 6000 帧之后的攻击画面继续正常。
两轮待机/演出相位不同，不进行逐像素误差或性能比较。

Windows 运行时重新构建成功；LoMemoryAliasTest、LoShaderAluTest、LoStencilTest 均通过。
默认配置复跑目录为 `out/cg-white-final/`（不设置 LO_EDRAM_TRANSFER）。
该复跑第 4800 帧已进入重型坦克战斗菜单，画面正常；进程保持运行供手动检查。
本修复恢复了已观察到的火焰演出，但战斗其余粒子、光晕和阴影细节仍需继续对照 Xenia，
不能将炮口火焰恢复视为所有光效完全匹配。
所有游戏截图和捕获数据均保存在 ignored 的 `out/` 中。
