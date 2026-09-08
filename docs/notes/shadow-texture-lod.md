# 阴影纹理采样 LOD（2026-09-05）

## v0.4.2 整合 — 2026-09-08

本次包含六条已核对的战斗地形／物件／蒙皮 TAA 路径。17,287 项 CPU 检查、原 Map3 战斗 32 相位对照及原轮胎回归属于下文标识的独立候选；这些结果不等于最终 v0.4.2 包的实跑。敌人消散闪烁仍未修复，原报告者及更广场景验收保持待办。实际包验证以[状态总表](../STATUS.md)为准；下文保留各阶段证据。

当前汇总见[成果报告](../WORK_REPORT_2026-09-05.md)和[状态总表](../STATUS.md)。下文按实验时间保留证据，早期未完成状态不代表最新结果。

2026-09-08 更新：已验收的 Map3 TAA 轮胎修复现已包含在正式发布的 [v0.4.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.1) 中；详见文末发布记录。其余独立阴影报告保持原状态。

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

## 2026-09-05 用户反馈与9693E361新证据
用户明确：所有地图都会出现地面角色投影消失，当前版本相比之前有改善。不是自阴影明暗反馈。
独立副本44712，out/ground-shadow-current-01，营地save/profile来源audio-request-01，EXE为9693E361。静止1293–1412共120帧；一次右移输入1后3211–3450共240帧。contact.png/movement-contact.png保留，包含雾、相机和行走变化，不用整体像素差替代投影判断。
完整capture1=f1737含3份f32与所有draw/resolve；shadow seq03为864x864，全有限，非1像素100455，其中0像素29805；图中保留多个人物轮廓。这只能说明该帧阴影图未被整张清空，尚未关联消失时刻，不能宣称剩余问题已定位。
下一步将角色地面投影可见/消失对应帧与shadow mask及深度源对齐，检查共用投影采样/stencil/resolve链。现场input1/shots3/capture1已完成，进程保留，未改渲染。


2026-09-05 移动触发补证：9693/44712 按右移90polls、左上(-22000,22000)70polls后，f12611/seq133地面mask主角只剩碎片，NPC完整；draw225已缺失。第一次atlas为seq130，CPU使用同帧深度和第一个PS矩阵可投出完整主角及长影（out/ground-shadow-current-01/cpu-projection9.png，近似单点采样，不包含模板/裁剪）。371E/46136同路线f1489复现；实际投影/volume clip control皆0x80000，clip_disable翻译无效，渲染试改已撤回。下一步模板与边界几何，尚非修复完成。


### 2026-09-05 移动后地面投影修正：14F09F15

- 根因：`RB_MODECONTROL=5`应忽略上次IM_LOAD的PS。旧代码仍绑定PS1936817ead3b7b7d，它的SV_Depth来自插值r1.z；模板体VS97f07e5d73418e64不写这个插值，实际得到残留常量深度，破坏depth-fail模板计数。后续体可能继承无深度导出的d55 PS，因而角色/绘制顺序/位置改变时表现不同。
- 修正：只在mode4解析和绑定guest PS。mode5仍执行原几何、深度和模板状态。撤回无效clip试改与绕过模板诊断开关。捕获增加36索引体几何及最多24个stream dword、clip寄存器，正常渲染无调试覆盖。
- GPU定向测试：`LoStencilTest`增加几何z=0、存储z=0.5、GREATER_EQUAL条件；残留PS导出z=1会走相反模板路径，无PS正确使用几何深度；两组均得到预期像素，原非零reference测试也通过。另两个深度清除测试通过。日志`out/shadow-depthonly-*-test.log`。
- 实际修正版：`out/ground-shadow-depthonly-fixed-01/Playtest.exe`，SHA256 `14F09F15CFB23B451821E013E5C9F02CAD906D39D487F52916EB1142D4371084`。f4738/seq04、f7128/seq22、f8354/seq40为各帧第一份b0d9000投影遮罩，均有主角及地面投影。f8354共554个draw，161个mode5全部PS=0。截图两组共330帧；并非所有帧逐帧审核，已检查移动取样及三个完整捕获。
- 路线控制：右90polls，左上70polls，然后再左上70、右下25。相同polls因实际位置/碰撞/运行节奏导致路线端点有偏差，图像对照是同营地相近位置，不是相同相机矩阵的逐像素A/B。原失败帧9693/f12611和371E/f1489已保留。修正后主角投影在多个移动后位置保留，完整跨地图、遇敌和长期稳定性不在本次证据范围。
- 原图与对照：`out/ground-shadow-depthonly-fixed-01/comparison.png`，`mask1/2/3.png`、`screen1/3.png`；失败原图`out/ground-shadow-current-01/mask9.png`。没有提交或推送，也未改用户原始save/profile。
- 参考：Xenia `draw_util.cc::IsPixelShaderNeededWithRasterization`只在kColorDepth使用guest PS，见 https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/draw_util.cc 。本地参考`out/shadow-reference/draw_util.cc`。

