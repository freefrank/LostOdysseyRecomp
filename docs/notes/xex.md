# 游戏数据来源与 XEX

## 原始数据（本地路径，不入库）

`<game-data-directory>`，GOD 格式（Games on Demand），TitleID `4D5307FA`，
内容类型 `00007000`。"CH" 版本，预计为亚洲区繁体中文版，XEX 与美版地址不同，
参考 re:Blue / 其他项目的地址时不可直接照抄。

| 容器（4D5307FA/00007000/） | 盘 | MediaID  | 大小 |
|---|---|---|---|
| 65975F33B848FC1188CC | Disc 1 | 39F7D748 | 5.1G |
| 62A4296DC2D351F07D77 | Disc 2 | 0EF8CEA8 | 4.3G |
| 6AC59496FC5538BBCDFA | Disc 3 | 309E3386 | 5.5G |
| DB4D07AEE9167C1AFB3A | Disc 4 | 7B21A91D | 5.7G |

DLC（STFS，内容类型 `00000002`，Marketplace）：
1. 其他迷宮－深淵的追尋者（额外迷宫）
2. 獎勵物品二合一組
3. 特殊物品三合一組

## 提取

不需要转 ISO。用 `xenia-vfs-dump`（Xenia 源码自带工具）或 God2Iso + extract-xiso
把四个容器分别解包到 `LostOdysseyRecompLib/private/disc1..4/`。
`default.xex` 在 Disc 1 根目录。

## default.xex（Disc 1，用 tools/xex_info.py 读取）

- 原始 PE 名 `XenonLaunch-RPGame.exe`，image_base `0x82000000`，entry `0x827CA440`，image_size `0x13C0000`
- 加密 AES，压缩 LZX（window 0x8000）。XenonRecomp 的 XenonUtils 自带解密解压，`tools/xexdump` 可导出平坦镜像
- title version 4，base version 4，无 XEXP 补丁。四张盘的 XEX 大小相同，各自 media_id 不同，
  多盘 media id 列表：39F7D748 0EF8CEA8 309E3386 7B21A91D。密文不同，解密后是否相同待比对
- game_regions `0x00FFF900`：PAL 全区 + 日本 + NTSC-J 其他地区，不含北美和中国大陆。
  参考美版地址时必须重新定位
- 导入：xam.xex 136 个、xboxkrnl.exe 319 个，内核版本 2.0.6683
- 静态库（XDK 6274）：XAPILIB XBOXKRNL LIBCMT MDISC XGRAPHC D3DX9 XNET XAUD XMP X3DAUD XMEDIA XONLINE D3D9LTCG
  → D3D9 是静态链接进游戏的，渲染层要在游戏内的 D3D 函数上打钩子（UnleashedRecomp 同款做法）
  → MDISC 是多盘切换库，XNET/XONLINE 需要桩

## 盘内文件布局

每张盘根目录相同：`default.xex`、`LO.fpi`（索引，每盘不同）、`LO.fpd`（每盘相同）、
`$SystemUpdate/`、以及 12 个 `xenon_*.fpd` 资源包。`.fpd` 以 `cpx` 魔数开头，是 feelplus 自定义容器，
UE3 的 UPK 包在其内部。battle/chr/loc/obj/scr/sys/vfx/world 四盘大小完全一致；
event/field/mov/snd 每盘不同（剧情推进内容）。合并策略：以文件哈希判定重复，重复的只保留一份。

## 待确认
- [ ] 四张盘解密后的 XEX 是否逐字节相同（决定重编译目标是否唯一）
- [ ] 是否曾发布 Title Update；若有需拿到 XEXP
- [ ] `.fpd`/`.fpi` 格式解析（阶段 3）
