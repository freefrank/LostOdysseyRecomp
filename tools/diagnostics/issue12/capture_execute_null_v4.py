"""Issue 12 capture, v4 = v3 + component-side evidence at the fault:
  * the owning SkeletalMeshComponent (FPrimitiveSceneInfo+0xC): its Materials
    TArray (+624/+628), attach/reattach flag word (+80), SkeletalMesh (+640),
    and every Materials entry resolved through FName;
  * the SkeletalMesh asset's Materials (+88/+92);
  * the 10-slot pre/post garbage-collection callback tables (0x83302B08 /
    0x83302AE0) and GC globals, to show whether any pre-GC callback could flush
    the rendering thread before the purge.
Read-only; same protocol as v1..v3."""
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_execute_null as base  # noqa: E402
import capture_execute_null_v3 as v3  # noqa: E402

PRE_GC_TABLE, POST_GC_TABLE = 0x83302B08, 0x83302AE0
GC_GLOBALS = {"GIsGarbageCollecting_0x83313650": 0x83313650, "GObjFirstGCIndex_0x83315F48": 0x83315F48,
              "purge_pending_0x83315FA0": 0x83315FA0, "purge_flag_0x83315F98": 0x83315F98,
              "GObjLastNonGCIndex_0x832383C4": 0x832383C4, "GEngine_0x83315FB4": 0x83315FB4}


class ProbeV4(v3.ProbeV3):
    def materials_array(self, data, num, label):
        out = {"label": label, "data": f"0x{data or 0:08X}", "num": num, "entries": []}
        for i in range(min(num or 0, 32)):
            ptr = self.g32(data + i * 4)
            entry = {"index": i, "ptr": f"0x{ptr or 0:08X}"}
            if ptr:
                first = self.g32(ptr)
                entry["first_word"] = f"0x{first:08X}" if first is not None else None
                entry["object"] = self.object_info(ptr, "material")
            out["entries"].append(entry)
        return out

    def inspect_target(self, tid, exception):
        matched = super().inspect_target(tid, exception)
        if not matched:
            return matched
        capture = json.loads((self.out / "capture.json").read_text(encoding="utf-8"))
        v4 = {}
        try:
            guest = {k: int(v, 16) for k, v in capture["guest"].items()}
            proxy = guest["r31"] & 0xFFFFFFFF
            scene_info = self.g32(proxy + 0x10)
            component = self.g32(scene_info + 0x0C) if scene_info else None
            v4["component"] = f"0x{component or 0:08X}"
            if component:
                v4["component_object"] = self.object_info(component, "SkeletalMeshComponent")
                flags80 = self.g32(component + 80)
                v4["component_word_0x50"] = f"0x{flags80 or 0:08X}"
                v4["component_attached_bit31"] = bool(flags80 and flags80 & 0x80000000)
                v4["component_deferred_reattach_bit29"] = bool(flags80 and flags80 & 0x20000000)
                v4["component_scene_0x48"] = f"0x{self.g32(component + 72) or 0:08X}"
                v4["component_owner_0x4C"] = f"0x{self.g32(component + 76) or 0:08X}"
                v4["component_words_0x40_0x60"] = [f"{w:08X}" for w in struct.unpack(">8I", self.gbytes(component + 0x40, 0x20))]
                mats_data, mats_num = self.g32(component + 624), self.g32(component + 628)
                v4["component_materials"] = self.materials_array(mats_data, mats_num, "component Materials (+624)")
                mesh = self.g32(component + 640)
                v4["component_skeletal_mesh"] = f"0x{mesh or 0:08X}"
                if mesh:
                    v4["mesh_materials"] = self.materials_array(self.g32(mesh + 88), self.g32(mesh + 92), "SkeletalMesh Materials (+88)")
                v4["component_mesh_object_0x2BC"] = f"0x{self.g32(component + 700) or 0:08X}"
                v4["component_pending_mesh_0x2A8"] = f"0x{self.g32(component + 680) or 0:08X}"
                owner = self.g32(component + 76)
                if owner:
                    v4["owner_words_0x40_0x60"] = [f"{w:08X}" for w in struct.unpack(">8I", self.gbytes(owner + 0x40, 0x20))]
            v4["pre_gc_callbacks"] = [f"{w:08X}" for w in struct.unpack(">10I", self.gbytes(PRE_GC_TABLE, 40))]
            v4["post_gc_callbacks"] = [f"{w:08X}" for w in struct.unpack(">10I", self.gbytes(POST_GC_TABLE, 40))]
            v4["gc_globals"] = {k: f"0x{self.g32(a) or 0:08X}" for k, a in GC_GLOBALS.items()}
            # Deeper game-thread stack for the main thread (largest frames first).
            for t in capture.get("v2", {}).get("threads", []):
                if t.get("guest") and t.get("guest_backtrace") and "8249A758" in t["guest_backtrace"]:
                    sp = int(t["guest"]["r1"], 16)
                    frames = []
                    for _ in range(40):
                        prev = self.g32(sp)
                        if prev is None or prev <= sp or prev - sp > 0x200000 or prev & 7:
                            break
                        saved = self.g32(prev - 8)
                        frames.append({"sp": f"{sp:08X}", "frame_size": prev - sp, "saved_lr": f"{saved:08X}" if saved is not None else None})
                        sp = prev
                    v4["game_thread_frames"] = frames
        except BaseException as error:
            v4["extension_error"] = repr(error)
        capture["v4"] = v4
        base.dump_json(self.out / "capture.json", capture)
        self.log("v4_extension_written")
        return matched


def main():
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--check-layout", action="store_true")
    p.add_argument("--session", type=Path)
    p.add_argument("--pid", type=int)
    p.add_argument("--output", type=Path)
    p.add_argument("--timeout-seconds", type=int, default=600)
    args = p.parse_args()
    if args.check_layout:
        print(json.dumps(base.layout_check(), indent=2))
        return 0
    if args.session is None or args.pid is None or args.output is None:
        p.error("--session, --pid, and a fresh --output directory are required")
    return ProbeV4(args).run()


if __name__ == "__main__":
    raise SystemExit(main())
