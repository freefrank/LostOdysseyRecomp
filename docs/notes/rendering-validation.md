# 渲染验证（2026-09-04）

## 构建与回归测试

先按 [依赖补丁说明](../../tools/patches/README.md) 准备子模块，并准备 README 中所列本地游戏数据、重编译产物和 Windows 工具链。
从仓库根目录运行：

```powershell
.\tools\build_runtime.bat
.\tools\build_runtime.bat LoMemoryAliasTest
.\tools\build_runtime.bat LoShaderAluTest
.\tools\build_runtime.bat LoStencilTest
.\out\build\windows-clang\LostOdysseyRecomp\LoMemoryAliasTest.exe
.\out\build\windows-clang\LostOdysseyRecomp\LoShaderAluTest.exe
.\out\build\windows-clang\LostOdysseyRecomp\LoStencilTest.exe
```

三个测试本身不读取游戏资产；当前整体 CMake 配置仍依赖本地项目构建环境。

| 测试 | 覆盖 | 平台验证 |
|---|---|---|
| LoMemoryAliasTest | A/C/E 共享写入、E 的一页偏移及释放 | Windows、WSL Manjaro |
| LoShaderAluTest | 真实 GPU ALU 旧值读取、后续结果、gamma、swizzle 和 resolve R/B 往返 | Windows D3D12 |
| LoStencilTest | 左半写 stencil=3，EQUAL/NOT_EQUAL 的 GPU 像素读回；D24 端点静态断言 | Windows D3D12，LO_BUILD_GPU=ON |

另以 LoShaderTool 编译开场现场捕获的 162 个 shader，结果 0 失败；捕获数据只在本地，不作为仓库测试资产。

## 实机复现

使用 Disc 1，默认 30 fps、默认 EDRAM transfer 和 shader cache v19。
从仓库根目录设置自动输入后启动：

```powershell
New-Item -ItemType Directory -Force out/render-check | Out-Null
$env:LO_AUTO_PULSE = '6'
$env:LO_AUTO_BUTTONS = 's@300,a@600,b@900,u@1100,a@1300,s@2000,k@2100,a@2200,a@2800,a@3400'
$env:LO_GPU_STATS = '1'
$env:LO_RELATIVE_DRAW_STATS = '1'
$env:LO_SCREENSHOT_EVERY = '300'
$env:LO_SCREENSHOT_PATH = 'out/render-check/shot.ppm'
.\out\build\windows-clang\LostOdysseyRecomp\LostOdysseyRecomp.exe --game .\LostOdysseyRecompLib\private\disc1 --quiet-kernel 2> out/render-check/stderr.log
```

自动输入按 swap 计数；本轮第 2400 帧为攻击目标列表，第 3000 帧为正面角色近景。
如果状态偏离，先确认菜单与输入时序，不把不同镜头当作渲染回归。
logger 输出在 stderr；日志和截图放在 ignored 的 `out/` 目录。

本轮基线：`out/render-final/`（几何与纹理修复后）。
光影最终结果：`out/render-light-depthpack/`；中间实验 `render-light-stencil-ref/` 的红色偏光不是最终结果。
Xenia 对照条件见 [对照记录](xenia-render-comparison.md)。

## 已知范围

已验证开场材质、模型完整性、后期轮廓和金属高光改善；未验证战斗结束、通关或全部阴影细节。
未调整曝光，也未关闭正常深度、客体遮挡开关或景深。遮挡计数仍为近似实现。
模板正反面不同参考值/掩码尚有限制，本场景没有触发对应警告。
