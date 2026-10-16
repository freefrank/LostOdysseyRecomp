# 在 Blender 中打开模型

[English](Open-Models-in-Blender.md)

导出工具把模型和动画写成 glTF `.glb` 文件。免费的 3D 软件 Blender 可以显示模型的颜色、播放动画。以下步骤用 Blender 5.2 LTS 核对过。

双击 `.glb` 文件不会用 Blender 打开它；Blender 的 **File > Open**（中文界面：**文件 > 打开**）只能打开 Blender 自己的 `.blend` 文件。请按下面的步骤导入。

Blender 在中文 Windows 上可能显示中文界面。下文先写英文菜单名，括号里是中文界面的名字。

## 1. 安装 Blender（只需一次）

1. 打开 <https://www.blender.org/download/>，点 **Download Blender**，下载的是 Windows 安装包（`.msi`）。
2. 运行安装包，一直点 **Next**，最后点 **Finish**。
3. 从开始菜单启动 **Blender**。中间会弹出欢迎画面，在它外面任意处点一下即可关闭。

这时能看到网格上的一个灰色立方体。中间的大区域是 **3D Viewport**（3D 视图），右上角的列表是 **Outliner**（大纲视图）。

## 2. 拿到 .glb 文件

1. 在 MO2 里打开 **Tools > Tool Plugins > Export Lost Odyssey assets**。表格是空的话，先点 **Scan game data**。
2. 勾选 **Models**（以及 **With textures**）或 **Animations**，再点一行，例如 `Kaim (pc_000a0_m)`。右侧会显示它的预览图。
3. 点 **Export and show the file**。资源管理器会打开，并选中这个 `.glb` 文件。

用 **Start** 或 **Export selected** 导出的文件在输出文件夹的 `models\` 和 `animations\` 里。`.glb` 要和旁边的 `textures` 文件夹放在一起。

## 3. 导入

1. 删除立方体：把鼠标移到 3D 视图上，按键盘上的 **Delete**，立方体消失。
2. 从资源管理器把 `.glb` 文件拖到 3D 视图上松开，稍等模型就会出现；动画文件要几秒。
   - 拖进去没有反应时，用菜单：**File > Import > glTF 2.0 (.glb/.gltf)**（**文件 > 导入 > glTF 2.0 (.glb/.gltf)**），选中文件，点 **Import glTF 2.0**（**导入 glTF 2.0**）。
3. 把鼠标移到 3D 视图上按 **Home**，视图会对准模型。小键盘的 **.** 键只对准选中的模型。
4. 显示颜色：3D 视图右上角有四个小球按钮，点第三个 **Material Preview**（**材质预览**）。也可以按 **Z**，再选 **Material Preview**。

查看方法：按住鼠标中键（滚轮）拖动可以旋转视角，滚动滚轮缩放，按住 **Shift** 加中键拖动可以平移。

导入后会看到：

- 人物是 T 字姿势（双臂平伸），身高约 1.8 米，游戏里就是这样存的。
- 大纲视图里有 `Armature`（骨架）、模型本身（例如 `pc_000a0_m`），以及 `glTF_not_exported` 集合里的 `Icosphere`（中文界面可能叫“棱角球”）。Blender 用它来画骨骼，默认隐藏，不用管它。
- 地图部件（例如 `StaticMesh_705`）没有骨架。它放在地图里原来的位置，可能离网格中心很远，按 **Home** 就能找到。

## 4. 播放动画

动画文件（来自 `animations\`，例如 `pc_000a0_f.0.glb`）包含人物和它的全部动画。凯姆的 `pc_000a0_f` 有 62 个。

1. 按第 3 步导入。
2. 鼠标放在 3D 视图上按 **Space**（空格），人物开始播放一个动画。大多数动画很短，播完后人物会停住，直到播放到第 250 帧再从头开始。再按一次 **Space** 停止。
3. 换一个动画：
   1. 在大纲视图里点 `Armature`。
   2. 点窗口顶部的 **Animation**（**动画**）标签。
   3. 下方的编辑器是 **Dope Sheet**（**动画摄影表**）。在它的标题栏里打开写着 **Dope Sheet** 的菜单，选 **Action Editor**（**动作编辑器**）。
   4. 在同一个标题栏里打开动画列表（显示当前动画名，例如 `fx07koro01_n_st`），点另一个名字。
   5. 鼠标放回 3D 视图上，按 **Space**。

动画文件不带颜色，在材质预览里也是灰色。想看有颜色的人物，请导入同一个人物的模型（`..._m`，导出时勾选 **With textures**）。

## 常见问题

| 看到的情况 | 解决办法 |
| --- | --- |
| 双击 `.glb` 打开了别的程序，或者没反应 | Windows 没有给 `.glb` 设置打开程序。先启动 Blender，再把文件拖进去，或用 **File > Import > glTF 2.0 (.glb/.gltf)**。 |
| **File > Open** 里看不到 `.glb` 文件 | **File > Open** 只能打开 `.blend` 文件。请用 **File > Import > glTF 2.0 (.glb/.gltf)**。 |
| 拖进去没有反应 | 拖到 3D 视图或大纲视图上，不要拖到别的区域；或者用 **File > Import**。 |
| 模型是灰色或白色 | 切换到 **Material Preview**（第 3 步第 4 项）。还是灰色：导出时没勾 **With textures**，或者这是动画文件。勾选 **Models** 和 **With textures** 重新导出模型。 |
| 模型是粉红色 | Blender 没找到贴图。请从导出工具写入的位置导入 `.glb`，让它旁边有 `textures` 文件夹。 |
| 视图里什么都没有 | 鼠标放在 3D 视图上按 **Home**。 |
| 模型看起来特别小或特别大 | Blender 用米做单位：人约 1.8 米，建筑可能有 30 米。按 **Home** 对准，滚动滚轮缩放。 |
| 人物不动 | 鼠标放在 3D 视图上按 **Space**。只有 `animations\` 里的文件带动画。如果选动画时选中的是模型而不是骨架，先在大纲视图点 `Armature`，再重新选动画。 |
| 大纲视图里有 `Icosphere` | 它用来画骨骼，不用管。 |
