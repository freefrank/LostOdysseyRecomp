# 接手入口（2026-09-05）

先读[状态总表](../STATUS.md)、[路线图](../ROADMAP.md)，再读对应专项记录。[旧交接](../archive/2026-09-04/handoff.md)仅供历史追溯。

## 当前基线与未提交改动

`81f955d` 已推送文件日志/GPU停帧诊断，`21d6523` 已推送 XMA/PCM 初版及循环终点修正。它们不是全部音频、光影或营地稳定性的完成标志。

工作区仍含未提交的遇敌资源恢复、宽printf与switch修正、存档细节、传送/POI、输入和诊断代码。不要用一次文档提交混入这些代码；先检查 `git status` 和专项证据。生成 PPC 和游戏资产不提交；依赖修改通过[补丁](../../tools/patches/README.md)分发。

## 最近实际结果

- 用户确认手动保存成功。独立进程从用户存档副本载入 **Highlands of Wohl - Gorge（u12_0）**，沿正常路线触发演出并进入营地，控制恢复。未完成营地新存档读回，未通关。
- 用户曾报告营地窗口无响应但日志继续。后台未复现，最新用户试跑暂未卡住；根因仍未知。主动暂停测试GPU所得 WAIT_REG_MEM 报告只是诊断验证，不是用户卡死根因。
- 用户确认背景音和部分对白消失，其它声音仍在。循环终点跨越修正通过离线和部分环境音回跳采样；长音轨、对白和循环子帧未完成。
- 人物/敌人阴影、火焰黑红格子、Ring外环、箱子黑色特效均仍待修。Xenia火焰本身也有glitch，以用户实机参考为正确性依据。
- F1判胜、同地图传送和POI已有局部验证；地图ID/名称、随时存档、攻击力调整尚未实现。

## 执行约定

用户2026-09-05最新授权：可使用电脑、桌面和前台游戏；每项feature或bugfix验证后单独commit并push，无需再问。此前仅后台/禁止push的限制已被覆盖。仍保留用户原始save/profile，回归使用独立副本。

本任务配置每30分钟自动续跑；这是定时触发，不代表始终有agent运行。每次先检查实际进程和agent状态，结束不再需要的独立捕获副本。未完成九项继续跟踪，全部完成后暂停自动续跑。

## 接下来的调查

1. 音频继续验证不同音轨启动、输入缓冲切换与循环子帧。见[音频](audio-output.md)。
2. 缺失polygon offset只是阴影候选；补齐后需要GPU及同场景对照，不能直接标修复。见[GPU](gpu.md)。
3. Ring测试使用持续RT/释放，A或右肩键不能代替。资源崩溃已通过局部回归，不等于外环可见。
4. 原生地图定义插入 `82A0D648` 提供ID与包名：2=u11_0_scrw、3=u13_0_scrw、4=u12_0_scrw。内部开发地名可能过时，需关联实际本地化显示名。
5. `fcController.CheckSavePoint` 的 `829EC140 → 82A16DC0` 会激活存档交互；不是可直接强制true的菜单门禁。随时存档需找到正常保存流程和可用性判断。

## 代码地图

路径相对 `LostOdysseyRecomp/`：

| 子系统 | 文件 |
|---|---|
| 入口/日志 | `main.cpp`、`os/logger.h` |
| 客体内存/导入 | `kernel/guest_address_space.cpp`、`memory.cpp`、`xex_loader.cpp` |
| GPU | `gpu/command_processor.cpp`、`renderer.cpp`、`video.cpp` |
| 着色器 | `gpu/shader/xenos_translator.cpp`、`dxc_compiler.cpp` |
| 音频 | `apu/audio.cpp`、`xma.cpp`、`xma_loop.h` |
| 调试与输入 | `debug/`、`hid/hid.cpp` |

测试入口见[渲染验证](rendering-validation.md)、[攻略测试](walkthrough-testing.md)、[存档](save-storage.md)、[营地](third-map-hang.md)。`out/`截图/日志仅在本地。历史会话与逆向工具索引见[归档](../archive/README.md)和[Ghidra说明](../../tools/ghidra/README.md)。

## 仓库同步

当前主目录已迁移到清洗后的公开历史。提交完成后运行 `tools/push_all.ps1`，核对 Gitea 和 GitHub 的 main 一致。旧历史只在 Gitea archive 分支保留，不合并、不推到 GitHub。`out/github-release` 已完成使命，不再作为独立开发目录。流程见[同步说明](../PUBLISHING.md)。
