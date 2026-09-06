# 跨版本文本补充补丁（已挂起）

2026-09-06：用户要求先挂起，暂不继续。仅完成只读资源调查，**没有生成可安装补丁，没有修改运行时、原始游戏数据或存档，也不属于 v0.2 功能**。语音补充不在本次范围内。只有用户重新提出需求后才恢复。

## 研究范围

两套现有支持数据按用户提供的 Redump 条目标识：

- [USA, Europe — En,Ja,Fr,De,Es,It（Disc 1，#11817）](https://redump.info/disc/11817)，本地 XEX 版本 `0.0.0.3`、Media ID `368DE6DD`。
- [Europe, Asia — En,Ja,Zh,Ko（Disc 1，#39111）](https://redump.info/disc/39111)，本地 XEX 版本 `0.0.0.4`、Media ID `39F7D748`。本地资源进一步区分简体与繁体中文。

设想是保留各自现有语言，前者补简中／繁中／韩文，后者补德／法／西／意；字体和包含文字的界面资源也需考虑。识别条目不等于完成整张 ISO 的 Redump 哈希比对。

## 已取得的证据

- 两版各自四盘的 `xenon_loc.fpd` 完全相同：Europe/Asia 为 87,785,472 字节，SHA256 `87d88853eee59a496066988c60c39e7e20023171e6691c09210ad3d6666be69a`；USA/Europe 为 73,170,944 字节，SHA256 `caa50fd961aec7b197240d17099efae0b166b3b5812b5f1ef68fa81bf7800097`。这只证明该资源包可在同版四盘之间复用，不能据此断定完整文本补丁只需 Disc 1。
- 本地原型已解析两版 Disc 1 的 FPI 目录树与压缩名称，并导出资源清单。文本并非只在一个包中：`LO.fpd` 有 coalesced 本地化资源；`xenon_loc.fpd` 有菜单、字体、教程、战斗文本和制作人员表；`xenon_scr.fpd` 有脚本消息；`xenon_event.fpd` 有字幕及按语言区分的资源。
- 原版索引采用目录节点和子项索引，资源偏移与名称表随版本不同。直接替换整份 FPI 或文本包不能保证保留原版其他资源及语言。
- 现有设置菜单按 XEX 版本限制语言列表。即使补齐资源，也还需要正确识别已安装补丁和处理原版语言注册表；尚未实现或实跑。

## 本地研究产物

以下均留在 ignored 的 `out/`，不是发行工具或补丁：

- `out/text-patch-audit.py`：四盘文本包哈希及原始索引调查。
- `out/inspect-text-index.py`：Disc 1 名称／目录解析原型，依赖本地解密镜像，不是经过完整边界验证的通用解析器。
- `out/text-patch/asia-index.json`、`eu-index.json`、`index-summary.txt`：已解析清单。
- `out/text-fpi-*.txt`、`out/text-string-codec.txt`、`out/text-wide-codec.txt`：只读反编译记录。

如将来恢复，需要先完成四盘文本覆盖与共享资源差异核对，再设计可撤销的资源和索引追加方案，并验证原有语言、新增语言、字幕、字体及换盘。当前不把这些工作加入近期执行队列。

[当前进度](../STATUS.md) · [路线图](../ROADMAP.zh-CN.md) · [两版兼容性](europe-support.md)
