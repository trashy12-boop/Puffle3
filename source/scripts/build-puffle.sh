#!/bin/sh
# Original multi-game browser, no bundled SWFs or forced auto-launch.
set -eu
cd "$(dirname "$0")/.."
mode=${1:-release}
case "$mode" in
 release) suffix=; RP_PLAYER_FEATURES=path-snapshots,avm1-fast-paths ;;
 diagnostic) suffix=_diagnostic; RP_PLAYER_FEATURES=path-snapshots,avm1-fast-paths,profile-vm ;;
 baseline) suffix=_v4_baseline; RP_PLAYER_FEATURES=path-snapshots,profile-vm ;;
 path-baseline) suffix=_path_baseline; RP_PLAYER_FEATURES=profile-vm ;;
 *) echo 'Usage: build-puffle.sh [release|diagnostic|baseline|path-baseline]' >&2; exit 2 ;;
esac
export RP_PLAYER_FEATURES
./scripts/get-ruffle.sh
./scripts/build-player.sh --release
name=Puffle3_01.09_v5${suffix}
rm -f "build/$name.raw.elf" "build/$name.elf"
make package TARGET="$name" TITLE_ID=RFLP00001 APP_TITLE="Puffle 3 Emulator" SFO_XML=assets/sfo-v5.xml
