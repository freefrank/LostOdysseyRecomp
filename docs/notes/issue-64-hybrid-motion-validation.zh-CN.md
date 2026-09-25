# Issue #64：Hybrid MV修复与验证

日期：2026-09-25。分支：`fix/issue-64-hybrid-motion`。基线：`2c427b9447f41cc070ed917dfe0bb1a7700557cb`；运行时集成：`d5865773aae2568eab40f51b49a4215dc24d4a1a`。

## 1. 修复范围

对合格的Vulkan DLSS/FSR场景，在GPU上生成相机/深度运动矢量，并用有效的几何MV覆盖对应像素。无法获得几何MV时，使用相机MV估算；输出明确标记为`MotionState::Hybrid`（数值3），不会伪装成已验证的角色/骨骼运动。若原几何replay整帧中止，本帧只能得到相机估算，不能宣称保留了中止前的部分几何覆盖。

缺失几何MV或motion pipeline暂未就绪，不再必然阻止具有完整Hybrid输入的SR执行。相机、深度、颜色来源、尺寸、帧/epoch和资源资格检查仍然保留；原生对象MV、透明特效和shader mapping仍可继续渐进完善。

DLSS使用`pInBiasCurrentColorMask`，进入SDK前要求GENERAL布局。FSR保留原material reactive mask，将Hybrid置信度单独接入transparency-and-composition输入，要求SHADER_READ布局。两种mask都是偏向当前帧的机制，不保证完全拒绝历史或消除拖影。新增路径不应用于普通TAA，也不授权Frame Generation。

输出资源按原有GPU完成serial复用，批次上限8；正常渲染不增加GPU readback或额外队列等待。新增全屏pass和纹理有成本，本轮没有实机帧时间或显存峰值测量。

DLSS尺寸查询增加有限恢复：同一缓存key/设备epoch下，首次失败后约1秒、再约2秒各重试一次，总计最多3次。成功模式保持可用，Unavailable不重试。失败的DLAA查询不会被擅自替换为猜测的1:1尺寸；日志保留实际optimal/min/max、原始NGX返回码。

## 2. Issue日志实际说明了什么

