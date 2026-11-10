#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
revision=1125b0018c110fb7a1e51f080c7b47d5dc94d64f
archive_sha=51ba04f95c1c90d5a895c82e6d25c0e857aaabac11ed6c11464be890189525e5
if [ ! -d deps/ruffle ]; then
    mkdir -p deps
    archive=$(mktemp)
    trap 'rm -f "$archive"' EXIT HUP INT TERM
    curl -fL --retry 2 "https://codeload.github.com/ruffle-rs/ruffle/tar.gz/$revision" -o "$archive"
    printf '%s  %s\n' "$archive_sha" "$archive" | sha256sum -c -
    mkdir deps/ruffle
    tar --no-same-owner -xzf "$archive" --strip-components=1 -C deps/ruffle
    printf '%s\n' "$revision" > deps/ruffle/.puffle-upstream-revision
elif [ -d deps/ruffle/.git ]; then
    test "$(git -C deps/ruffle rev-parse HEAD)" = "$revision"
else
    test "$(cat deps/ruffle/.puffle-upstream-revision)" = "$revision"
fi
mkdir -p deps/ruffle/.puffle-patches
for name in ruffle-ps3-05.patch ruffle-ps3-06.patch ruffle-ps3-08-profile.patch ruffle-ps3-09-path-snapshots.patch ruffle-ps3-10-avm1-fast-paths.patch ruffle-ps3-11-direct-dispatch.patch; do
    patch_file=$(pwd)/scripts/$name
    digest=$(sha256sum "$patch_file" | cut -d ' ' -f1)
    marker=deps/ruffle/.puffle-patches/$name
    if [ -f "$marker" ]; then
        test "$(cat "$marker")" = "$digest" || { echo "Changed patch: $name; use a fresh deps/ruffle checkout." >&2; exit 1; }
        echo "$name already applied (recorded)."
    elif patch -d deps/ruffle -p1 -R --dry-run -s < "$patch_file" >/dev/null 2>&1; then
        printf '%s\n' "$digest" > "$marker"
        echo "$name already applied."
    elif patch -d deps/ruffle -p1 --dry-run -s < "$patch_file" >/dev/null 2>&1; then
        patch -d deps/ruffle -p1 --forward < "$patch_file"
        printf '%s\n' "$digest" > "$marker"
    else
        echo "Patch conflicts: $name. Existing source was preserved; use a fresh deps/ruffle checkout." >&2
        exit 1
    fi
done
echo "Ruffle revision: $revision + Puffle patches and optional diagnostics"
