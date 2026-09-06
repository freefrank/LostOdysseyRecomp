# 窗口消息停摆：独立事件线程

2026-09-05：用户在 Map12、Map13 多次报告标题栏未响应而画面仍更新。事后查询 PID40932 已恢复响应；没有抓到故障当刻栈，不能认定具体触发原因。一次 LLDB 尝试因缺少 python311.dll 在附加前失败，未停止游戏。

旧代码 SDL 窗口创建、事件泵、GPU 执行和 Present 共用 CP 线程，因此长 GPU 等待、同步编译或捕获会饿死窗口消息。video.cpp Windows 路径改为专属 jthread：窗口/手柄初始化、事件处理和销毁归该线程，promise 发布初始化结果与 HWND；D3D12 创建/绘制/Present 仍归 CP。非 Windows 路径保留原泵。

## 实际游戏定向验证

测试脚本 out/test-window-pump.py 自行启动两个隐藏测试进程，独立工作目录，无用户存档或输入。只暂停其自身进程的渲染线程约 6 秒，以 WM_NULL SendMessageTimeout(300ms) 检测响应，然后恢复；finally 仅结束本脚本启动的进程。

- 旧版 FB225C29：窗口/渲染同线程；阻塞期间六次全部超时，恢复后恢复响应。结果 out/window-pump-test-baseline/results.json。
- 最终修正版 730E2653F11002A6B8276D474B4475FF58AFA76B4FA9FF5FFA646E9DF4CC3CEF：窗口线程46356，渲染39340；六次0.04–0.08ms全部响应，恢复后继续swap。结果 out/window-pump-final-fixed/results.json，脚本 out/test-window-pump-final.py。
- 编译通过：out/build-window-pump.log；独立链接通过 out/link-window-pump.log。可执行 out/window-pump-fixed.exe，调试符号同名pdb。主build目录旧exe仍被用户运行，未覆盖或重启。

该测试证明长渲染阻塞不会再饿死窗口消息，不证明用户每次窗口未响应均由同一触发点造成。真实 Map13、F1菜单及手柄连续游玩回归仍待新版运行。当前用户 PID40932 不含此修正。已询问是否存档并允许重启；未获回答前不得结束用户游戏。
