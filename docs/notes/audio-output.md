# 音频输出初版（2026-09-05）

当前汇总见[成果报告](../WORK_REPORT_2026-09-05.md)和[状态总表](../STATUS.md)。下文按实验时间保留证据，早期未完成状态不代表最新结果。

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

## 解码写回覆盖消费者游标（2026-09-05）

新增 opt-in `LO_TRACE_XMA_CONTEXT_WRITES=1`，比较 Work 加载的上下文与解码结束时客体内容，最多报告64次，不修改客体状态。旧逻辑诊断版7057BB13的后台独立副本 `out/xma-context-writes-01/`（PID22552）在12.018秒即捕获context0 DWORD9从1被消费者推进到5，却写回1；随后反复出现3→7→3、7→11→7。这是实际丢失更新，不能用单线程参考实现的整块Store推断本地线程安全。

修正 Work 使用 StoreDecoded，仅发布解码器更新的DWORD0/1/2/4；DWORD9消费者读取游标和未修改字段不再从旧快照写回。输出不足时也移除无意义的整块写回。Clear/Allocate仍保留完整初始化语义。诊断保留并增加published标志，区分观察到的并发更新是否会被写回覆盖。

D58526BC运行时构建通过（out/build-xma-read-cursor.log）；后台独立副本 `out/xma-read-cursor-01/` PID35404已启动，后续验证见进度。该修复只解决已证实的消费者游标回退，不证明声音丢失全部恢复；DWORD0/1的同字输入状态并发、驱动Lock/Busy语义及循环子帧仍需检查。

另对现有后台战斗18768作20秒只读采样，context0有21个不同输入位置、15个输出游标组合，context1短暂存在后清空。数据 `out/query-watch-01/xma-context-samples.json`；不支持“所有解码器已停死”，也不能证明缺失音轨仍存在。

修正版本35404在26–29秒捕获context0/4共64条消费者DWORD9并发推进，均published=false，不再写回旧读游标；其后约一分钟渲染query路径823CF3F0再次崩溃，这次是写guest0（r5=0060EDF0，r4/r31=0），并非已证明同一个device指针损坏。进程已退出，截图请求1未完成；保留run.log，不自动重启掩盖问题。该次只能验证音频写回路径，完整地图/对白回归被query崩溃打断。

## 共享文件句柄的定位读取竞争（2026-09-05）

`NtReadFile` 原实现对同一FILE*分别调用_fseeki64和fread；CRT单次调用的锁无法保护两次调用之间的文件位置。增加真实guest import集成回归：4线程共享一个NtCreateFile返回的句柄，各自读取不同偏移，每线程2000次，按原始payload模式逐字节比对。未修正版本8000次请求中932次数据不符，日志out/storage-concurrency-before.log；测试没有修改调度或在实现中插入延迟。

FileHandle增加每句柄ioMutex，覆盖NtReadFile/NtWriteFile/NtReadFileScatter及位置/大小查询修改和flush。不会把不同句柄串行化；没有改为全局锁。文件打开初始化发生在句柄发布前，关闭并发与句柄引用生命周期不在本次修复范围。

同一测试修正后0/8000错误；CREATE_ALWAYS覆盖及新进程read-overwritten再测0/8000，存档元数据/事件/缩略图等原集成回归通过，见out/storage-concurrency-after.log、storage-concurrency-overwrite.log、storage-concurrency-read-overwritten.log。runtime构建B8DBBBD9通过。测试覆盖的是并发定位读取的正确性，未证明游戏中声音缺失或GPU崩溃由此导致；真实音轨复验仍待进行。未提交。

2026-09-05新构建游戏回归：B8DBBBD9独立out/io-lock-camp-01 PID18752，save/profile复制自此前camp-null-current-01测试副本。LO_QUERY_CALL_TRACE/LO_GPU_WAIT_TRACE开启，默认后台静音，未操作桌面。shot1034读入营地开过左箱的位置；普通摇杆移动到场地中央shot2361，再接近士兵shot5488，证明新I/O锁版能完成营地读档和移动。未见query保存寄存器异常、wait状态改变、XMA解码报错或崩溃。A输入未出现对话框，因此不计对白回归通过。

捕获前60秒立体声float于audio.f32，按秒分析audio.json。最后3秒RMS0.0252/0.0223/0.0222、峰值0.0948/0.0994/0.0998；仅支持该段仍有输出，不能证明所有音轨存在。当前捕获硬限制前60秒，已不足以观察随后人物交互，下一步应支持按需有界捕获，再核对实际有声对白。现场input6/shots5，进程保留。

