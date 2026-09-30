#!/usr/bin/env bash
# CI-only: modern C++23 tools on the AppImage's oldest supported glibc.
# Do not add a newer Ubuntu distribution's apt sources to obtain a compiler.
set -euo pipefail
. /etc/os-release
if [[ "$ID" != ubuntu || "$VERSION_ID" != 22.04 ]]; then
  echo 'AppImage release builds require Ubuntu 22.04 (glibc 2.35).' >&2
  exit 1
fi
: "${GITHUB_ENV:?This installer configures a GitHub Actions job}"
: "${GITHUB_PATH:?This installer configures a GitHub Actions job}"
sudo apt-get update
sudo apt-get install -y --no-install-recommends ca-certificates wget gnupg software-properties-common
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
key=$(mktemp)
trap 'rm -f "$key"' EXIT
wget -qO "$key" https://apt.llvm.org/llvm-snapshot.gpg.key
fingerprint=$(gpg --show-keys --with-colons "$key" | awk -F: '$1 == "fpr" {print $10; exit}')
if [[ "$fingerprint" != 6084F3CF814B57C1CF12EFD515CF4D18AF4F7421 ]]; then
  echo 'Unexpected LLVM apt signing key.' >&2
  exit 1
fi
sudo install -m 644 "$key" /usr/share/keyrings/lo-llvm.asc
printf '%s\n' 'deb [arch=amd64 signed-by=/usr/share/keyrings/lo-llvm.asc] https://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main' |
  sudo tee /etc/apt/sources.list.d/lo-llvm.list >/dev/null
sudo apt-get update
sudo apt-get install -y --no-install-recommends clang-18 clang-tools-18 lld-18 g++-13 binutils libfuse2
# The preinstalled runner may have several GCC versions. Select the headers
# explicitly so clang cannot silently switch to a newer, unrelated toolchain.
gcc_dir=$(dirname "$(g++-13 -print-libgcc-file-name)")
{
  echo 'CC=clang-18'
  echo 'CXX=clang++-18'
  echo "CFLAGS=--gcc-install-dir=$gcc_dir"
  echo "CXXFLAGS=--gcc-install-dir=$gcc_dir"
  echo 'LDFLAGS=-fuse-ld=lld'
} >> "$GITHUB_ENV"
echo '/usr/lib/llvm-18/bin' >> "$GITHUB_PATH"
[[ "$(getconf GNU_LIBC_VERSION)" == 'glibc 2.35' ]]
clang++-18 --version
