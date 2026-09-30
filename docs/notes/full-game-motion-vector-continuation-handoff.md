# Full-Game Motion Vector：持续开发交接与验收计划

日期：2026-09-18  
目标仓库：`freefrank/LostOdysseyRecomp`；目标分支：`mv`  
本次核对的代码基线：`f8130e947455f06b377d36ad3f4596c17bc5ec7b`  
前一份交接：`docs/notes/full-game-motion-vector-next-step-handoff.md`

> 给执行agent：先修构建、历史生命周期和未初始化MV读取，再实现真正的GPU previous-position replay。不要继续用CPU相机重投影替代物体MV交付。本文件补充并修正前一份交接；冲突处以本文件和重新核实的代码证据为准。

## 0. 当前状态与本轮目标

当前已有MV数据结构、完整VS常量快照的意图、current/previous双表、CPU相机重投影参考和TAA纹理接口。但这些模块尚未形成经验证的运行时GPU MV闭环；基础实现还有下面列出的错误。[S1][S2][S3][S4]

本次是基于固定commit的代码核对与开发计划，没有在本次交接中构建完整游戏、运行GPU测试或验证实机画质。不要将这些步骤标记为已通过。

本轮目标依次为：

1. 恢复可构建、可测试、默认行为不回退的基线。
2. 建立可靠的帧/场景/实例对应关系，并留存实际位置计算依赖。
3. 实现GPU几何重放，生成当前帧MV、逐像素有效性及动态表面的前帧深度信息。
4. 验证真实场景后，让现有TAA安全消费MV。

本轮不接DLSS、FSR、XeSS，不做Frame Generation，不改存档、导入器、updater或无关渲染逻辑。允许小范围必要的shader translator、pipeline和资源生命周期改动，不重写整个渲染器。

开始前记录实际HEAD、工作区状态和已有构建环境。分支前进时先比较差异，已经修好的问题保留其回归测试，不重复覆盖其他开发者的实现。不得为了回到本基线执行破坏性reset或force push。

## 1. 必须先修的已确认问题

| 优先级 | 基线中的事实 | 必须完成的处理 |
|---|---|---|
| P0 | `renderer.cpp`将`vsConstants`声明为`uint32_t[256*4]`，新调用却写`vsConstants.data()`。C数组没有该成员；运行时环境变量不能绕过编译检查。[S1] | 修正真实类型和调用点，构建包含renderer的主程序，不能只构建独立测试。 |
| P0 | `HistoryOwner`分配并传递MV纹理；TAA只因指针非空便设置`pad0 |= 4u`，没有对应的有效生产证明。[S3][S4] | 立即关闭这条自动启用路径。资源分配、清零、生产完成是三个不同状态。 |
| P0 | `DrawTemporalTracker::BeginFrame()`在同帧第二次调用时会清空两张表和统计，而renderer在draw路径中调用它。[S1][S2] | 帧入口必须幂等；同帧重复调用不得清表。处理epoch、首次调用、跳帧及功能开关。 |
| P0 | 测试只创建一个`uint32_t boolConst`和一个`loopConst`，却传给分别复制8和32个元素的接口。[S2][S5] | 修复确定的越界读取；使用真正的定长数组，并运行内存检查。普通测试打印PASS不能排除未定义行为。 |
| P1 | 重复key出现时只将current entry置无效；第一次调用可能已返回前帧指针，`FindPrevious()`也不检查当前帧冲突。[S2] | 将配对结果延迟至当前场景draw集合冻结后确认，避免已经发出的错误MV无法撤回。 |
| P1 | `EvaluateGrid()`未使用`drawTracker`，没有读取前帧深度；声明的disocclusion阈值没有参与计算。[S2] | 明确命名为CPU camera-MV reference，删除误导性完成声明。不能据此声称物体、skinning或disocclusion已经实现。 |

### 1.1 常量传递与构建修复

