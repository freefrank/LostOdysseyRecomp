# 音频输出初版（2026-09-05）

`apu/xma.cpp` 从交替输入缓冲中的2048字节包组装压缩帧，调用 XMAFRAMES 解码器，将512样本解码结果转成客体需要的大端交错 int16，按128样本子帧写入输出环。`apu/audio.cpp` 将游戏最终提交的6声道 planar 大端 float 下混成48kHz双声道 PCM，通过 SDL 队列输出。设备缓冲有上限，回调追赶有界，非有限值归零。

FFmpeg 来自 `https://github.com/xenia-project/FFmpeg`，固定提交 `15ece0882e8d5875051ff5b73c5a8326f7cee9f5`。CMake FetchContent 获取源码，`thirdparty/ffmpeg.cmake` 的显式源文件列表构建 avcodec/avutil，沿用其配置和原始版权头；许可证原文位于获取源码中的 COPYING.LGPLv2.1，各源文件保留自身许可证。发布打包阶段尚未完成，需携带依赖的许可证及对应源码/重链接材料；当前没有发布二进制。

## 已验证

- Windows clang 构建通过，SDL WASAPI 设备成功打开。
- `out/audio-first/` 后台标题→读档→地图持续运行200秒以上，没有 XMA 解码错误日志。前37.75秒捕获包含2797647个非零样本，峰值0.13768，RMS 0.01285537，无 NaN/Inf。
- `LO_AUDIO_CAPTURE=<路径>` 保存输出前的双声道 f32le，最多60秒；`out/audio-first/preview.wav` 为35秒导出。
- `LO_BACKGROUND=1` 默认静音以免打扰用户；`LO_AUDIO_MUTE=1` 可单独静音。捕获在静音处理之前，证明解码产出，不等同于已经听过扬声器输出。
- `out/diagnostics-01/` 音频和地图渲染共同运行；人为暂停GPU7秒时音频仍推进，GPU恢复后回到30fps。

## 未决

循环子帧边界、跨输入缓冲的极端帧、战斗声音、语音与听感仍需验证。不能将非零 PCM 当作所有音轨正确的证据。开场影片播放不是本次 XMA 输出的覆盖范围。

## 用户复测与循环终点修正

用户在诊断版 PID 10672 中报告背景音和部分对白消失，但其它声音仍正常；光影缺陷没有变化。真实日志 `out/build/windows-clang/LostOdysseyRecomp/logs/runtime-1788588572991415.log` 中101至121秒音频峰值下降至零，但设备队列仍为6144/8192字节，渲染仍推进。不能认定所有声音丢失都源自同一问题。

发现循环判断只接受 `readOffset == loopEnd`，而尾帧可能直接跳到下一包，跨过终点；旧代码还在检查循环前先作废耗尽的输入缓冲。对照本地 Xenia `out/audio-reference/xma_context.cc` 的 `TrySetupNextLoop`，改为到达或跨过终点时回跳，并在换缓冲之前保留循环输入、清除跨包跳数。有限循环递减，无限循环255保持。

新增 `LoXmaLoopTest`：使用真实上下文的219包、终点3580323位验证尾包跨越，并检查有限循环、无限循环、不应提前回跳和空循环范围。测试及运行时构建通过。用户自行结束10672后才重链接标准 EXE；没有结束用户进程。

后台 `out/audio-loop-01/` 使用独立旧Hypocenter存档，PID 39612。`loop-samples.jsonl` 记录输入有效位和读取位置；环境音上下文4已从99631跨循环回到68245，继续推进，输入仍有效。仍需更多长音轨、实际对白回归；loop_subframe_start/end/skip 尚未完整实现，此次修正不代表音频全部完成。
