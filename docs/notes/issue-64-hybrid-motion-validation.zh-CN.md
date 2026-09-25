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

最终提交使用保留的只读工作流`SR hybrid regression`复验：两平台CPU、Linux sanitizers、实际Vulkan像素、几何replay、TAA、嵌入式renderer、捕获JSON，以及固定生产SDK头文件的on/off编译。结果以对应提交的Actions记录为准。临时补丁脚本和写入工作流已移除。

测试覆盖运动方向、32个jitter相位、几何覆盖优先、无效深度、reset、连续帧恢复、旧帧/epoch拒绝、批次耗尽后恢复、GENERAL/SHADER_READ资格，以及SDK绑定元数据。嵌入式renderer使用替代vendor边界；源文件编译不等于真实NGX/FSR SDK执行。没有完整游戏链接、RTX实机、Steam Deck、画质或性能验收。

## 4. 本地复验

先按项目现有构建流程重编译整个程序，保留官方NGX/FSR运行库。使用存档副本，不覆盖原存档。选择Vulkan；DLSS须在支持的RTX机器上测试。确认没有显式设置`LO_MV_ENABLE=0`、`LO_MV_REPLAY=0`或`LO_MV_CONSUME=0`。

Windows PowerShell：

```powershell
$env:LO_SR_HYBRID_MV = "1"
$env:LO_MV_LOG = "1"
.\LostOdysseyRecomp.exe
```

Linux：

```bash
LO_SR_HYBRID_MV=1 LO_MV_LOG=1 ./LostOdysseyRecomp
```

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