不要仅机械地将`.data()`删掉后宣布完成：

- 核对新代码中的`boolConstants.data()`、`loopConstants.data()`是否有真实且类型正确的定义。已核实的draw路径使用`shared.bools[8]`和`shared.loops[32]`；从实际有效的常量容器捕获，禁止补零假装数据齐全。[S1]
- 建议快照保存`std::array<uint32_t, 1024>`原始位模式，或用明确的`memcpy`/逐元素`bit_cast`转换为float存储。不要依赖将`uint32_t*`转为`float*`后直接读取的类型别名行为，也不要做数值转换改变浮点位模式。
- 使用定长数组引用或固定extent的`std::span`约束VS/bool/loop输入。缺少位置必需输入时返回无效，不得将全零快照标为valid。
- 检查新增索引寄存器常量、测试target、平台条件编译和依赖；优先编译真实`LostOdysseyRecomp`目标。关闭`LO_ENABLE_MV_DRAW_TRACKING`不能跳过这一步。

最低测试修复示意：

```cpp
std::array<uint32_t, 8> bools{};
std::array<uint32_t, 32> loops{};
bools[0] = 1;
// 用完整数组调用修正后的接口，并验证首尾元素都正确保存。
```

### 1.2 帧入口与启停

为tracker引入显式未初始化状态，以及与temporal系统一致的frame、epoch、scene/view标识。至少保证：

```text
BeginFrame(N, E) → Record(A) → BeginFrame(N, E)
结果：A仍存在，previous和统计不变。

已完成场景N → BeginFrame(N+1, E)
结果：N成为只读previous；新current为空。

BeginFrame(N, E+1)、跳帧、倒退、关闭后重开
结果：旧配对全部失效。
```

只选一个明确的表轮换位置，避免BeginFrame与EndFrame各轮换一次。previous代表上一份完整、兼容的场景输入，不代表上一份偶然提交过的draw。连续present同一张图不能推进MV历史。

tracker不得只在TAA开启时推进、却在独立MV诊断开启时继续写入不推进的表。将所有启停逻辑集中在同一处；关闭MV时跳过key构造、快照、哈希表、GPU pass和统计扫描。

### 1.3 修正CPU参考的语义与测试预期

基线`EvaluateGrid()`将`depth >= 1`标无效，测试也把1称为sky/clear；现有TAA的host reverse-depth检查是`0 < d <= 1`。[S2][S4][S5] 与当前深度契约统一：0是clear，1不能仅因位于端点就自动算clear。补0、1、越界、NaN/Inf及实际projection的测试。

固定128px阈值只能是显式诊断/保守策略，不能当作“大于128px一定错误”的通用事实。camera cut由连续性/epoch处理，有限的大位移与越界分别记录。返回`(0,0)`的helper也必须伴随有效性，不能把重投影失败混同为静止。

## 2. MV有效性：不要再用指针代替内容状态

在已有`HistoryOwner`附近实现一个小型、明确的状态记录即可，不需要通用render graph。下列名称是建议的新接口，不是声称当前已存在：

```cpp
struct MotionFrameStamp {
    uint64_t frame, epoch, sceneId, resourceGeneration;
    uint32_t renderWidth, renderHeight;
};

struct MotionFrameOutput {
    RenderTexture* velocity;       // RG16F，backward render-pixel MV
    RenderTexture* validity;       // 例如R8_UNORM：0无效，1有效
    RenderTexture* previousDepth;  // 动态表面前帧深度；初版可用R32_FLOAT
    MotionFrameStamp stamp;
    bool recordedForCurrentScene;  // 生产命令已成功录入且位于消费之前
};
```

每个新场景帧先将`recordedForCurrentScene=false`。分配成功或clear成功均不能单独把它设为true。只有资源、extent、frame/epoch/scene和GPU生产顺序全部匹配，才允许消费者绑定有效输出。

