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

XenonRecomp 补丁保存本项目指令、分析与上下文适配。plume 补丁保存 D3D12 readback 安全检查、stencil reference 赋值及深度清除矩形分批提交（避免大量矩形触发驱动退出）。
补丁应用后，主仓库显示这两个子模块有本地修改是预期状态。`build_runtime.bat`不自动应用补丁；`build_tools.bat`会检查并尝试应用XenonRecomp补丁，plume仍需按上面的命令准备。
已有本地修改时不要重复应用；可以用以下只读检查确认补丁已应用：

```powershell
git -C tools/XenonRecomp apply --reverse --check ../patches/XenonRecomp-lostodyssey.patch
git -C thirdparty/plume apply --reverse --check ../../tools/patches/plume-lostodyssey.patch
```

继续修改依赖后需同步相应补丁，并重新构建验证。游戏数据、生成的 PPC 代码及工具缓存不属于补丁。
