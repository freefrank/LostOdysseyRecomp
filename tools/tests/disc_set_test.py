"""Synthetic installed-disc integration test; requires LoStorageTest, no game data."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def make_disc(root, disc, edition=3):
    directory = root / f"disc{disc}"
    directory.mkdir(exist_ok=True)
    xex = bytearray(80)
    xex[:4] = b"XEX2"
    struct.pack_into(">III", xex, 20, 1, 0x40006, 48)
    media = {3: (0x368DE6DD, 0x1888BE4E, 0x6DD59D08, 0x0C0E80B5),
             4: (0x39F7D748, 0x0EF8CEA8, 0x309E3386, 0x7B21A91D)}
    struct.pack_into(">IIII", xex, 48, media[edition][disc-1], edition, edition, 0x4D5307FA)
    xex[66:68] = bytes((disc, 4))
    (directory / "default.xex").write_bytes(xex)
    fpi = bytearray(2048)
    struct.pack_into("<H", fpi, 12, 1)
    fpi[20:22] = bytes((disc, 4))
    entries = 64+13*48
    struct.pack_into("<HHIIIII", fpi, 24, 1, 13, 13, 64, entries, entries+13*24, 0)
    names = ["LO", "xenon_chr", "xenon_event", "xenon_field", "xenon_obj", "xenon_scr",
             "xenon_sys", "xenon_vfx", "xenon_world", "xenon_battle", "xenon_loc", "xenon_mov", "xenon_snd"]
    for i, name in enumerate(names):
        struct.pack_into("<I", fpi, 64+i*48+4, entries+i*24-(64+i*48))
        struct.pack_into("<I", fpi, entries+i*24+16, 8192)
        (directory / f"{name}.fpd").write_bytes(bytes([disc*16+i])*8192)
    (directory / "LO.fpi").write_bytes(fpi)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve()
    with tempfile.TemporaryDirectory(prefix="lo-disc-test-") as temporary:
        root = Path(temporary)/"game"
        root.mkdir()
        for disc in range(1, 5):
            make_disc(root, disc)

        def run(label, rejected=True):
            result = subprocess.run([str(executable), "disc-rejected" if rejected else "discs",
                                     str(Path(temporary)/"runtime"), str(root)], capture_output=True, timeout=20)
            if result.returncode:
                raise RuntimeError(label+"\n"+result.stdout.decode(errors="replace")+result.stderr.decode(errors="replace"))
            print("PASS", label)

        run("four synthetic discs, aliases, byte reads, events and retained handles", False)
        second = root/"disc2"
        (second/"LO.fpi").unlink()
        run("missing index")
        make_disc(root, 2)
        (second/"xenon_snd.fpd").write_bytes(b"short")
        run("truncated archive")
        make_disc(root, 2, 4)
        run("mixed edition")
        make_disc(root, 2)
        data = bytearray((second/"LO.fpi").read_bytes())
        data[20] = 3
        (second/"LO.fpi").write_bytes(data)
        run("wrong index disc")
        make_disc(root, 2)
        data = bytearray((second/"LO.fpi").read_bytes())
        struct.pack_into("<I", data, 68, 0xFFFFFFFF)
        (second/"LO.fpi").write_bytes(data)
        run("out-of-bounds index table")
        make_disc(root, 2)
        (second/"default.xex").write_bytes(b"XEX2")
        run("truncated execution metadata")


if __name__ == "__main__":
    main()
