# 运行时纹理导入（含 4× 放大）：设计与可行性

2026-10-09。来源：静态阅读 main 41f0560c 的代码；对 `--export-assets` 全量导出（`_scratch/asset-export-full`，29,435 张）做统计；没有运行游戏。依据一栏：「代码」= 读代码确认，「统计」= 对导出 `index.csv` 的计算，「推断」= 未验证。

## 结论

- 可行。拦截点是 `GetTexture()` 的上传分支。它只在新建缓存项和检测到内容变化时运行，所以指纹每次上传算一次，绘制路径没有额外开销。
- 指纹：上传时对基础层「按存储形式」（tiled、未做字节序交换）的字节算 XXH3-64。导出器对 LZO 解压后的 mip 0 做同样的计算。两边字节应当相同（推断），P0 用实测匹配率确认。
- 更大的纹理：着色器里所有以纹素为单位的计算都经过 `xeTextureSize`。普通上传现在不填它（=0，退回主机尺寸）。替换项把它设成原始逻辑尺寸即可，分辨率缩放的 resolve 已经在走这条路。
- 载荷：LOTEX1 只能装与原图同尺寸的 RGBA8，装不下 4× 加 mip 链。需要一个新的版本化格式（BC1/BC3/BC4/BC7 + 完整 mip），由 `lo_mod.py pack` 预压缩。格式需要你拍板。
- 显存：一张中等地图包 4× RGBA8 约 0.8 GB，不可行；4× BC 约 0.1 GB。
- 管线缓存、着色器包、超分和帧生成都不受影响。

## 1. 上传拦截点

| 项 | 内容 | 依据 |
|---|---|---|
| 入口 | 每次绘制 `bindTextures` 对着色器用到的每个槽读 6 个 fetch 常量，调用 `GetTexture` | `gpu/renderer.cpp:8827-8848`，代码 |
| 已知信息 | format、endian、base、tiled、pitch、宽高（7064-7075）；packed mips、sourceMip、mipAddress、mipLevels（7077-7087）；缓存 key = 地址 + 格式 + 尺寸 + flags + mipAddress（7095，`texture_key.h:8-19`） | 代码 |
| resolve 先排除 | `FindResolved` 命中就返回 resolve 纹理，不进上传（7103-7166）；resolve 写入时 `InvalidateRange` 退役重叠的上传纹理（7417-7432，调用点 12476/12511/12617） | 代码 |
| 命中路径 | 每个活跃纹理每帧做一次抽样哈希，每 16 帧一次全量哈希，变了就退役重传（7167-7189，`texture_content.h:93-104`） | 代码 |
| 上传路径 | 算布局（7215-7241）→ 逐块拷贝并交换字节序到 staging（7243-7290）→ 读 mip 链（7312-7341）→ 无 BC 设备 CPU 解码（7342-7378）→ 记录 guest 哈希（7381-7383）→ 建纹理并经上传环拷贝（7384-7413） | 代码 |
| 频率 | 每个缓存 key 一次，加上每次内容变化一次。计数已有：`textureReuploads`（13260-13262），`nTexture`/`texBytes`（7191-7192，13216-13234） | 代码 |
| 数据可能晚到 | 首次上传可能是全零，之后靠重哈希重传（注释 7171-7173） | 代码 |

**替换条件**：`tiled && dimension == 1 && base != 0 && sourceMip == 0 && format ∈ {2, 6, 18, 19, 20} && 最短边 > 16 && !FindResolved`。

- 电影 Y/U/V 是线性 `LIN_L8`（`hd-cutscene-replacement.zh-CN.md:26`），被 `tiled` 排除；导出器也跳过 `bNoTiling`（`modding/asset_export.cpp:439`）。
- cube/3D：导出器不支持 `TextureCube`（`asset_export.cpp:367`）。
- `base == 0` 是只有低层 mip 的纹理（7079-7080，7203-7210）。v1 不替换，P0 统计比例。
- 最短边 ≤ 16 时 level 0 在 packed tail 里，和低层 mip 共用一个 tile（7085，`texture_layout.h:46`）。共 371 张，其中 160 张 < 16，texture_prep 本来就跳过（`texture_prep.py:81`）；另外 211 张正好 16，放大了也不替换（统计）。
- 手柄图集：上传时识别并另建 PlayStation 变体（7298-7308，`SelectControllerAtlas` 6979-7038）。v1 识别到就不替换。
- 动态写入的纹理（字形缓存等）不在导出表里，指纹不会命中，自动排除（推断）。