整帧就绪与逐像素有效性分开：部分像素有效也可交付，但其余像素必须明确拒绝历史。没有任何有效像素时不得据此宣称MV成功覆盖场景。

在同一有序GPU队列里，“录制完成且有正确barrier”不要求CPU等待GPU执行完成。资源复用/销毁则必须等待已有fence证明安全；不要为了设置ready增加逐帧同步等待。

生产失败、allocation变更、resize、camera cut、epoch变更、AA切换和不兼容菜单/视频切换都撤销ready。旧帧ready不可沿用。未就绪时保留原有无显式MV的路径；已进入MV路径的无效像素则拒绝历史，不能无条件伪装成静态相机MV。

## 3. 必查的VP/VS证据仓库

主入口：

**https://github.com/freefrank/LostOdysseyRecomp-build-inputs/tree/main/feedback**

这里有大量真实VS/PS微码、VP/位置诊断、shader关联统计和稀疏时序数据。不得先凭印象发明通用VP slot、骨骼布局或position stream，再回来找数据解释。[A1]

| 位置 | 用途 |
|---|---|
| `feedback/README.md` | 数据结构、校验规则、增量归档边界。 |
| `feedback/checkpoint.json` | 记录本次分析使用的归档检查点。 |
| `feedback/triage/{README.md,ledger.json,REPORT.md}` | 候选、证据缺口、review/implementation/validation/acceptance的独立状态。 |
| `feedback/data/observations/` | VP/position等诊断；按真实schema解析。 |
| `feedback/data/programs/{vs,ps}/` | 以SHA-256命名的原始微码；配合元数据校验stage、长度和renderer-byte-FNV。 |
| `feedback/data/shader_sources/`、`shader_source_observations/` | program映射及GPU/backend/build关联。 |
| `feedback/data/temporal_sequences/`、`temporal_payloads/` | 稀疏camera/depth/motion时序；按现有格式解码。 |

旧`REPORT.md`写有3401条observations、431个shader sources、18个候选，其reviewed code context为2026-09-10。这是历史报告口径，不能当作当前整个归档的数量，更不能把18个候选当作全游戏shader全集。[A2]

执行要求：

1. 固定并记录build-inputs仓库commit和checkpoint；离线分析，无需重新采集全体玩家数据。
2. 校验实际微码及其hash映射，使用现有`LoShaderTool`/translator，先读真实CLI帮助再批量运行。
3. 生成可复现的`mv-coverage.csv`和简短报告。列至少含：VS renderer hash、program SHA、源是否存在、观测数量、`max_draws`、PS pairs、已有position mapping、oPos依赖的fetch/constants、relative addressing、证据分类、缺口、MV实现/验证状态。
4. `max_draws`保留其原始统计含义，不能冒充总调用次数、玩家人数或真实帧覆盖率。归档`--new-only`可能保留旧mutable metadata，报告需说明这一限制。[A1]
5. relative constants只是依赖特征，不能单独证明skinning；已知VP slot也不能证明instance identity、geometry完整性或安全的MV覆盖。
6. 稀疏时序和程序源码不足以替代相邻完整draw输入。缺前帧stream、身份或纹理时，明确列出并做最小定向捕获。

归档保持私有。公开代码提交只放合成fixture、工具和必要的分析结论；不要复制原始shader二进制、玩家payload、凭据或带token的下载地址。没有归档访问权限时，继续可完成的合成测试，并把真实shader验收标为blocked，不得编造分析结果。

## 4. 配对可靠性：扩充key只是候选检索

现有key比第一版完整，但仍然不能证明两个draw属于同一个物体。首个active fetch也不能自动命名为position buffer。[S1][S2]

### 4.1 两阶段匹配

使用以下流程，或者提供等价、可测试的实现：

```text
Collect：收集当前场景draw及其不可变输入
Finalize：冻结本场景集合，识别重复/歧义/不兼容
Match：只接受当前和前帧均唯一且身份可证明的对应
RecordMV：使用冻结后的匹配结果录制GPU replay
```

