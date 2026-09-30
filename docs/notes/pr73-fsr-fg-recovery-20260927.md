# PR73：FSR+FG初始化与运行状态修正（待实机验证）

日期：2026-09-27。代码基线：`b001e8f645c6f0cabefbea8a7586e6354cd80a5c`。
本记录补充[上轮P0/P4记录](v0.8.0-p0-p4-cleanup-continuity-20260927.md)，不覆盖已有运行证据。

## 证据与诊断边界

用户提供的`pr73-validation-20260927/REPORT.md`记录：`run03-fsr-validation`和`run04b-fsr-no-layer`均重复出现`NGX create feature failed 0xbad0000b`，因此不能只归因于验证层。后者正常退出、baseline preserved、两项cleanup complete，但未通过FSR+FG组合验收。该报告的Windows构建、CI、DLSS/DLAA正常关闭结果继续作为旧提交的基线。

代码中存在一个可定位的生命周期差异：Streamline代理创建Vulkan设备后，native `ProbeOnce`执行独立Init/capability/Shutdown1；只有DLSS尺寸查询随后通过EnsureSession重新开启持久NGX会话，FSR不经过该步骤。这是本轮修正的候选初始化缺口，**尚未证明它就是0xbad0000b的实机根因**。没有原始NGX日志时，不能仅从错误码判定丢失DLL、格式错误或初始化顺序已经得到最终归因。

## 代码变更

1. FG与native SR共用设备时，native探测保留其成功建立的NGX运行时及能力参数，直到已有GPU owner退出清理。SR能力不可用与持有运行时分开记录；后续尺寸查询不得另开临时Init/Shutdown。未启用FG的Vulkan、独立probe和D3D12保留原行为。没有创建“占位DLSS SR feature”，也没有改写FSR尺寸、jitter/reset、token及失败动作。
2. 区分`mode_requested`和`runtime=off/pending/ready/unavailable`。SetOptions成功不再直接算作运行可用；检查GetState返回值、非零DLSSGStatus，以及Streamline延迟NGX创建错误事件。日志回调只更新原子计数，不重入SDK。其精确匹配`NGX create feature failed`，依赖当前只加载DLSS-G/Reflex/PCL、没有sl.dlss插件的约束；不拿通用sdk_errors计数充当运行健康或validation证据。
3. 创建/feature请求失败显式确认eOff并锁住失败配置；无法确认关闭仍安全终止。同一配置不逐帧重试，输入短缺、cpuSerial、geometry/temporal reset不解锁。有效SR请求/质量、输入输出尺寸、格式、设备等配置改变，或既有成功的窗口/resize quiesce之后，允许下一次合格输入重新请求SDK创建。没有新增周期性重试、强制全局等待或提前资源释放。
4. 运行可用查询与renderer输入采集准入分开。失败后仍可观察下一组有效配置，避免“停采集后永远无法恢复”。连续性成功样本移到Present状态检查后；中断后首次GetState累计计数不计作生成区间，避免把多个FG-off帧误当作2x生成。

## P0两类宿主错误：未闭环

本轮审查了最终present提交的binary semaphore、ALL_COMMANDS等待范围、proxy/native末尾layout分支、checked post-Present marker，以及snapshot/depth资源的持有和回收入口。没有从报告摘要确定`PRESENT_AFTER_WRITE`对应哪张image、哪次写入，也没有取得`DestroyImageView`的具体对象、VUID与未完成提交。

**本轮没有修改这些barrier、semaphore或ImageView释放路径，也不宣称两类错误已修复。** 现有SDK clone/layout、WAW和pacer归因继续单独保留。下一轮需用原始validation对象与提交链交叉定位；不因函数名称含Present或DestroyImageView就全局等待或一律归咎SDK。

## 本轮验证与顺序

本地只构建新增`LoFgRuntimePolicyTest`并运行`streamline_runtime_recovery`：1/1通过，44项检查，包括失败后10,000次同配置请求均被抑制、输入缺失不解除失败、必要配置/资源边界恢复、请求不等于运行可用、以及GetState查询间断的计数保护。同一测试以C++20、`-Wall -Wextra -Werror -pedantic`编译并通过。定向CI只运行该新增目标；上轮9/9 CI不作为本补丁的新验证重复报告。

```sh
cmake -S tools/tests/streamline_fg -B build/fg-recovery -DLO_STREAMLINE_FG_CPU_ONLY=ON
cmake --build build/fg-recovery --config Release --target LoFgRuntimePolicyTest --parallel 2
ctest --test-dir build/fg-recovery -C Release -R '^streamline_runtime_recovery$' --output-on-failure
```

这些CPU测试只执行生产使用的恢复/计数策略和错误消息识别，**不执行NGX、Streamline、Session、video或renderer的native代码**。远程电脑离线；本轮未完成新代码的Windows游戏构建、FSR+FG无层短测或任何GPU/窗口/显示验证，未访问和修改用户原始存档、设置及缓存。

实机顺序保持：先新binary的FSR+FG无验证层短测，检查创建错误是否消失、失败状态是否真实、是否按必要边界恢复，以及受本次NGX持有变化影响的FSR退出；通过后再做同步validation与必要resize/模式切换，最后前台生成、节奏和显示取证。既有DLSS/DLAA正常退出及旧CI不重跑作铺垫，也不能自动移植为新binary的验收。

Gate 1保持**NOT PASSED**，P4 FSR+FG组合与全程连续性仍待验证；UI分离暂挂，FG默认关闭。旧48个enabled周期和run11性能材料保留原binary与场景边界。