## 2. 身份桥：指纹

```text
fp = XXH3_64(基础层 tiled 范围的原始字节, 长度 = pitchBlocks × blocksYAligned × bytesPerBlock)
匹配键 = (fp, Xenos format, width, height)
```

- **字节范围**：不能用 `guestBytes`，它按 4 KiB 向上取整（7233-7234），G8 小纹理会把后面无关的内存算进去。用不取整的 tiled 范围。
- **运行时成本**：XXH3 已经链接（`texture_content.h:18-23`），上传时本来就对同一段内存做全量哈希（7382）。多一次基础层 XXH3：512² DXT1（128 KiB）约 10 µs，2048² DXT5（4 MiB）约 0.3 ms，远小于逐块解码循环（推断）。结果存进 `HostTexture`，绘制路径零开销。
- **导出器**：`ReadTopMip` 返回 LZO 解压后的 mip 0，注释写明「tiled, big endian」（`asset_export.cpp:411-459`）。用同一公式（导出器的 pitch 见 :513）取前 N 字节算同一个哈希，写进 `index.csv` 新列 `fingerprint`。
- **两边字节是否相同**：导出器直接对这些字节 untile 就得到正确图片，运行时也直接对 guest 内存 untile，所以游戏大概率原样拷贝（推断）。没确认的是 tile 里的填充字节：包里是什么，guest 内存里就取决于游戏是否整段拷贝。P0 同时记录备选的「线性块」指纹（按行拼接未交换字节序的有效块，不含填充），哪个匹配率高用哪个。
- **按 mip 层记录**：导出器读到了 mip 数，但只取第一个 bulk（`asset_export.cpp:448-450`）。如果游戏把某些纹理建成缩小版（UE3 流送或 LOD 组丢掉顶层，推断），运行时看到的是第 k 层的字节，只有 mip 0 的表永远对不上，P0 也分不清原因。所以导出器对每个存储层都算指纹（后面的 bulk 头格式相同），连接脚本按层统计。替换时第 k 层的匹配直接用替换图自己的第 k 层：链是完整的，边长都是 2 的幂。
- **重复**：同名、同尺寸、同格式的 key 有 5,006 组（18,419 个 key，不含 lightmap）。抽 150 组，80% 的 PNG 逐字节相同。粗算三分之一以上的 key 是别的 key 的副本：同一张纹理被烘进了多个地图包（统计）。同一指纹的多个 key 解析到同一个载荷就没问题；解析到不同载荷时，按 wiki 规则拒绝并诊断（`docs/wiki/Runtime-Texture-Replacement.md:21`）。打包器按指纹去重并提前报冲突；运行时按指纹共享一张 GPU 纹理。
- **碰撞**：表里不到 2 万个不同指纹，一次会话上传最多百万量级，误匹配概率约 1e-9，再加上格式和尺寸校验，可以忽略。

## 3. 更大的替换纹理

