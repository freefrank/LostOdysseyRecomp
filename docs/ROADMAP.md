# 路线图

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
- [ ] 音频：XMA 上下文/寄存器已模拟（apu/xma.cpp，静音占位解码，XAudio 音轨可推进）；真解码（ffmpeg XMA2）与 PCM 输出未做；开场 WMV 影片仍黑屏
- [x] 输入：SDL 手柄映射（hid/，键盘回退）
- [x] 调试期间默认关闭手柄震动；`LO_CONTROLLER_RUMBLE=1` 可恢复（2026-09-04，按用户要求）
- [x] 游戏进入标题画面并可进入主菜单/设置菜单（2026-09-03，登录与存档设备检查已过）

## 阶段 3：可通关
- [ ] 随机遇敌双方 T 姿势、攻击卡在执行阶段及敌人闪烁：已独立复现，正在追踪动画配置与骨骼更新，见 notes/encounter-animation.md
- [x] Windows debug 判胜路径：调用游戏胜利阶段与结果初始化，用户实测确认可跳过战斗（2026-09-04）
- [ ] 主角火焰受击 glitch：用户确认 Xenia 也异常，需独立诊断或原机参考，不能直接复制 Xenia 效果
- [ ] 游戏内 debug menu：临时修改主角攻击力/倍率，便于快速推进剧情；只影响主角、可恢复且不写入存档，见 [需求说明](debug-menu-requirements.md)
- [~] 存档系统：异步完成、缩略图 ABI、持久化枚举和 NT 写入修复；用户确认手动保存成功，独立进程已读取副本进入地图，完整兼容性仍待验证，见 notes/save-storage.md
- [ ] 四张盘的数据合并与读盘路径重定向
- [ ] 过场、战斗、千年之梦、大地图逐一验证
- [x] 开场战斗角色黑色剪影修复（2026-09-04：A/C 物理地址别名共享内存，E 偏移一页；正常遮挡逻辑下角色材质恢复；Windows/Linux 别名测试通过，详见 notes/physical-alias-rendering.md）
- [x] 开场战斗角色破面、后期轮廓偏移与纹理 gamma 缺失修复（2026-09-04：16 位索引对齐、resolve 逻辑尺寸、纹理解码；D3D12 数值测试与 Xenia 场景对照，详见 notes/rendering-index-and-resolve.md）
- [x] 开场战斗光照高光恢复（2026-09-04：接通 stencil、修复 D3D12 参考值丢失及 D24 清理溢出；GPU 测试与同机位实跑，见 notes/lighting-stencil-depth-clear.md）
- [x] 首场战斗后的实时演出白屏修复（2026-09-04：EDRAM 在 draw 前进行格式转换，恢复场景及炮口火焰，进入重型坦克战斗；见 notes/post-battle-whiteout.md）
- [ ] 修复全部崩溃，通关一次（2026-09-04：进入 RPBattle__Scene 的 memset 截断崩溃已修，见 notes/recomp.md；已验证新游戏、首场战斗后演出及重型坦克战斗，尚未验证重型坦克战斗结束或通关。当前渲染状态与验证入口见 notes/handoff.md）

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
