# Optional log collection

The Worker `lost-odyssey-taa-collector` at `https://lo.dotslash.pro` receives opt-in runtime log summaries from Windows builds and stores them in D1 `lost-odyssey-taa-collection`. The names predate the switch from TAA telemetry to logs (2026-10-09). See the [privacy statement](../../PRIVACY.md) · [隐私说明](../../PRIVACY.zh-CN.md).

## Client

`LostOdysseyRecomp/os/log_collection.cpp`. Consent lives in `log-collection.ini` next to the other settings files: absent or invalid = undecided, `0` declined, `1` enabled. First-run setup asks once (No is the default); existing players are asked when they open Settings. Settings > System > Log collection changes it at any time. The old `taa-collection.ini` answer is not reused.

At startup, five seconds in, a background thread picks the newest `logs/runtime-<digits>.log` older than the current session that has not been sent (`logs/log-collection-sent` holds the last one) and posts one request. Nothing is sent from a crashing process, and the thread is never joined.

The text sent contains only:

- `[note]` lines: build, host OS, Wine version, GPU and driver, settings summary, map and battle loads, temporal suspects (unmapped depth writers while jitter is on), temporal jitter misses (mapped shaders whose jitter was rejected) and the clean-shutdown line;
- `[error]` lines, `[crash]` reports and `hang watch:` warnings.

Repeated messages are sent once. `%USERPROFILE%`, the account name, the computer name and any folder after `Users\` or `home/` are replaced. Each line is capped at 2,000 bytes and the text at 60,000 bytes (the first quarter and the end are kept). `LO_LOG_COLLECTION_URL` points a test build at a local `wrangler dev`.

The `Debug log` setting (or `LO_DEBUG_LOG=1`) writes `[info]` and kernel lines as well; without it the runtime log holds only notices, warnings and errors.

## Protocol

`POST /v1/logs`, `Content-Type: application/json`, no `Content-Encoding`, body at most 128 KiB:

```json
{"schema":1,"build":"0.9.1","platform":"windows","text":"..."}
```

`build` matches `^[0-9A-Za-z._+-]{1,80}$`, `platform` is `windows`, `text` is 1–65,536 bytes of well-formed UTF-8 without control characters other than tab and newline. Any other field is rejected. The reply is `{"accepted":1,"id":"<sha256 of text>"}`. The same text uploaded again only refreshes `last_seen`. A native rate limit allows 10 requests per minute per source IP.

`/v1/taa`, `/v1/temporal` and `/v1/shader-sources` answer `410` (`retired`). Old clients treat that as a failed upload.

## Storage and retention

`schema.sql` creates `runtime_logs(id, build, platform, text, first_seen, last_seen)`. The daily cron deletes rows whose `last_seen` is older than 30 days. It also keeps expiring the retired TAA tables (`observations`, `temporal_sequences`, `shader_source_observations`, `shader_sources`) until they are empty. The private repository [LostOdysseyRecomp-build-inputs](https://github.com/freefrank/LostOdysseyRecomp-build-inputs) archives `runtime_logs` every four hours next to the earlier TAA data; those copies do not expire.

The Worker reads `CF-Connecting-IP` only for the rate limiter and never stores it. Workers Logs, invocation logs and traces are disabled.

## Deploy

Pinned Wrangler 4.130.0:

```sh
npm ci
npm test
npx wrangler d1 execute lost-odyssey-taa-collection --remote --file schema.sql
npx wrangler deploy
```

Read recent logs:

```sh
npx wrangler d1 execute lost-odyssey-taa-collection --remote --command "SELECT id, build, datetime(last_seen,'unixepoch') FROM runtime_logs ORDER BY last_seen DESC LIMIT 20"
```