| 位置 | 现状 | 4× 时 | 依据 |
|---|---|---|---|
| 纹素偏移、非归一化坐标 | `XeTex2D` 算 `offset / dims`、`uv / dims`；dims 在 `xeTextureSize` 非零时用它，否则用 `GetDimensions` | 普通上传从不设 `guestWidth`（赋值只在 2670/2689/3617/6805/7127/12155/12256），所以现在拿的是主机尺寸。替换项必须把 `guestWidth/Height` 设成原始逻辑尺寸（写入点 9406）。resolve 缩放已经这样做（1934，7111-7134） | `gpu/shader/common_hlsl.h:355-382`，代码 |
| getWeights | `XeWeights2D` 同样用 dims | 同上 | `common_hlsl.h:405-408`，代码 |
| 寄存器 LOD、梯度、LOD bias | 翻译器不实现，只分 `Sample` 和 `SampleLevel(0)`；sampler 不带 fetch 的 mip 范围和 bias | 硬件按主机尺寸算 LOD。替换带完整 mip 链时自然多出两级，无需改动 | `xenos_translator.cpp:502-508`，`sampler_description.h:6-31`，代码 |
| `SampleLevel(0)` 和 basemap mip filter | 前者用于无计算 LOD 的 fetch 和顶点着色器；后者把 `maxLOD` 设为 0 | 永远采样 4× 基础层，缩小时闪烁、带宽大。P0 统计 sampler key bit 4-5，必要时这类绑定改回原图 | `common_hlsl.h:378-382`，`xenos_translator.cpp:36`，`sampler_description.h:13-14`，风险 |
| 寻址和边框 | 寻址模式在归一化空间一一对应；边框颜色目前不取自 fetch 常量 | 没有变化。唯一的风险是放大破坏了平铺的周期性，在 WRAP 下出接缝；texture_prep 对世界纹理用 `"pad": "wrap"` | `sampler_description.h:15-26`，`texture_prep.py:35`，代码 |
| 各向异性 | `Eligible` 看 `guestBytes != 0` | 替换项保留 guest 字段即可 | 9418-9421，代码 |
| 渲染器簿记 | 抽样检查比的是 guest 哈希 | 替换项的 key、`guestAddress/guestBytes`、`guestHash/guestFullHash` 必须保持 guest 值，否则每帧重传。`width/height`、`bindingWidth` 只用于诊断描述 | 7178，6967-6977，代码 |
| 直接 `GetDimensions` | 只有 UE3 阴影投影里的场景深度 resolve | 不受影响 | `xenos_translator.cpp:73-120`，代码 |
| 游戏自己算的纹素常量 | UI 半纹素偏移等按原尺寸算 | 归一化坐标下仍落在原网格上，4× 只是更细；后处理读的是 resolve | 推断 |
| 内容 | 最近邻采样的数据/LUT、图集边缘渗色、法线通道布局、lightmap 图集 | 打包器按类别跳过或保持原尺寸；P0 记录过滤模式和 fetch swizzle | 9405，推断 |

检测办法：

1. **最近邻 4× 测试包**：每张替换都是原图最近邻放大，mip 2 与原图逐像素相同。实现正确时画面应与原版几乎一致，任何偏移或错位都说明有尺寸相关的 bug。
2. **染色测试包**：替换图整体染色，直接看哪些表面被替换了。
3. 翻译器为每个槽记录是否用了非归一化坐标、偏移或 getWeights。替换纹理绑到这样的槽时打一次日志。

## 4. 载荷格式与显存

显存（统计；含 mip 链 ×4/3；texture_prep 上限 4096，`texture_prep.py:31`，所以 2048 的源图只放大 2×；所有导出纹理都是 2 的幂）：

| 范围 | 张数 | 原版 | 4× BC 混合 | 4× 全 BC7 | 4× RGBA8 |
|---|---|---|---|---|---|
| 中位地图包 `gu2_0_map.xxx` | 61 | 7 MB | 105 MB | 210 MB | 841 MB |
| 最大地图包 `u3f_0_map.xxx` | 141 | 11 MB | 177 MB | 342 MB | 1,368 MB |
| 全部导出（含重复） | 29,435 | 3.8 GB | 55 GB | 101 GB | 408 GB |

地图包不含角色、物件、特效和 sys 的共享纹理。运行时实测点：4K Uhra 每帧活跃 173 张、24 MB（`texture_content.h:93-94`）。按 4× 同压缩率约 0.4 GB；若全部解成 RGBA8 约 3 GB。结论：必须预压缩。

| 原格式 | 张数 | 建议 | 说明 |
|---|---|---|---|
| DXT1（18） | 25,694 | BC1 或 BC7 | BC7 质量更好，体积翻倍 |
| DXT3 / DXT5（19 / 20） | 36 / 3,422 | BC7 或 BC3 | |
| A8R8G8B8（6） | 177 | BC7 或 RGBA8 | 主机纹理通道顺序是 B,G,R,A（8in32 交换后按 R8G8B8A8 读，6934；导出器写真 RGBA，`asset_export.cpp:493-496`），打包时要换 R/B。前提是 format 6 的 fetch endian 为 2（8in32），P0 的 endian 列确认 |
| G8（2） | 106 | BC4 | 原来是 `R8_UNORM`（6931），BC4 读出同样的 (r,0,0,1) |

- 只用 `*_UNORM`，不用 `_SRGB`：伽马由着色器的 `XeGammaToLinear` 处理（`common_hlsl.h:148-169`）。
- 通道布局和原图解码后一致，fetch 的 sign/swizzle 继续生效（9405，`common_hlsl.h:159-177`）。BC5 法线要等 P0 证明着色器只读两个通道。
- **容器**：LOTEX1 是 RGBA8，尺寸必须等于原图（`image_mod.h:15,37-39`），wiki 也写明它是唯一的 v1 格式（`Runtime-Texture-Replacement.md:33`）。两个选项：(a) 新的 LOTEX2：魔数、key、源格式和尺寸、指纹列表、主机格式、mip 数，后跟可直接上传的各级块数据；(b) LOTEX1 头加一个新 format 码，key 后面内嵌一个 DDS（DX10 头），方便用现成工具检查。旧代码遇到未知 format 会回退原图（`image_mod.h:38`）。编码在打包时做（texconv 或 Compressonator，生成完整 mip）。

