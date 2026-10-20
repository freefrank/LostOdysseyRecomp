# Match the upstream musl build, with an immutable base and source archive.
FROM alpine:3.21.0@sha256:21dc6063fd678b478f57c0e13f47560d0ea4eeba26dfc947b2a4f81f686b9f45 AS build
RUN apk add --no-cache \
    bash alpine-sdk util-linux file autoconf automake libtool xz \
    eudev-dev gettext-dev linux-headers meson \
    zstd-dev zstd-static zlib-dev zlib-static clang musl-dev mimalloc-dev
WORKDIR /build
RUN wget -O runtime.tar.gz https://codeload.github.com/AppImage/type2-runtime/tar.gz/8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa \
    && echo 'c449eb42c2190820872631f5e22f8e15b1e985a36dc4a9e2de3585bb7536c785  runtime.tar.gz' | sha256sum -c - \
    && tar -xzf runtime.tar.gz --strip-components=1
# Retain the LGPL source and license from the upstream dependency installer.
RUN mkdir -p /deliver/licenses /deliver/relink \
    && cp scripts/common/install-dependencies.sh /build/install-dependencies.original \
    && sed -i '/tar xf fuse-3.\*.tar.xz/a cp fuse-3.15.0.tar.xz /deliver/relink/; cp fuse-3.15.0/LGPL2.txt /deliver/licenses/libfuse.txt' scripts/common/install-dependencies.sh \
    && sed -i '/tar xf 0.5.2.tar.gz/a cp squashfuse-0.5.2/LICENSE /deliver/licenses/squashfuse.txt' scripts/common/install-dependencies.sh \
    && bash scripts/common/install-dependencies.sh
COPY automatic-extract.patch /build/automatic-extract.patch
RUN patch -p1 < automatic-extract.patch \
    && printf '%s\n' '8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa-lo-fallback-1' > src/runtime/version \
    && bash scripts/build-runtime.sh \
    && cp out/runtime-x86_64 /deliver/ \
    && cp LICENSE /deliver/licenses/type2-runtime.txt \
    && cp /build/install-dependencies.original scripts/common/install-dependencies.sh \
    && apk info -v > /deliver/build-packages.txt
# Supply source and a relinkable object for the statically linked LGPL libfuse.
RUN cp -r src patches scripts /deliver/relink/ \
    && cp LICENSE /deliver/relink/LICENSE \
    && cd src/runtime \
    && clang -std=gnu99 -Os -D_FILE_OFFSET_BITS=64 \
       -DGIT_COMMIT='"8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa-lo-fallback-1"' \
       -ffunction-sections -fdata-sections -fPIE \
       -I/usr/local/include/squashfuse -I/usr/include/fuse3 -c runtime.c -o /deliver/relink/runtime.o \
    && cp /usr/local/lib/libsquashfuse*.a /usr/lib/libfuse3.a /usr/lib/libzstd.a /usr/lib/libz.a /usr/lib/libmimalloc.a /deliver/relink/ \
    && cp data_sections.ld /deliver/relink/
COPY licenses/ /deliver/licenses/
COPY relink.sh /deliver/relink/relink.sh
COPY Dockerfile automatic-extract.patch /deliver/relink/
RUN sha256sum /build/automatic-extract.patch > /deliver/patch.sha256 \
    && tar -C /deliver -czf /deliver/runtime-relink.tar.gz relink \
    && rm -r /deliver/relink \
    && cd /deliver && sha256sum runtime-x86_64 > runtime.sha256

FROM scratch
COPY --from=build /deliver/ /
