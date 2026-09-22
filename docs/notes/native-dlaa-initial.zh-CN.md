# 原生 DLAA 初步实现

日期：2026-09-21。基线：`dlss@fcda598006dd5de50ee0ac7a17f9db78c39b1d1a`。

状态：实验性、开发分支配置与图形菜单入口。已实现模式/尺寸/NGX 接入、图形菜单选项及 CPU 回归，包含 MSVC 原生 DLSS fixture 的 `/utf-8` 源码编码支持。图形菜单改动及最新构建尚未部署，尚未获得用户界面与体验验收；运行时前期用户日志显示 NGX 可用（RTX 5080）、1440p/4K 尺寸切换及暂态 motion_pipeline_pending 回退，但不足以作为 DLAA 实际 Evaluate 与画质验收依据。本文不提升原生 DLSS 的 P2/Gate 3 状态。

## 开启

先退出游戏，在当前生效的 `settings.ini` 中设置：

```ini
graphics_backend=1
upscaler=1
dlss_quality=3
```

需要启用 `LO_ENABLE_DLSS=ON` 的构建、可用的 NVIDIA NGX runtime 和受支持的 Vulkan/RTX 环境。不要改用户存档或共享资源路径。

原有 `dlss_quality` 编号保持不变：0 Quality、1 Balanced、2 Performance；新增 3 DLAA。关闭时用 `upscaler=0`；切回 Quality 用 `dlss_quality=0`。图形菜单中已暴露 Upscaler（Off/DLSS）与 DLSS quality（Quality/Balanced/Performance/DLAA）选项；Upscaler 为 Off 时自动隐藏 DLSS quality 行。未部署的最新构建中，现有菜单的空间缩放品质和抗锯齿列表不代表 DLAA 的实际执行状态。

DLAA 采用实际输出内容尺寸作为场景输入尺寸。2560×1440 内容对应 2560×1440 → 2560×1440；1280×800 drawable 内的现有 16:9 内容仍是 1280×720。图形菜单已移除内部分辨率行，但保留已持久化的原有数值及旧 `antialiasing` 用于回退，不覆盖用户设置。

## 实现边界

- 共享模式清单驱动四档 sizing，显式映射到官方 `NVSDK_NGX_PerfQuality_Value_DLAA`。不把项目的枚举值直接强转成 NGX 枚举。
- `ProbeOnce` 仍检查原有三档 SR，维持既有探针契约。DLAA 能力由 `QueryOutputSizing` 按输出单独检查；DLAA 失败不将正常 SR 模式一起禁用。
- 查询必须成功、尺寸非零、min/optimal/max 有序，且 DLAA optimal 严格等于输出内容尺寸。失败时不伪造 1:1 推荐尺寸。Planner 和 RecordIsolated 再次检查 DLAA 的等尺寸约束。
- 设置验证、数组索引和请求签名统一正规化非法 quality。第四档沿用现有 2-bit quality 和 24-word v2 帧计划，不新增 temporal consumer。
- 沿用 `DlssSr` 的时序输入、jitter、像素 MV、reversed-Z、颜色资格判断、独立 NGX 命令列表、scratch、guest alpha 合成及失败保活。没有新增原地覆盖或纹理优化。

## 已执行验证

在本次编辑环境，使用通过 Git blob SHA 核对的基线文件运行新 `LoNativeDlaaTest`：

```sh
g++ -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Werror -pthread \
  -I LostOdysseyRecomp tools/tests/native_dlaa_test.cpp -o LoNativeDlaaTest
./LoNativeDlaaTest
```

结果：350 个 CPU contract checks 通过。另以 `-fsanitize=address,undefined -fno-omit-frame-pointer` 构建执行，同样通过；`git diff --check` 通过。

覆盖：原编号与非法值处理、720p/1080p/1440p/4K/超宽/16:10/非整比例内容尺寸、完整帧计划往返、稳定帧不递增 geometry epoch、同尺寸 TAA/DLAA 和 quality 切换、输入探针、readback、不支持/错误/异常尺寸回退、请求级失败闭锁、过期失败拒绝及显式重试。

新测试已加入现有 `LoNativeDlssCpuTests` 和 CPU CTest 标签，因此 `dlss` 分支现有 Windows/Linux CPU workflow 会包含它。CI 是否成功应以对应提交的 Actions 结果为准，不由这份本地记录推断。

仓库中可执行：

```sh
cmake -S tools/tests/native_dlss -B out/native-dlss-cpu \
  -DLO_NATIVE_DLSS_CPU_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/native-dlss-cpu --config Release --target LoNativeDlssCpuTests
ctest --test-dir out/native-dlss-cpu -C Release -L cpu --output-on-failure
```

## 下一步 RTX 验证

使用 SDK-enabled 构建运行 `LoNativeDlssProbe --sizing 2560 1440`。检查第四个 `modes` 元素：state=Ready（1）、optimal=2560×1440、原生结果成功。该命令的退出码只表示至少一档可用，退出码 0 本身不能证明 DLAA 可用；默认 Probe 和 `--production-sizing` 也不能代替 DLAA 检查。

随后扩展真实 Renderer fixture 的 DLAA 1:1 Create/Evaluate、非均匀 RGB/alpha、模式切换和失败注入，并在游戏检查细线、移动镜头、遮挡、透明物、UI、字幕及帧耗时。现有纯色/零 MV fixture、CPU epoch 检查均不能证明实际时序画质或 GPU history reset。没有 RTX 结果前不要标记 DLAA 已验收。