必须测试“上一帧一个A，本帧两个相同key的A/B”：本帧两者都不能继承上一帧唯一对象的MV。不能让第一次匹配先写出有效MV、第二次再发现冲突。`FindPrevious()`及统计也必须服从最终判定。

### 4.2 身份与几何依赖

- 用oPos依赖分析确定相关fetch；位置可能依赖多个流。保留slot、stride、格式、offset及内容生命周期，不能只挑第一个active slot。
- 核对索引数据真正来自`DrawInfo`、DMA还是inline PM4。不要假设`REG_VGT_DMA_BASE`永远代表本draw，也不要未经核实把`REG_VGT_INDX_OFFSET`当作first index。
- 区分scene/view和经验证的pass角色，防止shadow与scene、同物体的depth/material/light层互相混配。不要简单以全PS hash分裂一切，也不要让重复lighting pass覆盖已确认的表面MV。
- 优先使用能证明生命周期的guest object/proxy/instance标识；地址、拓扑和program组成的key仅用作候选。无法证明身份时保守无效。
- 地址复用、LOD/拓扑变化、dynamic buffer重写必须有generation/content证据。测试上一帧对象A消失、本帧B复用同地址的情况；每帧都只有一次draw也可能误配。
- 不得将当前世界矩阵、完整常量hash或draw ordinal直接当长期instance ID，否则运动和重排都会破坏对应关系。

## 5. 保存真正的前帧位置输入，排除jitter污染

保存完整256个float4仍是第一版合理起点，但它只覆盖常量部分。位置函数还可能依赖vertex/index streams、fetch描述、bool/loop、VTF纹理及其内容版本。

当前draw路径先调用`ApplyDrawJitter(..., vsConstants, ...)`，随后才执行新加的tracking代码。因此直接复制该位置的常量不能未经检查就称为“unjittered快照”。[S1]

实施要求：

1. 在host jitter修改之前保存guest常量位模式；另存本帧实际应用的jitter和host viewport/VTE/half-pixel等转换元数据。
2. 当前和前帧分别使用自己的输入与host转换。固定Xenos half-pixel修正与temporal jitter是不同数据，不能互相抵消。
3. 静态/常量驱动的几何可复用内容已证明不变的当前stream；CPU改写顶点时，必须保留前帧字节或等价且可重放的版本。仅保存generation数字而找不到旧内容没有用。
4. VTF/动态拓扑等暂不支持时标为unsupported；不要把当前纹理或当前变形结果代替前帧输入。
5. 所有GPU重放使用的upload/descriptor/texture必须留存至提交fence完成。不要保存会被下一次draw覆盖的临时指针。

为调试输出具体拒绝原因：`missing_previous_state`、`ambiguous_instance`、`buffer_reused`、`topology_changed`、`unsupported_position_dependency`等。名称可调整，语义必须可区分。

## 6. 真正的GPU MV producer

### 6.1 第一份交付必须含真实GPU写入

先用合成的静态背景和横移三角形/四边形验证，再接bell场景。复用当前translator的可执行位置逻辑；不通过HLSL字符串替换猜测位置表达式，不要求先做完覆盖所有shader的编译器优化。

```text
当前draw及已确认前帧对应
    → 当前位置函数(current VS inputs)
    → 前帧位置函数(previous VS inputs)
    → 当前/前帧unjittered clip位置
    → 用当前真实jittered位置进行光栅化
    → 输出backward MV + validity + 前帧表面depth
    → 资源barrier
    → 当前帧TAA
```

GPU输出必须通过现有Plume/backend录制真实draw或dispatch写到纹理。`EvaluateGrid()`产生CPU vector、纹理被clear或shader能编译，均不满足该交付。

CPU逐像素评估保留在离线参考测试中。禁止以每帧readback深度→CPU计算→upload速度图作为运行时方案。

