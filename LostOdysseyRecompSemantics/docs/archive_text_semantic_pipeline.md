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
