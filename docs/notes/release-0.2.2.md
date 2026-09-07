# v0.2.2 release notes / 发布说明

Status: release preparation; not yet published. / 状态：发布准备中，尚未发布。

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
