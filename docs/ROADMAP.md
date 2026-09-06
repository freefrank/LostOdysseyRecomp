# 路线图

## 对白倍速修复（2026-09-05 最新结果）

用户已确认原始对白和修复后的游戏录音听感正常。XMA 跨包帧结束时曾额外跳过续接包中的新帧，导致对白被截短；现已保留这些帧，不改变采样率或音量。同段装甲车对白从 2129 帧恢复到完整 4062 帧，游戏录音相对原始音轨的时间斜率从 0.547 恢复为 1.000。另有 34 个多声道缓冲、4264 帧解码零错误。

本地验证构建 `135DCA79` 已安装，存档和配置文件校验未变。验证范围为该过场与上述音频样本；其他对白、战斗声音和循环子帧边界仍需覆盖。详细证据见 [音频记录](notes/audio-output.md)。下方较早实验的未完成状态按各自日期理解。

2026-09-05同步。`[x]`表示所述范围已有验证，不代表全游戏完成；哪些改动已推送见[状态总表](STATUS.md)。

状态标记：`[ ]` 未开始 `[~]` 进行中 `[x]` 完成

## 阶段 0：准备
- [x] 建立仓库骨架、子模块
- [x] 提取游戏数据：四张盘已用 tools/god_extract.py 解到 `LostOdysseyRecompLib/private/disc1..4`
- [x] 确认 XEX 版本：v4、PAL+JP 区域、无 TU，见 `docs/notes/xex.md`
- [x] XenonAnalyse 生成初版跳转表（841 张）；Ghidra 12.1.3 + XEXLoaderWV 已装，default.xex 已 headless 导入
- [x] Xenia Canary 已实际运行至开场战斗并保存同机位原图对照（2026-09-04，见 notes/xenia-render-comparison.md）

## 阶段 1：重编译产出可编译
- [x] 填写 TOML：save/rest 地址、invalid_instructions、80 条显式函数边界、setjmp/longjmp
- [x] XenonRecomp 零错误、零未实现指令产出（run 15，本地补丁补了 30 条指令）
- [x] 生成的 C++ 全部 251 个文件用 clang-cl 22 编译通过（/O2，零错误）；MSVC 不支持，不再作为目标
- [ ] XenosRecomp 处理全部着色器，记录不支持的指令

## 阶段 2：跑到主菜单
- [x] 标题动态背景恢复（2026-09-04：小尺寸纹理 level 0 的 packed mip 偏移；实跑确认灰色背景持续变化，见 notes/title-packed-mips.md）
- [x] 内核 HLE：线程、同步、内存、文件系统、XAM 用户档案（游戏可稳定运行到标题场景循环，60 fps swap）
- [x] 渲染后端：plume + D3D12，先画出第一帧（2026-09-03：标题画面 "Press START" 正确显示；实现见 gpu/renderer.cpp，状态见 notes/gpu.md）
- [~] 音频：已接入 Xenia FFmpeg XMAFRAMES 解码和 SDL 48kHz 双声道输出，后台标题/读档/地图捕获非零 PCM；循环终点跨越修正及环境音回跳已局部验证。另修正解码写回覆盖消费者游标、共享文件句柄定位读取竞争（集成测试932/8000错误降至0）；另修正XMA命令寄存器覆盖丢请求（真实MMIO连续32次kick/clear集成测试通过，新副本营地读档通过）；尚未证明这些修正解决全部缺声。对白、长音轨与循环子帧仍待验证，见 notes/audio-output.md；开场 WMV 影片仍黑屏
- [x] 输入：SDL 手柄映射（hid/，键盘回退）
- [x] 调试期间默认关闭手柄震动；`LO_CONTROLLER_RUMBLE=1` 可恢复（2026-09-04，按用户要求）
- [x] 游戏进入标题画面并可进入主菜单/设置菜单（2026-09-03，登录与存档设备检查已过）