### 6.2 数学契约

统一保留：`MV = previousPixel - currentPixel`，单位为实际render pixels，X向右、Y向下；不含temporal jitter。

静止相机中物体从x=98移动到x=100，当前表面MV应为`(-2,0)`。静止场景即使jitter变化，几何MV也应接近零。

两套clip位置都通过已验证的host viewport、坐标转换与half-pixel规则映射至同一像素单位。不要把已包含object变换的矩阵再次当纯camera VP相乘；不要把clip.w或Y翻转处理两次。

对当前和前帧clip位置保留足够的varying信息，在fragment处按正确的透视插值计算投影。不要先在每个顶点除w得到2D速度，再默认其线性插值正确。用带明显透视的三角形测试验证；零/负/非有限previous w必须明确拒绝。

### 6.3 正确的可见表面覆盖

MV画在当前表面的覆盖位置，不能画在前帧位置。

- 第一个合成GPU fixture可限制为opaque triangles、无stencil、固定拓扑。真实shader逐项扩大支持，不能把这个限制下的成功叫full-game。
- 保留与当前表面相关的topology、cull、viewport/scissor、depth bias及alpha/discard语义。Alpha-test几何不能按完整三角形写满MV。
- 当前已保存的`R32_FLOAT` depth是采样资源，不是可直接当D32/stencil attachment使用的等价物。[S3] 选择真实兼容的只读depth/stencil方案，或对受限PoC采用明确验证过的depth采样比较；不可忽略stencil、任意放宽depth容差。
- 不能为了MV修改真实scene depth。若后录制replay，确保所用深度、stencil和可见性属于所选color阶段；无证明的pass拒绝支持。
- 不绘制UI、shadow map、clear rectangle和任意fullscreen后处理的物体MV。
- 未支持的前景、透明特效和disocclusion区域不能露出后方静态表面的“有效”MV。无法可靠标出其coverage时，扩大保守拒绝区域并记录原因。

需要新插值量或MRT时使用独立、明确的host shader variant/layout。检查DXIL与SPIR-V，并把variant、RT格式和binding差异纳入pipeline/cache key；不得抢占已有guest TEXCOORD/MRT槽。

### 6.4 安排生产时点，解决资源分配晚于draw的问题

基线MV纹理在`CaptureDepth()`尺寸变化时分配，位置在scene resolve相关流程；不能直接假定第一次scene draw时已经有可写MV target。[S3]

建议第一版在已确认scene depth/color阶段之间冻结draw快照，随后重放该场景。如果需要更早分配，在scene extent确定后独立prepare，不提前假定场景识别成功。

给出实际调用顺序图：capture inputs→freeze matches→准备/clear targets→record replay→transition→TAA→提交/fence回收。MV不能在TAA消费后才写入；窗口present次数不能替代场景frame token。

每个新场景帧clear一次：MV为0，validity为0，previousDepth为约定无效值。只对已确认表面写validity=1；clear到零不代表静止有效。

## 7. 接TAA时必须同时修坐标与动态深度拒绝

### 7.1 显式分支与合法零速度

用有明确名称的MV enable flag替代magic bit语义，增加texture format/extent/alias及stamp检查。MV和新增mask/depth输入也必须纳入output alias检查。[S4]

去掉`all(q==0)`或`all(raw==0)`这样的数值哨兵；使用明确的是否有有效motion来源状态。合法零MV必须可用，invalid MV必须可拒绝，二者不能混淆。[S4]

### 7.2 raw raster与stable-grid要分别验证

对于同extent、MV定义在当前raw raster样本上的情况，基本关系为：

```text
previousRaw = currentRaw + MV + previousJitter - currentJitter
```

当前普通MV分支直接`q = position.xy + mv`，未完成这个jitter差值处理。[S4] 不能将相机矩阵分支的jitter逻辑想当然地视为MV分支也已处理。

