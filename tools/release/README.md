# Release ZIP verification

`verify_package.py` checks a Windows release ZIP after packaging. Supply the
expected tag and full commit explicitly:

```sh
python tools/release/verify_package.py \
  --assets /path/to/release-assets --version v0.6.15 \
  --commit <40-character-commit> --output /path/to/package-verification.json
```

Use `--package /path/to/LostOdysseyRecomp-windows-x64-v0.6.15.zip` instead of
`--assets` to select one ZIP. The tool reads the ZIP and its adjacent
`.zip.sha256` sidecar. It checks the archive checksum, ZIP entry names, manifest
file list, formal version, clean source state, build and packaging commits, and
linked runtime provenance. It writes only the requested JSON report. Release
assets remain unchanged; the updater is neither extracted nor launched.

The archive checksum is read once. Payloads are not rehashed, and this tool
does not run the game or verify shader behavior. It handles the Windows ZIP
format; the Linux AppImage has no corresponding embedded ZIP manifest.