完成捕获后核对完整路径，停止本轮旧研究副本44712/46136/35500以释放GPU；修正版42672保留在后台营地，input4/shots2/capture3完成，未操作其他历史进程。


## Map 12/13 新反馈与初步诊断

2026-09-05：用户确认当前人物脚下投影基本修复；Map 13 动态阴影落在人物表面仍闪烁。Map 12 实时过场墙面/海报有硬边黑斑闪烁，用户截图保留在 out/map12-blackpatch-01，尚未证明两者同源。

328352E9 独立 out/map13-shadow-01 使用当前用户存档副本，实际是峡谷早期存档，不是 Map 13。f4112 完整捕获471 draws，其中143个 mode5 全部PS=0；screen深度921600值、shadow深度746496值全部有限。60连续截图已保存，不能以此关闭Map 13问题。32416捕获后按完整路径核对停止。

改用较后方的 out/audio-request-01 营地存档副本，out/map13-shadow-02 PID45076仍为328352E9。实际地图日志4→5装甲车→9乌拉大门，已恢复行走；尚未进入Map12/13，无新渲染修正。启用截图/全draw捕获/地图日志/磁盘shader缓存，但遗漏LO_TELEPORT_COMMAND_FILE，当前副本尚未实际使用POI传送。用户授权沿既有攻略继续并使用POI；下一次启动需启用完整诊断接口。


2026-09-05 用户操作 Map12 现场：PID40932 / FB225C29，地图日志确认 id12 Monorail - The Great Gate Station。请求301捕获连续120帧17970–18089及17970完整draw/resolve/原始深度，保存在out/user-shadow-session-20260905-161210。map12-poster-pair.png对比17970与17990，海报人物头部明显出现不规则深色块，静止视角附近随帧改变；视觉复现确认。不能据此判定与Map13同因或属于真实动态阴影，根因未定。完整绘制捕获只有17970，不能代替17990故障帧绘制数据。用户控制游戏，未输入或移动人物。


2026-09-05 Map13周期阴影闪烁已捕获：用户明确环境阴影扫过每个人物时闪烁。请求303连续600帧94701–95300；map13-shadow-adjacent.png中94925/94926/94927/94928凯姆右臂/裤腿连续暗→亮→暗→亮。与角色姿势小幅变化不成比例，确认视觉异常但根因仍未确定。原版静态参考original-map13-reference.png不能证明原版时序表现。当前RT用Texture2D默认RenderMultisampling COUNT_1（renderer.cpp1100/plume_render_interface_types.h781），MSAA未完整还原是候选，不能把整片明暗跳变直接定为AA；需要阴影深度/过滤与覆盖对照。


730E2653用户Map13实际复查：PID31336，请求401连续600帧3797–4396，相同附近机位；4213→4214→4215右臂/裤腿亮→暗→亮仍存在（shadow-flicker-confirmed.png）。固定裤腿ROI相邻均值差>8：旧24/599，新18/599，反向连续大跳各2次；不能用数量差认定改善，非严格同动画相位AB。max step旧13.04新13.24。窗口修正未消除Map13闪烁，与Map12同源仍未证明。证据out/user-window-fixed-20260905-171424/shadow-comparison.json及shadow-flicker-confirmed.png。

## 2026-09-07：0.4.1-dev TAA 轮胎与小物件修正

