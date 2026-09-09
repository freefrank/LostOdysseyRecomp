"""External read-only Issue 12 exception capture. Runtime owner launches this only.

No WriteProcessMemory, SetThreadContext, injected calls, or process termination.
Only DebugActiveProcess's initial breakpoint is handled. All real exceptions are
returned DBG_EXCEPTION_NOT_HANDLED. See README.md for the bounded detach protocol.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import datetime
import hashlib
import json
import os
from pathlib import Path
import struct
import time
import traceback

ROOT = Path(__file__).resolve().parent
RUNTIME = ROOT.parent / "runtime"
OFFICIAL = RUNTIME / "official-v0.4.2" / "LostOdysseyRecomp.exe"
EXE_SHA = "13f1294bbb54efbf9a712a066441cb019ba5497cf1242bde6905e6c8bc1757b6"
BASE = 0x100000000
DBG_CONTINUE = 0x10002
DBG_EXCEPTION_NOT_HANDLED = 0x80010001


class EXCEPTION_RECORD(C.Structure):
    _fields_ = [("code", W.DWORD), ("flags", W.DWORD), ("record", C.c_void_p),
                ("address", C.c_void_p), ("count", W.DWORD),
                ("info", C.c_uint64 * 15)]


class EXCEPTION_INFO(C.Structure):
    _fields_ = [("record", EXCEPTION_RECORD), ("first_chance", W.DWORD)]


class EVENT_DATA(C.Union):
    _fields_ = [("exception", EXCEPTION_INFO), ("raw", C.c_ubyte * 160),
                ("align", C.c_uint64)]


class DEBUG_EVENT(C.Structure):
    _fields_ = [("code", W.DWORD), ("pid", W.DWORD), ("tid", W.DWORD),
                ("data", EVENT_DATA)]


def layout_check():
    assert os.name == "nt" and C.sizeof(C.c_void_p) == 8, "64-bit Windows required"
    assert C.sizeof(EXCEPTION_RECORD) == 152
    assert EXCEPTION_RECORD.info.offset == 32
    assert C.sizeof(EXCEPTION_INFO) == 160
    assert EXCEPTION_INFO.first_chance.offset == 152
    assert C.sizeof(DEBUG_EVENT) == 176 and DEBUG_EVENT.data.offset == 16
    return {"exception_record": 152, "exception_info": 160, "debug_event": 176,
            "context_size": 1232, "context_alignment": 16, "ppc_lr_offset": 256,
            "ppc_ctr_offset": 264, "ppc_gpr_order": [3, 0, 1, 2] + list(range(4, 32))}


def api():
    k = C.WinDLL("kernel32", use_last_error=True)
    specs = {
        "OpenProcess": ([W.DWORD, W.BOOL, W.DWORD], W.HANDLE),
        "OpenThread": ([W.DWORD, W.BOOL, W.DWORD], W.HANDLE),
        "CloseHandle": ([W.HANDLE], W.BOOL),
        "ReadProcessMemory": ([W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t,
                               C.POINTER(C.c_size_t)], W.BOOL),
        "QueryFullProcessImageNameW": ([W.HANDLE, W.DWORD, W.LPWSTR,
                                        C.POINTER(W.DWORD)], W.BOOL),
        "GetProcessTimes": ([W.HANDLE] + [C.POINTER(W.FILETIME)] * 4, W.BOOL),
        "DebugActiveProcess": ([W.DWORD], W.BOOL),
        "DebugActiveProcessStop": ([W.DWORD], W.BOOL),
        "DebugSetProcessKillOnExit": ([W.BOOL], W.BOOL),
        "WaitForDebugEvent": ([C.POINTER(DEBUG_EVENT), W.DWORD], W.BOOL),
        "ContinueDebugEvent": ([W.DWORD, W.DWORD, W.DWORD], W.BOOL),
        "GetThreadContext": ([W.HANDLE, C.c_void_p], W.BOOL),
        "SuspendThread": ([W.HANDLE], W.DWORD),
        "ResumeThread": ([W.HANDLE], W.DWORD),
    }
    for name, (args, result) in specs.items():
        fn = getattr(k, name)
        fn.argtypes, fn.restype = args, result
    return k


def checked(ok, name):
    if not ok:
        raise C.WinError(C.get_last_error(), name)
    return ok


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def dump_json(path, data):
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    tmp.replace(path)


class Probe:
    def __init__(self, args):
        self.args = args
        self.k = api()
        self.process = None
        self.thread = None
        self.attached = False
        self.pending = None
        self.first_execute_null_pending = False
        self.suspended = False
        self.initial_breakpoint = False
        self.out = args.output.resolve()
        if not self.out.is_relative_to(ROOT) or self.out == ROOT:
            raise ValueError("Output must be a fresh subdirectory of debug-probe")
        self.out.mkdir(parents=False, exist_ok=False)
        self.events = self.out / "events.jsonl"
        self.result = {"started_utc": utc(), "layout": layout_check(),
                       "helper_sha256": sha(Path(__file__)), "outcome": "starting"}

    def log(self, what, **data):
        row = {"utc": utc(), "event": what, **data}
        with self.events.open("a", encoding="utf-8") as f:
            f.write(json.dumps(row) + "\n")

    def identity(self):
        session_path = self.args.session.resolve()
        if not session_path.is_relative_to(RUNTIME) or session_path.name != "session.json":
            raise ValueError("Only an Issue 12 runtime session.json is accepted")
        session = json.loads(session_path.read_text(encoding="utf-8-sig"))
        if session["pid"] != self.args.pid or session["exe_sha256"].lower() != EXE_SHA:
            raise ValueError("PID or session executable hash mismatch")
        expected_path = Path(session["exe"]).resolve()
        if expected_path != OFFICIAL.resolve():
            raise ValueError("Only the frozen official v0.4.2 EXE is accepted")
        self.process = checked(self.k.OpenProcess(0x1010, False, self.args.pid), "OpenProcess")
        path = C.create_unicode_buffer(32768)
        length = W.DWORD(len(path))
        checked(self.k.QueryFullProcessImageNameW(self.process, 0, path, C.byref(length)),
                "QueryFullProcessImageNameW")
        if Path(path.value).resolve() != expected_path:
            raise ValueError("Live process executable path mismatch")
        times = [W.FILETIME() for _ in range(4)]
        checked(self.k.GetProcessTimes(self.process, *(C.byref(t) for t in times)),
                "GetProcessTimes")
        creation = times[0].dwLowDateTime | times[0].dwHighDateTime << 32
        if creation != session["creation"]:
            raise ValueError("Live process creation time mismatch (PID reuse)")
        digest = sha(expected_path)
        if digest != EXE_SHA:
            raise ValueError("Executable bytes do not match official frozen identity")
        self.result["identity"] = {"pid": self.args.pid, "creation": creation,
                                   "exe": str(expected_path), "sha256": digest,
                                   "session": str(session_path), "session_sha256": sha(session_path)}
        self.log("identity_verified", **self.result["identity"])

    def read(self, address, size):
        if size < 1 or size > 16384 or address < 0x1000 or address + size > 0x800000000000:
            return {"address": f"0x{address:X}", "size": size, "error": "bounded read rejected"}, b""
        data = C.create_string_buffer(size)
        got = C.c_size_t()
        ok = self.k.ReadProcessMemory(self.process, C.c_void_p(address), data,
                                      size, C.byref(got))
        result = {"address": f"0x{address:X}", "requested": size, "read": got.value}
        if not ok:
            result["winerror"] = C.get_last_error()
        raw = data.raw[:got.value]
        result["hex"] = raw.hex()
        result["sha256"] = hashlib.sha256(raw).hexdigest()
        return result, raw

    def guest_read(self, label, address, size, destination):
        if address < 0x1000 or address + size > BASE:
            destination[label] = {"guest": f"0x{address:X}", "error": "invalid guest range"}
            return b""
        item, raw = self.read(BASE + address, size)
        item["guest"] = f"0x{address:08X}"
        item["be32"] = [f"{v[0]:08X}" for v in struct.iter_unpack(">I", raw[:len(raw) // 4 * 4])]
        destination[label] = item
        return raw

    def host_context(self, tid):
        self.thread = checked(self.k.OpenThread(0x808 | 0x2, False, tid), "OpenThread")
        backing = C.create_string_buffer(1232 + 15)
        address = (C.addressof(backing) + 15) & ~15
        C.c_uint32.from_address(address + 0x30).value = 0x100003
        checked(self.k.GetThreadContext(self.thread, C.c_void_p(address)), "GetThreadContext")
        raw = C.string_at(address, 1232)
        names = ["rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                 "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip"]
        return {name: struct.unpack_from("<Q", raw, 0x78 + index * 8)[0]
                for index, name in enumerate(names)}, raw

    def inspect_target(self, tid, exception):
        """Called only at first-chance execute-null; never scans for context."""
        host, host_raw = self.host_context(tid)
        ctx_item, raw = self.read(host["rcx"], 272)
        guest = {}
        if len(raw) == 272:
            order = [3, 0, 1, 2] + list(range(4, 32))
            guest = {f"r{reg}": struct.unpack_from("<Q", raw, index * 8)[0]
                     for index, reg in enumerate(order)}
            guest["lr"], guest["ctr"] = struct.unpack_from("<QQ", raw, 256)
        checks = {"rip_zero": host["rip"] == 0, "base_exact": host["rdx"] == BASE,
                  "context_aligned": host["rcx"] % 64 == 0,
                  "context_read": len(raw) == 272, "lr_exact": guest.get("lr") == 0x823CB53C,
                  "ctr_zero": guest.get("ctr") == 0,
                  "guest_sp_range": 0x1000 <= guest.get("r1", 0) < BASE - 0xB00,
                  "guest_sp_aligned": guest.get("r1", 1) % 16 == 0}
        candidate = {"exception": exception, "checks": checks,
                     "host": {k: f"0x{v:016X}" for k, v in host.items()},
                     "guest_context_candidate": ctx_item,
                     "guest": {k: f"0x{v:016X}" for k, v in guest.items()}}
        if not all(checks.values()):
            self.log("execute_null_did_not_match_context", **candidate)
            dump_json(self.out / "mismatch.json", candidate)
            return False
        reads = {}
        stack = self.guest_read("guest_stack", guest["r1"], 0xB00, reads)
        if len(stack) != 0xB00:
            candidate["checks"]["guest_sp_read"] = False
            self.log("execute_null_stack_not_readable", **candidate)
            dump_json(self.out / "mismatch.json", candidate)
            return False
        candidate["checks"]["guest_sp_read"] = True
        self.log("target_exception_matched", tid=tid, guest=candidate["guest"])
        (self.out / "host-context.bin").write_bytes(host_raw)
        (self.out / "ppc-context.bin").write_bytes(raw)
        host_stack, host_stack_raw = self.read(host["rsp"], 4096)
        (self.out / "host-stack.bin").write_bytes(host_stack_raw)
        candidate["host_stack"] = host_stack
        object_data = self.guest_read("r3_object", guest["r3"] & 0xFFFFFFFF, 0x100, reads)
        if len(object_data) >= 4:
            vtable = struct.unpack_from(">I", object_data)[0]
            candidate["vtable"] = f"0x{vtable:08X}"
            table = self.guest_read("r3_vtable", vtable, 0x180, reads)
            if len(table) >= 0x128:
                candidate["slot_124"] = f"0x{struct.unpack_from('>I', table, 0x124)[0]:08X}"
        self.guest_read("r27_record", guest["r27"] & 0xFFFFFFFF, 0x20, reads)
        self.guest_read("r31_owner", guest["r31"] & 0xFFFFFFFF, 0x160, reads)
        self.guest_read("outer_records", guest["r22"] & 0xFFFFFFFF, 0x60, reads)
        current = (guest["r22"] + 12 * guest["r25"]) & 0xFFFFFFFF
        row = self.guest_read("current_outer_record", current, 12, reads)
        if len(row) == 12:
            inner = struct.unpack_from(">I", row)[0]
            self.guest_read("inner_records", inner, 0x60, reads)
        self.guest_read("caller_r28", guest["r28"] & 0xFFFFFFFF, 0x40, reads)
        candidate["reads"] = reads
        candidate["captured_utc"] = utc()
        dump_json(self.out / "capture.json", candidate)
        self.result["capture"] = "capture.json"
        return True

    def continue_event(self, status):
        event = self.pending
        checked(self.k.ContinueDebugEvent(event.pid, event.tid, status), "ContinueDebugEvent")
        self.pending = None

    def detach(self):
        if self.attached:
            checked(self.k.DebugActiveProcessStop(self.args.pid), "DebugActiveProcessStop")
            self.attached = False
            self.log("detached")

    def release_original_exception(self):
        # Prevent exception dispatch from reaching UnhandledExceptionFilter while
        # the debug port remains attached. Resume exactly this added suspend count.
        prior = None
        if not self.suspended:
            prior = self.k.SuspendThread(self.thread)
            if prior == 0xFFFFFFFF:
                raise C.WinError(C.get_last_error(), "SuspendThread")
            self.suspended = True
        try:
            self.log("fault_thread_temporarily_suspended", previous_count=prior)
            self.continue_event(DBG_EXCEPTION_NOT_HANDLED)
            self.log("original_exception_continued_not_handled")
            self.detach()
        finally:
            self.resume_own_suspend()

    def resume_own_suspend(self):
        if self.suspended:
            prior = self.k.ResumeThread(self.thread)
            if prior == 0xFFFFFFFF:
                raise C.WinError(C.get_last_error(), "ResumeThread")
            self.suspended = False
            self.log("fault_thread_own_suspend_resumed", previous_count=prior)

    def run(self):
        try:
            self.identity()
            checked(self.k.DebugActiveProcess(self.args.pid), "DebugActiveProcess")
            self.attached = True
            checked(self.k.DebugSetProcessKillOnExit(False), "DebugSetProcessKillOnExit(FALSE)")
            self.log("attached_kill_on_exit_false")
            deadline = time.monotonic() + self.args.timeout_seconds
            while time.monotonic() < deadline:
                if (self.out / "stop.request").exists():
                    self.result["outcome"] = "owner_requested_detach"
                    break
                event = DEBUG_EVENT()
                if not self.k.WaitForDebugEvent(C.byref(event), 500):
                    error = C.get_last_error()
                    if error in (0, 121, 258):
                        continue
                    raise C.WinError(error, "WaitForDebugEvent")
                self.pending = event
                if event.pid != self.args.pid:
                    raise RuntimeError("Unexpected process in debug event")
                status = DBG_CONTINUE
                if event.code in (3, 6):  # CREATE_PROCESS or LOAD_DLL: own file handle.
                    handle = struct.unpack_from("<Q", bytes(event.data.raw))[0]
                    if handle:
                        self.k.CloseHandle(handle)
                if event.code == 1:
                    ex = event.data.exception
                    record = ex.record
                    info = {"tid": event.tid, "code": f"0x{record.code:08X}",
                            "first_chance": ex.first_chance,
                            "address": f"0x{record.address or 0:X}",
                            "info": list(record.info[:min(record.count, 15)])}
                    self.log("exception", **info)
                    status = DBG_EXCEPTION_NOT_HANDLED
                    if not self.initial_breakpoint and record.code == 0x80000003 and ex.first_chance:
                        self.initial_breakpoint = True
                        self.continue_event(DBG_CONTINUE)
                        dump_json(self.out / "armed.json", {"utc": utc(), **self.result["identity"]})
                        self.log("armed_after_attach_breakpoint")
                        continue
                    if (self.initial_breakpoint and ex.first_chance and record.code == 0xC0000005
                            and record.count >= 2 and record.info[0] == 8 and record.info[1] == 0
                            and not record.address):
                        self.first_execute_null_pending = True
                        matched = self.inspect_target(event.tid, info)
                        self.release_original_exception()
                        if matched:
                            self.result["outcome"] = "target_captured_detached_original_exception_released"
                        else:
                            self.result["outcome"] = "context_mismatch_detached_original_exception_released"
                        break
                if event.code == 5:
                    self.result["outcome"] = "target_exited_before_matching_capture"
                    self.result["exit_code"] = struct.unpack_from("<I", bytes(event.data.raw))[0]
                    self.continue_event(status)
                    self.attached = False
                    break
                self.continue_event(status)
            else:
                self.result["outcome"] = "timeout_detach"
        except BaseException:
            self.result["outcome"] = "error"
            self.result["error"] = traceback.format_exc()
        finally:
            # No termination APIs. Make every cleanup operation independent so a
            # failed log/continue/detach cannot skip matching ResumeThread.
            errors = []
            if self.pending is not None and self.first_execute_null_pending and self.thread:
                try:
                    # Capture/serialization can fail after OpenThread. Preserve
                    # the same exception-dispatch ordering even on that path.
                    self.release_original_exception()
                except BaseException:
                    errors.append(traceback.format_exc())
            if self.pending is not None:
                try:
                    status = DBG_EXCEPTION_NOT_HANDLED if self.pending.code == 1 else DBG_CONTINUE
                    self.continue_event(status)
                except BaseException:
                    errors.append(traceback.format_exc())
            try:
                self.detach()
            except BaseException:
                errors.append(traceback.format_exc())
            try:
                self.resume_own_suspend()
            except BaseException:
                errors.append(traceback.format_exc())
            if self.thread:
                self.k.CloseHandle(self.thread)
            if self.process:
                self.k.CloseHandle(self.process)
            if errors:
                self.result["cleanup_errors"] = errors
            self.result["finished_utc"] = utc()
            self.result["still_attached"] = self.attached
            self.result["own_suspend_remaining"] = self.suspended
            dump_json(self.out / "result.json", self.result)
        return 0 if self.result["outcome"].startswith("target_captured_") else 2


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--check-layout", action="store_true", help="No process access or attach")
    p.add_argument("--session", type=Path)
    p.add_argument("--pid", type=int)
    p.add_argument("--output", type=Path)
    p.add_argument("--timeout-seconds", type=int, default=600)
    args = p.parse_args()
    if args.check_layout:
        print(json.dumps(layout_check(), indent=2))
        return 0
    if args.session is None or args.pid is None or args.output is None:
        p.error("--session, --pid, and a fresh --output directory are required")
    if not 30 <= args.timeout_seconds <= 1800:
        p.error("Timeout must be 30..1800 seconds")
    return Probe(args).run()


if __name__ == "__main__":
    raise SystemExit(main())
