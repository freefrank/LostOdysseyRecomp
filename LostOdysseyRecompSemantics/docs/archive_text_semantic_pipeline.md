# Archive and text semantic recovery

This continuation uses the address-level findings consolidated in commit
72a5abdb6310e9ba43e232a19b6ab73706e79e23. Findings guide target selection;
they do not count as implementations or runtime validation. The implementations
remain logical/ABI work intended to support a later Rust port.

## Archive descriptor versus entry tree

Direct recovery of 82853208 distinguishes two layouts that the original ledger
worded together:

- 82854400 advances archive descriptors by 48 bytes. It invokes 82853208 on each
  descriptor before relocating descriptor pointers and processing its entry tree
- 82853208 swaps the halfword at +2 and words at +4, +8, +12, +16, +20, +24,
  +28, +32 and +36. It leaves the remaining fields untouched
- 82853A10 walks 24-byte entries. With r6 == 0 it swaps +0/+4/+8, halfword +12,
  and +20. Directory entries (flag 0x10000000) additionally swap halfwords +14
  and +16; other entries swap word +16
- Both endian paths relocate a directory's +20 child index using the archive's
  +4 entry-table pointer, then recurse for the +14 halfword child count. This is
  a load-time transformation, not an idempotent general-purpose byte swap
- 828571B8 returns header byte +3 multiplied by 2048 for the CPX reserve size

A small fixture checks descriptor fields and untouched bytes, nested directory
relocation in both source-endian modes, and CPX reserve units. The full library
build passes. No archive file-I/O, decompression or gameplay acceptance is claimed
by these helpers. Private generated guest bodies remain outside the publication;
recovery draft metadata records body hashes and source locations.

## Next connected work

Follow the archive loader/member-read and CPX decoder chains, and the text-bank
parser/consumer chain documented in the ledger. Verify each layout from the guest
body and callers rather than assuming all documentary labels are exact symbols.

## CPX block decoder

CPX block decoding now composes bit input, parameter tables, match lengths and literal/overlapping-back-reference output in both halfword and byte modes. Eight tiny blocks validate stored/no-copy and all three distance modes, with byte counts and block counters. The complete archive streaming/in-place orchestration remains pending; native-asset and gameplay acceptance are not claimed.

## CPX context and block index

CPX context recovery now owns the copied header/index, exposes block offsets and completion, reuses lazy 65552-byte scratch and releases its owned buffers. A two-block synthetic file runs indexed lookup through the recovered decoder with exact output and full cleanup. Lazy manager initialization remains an explicit guest boundary and preserves the requested allocation arguments. The asynchronous reader and registry integration remain pending.

## Text banks and menu lookup

The documented text-bank chain now decodes offset-based UTF-16/narrow string tables, constructs one-column and seven-column (84-byte) records, releases consumed input/temporary strings and resolves menu text IDs through 60-byte rows. Existing recovered string assignment is reused through allocator callbacks. Small fixtures verify ignored length fields, empty/disabled records, replacement ownership and menu fallback. This is logic recovery, not a completed localization importer or game-runtime integration.

## Packed archive names and candidate selection

Archive names now expand packed base-40 halfwords through the guest alphabet, fold ASCII case and append descriptor suffixes/extensions. Prefix candidate selection preserves path boundaries, exact-match early exit and the original low-byte insertion ordering. A synthetic guest-table fixture validates the connected path without publishing private tables. Full member lookup and native archives remain pending.
The name pool is packed base-40 data, not a UTF-16 string array. Name descriptors use a low 18-bit pool index, a 5-bit suffix index at bit 18 and a 5-bit extension index at bit 23. Alphabet and suffix data remain guest-owned.

## Archive metadata

Archive metadata now decodes the packed year/month/day/time fields, bridges them to the guest FILETIME conversion and applies the original guest-pattern suffix rewrite. The weekday calculation preserves the source single month-table load; the bridge intentionally omits that weekday field. A focused fixture checks field order, success/failure output handling and suffix matching. Kernel calendar/error services remain explicit guest boundaries.

## Archive member lookup

Archive lookup now composes candidate prefix selection with recursive 24-byte entry matching, decoded names, packed timestamps and output metadata. Focused synthetic checks cover regular files, directories (type 16), exact archive matches (type 17), loose members (type 0 with flag 4), path append and misses. Guest time conversion remains an explicit boundary; no archive file I/O or gameplay acceptance is claimed.

## Overlay archive lookup

The archive lookup entry now normalizes case, slash and hyphen spelling, searches overlay archives in configured order under guest lock boundaries, and falls back to the base archive. It composes the recovered prefix/member lookup and preserves the original 240-byte output initialization. Synthetic overlay success and miss-to-base cases pass. Overlay root formatting and kernel locking/time services remain guest boundaries, not recovered filesystem behavior.

## CPX stream orchestration

CPX stream orchestration now composes the recovered block decoder and context helpers with split reserve handling, chunked reads, time-budget yields and completion cleanup. Registry unlink and context shutdown are also recovered. Two small synthetic paths cover a split-reserve two-block stream and a plain read; a separate lifecycle fixture checks linked-list removal and ownership. Runtime I/O, clock, allocator, registry construction and atomic status exchange stay explicit service boundaries. Cross-boundary scratch assembly and allocation failures are source-reviewed only; this is not native archive or multithreaded acceptance.
