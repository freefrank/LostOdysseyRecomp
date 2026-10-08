#!/usr/bin/env python3
"""Build the scene-tagged pipeline corpus (recipe file v2) from recipe tour outputs.

  python tools/tours/build_corpus.py --out DIR [--map DIR...] [--cutscene FILE...]
                                     [--battle DIR...] [--stage DIR...]

Merges with tools/pipeline_recipes.py. Each group is merged into DIR/group-<name>.bin,
the groups into DIR/pipelines_corpus.bin, which also drops the rect-list recipes
that no scene (or more than eight) asks for. Inputs are never written.

  --map, --battle  folders of per-scene recipe files from the map and battle tours:
                   map-ID.bin and battle-ID.bin get that scene tag, untagged.bin none
  --stage          the same for the stage tour, which fought one formation on many
                   battle stages: its battle-ID files stay untagged
  --cutscene       recipe files without scene tags (cutscene tours)

Main has no command that splits a tour run's recipe file into the per-scene
files; they came from an unmerged `recipe_tour.py scenes` step.
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pipeline_recipes as pr  # noqa: E402


def specs(folder, untag_battles=False):
    out = []
    for f in sorted(folder.glob("*.bin")):
        kind, _, ident = f.stem.partition("-")
        if f.stem == "untagged" or (kind == "battle" and untag_battles):
            out.append(str(f))
        else:
            out.append(f"{f}@{kind}:{ident}")
    return out


def stats(path):
    _, recipes = pr.load(str(path))
    keys = list(recipes)
    untagged = sum(1 for s in recipes.values() if not s)
    many = sum(1 for s in recipes.values() if s == [pr.MANY])
    maps = {t for s in recipes.values() for t in s if t >> 28 == 1}
    battles = {t for s in recipes.values() for t in s if t >> 28 == 2}
    return recipes, (f"{len(keys)} recipes, {len({k[:8] for k in keys})} VS, {len({k[8:16] for k in keys})} PS, "
                     f"{untagged} untagged, {many} in >8 scenes, {len(maps)} maps, {len(battles)} battles")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", type=Path, required=True, help="folder for group-*.bin and pipelines_corpus.bin")
    for name in ("map", "battle", "stage"):
        ap.add_argument(f"--{name}", action="extend", nargs="+", default=[], type=Path, metavar="DIR",
                        help=f"{name} tour per-scene folders")
    ap.add_argument("--cutscene", action="extend", nargs="+", default=[], type=Path, metavar="FILE",
                    help="cutscene tour recipe files")
    args = ap.parse_args()
    for folder in args.map + args.battle + args.stage:
        if not folder.is_dir() or not any(folder.glob("*.bin")):
            ap.error(f"{folder}: not a folder with per-scene .bin files")
    for f in args.cutscene:
        if not f.is_file():
            ap.error(f"{f}: no such recipe file")

    groups = {
        "map": [s for d in args.map for s in specs(d)],
        "cutscene": [str(f) for f in args.cutscene],
        "battle": [s for d in args.battle for s in specs(d)],
        "stage": [s for d in args.stage for s in specs(d, untag_battles=True)],
    }
    groups = {name: inputs for name, inputs in groups.items() if inputs}
    if not groups:
        ap.error("no inputs: give at least one of --map, --cutscene, --battle, --stage")
    args.out.mkdir(parents=True, exist_ok=True)

    seen = set()
    for name, inputs in groups.items():
        out = args.out / f"group-{name}.bin"
        pr.main(["x", "merge", str(out)] + inputs)
        recipes, line = stats(out)
        new = [k for k in recipes if k not in seen]
        seen.update(recipes)
        print(f"{name}: {line}; {len(new)} not in the groups above")
    corpus = args.out / "pipelines_corpus.bin"
    pr.main(["x", "merge", str(corpus)] + [str(args.out / f"group-{n}.bin") for n in groups])
    # Rect-list recipes need the rect-list VS variant, which the renderer makes at
    # the first such draw; startup preparation counts the untagged/"many" ones as
    # missing shaders and never builds them, so they are dropped.
    version, recipes = pr.load(str(corpus))
    keep = {k: s for k, s in recipes.items()
            if not (struct.unpack_from("<12I", k, 16)[4] == 8 and (not s or s == [pr.MANY]))}
    print(f"dropped {len(recipes) - len(keep)} startup rect-list recipes")
    pr.write(str(corpus), version, keep)
    print("corpus:", stats(corpus)[1])
    data = corpus.read_bytes()
    print("sha256", hashlib.sha256(data).hexdigest(), "size", len(data))
    pr.info([str(corpus)])


if __name__ == "__main__":
    main()
