# Dual-screen companion (Android)

On Android handhelds with a second screen, such as the AYN Thor, the second screen shows a live, touchable panel next to the game, in the spirit of the [Eden Duo companions](https://github.com/igawa6/eden-duo-companions).

| Tab | What it shows |
| :--- | :--- |
| **Party** | The five party members with HP, MP, EXP progress and row. Tap one for weapon, ring, accessories (with free slots) and the number of learned skills. Reserve members are listed beside them. |
| **Map** | A radar with the player in the centre and the current map's points of interest (save points, exits, mechanisms, items, entrances), a list sorted by distance, zoom buttons and pinch. With the optional debug setting, **Go here** teleports through the existing debug teleport. |
| **Bag** | Items and materials with their counts. |
| **Pad** | Touch buttons held while pressed: D-pad, A/B/X/Y, LB/RB/LT/RT, Start, Back, a large **Ring (RT)** button and LB+RB for the Debug Menu. The panel switches to this tab when a battle starts and back when it ends. |

The header shows the map name, the state (exploring, battle, menu, scene, title), the disc and gold. **☾** turns the panel black until it is tapped; **⚙** has language (English/Spanish), vibration, the battle switch, teleport and **Turn panel off**. **CTRL → Second-screen panel** turns the panel on or off.

## Game look from the player's files

The panel can use the game's own menu art: wide character portraits, brushed-steel windows, the white cursor, item icons and controller glyphs. The APK ships no game art. `tools/companion_theme.py` crops these pieces from the UI textures that `tools/ui_assets/export.py` decodes from the player's own `disc1`:

```sh
clang++ -std=c++20 -O2 -ILostOdysseyRecomp -Ithirdparty/lzokay tools/ui_assets/decode.cpp thirdparty/lzokay/lzokay.cpp -o LoUiAssetDecoder
python tools/ui_assets/export.py --private <folder with disc1> --output ui-export --decoder LoUiAssetDecoder
python tools/companion_theme.py --export ui-export --output companion-theme --language int
```

Only `LO.fpi`, `xenon_sys.fpd`, `xenon_loc.fpd` and `xenon_vfx.fpd` from `disc1` are read. Copy `companion-theme` to `Android/data/io.github.freefrank.lostodyssey/files/`, or to `LostOdyssey/companion-theme` on the shared storage (some devices do not let the app read files copied into `Android/data` over USB). Without a theme the panel uses the same layout drawn with gradients. **⚙** says whether the theme was loaded.

## Limits

- HP and MP are the out-of-battle values the Cheats page already uses. During a battle the panel shows the last field values and says so.
- The radar uses the engine's X/Y axes; there is no camera heading or map drawing.
- Item, weapon, ring and accessory names come from the English cheat catalog.

## How it works

| Part | Files |
| :--- | :--- |
| Samples the state on the GEngine tick every 200 ms into an owned JSON snapshot; nothing else touches guest memory | `LostOdysseyRecomp/companion/companion.{h,cpp}`, called from `debug/teleport.cpp` (Android only) |
| Read-only inventory counts and learned skills in the cheats snapshot | `LostOdysseyRecomp/debug/cheats.h` |
| Panel buttons as a second touch source merged with the on-screen controls | `LostOdysseyRecomp/hid/android_touch.{h,cpp}` |
| An Android `Presentation` with a WebView on the second display. Its window is not focusable, so controllers keep driving the game | `packaging/android/runtime/src/main/java/.../DualScreenCompanion.java` |
| Start/stop with the activity; CTRL switch | `RuntimeActivity.java`, `TouchControlsView.java`, `res/values*/strings.xml` |
| Panel page | `packaging/android/runtime/src/main/assets/companion/` |

The display is chosen as Eden Duo does: the union of presentation displays and all displays, without the main one, physical panels first. The page and the theme share one origin (`https://appassets.androidplatform.net/companion/`) so `fetch` and `localStorage` work.

To preview the page on a PC with sample data, serve `packaging/android/runtime/src/main/assets/companion/` (for example `python -m http.server`) and open `index.html` at 1240×1080; add `?mode=battle`, and `&theme=<URL of a companion-theme folder served with CORS>`.