[#64](https://github.com/freefrank/LostOdysseyRecomp/issues/64)的第一份附件是D3D12会话，第二份`runtime.log`才是Vulkan。后者识别RTX 4080 SUPER，并曾在帧1331、21669提交FSR输出，随后出现`UnknownColorEncoding`、`NoEligibleScene`等回退；DLSS尺寸错误发生在DLAA选择后。

因此，日志不能证明所有报错都由缺失MV造成。本补丁修复缺失MV的可恢复路径和尺寸错误缓存问题，不宣称已定位该驱动上DLAA异常尺寸的最终原因，也不会绕过未知颜色来源。Issue应保持开放，直到报告者路线完成验证。

## 3. 自动验证证据

初始隔离验证[Actions 36176747773](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36176747773)：Windows/MSVC与Linux/GCC各通过150项CPU检查，Linux ASan/UBSan通过；llvmpipe通过3,391项Hybrid GPU/HistoryOwner检查，同时通过既有时序输入、SR路由与TAA回归，实际`renderer.cpp`编译通过。

初始原生适配器[Actions 36176866401](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36176866401)：FSR及NGX SDK-on/off源文件编译通过。以上证据早于最后的NGX布局与捕获元数据修正，不能代替最终提交的复验。

最终代码/CI提交`d6bb513680d4ee8754fed5b03725647de21cda09`已通过[Actions 36178376986](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36178376986)：Windows与Linux CPU两个job，以及`vulkan-and-native` job均为success；Linux ASan/UBSan、实际Vulkan像素、几何replay、TAA、嵌入式renderer、捕获JSON，以及固定生产SDK头文件的on/off编译均通过。此后的本文件更新只补充证据与复验步骤，不改变受测运行时代码。临时补丁脚本和写入工作流已移除，保留只读工作流`SR hybrid regression`。

测试覆盖运动方向、32个jitter相位、几何覆盖优先、无效深度、reset、连续帧恢复、旧帧/epoch拒绝、批次耗尽后恢复、GENERAL/SHADER_READ资格，以及SDK绑定元数据。嵌入式renderer使用替代vendor边界；源文件编译不等于真实NGX/FSR SDK执行。没有完整游戏链接、RTX实机、Steam Deck、画质或性能验收。

## 4. 本地复验

先按项目现有构建流程重编译整个程序，保留官方NGX/FSR运行库。使用存档副本，不覆盖原存档。选择Vulkan；DLSS须在支持的RTX机器上测试。确认没有显式设置`LO_MV_ENABLE=0`、`LO_MV_REPLAY=0`或`LO_MV_CONSUME=0`。

Windows PowerShell：

```powershell
$env:LO_SR_HYBRID_MV = "1"
$env:LO_MV_LOG = "1"
.\LostOdysseyRecomp.exe 2> sr-diagnostics.log
```

Linux：

```bash
LO_SR_HYBRID_MV=1 LO_MV_LOG=1 ./LostOdysseyRecomp 2>sr-diagnostics.log
```

`DLSS sizing:`原始尺寸诊断写入stderr，应同时保留`sr-diagnostics.log`和程序生成的runtime日志；每次对照运行先另存上一份诊断文件。

无需游戏资产的CPU复验，在仓库根目录执行：

```bash
cmake -S tools/tests/sr_hybrid -B out/hybrid-cpu -DCMAKE_BUILD_TYPE=Release
cmake --build out/hybrid-cpu --config Release --parallel 2
ctest --test-dir out/hybrid-cpu -C Release -V
```

完整的Vulkan/原生头文件复验依赖和命令见[保留的CI工作流](../../.github/workflows/sr-hybrid-regression.yml)。

A/B对照将`LO_SR_HYBRID_MV`改为`0`后重启程序；这是进程启动开关，不是运行中热切换。必须使用同一EXE、存档、分辨率与相近镜头路线。诊断完成后关闭`LO_MV_LOG`，性能比较不得包含F1捕获/readback过程。

| 场景 | 操作与合格条件 |
|---|---|
| 缺失几何MV | 在原先MV不足的可识别场景开启FSR/DLSS；存在`SR hybrid MV`记录，随后实际SR提交；不能只看菜单选项。 |
| 静止镜头 | 检查地面与细线稳定性；jitter不应制造周期运动或全屏抖动。 |
| 平移与转向 | 分别缓慢平移、快速转向；静态场景矢量方向为previousPixel−currentPixel，不能带入重复jitter。 |
| 角色与特效 | 行走、转身、衣物、烟雾/粒子、遮挡边缘；未知对象运动允许近似，但不能出现不可接受的拖影、拉丝或闪烁。 |
| 生命周期 | 读档、切图、镜头切换、暂停返回、窗口resize与分辨率改变；允许暂态fallback，随后应恢复；不能沿用切换前的历史。 |
| Provider/quality | FSR Quality/Native AA、DLSS Quality/DLAA切换；失败模式不可永久拖垮其他正常模式。 |
| 资源与回退 | 重复切换并长时间移动；检查崩溃、设备丢失、显存持续增长。关闭Hybrid后应回到原有策略。 |

## 5. 需要保留的证据

每条失败路线保存版本/源码SHA、EXE SHA-256、GPU/驱动、系统、分辨率和quality；附同一进程runtime日志，以及移动或故障时的F1“Capture render state”ZIP。记录实际Submitted/fallback原因，菜单显示“no submitted output”本身不能证明SDK从未运行。

FSR的`fsr-evaluations.json`中检查`motion_state=3`、`transparency_composition_bound_to_sdk=true`、`hybrid_confidence_bound_to_sdk=true`，并核对实际`checked_submit`/`gpu_completed`。DLSS的`dlss-evaluations.json`中检查`motion_state=3`及`bias_current_color_bound_to_sdk=true`。mask绑定字段只描述接线，不代表每个像素拥有正确对象运动；捕获被省略或未执行SDK时不能当作生效证据。

DLAA仍报SizingError时，保留`DLSS sizing:`中的quality、output、optimal/min/max和raw_ngx；确认有限重试后仍失败，再研究具体NGX/驱动返回。`UnknownColorEncoding`或无合格相机/深度仍需独立定位，不应通过清空资格检查来“修复”。

关闭#64的条件：报告者原失败路线实际提交并可恢复，DLAA尺寸问题有解释或独立修复，A/B画质没有明显回归。自动测试通过不能替代这一步。

## 6. 2026-09-25 RTX 5080 实机复验

使用同一`e2131d7f` EXE（SHA-256：`E4A9AAF7756D5D5AAF170E6A41C787FCC8E566DE7FC4C3375DA33CB490B6E6FE`），Vulkan、RTX 5080、驱动`616.56`、`3840x2160`。ON/OFF目标环境变量分别确认生效为`1/0`。完整SDK-on Windows build成功；本次没有重跑已有CI。

DLSS Quality的成对捕获为ON `6131-6133`、OFF `2516-2518`。两组各帧均为`vendor_success=true`、`adopted=true`、`checked_submit=true`、`gpu_completed=true`；ON为`motion_state=3`且`bias_current_color_bound_to_sdk=true`，OFF为`motion_state=1`且该绑定为`false`。另有ON DLAA提交`frame=7590`；本次所有DLSS sizing mode均成功，没有`SizingError`。因此已确认真实DLSS SDK提交、Hybrid运动状态和bias接线生效。

FSR捕获为ON Native AA `f9858-9860`、OFF Quality `f4541-4543`，quality不一致，不能作为严格A/B画质对照。两组均有`checked_submit=true`、`gpu_completed=true`；ON为`motion_state=3`、Hybrid mask为`true`，但该捕获因`capture_limit`省略原始输入/输出，不能据此判断像素级结果；OFF为`motion_state=1`、mask为`false`。

用户实测确认Hybrid开启和关闭时地板仍有上次尝试修复的闪烁，实机画质验收失败，不能合并到`main`。四组导出的`seq00`、`seq02`地板稳定，`seq05`已出现明暗交替；该变化早于DLSS/FSR处理。ON DLSS首次可见变化范围为`draw748..1316`，更细的draw归因尚未完成，不能据此宣称Hybrid是唯一根因。F1 readback会造成约2--3秒帧间隔/reset，不能用来代表正常运行性能，也不能直接证明历史reset根因。

原始证据保存在本地忽略产物目录：`out/validation/issue64/manual-e2131d7/analysis/all-floor-metrics.json`和`all-floor-comparison.png`。这些文件用于复核，不是公共发布附件。该次Hybrid ON/OFF运行的状态是：真实SDK执行和绑定已验证，但地板闪烁未解决；这条历史失败记录保留，不代表后续地板单项修复的验收结果。

## 7. 后期光照候选的限定范围修复

根据新的ON DLSS捕获，在生产`temporal_scene.h`中为此前`held`的`2078ccaa70d44732`增加单条slot 8映射，对应后期光照`draw1206..1210`。这不是对原35-source批次统计的改写：历史批次仍为35个source，历史映射数量仍记为119；本次是其后的单项限定范围映射。

`LoTemporalJitterTest --captured-f6131-late-floor`通过334,086项检查，覆盖5条捕获绘制、32个相位和1440p/4K；旧clip分离为0.488091 px，tex0分离为0.488205 px，paired-depth lookup误差为0。`--feedback-mapping-batch`在从held列表移除该候选后通过216,624项，原35个VS的其他结果不变。这些是捕获常量、独立公式和合成顶点的CPU检查，不是真实GPU地板像素验收。捕获分析已核对`seq04`（`draw1199`）为`b0d9000`、format 6、2560x1440，`draw1206..1210`的texture 0与其地址和format相同、guest尺寸为1280x720；producer `draw1058..1197`共140条绘制、6个VS均已映射且VP匹配。实际guard与像素相位仍未核实。完整Windows SDK-on build已成功。新候选EXE（`e2131d7+dirty`，仅含本次runtime单项映射）SHA-256为`05A2C9EC8983D293946D9987B828EF1BB45B3405090BC9056BF8D5FE25B0BF03`，在`manual-floor-fix1`目录完成本地实机验收；`sr-diagnostics.log`记录frame 1320的`geometry_view_ready=true`、`reset=false`、`input=2560x1440`、`confidence_mask=1`。新证据已将范围限定到该后期地板光照映射；仍未证明producer实际通过jitter守卫、像素采样相位或完整像素覆盖。用户已接受该场景地板修复，本地验收完成，当前可接受合并到`main`；这不扩大为所有模式、全游戏或Issue #64原RTX 4080 SUPER DLAA报告者已验收。Issue #64保持开放，尚未发布新的Release。

此前的`2e33e75`修复已经包含在远端`main`及本次EXE中；`a027ab99fa3e3b0d`和`ff769ec7b88e575f`的slot 7/8映射均已存在。上次审查保留为`held`的后期光照候选`2078ccaa70d44732`在本次变更前仍未映射；新ON DLSS三帧中，它与PS `4013372b6413788f`出现在`draw1206-1210`，对应`seq04`（draw1199）的光照mask阶段，slot 8精确匹配场景VP，五组几何均匹配更早的深度/材质绘制。地板颜色稳定状态是`seq00`和`seq02`，不能把`seq04`当作地板颜色稳定帧。这缩小了后续调查范围并促成本次单项映射，但生产者实际通过jitter守卫、像素采样相位及实际像素覆盖尚未验证，不能认定该候选是唯一根因。另一候选`e810cfacc107fd3c`位于draw746，早于地板仍稳定的seq02，当前证据优先排查后期候选。