后续状态澄清：本节保留第一候选交付时的验证边界；随后用户原场景验收确认仍闪，已继续修复，详见下节。不能再将第一候选记为待验收或已解决。

本机 RTX 5080 的 v0.4.0 导出 `f14774` 对应实际 2560×1440 场景。用户报告轮胎与远处小物件的阴影闪烁，并在同站位关闭 AA 后确认不闪。逐层核对发现，轮胎深度与补光使用相同网格和相机，但补光 VS `a27a7234977e0d4a` 的 c7 VP 未参与 jitter；阴影遮罩 VS `99c2b4b0960a9ccd` 将偏移后的 clip position 同时用于光栅化与深度读取，而 PS `d55a20d004031279` 的位置重建仍使用未补偿常量。单帧导出不证明这两项对可见闪烁各自的贡献。

用户随后授权修复，目标版本为 0.4.1。当前本地 `0.4.1-dev` 补齐上述补光 VS 及已核对的 `3148f81d65d3b5f4` c233 VP，并将逐帧偏移与阴影补偿放入共用生产 helper。对精确匹配的遮罩 shader 对，仅调整 PS c4 为 `c4 - jitterNdcX*c2 - jitterNdcY*c3`，保留深度 UV／线性化参数。修正继续检查相机、viewport、深度 allocation 和实际采样的当前场景深度；采样视图须为完整 2D guest 尺寸，裁剪视图不能继承原 resolve 的授权。guest viewport 判断先于独立 Z bias，不移除原有深度偏置。

整合 Release 构建通过；`LoTemporalJitterTest` 共 6,463 项 CPU 检查使用捕获常量和独立 shader 数学验证 32 个相位、四种分辨率下的绘制层对齐、阴影重建、深度 UV 与拒绝条件。Scene fixture、75 项相机数学、141 项历史诊断，以及 GPU temporal resolve 和 polygon offset fixture 均通过。

冻结开发包 EXE `6d3bc037…` 在有效 Map2 存档点完成 Auto 1440p 的 TAA／AA Off 检查，实际加载的 EXE、DXC 和 DXIL 哈希已核对，两种输出均经目视检查。TAA 的 32 个阴影相位全部应用补偿，上传常量与 float32 公式完全一致；Off 的 32 个样本均保持 VS／PS 常量不变。该位置没有原报告的轮胎补光 pass；依据导出坐标尝试定位原网格未成功，不能算原场景实跑通过。对应轮胎层的证据仍限定为捕获常量的 CPU 回归，修正版原位连续画面与玩家验收待完成。TAA 继续为实验功能，当前改动未发布。包身份与限定证据见[状态总表](../STATUS.md)。

诊断与测试说明见 [tools/tests/README.md](../../tools/tests/README.md)。本地证据保留在 `out/tire-flicker-20260907-f14774/REPORT.md` 和 `out/v0.4.1-tire-fix/`，原始私人导出不纳入公开文档。另一个 AMD 首战身体阴影导出 `f3182` 仍未确定时序根因，用户要求挂起；本次实现不关闭该报告，也不重新判定前述 Map 13 调查。

## 2026-09-07：第一候选原场景验收失败

本节保留第一候选失败及继续调查时的状态；后续 r2 已通过本机原轮胎位置用户验收，见文末验收记录。

用户确认第一候选仍闪。现场 PID 65176 的实际 EXE 为 `6d3bc037…`，TAA／60 FPS／Auto 1440p，地图为 Map3；新导出 `f24389` 和 17:04 存档已保留并核对。此为明确的原场景验收失败，轮胎／小物件问题仍开放，不因此前 CPU 或 Map2 检查通过而关闭。独立 AMD 首战与另一 RX 9060 XT 报告继续分别挂起，本机 Map3 修复获得新的继续授权。

独立存档副本已到达同 Map3、同位置。32 相位中的 128 对基础深度／补光绘制均已应用 jitter，VP 上传逐字匹配；32 次阴影补偿与 float32 公式一致，没有 frame-gap reset，但轮胎遮罩仍随相位变化。新发现的 `ff9da3984ce8d094` 轮胎材质和 `118a37c0d32c0477` 角色材质绘制会写入场景深度，却尚未纳入 jitter。下一候选正在补齐这两层；此检查点没有修正后对照或验收结果。