## 阶段 3：可通关
- [~] 后续遇敌：已修复开场资源遗留导致的主角 T 姿势和攻击停滞，独立副本通过普通攻击、反击、自然胜利；敌人地图姿态与闪烁待查，见 notes/encounter-animation.md
- [x] Windows debug 判胜路径：调用游戏胜利阶段与结果初始化，用户实测确认可跳过战斗（2026-09-04）
- [ ] 主角火焰受击 glitch：用户确认 Xenia 也异常，需独立诊断或原机参考，不能直接复制 Xenia 效果
- [ ] 游戏内 debug menu：临时修改主角攻击力/倍率，便于快速推进剧情；只影响主角、可恢复且不写入存档，见 [需求说明](debug-menu-requirements.md)
- [~] 存档系统：异步完成、缩略图 ABI、持久化枚举和 NT 写入修复；用户确认手动保存成功；补齐 CREATE_ALWAYS 覆盖语义，游戏覆盖保存及独立进程读回通过，完整兼容性仍待验证，见 notes/save-storage.md
- [ ] 四张盘的数据合并与读盘路径重定向
- [ ] 过场、战斗、千年之梦、大地图逐一验证
- [~] 按攻略后台推进：Hypocenter残骸/Ram与戒指教学通过；修正戒指资源宽printf后缀及遗漏switch，后续战斗已自然胜利，已确认Wasteland早期战斗及后续Gorge读档；已从Gorge存档副本启动并沿正常路线到营地；营地新保存/读回未验证，见 notes/walkthrough-testing.md、notes/battle-ring-resource.md
- [x] 开场战斗角色黑色剪影修复（2026-09-04：A/C 物理地址别名共享内存，E 偏移一页；正常遮挡逻辑下角色材质恢复；Windows/Linux 别名测试通过，详见 notes/physical-alias-rendering.md）
- [x] 开场战斗角色破面、后期轮廓偏移与纹理 gamma 缺失修复（2026-09-04：16 位索引对齐、resolve 逻辑尺寸、纹理解码；D3D12 数值测试与 Xenia 场景对照，详见 notes/rendering-index-and-resolve.md）
- [x] 开场战斗光照高光恢复（2026-09-04：接通 stencil、修复 D3D12 参考值丢失及 D24 清理溢出；GPU 测试与同机位实跑，见 notes/lighting-stencil-depth-clear.md）
- [x] 首场战斗后的实时演出白屏修复（2026-09-04：EDRAM 在 draw 前进行格式转换，恢复场景及炮口火焰，进入重型坦克战斗；见 notes/post-battle-whiteout.md）
- [ ] 修复全部崩溃，通关一次（2026-09-04：进入 RPBattle__Scene 的 memset 截断崩溃已修，见 notes/recomp.md；已验证新游戏、首场战斗后演出及重型坦克战斗，尚未验证重型坦克战斗结束或通关。当前渲染状态与验证入口见 notes/handoff.md）


- [x] Debug menu 同地图人物传送：坐标、记录点返回和轴向微调；后台原生传送/返回/恢复行走及游戏菜单拒绝验证通过，见 notes/debug-teleport.md。
- [x] 当前地图 POI 传送列表：自动枚举已加载地图的存档、出入口、机关及拾取点；Hypocenter 存档点、机关附近落点、出口附近与旧编号拒绝已后台验证，见 notes/debug-teleport.md。

## 当前九项反馈

完整证据和发布状态见[总表](STATUS.md)与[本轮成果报告](WORK_REPORT_2026-09-05.md)。

- [~] 人物/遇敌阴影：局部清除误擦整张atlas已定位；tile/MSAA矩形映射10F4D144构建与覆盖测试通过，实际游戏回归待验。
- [ ] 火焰受击黑红格子；Xenia同样异常，以实机参考为准。
- [x] Ring外环在当前构建的Hypocenter/Edge of Wasteland遇敌已实际显示并变化；RT释放获得Good并造成101伤害，见 notes/battle-ring-resource.md。此项限定原反馈路径，不代表所有战斗场景已测。
- [ ] 第二地图箱子破坏特效黑色。
- [x] Debug menu当前地图ID和本地化名字：标题未知状态、Hypocenter/Gorge与2→3切图更新实测，见 [地图信息](notes/debug-map-info.md)。
- [ ] 随时存档开关已实现并构建；新构建非存档点保存槽03、开关恢复、重启位置读回通过；原生存档点及进入营地权限恢复通过，桌面UI点击/布局待验，见[专项笔记](notes/save-anywhere.md)。
- [~] 营地无响应：独立首战抓到临界区大小端错误导致的死锁，递归数/拥有者修正及四线程回归通过；选定游戏流程通过；另有GPU query/wait指针损坏仍未修复，见 [临界区调查](notes/critical-section-endian.md)。文件日志/GPU诊断已推送，新修正未发布。
- [~] 声音输出已推送；部分背景音/对白丢失仍待修复。

## 阶段 4：现代化
- [ ] 任意分辨率与宽屏，UI 布局修正
- [ ] 帧率解锁，找出写死 30fps 的逻辑
- [ ] HDR 输出与 tone mapping
- [ ] 高分辨率阴影、TAA、DLSS/FSR
- [ ] SSAO，暴露干净的深度缓冲给 ReShade
- [ ] Vulkan 后端，Steam Deck 验证
- [ ] 安装器与首次运行向导

## 阶段 5：可选
- [ ] 屏幕空间 GI / SSR
- [ ] 硬件光追阴影与反射
- [ ] 高清贴图替换管线、mod 加载器


音频后续局部回归（2026-09-05）：修正guest显式更新输入游标后仍叠加旧packet skip导致选错音轨的问题。518AF9B5独立副本完整营地→装甲车剧情→城门控制恢复，未再出现原稳定解码错误；全部对白与间歇GPU崩溃仍未通过，详见notes/audio-output.md。
