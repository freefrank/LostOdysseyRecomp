# 随时存档：菜单权限与验证

2026-09-05，亚洲版当前构建。功能已加入，运行时构建通过；新构建开关、保存及重启位置读回通过，普通存档点及进入营地前后的权限恢复回归通过；桌面复选框实际点击与布局尚未验证。

## 已确认的路径

- System 菜单 Save 项 ID=34 (0x22)，菜单表首项位于 0x8326D690，条目长 0x1c。
- 0x80000000 是可见位，0x40000000 是启用位。82877110 查询这两位，82876EA8 设置可见/启用参数。
- 82878CB8 构建 System 列表，widget 为主菜单对象+0x238d4，828B6968 从表创建条目。
- 822E0E10 中 822E7EA4 判断选中项34，822E7FF8 调用8287C8F8，后者调用82860008进入原有存档界面。
- EasySave native828F1A68→rpPlayData虚表+0x220→82906E18 是内存状态复制，不是菜单允许保存判断。EasySaveFlag反射为字段+0x3c，mask0x40000000；未修改该字段。
- CheckSavePoint 会激活存档点，不应伪造该调用或存档点指针。

## 后台原型验证

独立副本 out/critical-section-camp-01 PID33632，临界区修复构建；未操作桌面或原始用户save/profile。
营地开箱后 System 的 Save 灰色；fcController_0=0x03a43c00，mp_SavePoint(+0x86c)=0。
只将表首项0x80000000改为0xc0000000，退回再进入System：shot47183恢复可选，shot48111进入存档检查，shot48478列出槽位。
选择空槽02：shot49648保存中，shot50635显示槽02完成，时间00:46，675G，Kaim Lv10。副本生成第二份206000字节save.bin。
这证明正常流程能从非存档点写入；尚未证明重启恢复的位置/剧情状态完整。
原型进程保留在保存列表；input最后25，shots最后22；测试改过的表启用位目前仍开启。

## 新实现与待验证

新增 debug/save_anywhere.cpp/.h；menu_window.cpp 底部复选框。默认关闭，不持久化，不自动发起保存。
UI仅更新atomic请求；822E0E10 guest菜单线程应用启用位，82876EA8记录游戏原生权限，关闭时恢复原生启用位；保留原可见性和其它位。
测试入口 LO_SAVE_ANYWHERE_REQUEST 指定文本文件，内容为非零serial与0/1，例如 `1 1`；每250ms读取，新序号执行一次，与UI调用同一setter。
构建日志 out/save-anywhere-build.log = BUILD OK。新EXE已在后台独立副本运行，见下方回归。
下一步：复制新构建与camp副本save/profile到新后台目录，从槽02加载，验证营地位置、行走、物品与剧情；新构建测试OFF→ON→保存→OFF，验证关掉后离开存档点仍灰，在存档点仍可保存。需验证菜单表重置和地图切换时原生权限跟踪。
所有反编译输出仅在out/save-*.txt，未加入公开源码。

## 新构建后台回归

out/save-anywhere-01/SaveTest.exe PID24516：复制camp副本save/profile，默认关闭，Continue读最新槽02。
- shot1009：返回营地左侧开过的箱子旁；shot1756菜单保留Lv10/675G/00:46。
- shot2554：默认OFF，Save灰色。
- save-request `1 1` 后重新进入System，shot3208：Save启用。
- 正常Save到空槽03，shot5098显示00:48/675G/Lv10，实际生成第三份save.bin。
- save-request `2 0` 后退回再入System，shot5781：Save恢复灰色。
- 返回场景后摇杆24000持续90 polls，坐标由(2397.9229,214.01355,147.1508)变为(2675.8682,701.6195,147.1508)，读档后的行走可用。12000输入未产生移动，不作为失败证据。

out/save-anywhere-reload-01/SaveReload.exe PID4428：再次新进程，复制上述三个槽和profile，默认OFF的Continue读取最新槽03。shot942重新回到开箱处；status序号1核对保存位置。没有使用传送移动角色。
仅后台文件请求驱动，与UI共用SetSaveAnywhereEnabled；本轮没有打开桌面debug窗口，复选框布局及实际点击未视觉验证。
剩余：原生存档点开关后仍可保存、地图切换的权限更新、更多剧情状态/物品检查。不得用本轮局部回归宣称所有地点和所有剧情都可安全保存。
当前PID4428保留供下一步回归，input未请求、shots1、teleport1，save-request未创建。PID24516已核对路径后停止；独立副本留存。新构建SHA256：037229025402B00D12AF821AF9E4CC1A779EA75405598FABC33FB49E3CEA5BFB。

## 普通存档点与进入营地的权限回归

2026-09-05，out/save-point-regression-01/SavePointTest.exe PID35012，使用同一03722902构建与third-map-repro-01独立save/profile。
- shot967位于Gorge原生存档点，有A Save交互提示。
- shot1737默认OFF时System Save可用；ON→OFF再进入System，shot2428仍可用。
- 正常保存空槽02，shot3746确认00:20新存档完成。开关没有误禁用原生保存。
- 向右走先撞墙，shot5016仍有A Save，不能算离开存档点；随后向前走至拾取Healing Medicine位置，shot6615。
- 重新打开System，shot7332 Save灰色，证实权限会随离开存档点而更新。
- 再打开开关，普通步行触发士兵演出，shot8381演出、11150回到营地；没有传送。shot11958新菜单Save可用，关闭后重新进入，shot12606恢复灰色。
- 本轮没有重建运行时或改游戏代码，只补真实回归及专项文档索引。全部结果对应实际菜单截图，不以日志继续输出代替。

当前PID35012保留在营地System菜单，input30、shots13、save-request4=OFF；PID4428仍在营地开过的箱子旁。未提交推送。
后续转回渲染。随时存档主要后台回归已覆盖；UI实际点击/布局及后续地图流程继续纳入总体测试，不能推断游戏任意事件中途保存都安全。