平台：

- D3D12：BC 默认可用（`gpu/backend_device.h:16`）。
- Vulkan：看 `textureCompressionBC`（`backend_device.h:38`），启动日志 `vulkan limits: ... bc=`（`video.cpp:2389-2393`）。
- Metal：plume 有 BC1/BC3/BC7 的格式映射（`thirdparty/plume/plume_metal.cpp:405-446`），`Inspect` 把非 Vulkan 后端都当作支持 BC（`backend_device.h:16`）。这只是映射表，不是能力查询。Apple silicon 的 macOS 支持 BC（推断），要在 MacBook 上验证 BC7 的上传和采样。
- Android：无 BC 的设备走 CPU 解码（7342-7378），`bc_decode.h` 只有 BC1-3（:6-9），4× RGBA8 在手机上不可接受，所以这类设备不加载替换。有 BC 的设备（Adreno 专有驱动和 Turnip，推断，看 `bc=` 日志确认）照常加载，但设显存上限：跳过顶层 mip，4× 包在手机上相当于 2× 或 1×，同一个包适用所有平台。Vulkan 后端本来就要求 shaderInt64 和至少 5 个 descriptor set（`backend_selection.h:63-65`），Mali 多半过不了这一关（推断，来自以前的设备日志，未在仓库里核对）。ASTC 要给 plume 加格式、再出一套载荷，不建议。

## 5. 加载与缓存

- **第一版同步**：上传 miss 且指纹命中时直接读文件上传。上传环 96 MiB（279），满了会在帧中途 Flush/Begin（4616-4633）。4096² BC7 整条链约 22 MB，进图时会卡一下，但能用。
- **之后异步**：命中后先用原图，工作线程读文件；读完后在下一次命中路径换掉 `tex->texture`，旧纹理进 `retiredTextures` 等 fence 后释放。描述符缓存按指针做 key（`texture_descriptor_cache.h:43-61`），换指针自然生成新的描述符集。任务带上 `TextureKey`，换之前确认缓存项还在、guest 哈希没变。每帧上传字节设预算。
- **共享和淘汰**：替换的 GPU 纹理按指纹共享，重复纹理只占一份。引用归零后进 LRU，按显存预算淘汰，回到同一地图时不用重读磁盘。CPU 端不保留文件数据。
- **失效**：guest 内容变化或 `InvalidateRange` 时，原有机制退役缓存项，重传时重新算指纹（7186-7188）。`modding::Generation()` 变化（只在 Initialize/Reload，`mod_api.cpp:205-252`）时清空整个纹理缓存。
- **不受影响**：`pipeline_cache::Key` 不含纹理（`pipeline_cache.h:35-44`），着色器用的 `Texture2D<float4>` 与格式无关，所以管线缓存和着色器包不变。DLSS/FSR/XeSS/帧生成读的是 resolve 和渲染目标，不走上传路径，只受显存压力和进图卡顿的间接影响。渲染器不加负 mip bias（`sampler_description.h:30`），超分时由渲染分辨率决定用哪级 mip。

## 6. Mod API 集成

- 导出器 `index.csv` 加 `fingerprint` 列（`asset_export.cpp:935-939`）；texture_prep 原样传递。
- `lo_mod.py pack`（`:256`）把 key 和指纹写进载荷头，同一指纹的多个 key 去重。standalone 布局可以让多行 `image:<key>=` 指向同一个文件。
- 运行时在 Initialize/Reload 之后用后台线程枚举当前生效的图片条目（standalone 赢家加 `overlay/images/*.lotex`），读每个文件头，建 `fingerprint → (key, path)` 表。`Snapshot.entries` 目前是私有的（`mod_api.cpp:21-28`），需要加一个只读枚举接口。
- overlay 文件名仍按 key 的 FNV 哈希（`mod_api.cpp:193-204`），MO2 不用改，冲突仍按 MO2 顺序决定。
- 上传时只查内存表。命中后用已有的 `Resolve(key)` 复核优先级（overlay > provider > standalone，`mod_api.cpp:269-304`），结果按 (指纹, generation) 缓存，不在绘制时访问文件系统。
- 没有图片 Mod 时不扫描、不算指纹。

