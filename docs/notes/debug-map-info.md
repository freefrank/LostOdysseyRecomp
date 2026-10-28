# Debug Map ID and Localized Names (2026-09-05)

[简体中文](debug-map-info.zh-CN.md)

> **Topic reference.** The implementation and measurements below apply only to the versions and scope stated for each one. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

The F1 panel shows the numeric ID, the internal package name, and the map name in the current language. When there is no match, when several different IDs appear during loading, or when the game thread has not updated for more than two seconds, it shows "Loading or not yet identified". It does not pass off a place name from a save preview as the current map. The implementation is `debug/map_info.cpp`. It is called from the existing `82290B60` engine update hook, and the window reads only a locked string snapshot.

## Data sources

- The native definition insert at `82A0D648`: r4 is the map ID, and r5 is a row structure whose +4 field is the map package name FString. The original function always continues to run. The hook only records the ID-to-package mapping.
- The current world is at `0x83318744`. The Levels array is at +0x44/+0x48. The package name is read by following each Level's UObject Outer (+0x28), then matched against the native definitions. The FName array is at `0x833690D0`/+4, the index is at UObject+0x2C, and the text is at +16 of the name entry.
- The localized map name FString array is at `0x832C9728`/+4. It is indexed by the native ID, with 12 bytes per entry. In the running game, the IDs 1 Battlefield, 2 Hypocenter, 3 Edge of Wasteland, and 4 Gorge were also checked. Font resource row numbers 10/11/12 are not the numeric IDs of these maps, so that source of error was ruled out.
- Package name matching is lowercase. A package that does not match is not guessed to have an ID. Reads run on the game thread after the native tick returns and update every 250 ms. The guest map state is not written.

## Measurements

All runs used separate save and profile copies. The user's original saves were not changed.

- `out/map-info-current-01/window.log`: at 24.643 seconds, it read ID 4 / `u12_0_scrw` / Highlands of Wohl - Gorge. The text in the window matched the map scene.
- `out/map-info-hypocenter-01/run.log`: during the title phase, the window showed unknown. At 77.904 seconds, it read ID 2 / Highlands of Wohl - Hypocenter.
- `out/map-info-hypocenter-01/final.log`: in the final build, at 24.965 seconds, it showed ID 2 / `u11_0_scrw`. The exit POI was clicked, and the character walked forward normally. At 111.691 seconds, it updated to ID 3 / `u13_0_scrw` / Highlands of Wohl - Edge of Wasteland. The window text, usable controls, and character position were checked after the map change.
- The final build log `out/map-info-label-build.log` passed. The display label is updated only when needed, so the same string is not refreshed every frame.

The tests cover only the opening area above and the current English runtime language. They do not claim that all four discs, all languages, or all streaming-load combinations pass. The feature takes names from the game's in-game tables and does not build a separate translation table that has not been verified. For the debug window interaction and limits on automatic F1 key presses, see the [teleport notes](debug-teleport.md).
