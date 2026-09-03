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

XenonAnalyse 首轮检测到 841 张，写入 `config/switch_tables.toml`。
后续 XenonRecomp 报错或运行时崩溃指向 `mtctr r0 / bctr` 附近时，回来手工补。

## 待办
- [ ] setjmp / longjmp：找 RtlUnwind（xboxkrnl 序号 0x147）导入桩的调用者，longjmp 调用它，setjmp 紧随其后
- [ ] .embsec_* 段的性质
- [ ] XenonRecomp 首轮输出的错误清单
