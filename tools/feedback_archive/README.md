# Private feedback archive tools

Reusable Python 3.10+ standard-library tools for Lost Odyssey's opt-in D1
diagnostics. Source code lives here; feedback, shader programs, checkpoints and
review ledgers stay in an explicitly selected **private directory**.

## Commands and effects

| Entry point | Inputs and effects |
| --- | --- |
| `scripts/archive_feedback.py` | Sends SELECT queries to the chosen Cloudflare D1 database, downloads records, and writes `feedback/`, `.feedback-staging/` and a temporary lock under `--repo`. No remote database writes, Git operations or publishing. |
| `scripts/feedback_ledger.py` | Offline `sync`, `report`, and `review` using explicit paths. Sync/review update a local ledger; report prints Markdown or writes `--output`. No network or game operations. |
| `scripts/export_programs.py` | Exports explicitly selected VS/PS payloads after strict stage/hash/length/SHA-256/byte-FNV preflight. Writes `vs_HASH.bin`, `ps_HASH.bin`, and `provenance.json` into a new output directory outside the archive. |
| `skills/lo-feedback-triage/scripts/feedback.py` | Wrapper around this package's ledger implementation. Requires `--archive`; uses `feedback/triage/` inside that directory for default ledger/context/seed locations. |

From the game repository root, inspect help without accessing the network:

```sh
python -B tools/feedback_archive/scripts/archive_feedback.py --help
python -B tools/feedback_archive/scripts/feedback_ledger.py --help
python -B tools/feedback_archive/scripts/export_programs.py --help
python -B tools/feedback_archive/skills/lo-feedback-triage/scripts/feedback.py --help
```

For an authorized D1 refresh, create or select an existing private archive
directory. Supply a dedicated Account / D1 / Read token in the process environment
as `CLOUDFLARE_API_TOKEN`, then explicitly select account, database and output:

```sh
python -B tools/feedback_archive/scripts/archive_feedback.py --repo /path/to/private-archive --account ACCOUNT_ID --database DATABASE_UUID
```

There are no deployment-specific account/database defaults. The checkpoint is
bound to the chosen account/database; a mismatch fails before querying D1.
Keep generated data out of this public source tree. The script does not create
or configure a scheduled job, upload data, commit, or push.

`--new-only` preserves already archived metadata and fetches full records only for
unknown keys. It deliberately skips updates to timestamps/counts for known keys,
but still advances the checkpoint; later ordinary runs only cover their overlap
window. Omit it when current metadata and observation history matter.

## Offline ledger

```sh
python -B tools/feedback_archive/skills/lo-feedback-triage/scripts/feedback.py sync --archive /path/to/private-archive
python -B tools/feedback_archive/skills/lo-feedback-triage/scripts/feedback.py report --archive /path/to/private-archive
python -B tools/feedback_archive/skills/lo-feedback-triage/scripts/feedback.py review --archive /path/to/private-archive --case CASE_ID --file /path/to/review.json
```

These examples start in the game repository root. From another working directory,
use the absolute script path. If installed as the `lo-feedback-triage` skill, its
`scripts/feedback.py` works through the installed directory junction and still
requires the private `--archive`. It resolves code from this package, never from
the archive. The complete `tools/feedback_archive/` directory must remain together.

Optional `--ledger`, `--seed` and `--context` override defaults. Missing default
seed/context files are allowed; missing explicit files are errors. Unknown source
context remains a gap. Context is a reviewed source snapshot, not automatic shader
analysis. Seeded reviews remain historical until bound to current evidence.

The importer consumes canonical `observations` and `shader_sources`, including
schema 2/3 TAA candidates and schema 4 compact CPU windows. It retains relevant
schema 1 records but does not import temporal sequences or GPU association rows
into shader cases. Repeated content reuses cases. New program content, PS pairings,
binding state or relevant reviewed context can reopen review; counters, timestamps
and CPU history alone do not establish a changed shader analysis.

Review JSON requires `decision`, `summary`, `evidence`, `gaps`, and `scope`;
implementation, validation, and acceptance can be recorded independently. For example:

```json
{
  "decision": "needs_evidence",
  "summary": "The observed pair still lacks visual validation.",
  "evidence": ["private/review.md"],
  "gaps": ["same-scene visual comparison"],
  "scope": "one reviewed VS/PS pair"
}
```

See the [skill](skills/lo-feedback-triage/SKILL.md),
[review rules](skills/lo-feedback-triage/references/review-rules.md), and
[compact diagnostic limits](skills/lo-feedback-triage/references/compact-diagnostics.md).
CPU readiness and accepted uploads do not establish GPU pixels or a repaired game.

## Archive format and recovery

`feedback/data/programs/{vs,ps}/<sha256>.bin` deduplicates programs by stage/content;
source metadata also checks byte-FNV and length. `observations/<id>.json` preserves
canonical diagnostic JSON. `shader_source_observations/` preserves GPU/backend/build
associations. Temporal metadata references validated packed streams in
`temporal_payloads/`; an existing verified representation is reused across codecs.

Scans use a D1 clock upper bound minus two minutes and a ten-minute checkpoint
overlap. Indexed timestamp/rowid pagination handles same-second groups; shader
source pagination uses its composite key. All table queries and content checks
finish before staged files are promoted. The checkpoint is promoted last. Local
promotion is not a transactional multi-file filesystem operation; a disk failure
may leave some data promoted with the prior checkpoint. A retry can reuse verified
staging payloads. An existing lock requires checking the associated process before
removing a genuinely stale lock.

The archive is an incremental research copy, not a transactional database backup.
It cannot recover expired records that were never downloaded. Files have no local
automatic expiry. If an archive owner publishes records and checkpoint in private
Git, that publication and retention remain their separate workflow.

## Streaming jitter coverage

`jitter_coverage.py` performs a bounded, read-only aggregation of the private
archive's VS/PS evidence against an explicit shader mapping. It requires an
explicit archive, mapping file, and new output path outside the archive:

```sh
python -B tools/feedback_archive/scripts/jitter_coverage.py --archive /path/to/private-archive --mapping /path/to/temporal_scene.h --output /path/to/new/jitter-coverage.json
```

The stream is memory-bounded and uses a sample limit of 8 for representative
records. It is a coverage report, not the review ledger and not a current
player-count or acceptance metric. Feedback data remains private; the command
does not write to D1, publish Git data, or alter the production shader map.

## Export selected programs for source review

Create a selection JSON containing explicit 16-hex renderer hashes, for example:

```json
{"vs": ["09f67586057d7083"], "ps": ["2f606db52def4352"]}
```

Then export from the private archive to a new directory outside it:

```sh
python -B tools/feedback_archive/scripts/export_programs.py --archive /path/to/private-archive --selection /path/to/selection.json --output /path/to/new/program-review
```

The exporter requires exactly one source identity per selected stage/hash and
preflights payload length, SHA-256, and renderer byte-FNV before creating the
destination. `provenance.json` records the selection, source metadata, verified
identities, and limits; the output payloads are named `vs_HASH.bin` or
`ps_HASH.bin`. This is source verification only: the export does not translate
or compile shaders, edit the runtime map, or establish GPU pixels.

## Provenance and validation

Recovered from the private build-inputs repository at commit
`d4feb17917189a5b9f051b48b783f5fc62081658`: `scripts/archive_feedback.py`,
`scripts/feedback_ledger.py`, three synthetic test modules, and the
`skills/lo-feedback-triage/` source. This supersedes the older archive snapshot
`4dc0409`: the newer code adds schema 4 support, indexed pagination, query counters,
`--new-only`, the ledger, and its review workflow.

Migration removed personal account/database constants, implicit output under the
script's parent, and execution of a ledger implementation from the private data
checkout. The wrapper now requires an explicit archive; optional reviewed context
and seed preserve unknown state when absent. Historical live counts, deployment
status, private rows, real shaders, checkpoint/ledger/seed/context files, and the
scheduled commit/push workflow were not imported.

Run the synthetic suite once when this implementation changes:

```sh
python -B -m unittest discover -s tools/feedback_archive/tests -v
```

It exercises SQLite-backed pagination, payload validation, staged failure/retry,
ledger identities/review invalidation and the relocated CLI. It requires no token,
network, game, or private data. Passing it does not validate a live D1 deployment
or rendering behavior.
