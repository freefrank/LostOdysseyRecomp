#!/usr/bin/env bash
# Build the shipped runtime independently of the host's glibc and FUSE setup.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
destination=${1:-"$root/out/tools/appimage-runtime"}
mkdir -p "$destination"
docker build --platform linux/amd64 \
  --output "type=local,dest=$destination" \
  "$root/packaging/linux/appimage-runtime"
chmod +x "$destination/runtime-x86_64"
"$destination/runtime-x86_64" --appimage-version
sha256sum "$destination/runtime-x86_64"
