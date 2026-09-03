# Ghidra 脚本

前置：Ghidra 11+，安装 Xbox 360 XEX 加载器插件（例如 ghidra-xex 或 ghidra_xbox360）。

计划中的脚本：
- `export_switch_tables.py`  从已识别的跳转表导出 XenonRecomp 的 switch_tables.toml
- `export_functions.py`      导出函数边界，生成 TOML 的 functions 数组
- `ue3_reflection.py`        扫描 UE3 反射字符串，批量给函数和类命名
- `find_save_rest.py`        定位 savegprlr/restgprlr 等编译器辅助函数