证据：`out/v0.4.1-tire-v2/live-identity.json`、`fixed-map3-60/audit.json` 与 `fixed-map3-60/pixel-audit.json`。第一候选及其历史验证继续保留，用作失败基线。

## 2026-09-07：r2 补齐材质层与同 Map3 对照

本节前半保留 r2 交付前的对照检查点；随后完成的 AA Off 检查和用户验收追加于下节。

完整检查 `f24389` 的 76 个标准场景相机深度写入绘制，发现八个漏项：七个轮胎／小物件 `ff9da3984ce8d094` 绘制及一个角色 `118a37c0d32c0477` 绘制。这些 shader 和相应轮胎绘制已存在于最早的 `f14774`，七份相关 HLSL 在两次导出中逐字一致；这是首次调查／修正覆盖不完整，并非游戏新换了 shader。r2 将两者分别映射至 c7 和 c233，保留相机、viewport、allocation 和阴影深度限制，不对光源空间 atlas 绘制施加场景 jitter。

r2 的整合构建、8,192 项 `LoTemporalJitterTest` 和 Scene fixture 通过。新增 CPU 检查独立模拟基础深度、不透明材质和补光的位置运算，覆盖两份实际轮胎 world 矩阵、32 相位和三种尺寸；也比较角色材质／补光投影，验证 Z/W 保持及 atlas 拒绝。旧材质层不偏移时可产生最高 0.487799 像素错位。

实际 EXE `da681c89…` 在同用户存档、同 Map3 位置，TAA／60 FPS／Auto 1440p 下记录完整 32 相位：128 个基础深度、64 个轮胎材质、128 个补光和 32 个阴影绘制均已应用偏移，配对 VP 上传逐 bit 相同，阴影补偿公式完全一致，没有 frame-gap reset。近处轮胎最终 TAA ROI 的 32 相位平均亮度范围从第一候选的 2.2184 降至 r2 的 0.19284（0–255）；单像素范围仍非零，不能据此宣称零闪烁或全面修复。r2 尚待玩家视觉验收，第一候选的失败结论继续保留。

开发 ZIP 为 `out/v0.4.1-tire-v2/delivery/LostOdysseyRecomp-windows-x64-v0.4.1-dev-r2.zip`，包含另项已实现的三帧导出；45 个包文件和 importer self-test 通过，包身份见[状态总表](../STATUS.md)。详细证据为 `out/v0.4.1-tire-v2/capture-review/REPORT.md`、`r2-map3-60/audit.json` 和 `r2-map3-60/pixel-audit.json`。本检查点的 r2 AA Off 包实跑仍在补充，不提前记为通过。

## 2026-09-07：r2 原轮胎位置用户验收通过

用户在保持 TAA、原轮胎位置复查后明确回复“不闪了”，并说明游戏仍在运行。已核对用户安装目录中运行的 PID 43376：17:25:16 启动，实际 EXE SHA256 为 `da681c893b7d9187dd21c7813271e32f292ee1b3b6f740d25aa4d88f9dcd5d02`，与 r2 包一致。五个 save／profile／settings 文件哈希未变，EXE 由用户更新为 r2。**本机 Map3 轮胎闪烁缺陷已解决并获用户验收**；第一候选验收失败的历史不改写。

r2 实际解压包的同位置 AA Off 检查也已完成：352 条绘制上传（含 64 条 `ff9da` 材质）均为 Disabled，VS／PS 常量字节不变；近、远轮胎遮罩在 32 帧中逐像素一致，并与第一候选 Off 对照逐像素相同。Off 实跑的 EXE／DXC／DXIL 真实模块与哈希已核对，TAA 和 Off 两组场景截图均经目视检查。四个本轮独立测试进程已主动结束，用户正在运行的验收进程未触碰。证据：`out/v0.4.1-tire-v2/r2-off-map3-60/audit.json`、`off-comparison.json`、`r2-modules.json` 和 `user-preserved.json`。

其他地图、动态画面和硬件列为后续回归，不作为继续挂起本缺陷的理由。原 AMD 首战及另一 RX 9060 XT 报告仍分别挂起；当前 r2 为未发布的 `0.4.1-dev` 开发包，没有提交或发布状态变化。

