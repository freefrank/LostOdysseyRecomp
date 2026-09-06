# Lost Odyssey Recompiled v0.2

## English

v0.2 adds the audited USA/Europe edition and automatic access to imported discs. Supply your own game data; game files are not included.

- Import the supported USA/Europe version 0.0.0.3 four-disc set alongside the existing Asian version 0.0.0.4 support. Exact XEX checks prevent unsupported builds and mixed-edition installations.
- Game text choices follow the installed edition: English, Japanese, German, French, Spanish and Italian for USA/Europe. Asian choices remain unchanged. Voice choices now follow the actual game resources; the settings interface retains its five existing translations.
- First launch imports missing game data before showing language and graphics setup, so the initial language list matches the imported edition. Incompatible old game-language settings fall back to English.
- When the original game requests another disc, the runtime automatically selects its imported directory and reloads its index. Import all four discs from the same edition under one parent directory; no manual disc swap is required. Missing or invalid discs fail without changing the current mount.

Extract the complete ZIP to a writable folder and run **LostOdysseyRecomp.exe**. Follow the import and setup prompts. Use **InstallGame.exe** to add later discs, and preserve your save/profile folders when updating.

Local validation passed four-disc content hashes, six game-language starts and menu/settings checks, both editions' voice selectors, and controlled original-manager 1 → 2 → 3 → 4 → 1 sequences for both editions. Importer, installation-rejection and storage tests passed. The local package passed manifest checks, importer self-test and a 30-second rendered launch with a minimal PATH.

**Chapter-boundary story transitions and a complete playthrough remain unverified.** These tests do not establish full-language audio coverage or full-game compatibility. Existing visual effects, shader and intermittent stability issues remain; DLSS and frame generation remain disabled placeholders.

## 简体中文

v0.2 加入经核对的欧美版支持，以及自动读取已导入盘。请自行提供游戏数据，发布包不含游戏文件。

- 支持欧美 0.0.0.3 四盘版本，保留亚洲 0.0.0.4 支持。严格核对 XEX，阻止不受支持的版本及不同版本混装。
- 游戏文本选项按安装版本提供：欧美版为英／日／德／法／西／意，亚洲版保持不变。语音选项改为读取实际资源列表；设置界面仍保留现有五种翻译。
- 首次启动缺少数据时先导入，再选择语言与图形设置，确保初始语言列表匹配版本。不兼容的旧游戏语言设置回退英文。
- 原游戏请求下一盘时自动切换到已导入目录并重载对应索引。同版四盘全部导入至同一父目录后，无需玩家手动换盘。目标盘缺失或不合法时保留当前挂载并返回失败。

完整解压 ZIP 到可写目录，运行 **LostOdysseyRecomp.exe**，按提示导入并完成设置。可用 **InstallGame.exe** 追加光盘；更新时保留存档与档案目录。

本地验证已通过四盘文件全量哈希、六种游戏语言启动和菜单／设置检查、两版语音选择器，以及两版原管理器 1 → 2 → 3 → 4 → 1 受控换盘。导入器、异常安装和存储测试均通过；本地测试包通过清单校验、导入器自测及精简 PATH 下 30 秒实际渲染启动。

**章节交界剧情与完整通关仍未验证。** 上述测试不代表完整配音或全游戏兼容。已有特效、着色器和偶发稳定性问题仍存在；DLSS 与帧生成仍为禁用占位。
