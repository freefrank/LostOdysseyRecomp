"""Issue 12 execute-null capture, v2: same read-only debugger protocol as
capture_execute_null.py, plus (at the matching fault, while the process is
frozen by the debug event):
  * every thread's host context, its PPCContext (located on the host stack) and
    a guest back-chain walk (saved LR at [caller_sp - 8]);
  * FName resolution through FName::Names (guest 0x833690D0) for the freed
    object, its Outer/Class, the proxy's Owner actor and SkeletalMesh;
  * GObjObjects Num / GIsThreadedRendering / render ring state.
No guest memory, register or code is written. Detach protocol unchanged.
"""
import ctypes as C
from ctypes import wintypes as W
import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_execute_null as base  # noqa: E402

BASE = base.BASE
NAMES = 0x833690D0
GOBJ = 0x833690F4
GIS_THREADED = 0x83318040
RING = 0x8336A7A4
CODE_LO, CODE_HI = 0x82000000, 0x833C0000
TH32CS_SNAPTHREAD = 4


class THREADENTRY32(C.Structure):
    _fields_ = [("dwSize", W.DWORD), ("cntUsage", W.DWORD), ("th32ThreadID", W.DWORD),
                ("th32OwnerProcessID", W.DWORD), ("tpBasePri", C.c_long),
                ("tpDeltaPri", C.c_long), ("dwFlags", W.DWORD)]


class THREAD_BASIC_INFORMATION(C.Structure):
    _fields_ = [("ExitStatus", C.c_long), ("TebBaseAddress", C.c_void_p),
                ("ClientIdProcess", C.c_void_p), ("ClientIdThread", C.c_void_p),
                ("AffinityMask", C.c_size_t), ("Priority", C.c_long), ("BasePriority", C.c_long)]


