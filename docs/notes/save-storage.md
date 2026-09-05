# 手动存档失败（2026-09-04）

> 2026-09-05 同步：本文保留逐轮取证记录；相关新增代码仍有本地未提交部分，发布范围见[当前状态](../STATUS.md)。历史 PID 和测试中状态不代表进程仍在运行。

用户确认 `Save failed` 出现在菜单/存档点手动存档。修复后用户确认保存成功；`save/user00/save.bin` 实际写入 206000 字节，独立诊断进程从其副本读档并进入 Highlands of Wohl - Hypocenter 存档点。完整存档系统兼容性仍需后续验证。

## 修复

对照本地 Xenia `kernel/xam/xam_content.cc`：

- `XamContentCreateEx` 补齐 XOVERLAPPED 的结果、创建/打开 disposition、HRESULT 扩展错误、事件和 APC，异步返回 IO_PENDING。
- Close、Flush、GetCreator、GetDeviceState 补齐异步完成。
- SetThumbnail 修正为 user、content、buffer、size、overlapped 五参数 ABI，并保存缩略图。
- 内容 root 名统一大小写与冒号处理；查询返回独立字符串，避免 root 关闭后悬空引用。
- `.lo-content` 保存 XCONTENT_DATA，新进程从存档目录重建列表；枚举器持有快照。
- NT 文件写入检查 seek、短写和 flush 错误；相对目录句柄路径不再作为客体盘符解析。

保留原有工作目录布局：`save/<content filename>/`、`profile/`。不同启动工作目录会产生不同的存档位置；本次没有移动或删除已有存档。`.lo-content` 和 `.lo-thumbnail.png` 为本地辅助文件，不提交仓库。

## 验证

`LoStorageTest` 链接实际 runtime，通过 PowerPC 导入包装调用 XAM 和 NT 接口。测试不读取游戏资产，但整体构建依赖现有本地重编译环境。

```powershell
.\tools\build_runtime.bat LoStorageTest
# 使用新的隔离目录；write 验证 CREATE_NEW 碰撞，不覆盖已有测试数据。
.\out\build\windows-clang\LostOdysseyRecomp\LoStorageTest.exe write C:/path/to/new-test-directory
.\out\build\windows-clang\LostOdysseyRecomp\LoStorageTest.exe read C:/path/to/new-test-directory
```

已通过：异步完成事件、disposition、大小写 root、4096 字节真实写入、缩略图 ABI、close/flush、创建碰撞的异步 HRESULT。第二个独立进程可枚举并逐字节读回 payload。本地证据在 `out/storage-integration-1/`。

游戏内保存由用户确认，存档副本读档由 `out/animation-trace/shot_960.png` 验证。保存日志入口为 `out/save-validation/run.log`。完整内容管理兼容性（删除、所有截断模式、跨用户管理）仍未完成。

## 已有槽位覆盖修复

后续遇敌验证时，旧实现的新建保存成功，但覆盖已有槽位仍出现 Save failed。对照 Xenia xeXamContentCreate，CREATE_ALWAYS 应重建容器并报告 disposition=1；旧实现报告 2，且未刷新内容元数据。

现在 mode=2 重建 savedata 容器、写入新 metadata 并替换注册表项。删除范围必须是规范化 save 根目录的直接子目录，拒绝符号链接和非 savedata 内容。测试数据、游戏资产和原始用户存档分别隔离。

`out/storage-overwrite-20260904/` 的 write、read、overwrite、read-overwritten 四次独立调用均通过；覆盖测试还检查旧 payload/marker 已移除、元数据 T→U、4096 字节读回一致。游戏 `out/encounter-reload/run.log` 确认 mode=2 disposition=1、206000 字节写入，存档列表正常更新至 550G / 00:11，未再出现 Save failed。

最终版本四阶段测试在 `out/storage-overwrite-final/` 再次通过。`out/overwrite-final-reload/` 的新进程已读回本轮覆盖后的存档，进入 Hypocenter 存档点，凯姆普通资源 model=0 正确保持。
