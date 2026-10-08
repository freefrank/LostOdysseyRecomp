# Recipe tours: diagnostic patches and corpus builder

The tours record pipeline recipes for the shipped corpus. Driver: [`tools/recipe_tour.py`](../recipe_tour.py); method and results: [`docs/notes/pipeline-first-use-stalls.md`](../../docs/notes/pipeline-first-use-stalls.md), section 语料巡游.

## Patches (never merged)

`recipe_tour.py maps` and `battles --disc N` need a diagnostic build with three command files that main does not have. `diag-tour.patch` adds them as `LostOdysseyRecomp/debug/diag_tour.cpp` and calls it from the engine tick in `debug/teleport.cpp`. Each command runs once when its serial changes:

- `LO_DIAG_MAPJUMP_COMMAND_FILE`: `<serial> <map package>` jumps to a map through the original RequestMapJump.
- `LO_DIAG_DISC_COMMAND_FILE`: `<serial> <1-4>` asks the disc manager for that disc.
- `LO_DIAG_PROBE_COMMAND_FILE`: `<serial> <package>...` looks each package up in the package cache and logs `diag probe package=<name> found=0|1`.

`mac-tour.patch` is the same `diag_tour.cpp` plus the battle tour hook (`debug/battle_tour.*` and the walking-encounter changes in `no_encounters.cpp`) as it was before that part merged in #263. Only the diag half is still missing from main, so `diag-tour.patch` is the one to use.

Apply in a worktree, never in the main checkout; the runtime's source glob picks up the new file. To undo, restore `teleport.cpp`, delete `diag_tour.cpp` and the `.rej` file, or remove the worktree.

```sh
git apply --reject tools/tours/diag-tour.patch
```

Status against main 16a83cb8 (2026-10-07): neither patch passes `git apply --check`. `diag-tour.patch` creates `diag_tour.cpp` and adds the declaration, but its last hunk is rejected because the engine tick line in `teleport.cpp` gained `PollSceneLoads`: append `DiagTourTick(ctx, base);` to that line by hand and delete the `.rej` file. `mac-tour.patch` fails because `battle_tour.cpp` and `.h` exist now. The patched sources were not compiled against current main.

## Corpus builder

`build_corpus.py` merges the tour outputs into `pipelines_corpus.bin` (recipe file v2 with scene tags) using [`tools/pipeline_recipes.py`](../pipeline_recipes.py):

```sh
python tools/tours/build_corpus.py --out DIR --map SCENES... --cutscene FILE... --battle SCENES... --stage SCENES...
```

`SCENES` is a folder of per-scene recipe files from one tour run: `map-<id>.bin` and `battle-<id>.bin` get that scene tag, `untagged.bin` none. `--stage` drops the battle tags (the stage tour fought one formation on many battle stages). `--cutscene` files carry no tags. Every group is optional. The folders came from a `recipe_tour.py scenes` step that is not in main. Inputs are only read; `DIR` receives `group-<name>.bin` and `pipelines_corpus.bin`, and the script prints the corpus counts and SHA-256.
