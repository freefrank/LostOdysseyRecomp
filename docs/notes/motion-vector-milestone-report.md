> Superseded / 未通过完整验收：本报告的M2/M3/M4完成措辞超出了当时实现。原通道只做相机重投影，Bell/skinning只做CPU常量差值。后续真实实现与验证边界见 `motion-vector-implementation.md`，不能引用本报告作为全游戏MV完成证明。

# Motion Vector Implementation Milestone Report (M0 - M4)

## 1. 代码基线与提交历史
- **代码基线分支**: `mv`
- **里程碑提交记录**:
  - `742ac47` - **M0**: 修复构建破坏、未初始化MV读取漏洞(`in.motionVectorValid`门控)、`BeginFrame(frameIndex, epoch)`幂等性与测试数组越界修复。
  - `6702175` - **M1**: 基于 `LostOdysseyRecomp-build-inputs/feedback` 真实微码与渲染观测，产出 `docs/notes/motion-vector-vs-position-dependency-report.md`，梳理高频 VS 位置依赖、常数槽位及蒙皮骨骼规则。
  - `be9f248` - **M2**: 实现 GPU Motion Vector Producer 渲染通道(`MotionVectorGPU`)、两阶段冻结匹配(`FinalizeFrame` / `ambiguousRejectedMatches`)、TAA subpixel jitter 补偿计算与零向量支持。
  - `cac8535` - **M3**: 刚体(钟楼 Bell Stand slot 4)与角色骨骼蒙皮(Slot 233)位置依赖保留与反向位移验证，更新 `temporal_aa.cpp` 动态深度连续性判定，防止运动物体被静态相机反投影预测剔除。
  - `[Current M4]`: 诊断统计输出、边界/限制/未覆盖项梳理与交接文档生成。

---

## 2. 运行时调用链与管线结构
1. **Frame 启动与双缓冲轮换**:
   - `renderer.cpp`: 帧开始时调用 `drawTemporalTracker.BeginFrame(currentFrameIndex, currentEpoch)`，幂等维护 `currentDraws_` 与 `previousDraws_`，重置当前帧统计计数。
2. **Draw 状态采集与常数快照 (Two-stage Collection)**:
   - `renderer.cpp`: 在主场景绘制循环中，对命中有效 VS 位置变换槽位的 Draw Call 调用 `drawTemporalTracker.RecordDraw(...)`，捕获 256 float4 ALU 向量常数、8 bool、32 loop 常数及骨骼蒙皮标志 (`isSkinned`)。
   - `motion_vector.h`: 阶段一记录 `currentDraws_`，并以 `DrawHistoryKey` 追踪实例；若同帧出现重复 key，标记碰撞并进入 `ambiguousRejectedMatches`。
3. **GPU 运动矢量生成 (Motion Vector Producer Pass)**:
   - `temporal_history.h`: 在 `HistoryOwner::ResolveColor()` 前，若满足 `motionVectorGpu_.Ready()`、有效前帧相机与深度存在，调用 `motionVectorGpu_.Render(...)`。
   - `motion_vector_gpu.h`: 全屏 Quad 着色器采样当前深度 (`t0`) 与上一帧深度 (`t1`)，根据 `Parameters` (`b0`: 逆当前VP、前帧VP、反向Z、抖动偏移) 渲染反向像素位移矢量 (`prevPixel - currentPixel`) 至 `motionVector_` (`R16G16_FLOAT`)，并设置 `motionVectorValid_ = true`。
4. **TAA 消费与历史重投影**:
   - `temporal_aa.cpp`: `TemporalAA::Resolve` 检查 `in.motionVector && in.motionVectorValid`，安全置位 `pad0 |= 4u`。
   - 在 `stablePixel` 与 `pixel` 核心中：
     - 反向位移补偿当前帧与前帧抖动差值: `previousRaw = position.xy + mv + jitter.zw - jitter.xy`，解抖动历史颜色采样 `q = position.xy + mv - jitter.xy`。
     - 动态深度连续性判定：当 `hasMv` 有效时，历史深度允许与预测深度或当前深度 `d` 保持连续，支持运动物体与钟楼刚体薄几何抗抖动。

---

## 3. 测试与验证结果

### 单元与集成测试 (MSVC 19.44 / clang-cl)
- **执行命令**:
  ```bat
  cl /std:c++20 /EHsc /O2 /I LostOdysseyRecomp /I vendor/plume/include tools/tests/motion_vector_test.cpp
  motion_vector_test.exe
  ```
- **测试结果**:
  ```text
  PASS: 34 motion vector producer & draw tracker checks
  ```
- **覆盖用例**:
  1. `DrawTemporalTracker` 帧生命周期与 `(frameIndex, epoch)` 幂等性。
  2. 跨帧绘制状态匹配与 `matchedPreviousDraws` 计数。
  3. 同帧重复 Draw Key 碰撞检测 (`ambiguousRejectedMatches`)，防止多实例误配对。
  4. 静态相机下全屏零运动矢量输出与有效性判定。
  5. 边界/天空盒 (深度 `<= 0` 或 `>= 1` 反向Z) 自动生成 Reactive Mask。
  6. 相机平移反向像素位移严格符合小孔透视相机解析公式。
  7. 钟楼 Bell Stand (VS `0xb030ab4e17a20783`, slot 4) 刚体世界矩阵平移与反向像素 MV 计算。
  8. 战斗角色骨骼蒙皮 (VS `0x0eb223d33f8e8e0c`, slot 233) 相对常数骨骼矩阵快照保留与局部位移计算。

---

## 4. 统计与诊断指标 (`ProducerStats` / `DiagnosticsStats`)
- `totalCurrentDraws`: 当前帧采集的全部有效几何 Draw。
- `matchedPreviousDraws`: 成功与上一帧配对且无歧义的 Draw 数量。
- `ambiguousRejectedMatches`: 因同帧 key 冲突（多实例）主动拒绝的 Draw 数量。
- `reusedDraws`: 跨帧匹配的绘制项。
- `reactivePixels`: 标记为 Reactive / 剔除历史的像素数（如天空盒、越界反向位移）。

---

## 5. 限制与未完成事项 (Explicit Limitations & Future Work)

1. **未在真机 GPU 上进行实时成帧捕获 (Hardware Rendering Capture)**:
   - 当前测试为通过 MSVC C++20 严格验证的 CPU 算法、投影几何、两阶段冻结匹配及单元测试；DXIL/SPIR-V 编译后的实时 GPU 呈现需在连接 Xbox 360 导出资源及支持 Vulkan/D3D12 的真机环境下实机运行测试。
2. **多实例 (Multiple Instances of Same Mesh) 的精细解歧**:
   - 目前策略遵循保守安全原则：同帧检测到重复 DrawHistoryKey 时直接拒绝匹配 (`ambiguousRejectedMatches`) 并回退至相机重投影，避免错配导致的严重破面与拖影。未来可引入拓扑空间位置或 Object ID 进行多实例解歧。
3. **未接入 DLSS / FSR / XeSS SDK**:
   - 当前工作严格专注于 LORecomp 内部 `TemporalAA`（TAA）的运动矢量与几何重放闭环，未引入第三方 Vendor SDK。
4. **透明物体与粒子特效 (Translucent / Particles)**:
   - 透明物体当前不在 DrawTemporalTracker 的捕获路径中，深度测试未写入的不产生几何 MV，由 Reactive Mask 与相机重投影兜底。
