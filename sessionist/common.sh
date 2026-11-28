# Shared helpers for the Sessionist build scripts. Sourced, not run.

# apply_patch_stack <repo> <patch>...: keep <repo> at its pinned commit plus
# exactly these patches, applied in order. A fingerprint of the commit and the
# patch contents is kept in the repo's git dir; when it matches, nothing runs.
# Otherwise the repo is reset to its commit and the whole stack is reapplied
# (later patches overlap earlier ones, so they cannot be checked one by one).
apply_patch_stack() {
  local repo="$1"; shift
  local stamp want p
  stamp="$(git -C "$repo" rev-parse --absolute-git-dir)/sessionist-patches"
  want=$( { git -C "$repo" rev-parse HEAD; for p in "$@"; do basename "$p"; shasum -a 256 < "$p"; done; } | shasum -a 256 | cut -d' ' -f1)
  if [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$want" ]; then
    echo "patches up to date: $repo"
    return 0
  fi
  echo "resetting $repo and applying $# patch(es)"
  git -C "$repo" checkout -- .
  git -C "$repo" clean -fdq
  for p in "$@"; do
    git -C "$repo" apply --whitespace=nowarn "$p" || { echo "[RED] $(basename "$p") does not apply to $repo"; return 1; }
    echo "applied: $(basename "$p")"
  done
  echo "$want" > "$stamp"
}

prepare_dependencies() {
  git submodule update --init --recursive
  git -C thirdparty/plume submodule update --init --recursive
  apply_patch_stack tools/XenonRecomp "$PWD/tools/patches/XenonRecomp-lostodyssey.patch"
  apply_patch_stack thirdparty/plume \
    "$PWD/tools/patches/plume-lostodyssey.patch" "$PWD/tools/patches/plume-sdl3.patch" \
    "$PWD/tools/patches/plume-macos.patch" "$PWD/tools/patches/plume-ios.patch"
}
