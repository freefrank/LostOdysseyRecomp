# Ghidra 脚本

2026-09-05核对。当前本地使用Ghidra 12.1.3、XEXLoaderWV和JDK 21；这些工具及私有分析工程不随仓库分发。包装脚本[headless.bat](headless.bat)包含本机路径，换机需调整。

| 已有脚本 | 用途 |
|---|---|
| [ExportFunctions.java](ExportFunctions.java) | 按地址导出函数反编译/指令信息；参数为输出文件，随后一个或多个十六进制地址 |
| [ExportReferences.java](ExportReferences.java) | 导出指定地址的引用；参数为输出文件，随后一个或多个十六进制地址 |

已导入default.xex后，可从仓库根目录只读运行：

```powershell
.\tools\ghidra\headless.bat -process default.xex -noanalysis -readOnly -postScript ExportFunctions.java out/functions.txt 82A16DC0
```

输出放在ignored的`out/`中。反编译提示不能替代指令语义验证，Ghidra没有建立引用也不等于不存在引用。旧文档列出的四个Python脚本仅是规划，不是当前工具，已移除该虚拟清单。逆向结论写入[专项笔记](../../docs/notes/README.md)。
