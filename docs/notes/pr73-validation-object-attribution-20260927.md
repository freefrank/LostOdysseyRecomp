# PR73：validation对象归因与深度-only附件修正

日期：2026-09-27。代码基线：`ae82bfbfe94a576efe727504053b257b8e4e9572`。本记录补充[FSR+FG恢复记录](pr73-fsr-fg-recovery-20260927.md)，不覆盖旧运行报告，也不将旧binary证据移植到本次代码。

## 已收到的实机证据

用户提供的ae82bfb报告记录：Windows full native build成功；FSR无层运行、FSR同步验证层两次resize及DLSS最小回归均正常关闭，SDK errors与creation failures为0，提交/完成serial分别为2359、856、2450。这里只复用原报告的有限范围，不重跑已有CI、初始化或正常退出作为铺垫。FSR+FG的旧`0xbad0000b`在这些运行中已消失；后台`generated_intervals=0`不构成实际生成或显示验收。

已读取run02的`runtime.log`、`stderr.log`、`sl.log`及独立的`validation.log`。validation文件SHA256：`b69be34f774209564fe165dde3bdd61b7215694d5e71e66d0d094b133c1db029`。其中记录：

| 错误 | 对象与直接证据 | 目前结论 |
| --- | --- | --- |
| PRESENT_AFTER_WRITE | queue `0x10b164d0`，chain `0x1c400000001c4`，image `0x1c500000001c5`，最后写入是`vkCmdEndRenderPass`触发的layout transition | 10条后触发重复上限；尚未确定该image属于host返回的代理图像还是SDK最终图像 |
| DestroyImageView `01026` | view `0x19450000001945`/`0x19910000001991`/`0x19f800000019f8`分别仍被descriptor `0x17a600000017a6`/`0x12d500000012d5`/`0x17b500000017b5`使用 | 3条实际GPU未完成使用证据，不能改称无效句柄；原日志没有创建/销毁调用者与完成serial对应 |
| framebuffer `04533/04534` | attachment[1]为880×896；framebuffer先为1760×1792或1760×1728，resize后为1408×1434或1408×1382 | 尺寸合同错误；宽度10条触发重复上限，高度8条；并非单凭数值就可证明所有调用都来自同一路径 |
| topology `08773` | shader module `0x1a200000001a2`用于POINT_LIST而没有PointSize写入 | 5条；需映射shader/pipeline，不跳过绘制或开启设备特性掩盖错误 |

这些是记录数，不是完整频率。该validation日志没有时间戳、命令缓冲句柄或调用栈，不能从相邻记录、句柄数值大小或无记录推断全部所有权。

## 本次功能修正：深度-only附件一致性

runtime在26.896秒记录同一880×896 guest尺寸：颜色目标按2560×1440比例分配为1760×1792，深度目标保留880×896；resize后颜色变成1408×1434而深度仍为880×896。代码中已有`depthOnlyRaster`策略让无颜色写入且有深度附件的draw按深度网格光栅化，但pipeline的rtFormat及framebuffer仍绑定不需要的颜色目标，存在可独立确认的尺寸合同缺口。

现在此类draw同时使用`rtFormat=UNKNOWN`的无颜色pipeline及`GetFramebuffer(nullptr, depth)`；显式EDRAM别名清理之后也重新绑定同一framebuffer。pixel shader、depth/stencil状态和原有深度viewport保持不变，显式颜色别名清理仍执行；不缩小深度绘制区域来掩盖不匹配，不修改FSR尺寸/jitter/reset/token或NGX生命周期。有任意RT0颜色分量写入，或没有深度附件的draw保留原路径。

这修正了确认存在的代码缺口，**尚未实机证明覆盖日志中的全部04533/04534**。原日志没有逐draw的color mask；真正同时写颜色/深度的其他尺寸组合不由该规则处理。本轮没有修改通用Plume framebuffer尺寸。

## 事件级诊断：不能记为P0同步修复

`LO_VK_OBJECT_TRACE=1`启用有界句柄日志；普通运行默认关闭。现有隔离脚本仅在指定`-ValidationLayerDirectory`时自动启用，将诊断写入`stderr.log`，VUID仍在独立`validation.log`。

记录host-facing swapchain返回图像、每次chain创建后前4次Present的queue/semaphore、Plume及FSR SDK两条dispatch路径的image-view创建/销毁、renderer framebuffer映射、已有fence完成后的retirement事件，以及point pipeline的shader hash/module与元数据。每个进程最多8192个诊断事件，达到上限明确报警。`route`表示截获调用路径，不表示唯一消费者；没有拦截Streamline私有dispatch表，缺失记录不能据此认定为SDK对象。shader元数据也不能替代实际SPIR-V检查。

宿主原Present semaphore顺序、barrier、post-present checked fence及资源寿命均未修改。FSR的诊断回调仍转发原native proc，不更换资源、不额外等待。本轮没有新增每帧全局queue/device idle、全资源扫描、放宽completion或提前释放。**PRESENT_AFTER_WRITE和DestroyImageView仍未闭环；PointSize尚未修复。**

## 定向验证与剩余范围

本地仅构建`LoDrawAttachmentPolicyTest`与`LoVulkanObjectTraceTest`并运行3个CTest：3/3通过。附件策略33项检查覆盖全部RT0颜色mask及无深度分支；trace helper覆盖整数/指针句柄、关闭/开启和8192事件上限。两目标另经C++20 `-Wall -Wextra -Werror -pedantic`编译并运行。它们只执行CPU辅助策略，**不编译或执行renderer、Vulkan hooks、FSR SDK native回调，更不代表GPU错误已消失**。Windows/Linux定向CI结果以PR对应运行记录为准。

```sh
cmake -S tools/tests/streamline_fg -B build/pr73-validation-fix -DLO_STREAMLINE_FG_CPU_ONLY=ON
cmake --build build/pr73-validation-fix --config Release --target LoDrawAttachmentPolicyTest LoVulkanObjectTraceTest --parallel 2
ctest --test-dir build/pr73-validation-fix -C Release -R '^(draw_attachment_policy|vulkan_object_trace_.*)$' --output-on-failure
```

远程设备本次查询仍离线；未运行新Windows full native build或任何游戏、GPU、窗口、显示测试，未修改用户原始settings/save/profile/cache。下次先构建新binary，仅补受影响的FSR+FG同步validation及必要resize，保留同次运行四份日志用于对象映射；仍报错时按具体所有者及未完成提交修正，不能将新增trace当作修复通过。通过后再前台核对真实生成、节奏及显示。原NGX初始化修复的无层短测和DLSS最小回归无需无差别重跑；发生直接回归时再扩大测试。

P0 Gate 1仍为**NOT PASSED**；失败注入、settings restart、前台生成和显示仍待验证。FG保持默认关闭，UI分离仍暂挂，PR保持draft。
