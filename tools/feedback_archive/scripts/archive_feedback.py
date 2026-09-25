"""Read D1 feedback with SELECT queries and update an explicit local private archive."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import time
import urllib.error
import urllib.request

OVERLAP_SECONDS = 600
SETTLE_SECONDS = 120
SCHEMA = 1
TABLES = {
    "shader_sources": ("stage", "sha256"),
    "observations": ("id",),
    "shader_source_observations": ("stage", "sha256", "build", "backend", "gpu_key", "driver"),
    "temporal_sequences": ("id",),
}
COLUMNS = {
    "shader_sources": "stage,sha256,renderer_hash,byte_length,first_seen,last_seen",
    "observations": "id,diagnostic,max_draws,first_seen,last_seen",
    "shader_source_observations": "stage,sha256,build,backend,gpu_key,gpu,driver,first_seen,last_seen",
    "temporal_sequences": "id,metadata,summary,length(payload) AS byte_length,first_seen,last_seen",
}


class ArchiveError(Exception):
    """Messages contain operation names only, never response bodies or submitted data."""


def require(condition, message):
    if not condition:
        raise ArchiveError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2, allow_nan=False) + "\n").encode("utf-8")


def parse_json(value):
    return json.loads(value, parse_constant=lambda _: (_ for _ in ()).throw(ArchiveError("Non-finite JSON")))


def hex_id(value, size=64):
    require(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{%d}" % size, value), "Invalid content identifier")
    return value


def renderer_hash(data):
    result = 0xCBF29CE484222325
    for byte in data:
        result = ((result ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{result:016x}"


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ArchiveError("D1 redirect refused")


class D1:
    def __init__(self, token, account_id, database_id):
        require(bool(token), "Missing CLOUDFLARE_API_TOKEN (dedicated Account D1 Read token required)")
        require(re.fullmatch(r"[0-9a-fA-F]{32}", account_id) is not None, "Invalid account ID")
        require(re.fullmatch(r"[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}", database_id) is not None, "Invalid database ID")
        self.account_id, self.database_id = account_id.lower(), database_id.lower()
        self.rows_read = 0
        self.queries = 0
        self.token = token
        self.url = f"https://api.cloudflare.com/client/v4/accounts/{self.account_id}/d1/database/{self.database_id}/query"
        self.opener = urllib.request.build_opener(NoRedirect())

    def query(self, sql, params=()):
        require(sql.startswith("SELECT ") and ";" not in sql, "Only a single SELECT is allowed")
        body = json.dumps({"sql": sql, "params": [str(x) for x in params]}).encode()
        for attempt in range(4):
            request = urllib.request.Request(self.url, data=body, headers={
                "Authorization": f"Bearer {self.token}", "Content-Type": "application/json"})
            try:
                with self.opener.open(request, timeout=60) as response:
                    raw = response.read(16 * 1024 * 1024 + 1)
                require(len(raw) <= 16 * 1024 * 1024, "D1 response exceeds limit")
                data = parse_json(raw)
                codes = [str(e.get("code", "unknown")) for e in data.get("errors", []) if isinstance(e, dict)]
                require(data.get("success") is True, "D1 query rejected; codes=" + ",".join(codes))
                result = data.get("result")
                require(isinstance(result, list) and len(result) == 1, "Unexpected D1 query result count")
                result = result[0]
                require(result.get("success") is True, "D1 statement failed")
                require(result.get("meta", {}).get("rows_written", 0) == 0, "D1 reported a write")
                require(isinstance(result.get("results"), list), "D1 rows missing")
                self.rows_read += result.get("meta", {}).get("rows_read", 0)
                self.queries += 1
                return result["results"]
            except urllib.error.HTTPError as error:
                if error.code not in (429, 500, 502, 503, 504) or attempt == 3:
                    raise ArchiveError(f"D1 HTTP {error.code}; check token permission/account and service status") from None
            except (urllib.error.URLError, TimeoutError):
                if attempt == 3:
                    raise ArchiveError("D1 network request failed") from None
            time.sleep(2 ** attempt)
        raise ArchiveError("D1 retry limit")


def rows_since(db, table, lower, upper, page_size, keys_only=False):
    keys = TABLES[table]
    order = keys if table == "shader_sources" else ("last_seen", "rowid")
    columns = ",".join(dict.fromkeys(("last_seen",) + keys)) if keys_only else COLUMNS[table]
    if table != "shader_sources":
        columns += ",rowid"
    cursor = None
    while True:
        # The last_seen indexes include rowid implicitly. An equality seek on
        # timestamp plus rowid avoids rescanning a large same-second group.
        if cursor is not None and table != "shader_sources":
            same = db.query(f"SELECT {columns} FROM {table} WHERE last_seen = ? AND rowid > ? ORDER BY rowid LIMIT ?",
                            [cursor[0], cursor[1], page_size])
            require(len(same) <= page_size, "D1 page exceeds requested limit")
            for row in same:
                current = (row["last_seen"], row["rowid"])
                require(current[0] == cursor[0] and current > cursor, "D1 pagination did not advance")
                cursor = current
                yield row
            if len(same) == page_size:
                continue
        where = "last_seen >= ? AND last_seen <= ?"
        params = [lower, upper]
        if cursor is not None:
            if table == "shader_sources":
                where += f" AND ({','.join(order)}) > ({','.join('?' for _ in order)})"
                params.extend(cursor)
            else:
                where = "last_seen > ? AND last_seen <= ?"
                params = [cursor[0], upper]
        sql = f"SELECT {columns} FROM {table} WHERE {where} ORDER BY {','.join(order)} LIMIT ?"
        batch = db.query(sql, params + [page_size])
        require(len(batch) <= page_size, "D1 page exceeds requested limit")
        for row in batch:
            current = tuple(row[k] for k in order)
            require(type(row["last_seen"]) is int and lower <= row["last_seen"] <= upper, "D1 row outside scan bounds")
            require(cursor is None or current > cursor, "D1 pagination did not advance")
            cursor = current
            yield row
        if len(batch) < page_size:
            return


def temporal_raw_and_summary(packed):
    stride, raw_size = 4824, 16 + 32 * 4824
    require(len(packed) <= 256 * 1024, "Temporal payload too large")
    if packed[:4] == b"LOR1":
        raw = bytearray(packed[4:])
    elif packed[:4] == b"LOZ1":
        raw = bytearray(raw_size)
        source, target = 4, 0
        while source < len(packed):
            token = packed[source]
            source += 1
            count = (token & 127) + 1
            require(target + count <= raw_size, "Temporal expansion exceeds bound")
            if not token & 128:
                require(source + count <= len(packed), "Temporal stream truncated")
                raw[target:target + count] = packed[source:source + count]
                source += count
            target += count
        require(target == raw_size, "Temporal expansion size mismatch")
        for offset in range(16 + stride, raw_size):
            raw[offset] ^= raw[offset - stride]
    else:
        raise ArchiveError("Unknown temporal codec")
    require(len(raw) == raw_size and raw[:4] == b"LOMV" and struct.unpack_from("<III", raw, 4) == (1, 32, stride), "Temporal header mismatch")
    summary = dict(frames=32, grid=[32, 18], motion="camera-only", units="pixels-previous-minus-current-unjittered",
                   depth="reverse-nonlinear", validMotion=0, invalidDepth=0, maxAbsMotion=0,
                   jitterMin=[math.inf, math.inf], jitterMax=[-math.inf, -math.inf], historyReusableFrames=0)
    first, epoch = struct.unpack_from("<QQ", raw, 16)
    last_time = -math.inf
    for frame in range(32):
        offset = 16 + frame * stride
        index, current_epoch, timestamp, width, height, flags = struct.unpack_from("<QQdIII", raw, offset)
        require(index == first + frame and current_epoch == epoch and 32 <= width <= 7680 and 18 <= height <= 4320 and flags <= 15 and math.isfinite(timestamp) and timestamp >= last_time, "Temporal frame mismatch")
        require(not frame or (width, height) == (summary["width"], summary["height"]), "Temporal extent mismatch")
        last_time = timestamp
        summary.update(width=width, height=height)
        summary["historyReusableFrames"] += bool(flags & 2)
        jitter = struct.unpack_from("<ffff", raw, offset + 36)
        require(all(math.isfinite(x) and abs(x) <= 16 for x in jitter), "Temporal jitter invalid")
        for axis in range(2):
            summary["jitterMin"][axis] = min(summary["jitterMin"][axis], jitter[axis])
            summary["jitterMax"][axis] = max(summary["jitterMax"][axis], jitter[axis])
        require(all(math.isfinite(x) for x in struct.unpack_from("<40f", raw, offset + 52)), "Temporal camera invalid")
        require(struct.unpack_from("<I", raw, offset + 212)[0] == 0, "Temporal reserved field invalid")
        for sample in range(576):
            x, y, depth = struct.unpack_from("<eef", raw, offset + 216 + sample * 8)
            valid_depth = math.isfinite(depth) and 0 < depth <= 1
            summary["invalidDepth"] += not valid_depth
            if math.isfinite(x) and math.isfinite(y):
                require(flags & 1 and valid_depth, "Temporal motion/depth mismatch")
                summary["validMotion"] += 1
                summary["maxAbsMotion"] = max(summary["maxAbsMotion"], abs(x), abs(y))
    summary.update(firstFrame=str(first), lastFrame=str(first + 31), rawBytes=raw_size, packedBytes=len(packed))
    return bytes(raw), summary


class Archive:
    def __init__(self, repo, db, page_size=100, new_only=False):
        self.repo = Path(repo).resolve()
        self.db, self.page_size = db, page_size
        self.new_only = new_only
        self.skipped = 0
        self.staging = self.repo / ".feedback-staging"
        self.pending = {}
        self.counts = {table: 0 for table in TABLES}
        self.downloads = 0

    def existing(self, relative, staged=False):
        locations = (self.staging, self.repo) if staged else (self.repo,)
        for root in locations:
            path = root / relative
            if path.is_file():
                return path.read_bytes()
        return None

    def stage(self, relative, data):
        if self.existing(relative) == data:
            return
        path = self.staging / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_bytes(data)
        os.replace(temporary, path)
        self.pending[relative] = path

    def metadata(self, table, key, row, mutable=()):
        relative = f"feedback/data/{table}/{key}.json"
        old = self.existing(relative)
        if old is not None:
            previous = parse_json(old)
            immutable = set(row) - {"first_seen", "last_seen", "max_draws"} - set(mutable)
            require(all(previous.get(k) == row[k] for k in immutable), "Archived immutable metadata changed")
            row["first_seen"] = min(previous["first_seen"], row["first_seen"])
            row["last_seen"] = max(previous["last_seen"], row["last_seen"])
            if "max_draws" in row:
                row["max_draws"] = max(previous["max_draws"], row["max_draws"])
        self.stage(relative, json_bytes(row))

    def payload(self, table, row, relative, validator):
        data = self.existing(relative, staged=True)
        if data is None:
            keys = TABLES[table]
            sql = f"SELECT hex(payload) AS payload_hex FROM {table} WHERE " + " AND ".join(k + " = ?" for k in keys)
            records = self.db.query(sql, [row[k] for k in keys])
            require(len(records) == 1, "Payload disappeared during scan; retry required")
            value = records[0]["payload_hex"]
            require(isinstance(value, str) and len(value) <= 512 * 1024 and re.fullmatch("[0-9a-fA-F]*", value), "Invalid payload encoding")
            data = bytes.fromhex(value)
            self.downloads += 1
        validator(data)
        self.stage(relative, data)
        return data

    def accept(self, table, original):
        row = dict(original)
        row.pop("rowid", None)
        require(type(row["first_seen"]) is int and 0 <= row["first_seen"] <= row["last_seen"], "Invalid observation timestamps")
        if table == "observations":
            key = hex_id(row["id"])
            require(isinstance(row["diagnostic"], str) and sha256(row["diagnostic"].encode()) == key, "Diagnostic content hash mismatch")
            diagnostic = parse_json(row["diagnostic"])
            require(isinstance(diagnostic, dict) and diagnostic.get("schema") in (1, 2, 3, 4), "Unsupported diagnostic schema")
            require(type(row["max_draws"]) is int and row["max_draws"] > 0, "Invalid draw count")
        elif table in ("shader_sources", "shader_source_observations"):
            require(row["stage"] in ("vs", "ps"), "Invalid shader stage")
            digest = hex_id(row["sha256"])
            if table == "shader_sources":
                key = row["stage"] + "/" + digest
                hex_id(row["renderer_hash"], 16)
                require(type(row["byte_length"]) is int and 0 < row["byte_length"] <= 65536 and row["byte_length"] % 4 == 0, "Invalid shader size")
                def validate(data):
                    require(len(data) == row["byte_length"] and sha256(data) == digest and renderer_hash(data) == row["renderer_hash"], "Shader length/SHA-256/FNV mismatch")
                self.payload(table, row, f"feedback/data/programs/{key}.bin", validate)
            else:
                key = sha256(json_bytes([row[k] for k in TABLES[table]]))
                source_path = f"feedback/data/shader_sources/{row['stage']}/{digest}.json"
                if self.existing(source_path, staged=True) is None:
                    # A source can advance past the snapshot bound while its older association remains.
                    sources = self.db.query("SELECT " + COLUMNS["shader_sources"] + " FROM shader_sources WHERE stage = ? AND sha256 = ?", [row["stage"], digest])
                    require(len(sources) == 1, "Associated source missing; retry required")
                    self.accept("shader_sources", sources[0])
        else:
            key = hex_id(row["id"])
            require(isinstance(row["metadata"], str) and isinstance(row["summary"], str), "Invalid temporal metadata")
            metadata, summary = parse_json(row["metadata"]), parse_json(row["summary"])
            require(isinstance(metadata, dict) and set(metadata) == {"build", "backend", "gpu", "driver"} and isinstance(summary, dict), "Unknown temporal metadata schema")
            old_path = f"feedback/data/temporal_sequences/{key}.json"
            old = self.existing(old_path, staged=True)
            previous = parse_json(old) if old else None
            if previous:
                # Expiry and reappearance may encode identical raw content differently.
                # Retain the already verified wire representation, while checking that
                # the newly observed summary describes the same decoded sequence.
                archived_summary = parse_json(previous["summary"])
                semantic = lambda value: {k: v for k, v in value.items() if k != "packedBytes"}
                require(summary.get("packedBytes") == row["byte_length"] and semantic(summary) == semantic(archived_summary), "Temporal content summary changed")
                row["summary"], row["byte_length"] = previous["summary"], previous["byte_length"]
                summary = archived_summary
            # Content ID derives from metadata + raw bytes, not packed bytes. A small alias in
            # staging lets failed local runs reuse an already verified packed stream as well.
            digest = hex_id(previous["payload_sha256"]) if previous else key
            relative = f"feedback/data/temporal_payloads/{digest}.bin" if previous else f"feedback/data/temporal_pending/{key}.bin"
            def validate(data):
                raw, computed = temporal_raw_and_summary(data)
                require(len(data) == row["byte_length"] and computed == summary and sha256(row["metadata"].encode() + raw) == key, "Temporal summary/length/content hash mismatch")
            data = self.payload(table, row, relative, validate)
            row["payload_sha256"] = sha256(data)
            row["payload_path"] = f"feedback/data/temporal_payloads/{row['payload_sha256']}.bin"
            self.stage(row["payload_path"], data)
            if "temporal_pending/" in relative:
                self.pending.pop(relative, None)
        self.metadata(table, key, row, mutable=("gpu",) if table == "shader_source_observations" else ())
        self.counts[table] += 1

    def run(self):
        checkpoint_path = "feedback/checkpoint.json"
        old_bytes = self.existing(checkpoint_path)
        checkpoint = parse_json(old_bytes) if old_bytes else {"schema": SCHEMA, "account_id": self.db.account_id, "database_id": self.db.database_id, "through": 0}
        require(checkpoint.get("schema") == SCHEMA and checkpoint.get("account_id") == self.db.account_id and checkpoint.get("database_id") == self.db.database_id and type(checkpoint.get("through")) is int and checkpoint["through"] >= 0, "Checkpoint scope/schema invalid")
        now_rows = self.db.query("SELECT CAST(strftime('%s','now') AS INTEGER) AS now")
        require(len(now_rows) == 1 and type(now_rows[0].get("now")) is int, "D1 clock unavailable")
        upper = now_rows[0]["now"] - SETTLE_SECONDS
        require(upper >= checkpoint["through"], "D1 clock precedes checkpoint")
        lower = max(0, checkpoint["through"] - OVERLAP_SECONDS)
        for table in TABLES:
            for row in rows_since(self.db, table, lower, upper, self.page_size, self.new_only):
                if self.new_only:
                    if table == "shader_sources":
                        key = row["stage"] + "/" + hex_id(row["sha256"])
                        require(row["stage"] in ("vs", "ps"), "Invalid shader stage")
                    elif table == "shader_source_observations":
                        key = sha256(json_bytes([row[k] for k in TABLES[table]]))
                    else:
                        key = hex_id(row["id"])
                    relative = f"feedback/data/{table}/{key}.json"
                    if self.existing(relative) is not None or relative in self.pending:
                        self.skipped += 1
                        continue
                    sql = "SELECT " + COLUMNS[table] + " FROM " + table + " WHERE " + " AND ".join(k + " = ?" for k in TABLES[table])
                    records = self.db.query(sql, [row[k] for k in TABLES[table]])
                    require(len(records) == 1, "Metadata disappeared during scan; retry required")
                    row = records[0]
                self.accept(table, row)
        checkpoint.update(through=upper, overlap_seconds=OVERLAP_SECONDS, settle_seconds=SETTLE_SECONDS)
        self.stage(checkpoint_path, json_bytes(checkpoint))
        # No tracked file is touched until every query and validation has succeeded.
        # Git publishes these replacements together; checkpoint is promoted last locally.
        ordered = sorted(self.pending, key=lambda name: (name == checkpoint_path, name))
        for relative in ordered:
            target = self.repo / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            os.replace(self.pending[relative], target)
        return {"existing_records_skipped": self.skipped, "d1_rows_read": getattr(self.db, "rows_read", None), "d1_queries": getattr(self.db, "queries", None), "rows_scanned": self.counts, "payloads_downloaded": self.downloads, "files_changed": len(ordered), "through": upper}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True, help="Existing private archive directory; writes feedback/ and staging here")
    parser.add_argument("--account", required=True, help="Cloudflare account ID (no saved default)")
    parser.add_argument("--database", required=True, help="D1 database UUID (no saved default)")
    parser.add_argument("--new-only", action="store_true", help="Keep archived metadata unchanged; fetch full records only for unknown keys")
    args = parser.parse_args()
    require(args.repo.is_dir(), "Archive directory does not exist")
    db = D1(os.environ.get("CLOUDFLARE_API_TOKEN", ""), args.account, args.database)
    lock = args.repo / ".feedback-archive.lock"
    try:
        with lock.open("x"):
            pass
    except FileExistsError:
        raise ArchiveError("Archive already locked; inspect the local process before removing a stale lock") from None
    try:
        result = Archive(args.repo, db, new_only=args.new_only).run()
        print(json.dumps(result, sort_keys=True))
    finally:
        lock.unlink()


if __name__ == "__main__":
    try:
        main()
    except ArchiveError as error:
        print(f"Archive failed: {error}", file=sys.stderr)
        sys.exit(1)
    except Exception as error:
        # Never echo a submitted row, raw HTTP body, SQL parameter, or token via a traceback.
        print(f"Archive failed: unexpected {type(error).__name__}; checkpoint not published", file=sys.stderr)
        sys.exit(1)
