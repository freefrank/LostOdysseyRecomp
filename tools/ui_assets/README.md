# UI asset PNG export

`export.py` inventories `LO.fpi` from every existing `disc*` directory. It
selects package paths for localized menus, fonts, tutorials, title, credits,
world map, HUD and system UI,
then decodes their `Texture2D` exports and checks `Font` page references. It
does not scan all scene textures: character and prop packages are excluded.
`decode.cpp` extends the existing `out/issue40/dump_ui.cpp` probe using the
production menu-asset package, tiled BC3, LZO and CPX parsing conventions; it
also decodes tiled BC1 with its explicit transparency mode and A8R8G8B8
format 2 (including Japanese font pages). The
decoder emits RGBA, and Pillow writes unmodified-alpha PNGs and gray/checker
contact sheets. Other formats or unsupported package layouts appear as
explicit failed rows in the manifests; this is a bounded UI selection, not a
claim that every game texture can be decoded.

From the repository root on Windows with `clang-cl`, Ninja, CMake and Python
with Pillow installed:

```bat
call tools\setup_windows.bat
cmake -S tools\ui_assets -B out\issue40\ui-export\build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl
cmake --build out\issue40\ui-export\build --target LoUiAssetDecoder
python -B tools\ui_assets\export.py --private LostOdysseyRecompLib\private --output out\issue40\ui-export
```

`--inventory-only` writes `inventory.json` without compiling or decoding.
`--decoder PATH` specifies a separately built decoder. Use a *new* output
directory for a second complete export; the script refuses to replace an
existing manifest or image set. It reads FPI/FPD files only, and records their
size and modification times before and after extraction. No game process,
driver, save data, or network access is involved. Do not add exported assets
or manifests to Git.

Open `out/issue40/ui-export/sheets/INDEX.txt` to find the grouped, numbered
contact sheets. Labels carry the manifest ID, package stem and object name.
Original full-size transparent PNGs reside under `images/disc/package/object`.
`manifest.json` and `manifest.csv` map **every** selected source occurrence to
its archive offset/length, language, export offset/length, format, size, image
path or failure. Identical package bytes across discs share an image path while
retaining separate source mappings. Font entries list their page-export indices;
the referenced pages have their own PNG rows with `owner` set to the font.
