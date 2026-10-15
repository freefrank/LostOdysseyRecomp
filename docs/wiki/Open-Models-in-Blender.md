# Open models in Blender

[简体中文](Open-Models-in-Blender.zh-CN.md)

The export tool writes models and animations as glTF `.glb` files. Blender, a free 3D program, shows them with their colours and plays the animations. These steps were checked with Blender 5.2 LTS.

Double-clicking a `.glb` file does not open it in Blender, and Blender's **File > Open** only opens Blender's own `.blend` files. Import the file as below.

## 1. Install Blender (once)

1. Open <https://www.blender.org/download/> and press **Download Blender**. You get the Windows installer (`.msi`).
2. Run the installer and press **Next** until **Finish**.
3. Start **Blender** from the Start menu. A welcome box shows in the middle; click anywhere outside it to close it.

You now see a grey cube on a grid. The big middle area is the **3D Viewport**. The list at the top right is the **Outliner**.

## 2. Get a .glb file

1. In MO2, open **Tools > Tool Plugins > Export Lost Odyssey assets**. If the table is empty, press **Scan game data**.
2. Tick **Models** (and **With textures**) or **Animations**, then click a row, for example `Kaim (pc_000a0_m)`. The right side shows a picture of it.
3. Press **Export and show the file**. Explorer opens with the `.glb` file selected.

Files from **Start** or **Export selected** are in the output folder, under `models\` and `animations\`. Keep each `.glb` next to its `textures` folder.

## 3. Import it

1. Delete the cube: move the mouse over the 3D Viewport and press **Delete** on the keyboard. The cube disappears.
2. Drag the `.glb` file from Explorer onto the 3D Viewport and let go. The model appears after a moment; animation files take a few seconds.
   - If nothing happens, use the menu: **File > Import > glTF 2.0 (.glb/.gltf)**, pick the file and press **Import glTF 2.0**.
3. Move the mouse over the 3D Viewport and press **Home**. The view fits the model. The **.** key on the number pad fits the selected model only.
4. Show the colours: at the top right of the 3D Viewport there are four small sphere buttons. Click the third one, **Material Preview**. You can also press **Z** and pick **Material Preview**.

To look around: hold the middle mouse button (the wheel) and move the mouse to turn the view, turn the wheel to zoom, hold **Shift** and the middle mouse button to move the view.

What you see:

- A character stands in a T-pose (arms out), about 1.8 m tall. That is how the game stores it.
- The Outliner lists `Armature` (the skeleton), the model (for example `pc_000a0_m`) and an `Icosphere` in a `glTF_not_exported` collection. Blender uses the Icosphere to draw the bones and hides it; ignore it.
- A map piece (for example `StaticMesh_705`) has no skeleton. It sits where it stands in the map, so it can be far from the grid's centre: press **Home** to find it.

## 4. Play an animation

Animation files (from `animations\`, for example `pc_000a0_f.0.glb`) hold the character and all its animations. Kaim's `pc_000a0_f` has 62.

1. Import the file as in step 3.
2. Move the mouse over the 3D Viewport and press **Space**. The character plays one animation. Most are short: the character then stands still until playback starts again at frame 250. Press **Space** again to stop.
3. To pick another animation:
   1. In the Outliner, click `Armature`.
   2. At the top of the window, click the **Animation** tab.
   3. The editor at the bottom is the **Dope Sheet**. In its header, open the menu that says **Dope Sheet** and choose **Action Editor**.
   4. In the same header, open the animation list (it shows the current name, for example `fx07koro01_n_st`) and click another name.
   5. Move the mouse over the 3D Viewport and press **Space**.

Animation files have no colours, even in Material Preview. For a coloured character, import the same character's model (`..._m`, exported with **With textures**).

## Common problems

| What you see | What to do |
| --- | --- |
| Double-clicking the `.glb` opens another program or nothing | Windows has no program set for `.glb`. Start Blender first, then drag the file in or use **File > Import > glTF 2.0 (.glb/.gltf)**. |
| **File > Open** shows no `.glb` files | **File > Open** is for `.blend` files only. Use **File > Import > glTF 2.0 (.glb/.gltf)**. |
| Dropping the file does nothing | Drop it on the 3D Viewport or the Outliner, not on the other areas. Or use **File > Import**. |
| The model is grey or white | Switch to **Material Preview** (step 3.4). Still grey: the model was exported without **With textures**, or it is an animation file. Export the model again with **Models** and **With textures** ticked. |
| The model is pink | Blender did not find the textures. Import the `.glb` from where the tool wrote it, next to its `textures` folder. |
| The view is empty | Press **Home** with the mouse over the 3D Viewport. |
| The model looks tiny or huge | Blender uses metres: a person is about 1.8 m, a building can be 30 m. Press **Home** to fit it, turn the wheel to zoom. |
| The character does not move | Press **Space** with the mouse over the 3D Viewport. Only files from `animations\` have animations. If you clicked a name in the Action Editor while the model was selected, click `Armature` in the Outliner and pick the animation again. |
| An `Icosphere` is in the Outliner | It draws the bones. Ignore it. |
