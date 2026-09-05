# 内核 HLE 当前说明（2026-09-05）

实现在 `LostOdysseyRecomp/kernel/`，语义以Xenia源码及实际测试为依据。地址布局、导入及早期排错过程保存在[内核归档](../archive/2026-09-04/kernel.md)，其“XMA未实现/静音占位”与旧存档状态已过期。

| 范围 | 当前状态 / 入口 |
|---|---|
| 物理内存 | A/C共享、E偏移一页，见[别名验证](physical-alias-rendering.md) |
| 线程/时钟 | 客体线程、通知和时间基转换已支持当前早期流程；未验证所有内核调用 |
| 存档/档案 | 用户确认手动保存；异步完成、枚举及覆盖读回见[存档笔记](save-storage.md)，部分细节仍是本地改动 |
| XMA/XAudio | 已有真解码及SDL PCM输出，部分声音丢失仍未解决，见[音频笔记](audio-output.md) |
| 资源字符串 | 宽printf与戒指switch修正在本地通过局部战斗回归，见[戒指调查](battle-ring-resource.md) |
| 日志 | stderr同时默认写入工作目录logs，每次运行独立文件；GPU停帧有阶段和线程信息 |

`save/`、`profile/`、`cache/`默认相对工作目录；不是保证相对EXE。`LO_PROFILE_DIR`可覆盖档案位置。语言HLE当前返回英语；多语言原版不意味着移植语言选择已完成。WMV、网络与完整四盘流程未完成。

早期 `memset` 尾部截断导致的战斗骨骼越界已有修复，见[重编译](recomp.md)。营地卡死是另一个未定位问题，不能混用旧根因。当前待办见[状态页](../STATUS.md)。
