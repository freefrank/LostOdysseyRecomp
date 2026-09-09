# lzokay decompressor

Source: https://github.com/AxioDL/lzokay

Pinned commit: `db2df1fcbebc2ed06c10f727f72567d40f06a2be`.
The original MIT license is retained in `LICENSE` and copied into release packages.
Only the decompression portion of `lzokay.cpp` is compiled; compressor/encoder
implementation is omitted. The original public header is retained.

Local safety changes for bounded, potentially damaged game resource streams:

- Check the end of input while consuming a zero-byte extended length.
- Compare remaining input/output sizes before advancing pointers.
- Decode the little-endian word without an unaligned pointer dereference.
- Check lookbehind distance before constructing a pointer preceding output.

No game content is included. This is a static C++ dependency, with no Python or
additional runtime DLL requirement. Upstream source hashes and the exact local
delta are retained with the menu asset validation evidence.