## 2026-09-08：v0.4.1 正式发布

[v0.4.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.1) 于 00:03:50 UTC 从 `eb43f108d2cdd3aef682a32b202425c28d168472` 发布，包含 r2 已验收修复及三帧日志导出。正式 EXE 为 `9e0e13d991830de84d7fb85ac7a2543f779dbf7936ee5acd4cabe7cce5b2c57f`；与 r2 相比，渲染和捕获源码未变，源码版本改为 `0.4.1`。CI、45 项包文件、安装器及 8 项无图形启动路径检查、匿名下载核验通过。本轮正式包没有加载游戏或重做 GPU／玩家验收，画面依据仍为上方 r2 的同 Map3 对照与用户确认。独立 AMD 报告继续挂起；包哈希及证据见[状态总表](../STATUS.md)。

<a id="battle-terrain-flicker-v041"></a>

## 2026-09-07（本地）：v0.4.1 战斗地形闪烁诊断

发布后用户报告新的战斗闪烁。现场 PID 17424 的 EXE SHA256 为 `9e0e13d991830de84d7fb85ac7a2543f779dbf7936ee5acd4cabe7cce5b2c57f`，与正式 v0.4.1 一致；目录名称中的旧开发版本不能代表实际程序版本。设备为 RTX 5080，输出 2560×1440，AA3。原导出 `render-17888272305412556-f2871.zip` 的 223 个归档条目通过 CRC 检查，2871–2873 连续三帧的 57 份 raw resolve 尺寸正确，深度全部有限，未见 dropped draws。

原导出的最早颜色 resolve（seq00）已经出现地面／山体大片明暗变化，早于阴影遮罩生成、guest 后处理和最终 TAA。用户随后授权读取当前画面，现场截图显示战斗指令菜单和大片地形黑块；通过 F1 补充的 26786–26788 三帧导出通过 193 个条目的 CRC 检查，51 份 raw resolve 尺寸正确。26786 的山体区域纯黑，26787／26788 恢复，角色与 UI 相对稳定。这确认了新的可见战斗地形缺陷，但尚未确定全部异常像素的绘制归属。

独立 shader 审查确认同一组 18,678-index 几何的深度、材质和补光具有相同 index／vertex fetch、world 与 VP。深度 VS `f7fd88506d704a3d` 的 c4 已支持 jitter；材质 VS `400df7c5a60819f5` 与补光 VS `08dcef32bd434f8c` 的实际位置变换均使用 c7，却未纳入支持名单。深度与材质均使用 `GREATER_EQUAL` 并写入深度，补光使用同一比较但不写入深度。仅深度层偏移可使同坡度表面成片无法通过后续深度比较，与早期颜色输出的异常相符；这是已确认的覆盖漏项和强候选机制，尚非捕获 GPU 深度测试的最终根因证明。

F1 记录的是临时上传修改前的 guest 常量；AA3、scene-ready 或 TAA 输出存在都不能代替实际上传证据。三帧导出的 readback／写盘停顿超过 250 ms 时会影响后续帧 TAA／jitter 条件，因而首帧严重、后两帧恢复不能当作正常运行的无扰动时序对照。后续需核对同场景深度／材质／补光的实际上传常量，完成保留现有 viewport／深度身份限制的修正版 A/B 及 AA Off 对照，再关联最早颜色输出与用户可见变化。本次未改运行时代码、构建、更改 AA、替换 EXE 或提交发布，游戏保持运行；没有修复或新的玩家验收结论。

**本项作为新的 RTX 战斗缺陷待修复。** 原 Map3 轮胎缺陷的用户验收继续有效；两个旧 AMD 报告仍分别挂起，未证明与本项同因。诊断材料保留在本地 `out/battle-flicker-20260907-f2871/`，包括 `REPORT.md`、`shader-review/REPORT.md`、两份 `inventory.json` 和 `live-f26786/full-comparison.png`；私人原始捕获不纳入公开文档。

<a id="battle-taa-fix-dev"></a>

## 2026-09-07（本地）：战斗 TAA 六条路径修复，CPU 检查通过