stable-grid分支还涉及stable color与raw depth/MV的采样对应关系。明确每个纹理在哪个网格、从哪个表面取样、history采样如何转换；不能在轮廓处盲目双线性混合不同表面的MV。先通过raw-grid fixture，再验证stable-grid重建支撑域，不删除既有half-pixel约定来掩盖问题。

### 7.3 当前depth检验仍是假设静态世界，不能直接沿用

基线普通分支用camera-only `clip`预测previous depth；stable分支还要求camera-only `farRaw/nearRaw`距离MV得到的raw坐标不超过1像素。[S4]

因此仅写出正确2D物体MV仍然不够：物体自身移动时，深度预测可能错误；stable分支甚至会因物体相对相机预测移动超过1像素而拒绝正确历史。

第一版优先由geometry replay附带输出该当前表面在前帧的depth，并在对应history坐标验证可见性。对stable-grid边缘多表面情况，保留保守策略或建立有表面依据的支撑；不要对不同表面的min/max深度一律套同一个物体运动。

禁止通过取消所有depth rejection、全局拉大阈值或把history weight调高解决问题。必须单独通过“物体靠近/远离相机”和“遮挡后露出背景”的测试。

内部validity、TAA history rejection和未来vendor的reactive mask保持语义分离。本轮可将validity无效转换为TAA拒绝；不要预先认定三家SDK的mask含义相同。

## 8. Bell、skinning与覆盖扩展

Bell调查笔记提供了薄几何运动、`b030` depth/`4053` material和历史capture线索，但没有证明它的全部运动只来自某个刚体常量矩阵。[S6]

先针对相邻帧核实运动输入来自常量、骨骼、CPU变形stream还是其他依赖。不能在无证据时只替换VP后宣称bell MV正确。

验收顺序：合成静态/平移→真实静态相机移动→bell→一个真实skinned角色→多实例→透明和其他场景家族。Skinned角色要求手脚等部位具有不同运动，不能用整个角色一个位移替代。

Bell通过只是局部PoC。不得据此宣称全游戏覆盖或保证薄几何闪烁彻底消失。分别报告MV对应误差、历史接受率、闪烁变化及新增拖影；画质问题可能还需覆盖重建改进。

注意旧调查指出同步capture超过250ms会触发history reset。[S6] 使用已有fence后的异步readback或离线成对输入，不以每帧被capture重置的截图证明稳定TAA；性能测试必须关闭capture。

## 9. 必须新增的验收测试

所有阈值属于项目拟定初始门槛，不是vendor规格或已经取得的成绩。浮点误差在非遮挡、表面内部的选定landmark计算；轮廓单独评估。

| 测试 | 通过条件 |
|---|---|
| 主程序构建 | 实际renderer参与编译；记录compiler、配置、命令、exit code。能测的平台分别记录，未测平台不得标PASS。 |
| 常量及内存 | 1024/8/32元素首尾正确；无ASan/UBSan或可用等价检查报错；null/不完整输入不产生valid快照。 |
| 幂等frame入口 | 同帧多次BeginFrame保留draw/previous/统计；首帧、跳帧、epoch和开关行为正确。 |
| 重复实例 | 前帧一个、本帧两个同key对象全部拒绝歧义；不存在第一次输出已生效的问题。 |
| 地址复用与重排 | 对象更替不继承旧姿态；可证明的同一实例不依赖draw ordinal。 |
| GPU静止+jitter | 真实GPU MV有效像素接近零；改变jitter不改变几何MV，UI不抖动。 |
| GPU刚体平移/透视 | 静止相机中±2px X/Y方向正确；测试深度倾斜三角形。初始landmark误差目标≤0.25 render pixel。 |
| 相机移动 | 已证实静态表面MV与CPU参考一致；不能只检查一个中心像素。 |
| stale与未就绪 | N帧写入、N+1消失/resize/生产失败后不残留有效旧MV；资源存在但ready=false时消费者不采样它。 |
| Alpha/遮挡/前后移动 | alpha孔不写有效运动；背景不吃前景历史；动态previous depth拒绝正确。 |
| Bell及skinning | 静止相机下真实运动表面有对应MV，角色部位运动可区分；提交真实GPU输入输出证据。 |
| TAA两个网格 | raw和stable-grid均覆盖jitter-only、物体>1px运动、camera cut、disocclusion；正确motion不能被旧camera-only支撑条件系统性拒绝。 |
| Backend与回收 | 在可用D3D12/Vulkan上检查bindings/barriers/resize/多帧资源寿命；没有条件的后端明确未验证。 |
| 关闭回归与性能 | MV关闭时旧AA路径和资源生命周期不回退；记录新增CPU/GPU时间、内存、draw/dispatch及等待。 |

