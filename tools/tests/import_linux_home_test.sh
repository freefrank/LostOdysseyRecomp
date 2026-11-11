#!/usr/bin/env bash
# Exercise production import paths in a disposable Fedora Atomic-style root.
set -euo pipefail

if ! command -v bwrap >/dev/null 2>&1; then
    echo 'SKIP: bubblewrap is required for the isolated /home layout'
    exit 77
fi
binary=$(realpath -- "$1")
root=(--unshare-user --die-with-parent
      --ro-bind /usr /usr --symlink usr/bin /bin
      --proc /proc --dev /dev --tmpfs /tmp --dir /var)
# Debian/Ubuntu retain /lib separately on older hosts; Arch uses /usr/lib.
for lib in /lib /lib64; do
    if [[ -e "$lib" ]]; then root+=(--ro-bind "$lib" "$lib"); fi
done
if ! bwrap --uid 0 --gid 0 "${root[@]}" /bin/true; then
    echo 'SKIP: this host does not allow unprivileged mount/user namespaces'
    exit 77
fi
for target in var/home /var/home; do
    bwrap --uid 0 --gid 0 "${root[@]}" --dir /var/home --symlink "$target" /home \
        --ro-bind "$binary" /LoImportGameTest \
        --setenv HOME /home/x --unsetenv XDG_DATA_HOME --unsetenv FLATPAK_ID \
        --chdir /tmp /LoImportGameTest --linux-home
done
for target in var/other-home /var/other-home; do
    bwrap --uid 0 --gid 0 "${root[@]}" --dir /var/home --dir /var/other-home --symlink "$target" /home \
        --ro-bind "$binary" /LoImportGameTest --chdir /tmp \
        /LoImportGameTest --linux-other-home
done
# A user-owned /home link is not a system alias, even with the expected target.
bwrap --uid 1000 --gid 1000 "${root[@]}" --dir /var/home --symlink var/home /home \
    --ro-bind "$binary" /LoImportGameTest --chdir /tmp \
    /LoImportGameTest --linux-other-home
# Nor may /var/home itself redirect the supposedly trusted target elsewhere.
bwrap --uid 0 --gid 0 "${root[@]}" --dir /var/other-home \
    --symlink other-home /var/home --symlink var/home /home \
    --ro-bind "$binary" /LoImportGameTest --chdir /tmp \
    /LoImportGameTest --linux-other-home
