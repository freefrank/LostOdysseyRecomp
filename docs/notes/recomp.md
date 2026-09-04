# 重编译配置笔记

## 目标二进制

四张盘的 default.xex 解密解压后逐字节相同（md5 f07cb5afd8ab6fa8fbdc14f4fceb5858），
重编译目标唯一，用 Disc 1 的即可。镜像 base 0x82000000，size 0x13C0000，entry 0x827CA440。

## 节区布局（xexdump 输出）

| 节 | 起始 | 大小 | 说明 |
|---|---|---|---|
| .rdata | 0x82000600 | 0x21EF2C | 只读数据、字符串（UE3 反射名在这里找） |
| .pdata | 0x8221F600 | 0x62798 | 函数表，XenonAnalyse 用它做函数边界 |
| .text | 0x82290000 | 0xE4AA6C | 主代码，约 14.3 MB |
| .embsec_* ×8 | 0x830DAC00 起 | 共约 0x50000 | 嵌入代码段，名字乱码，大概率是加密/压缩后的安全段或 XNET 相关，待查 |
| .data | 0x83130000 | 0x248E98 | 可写数据 |
| .tls | 0x83379000 | 9 | |
| .XBMOVIE | 0x83379200 | 0xC | |
| .idata | 0x83380000 | 0x452 | 导入表 |
| .XBLD | 0x83390000 | 0xD0 | |
| .reloc | 0x83390200 | 0x144A24 | |

## 已定位的辅助函数（tools/find_ppc_helpers.py，每个模式唯一命中）

| 键 | 地址 |
|---|---|
| restgprlr_14 | 0x82B7A700 |
| savegprlr_14 | 0x82B7A6B0 |
| restfpr_14 | 0x82B7B17C |
| savefpr_14 | 0x82B7B130 |
| restvmx_14 | 0x82DF2D98 |
| savevmx_14 | 0x82DF2B00 |
| restvmx_64 | 0x82DF2E2C |
| savevmx_64 | 0x82DF2B94 |

## 跳转表

XenonAnalyse 检测到 841 张（全部为绝对地址表），写入 `config/switch_tables.toml`。
漏检的表手工写在 `config/switch_tables_manual.toml`，重新跑 XenonAnalyse 后要重新 append。
目前手工补了一张：82FB21AC 的 u16 偏移表（16 项）。

## invalid_instructions

函数之间夹着两类 8 字节的异常处理描述符（handler 指针 + 作用域表指针），第一字分别是
`0x82B7A940`（C++ frame handler，2165 处）和 `0x830D9F4C`（C specific handler，35 处），
以及 4 字节零填充。不跳过它们，XenonRecomp 的 pass2 边界分析会把描述符当代码吞掉后面整个函数
并在带跳转表的函数上死循环（表现为零输出挂死）。另有 `.text` 尾部 830DA0BC 起四个不可解码的数据字。

## 显式函数边界（tools/gen_function_bounds.py）

XenonRecomp 的边界分析把 `bctr` 当尾调用，不在 .pdata 里的叶函数只要带跳转表就会被截短，
截短后函数内部的 `b` 变成"跳出函数"的未解析目标（首轮 91 个 switch 站点、96 个未解析目标）。
脚本规则：
1. 每个 `// ERROR <addr>` 目标成为一个函数（到下一个符号，且不超出所在 pdata 函数末尾），
   目标跨轮累积在 `config/branch_targets.txt`；
2. 每张 label 未被所在 pdata 函数覆盖的跳转表，从最近符号起、到最远 label 之后的下一个
   pdata/bl 符号止，生成一个函数；
3. `config/function_bounds_manual.txt` 放手工条目（目前一条：827C39F8 大小 0x18，
   否则它流进 pdata 函数 827C3A10 抢走其跳转表）。
生成结果写进 TOML 的 `functions = [...]`。迭代到第 8 轮收敛：0 未解析目标、0 跳转表错误。

## 生成结果

`LostOdysseyRecompLib/ppc/`：250 个文件，252 MB，62808 个函数。用 MSVC 编出来的 XenonRecomp
（`tools/xexdump/CMakeLists.txt` 里把 `__builtin_bswap*` 映射到 MSVC 内建）跑一轮约 90 秒。

## 上游 XenonRecomp 缺失的指令（已在本地补丁中实现）

上游对 5985 处指令报 Unrecognized（3129 处在 .embsec 段），全部是 16 位向量运算及少量标量：
vslh 2351、vsrah 979、vsubshs 901、vspltish 604、vandc 265、vmaxsh 238、vminsh 92、vpkswss 81、
vctuxs 78、vcmpgtsh(.) 82、vcmpgtsw. 48、vaddsws 45、eqv 30、vaddsbs 26、vnor 20、vrlh 18、
vcmpequh(.) 24、vavguh 16、vsrh 12、vpkuhus 12、vpkswus 12、mulhd 12、mulhdu 8、vsububm 7、
vpkshss 7、vsrab 6、vpkuwus 6、cror/crorc 4、frsqrte 1；另 vcmpgtuh. 30 处缺 CR6 写回。
实现在 `tools/patches/XenonRecomp-lostodyssey.patch`（对子模块 recompiler.cpp 的补丁，
`tools/build_tools.bat` 编译前自动应用）。向量元素在宿主中是反序存放的，pack 类指令按上游
VPKSHUS 的惯例传 (vB, vA)。这些实现尚未经测试验证，运行期若音频/DSP 路径出错优先怀疑这里。

剩余告警：830DA0BC 起 4 个数据字（`46000201` 等，夹在导入桩的 `mtctr r11; bctr` 之间）在函数
重编译循环里无法解码，只输出为注释，不会被执行，可忽略。

## 待办
- [x] setjmp = 0x82DF34A0（带全局钩子检查的入口，7 处 bl；本体 0x82DF34B4 保存 f14-f31/r13-r31/v 到 r3），longjmp = 0x82DF3060（21 处调用，手工恢复 FPR 后经 0x82DF334C 调 RtlUnwind）
- [ ] .embsec_* 段的性质（8 个小代码段，名字乱码，.pdata 覆盖到 8312D330）
- [x] clang-cl 22 全量编译通过（out/smoke/all，目标文件共约 250 MB）
