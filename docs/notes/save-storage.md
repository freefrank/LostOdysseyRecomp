# 手动存档失败（2026-09-04）

用户确认 `Save failed` 出现在菜单/存档点手动存档。修复后用户确认保存成功；`save/user00/save.bin` 实际写入 206000 字节，独立诊断进程从其副本读档并进入 Ipsilon 山地存档点。完整存档系统兼容性仍需后续验证。

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