用户随后授权“修”。本地 `0.4.2-dev` 在 `temporal_scene.h` 补齐六条已按 HLSL 核对的位置路径：`f1b330b3ceea9a3b` 使用 c4；`8b5577db3ced3327`、`400df7c5a60819f5`、`08dcef32bd434f8c` 使用 c7；`0eb223d33f8e8e0c`、`1e9017d2b296f480` 使用 c233。现有 jitter 算法、viewport／VTE、相机位元、场景深度身份和精确遮罩 shader 对限制均保留；不会因为常量中残留 VP 就放行屏幕空间 shader。

`LoTemporalJitterTest` 通过 17,287 项 CPU 检查，覆盖 32 相位和 720p／1080p／1440p／4K。使用捕获常量独立计算三层位置后，修正的深度／材质／补光结果完全一致，Z/W、材质补光的 clip-derived 采样坐标、蒙皮路径与 Off／拒绝条件均受检查。旧材质不偏移的负对照产生最高 0.487771 像素层间分离；修正后的实际 raster offset 相对目标 jitter 最大误差为 0.000426 像素。这是两个不同指标，均非游戏画面改善幅度。原轮胎与 d55 阴影重建 CPU 用例继续通过。日志：`out/battle-taa-fix/LoTemporalJitterTest.log`。

`renderer.cpp` 增加 `LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE=1`，可在定向 resolve trace 活跃时记录已提交绘制的实际常量。`slot` 保留生产映射结果，`log_slot` 仅指明诊断读取槽位，使拒绝新增 shader 的旧基线也能记录原始／上传 VP；诊断不会强制 jitter 或绕过保护条件。用法见[测试说明](../../tools/tests/README.md)。

候选使用独立 v0.4.1 源码快照，排除并行的 Issue #7 崩溃诊断改动，版本为 `0.4.2-dev`。本检查点仅确认实现与 CPU 验证；实际上传、修正版／Off 连续画面对照、Map3 轮胎回归和实际开发包验证仍在进行，尚无玩家验收、提交、推送或发布。上节诊断保持其原日期边界；本战斗缺陷暂不关闭，已验收轮胎与两个分别挂起的 AMD 报告不合并。

<a id="battle-taa-runtime-dev"></a>

## 2026-09-07（本地）：原战斗场景 32 相位对照与开发包验证

隔离源码构建通过，六项选定用例 `LoTemporalJitterTest`、`LoTemporalSceneTest`、`LoTemporalMathTest`、`LoTemporalHistoryDiagnosticTest`、`LoPolygonOffsetTest` 和 `LoTemporalAATest` 均 exit 0。开发包为 `out/battle-taa-fix/delivery/LostOdysseyRecomp-windows-x64-v0.4.2-dev-battle.zip`，42,581,592 字节，SHA256 `0bb589145af79de9badd637fca02cd6043740a47700cb1e76bb3d887ed435969`；45 项 payload 哈希、归档检查和 importer self-test 通过，不含游戏或用户数据。修正版和 Off 均运行包内实际 EXE `fe39a9c90a04c6f27b3d370b41014ab0fa8ddee1aa3f903daa78ee7532d4bb74`。保留 v0.4.1 渲染行为并加入同类诊断的基线 EXE 为 `23f34ff7adef300775c9a4241a206caeb93a482a22f7cacceaa0519904534339`，不是正式发布 EXE。

使用当前存档副本在 Map3 随机遇敌，复现原报告的山体。三个独立进程均为实际 2560×1440，每组覆盖连续 32 帧及全部 jitter 相位。山体 ROI 为 `(200,300)-(900,650)`，近黑定义为原始线性 RGB 最大分量小于 `0.0001`。

| 组别 | 帧区间 | 每帧山体近黑比例范围 | 至少半区变黑的帧数 | 三层上传 VP 一致 |
| --- | --- | --- | --- | --- |
| v0.4.1 渲染基线／TAA | 4945–4976 | 0.056327%–100% | 16/32 | 0/32 |
| 修正版／TAA | 4933–4964 | 0.049388%–0.097959% | 0/32 | 32/32 |
| 修正版／Off | 4930–4961 | 0.055102%–0.091837% | 0/32 | 32/32 |

