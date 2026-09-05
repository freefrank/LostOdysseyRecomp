# 阴影纹理采样 LOD（2026-09-05）

当前汇总见[成果报告](../WORK_REPORT_2026-09-05.md)和[状态总表](../STATUS.md)。下文按实验时间保留证据，早期未完成状态不代表最新结果。

状态：已修正一项指令翻译缺口，162 个现有着色器离线编译通过。画面与闪烁回归仍在进行，未关闭阴影问题。

## 实际路径

`shadow-sequence-on-03` 的 f12511：深度图 `0xae00000` 为864×864，来源是 `dinfo=0x5a0`、880 pitch 的两块422×422角色绘制；全屏遮罩由 VS `99c2b4b0960a9ccd` / PS `d55a20d004031279` 读取屏幕深度 `0x9c00000` 和这张深度图，写入 `0xb0d9000`。

注意两种 shader hash 不同：command processor 的 bin 文件名按32位word做FNV；renderer/HLSL按字节做FNV。同一PS的本地bin为 `out/render-light-depthpack/shaders/ps_8cd07188caf60a72.bin`，207 dwords。`out/decode-shadow-fetch.py` 按CF执行序列解码，而不是把ALU字误认成fetch。

## 缺口与修改

该PS共17次纹理读取：5次 `useCompLod=1`，另12次 `useCompLod=0`，后者处于谓词条件分支；全部 `useRegLod=0`、`useRegGradients=0`、`lodBias=0`。旧翻译全部调用 `Sample`，因而让明确不使用计算LOD的读取依赖隐式导数。

Xenia 的 `dxbc_shader_translator_fetch.cc` 中，LOD源初始化为0，再按寄存器/偏置/计算梯度决定来源。源码参考：<https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/dxbc_shader_translator_fetch.cc>，本地副本 `out/shadow-reference/`。本次没有把偏置、寄存器LOD或显式梯度路径强制解释为零。

新增1D/2D、3D、Cube的显式零LOD helper，仅在上述三个来源均关闭且指令偏置为0时使用 `SampleLevel(..., 0)`。保留其它路径现有行为，其完整LOD/过滤覆盖不在本次完成声明内。renderer缓存版本从v19更新至v20，避免继续读旧DXIL。

## 验证与限制

- 实际阴影PS重新翻译后12次显式零LOD、5次隐式采样，DXC成功。
- `out/render-light-depthpack/shaders` 中162个shader重新翻译并编译，0 failures。日志 `out/shadow-lod-corpus.log`。
- 运行时构建通过：`out/shadow-lod-runtime-build.log`。
- 独立后台新程序 `out/shadow-lod-01`，支持按需截图和逐draw/resolve捕获。旧v19对照进程为 `shadow-sequence-on-03`，两边都启用polygon offset，比较时只检查LOD改动的影响。

编译成功只能证明代码可运行，不能证明阴影闪烁、火焰或遇敌投影已修复。仍需同场景连续帧和实际受击/遇敌验证。

运行时补充：shadow-lod-01 的实际 ps_d55a20d004031279.hlsl 已确认12次 LevelZero、5次隐式采样。4952–5071共120帧是首战菜单近景；14483–15082共600帧覆盖攻击、敌方近战反击和返回菜单。模型动作与场景可见，但没有覆盖火焰或后续地图遇敌，不能据此关闭相关问题。连续帧联系表保留于该本地目录 sequence-contact.png、attack-contact.png。

后续回合：16135–16434、17425–17724各300帧记录了第二/第三次攻击及敌方反击，第三回合实际HP由370降至364。已看到近战受击白亮火花，尚未出现用户给出的范围火焰场景。这些样本不作为火焰格子已修复的证据。

2026-09-05 采样器核对：实际阴影PS17次fetch的mag/min/mip均为3（使用fetch constant）；未实现指令级filter override是通用缺口，但不能解释这个特定shader。旧capture日志不含完整fetch六字，out/shadow-sampler-audit.json的0条不是采样器数量为0，而是缺少字段，不能作排除依据。

