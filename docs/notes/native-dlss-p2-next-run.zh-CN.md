# P2续开发与下一次实机验收（2026-09-21）

## 当前结论

本页记录的是 `035765a` 之前的续开发与下一次实机验收计划。其当时结论为：P2尚未完成游戏接入/验收，Gate 3未通过；游戏输入仍为`ColorEncoding::Unknown`，没有强制启用DLSS，也没有发布二进制或修改玩家存档。后续 RTX 5080 有界生产 SR 运行记录见 [当前验证记录](native-dlss-validation.md)，不应将本页的历史计划状态当作当前运行状态。

本轮开始时远端已有`59e9dce`：提交/录制/fence失败进入停止状态、避免等待失败提交的fence、设备丢失后的资源处理，并带有112项CPU故障检查。本轮在它之后继续开发，没有把这项已有提交算作本轮新增。以下“本轮”内容和验证结论均属于该历史检查点。

## 本轮已实现

| 提交 | 内容 |
|---|---|
| `19415e4` | 用真实Renderer方法驱动目标提升、提交与恢复；替换NGX/平台入口，Vulkan命令和读回真实执行 |
| `0a2af46` | 修复反向Z接线：HistoryOwner的R32深度未经翻转，原先NGX配置却默认正向Z；新增显式DepthConvention并检查匹配 |
| `4047d6f` | 新增`LoNativeDlssRendererTest --native`，直接使用真实Controller/NGX走Renderer链；增加异常退出时的本地图片资源排空 |
| `9375608`、`c2f0602` | 补齐该独立测试的Windows clang-cl链接：仅在测试中排除未调用的游戏入口，链接真实zstd依赖 |
| `a9d870c` | 三帧采集元数据检查器、13组单元测试、可选Python CTest接入；不自动认定颜色编码 |

生产链测试执行`PrepareSceneCopyDestination`、`ActivateSceneCopyDestination`、`RecordSceneCopyDlssUsing`、`Flush`、`PreparePromotionAccess`和恢复方法。默认模式仅以合成Vulkan写入替代NGX。覆盖成功/失败的3/2段提交、未知颜色拒绝、上传环不足不触发隐式Flush、跨Flush保留映射、Alpha、后续UI式RGBA写入，以及7种不兼容恢复边界。

它没有驱动完整游戏`DrawImpl`、PM4数据流、窗口交换链或真实文字/UI绘制。不要将这些测试描述为全游戏验证。

## 实际执行的验证

- 本地CPU CTest：7/7通过，包括6组C++测试和1组包含13项检查器测试的Python测试。Python只用于开发工具，不是游戏运行依赖。
- Linux软件Vulkan：真实Renderer路径1,536项检查通过；共享生产shader的256个RGBA16F像素精确匹配。成功日志中未见VUID或验证层错误。
- 反向Z变更已复测真实P1 GPU输入采集，并通过SDK开/关构建及report测试。
- Windows clang-cl与Linux：新的Renderer自测在SDK开启/关闭时均编译通过。SDK关闭时`--native`明确返回77（跳过），这不是原生NGX执行通过。
- SDK开启的Linux构建也执行了合成vendor模式。当时托管CI未执行RTX上的`--native`，未验证其画质、性能或驱动行为；后续 RTX 5080 有界生产记录已补充运行证据，但仍不构成画质、性能或玩家验收。

成功记录：

- [生产Renderer链首次通过](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35626007451)
- [反向Z、P1输入与SDK构建](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35626659861)
- [Linux原生测试入口构建与软件Vulkan](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35628539732)
- [Windows SDK开/关构建及明确跳过](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35628539860)

## 1. 历史复现步骤：RTX 原生 Renderer 自测

后续本机已通过该测试。代码行为、配置和验证条件未变化时复用已有结果，不因合并提交或文档同步重复执行。

