"""Issue 12 capture, v3: v2's all-thread / FName extension, but the trigger is any
first-chance ACCESS_VIOLATION whose faulting thread's PPCContext has
LR == 0x823CB53C (the DrawDynamicElements material call). run-07 showed the same
stale-material fault as a *read* AV inside the module (garbage vtable pointer)
rather than an execute-null, which v1/v2 would have let through.
Read-only; same attach / continue-unhandled / detach protocol as v1."""
import ctypes as C
from ctypes import wintypes as W
import json
import struct
import sys
import time
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import capture_execute_null as base  # noqa: E402
import capture_execute_null_v2 as v2  # noqa: E402

TARGET_LR = 0x823CB53C


def relaxed_inspect_target(self, tid, exception):
    """Locate the faulting thread's PPCContext (register candidates, then host
    stack scan) and require LR == TARGET_LR; RIP/RDX are recorded, not required."""
    host, host_raw = self.host_context(tid)
    ctx_addr, how, guest = None, "not found", None
    for reg in ("rcx", "rdi", "rbx", "rbp", "r12", "r13", "r14", "r15", "rsi"):
        found = self.ppc_context_candidate(host[reg])
        if found and (found["lr"] & 0xFFFFFFFF) == TARGET_LR:
            ctx_addr, how, guest = host[reg], f"register {reg}", found
            break
    if guest is None:
        handle = self.k.OpenThread(0x0048, False, tid)
        bounds = None
        if handle:
            try:
                bounds = self.stack_bounds(handle)
            finally:
                self.k.CloseHandle(handle)
        address, how2, found = self.find_ppc_context(host, bounds)
        if found and (found["lr"] & 0xFFFFFFFF) == TARGET_LR:
            ctx_addr, how, guest = address, how2, found
    ctx_item, raw = self.read(ctx_addr, 272) if ctx_addr else ({"error": "no context"}, b"")
    guest = guest or {}
    checks = {"context_found": guest != {}, "context_found_by": how,
              "rip_zero": host["rip"] == 0, "base_in_rdx": host["rdx"] == base.BASE,
              "lr_exact": (guest.get("lr", 0) & 0xFFFFFFFF) == TARGET_LR,
              "ctr_zero": guest.get("ctr") == 0,
              "guest_sp_range": 0x1000 <= guest.get("r1", 0) < base.BASE - 0xB00,
              "guest_sp_aligned": guest.get("r1", 1) % 16 == 0}
    candidate = {"exception": exception, "checks": checks, "trigger": "any first-chance AV with LR match (v3)",
                 "host": {k: f"0x{v:016X}" for k, v in host.items()},
                 "guest_context_candidate": ctx_item,
                 "guest": {k: f"0x{v:016X}" for k, v in guest.items()}}
    required = ("context_found", "lr_exact", "guest_sp_range", "guest_sp_aligned")
    if not all(checks[k] for k in required):
        self.log("access_violation_did_not_match_context", **candidate)
        base.dump_json(self.out / "mismatch.json", candidate)
        return False
    reads = {}
    stack = self.guest_read("guest_stack", guest["r1"] & 0xFFFFFFFF, 0x2000, reads)
    candidate["checks"]["guest_sp_read"] = len(stack) > 0
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
    candidate["reads"] = reads
    candidate["captured_utc"] = base.utc()
    base.dump_json(self.out / "capture.json", candidate)
    self.result["capture"] = "capture.json"
    return True


base.Probe.inspect_target = relaxed_inspect_target  # v2's super() call now uses the relaxed capture


class ProbeV3(v2.ProbeV2):
    def run(self):
        """Same loop as v1, but every first-chance 0xC0000005 after the attach
        breakpoint is inspected (LR match decides), not only execute-null."""
        try:
            self.identity()
            base.checked(self.k.DebugActiveProcess(self.args.pid), "DebugActiveProcess")
            self.attached = True
            base.checked(self.k.DebugSetProcessKillOnExit(False), "DebugSetProcessKillOnExit(FALSE)")
            self.log("attached_kill_on_exit_false")
            deadline = time.monotonic() + self.args.timeout_seconds
            while time.monotonic() < deadline:
                if (self.out / "stop.request").exists():
                    self.result["outcome"] = "owner_requested_detach"
                    break
                event = base.DEBUG_EVENT()
                if not self.k.WaitForDebugEvent(C.byref(event), 500):
                    error = C.get_last_error()
                    if error in (0, 121, 258):
                        continue
                    raise C.WinError(error, "WaitForDebugEvent")
                self.pending = event
                if event.pid != self.args.pid:
                    raise RuntimeError("Unexpected process in debug event")
                status = base.DBG_CONTINUE
                if event.code in (3, 6):
                    handle = struct.unpack_from("<Q", bytes(event.data.raw))[0]
                    if handle:
                        self.k.CloseHandle(handle)
                if event.code == 1:
                    ex = event.data.exception
                    record = ex.record
                    info = {"tid": event.tid, "code": f"0x{record.code:08X}", "first_chance": ex.first_chance,
                            "address": f"0x{record.address or 0:X}", "info": list(record.info[:min(record.count, 15)])}
                    self.log("exception", **info)
                    status = base.DBG_EXCEPTION_NOT_HANDLED
                    if not self.initial_breakpoint and record.code == 0x80000003 and ex.first_chance:
                        self.initial_breakpoint = True
                        self.continue_event(base.DBG_CONTINUE)
                        base.dump_json(self.out / "armed.json", {"utc": base.utc(), **self.result["identity"]})
                        self.log("armed_after_attach_breakpoint")
                        continue
                    if self.initial_breakpoint and ex.first_chance and record.code == 0xC0000005:
                        self.first_execute_null_pending = True
                        matched = self.inspect_target(event.tid, info)
                        self.release_original_exception()
                        self.result["outcome"] = ("target_captured_detached_original_exception_released" if matched
                                                  else "context_mismatch_detached_original_exception_released")
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
            errors = []
            if self.pending is not None and self.first_execute_null_pending and self.thread:
                try:
                    self.release_original_exception()
                except BaseException:
                    errors.append(traceback.format_exc())
            if self.pending is not None:
                try:
                    self.continue_event(base.DBG_EXCEPTION_NOT_HANDLED if self.pending.code == 1 else base.DBG_CONTINUE)
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
            self.result["finished_utc"] = base.utc()
            self.result["still_attached"] = self.attached
            self.result["own_suspend_remaining"] = self.suspended
            base.dump_json(self.out / "result.json", self.result)
        return 0 if self.result["outcome"].startswith("target_captured_") else 2


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
    return ProbeV3(args).run()


if __name__ == "__main__":
    raise SystemExit(main())
