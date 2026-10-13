# 高分辨率 / 高帧率 CG 替换：可行性研究

2026-10-09。来源：对重编译代码的静态逆向（未运行游戏）、FMV 资源导出（`asset-inventory-20261002/fmv`）、外部资料调研。相关 Issue：#265（玩家希望有 1080p/4K CG），#40（Mod 支持）。

## 结论

（2026-10-12：替换视频的格式改为 H.264 + MP4，见下文“解码”。）

可行，而且不用改游戏的存档、档案或播放逻辑：

- 让游戏照常播放原版 WMV，音频、字幕、跳过、结束时机都不变。
- 只把画面换掉：在 FLO_MOVIE 画四边形的位置，改画主机端解码出来的替换视频。
- 替换视频的分辨率和游戏无关，4K 可以。
- 帧率可以高于原版：游戏每帧都会重画这个四边形（60/120 fps），只要按影片时钟选替换帧，59.94 fps 的替换视频就能逐帧显示。

项目不附带任何替换视频。替换视频由玩家或 Mod 作者用自己的工具制作（AI 放大、补帧），作为 Mod 安装。

## 游戏怎样播放 CG

| 项目 | 内容 | 依据 |
|---|---|---|
| 素材 | 66 个 WMV，`bin/xenon/mov/mv_XXv.wmv`，在 `xenon_mov.fpd` 里。视频 `wmv3`（WMV9 Main），1280×720，30000/1001 fps，约 7 Mbit/s；音频 WMA Pro，1–3 条音轨（语言） | 资源导出 `videos.csv` |
| 驱动 | Matinee 轨道 `UInterpTrackMovie`。UpdateTrack `sub_827E2DD0` 在越过关键帧时调用 `sub_82815248(M, 名字)` 开始播放 | 代码 |
| 开文件 | 游戏自己按名字打开文件（`sub_828140C8` → `sub_8284CD80`），读入缓存；播放器只通过两个读回调拿数据，见不到路径。`FMVInfo.dat` 决定音轨（`entry[6 + 语音 ID]`）和视频轨（`entry[4]`） | 代码 |
| 解码器 | 静态链接的 Windows Media Format SDK 播放器（字符串 `WMFDecodeX86`、WMV1/2/3/A/P/R FourCC 检查），不是 `.xmv` API。4 个工作线程。镜像里有 65 个 DXVA Xenos 着色器，但找不到对它们的引用，所以应当是纯 CPU 解码 | 代码（DXVA 未使用是推断） |
| 音频 | 播放器解 WMA Pro，经静态链接的 XAudio2 输出 | 代码 |
| 上传 | `sub_82813D38` 建两组（双缓冲）三张 `D3DFMT_LIN_L8` 纹理：Y 为 W×H，U、V 为 W/2×H/2。渲染线程上 `sub_823F0038` 锁住后备组，GetNextFrame（`sub_82CF8C38`）逐行拷贝解码结果，解锁后切换前台组（M+12） | 代码 |
| 绘制 | FLO_MOVIE 场景代理 Render `sub_823EFC08`：Y/U/V 绑到纹理槽 0/1/2，像素着色器 `ps_ce6fe349adbda428` 做 YUV→RGB，`sub_823E4B50` 画四边形（lr `0x823EFF88`，就是 `gpu/hor_plus.cpp` 已经挂钩做黑边的位置） | 代码；着色器配对是推断 |
| 时序 | GetNextFrame 拿样本 PTS（player+288）和播放器时钟（player+44）比较：晚到的帧丢掉，早 38 ms 以上的不给；早 5 ms 以上时会在渲染线程上 `Sleep(差值 − 5)`。纹理按影片时钟以 29.97 fps 更新，前台组在每个游戏帧重画 | 代码；时钟是否跟随 XAudio2 播放位置未确认 |
| 暂停/跳过/结束 | 事件 1 暂停（播放器 vtbl[22]），2 恢复（vtbl[23]），4 释放；结束由 Matinee 时间决定。字幕是另一条轨道 `UInterpTrackLOSubtitles`，游戏单独绘制 | 代码；字幕是按类名推断 |

