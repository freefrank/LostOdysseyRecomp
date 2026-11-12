#!/bin/sh
# Run in this directory in Alpine 3.21 with clang, musl-dev and binutils.
# Replace libfuse3.a with your rebuilt libfuse (source archive is included).
set -eu
clang -static-pie -Wl,--gc-sections -T data_sections.ld runtime.o \
  -L. -lsquashfuse -lsquashfuse_ll -lzstd -lz -lfuse3 -lmimalloc -o runtime
strip --strip-debug --strip-unneeded runtime
printf 'AI\002' | dd of=runtime bs=1 count=3 seek=8 conv=notrunc