测试必须覆盖运行时接口与真实生产纹理，不能只新增另一个不接入renderer的reference类。将CPU测试和GPU测试分别接入现有runner/CI；没有GPU的CI显示skip而不是成功验证GPU。

## 10. 性能边界：保护Steam Deck方向

第一目标是正确性，但不能用CPU readback或全场无差别重复draw换取演示成功。

- 保留整套常量快照，使用可复用arena/表容量，避免每帧大量4KiB节点分配、零填充和释放。只有完整输入保留和相关测试通过后再做依赖裁剪。
- 以帧/场景为单位轮换和统计，不在每次draw扫描全表。开关关闭时不做MV准备工作。
- 第一版先为已验证的表面角色重放，不能把所有lighting/postprocess都再画一遍。重放策略是否扩展到MRT或其他优化，由测量决定。
- 不新增逐draw等待或同步readback。异步抓取使用正常提交fence，CPU/GPU快照生命周期独立维护。
- 用相同路线/分辨率/AA设置对比MV off/on，记录CPU capture/match/record、GPU replay/resolve、P50/P95 frame time、峰值内存及拒绝原因。没有Steam Deck就明确没有Deck测量，不承诺15W 60fps。

已有renderer使用`rasterScale = color->resolutionHeight / 720.0`等物理网格缩放。[S1] 下一阶段先审查并复用现有resolution模块；不要沿用旧口头判断，把internal resolution scaling一概当作尚不存在再重做。低于native的完整路径、UI和postprocess尺寸一致性仍需独立验证。

## 11. 按可独立验收的提交推进

| 提交/里程碑 | 交付 | 进入下一步的门槛 |
|---|---|---|
| M0：恢复安全基线 | 构建修复、MV readiness关闭/校验、幂等frame入口、测试数组越界修复、正确常量来源。 | 主程序和CPU回归测试通过；无未生产MV采样。 |
| M1：证据及身份 | 可复现归档分析、冻结后配对、scene/epoch/实例策略、前帧输入所有权、unjittered快照。 | duplicate/address-reuse/jitter快照测试通过；缺口明确。 |
| M2：GPU合成闭环 | 真正GPU replay、MV/validity/previous-depth输出、资源顺序与异步dump。 | GPU静止、±2px、透视、stale/resize测试通过。 |
| M3：真实几何闭环 | Bell与至少一个skinned角色，补足其真实位置依赖。 | 真实GPU成对证据与逐表面误差，不只是日志计数。 |
| M4：TAA验收与覆盖扩展 | 修正raw/stable网格、动态深度/支撑判定、透明拒绝、性能报告。 | 未出现新增严重拖影；支持范围和未支持情况清楚。 |

M0与不影响其正确性的归档工具工作可以并行；不能跨过门槛提前打开MV消费。每次提交只声称本次实际完成的阶段。GPU设备、归档或素材缺失时，交付可完成的代码与合成测试，明确阻塞项和下一条可执行动作，不编造实机结果。