## 推荐方案：主机端替换画面

### 挂钩点

1. **识别影片**：挂 `sub_82815248`（r3 = 影片对象 M，r4 = 名字 FString）。用名字查 Mod 解析器（`AssetKind::Movie`），记下 M → 替换文件。
2. **同步**：挂 `sub_823F0038`。返回 1（拿到新帧）时，记录这一帧的 PTS（`LoadWord(M+20) + 288`）和主机时间。
3. **暂停**：挂 `sub_82813888` / `sub_82815010`，暂停时冻结主机时钟。
4. **结束**：`sub_82814570`（释放命令）或渲染线程上的 `sub_82815640` 时关闭解码器。
5. **绘制**：在已有的 `sub_823E4B50` 挂钩里，像 `gpu/movie_clear.h` 那样发一个私有寄存器包（M、目标矩形、当前时间）。渲染器收到后，不画游戏的四边形，改画替换帧。

### 选帧（高帧率）

替换帧时间 = 游戏前台帧的 PTS + 它成为前台以来经过的主机时间。经过时间最多算一个原版帧（约 33 ms），防止游戏卡顿时替换画面跑到声音前面。60 fps 的替换视频在 60/120 fps 下每 16.7 ms 换一帧。游戏帧率低于替换帧率时，自然跳帧。

### 解码

2026-10-12 决定：用 **H.264 + MP4**，不用 AV1。原方案用 dav1d 在 CPU 上软件解码 AV1，4K60 对 Steam Deck 太重；H.264 的硬件解码覆盖最广（Windows、Steam Deck、macOS、较老的手机都有），制作工具也最通用。

- **替换视频规格**（给 Mod 作者）：
  - 容器 MP4，`moov` 放在文件头（ffmpeg `-movflags +faststart`）。
  - 视频 H.264 High profile，8 bit，`yuv420p`，逐行，BT.709 limited range，恒定帧率。
  - 帧率建议 60000/1001（原版 30000/1001 的两倍）；30000/1001 也行。
  - 分辨率保持 16:9。桌面包用 3840×2160（4K60 需要 Level 5.2）；给 Steam Deck 和手机的包用 1920×1080（Level 4.2）或 2560×1440（Level 5.1）。
  - 不带音轨：游戏照常播原版音频（多语言音轨、字幕都不变）。带了也会被忽略。
  - 时长和原版一致，从第一帧对齐。
  - 码率参考：4K60 约 40–60 Mbit/s，1080p60 约 12–20 Mbit/s。
- **解码器**：扩展项目里那份 FFmpeg（Xenia 的 4.4 分支，`thirdparty/ffmpeg.cmake` 的手写源码表，目前只编音频）：加 `h264` 解码器和 `mov` 解封装器，所有平台走同一套代码。硬件解码按平台用 FFmpeg 的 hwaccel：Windows `d3d11va`、Linux / Steam Deck `vaapi`（运行时加载 libva）、macOS `videotoolbox`、Android `mediacodec`；都不可用时软件解码。从 `.loarc` 归档读影片用 FFmpeg 的自定义 IO（`OpenAsset` 给出的偏移和长度）。
- **线程与上传**：解码线程提前 2–3 帧排队。硬件帧用 `av_hwframe_transfer_data` 取回 NV12，Y 作为 R8、UV 作为 R8G8 上传到持久映射的环形缓冲，着色器里按 BT.709 limited range 转 RGB。4K 每帧约 12 MB，60 fps 约 750 MB/s；1080p 约 190 MB/s。以后可以改成直接共享硬件帧，省掉取回。
- **性能预估（未实测）**：有硬件解码时 CPU 开销主要是取回和上传。软件解码 4K60 H.264 在桌面 CPU 上可行，在 Steam Deck 上不行，所以给 Deck 的包用 1080p60。
- **专利**：H.264 解码器有专利许可问题；硬件解码由系统和驱动提供，软件回退是 FFmpeg 的 LGPL 解码器。发布前再确认一次要不要保留软件回退。
- **原版解码照常运行**：方案 A 下游戏的 WMV 解码仍在跑（720p、CPU）。以后可以把视频轨设成 -1（挂 `sub_82815060`，播放器本来就接受 -1），只留音频，省掉原版视频解码；那样要同时跳过纹理创建和上传，并由主机自己画。这一步作为后续优化，中等难度。

