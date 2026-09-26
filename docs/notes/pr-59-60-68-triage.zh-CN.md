# PR #59、#60、#68 处理记录

核对时间：2026-09-26 07:58 UTC。以下状态依据对应 PR、当前 head 和 CI／发布记录整理；不代表额外的全游戏或实机验收。

## PR #59：关闭，未合并

当前 head `92694ae`。`docs/notes/battle-ring-resource.md` 第 119、122 行记录的原版 RT 顺序已经正常。PR 默认伪造 `255`，首次物理 RT 按下会被转换为客体 release／判定边沿，可能改变玩法，因此不属于当前必须修复项。

## PR #60：保持 OPEN，暂缓合并

当前 head `4bbe0014`。主线仍有 x64 硬编码，PR 的方向仍有价值；但 `tools/build_dxc_linux_arm64.sh` 第 22–30 行没有初始化固定 DXC/SPIR-V 依赖 submodules，第 28、94 行无条件删除环境覆写的 `WORK_SRC`／`BUILD_DIR`，且没有 CI。作者的平台测试目前只有自述，需先修正这些问题再评估合并。

## PR #68：已 squash 合并

合并提交为 `457ba24adb768474c3229d34ccaee0bc1cc93789`，PR head 为 `435b424`。已合入 API、`LOTEX1`/PNG packer、原生菜单图集与字体页面替换；通用客端纹理、TTF、模型和影片尚未接入。C++ 双平台 Mod API 与 9 项 Python 测试的 CI 已通过，该 PR head 的其他检查也通过；合并到当前主线后的检查另记如下。

Wiki 发布 workflow `36226859217` 重跑成功并确认 `has_wiki=true`，PR 正文已修正，Wiki 已实际发布。合并后的 workflow `36228382565` 已全项 SUCCESS，Windows／Linux tests 和 publish-wiki 均通过；SR hybrid workflow `36228382568` 也已全项 SUCCESS。Settings／AF workflow 最初被主线 PlayStation 提示夹具缺口阻塞，原因是 `hid::UsesPlayStationPrompts()` 没有测试替身；main 已加入可控 stub 与 Xbox→PlayStation→Xbox 像素断言，本地 Release `LoSettingsInteraction` 编译及 `settings_interaction` 1/1 已通过。修复后的 Settings workflow `36228625840` 与 AF workflow `36228625841` 均已 SUCCESS；本地 fixture 1/1 与 Settings 双平台结果也已通过。原生画面验收以及 Mod Organizer 2／USVFS 验收仍待完成；PR #68 不等同于发布验收。
