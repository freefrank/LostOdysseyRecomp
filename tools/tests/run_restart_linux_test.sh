#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
binary="$(mktemp /tmp/lo-restart-fixture-XXXXXX)"
trap 'rm -f "$binary"' EXIT
"${CXX:-g++}" -std=c++20 -O2 -pthread -I "$root/LostOdysseyRecomp" \
    "$root/tools/tests/restart_linux_fixture.cpp" -o "$binary"
"$binary"