## 按需音频捕获

设置 `LO_AUDIO_CAPTURE_REQUEST` 为本地请求文件路径，写入 `serial seconds`（非零新serial，seconds为0–60整数）。每32个音频提交帧读取一次，0停止当前录制；正数在请求同目录创建 `<请求文件stem>-<serial>.f32`，48kHz双声道float、静音前数据。按样本数严格限制长度，已有同名文件拒绝覆盖；无环境变量时不开启。每条请求记录开始提交帧和结束状态，可在进入后续剧情后才录制。原LO_AUDIO_CAPTURE的启动60秒行为保留。

8CAC1943构建通过（out/build-audio-request.log），独立out/audio-request-01 PID36092，营地测试存档副本，query/wait诊断开启，默认静音；运行验证后续记录于progress。此为取证能力，非声音缺失修复。

运行验证：36092在启动96.115秒（提交帧17760，已超过旧11250帧限制）接受request1录3秒，99.112秒成功结束，输出1152000字节，恰为144000双声道float样本帧。数据finite、peak0.005746/RMS0.001122，只用于验证取证路径。request2录10秒在约1秒时用request3/0停止，文件393216字节（1.024秒）；随后重新请求serial1拒绝覆盖原文件，SHA256保持一致。shot3802确认营地场景。当前audio-request.txt为1 3（已拒绝、无录制），下次用新serial4；input文件不存在/shots1。对白缺失仍未验证，下一步通过正常剧情触发实际有声场景并按需捕获。

2026-09-05 后台剧情回归：继续现有8CAC1943副本audio-request-01 PID36092，未重启。input1–5接近装甲车、选择Yes后停在士兵“我们撤退”对话；input6 A后shot20063进入Uhra Troops - Armored Vehicle。input7向前180poll接近右座碰撞，input8左前90poll触发剧情。shot21648车内实时镜头、22883夜景、23671车辆分屏、24778士兵对白“Sorry, not you, Officer Balmore.”；shot27007剧情后到城门塔外，input9左移30poll，shot27916角色位置改变，证明该段切图后控制恢复。本次没有复现query/wait异常，不证明间歇崩溃已修复。

音频请求6/7/8各60秒完成，输出各23040000字节；7覆盖车辆过场，8覆盖后段对白/抵达，分析JSON同名保留。未听辨/转录音频，不能据非零PCM宣布对白存在。只读vehicle-xma.json与dialogue-xma.json各21次采样：后者context0输入20种/输出19种，部分mono上下文继续推进，而多个16包stereo上下文输出0/0不变（有效位为1，输出容量30块，指针非零）。这些可能是预备/暂停音轨；尚未关联guest声音对象或kick请求，不能直接判解码器停死。下一步应关联这些stereo上下文的调度与音轨身份。现场input9/shots12完成、audio8完成，PID保留，无提交。

## XMA 命令寄存器被最后一次写入覆盖（2026-09-05）

从立体声不推进线索继续定位到实际guest循环82CC5B58：按每个硬件context单独向7FEA1940组寄存器写一个bit（stwbrx/eieio），连续三次写同组应启用三个context。旧LoMmioStore32只做普通赋值，XMA worker每250–1000微秒exchange最后一个值，先前请求必然可能丢失。锁82CC58E8与清除同样以bit命令写入。这不是音量或FFmpeg包解析问题。

对照 https://raw.githubusercontent.com/xenia-project/xenia/master/src/xenia/apu/xma_decoder.cc WriteRegister：每次写分别调用context.Enable/Disable/Clear；Disable/Clear与Work共用mutex。新增apu::xma::WriteCommand在MMIO普通镜像写入前处理三组命令，按本次mask逐个改变状态，锁定/清除同步完成；worker改扫描enabled contexts而不再取寄存器最后值。使用现有g_mutex串行Work与命令，避免仅OR位却重排kick/lock/clear。当前XMA状态寄存器读取模拟仍不完整，此次不宣称完整硬件时序。

F15118B4 runtime构建通过（build-xma-commands.log）。LoStorageTest新增xma-commands模式，经真实LoMmioStore32与XMA worker验证32个同步clear，以及连续32次单bitkick全部执行（以无输入的输出有效位退休确认，无私有音频数据），out/xma-command-integration.log PASS。新增测试复用现有runtime集成测试可执行文件，未跑游戏/写原始存档。