建议新增文件名仅作组织参考，可按仓库风格调整：`motion_vector_gpu.{h,cpp}`、tracker单元测试、MV GPU fixture、离线coverage工具和validation报告。不要创建空类、空测试或只返回成功的占位实现来满足清单。

## 12. 本轮完成定义及后续SDK门槛

只有以下条件同时满足，才可将本轮标为“可供现有TAA使用的几何MV闭环”：真实运行时GPU生产已接通，帧与像素有效性可靠，配对无已知误用路径，当前/前帧依赖完整，bell/skinning和遮挡测试有证据，raw/stable TAA验证通过，默认关闭路径及资源寿命不回退。

“Full-game”另需场景家族/主要shader/动态特效覆盖证据。不能以高频shader数量或一处场景成功代替全游戏验收。

之后再写vendor-independent的`TemporalFrameInputs`契约和SDK适配计划。先复用现有分辨率系统核实render/output extent、UI阶段、color/exposure/depth/jitter/motion约定，再依据实际Windows/Linux、D3D12/Vulkan、GPU及SDK版本建立能力矩阵。FSR/DLSS/XeSS接入、frame generation和性能收益都不能从本轮PoC成功直接推定。

### 每轮交回维护者的报告

```text
代码基线 / 本次commit：
实际完成的里程碑：
改变的运行时调用链：
使用的archive commit / checkpoint / program fingerprints：
构建、CPU测试、GPU测试、实机场景：各自的命令/结果/未测原因
GPU证据：color、depth、MV、validity、previousDepth、TAA输出
匹配统计及拒绝原因：
画质变化与新增CPU/GPU/内存成本：
默认关闭路径回归结果：
未完成项 / 下一条具体可执行动作：
```

统计区分“候选draw数”“最终有效匹配”“实际有效像素”。GPU像素统计来自真正的buffer/纹理统计与异步结果，不能用draw数推算，更不能输出默认零冒充已测值。

## 13. 核对依据

以下代码链接固定在本次基线，避免后续分支变化使结论失去上下文。归档链接为维护者入口，执行时另记录实际commit。

- [S1：renderer常量、逐draw帧入口、jitter、MV tracking](https://github.com/freefrank/LostOdysseyRecomp/blob/f8130e947455f06b377d36ad3f4596c17bc5ec7b/LostOdysseyRecomp/gpu/renderer.cpp)：重点3330–3620及4340–4385行。
- [S2：tracker与CPU reference](https://github.com/freefrank/LostOdysseyRecomp/blob/f8130e947455f06b377d36ad3f4596c17bc5ec7b/LostOdysseyRecomp/gpu/motion_vector.h)。
- [S3：HistoryOwner分配及传递MV](https://github.com/freefrank/LostOdysseyRecomp/blob/f8130e947455f06b377d36ad3f4596c17bc5ec7b/LostOdysseyRecomp/gpu/temporal_history.h)：重点195–285行。
- [S4：TAA shader与MV enable条件](https://github.com/freefrank/LostOdysseyRecomp/blob/f8130e947455f06b377d36ad3f4596c17bc5ec7b/LostOdysseyRecomp/gpu/temporal_aa.cpp)：重点90–183及250–296行。
- [S5：当前MV测试](https://github.com/freefrank/LostOdysseyRecomp/blob/f8130e947455f06b377d36ad3f4596c17bc5ec7b/tools/tests/motion_vector_test.cpp)。
- [S6：Bell调查及capture干扰记录](https://github.com/freefrank/LostOdysseyRecomp/blob/f8130e947455f06b377d36ad3f4596c17bc5ec7b/docs/notes/TAA_BELL_FLICKER_INVESTIGATION.md)。
- [A1：私有feedback归档说明](https://github.com/freefrank/LostOdysseyRecomp-build-inputs/blob/main/feedback/README.md)。
- [A2：历史triage报告](https://github.com/freefrank/LostOdysseyRecomp-build-inputs/blob/main/feedback/triage/REPORT.md)。
