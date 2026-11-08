# Puffle 3 — 01.08 v4

Install **Puffle3_01.08_v4.pkg** for normal use. This build opens the original multi-game selection screen. Its title ID is **RFLP00001**, updating the original Puffle installation. The separate v3 Mario-only application used SM6300001; v4 does not replace that separate title.

The font atlas now stores white RGB with glyph coverage in alpha. This fixes external-font labels turning black without changing the selection-screen design.

## Games, saves and controls

No game SWF is bundled. Put your legitimately obtained games in `/dev_hdd0/ruffle/games` (or the existing USB games directory). Keep Mario at the same path as before to retain the save/profile namespace, for example `/dev_hdd0/ruffle/games/SuperMario63.swf`. Cross launches the highlighted game; Square opens its controller settings; Start changes HDD/USB source. All original browser/settings functions remain.

Mario's default preset is selected for the exact basename `SuperMario63.swf`, allowing spaces, underscores, hyphens and capitalization differences. Other games retain their original generic defaults. Existing saved controller profiles override defaults and are preserved. If your older Mario profile uses different bindings, edit it through Square in the browser.

| Control | Mario default |
|---|---|
| Left stick / D-pad | Arrow keys / movement |
| Cross | Up / jump |
| Square | X / spin |
| Circle | Z / interact |
| Triangle | C / FLUDD |
| Start | P / pause |
| Select | Shift |
| Right stick | Flash mouse pointer |
| R1 / L1 | Left / right mouse click |
| L3 + R3 | Puffle overlay |

Generic games continue using left-stick mouse, Cross/R1 left click and Circle/L1 right click unless their saved profile says otherwise. Saves remain in `/dev_hdd0/ruffle/saves`, profiles in `/dev_hdd0/ruffle/profiles`. Back them up before manually changing game paths.

## Builds and diagnostics

`Puffle3_01.08_v4_diagnostic.pkg` enables VM/opcode/GC instrumentation and has additional overhead. Both packages update the same title; install one at a time. The normal package excludes those counters. Neither is claimed to achieve 30 FPS physical-console gameplay.

```sh
cd source
python3 scripts/bootstrap-linux.py /absolute/tools
. /absolute/tools/env.sh
scripts/build-puffle.sh release
# Or:
scripts/build-puffle.sh diagnostic
```

Builds pin Rust 1.99.1, PS3DEV/PSL1GHT and upstream Ruffle. Firmware, proprietary SDKs and game assets are not included. The normal and diagnostic builds enable `path-snapshots`; `scripts/build-puffle.sh baseline` disables that optimization for diagnostic comparisons. Legacy Mario-only build scripts are retained for development history; use build-puffle.sh for v4.

## Verification

```sh
scripts/test-portable.sh
cargo test --locked --release --manifest-path player/Cargo.toml \
  --features host-rsx-probe,profile-vm,path-snapshots -- --test-threads=1
```

The source includes an upstream patch plus an off/on feature switch. Read PERFORMANCE.md for actual results and remaining limits, and CHANGELOG.md for this iteration's changes.