新后台out/xma-command-camp-01 PID7860，save/profile来自audio-request-01测试副本，F15118B4。旧PID36092仍保留；不能拿旧进程音轨推进验证新代码。下一步新副本读营地/车内，比较之前未推进的stereo上下文及实际对白。无提交。

F15118B4新副本7860已实际读入同一营地（shot1710），20秒采样camp-xma.json：context0/1输入位置各21种、输出各14/17种，当前227/226包循环音轨继续推进。此前不同场景/不同生命周期ID不能一对一当A/B。现场无input、shots1，待沿已知路线进入车内核对立体声停滞，不能宣称全部缺声恢复。

2026-09-05 F15118B4游戏验证未通过完整剧情：7860按相同input1–8路线营地上车（shot5509/6967），shot9010实时剧情开始。vehicle-xma.json 21次采样中stereo context4/5输入分别20/14种、输出13/15种，mono context6也推进；相比旧版同段多个stereo输出0/0停滞，命令修正后确有多通道解码活动。但303.868秒开始FFmpeg Reserved bit / invalid fill bits / skip -9等错误，不能宣称缺声修好。audio-request-1/2分别捕获60秒；未听辨，不计对白通过。

只读保留out/xma-command-camp-01/dialogue-bitstreams的34个去重输入缓冲及contexts.json（20秒/0.2秒采样、最多64文件），均本地忽略的私有诊断样本；另xma-bitstreams仅抓到残留219包循环音，不能冒充失败对白样本。下一步可离线对比包边界/跨buffer解析。当前SwapInputBuffer把offset设32，参考设0且从包头取首帧偏移，是否导致本例尚未验证，未改代码。

随后7860在约556秒查询823CF430空读退出，故未取得切图后shot6。现场input8/shots6未完成、audio2完成；不自动重启，不把截图超时当挂起。详细寄存器见third-map-hang.md。无提交。

## 2160帧离线解码与失败帧取证（2026-09-05）

使用此前dialogue-bitstreams的34个缓冲，新增本地out/decode-xma-frames.cpp小工具，直接链接当前运行时同一lo_avcodec/lo_avutil，按captured context的channels/rate逐帧send/receive。独立包payload拼接器out/offline-xma-header.py与照运行时ProcessPacket流程的out/offline-xma-runtime.py，从每个缓冲起始按包头首帧位移解码，忽略越出采样尾部的不完整帧；共2160帧全部成功，34个.frames文件逐字节相同。结果及stderr在dialogue-bitstreams/offline-header、offline-runtime/results.json。不是连续完整音轨测试，不覆盖跨两个guest输入缓冲/动态锁定/生命周期。

初版离线脚本out/offline-xma-samples.py在more=0跳包后未取新包首帧位移，会产生伪错误；其offline目录为失败的诊断工具结果，不用于运行时结论。修正工具后两路2160帧零错误说明已存数据/当前解码库能处理这些完整帧。另回读实际代码发现ProcessPacket在SwapInputBuffer之后已紧接setInputReadOffset(0)，先前32位偏移候选被排除，未修改该路径。

新增LO_XMA_ERROR_CAPTURE_DIR：前16次解码错误保存准确传给FFmpeg的.frame与解码上下文words/channels/rate；只有显式设置才写本地私有目录，已有同名.frame拒绝覆盖。原错误日志增加context/stereo/current input/offset/bytes，避免后采样缓冲无法关联发生错误的帧。query_call_trace也改为返回后先复制值，再比较并打印同一份快照，以消除上次诊断比较与显示不一致。构建build-xma-error-capture.log通过；两项都是诊断改进，未实际捕获新失败帧，未宣称缺声/崩溃修好。当前没有启动新副本；旧7860已退出、36092等其它副本未操作。

## 精确失败帧：声道不匹配线索（2026-09-05）

B608555F后台独立副本xma-exact-error-01 PID5708读营地、上车（shot3444），input7前进/input8左前触发车内CG（shot14660）。489.227秒捕获context5的16个精确失败帧，位于out/xma-exact-error-01/errors；首帧上下文DWORD1=B8800000，stereo=true/48000Hz，input0物理A8000、16包，读取游标34060。错误为Reserved bit，result=-11。

