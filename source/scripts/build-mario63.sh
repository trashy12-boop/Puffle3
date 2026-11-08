#!/bin/sh
# Build a real PS3 application. No game SWF is bundled.
set -eu
cd "$(dirname "$0")/.."
mode=${1:-release}
case "$mode" in
 release) flavor=experimental; RP_PLAYER_FEATURES= ;;
 diagnostic) flavor=diagnostic; RP_PLAYER_FEATURES=profile-vm ;;
 *) echo 'Usage: build-mario63.sh [release|diagnostic]' >&2;exit 2 ;;
esac
export RP_PLAYER_FEATURES
./scripts/get-ruffle.sh
./scripts/build-player.sh --release
name=SuperMario63_PS3_${flavor}_v3
# C defines, Rust features and title metadata must never reuse an old C link.
rm -f "build/$name.raw.elf" "build/$name.elf"
make package TARGET="$name" TITLE_ID=SM6300001 APP_TITLE="Super Mario 63 $flavor v3" \
 SFO_XML=assets/sfo-sm63.xml \
 CFLAGS_PS3='-std=gnu11 -O2 -g -Wall -Wextra -Werror -mcpu=cell -mhard-float -ffunction-sections -fdata-sections -DRP_MARIO63 -DRP_TITLE_ID=\"SM6300001\"'
