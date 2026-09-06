# 启动时深度清除导致驱动退出（2026-09-05）

10F4D144 在两次正常窗口启动（30080、42696）约 5 秒时退出。
Windows 报告均为 nvwgf2umx.dll 32.0.16.1656 +0xeb38ad、0xc0000409。
独立后台诊断 33876 加入 LO_TRACE_CLEAR_CALL 后，同样退出；最后一条是
frame 87、base 0、pitch 1280、1280×736、720 个矩形的深度清除调用开始，没有返回记录。
原 EXE/PDB、日志及 WER 保存在 out/manual-launch-crash-30080，诊断在 out/startup-clear-trace-01。

## 修正

plume 的 D3D12 clearDepthStencil 将非空矩形列表按每批最多 16 个提交。
所有矩形、深度值和 stencil 参数保持一致；零矩形仍表示整图清除。
这是一项针对本机驱动表现的兼容修正，16 不是声明的 D3D12 API 数量上限。
此前的 tile/MSAA 局部覆盖映射保留，不退回误擦整张阴影图的实现。
依赖改动同步到 tools/patches/plume-lostodyssey.patch。

## 验证

- 新构建 SHA256：9693E361444C89C308A59E0D1743215A26F03481AAE5DB7735F089F84A65C5B6。
- LoDepthClearGpuTest 在 RTX 5080 上测试 0、1、16、17、720、721 个矩形；每例读回 942080 个深度像素，全部零差异。覆盖整图清除、批次边界、稀疏区域隔离和实际启动的 720 tile 矩形。
- 原 LoDepthClearLayoutTest 的图块隔离、pitch 换行、裁剪和 9 种 MSAA 组合通过。
- startup-batch-fixed-01：独立 save/profile，实际 720 矩形调用正常返回，35 秒观察通过；450 帧截图确认 Press START 标题。后续核查至 46 秒仍运行，核对路径后停止本次测试副本。
- 构建与 GPU 读回日志在 out/build-startup-batch-fix.log、out/build-depth-clear-gpu.log、out/depth-clear-gpu-test.log。

这里只关闭本次可复现的启动崩溃；遇敌阴影画面回归与旧 GPU query/wait 指针异常仍待完成。

第二轮 startup-batch-fixed-02（48032）未开启clear trace，35秒观察通过，约30fps，截图450已捕获。LoStencilTest重新链接后非零stencil reference遮罩回归通过。两个本轮后台测试副本均核对完整路径后停止，历史测试进程未动。

正常可见窗口43792使用默认配置与原工作目录再次启动，30秒观察未退出且窗口响应；日志runtime-1788636599487739.log持续约30fps并开始读取save.bin。该进程保留供用户操作。
