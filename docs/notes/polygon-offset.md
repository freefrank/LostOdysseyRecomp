# Polygon offset 接入与阴影回归（2026-09-05）

状态：代码与 GPU 测试通过，游戏画面验证进行中，尚未提交。不能据此关闭人物自阴影或遇敌阴影问题。

## 已确认缺口

渲染器原先只保留 PA_SU_SC_MODE_CNTL 的低三位，且 plume 描述符的 depthBias / slopeScaledDepthBias 始终为默认零。Xenos 使用 +11/+12 分别开启正背面 offset，+13 为非多边形 PARA 开关；参数在 0x2380–0x2383。RB_DEPTH_INFO 的格式位是 +16。

本地 Xenia 参照：`out/shadow-reference/draw_util.h` 的 GetD3D10IntegerPolygonOffset，draw_util.cc 的 GetPreferredFacePolygonOffset，以及 pipeline_cache.cc 的 D3D12 rasterizer 设置。对应规则为可见正面优先，零参数时尝试可见背面；非多边形使用 PARA 位及正面参数。斜率以 1/16 子像素为单位。UNORM24 在 D32 使用 [0.5,1) 区间的近似；float24 少三位尾数，偏移以八个 D32 ULP 为一组向远离零方向取整。当前 host 深度目标为 D32_FLOAT_S8_UINT，并非原生 D24。

实现 `gpu/polygon_offset.h` 和 renderer 的管线 key/描述符接入。bias 的实际常数和斜率参与 key，避免不同 offset 共用 PSO。未添加曝光补偿或关闭深度测试。双面不同 offset 仍采用 Xenia 的单偏移近似，不声称精确模拟双面独立偏移。

## 验证

- `LoPolygonOffsetTest` 使用真实 D3D12 的 8×2 目标。先写同一倾斜平面的深度，两行分别用 LESS 和 GREATER 比较，检查 disabled、正负常量、背面回退、PARA 开关和正负斜率。16 个读回像素全部符合预期；小非零 UNORM 偏移保留为1，float24保留为8。日志 `out/polygon-gpu-test-result.log`。
- 运行时构建 `out/polygon-offset-build.log` 通过。
- Gorge 两次独立存档副本：`out/shadow-offset-on-gorge` 和 `out/shadow-offset-off-gorge`；EXE SHA256 均为 `1C1F529C0A6E0100B4AAFD5ADA4D31F0DDD915D63E6611E7A2DD3BE6C796E9B4`。唯一渲染开关为 `LO_NO_POLYGON_OFFSET=1`。
- 日志显示 mode=0x1802/0x1806、depthControl=0x700766 的实际管线，开启后 bias=-168、slope=0；关闭时 bias=0。双方都加载 Gorge、完成 1200–1215 连续帧捕获并保持30fps。截图雾/粒子相位不同，不做逐像素改善宣称。
- `out/shadow-offset-on-opening` 使用旧自动路线落入设置菜单，不是战斗对照。已停止该进程。正在 `out/shadow-opening-on-02` 从无存档副本逐步进入新游戏，输入走独立命令文件。

`LO_TRACE_POLYGON_OFFSET=1` 最多记录128个启用offset的管线创建；`LO_NO_POLYGON_OFFSET=1` 用于同程序A/B，正常值为应用客体参数。后续需检查战斗、火焰和箱子场景；偏移接入并不等于这些问题都有同一根因。

标题输入补充：同一后台进程的 Start 6 polls 未离开标题，60 polls 成功；随后 Up 20 polls 选中 New Game，A 20 polls 进入新游戏初始化 Settings。这说明设置页本身不代表自动路线跑错，后续 B 打开 Save these settings 提示，再 Up/A 确认。测试目录 shadow-opening-on-02 保留截图与输入日志。

首战实测：开启偏移的 shot_23400.ppm 已取得主角近景；关闭偏移副本 shadow-opening-off-02 用相同 EXE、原始测试 profile 和新路线也正常进入首战。20 polls 的 A 会越过目标确认并完成一次攻击；3 polls 能停在目标列表。开启 shot_30600 与关闭 shot_5400 均为四名敌人的目标列表，角色待机姿势仍有相位差；没有足够证据认定闪烁改善。下一步使用相同启动路线抓取连续帧，比较相近姿势，不能直接给不同姿势图做像素差。

连续帧重试：shadow-sequence-on/off-03 两边2400–2519帧均停在 Settings 的 reset 提示，属于无效战斗样本。不能仅凭相同自动序列假定到达同场景。保留原进程用3 polls的 Up/A 和 Start/Back 推进，on 的 f12511 已取得真正战斗逐 draw/resolve 捕获，contact sheet 在 out/shadow-resolve-contact.png。该帧 +24 offset 的 vs87a76ceaf1eaec11 实际绑定 rt=0x2d0/12、depth=0x700263、dinfo=0x10000，是颜色材质阶段；不能把所有启用偏移的绘制统称为 shadow-map pass。深度解析 ae00000 与全屏黑白中间层 b0d9000 均已捕获，后续沿其生产和采样位置检查。
