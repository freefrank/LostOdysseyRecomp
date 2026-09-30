# General runtime texture replacement: remaining work

## Implemented versus pending

PR #68 connects LOTEX1 image replacements to the native settings-menu asset decoder. This path already knows the UE3 package, export index and object name. It can resolve canonical keys before decoding the original menu/font-page image.

The general guest renderer is not connected to this identity resolver. In particular, a successfully packed `DefaultTexture`, world texture or controller-icon atlas does not prove that the corresponding game draw can use the replacement. The native menu path and original guest rendering are different consumers.

## Why the next boundary is texture upload

The game reads packages from FPD archive extents described by `LO.fpi`. A package name is not necessarily an independently opened host file. Replacing an embedded image by redirecting an arbitrary archive read would require preserving package/archive structure and lengths.

The renderer's `GetTexture()` path instead reads Xenos fetch state, locates guest texture memory, performs tiling/endian conversion and uploads a host texture. An ordinary 2D upload is the candidate interception point. Render targets and resolved surfaces must remain outside that replacement path.

## Identity bridge that still needs implementation

A Xenos fetch does not carry the extractor's package/export/object key. General replacement therefore requires a verified bridge between the canonical identity and runtime content. Do not infer identity from a guest physical address, filename hash, texture dimensions alone or a PNG file checksum.

The extractor and renderer should share a versioned fingerprint definition over the same linear block bytes plus source format and logical dimensions. BC textures should not require CPU pixel decompression solely for identification. Mip selection, pitch padding, tiling/endian rules and collision/ambiguity handling must have common test vectors before enabling the bridge.

If identical source bytes represent multiple canonical objects with different requested replacements, the runtime must reject/diagnose the ambiguity or obtain additional identity information. Picking an arbitrary object would violate the package-key contract.

## Acceptance gates

1. Emit verified runtime fingerprints alongside extractor identities and reject unsupported layouts.
2. Resolve those fingerprints to canonical requests without per-draw filesystem scans.
3. Validate LOTEX1 payloads and integrate ordinary 2D uploads without altering guest archives.
4. Exclude render targets, resolved surfaces, unsupported cube/3D layouts and ambiguous matches.
5. Invalidate host uploads safely on mod generation changes, keeping guest fetch cache identity intact.
6. Verify descriptor dimensions, texel-offset/GetDimensions behavior and mip handling before permitting larger physical textures.
7. Observe a replacement upload, a draw sampling it, and the intended on-screen change as separate checks.

LOTEX1 remains the existing image format. Earlier draft documentation proposing `LOTX` headers, a `textures/<fingerprint>.lotex` directory or a shared overlay manifest is superseded; none of those alternative formats is a supported v1 ABI.

No general high-resolution texture replacement or whole-game font/model/movie replacement is claimed by this document.