class ProbeV2(base.Probe):
    def __init__(self, args):
        super().__init__(args)
        k = self.k
        k.CreateToolhelp32Snapshot.argtypes, k.CreateToolhelp32Snapshot.restype = [W.DWORD, W.DWORD], W.HANDLE
        k.Thread32First.argtypes = k.Thread32Next.argtypes = [W.HANDLE, C.POINTER(THREADENTRY32)]
        k.Thread32First.restype = k.Thread32Next.restype = W.BOOL
        self.nt = C.WinDLL("ntdll")
        self.nt.NtQueryInformationThread.argtypes = [W.HANDLE, C.c_int, C.c_void_p, W.ULONG, C.POINTER(W.ULONG)]
        self.nt.NtQueryInformationThread.restype = C.c_long
        self.name_cache = {}

    # ---- guest helpers -------------------------------------------------
    def g32(self, address):
        item, raw = self.read(BASE + (address & 0xFFFFFFFF), 4)
        return struct.unpack(">I", raw)[0] if len(raw) == 4 else None

    def g64(self, address):
        item, raw = self.read(BASE + (address & 0xFFFFFFFF), 8)
        return struct.unpack(">Q", raw)[0] if len(raw) == 8 else None

    def gbytes(self, address, size):
        item, raw = self.read(BASE + (address & 0xFFFFFFFF), size)
        return raw

    def name(self, index, number):
        key = (index, number)
        if key in self.name_cache:
            return self.name_cache[key]
        result = f"<name {index}_{number}>"
        data = self.g32(NAMES)
        num = self.g32(NAMES + 4)
        if data and num is not None and 0 <= index < num:
            entry = self.g32(data + index * 4)
            if entry:
                raw = self.gbytes(entry + 0x10, 256)
                chars = []
                for i in range(0, len(raw) - 1, 2):
                    hi, lo = raw[i], raw[i + 1]
                    if hi == 0 and lo == 0:
                        break
                    chars.append(chr(lo) if hi == 0 and 32 <= lo < 127 else "?")
                text = "".join(chars)
                result = f"{text}_{number - 1}" if number else text
        self.name_cache[key] = result
        return result

    def object_info(self, address, label):
        """UObject: +4 Index, +8 ObjectFlags(64), +28 Outer, +2C Name, +34 Class, +38 Archetype."""
        info = {"label": label, "address": f"0x{address:08X}"}
        if not address or address & 3:
            info["error"] = "null or unaligned"
            return info
        raw = self.gbytes(address, 0x40)
        if len(raw) < 0x40:
            info["error"] = "unreadable"
            return info
        words = struct.unpack(">16I", raw)
        info["first_words"] = [f"{w:08X}" for w in words[:4]]
        info["index"] = words[1]
        info["flags"] = f"0x{struct.unpack_from('>Q', raw, 8)[0]:016X}"
        info["outer"] = f"0x{words[10]:08X}"
        info["name"] = self.name(words[11], words[12])
        cls = words[13]
        info["class"] = f"0x{cls:08X}"
        if cls and not cls & 3:
            craw = self.gbytes(cls, 0x40)
            if len(craw) == 0x40:
                cw = struct.unpack(">16I", craw)
                info["class_name"] = self.name(cw[11], cw[12])
        outer = words[10]
        if outer and not outer & 3:
            oraw = self.gbytes(outer, 0x40)
            if len(oraw) == 0x40:
                ow = struct.unpack(">16I", oraw)
                info["outer_name"] = self.name(ow[11], ow[12])
                info["outer_flags"] = f"0x{struct.unpack_from('>Q', oraw, 8)[0]:016X}"
                ocls = ow[13]
                if ocls and not ocls & 3:
                    ocraw = self.gbytes(ocls, 0x40)
                    if len(ocraw) == 0x40:
                        info["outer_class_name"] = self.name(struct.unpack(">16I", ocraw)[11], struct.unpack(">16I", ocraw)[12])
        return info

    def guest_backtrace(self, sp, lr):
        chain = [f"{lr & 0xFFFFFFFF:08X}"]
        for _ in range(48):
            prev = self.g32(sp)
            if prev is None or prev <= sp or prev - sp > 0x200000 or prev & 7:
                break
            saved = self.g32(prev - 8)
            if saved is not None and CODE_LO <= saved < CODE_HI:
                chain.append(f"{saved:08X}")
            sp = prev
        return chain

    # ---- host thread helpers -------------------------------------------
    def thread_ids(self):
        snap = self.k.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
        ids = []
        try:
            entry = THREADENTRY32()
            entry.dwSize = C.sizeof(THREADENTRY32)
            ok = self.k.Thread32First(snap, C.byref(entry))
            while ok:
                if entry.th32OwnerProcessID == self.args.pid:
                    ids.append(entry.th32ThreadID)
                entry.dwSize = C.sizeof(THREADENTRY32)
                ok = self.k.Thread32Next(snap, C.byref(entry))
        finally:
            self.k.CloseHandle(snap)
        return ids

    def thread_context(self, handle):
        backing = C.create_string_buffer(1232 + 15)
        address = (C.addressof(backing) + 15) & ~15
        C.c_uint32.from_address(address + 0x30).value = 0x100003  # CONTEXT_CONTROL | CONTEXT_INTEGER
        if not self.k.GetThreadContext(handle, C.c_void_p(address)):
            return None
        raw = C.string_at(address, 1232)
        names = ["rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip"]
        return {name: struct.unpack_from("<Q", raw, 0x78 + index * 8)[0] for index, name in enumerate(names)}

    def stack_bounds(self, handle):
        info = THREAD_BASIC_INFORMATION()
        got = W.ULONG()
        if self.nt.NtQueryInformationThread(handle, 0, C.byref(info), C.sizeof(info), C.byref(got)) != 0 or not info.TebBaseAddress:
            return None
        item, raw = self.read(info.TebBaseAddress, 0x20)
        if len(raw) < 0x18:
            return None
        stack_base, stack_limit = struct.unpack_from("<QQ", raw, 8)
        return stack_limit, stack_base

    def ppc_context_candidate(self, address):
        """A PPCContext has r3,r0,r1,r2,r4.. (u64) from +0, lr at +0x100, ctr at +0x108."""
        if not address or address % 64:
            return None
        item, raw = self.read(address, 272)
        if len(raw) != 272:
            return None
        r1 = struct.unpack_from("<Q", raw, 0x10)[0]
        r13 = struct.unpack_from("<Q", raw, 0x68)[0]
        lr, ctr = struct.unpack_from("<QQ", raw, 0x100)
        if not (0x10000 <= r1 < BASE and r1 % 8 == 0):
            return None
        if not (0x10000 <= r13 < BASE and r13 % 0x1000 == 0):
            return None
        if not (CODE_LO <= (lr & 0xFFFFFFFF) < CODE_HI) or lr >> 32:
            return None
        if ctr >> 32:
            return None
        order = [3, 0, 1, 2] + list(range(4, 32))
        guest = {f"r{reg}": struct.unpack_from("<Q", raw, index * 8)[0] for index, reg in enumerate(order)}
        guest["lr"], guest["ctr"] = lr, ctr
        return guest

    def find_ppc_context(self, host, bounds):
        for reg in ("rdi", "rcx", "rbx", "rbp", "r12", "r13", "r14", "r15", "rsi"):
            found = self.ppc_context_candidate(host[reg])
            if found:
                return host[reg], f"register {reg}", found
        if not bounds:
            return None, "no stack bounds", None
        lo, hi = bounds
        start = max(lo, host["rsp"] & ~63)
        # Scan the live host stack in 64-byte steps (bounded to 2 MiB).
        address = start
        limit = min(hi, start + 0x200000)
        while address < limit:
            found = self.ppc_context_candidate(address)
            if found:
                return address, "stack scan", found
            address += 64
        return None, "not found", None

    def all_threads(self, fault_tid):
        threads = []
        for tid in self.thread_ids():
            entry = {"tid": tid, "is_fault_thread": tid == fault_tid}
            handle = self.k.OpenThread(0x0008 | 0x0040 | 0x0002, False, tid)  # GET_CONTEXT | QUERY_INFORMATION | SUSPEND_RESUME
            if not handle:
                entry["error"] = f"OpenThread {C.get_last_error()}"
                threads.append(entry)
                continue
            try:
                host = self.thread_context(handle)
                if not host:
                    entry["error"] = f"GetThreadContext {C.get_last_error()}"
                    threads.append(entry)
                    continue
                entry["host"] = {k: f"0x{v:016X}" for k, v in host.items()}
                bounds = self.stack_bounds(handle)
                entry["host_stack"] = [f"0x{b:X}" for b in bounds] if bounds else None
                address, how, guest = self.find_ppc_context(host, bounds)
                entry["ppc_context"] = f"0x{address:X}" if address else None
                entry["ppc_context_found_by"] = how
                if guest:
                    entry["guest"] = {k: f"0x{v:X}" for k, v in guest.items() if k in ("r1", "r3", "r13", "r31", "lr", "ctr")}
                    entry["guest_backtrace"] = self.guest_backtrace(guest["r1"] & 0xFFFFFFFF, guest["lr"])
            finally:
                self.k.CloseHandle(handle)
            threads.append(entry)
        return threads

    # ---- capture ---------------------------------------------------------
    def inspect_target(self, tid, exception):
        matched = super().inspect_target(tid, exception)
        if not matched:
            return matched
        capture = json.loads((self.out / "capture.json").read_text(encoding="utf-8"))
        guest = {k: int(v, 16) for k, v in capture["guest"].items()}
        extra = {}
        try:
            extra["names_data"] = f"0x{self.g32(NAMES) or 0:08X}"
            extra["names_num"] = self.g32(NAMES + 4)
            extra["gobj_num"] = self.g32(GOBJ + 4)
            extra["gis_threaded_rendering"] = self.g32(GIS_THREADED)
            ring = self.gbytes(RING, 28)
            extra["render_ring"] = [f"{w:08X}" for w in struct.unpack(">7I", ring)] if len(ring) == 28 else None
            extra["name_test_None"] = self.name(0, 0)
            r3 = guest["r3"] & 0xFFFFFFFF
            extra["freed_object"] = self.object_info(r3, "r3 freed material")
            parent = self.g32(r3 + 0x4C)
            if parent:
                extra["freed_object_w4c"] = self.object_info(parent, "freed material +0x4C (Parent?)")
            proxy = guest["r31"] & 0xFFFFFFFF
            owner = self.g32(proxy + 0x110)
            mesh = self.g32(proxy + 0x114)
            extra["proxy_owner"] = self.object_info(owner, "proxy+0x110 Owner actor") if owner else None
            extra["proxy_skeletal_mesh"] = self.object_info(mesh, "proxy+0x114 SkeletalMesh") if mesh else None
            scene_info = self.g32(proxy + 0x10)
            extra["proxy_scene_info"] = f"0x{scene_info or 0:08X}"
            if scene_info:
                si = self.gbytes(scene_info, 0xA4)
                extra["proxy_scene_info_words"] = [f"{w:08X}" for w in struct.unpack(f">{len(si)//4}I", si[:len(si)//4*4])]
                component = self.g32(scene_info + 0x0C)  # guess: FPrimitiveSceneInfo::Component
                for off in (0x0C, 0x10, 0x14, 0x18):
                    cand = self.g32(scene_info + off)
                    if cand:
                        info = self.object_info(cand, f"scene_info+0x{off:X}")
                        if "class_name" in info:
                            extra.setdefault("scene_info_objects", []).append(info)
            # Every material record of every LOD, resolved.
            data = self.g32(proxy + 0x134)
            num = self.g32(proxy + 0x138) or 0
            lods = []
            for lod in range(min(num, 8)):
                inner = self.g32(data + lod * 12)
                inner_num = self.g32(data + lod * 12 + 4) or 0
                records = []
                for e in range(min(inner_num, 32)):
                    mat = self.g32(inner + e * 8)
                    use = self.g32(inner + e * 8 + 4)
                    first = self.g32(mat) if mat else None
                    rec = {"material": f"0x{mat or 0:08X}", "use_index": use,
                           "first_word": f"0x{first:08X}" if first is not None else None,
                           "valid_vtable": first is not None and CODE_LO <= first < CODE_HI}
                    if mat and rec["valid_vtable"]:
                        rec["object"] = self.object_info(mat, "material")
                    elif mat:
                        rec["stale_object"] = self.object_info(mat, "stale material")
                    records.append(rec)
                lods.append({"lod": lod, "num": inner_num, "records": records})
            extra["lod_sections"] = lods
            extra["threads"] = self.all_threads(tid)
        except BaseException as error:  # keep the capture even if the extension fails
            extra["extension_error"] = repr(error)
        capture["v2"] = extra
        base.dump_json(self.out / "capture.json", capture)
        self.log("v2_extension_written", threads=len(extra.get("threads", [])))
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
    return ProbeV2(args).run()


if __name__ == "__main__":
    raise SystemExit(main())
