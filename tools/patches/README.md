# 本地依赖补丁

主仓库固定上游子模块提交，并保存本项目补丁；不要把依赖修改推送到上游仓库。
在首次检出的干净子模块上，从仓库根目录执行：

```powershell
git submodule update --init --recursive
git -C tools/XenonRecomp apply --check ../patches/XenonRecomp-lostodyssey.patch
git -C tools/XenonRecomp apply ../patches/XenonRecomp-lostodyssey.patch
git -C thirdparty/plume apply --check ../../tools/patches/plume-lostodyssey.patch
git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch
```

XenonRecomp 补丁保存本项目指令、分析与上下文适配。plume 补丁保留 D3D12 readback 安全检查、stencil reference 赋值及深度清除矩形分批提交（避免大量矩形触发驱动退出），并使 pool／device 两处 `createTexture` 在原生资源创建失败时返回 `nullptr`，让调用方的分配失败回退能够识别失败；同时补充 Windows Vulkan 所需的 swapchain 图像数量、MRT viewport、device-address、上传一致性／readback 失效、捕获路径和相关资源创建处理。该补丁只描述本项目已核对的 plume 集成范围，不代表所有平台或 GPU 均已通过验证。
补丁应用后，这两个子模块有本地修改是预期状态。`.gitmodules` 对它们设置 `ignore = dirty`，日常主仓库状态不再反复提示补丁造成的工作树修改；子模块提交指针发生变化时仍会提示。检查实际修改可运行 `git status --ignore-submodules=none --short` 或进入子模块运行 `git status --short`。此设置也会隐藏额外的意外修改，因此修改依赖时仍需检查子模块状态及补丁。`build_runtime.bat`不自动应用补丁；`build_tools.bat`会检查并尝试应用XenonRecomp补丁，plume仍需按上面的命令准备。
已有本地修改时不要重复应用；可以用以下只读检查确认补丁已应用：

```powershell
git -C tools/XenonRecomp apply --reverse --check ../patches/XenonRecomp-lostodyssey.patch
git -C thirdparty/plume apply --reverse --check ../../tools/patches/plume-lostodyssey.patch
```

继续修改依赖后需同步相应补丁，并重新构建验证。游戏数据、生成的 PPC 代码及工具缓存不属于补丁。

拉取更新了受跟踪的 XenonRecomp 补丁时，应先检查 `tools/XenonRecomp/` 的**实际已修改工作树**，将其与更新后的补丁谨慎同步；不要在已有修改上盲目重复应用，也不要丢弃无关的本地改动。确认实际源码与预期补丁一致后，按仓库根目录的正常顺序执行 `.\tools\build_tools.bat`、`python -B tools/ppc_codegen.py generate`、`.\tools\build_runtime.bat`。`build_tools.bat` 会尝试自动应用补丁，遇到部分更新的工作树时应先理顺源码与补丁，而不是随意重盖工具收据或复用旧生成器。仅增量构建运行时或执行 `python -B tools/ppc_codegen.py check`，都不能证明实际依赖源码已跟上受跟踪补丁；例如本地头文件中残留旧 `PPCTimeBase` 时，可能继续生成使用旧时钟路径的 PPC 代码。

Vulkan 改动应从受跟踪的 plume 子模块状态和上方补丁应用；它们不替代驱动提供的 `vulkan-1.dll`／ICD，也不要求另行捆绑 SDK。保持 plume 源码与补丁处于兼容提交，并在修改过的依赖树上应用前先审阅补丁。

2026-09-07 核验：保留原有 hunk 后，Plume 补丁从固定 HEAD `d890ac8` 应用到隔离 index／object store，所得 Git 规范化 blob 与当前源一致；XenonRecomp 补丁从固定 HEAD `ddd128b` 的同类核验也通过。现有工作树的 CRLF／混合换行导致部分原始文件字节不同，未重写换行或宣称 raw 字节一致；真实子模块源文件、index 和 HEAD 均未改变。证据：`out/v0.4.0-followup/patch-sync-validation.json`。

2026-09-14 精简核验：针对固定 HEAD `d890ac899e505fb30040e037a4037cdeca68f033`，Plume 补丁由 78,500 行／3,398,001 字节缩减为 1,849 行／92,265 字节，保留 6 个真实改动文件（含新增 `plume_log.h`），并消除 3 个同 SHA 的 `-dirty` gitlink 伪差异。正向 `apply --cached --check` 通过，应用后 tree 与旧补丁应用 tree 一致（133 个普通文件规范化内容及 gitlink 均核对）；当前 Plume 工作树 `reverse --check` 通过。未改动源码，未进行构建或游戏运行验证；该补丁尚未发布。