三层按同一 18,678-index 几何核对。基线 224 条选定 draw 中仅 96 条应用 jitter；修正版 224/224 应用，深度／材质／补光 VP 上传全部符合独立 float32 计算并逐 bit 匹配；Off 的 224 条 VP／PS bank 均保持原字节。相关 Z/W 和阴影重建补偿检查通过。此证据支持该覆盖修复消除本次取样到的成片山体黑块；不等于整场战斗或全部 shader 验收。

三组日志观测到的相邻 upload／resolve 时间戳最大间隔分别为 97／121／84 ms，没有观测区间超过 250 ms。这些是精度为 1 ms 的 CPU 日志节点，不能代替 GPU 帧时，也未覆盖首条上传前的间隔。实际 TAA 输出已被捕获，但这些帧没有 temporal summary 日志，history reuse 与 summary gap 标志保持未知，不能从缺少记录推断为成功或未重置。跨进程相机／动画时间不同，同相位不代表同一相机，不能宣称逐帧像素相同。

实际开发包另行通过同 Map3 原轮胎位置回归：f2519–2550 覆盖完整 32 相位，352/352 条日志上传应用 jitter，64 组三层轮胎 VP 全部一致。该轮胎运行的 32 条 temporal summary 均 ready／completed／reused=true、gap=false、jitter_misses=0；这不能补足前述战斗窗口缺少的 summary。按相位与已验收 r2 比较，近／远轮胎 mask 和 depth 四组 ROI 均 32/32 逐字节相同，轮胎逐相位对照图经目视检查稳定。火粒子时间不同，source／TAA 颜色不宣称逐字节一致。证据：`out/battle-taa-fix/map3-regression.json`。

