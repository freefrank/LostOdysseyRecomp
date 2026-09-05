# 游戏数据来源与 XEX

> 核对日期：2026-09-05。README所称亚洲多语言版依据项目所有者对来源版本的说明；下列XEX字段已从本地文件重新读取。区域掩码不能单独证明零售地区或语言清单。

## 版本与语言的使用范围

开发数据来源为亚洲多语言版，Title ID `4D5307FA`，Disc 1 Media ID `39F7D748`，title/base version均为`0x00000004`（0.0.0.4），region mask `0x00FFF900`。未对其他区域XEX或Title Update进行移植验证。

当前HLE在`ExGetXConfigSetting`语言项返回英语1，现有实跑使用英语和`int`资源后缀。镜像可找到INT/JPN/KOR/FRA/DEU/ITA等语言代码，但引擎字符串存在不能证明该零售包含有完整对应文本和语音。完整原版语言清单及移植版逐语言切换仍待验证，不宣称所有语言已支持。

## 原始数据（本地路径，不入库）

用户自备的四盘数据目录，GOD 格式（Games on Demand），TitleID `4D5307FA`，
内容类型 `00007000`。来源标记为"CH"，项目所有者说明为亚洲多语言版；不要仅凭目录名确定繁体中文支持范围。不同区域的XEX地址未经本轮对照，
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

本项目已有 `tools/god_extract.py` 用于从GOD容器提取，四盘已分别解包到 `LostOdysseyRecompLib/private/disc1..4/`。
`default.xex` 在 Disc 1 根目录。

## default.xex（Disc 1，用 tools/xex_info.py 读取）

- 原始 PE 名 `XenonLaunch-RPGame.exe`，image_base `0x82000000`，entry `0x827CA440`，image_size `0x13C0000`
- 加密 AES，压缩 LZX（window 0x8000）。XenonRecomp 的 XenonUtils 自带解密解压，`tools/xexdump` 可导出平坦镜像
- title version 4，base version 4，无 XEXP 补丁。四张盘的 XEX 大小相同，各自 media_id 不同，
  多盘 media id 列表：39F7D748 0EF8CEA8 309E3386 7B21A91D。2026-09-05重新核对四个本地解密镜像，MD5均为`f07cb5afd8ab6fa8fbdc14f4fceb5858`；原始密文不同
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
- [x] 四张盘的本地解密镜像MD5一致，当前重编译目标采用Disc 1；见[重编译笔记](recomp.md)
- [ ] 是否曾发布 Title Update；若有需拿到 XEXP
- [ ] `.fpd`/`.fpi` 格式解析（阶段 3）