无需游戏资源，不触碰游戏存档。在与现有clang-cl构建相同的、已初始化Visual Studio环境的开发终端中执行。使用独立构建目录，替换SDK路径；依赖沿用固定版本SDK`374959484e79a640feaba44c93ac8cfb0a03f5b5`、既有Plume补丁及已初始化的公开子模块。

```powershell
cmake -S tools/tests/motion_replay -B out/p2-native -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DLO_ENABLE_DLSS=ON -DLO_DLSS_SDK_ROOT="<本机固定版本SDK目录>" -DLO_DLSS_STAGE_RUNTIME=ON
cmake --build out/p2-native --target LoNativeDlssRendererTest --parallel 2
ctest --test-dir out/p2-native -R '^LoNativeDlssRendererNativeTest$' -V --output-on-failure --output-log native-renderer.log
```

每一步失败即停止，不要继续使用旧exe。需要可用的DXC；自动加载失败时将`LO_DXC_PATH`设为本机`dxcompiler.dll`的完整路径。Linux使用`clang++`/`clang`和对应`libdxcompiler.so`，同样需要实际NVIDIA驱动和SR运行库；不要选择lavapipe来冒充NVIDIA结果。

原生测试覆盖Quality连续两帧（feature复用）、切换Balanced、切换1920×1080 Performance（重建feature）、注入Evaluate失败（排除隔离命令缓冲、保留回退），并检查读回RGB有限/非零、Alpha和恢复后的后续RGBA写入。

退出0表示这个合成输入下的NGX+Renderer测试通过；77表示SDK/能力不可用；其他非零表示失败。它依然不证明真实游戏运动响应、颜色准确度或性能。保留日志中的设备名称、MODE和各条`NATIVE_CASE`。

## 2. 采集真实游戏颜色链

沿用原交接的隔离要求：独立CWD、明确`--game`、`LO_GRAPHICS_API=vulkan`、`LO_NO_UPDATE=1`，不在玩家原安装/存档目录启动。`LO_PROFILE_DIR`并不能重定向全部宿主配置和存档。

在实际3D场景通过现有F1菜单触发连续三帧渲染捕获。需要低内部分辨率输入时沿用`LO_DLSS_INPUT_PROBE=1`。不要为了捕获而移除`Unknown`守卫。采集可能引起停顿，不能拿采集时的帧率评估性能。

真实目录结构为一个`captures/render-.../`下的三个`frame-01-f...`、`frame-02-f...`、`frame-03-f...`目录，每个都有自己的`p2-oracle.jsonl`。解压捕获后，将整个该次捕获目录交给检查器：

```powershell
python tools/tests/native_dlss/check_p2_oracle.py "<该次render-...目录>" --output p2-color-review.json
```

检查器只读取固定的`frame-*/p2-oracle.jsonl`模式，不扫描游戏、纹理或shader资源。输出包含c10.x的原始位和浮点值、c0–c10/c255、生产→resolve→copy链的文件/行号，以及缺失、版本不匹配、旧帧或帧不连续的原因。

退出0只表示记录结构可供人工颜色审查，2表示证据不完整，1表示输入/解析错误。输出始终保持`color_encoding=unknown`、`p2_accepted=false`。现有v1记录没有覆盖所有中间写入、实际GPU执行和全部采样/格式变换，因此检查器不能自动证明SDR或HDR。

## 3. 仍需完成的游戏接入

确认实际producer的c10.x、shader最终RGB运算、取样/格式变换与目标链后，才能添加窄范围的运行时颜色资格判定，将已证明的输入传给NGX；任何不匹配继续走Unknown回退。不能依据UNORM存储格式、非零输出或检查器退出0直接赋值Sdr。

随后在RTX上执行真实游戏：移动镜头/角色、遮挡变化、透明特效与UI、普通中途Flush、resize/品质切换、错误回退、退出与重进，并检查画面和性能。驱动真实DeviceLost和整条视频呈现链的故障行为仍不能用CPU注入结果替代。

在完成上述证据与实现前，不能向玩家宣称游戏内DLSS已经可用。FG、D3D12移除和SDK二进制分发均不在本次改动范围。
