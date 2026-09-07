# v0.2.2 release notes / 发布说明

Published as the latest full release on **2026-09-07 at 03:11:10 UTC** (2026-09-06 local), not a draft or prerelease. / 已于 **UTC 2026-09-07 03:11:10**（本地 2026-09-06）正式发布，为最新版本，非草稿／预发布。

[Download / 下载](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2) · [Hosted CI / 托管 CI](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34077788392)

## English

This update fixes the tested AMD black or overly dark title/background and depth-of-field output. Newly allocated resolve render targets are fully initialized before partial copies, and cached framebuffer views are removed when their textures retire.

Validation includes AMD GPU resolve tests and title/battle checks, plus NVIDIA RTX 5080 GPU/layout tests and title/settings/Map 12 comparisons. The user also tested the NVIDIA build and reported no glitches. The manual run's logs showed no new blocker; existing warnings remain.

This update also preserves Unicode installation and save paths on Windows, including startup arguments and saved-content discovery/reopening. Eight startup-path cases and eight ASCII/Unicode storage runs passed, covering write, read, overwrite and readback; read runs also verified 8,000 concurrent positioned reads without mismatches.

Two known shader-preparation failures are outside this fix's scope. The complete first-save gameplay crash reported in Issue #4 has not been reproduced; the path/storage tests do not establish that this crash is resolved or replace in-game acceptance. This release does not establish complete-playthrough compatibility or cover every scene and driver.

See [rendering implementation and validation](amd-resolve-initialization.md) and [Unicode path validation](save-path-unicode.md).

## 中文

本次更新修复已验证 AMD 场景中的标题、背景全黑或偏黑，以及景深合成异常。新分配的 resolve 渲染目标在局部复制前先完整初始化，纹理退役时同步移除缓存的 framebuffer 视图。

验证包括 AMD GPU resolve 测试与标题／战斗检查，以及 NVIDIA RTX 5080 的 GPU／布局测试和标题、设置、Map 12 对照。用户也实际测试 NVIDIA 构建，确认未发现 glitch；人工运行日志未见新增阻塞，既有警告仍保留。

本次更新也修复 Windows Unicode 安装目录与存档路径的转换问题，覆盖启动参数和存档发现／重新打开。8 项启动路径用例及 8 组 ASCII／Unicode 存储测试通过，覆盖写入、读取、覆盖与读回；读取测试还完成了 8,000 次并发定位读取，零数据不匹配。

两个已知着色器预编译失败不在本次修复范围内。Issue #4 报告的首次存档后完整游戏崩溃尚未复现，路径／存储测试不代表该崩溃已经解决，也不能替代游戏内验收。此次发布不代表完整通关兼容性，也不覆盖所有场景和驱动。

详见[渲染实现与验证记录](amd-resolve-initialization.md)及[Unicode 路径验证](save-path-unicode.md)。

## Package verification / 正式包验证

Tag commit: `f03efe370d444db1a8a9c1213c240da697f58504`. ZIP: **38,205,388 bytes**, SHA256 `e91af2f49c03da48714731b07912767614d676be9d8887192647b217f2d29789`.

Hosted CI passed every step, including Unicode startup and storage regression. The downloaded package passed CRC and all 44 manifest hashes, contained no private game data, and its `InstallGame --self-test` exited 0. All eight startup-path cases also passed against the official EXE. An isolated RTX 5080 run from a Chinese working directory stayed alive for 49.54 seconds; the swap-1200 screenshot was visually confirmed as Map 12. The two existing shader-preparation failures remained, without new error/fatal records. The test cleaned up its own process afterward; cleanup exit code 1 was not a crash.

托管 CI 全步骤通过，包含 Unicode 启动与存储回归。下载包通过 CRC 及全部 44 项 manifest 哈希检查，不含私有游戏数据；包内安装器自测返回 0，正式 EXE 的 8 项启动路径用例全通过。中文工作目录下 RTX 5080 隔离运行 49.54 秒，swap 1200 截图目视确认为 Map 12。两个既有着色器预编译失败仍保留，未见新增 error／fatal；随后测试清理自身进程，清理退出码 1 不代表崩溃。

Local evidence in the original workspace / 原工作区本地证据：`out/release-v0.2.2/{published.json,package-validation.json,smoke-result.json,ci.log,smoke-存档/scene.png}`.

The first CI attempt (`34076841551`) built successfully but its startup test could not print U+2032 through CP1252. The test-only UTF-8 stdout correction passed eight locally forced-CP1252 cases before the successful rerun; it did not change runtime behavior. / 首轮 CI 构建成功，但启动测试无法通过 CP1252 输出 U+2032；仅修测试 stdout 为 UTF-8，强制 CP1252 本地 8 项通过后重跑成功，未改变运行时行为。
