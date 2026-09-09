# TAA shader 覆盖审查

记录日期：**2026-09-07（本地）**。当前实现位于本地 `taa-fix` 开发分支，开发包版本为 `0.4.2-dev`，尚未发布；新战斗修复的玩家验收待完成。生产改动仍为六条已核对的位置路径及 draw 诊断日志。本报告整理静态扫描与后续捕获证据，不表示已修复所有候选。已验证山体与原轮胎的结论见[运行记录](shadow-texture-lod.md#battle-taa-runtime-dev)，当前状态见 [STATUS](../STATUS.md)。

## 来源与分母

独立扫描实际安装的 disc1–disc4，完整读取 52 个 FPD，核对四个 FPI，验证全部 30,399 个 CPX 区段，并完整解码按编码内容 SHA-256 去重后的 10,060 个 CPX。没有用运行缓存或稀疏位置索引代替资源扫描。

| 来源 | VS | PS | 合计 |
| --- | ---: | ---: | ---: |
| 原始资源 | 611 | 20,075 | 20,686 |
| 静态 XEX 去重新增 | 3 | 1 | 4 |
| 现有规则可枚举派生（354 fixed、1,891 linked） | 2,245 | 0 | 2,245 |
| 资源与有限派生全集 | 2,859 | 20,076 | 22,935 |

原始资源 hash 集合与独立运行提取一致，22,935 项对应 shader 源 `.bin` 字节逐字节一致。运行缓存额外的 25 VS 和 1 PS 单列来源。该核对是源字节一致性，不是所有 HLSL 已通过 DXC 或 GPU 执行。

完整性限于现有容器格式及 microcode 校验器识别到的四盘资源。盘内资源、有限派生、实际运行 linked shader 和场景 draw 是不同分母；未知格式、未触发的链接组合及未访问场景不作穷尽承诺。

## VS 位置程序与候选

全部 2,859 VS 按实际到达 guest `oPos` 的组件依赖审查：1,860 个直线矩阵候选、765 个直接顶点位置、216 个控制流未证明、18 个常量／VertexID 位置。504 个精确位置 slice 在忽略 fetch 布局和临时寄存器名后归为 **408 个位置程序组**。这些组没有证明相机、几何、viewport 或 depth allocation 相同，不能直接作为语义白名单。

此前捕获与额外缓存合计 43 个 observed VS，其中 28 个不在有限资源／派生集合。当前 29 个位置白名单 hash 中仅 7 个落在该有限集合；“1,853 个未列入名单的直线矩阵 hash”不能换算为运行覆盖率或故障率。

此前 observed 审查保留七条位置候选，尚未因此加入生产映射：

| 最终位置槽 | Shader | 尚缺证据 |
| --- | --- | --- |
| c7 | `3c86f4a89d220ee8`、`81217dc973d5dc31`、`e7b38eb08c70e5e1`、`f3b9f20b3d3a62d5` | 实际 draw 的相机、深度身份和上传 |
| c8 | `8d9770d1bd8ba0fa` | 实际 pass 与主场景配对 |
| c233 | `81bc335604d04e8b`、`87a76ceaf1eaec11` | 骨骼／控制流边界、实际 pass 与上传 |

`481be7e343ab5ba3` 的 c234 使用其他小 viewport／相机；`73a203360434358c`、`eb5f611c4321708e` 真正位置使用非主相机 c0，不使用残留 c8／c233，后者还有不兼容 Z viewport。这些负例已排除。`6318b7b358aa8b45`（雾）与 `9b81c55ca39bb529`（DOF）直接输出输入位置，也不能因常量中存在相机值就启用位置 jitter。

## PS 与屏幕坐标候选

审阅全部 20,076 PS，并将 cache-only PS `f1cc033418e7b30f` 单列为普通纹理乘色。保留的重建候选为：

- 阴影重建共 20 个，包含已支持的 d55 与 **19 个额外变体**，有八组常量布局。缺少每个实际场景的纹理绑定、配对 VS 和上传；不能统一套用 d55 的 c4 修正。
- 三个平面深度投影 PS：`85f3f7dde9660bc1`、`b12942d47bfc9abe`、`950be0f3b9264def`，仍需核对原始 clip XY/W 与重建坐标的配对合约。
- 一个 motion／depth blur PS：`2d7c981216326bf2`，涉及当前／历史投影与分支，需独立运行验证。
- 独立雾 ray 合约：VS `6318b7b358aa8b45`／PS `c55d2c256efff0d7`。六帧捕获常量在 32 相位、四分辨率 CPU 对照中的 RGBA 最大差为零；合成正对照存在敏感性。这不支持把该雾路径认作已观察战斗黑块的原因。

另一个 DOF 半径及三个 normal／refraction 路径未据此判定为重建漏补偿。以上数量是候选数，不是已确认视觉故障数。

## 2026-09-07：敌人消散暴露有限链接边界

用户在实际安装的 `0.4.2-dev` EXE `fe39a9c9…` 上仍观察到敌人消散闪烁。f5446–5448 三帧归档校验通过，54 份 raw resolve 尺寸正确、深度有限。固定曝光检查显示 f5446 的胸甲、肩甲／武器出现整片黑多边形，后两帧恢复纹理；黑面在 draw93 对应的最早颜色 resolve 已出现，早于阴影遮罩、后续蓝紫粒子与最终 TAA。这是新场景缺陷，尚未实施新的修复。

新增运行 VS **`4bd8985d84983b83`** 以 **c230–c233** 计算最终位置。每帧 draw10–13、30–33 的实际位置槽与主相机位匹配，viewport／VTE／guest depth 条件符合场景深度预处理；当前生产名单未覆盖它。后续本体使用已覆盖的 c233 路径，索引／fetch 描述、world 及已检查骨骼前缀有明确配对。F1 不含实际顶点索引访问和修改后的上传，不把前缀匹配扩展为完整 bone bank 或所有顶点等价。

它在 fetch 入口之后的完整 guest ALU、控制流及导出文本与盘内原始 VS **`adc97a079302f52e`** 一致，差异位于链接后的 vertex-fetch 入口。原始资源早已进入 c230 候选族；新运行 hash 超出有限派生枚举，不能据此断言此前漏读了某张盘。这也说明仅维护一份静态 hash 列表无法覆盖所有运行链接结果。

PS `8ead384aedf2bb23` 的消散 clip 使用对象 UV 的三纹理采样，没有场景深度／屏幕重建。draw10–13 的 c3.x 约为 `0.04801565 → 0.05117329 → 0.05707855`，与对应本体阈值同步；draw30–33 保持 1，不能把两个实例都称为正在消散。67 个捕获 PS 的 HLSL 均与四盘扫描对应文件逐字节相同；72 组 depth／color 配对的 UV 源、三纹理完整描述与 clip 参数一致，10,080 个 float32 边界算术样本无 clip 决策分歧。这不证明实际纹理内容、GPU 采样或不同投影层在同一屏幕像素的 UV 一致。此时优先候选是未 jitter 的深度与已覆盖材质可能使用不同投影网格。

缺口已由位置合约与捕获确认，实际致因仍需上传及同场景原版／候选／Off 对照。F1 每帧停顿约 0.706–0.900 秒，会影响 250 ms gap 条件，三帧不是正常动画时序或 32 相位对照。没有新的 actual-upload 证明、正常时序 32 相位证据或 Off 比较，也未证明该缺口解释全部黑面。原山体修复的有限实跑验证、用户已验收轮胎和两个分别挂起的 AMD 报告均保持原结论。

## 分析限制与后续覆盖方案（未实现）

全部资源已翻译，但 612 份 HLSL 带 translator notes：611 份原始 VS 尚缺运行链接的 fetch／link 元数据，另一份 PS `78af7d75d932c582` 含未知 scalar opcode／export。提示不等于统一编译失败，也不证明语义有效。静态组件依赖和 PS taint 是候选筛选；控制流、动态常量和未知指令仍有边界。本次没有全量 DXC 编译、GPU draw 或全游戏视觉验收。

现有 `jitter_misses` 只统计 `temporalSlot >= 0` 的已知路径，未知 VS 被排除；因此旧日志中的零 misses 不证明所有 VS 已覆盖。当前 draw 日志的默认 hash 过滤同样不是全体发现机制。

<a id="shader-log-coverage-todo"></a>

### 2026-09-08：独立 shader 日志与覆盖审计 TODO（未实现）

按用户要求，将已有覆盖设计与独立日志方案合并为以下待办。全部尚未实现，未进行本方案的构建或运行验证；现有 runtime 日志轮转和 F1 的 runtime 日志快照不代表已经具备独立 shader 日志、成组保留或累计覆盖清单。路线图保留[英文](../ROADMAP.md)／[中文](../ROADMAP.zh-CN.md)各一个入口。

1. **分离日志与来源。** runtime 保留启动、崩溃、运行状态和严重 shader 问题摘要；独立 `shader-*.jsonl` 记录资源来源、动态链接来源、翻译／编译失败和 TAA 覆盖事件。在链接时记录 `runtime hash → 原始 VS / vertex-fetch 指纹 → 已审查位置族及最终槽位`，保留未知或歧义结果；408 个归一化组只用于缩小审查范围。`command_processor::HashWords` 按 uint32 计算 FNV，`renderer::GetShader` 按字节计算 Fnv1a，必须标注 hash namespace；`LO_SHADER_DUMP_DIR` 中命令处理器生成的 `.bin` 文件名不能直接与 renderer 的字节 hash 比较。
2. **分别记录诊断类别。** 区分“缓存未命中”“来源未知”“翻译／编译失败”“TAA 未覆盖”和“预期拒绝”。缓存未命中不等于编译失败，来源未知不等于 TAA 未覆盖，符合现有 guard 的拒绝也不自动归为缺陷；一个事件可保留多个独立分类及原因，不将它们合并成一个失败计数。
3. **审计实际 draw，包括未知 shader。** 记录 VS／PS、pass、frame，以及 map 值和是否可用；无法取得的上下文显式标为 unknown。保留实际相机、viewport／depth 身份、jitter 是否实际应用、拒绝理由及上传证据。未知 VS 不能被现有 `temporalSlot >= 0` 条件或默认 hash 过滤漏掉。优先审查同网格 depth／material／light 的覆盖不一致，并核对 world 与骨骼配对，避免把残留 VP 或复用网格地址当成主相机／同一对象。
4. **去重并限制写入开销。** 以 shader＋pass＋reason 为基础保留阶段／状态差异，记录首次、最后命中及次数，避免只按 shader hash 抹掉不同 pass。采用后台批量、限量写入，避免每帧同步刷盘；日志开销和遗漏边界留待实现时验证。
5. **按会话保留，按版本累计。** runtime 与 shader 日志使用同一会话身份，默认保留当前及最新两组旧会话；保护活动文件和自定义路径，无法删除的组留待后续启动处理。另存按源码、翻译器及 TAA 规则版本分组的累计覆盖清单，避免三组会话轮转丢失历史。分别报告盘内资源、有限派生、运行 linked shader 和实际场景 draw 的分母，未触发的链接组合及未访问场景不作穷尽承诺。
6. **扩展 F1 日志快照。** 在现有 runtime 快照之外包含对应会话的独立 shader 日志快照，并记录其可用性；缺失日志不应丢弃渲染捕获。此项也是待实现行为。
7. **整理缺口与确认修复分开。** 后续可从独立日志和累计清单发现、整理未覆盖项，无需每次 F1 导出。确认闪烁因果和修复仍需同场景验证，必要时导出；未知路径只记录，不自动启用 jitter。候选经真实位置合约、实际上传、正常时序 32 相位和 Off 对照后再决定是否加入映射，未验证项继续保持待办。

## 本地证据索引

以下为保留在 ignored `out/` 的本地审阅资料，不随文档发布原始游戏素材或私有捕获：

- `out/taa-whole-game-scan/REPORT.md`、`shader-inventory.json`、`vs-families.md`、`pixel-contract-review.md`：扫描来源、逐 shader 候选及边界。
- `out/battle-taa-fix/REPORT.md`、`test-results.json`、`runtime-comparison.md`、`map3-regression.json`：现有六路径实现及有限运行验证。
- `out/enemy-death-f5446/REPORT.md`、`inventory.json`、`shader-review/REPORT.md`、`pixel-review/REPORT.md`、`enemy-localization.png`：新消散捕获与配对检查。输入 ZIP SHA256 为 `f5a2149426d13b4e4c48262f894cacc42a5117c4ccc346a1f4c5f4fdf26b5ee1`。

## 2026-09-09 当前状态澄清

当前本地 source batch 为 0.5.6，仍属于 v0.5.0 milestone；版本消费者重编没有追加功能验证，以下功能证据继续绑定 earlier candidate EXE `9a7b626320ede0bd82bcbf93cde08aaeefc2c631aa29ebacba4fc31a5d321288`。

本轮已实现独立 shader JSONL 日志及 F1 日志快照接口，并通过并发写入、真实 DXC 成败、路径、保留、退出 flush 和生产归档定向检查；因此上面的独立日志条目保留为历史 TODO，不能再描述为当前实现缺失。累计覆盖清单、未知实际 draw 的完整审计和 provenance-family reconciliation 仍未实现。

Map16 的 `fcbb75d0feb3fcb9`（c7）、`e8c0d438c690c784` 与 `576d669b2ad3c898`（c8）已加入 temporal constant 映射，并通过 20,635 项 CPU 检查，其中新增 Map16 3,348 项、32 个 phase、最大 clip 偏移误差 0.002845 像素。随后固定场景实跑覆盖 32 个连续 3840×2160 phase，所有 summary 均 ready/completed/history-reused，`gap=false`、`jitter_miss=0`，地面 ROI 未出现黑帧，160 个材质上传与对应深度上传逐位一致。该证据确认这三个路径在固定场景的修复；它不代表全游戏、全部阴影 PS 或用户验收。576d 的运行 PS payload 未记录，不能夸称 PS 逐位验证。

离线 family audit 将 `4bd8985d84983b83`（c230）敌人深度路径列为最高优先级，因为已有 f5446–5448 实际 draw 证据；它仍未实现。`22225401fc8ea621`、`3eb16ad927f44289` 和 `52e4405f97159d2f` 只有同源 cache／ALU 推断，当前帧没有实际 draw，不能加入生产映射或称为新故障。完整排序见 `out/v0.5.0/rendering-fixes/analysis/prioritized-missing-paths.md`。

当前 0.5.0 候选又加入四条 capture 确认的 c7 路径（`e8ec18f1d3eac4df`、`1ea46291cb1c7298`、`7d403bdef896a97f`、`45ed0948b6b701a7`），并优先上传异常 shader 摘要。用户依据 `out/v0.5.0/slot2-sol-check/REPORT.md` 对 Ghost Town slot-02 场景的六张间隔截图（约 11.37 秒）确认本次缺口修复；该结论标记为已验收，其他场景仍属回归覆盖，不延伸为全游戏修复或连续录像结论。更完整的 log-only 定位经验见[时序日志排查总结](taa-log-only-triage-2026-09-09.md)。稀疏相机 MV／jitter 收集服务于后续研究，不构成物体／骨骼 MV 或 DLSS 帧生成实现。