renderer采样器palette仅64项，溢出原静默返回0，增加一次有界警告；LO_TRACE_SAMPLERS记录新增slot/key（最多64条），未改变渲染。79A98C04构建通过build-sampler-diagnostics.log。新后台3152 shadow-sampler-01独立camp副本，shot1300确认营地；实日志仅5项：485/0/480/495/15，无溢出，排除本次营地画面的palette耗尽原因。未进入战斗，不覆盖长流程其它场景。进程保留，无input/shots1，工具结束，无提交。后续沿shadow map生产→resolve→采样深度数值继续查，不盲目增加容量或改过滤。

2026-09-05 原始深度取证：DumpResolveStep对显式捕获的R32_FLOAT增加同名.f32（紧密行、小端float32），仍保留PPM预览，不额外GPU同步；4AEA396B构建通过build-depth-raw-capture.log。新后台33100 shadow-depth-raw-01独立camp副本，shot1241营地，capture1抓frame2034。screen depth9c00000为1280x720、921600个有限值，范围0.00116826–0.02320692；shadow ae00000两次resolve均864x864全有限，范围0–1。文件长度均width*height*4，depth-stats.json验证通过。

两次同帧shadow resolve相差9238像素，差异包围盒(51,171)–(765,857)，unique3257→1095。它们是不同绘制阶段，不是相邻帧闪烁证据；需要结合各次生产/采样顺序判断覆盖是否合理。原8bit PPM不足验证小深度差。现场无input/shots1/capture1完成，进程保留，工具结束，无提交；下一步可直接用本进程capture2及已抓完整draw/fetch日志，勿重启。

2026-09-05 同帧生产/消费顺序：从frame2034完整日志生成shadow-order.json。ae00000 seq2在行4150完成，其后12个写颜色的shadow PS绘制和27个mask0绘制（至4376）；seq3到4832才发生，其后另12个颜色/18个mask0绘制（至5058）。因此该捕获中第一张阴影图在覆盖前已被消费，不能以seq2/seq3数值不同推断提前覆盖。R32 copy前后有COPY_SOURCE/COPY_DEST/SHADER_READ转换；此记录不证明正常未同步捕获时不存在资源同步问题。

下一步应检查每组shadow比较输入的投影坐标/深度值或捕获正常相邻帧，不改为永久保留seq2。这一轮完成日志顺序证据，未修改渲染。33100仍是下一现场（运行前需重查），无input/shots1/capture1；无提交。

2026-09-05 投影数值审计：out/audit-shadow-projection.py 对 frame2034 的69条 shadow PS 常量记录（含mask0绘制）与原始screen深度计算重建分母，全部为正，范围1.68445e-5至0.00222292。所有记录的shadow投影w系数均为(0,0,0,1.0000001192)，因此本例没有随像素跨零的shadow透视分母。证据 projection-audit.json。HLSL的FLT_MIN实际定义为负最大有限值0xff7fffff，并非C语言最小正数；不能以宏名误判负倒数被截正。shadow比较使用min(projectedZ,0.999)，UV除以近似1的w；未发现可由这些记录证明的除零/符号异常。全图范围不等于实际stencil覆盖区，尚未验证采样坐标与深度的逐像素对应或相邻帧稳定性；本轮无渲染修改。

2026-09-05 连续帧取证：CIM确认33100仍为独立shadow-depth-raw-01；通过shots请求2捕获148495–148524共30连续帧（约1秒），未发角色输入。temporal-contact.png显示营地远景，人物和背景可见；temporal-stats.json记录29组相邻帧RGB平均绝对差0.398–1.026/255，单通道变化超过32的像素每对1–27个。未见全屏交替明暗，但人物占画面很小、只有1秒，不能排除局部自阴影闪烁，更不能覆盖战斗/火焰。此测试避免继续将同一帧两次shadow resolve差异当作闪烁；下一步需实际遇敌近景连续帧。现场shots2完成、capture1、无input，未改代码/提交/停止进程。

2026-09-05 实际遇敌现场：新后台独立out/shadow-encounter-01 PID5408，4AEA396B，与营地基线相同EXE，使用query-watch-01的Hypocenter存档/profile副本。loaded.png确认起点，文件输入1–18复走地图，shot5334进入真实战斗；目标列表为三名Insane Khent Soldier（并非原记录的两名）。input19仅选攻击停目标列表，未执行攻击。shots4采5755–5814主角镜头60帧，shots5采5905–5964敌人目标60帧，sequence-4/5.png、encounter-temporal.json留证。相邻RGB平均差分别1.882–3.274和0.848–1.253，包含镜头/待机/粒子，不能当阴影异常强度。

