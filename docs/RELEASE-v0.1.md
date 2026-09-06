# Lost Odyssey Recompiled v0.1

First experimental Windows x64 release. Requires Windows 10/11, a Direct3D 12 GPU,
and your own supported four-disc Asian edition of Lost Odyssey. Game files are not included.

- Portable runtime with required DXC libraries and a graphical game importer.
- Import from extracted folders, default.xex, XDVDFS ISO, or Xbox 360 GOD containers.
- First-launch language and graphics setup before the game starts.
- Five interface and game text languages: English, Japanese, Korean, Traditional Chinese,
  and Simplified Chinese, based on the supported game resources.
- FXAA, resolution and window mode settings; DLSS and frame generation are placeholders.
- Built-in shader location index and parallel first-launch shader preparation. Supported
  resources avoid a full scan; unfamiliar resources retain the scan fallback.
- Includes the recent dialogue playback, shadow rendering and window responsiveness fixes.

Extract the ZIP, run **LostOdysseyRecomp.exe**, choose your initial settings, then import
your game when prompted. You can also run **InstallGame.exe** directly to import more discs.
Keep the entire package together. See the included README for compatible input details.

This is an early release, not a full-game compatibility claim. Testing has focused on early
areas and selected scenes. Some shader translations and visual effects remain incomplete;
fullscreen, mouse and mixed-DPI behavior need broader testing. Keep backups of your saves.