## 7. 分阶段计划

| 阶段 | 内容 | 主要改动 | 工作量 | 风险 |
|---|---|---|---|---|
| P0 诊断 | 环境变量 `LO_TEXTURE_FINGERPRINT_LOG=<路径>` 记录每次上传；导出器加指纹列和 `--fingerprints-only`（不写 PNG）；连接脚本算匹配率 | `gpu/renderer.cpp`（GetTexture、绑定处）、新的共享头 `modding/texture_fingerprint.h`、`asset_export.cpp`、新脚本 `tools/modding/fingerprint_match.py` | 1 天 + psvita 一次运行 | 匹配率低时要先查 RE（游戏是否改写纹理数据） |
| P1 同尺寸替换 | 指纹表、同步读取 LOTEX1、按主机通道顺序上传 RGBA8、CPU 生成 mip | `modding/mod_api.{h,cpp}`（枚举）、新的 `modding/texture_replacements.{h,cpp}`、`renderer.cpp` | 2 天 | 同尺寸 RGBA8 替换 BC1 占 8 倍显存，只适合测试 |
| P2 更大纹理 | 替换项设 guest 逻辑尺寸、要求完整 mip 链、尺寸比为 2 的幂；最近邻测试包和染色测试包 | `renderer.cpp`、`tools/modding` | 2 天 + 验证 | §3 的风险，尤其 LevelZero / basemap |
| P3 压缩载荷 | 选定容器；打包时 BC 编码、去重、写指纹；异步加载、LRU、显存预算 | `image_mod.h` 或新头、`lo_mod.py`、`texture_replacements`、`renderer.cpp`、wiki | 3–4 天 | 编码器的跨平台可用性；包体积（全量 4× 去重后约 35–65 GB，推断） |
| P4 平台 | Vulkan 按 `bc=` 决定；无 BC 不加载；手机跳顶层 mip；MacBook 验证 BC7；TB321FU 实测 | `renderer.cpp`、设置项 | 1–2 天 | Metal BC7 未验证；手机显存 |

工作量是估计。

**P0 每条记录的字段**（每次 miss 上传一行，包括重传；同一指纹在一次运行内只记首次，另计次数）：

- 帧号、guest 地址、format、endian、tiled、pitch32、宽高、原始宽高、sourceMip、`base != 0`、packedMips、mipLevels、dimension、是否为重传。
- tiled 范围字节数和 `fp_tiled`；`fp_linear`；该范围是否全零。
- 绑定时另记一种行：每个不同的 (指纹, sampler key, textureInfo) 组合记一次，每个指纹最多 8 种。字段：textureInfo（sign + swizzle，9405）、sampler key（过滤、mip filter、寻址、各向异性，9412-9425）、VS/PS 哈希。同一张纹理会被不同材质用不同的采样方式读，只记首次绑定会漏掉 §3 需要的信息。

导出器对应列：`fingerprint`、`fingerprint_linear`、`raw_bytes`、`extent_bytes`、`mips`，以及每个低层 mip 的指纹（单独一个文件，每行 key、层号、宽高、指纹）。`raw_bytes` 与 `extent_bytes` 不相等的行单独列出。

**psvita 运行**：标题、一张野外地图、Uhra 城、一场战斗、菜单、一段 CG。脚本按指纹连接两边，按张数和字节数分别给出匹配率，再按格式和包类别拆开，列出最大的未匹配项。可以先定一个目标：符合替换条件的上传，按字节计 ≥ 95% 能匹配。

## 需要你决定

1. 载荷容器：LOTEX2，还是 LOTEX1 加新 format 码内嵌 DDS。
2. 指纹的字节范围，等 P0 结果再定（tiled 范围还是线性块）。
3. 重复 key 解析到不同载荷时：按 wiki 拒绝，还是按 Mod 优先级取一个。
4. 默认显存预算和手机上的尺寸上限。

## 导航

- 导出数据：`C:\Users\freefrank\worktrees\LostOdysseyRecomp\_scratch\asset-export-full\textures\index.csv`；统计脚本是一次性的，没有保留。
- [运行时纹理替换的剩余工作](../wiki/Runtime-Texture-Replacement.md)、[Mod API](../wiki/Modding-API.md)、[高分辨率 CG 替换](hd-cutscene-replacement.zh-CN.md)。