capture1另抓f7254全draw/resolve，得到screen seq01 9c00000和shadow seq02 ae00000原始f32，大小3686400/2985984字节。下一步直接查这个遇敌帧的shadow生产、mask和人物材质绑定，勿重走营地或重复Ring判定。现场input19/shots5/capture1均完成，所有辅助脚本结束，5408保留，无提交；未宣称修复。

2026-09-05 遇敌shadow atlas异常已定位到具体样本：f7254 seq02 ae00000全有限，min0.3861033/max1，11574非clear像素全部位于右上432x432象限；左上/下半完全1。日志却有左上(5,5)和右上(437,5)各4次422x422模型深度绘制。两组间存在prim8 depth-only clear，pitch440/base5a0；renderer的跨pitch深度clear分支对同base其它pitch直接clearDepthStencil整张纹理，忽略矩形覆盖。这是会抹掉先前atlas内容的具体候选，比继续查营地d55 PS更直接；本帧没有d55 PS，不能把营地采样PS结论直接套用。resolve-contact.png/shadow-draws.txt可复查。

先扩展显式capture期间的跨pitch深度clear日志，记录frame、原rect、目标尺寸；原日志前8次额度在启动已耗尽，当前样本缺具体rect。build-shadow-clear-trace.log通过，新EXE 8BB6BC7AC7999D6BF3ADF7A2188A22931106030D436052DD9C543D2E0D6B57AA。仅诊断未改clear行为，尚未运行新EXE；下一步捕获实际rect并按EDRAM tile范围映射验证，不能简单禁用clear或整图复制。旧5408保留input19/shots5/capture1，工具结束，无提交。

2026-09-05 真实清除矩形确认：新8BB6BC7A后台13472 shadow-clear-trace-01独立Hypocenter副本，input1补Last Saved Game确认，input2–19路线、20攻击目标。shot6405真实遇敌，capture1=f6918。实日志两次pitch440/base5a0 clear：(-0.5,-0.5)..(199.5,215.5)与(239.5,-0.5)..(399.5,215.5)，但两次都对pitch880、880x896目标执行整张clear至1。第二个明显局部矩形会把左侧已绘制内容一起清掉，之前atlas丢失候选已获得执行日志支持。还同时整清pitch240别名；后段240pitch的后处理clear也抹其它同base深度view，说明需通用覆盖映射，不能只按shader特判。

下一实现须考虑EDRAM tile/sample布局及MSAA，不简单按pitch比率缩放整图。Xenia本地draw_util.cc1079起将4X的x与2X/4X的y扩为sample坐标再映射tile。当前日志未记录surfaceInfo.msaa，应补入诊断/用覆盖映射测试验证。本轮无渲染行为修改，不宣称阴影已修复。13472保留input20/shots4/capture1，旧5408也保留，所有route/capture工具结束，无提交。

2026-09-05 实现跨pitch深度clear覆盖映射：新增gpu/depth_clear_layout.h，按80x16 sample tile与1x/2x/4x布局，把源局部矩形切成目标clear rect列表；Draw记录depth view的MSAA布局，替换原整图clear。空映射必须跳过API（0个rect在API中代表全图）。保留现有depth-only清除语义，不改颜色别名清除。目标仍是单采样host texture，因此部分sample覆盖会清对应host像素；不声称完整MSAA仿真。

tools/tests/depth_clear_layout_test.cpp通过：右侧atlas清除保留左侧、pitch减半跨tile换行、目标外空覆盖，以及9种源/目标MSAA组合与独立逐sample地址枚举对照。日志out/depth-clear-layout-test.log。runtime构建out/build-depth-clear-layout.log通过，EXE 10F4D144B6E1BF99546D9FA6BA35556FD5C187A1A61DBD68D822B998470DD3E1。尚未运行新构建，不能宣称实际阴影已恢复；下一步新副本相同遇敌capture，核对日志msaa与左右atlas实际内容，再扩展火焰/营地。不要把仍活着的旧13472当修正版。当前CIM旧31720已消失，未调查退出原因；其余旧测试保留，无提交。