同一lo_avcodec离线每帧新建decoder重放：双声道16/16失败（replay.json），单声道16/16成功（replay-mono.json）。因此不是仅靠旧decoder残留状态才能触发，优先调查guest声道配置与实际选择的多流packet是否一致。isStereo读取DWORD1 bit29与本地Xenia参考一致；不能据此直接强制所有失败音轨改mono。此前34个buffer未包含这些帧的完整连续bit串（corpus-matches.json为空），它们不能代替这次失败样本。

随后CIM查询PID5708已无进程，run.log止于528.823秒，未找到query保存寄存器异常或明确崩溃记录；仅确认进程终止，不推断原因，未重启。现场input8/shots4，后台工具均完成，未提交。下一步捕获失败帧所在原始packet链及context启用时的声道/起始位移，区分选错流和配置被覆盖。

2026-09-05：扩展首次错误捕获为两个有效输入buffer各最多32包，并记录实际packet/offset/pending_skip；BEC8C20A构建通过（build-xma-packet-capture.log）。新后台32056 xma-packet-error-01，独立camp save/profile，shot930营地、2091登车、3064车内，input8/shots4触发CG。成功捕获error-1-context-5-input0.bin 32768字节；错误在packet2 offset1292，pending_skip0。

原始buffer包含三条交错链：0→3→4→7→9→11→13→15、1→5→8→12、2→6→10→14。按包头拆出部分完整帧并同库重放（errors/chain-replay.json）：链0双声道25帧通过/单声道25失败；链1双声道25通过/单声道25失败；链2单声道28通过/双声道28失败。部分帧解析工具未覆盖全部尾帧，不能称完整音轨测试；已足以证明同一buffer确有两stereo+一mono，且失败context5读到了mono链2。下一步查context5初始offset以及跨buffer待跳过数，不能全局改mono或忽略错误。未提交，PID32056保留，现场input8/shots4，所有工具已结束。

## Guest显式重定位与残留skip叠加（2026-09-05）

新增每context最近8次Work入口快照，E9F410F6构建通过，新后台5608 xma-history-01，独立副本shot1051营地、2309登车、3291车内、4348CG。首错error-1-context-5.txt提供准确状态链：旧buffer A0000的offset34000/skip4；退休后输入无效、offset0但host skip1；guest提交新buffer A8000时主动设offset4020（第1包+32bit），host仍skip1；ProcessPacket再次跳过1包，最终读第2包mono并失败。声道标记一直是B8800000双声道，排除该例stereo位读取错误。

局部修正：Context记录最后发布的inputReadOffset；下一次Work若guest显式改动该位置，丢弃基于旧位置的pending packet skip，保留正常连续跨buffer未改游标时的skip。Reset清除追踪状态。不改变声道数、PCM、解码错误策略。build-xma-explicit-offset.log通过；尚未新构建游戏回归，不能宣称全部缺声修复。5608是诊断旧构建，不能拿它验证新修正。现场input8/shots4，工具结束，无提交。下一步独立新副本同路线比对Reserved bit错误及后续剧情。

2026-09-05：518AF9B5新修正版31720 xma-offset-fix-01后台同camp路线，shot1176营地/2273登车/2945车内/3855CG/5103夜间车辆/7075后段。至244秒未出现xma frame decode failed、Reserved bit或invalid fill bits；旧版同CG起始即稳定失败，此次越过该点。vehicle-xma.json/later-xma.json各21次采样，后者context3–6输入13–15种位置、输出13–15种，存在推进及音轨生命周期更替；前者一段长停顿，不能仅以零错误宣布全部声音恢复。audio-request-1.f32完整60秒，audio2在244秒开始60秒捕获，未听辨；尚未到城门控制恢复。进程保留，input8/shots6，采样/截图命令完成，无提交。下轮继续同PID，勿重新走营地。

2026-09-05：继续同一518AF9B5 PID31720，无重启。shot8947已到城门，input9左移30poll后shot9703主角从门边移到左侧，确认剧情结束切图及控制恢复。至345秒没有XMA frame decode failed/Reserved bit/invalid fill bits或query保存寄存器异常，原来CG开头稳定错误已在完整营地→装甲车→城门路线消失。此结果支持本次显式位置更新后清除残留skip修正；不证明间歇GPU崩溃永久解决，也不证明所有对白已听辨完整。audio1/2均60秒各23040000字节完成，原始PCM本地保留。现场input9/shots8/audio2完成，进程保留，工具结束，无提交。下一步应保留此音频基线，继续图形问题或对实际对白做单独验证，勿重复营地路线仅换日志。
