# 角色破面与后期重影调查（2026-09-04）

## 已确认：16 位索引地址错误对齐

`command_processor.cpp` 原先把所有 `VGT_DMA_BASE` 都按 4 字节对齐，
使地址末尾为 2 的 16 位索引子区间向前错一个索引。随后三角形仍按每三个索引分组，
造成首个顶点来自上一子网格，且整段三角形连接错位。不是骨骼矩阵失效。
修复：16 位按 2 字节对齐，32 位按 4 字节对齐。

参照本地 Xenia `src/xenia/gpu/draw_extent_estimator.cc:110-136` 的索引格式分支。

证据（均在 ignored `out/`）：

- `render-geometry/geometry/0139.*`：Kaim 面部子网格，8268 个索引。
- 原始前三个索引为 8144、9717、9085；8144 是身体位置 z=53.53，其余是面部 z=166。
- 离线使用原始顶点、权重、骨骼常量，并在 D3D12 compute 中重放实际翻译的 VS，
  可以复现长条破面，排除“只是后处理”和缓存采样未更新这两种解释。
- 原索引连接中 51 个三角形最长边超过 20 个模型单位，最大 160.08。
  将起点纠正一个 16 位索引后，超过 20 的三角形为 0，最大边 7.02。
- `render-index-alignment/shot_2400.png`：正常深度状态下角色背部、盔甲和士兵破面消失。
- 该次运行只修索引地址，没有改后处理，故偏移重影仍然存在。

## 后期纹理的逻辑尺寸与存储行距

`render-geometry/draws/seq00..17` 分通道截图证明：长条破面在几何阶段出现，
偏移的模糊人形在后期合成出现，是两件事。
第 2400 帧日志显示模糊纹理 fetch 为 428×242，但 resolve 行距为 448，
`GetTexture` 原先直接返回 448×242 的 resolve 资源。
归一化坐标、纹素偏移与 GetDimensions 因此使用错误宽度，多次模糊会累积水平偏移。

修复方式：保留 resolve 存储资源，对尺寸更小的 2D fetch 创建逻辑尺寸视图，
按像素复制有效矩形，不做缩放。视图随 resolve 表存活，绑定前刷新，
保留正确的采样边界和纹素尺寸。实机复跑目录 `render-resolve-view/`。

## 顺带修正：同一 Xenos ALU 指令的双结果提交

同一指令的 vector 和 scalar 操作需要读取写回前的 GPR。
原翻译器先写 vector 目标，再计算 scalar，会发生读后写串扰。
现在暂存 vector 结果，算完 scalar 后再提交两者；该修复最初使用 v17；加入纹理解码后最终统一使用 v19。

参照 Xenia `dxbc_shader_translator_alu.cc` 的 ProcessAluInstruction：
先 ProcessVector/ProcessScalar，再 Store 两组结果。

`LoShaderAluTest` 用合成 Xenos 指令翻译后在真实 D3D12 compute 执行，
验证同指令读取旧值、后续指令读取新值；输入 1..4 全部通过。
`LoShaderTool` 编译现场 162 个 shader，失败 0。
单独修正 ALU 后角色仍破面，因此不能把此项当作主要几何根因。

## 实验边界

- 深度 ALWAYS 单变量实验仍有破面，并导致穿透；已撤掉。
- 顶点重放比较必须区分相同网格的不同角色实例，不能仅按 VB/index 内容配对；
  位置不同的实例自然有不同矩阵。
- Xenia 对照来自原始 1280×720 F12 截图；场景相同，但动作时刻不完全一致。
- 游戏资产、常量、VB、索引、shader dump 只存本机 ignored out/private。

## 材质偏灰：忽略了 texture fetch 的 gamma 与通道映射

第 2400 帧实际绑定统计中，1916 条纹理引用的 sign=0x3f（RGB gamma、A unsigned），
807 条为 unsigned，10 条 sign=0x55 是已有 signed float/HDR 资源。
原实现只绑定纹理和 sampler，没有执行 fetch 常量里的 sign 或 swizzle。

现在按 Xenia PWLGammaToLinear 的分段公式进行 gamma 解码，先处理源通道，
再执行 fetch swizzle；unsigned biased 也按 2x-1 处理。
浮点 signed 资源直接保留已有符号。本次未新增整数 SNORM 格式支持。
与 Xenia D3D12 路径一样，gamma 在宿主采样过滤后解码；并非声称复现逐 tap 硬件过滤。

`LoShaderAluTest` 同时验证四个 gamma 区间起点及先解码再 BGR 映射：
0、64/255、96/255、192/255 → 0、64/1023、128/1023、516/1023。
GPU 回读全部通过。最终 162 个现场 shader 再编译，失败 0。

纹理 swizzle 同时要求保留 resolve 的 `RB_COPY_DEST_INFO.copy_dest_swap`：
宿主 resolve 保持 canonical RGBA 供当前 presenter 使用；作为客体纹理采样时，
先恢复 resolve 的 R/B 交换，再执行 fetch 的 sign/swizzle。否则后期 BGR fetch 会
把红眼交换成蓝眼。合成 GPU 测试覆盖此往返，避免两次交换中只实现一半。
深度 resolve 不应用该交换。最后验证目录为 `out/render-final/`。

## 最终实机验证结果

`out/render-final/shot_2400.png`：攻击目标列表中，角色背部及士兵网格完整，
模糊轮廓对齐，红眼与蓝色目标圈正确；第 3000 帧近景确认脸部、头发和盔甲
无原来的拉丝/错连与偏移模糊人形。和 Xenia 相比，明暗已有改善，光照/阴影细节仍未完全一致。
最终对照页为 `out/xenia-comparison/comparison-fixed.html`。
正常深度、遮挡和景深保留，未启用调试 bypass。后续光影修复见 [光影笔记](lighting-stencil-depth-clear.md)。
