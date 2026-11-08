#!/bin/sh
# Original multi-game browser, no bundled SWFs or forced auto-launch.
set -eu
cd "$(dirname "$0")/.."
mode=${1:-release}
case "$mode" in
 release) suffix=; RP_PLAYER_FEATURES=path-snapshots ;;
 diagnostic) suffix=_diagnostic; RP_PLAYER_FEATURES=path-snapshots,profile-vm ;;
 baseline) suffix=_baseline; RP_PLAYER_FEATURES=profile-vm ;;
 *) echo 'Usage: build-puffle.sh [release|diagnostic|baseline]' >&2; exit 2 ;;
esac
export RP_PLAYER_FEATURES
./scripts/get-ruffle.sh
./scripts/build-player.sh --release
name=Puffle3_01.08_v4${suffix}
rm -f "build/$name.raw.elf" "build/$name.elf"
make package TARGET="$name" TITLE_ID=RFLP00001 APP_TITLE="Puffle 3 Emulator" SFO_XML=assets/sfo-v4.xml