### Mod 包格式

沿用 Mod API v1：

- standalone：`mod.ini` 里写 `movie:bin/xenon/mov/mv_00v.wmv#0:video=movies/mv_00v.mp4`。
- overlay：`OverlayRelativePath` 已经为 `movie` 预留了 `overlay/movies/key-fnv1a64-<hash>.loasset`。

键的写法需要在实现时定下来。影片名本身唯一且稳定，用不用 export 序号由实现决定。`lo_mod.py` 要加一个 `movie` 打包入口（检查分辨率、帧率、编码）。MO2 不用改。

## 工作量

| 阶段 | 内容 | 估计 |
|---|---|---|
| 1 | FFmpeg 源码表加 `h264`、`mov` 和各平台 hwaccel（含每个平台的 `config.h`），解码线程和帧队列 | 3–4 天 |
| 2 | 五个挂钩、私有寄存器包、D3D12/Vulkan/Metal 的 Y/UV 上传和 YUV→RGB 绘制 | 2–3 天 |
| 3 | `lo_mod.py` 打包 `movie`、Wiki、日志 | 半天 |
| 4 | 在 psvita 上用自制的 `mv_00v` 4K60 替换版做冒烟测试（不发布） | 半天 |

## 实现前要在 psvita 上补的追踪

1. 每次 `sub_823F0038` 的返回值、ttp、player+288、M+144/148 和 GetStatus 时间：确认 PTS 单位，以及时钟是不是音频驱动。
2. 渲染线程在 GetNextFrame 里的 `Sleep` 是否真的发生、每帧多久（会影响 CG 期间 60/120 fps 的帧节奏，跟替换无关，也值得知道）。
3. 4 个播放器线程各自的 CPU 时间。
4. M+120/M+124（宽高）和纹理 pitch。
5. 只放音频的实验（视频轨 -1）：有声音、时钟走、不崩溃。
6. 按 Back 跳过时触发哪个事件，是否走到 `sub_82814570`。
7. 和 `ps_ce6fe349adbda428` 配对的顶点着色器，以及 c254/c255 的值（YUV 矩阵）。

## 其他选项

- **不靠 Mod 的画质提升**：在 FLO_MOVIE 绘制时，对原版 720p 帧用更好的放大滤波（例如 FSR 1 EASU + RCAS，或 Lanczos）代替双线性。成本低，所有玩家都受益，但不会增加细节。可以和替换方案共用同一个绘制挂钩。
- **运行时实时补帧/AI 放大**：太重，不考虑。离线工具做得更好，结果交给 Mod。
- **帧生成**：DLSS-G/FSR FG 已经覆盖在 CG 四边形上，替换方案不改变这一点。

## 导航

- 资源导出：`C:\Users\freefrank\worktrees\LostOdysseyRecomp\asset-inventory-20261002\fmv`（66 个原始 WMV、`videos.csv`）。
- 逆向脚本（字符串、交叉引用、函数范围、调用图、`bl` 目标）在 `C:\Users\freefrank\worktrees\LostOdysseyRecomp\_keep\scratch-helpers\guest-re`，读 `LostOdysseyRecompLib/private/image_disc1.bin` 和 `out/decomp-index/catalog.sqlite`。
- [Mod API](../wiki/Modding-API.md)、[Mod Organizer 2](../wiki/Mod-Organizer-2.md)、[运行时纹理替换的剩余工作](../wiki/Runtime-Texture-Replacement.md)。
