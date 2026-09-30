"""Generate PPC sources. Build XenonRecomp first; requires Python 3.11+."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import tomllib


def layout(root):
    config = root / "LostOdysseyRecompLib/config/LostOdysseyRecomp.toml"
    main = tomllib.loads(config.read_text(encoding="utf-8"))["main"]
    output = (config.parent / main["out_directory_path"]).resolve()
    if output != root / "LostOdysseyRecompLib/ppc":
        raise ValueError("Unexpected PPC output directory")
    return config, output


def generated_files(output):
    return [p for p in output.iterdir() if p.is_file() and p.suffix in {".cpp", ".h"}]


def require_outputs(output):
    if not any(output.glob("ppc_recomp.*.cpp")):
        raise ValueError("Missing generated PPC sources")
    for name in ("ppc_func_mapping.cpp", "ppc_config.h", "ppc_context.h", "ppc_recomp_shared.h"):
        if not (output / name).is_file():
            raise ValueError(f"Missing generated file: {name}")


def generate(root, executable):
    config, output = layout(root)
    output.mkdir(parents=True, exist_ok=True)
    # XenonRecomp can return zero without producing sources. Keep the previous
    # generated set until a new set has been produced successfully.
    with tempfile.TemporaryDirectory(prefix="ppc-codegen-", dir=output.parent) as temporary:
        backup = Path(temporary)
        moved = []
        try:
            for path in generated_files(output):
                path.rename(backup / path.name)
                moved.append(path.name)
        except BaseException:
            for name in moved:
                (backup / name).rename(output / name)
            raise
        try:
            subprocess.run([str(executable), str(config),
                            str(root / "tools/XenonRecomp/XenonUtils/ppc_context.h")],
                           cwd=root, check=True)
            require_outputs(output)
        except BaseException:
            for path in generated_files(output):
                path.unlink()
            for name in moved:
                (backup / name).rename(output / name)
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["generate"])
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--executable", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    executable = args.executable or root / "out/build/tools/XenonRecomp/XenonRecomp" / (
        "XenonRecomp.exe" if sys.platform == "win32" else "XenonRecomp")
    try:
        generate(root, executable.resolve())
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"PPC generation failed: {error}", file=sys.stderr)
        return 1
    print("PPC generation: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