该交付检查点的七个受保护原安装文件哈希未变，全部自有游戏进程已结束。当时本轮玩家验收未完成，开发包未提交、推送或发布。此前轮胎已验收结论继续有效，两个旧 AMD 报告仍分别挂起。入口与保留检查见本地 `out/battle-taa-fix/REPORT.md` 和 `handoff-verification.json`；运行与构建证据为 `runtime-comparison.md`／`.json`、`build-fixed.json`、`test-results.json` 与 `delivery-audit.json`。初期无效路线不纳入上述通过结果；后续安装版本与新反馈见[消散诊断](#enemy-death-f5446)。

<a id="taa-four-disc-audit-20260907"></a>

## 2026-09-07（本地）：四盘 TAA 静态覆盖审查

按用户新增要求，独立扫描实际安装的 disc1–disc4：全读 52 个 FPD、核对四个 FPI 和 30,399 个 CPX 区段，完整解码按编码内容去重的 10,060 个 CPX。识别到 20,686 个原始资源 shader，加上四个静态 XEX shader 与现有规则可枚举的 2,245 个 VS 派生，共生成 22,935 份 HLSL（2,859 VS／20,076 PS）。原始资源 hash 集合与独立运行提取一致，对应 shader 源字节逐字节一致；额外 observed／cache-only 变体另保留来源。覆盖边界是现有容器格式与 microcode 校验器识别到的四盘资源，未知格式及所有动态链接组合不作完备性保证。

全部 VS 位置组件依赖被审查并归为 408 个位置程序组，另审阅全部 20,076 个 PS 和一份单独的 cache-only PS。候选仍包括七个 observed VS 位置路径、19 个额外阴影重建 PS、三个平面深度投影 PS、一个 motion／depth blur PS，以及独立 fullscreen 雾 ray 合约。阴影变体有不同常量布局，不能统一照搬 d55 的 c4 修正；不同相机／viewport 和残留 VP 的负例已排除。雾合约在六帧捕获常量、32 相位、四分辨率的 CPU 对照中 RGBA 最大差为零，不支持它是本次战斗黑块的原因。

这些是待取得实际配对、相机／深度／viewport 和上传证据的候选，未因此扩展生产名单，也不是额外已确认视觉故障。612 份 HLSL 带 translator notes，其中 611 份为原始 VS 尚未运行链接的 fetch/link 信息，另一份 PS 含未知指令／export；不能把这些提示一律计作编译失败。控制流和动态常量分析仍有边界，本次没有对全部 HLSL 执行 DXC 或 GPU draw，也未进行全游戏视觉验收。可跟踪的[覆盖审查](taa-coverage-audit.md)保留来源、候选及后续运行链接实例；完整本地报告与逐 shader 来源保留在 `out/taa-whole-game-scan/REPORT.md`、`vs-families.md`、`pixel-contract-review.md` 和相应清单。

<a id="enemy-death-f5446"></a>

## 2026-09-07（本地）：0.4.2-dev 敌人消散仍闪烁，开发分支状态

用户新增要求将本次改动提交到 `taa-fix`，当前为本地开发分支、未发布，玩家验收待完成。生产实现仍只有上文六条位置路径和诊断日志，17,287 项检查结果保持原覆盖范围；没有新增敌人消散修复。用户已自行用候选 EXE `fe39a9c90a04c6f27b3d370b41014ab0fa8ddee1aa3f903daa78ee7532d4bb74` 替换安装版本，审查时无游戏进程；前述“原七文件哈希未变”仅属于此前交付检查点。

新 ZIP `render-17888399554225865-f5446.zip` 的 SHA256 为 `f5a2149426d13b4e4c48262f894cacc42a5117c4ccc346a1f4c5f4fdf26b5ee1`，224 项归档 CRC 通过；f5446–5448 连续，54 份 raw resolve 尺寸正确、深度有限，来源为 `0.4.2-dev`／AA3／2560×1440。固定 x16 曝光与 gamma 2.2 检查原始 float16 HDR，f5446 敌人的胸甲、肩甲／武器已有整片黑多边形，后两帧恢复纹理。最早颜色 resolve（draw93）已见异常，早于 shadow mask、seq08 蓝紫粒子及最终 TAA；不把后期阶段当作首发位置。

三帧各 310 draw／34 VS 的完整位置审查只新增一个运行 hash：`4bd8985d84983b83`。它最终使用 c230–c233 输出主相机位置，每帧 draw10–13、30–33 关闭颜色写入并执行 GREATER_EQUAL 深度写入；实际位置槽、viewport／VTE 和 guest depth 条件匹配，但当前 `PositionVPSlot` 未覆盖。与后续已支持 c233 的基础／补光层，已按索引、fetch 描述、world 及骨骼前缀配对；F1 缺少 vertex/index 内容，不把前缀相同扩展为所有顶点访问或整个 bone bank 一致。

PS `8ead384aedf2bb23` 使用对象 UV 的三纹理 clip，没有场景深度／屏幕坐标重建。draw10–13 的 c3.x 约为 `0.04801565 → 0.05117329 → 0.05707855`，与对应本体层同步；draw30–33 保持阈值 1，属于另一个稳定阈值实例。67 个捕获 PS 的 HLSL 均与全盘对应文件逐字节一致；72 组配对的 UV 源、纹理完整描述及 clip 参数匹配，10,080 个 float32 边界算术样本无决策分歧。这是参数与算术核对，不是实际纹理采样或 GPU replay。此证据优先支持深度与材质投影覆盖不一致的候选；还没有这些帧的 actual upload，不能直接断言每帧哪些层已经 jitter 或哪个像素被 depth test 拒绝。

运行 hash `4bd8985d84983b83` 在 fetch 入口之后的完整 guest ALU、控制流与导出文本，与盘内已枚举原始 VS `adc97a079302f52e` 一致。它是原始 c230 族产生的新链接结果，说明有限派生枚举不包含所有运行组合，不能据此说四盘资源漏扫。持久来源和未实现的链接来源／draw 覆盖方案见 [TAA 覆盖审查](taa-coverage-audit.md)；408 个归一化位置组不能直接用作语义白名单。

三帧 F1 各停顿约 0.706–0.900 秒，会改变 250 ms gap 条件；相机、姿态和消散阈值也在推进。新缺口已确认，黑面首发阶段已定位，但根因仍需正常时序实际上传、32 相位与同场景原版／候选／Off 对照，尚未视觉证明或修复。已验证山体结果与原轮胎用户验收保留，两个旧 AMD 报告继续分别挂起。本地证据：`out/enemy-death-f5446/inventory.json`、`shader-review/REPORT.md`、`pixel-review/`、`enemy-localization.png`。
