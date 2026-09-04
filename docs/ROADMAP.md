# 路线图

状态标记：`[ ]` 未开始 `[~]` 进行中 `[x]` 完成

## 阶段 0：准备
- [x] 建立仓库骨架、子模块
- [x] 提取游戏数据：四张盘已用 tools/god_extract.py 解到 `LostOdysseyRecompLib/private/disc1..4`
- [x] 确认 XEX 版本：v4、PAL+JP 区域、无 TU，见 `docs/notes/xex.md`
- [x] XenonAnalyse 生成初版跳转表（841 张）；Ghidra 尚未安装，符号定位暂用 tools/ 下脚本
- [~] Xenia Canary 已放在 tools/xenia_canary.exe，尚未跑通游戏

## 阶段 1：重编译产出可编译
- [x] 填写 TOML：save/rest 地址、invalid_instructions、80 条显式函数边界、setjmp/longjmp
- [x] XenonRecomp 零错误、零未实现指令产出（run 15，本地补丁补了 30 条指令）
- [ ] 生成的 C++ 能在 MSVC 和 Clang 下编译通过
- [ ] XenosRecomp 处理全部着色器，记录不支持的指令

## 阶段 2：跑到主菜单
- [ ] 内核 HLE：线程、同步、内存、文件系统、XAM 用户档案
- [ ] 渲染后端：plume + D3D12，先画出第一帧
- [ ] 音频：XMA 解码接 ffmpeg
- [ ] 输入：SDL 手柄映射
- [ ] 去掉启动阶段的完整性校验，游戏进入标题画面

## 阶段 3：可通关
- [ ] 存档系统
- [ ] 四张盘的数据合并与读盘路径重定向
- [ ] 过场、战斗、千年之梦、大地图逐一验证
- [ ] 修复全部崩溃，通关一次

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
