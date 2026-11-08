#!/usr/bin/env bash
# Uploads Puffle3's complete source ZIP to the existing GitHub repository.
# Requires Git, unzip, and authenticated GitHub push access.
set -Eeuo pipefail
ZIP_PATH="${1:-Puffle3_Source_Code_v4.zip}"
REPO_URL="${2:-https://github.com/trashy12-boop/Puffle3.git}"
if [[ ! -f "$ZIP_PATH" ]]; then
  echo "ZIP not found: $ZIP_PATH" >&2
  exit 1
fi
for cmd in git unzip; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
done
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
unzip -q "$ZIP_PATH" -d "$WORK_DIR/archive"
SRC="$WORK_DIR/archive/Puffle3_Source_v4"
[[ -f "$SRC/Makefile" && -f "$SRC/player/Cargo.toml" && -f "$SRC/src/ps3_main.c" ]] || {
  echo "Unexpected ZIP layout." >&2
  exit 1
}
git clone "$REPO_URL" "$WORK_DIR/repo"
cd "$WORK_DIR/repo"
rm -rf source
mkdir -p source
cp -a "$SRC/." source/
git add -A source
if git diff --cached --quiet; then
  echo "Source tree is already up to date."
  exit 0
fi
git commit -m "source: publish complete Puffle3 v01.08 v4 source tree"
git push origin HEAD:main
echo "Published to ${REPO_URL%.git}/tree/main/source"
