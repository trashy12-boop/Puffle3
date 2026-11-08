# Puffle3

**An experimental, native PlayStation 3 port of [Ruffle](https://github.com/ruffle-rs/ruffle), an open-source Adobe Flash Player emulator.** Puffle3 aims to make classic Flash `.swf` games playable on real PS3 hardware using a custom PS3 frontend, controller mapping, and a ported Ruffle runtime.

**⚠️ Extremely experimental:** Puffle3 is not a finished or fully compatible Flash player. Some games boot and play well, others run at very low FPS, show graphical differences, freeze, or fail completely. **Playable menus do not necessarily mean playable gameplay.** Keep backups of your saves.

## Tested on actual PS3 hardware

These results were reported by the project owner while testing Puffle3 on a real PS3. They describe observed performance, **not a guarantee for every version, area, or console**.

| Flash game | Observed behavior |
|---|---|
| **Super Mario 63** | Title screen and menus run smoothly, with minor visual differences. Actual gameplay runs at approximately **8–10 FPS**. |
| **The Right Mix** | Runs **very well throughout most of the game**, with occasional slowdowns/stuttering in certain scenes. |
| **Bullet Bill** | Introduction and main menu run normally, but gameplay performs **very poorly**. |
| **Koopa's Revenge** | Extremely slow gameplay, around **2 FPS**. Currently not practically playable. |

**Current development snapshot: 01.09 v5.** The archive's technical measurements distinguish host/RPCS3 diagnostics from physical-console results; **v5 has not been measured on real PS3 hardware**, so the reports above should not be interpreted as verified v5 benchmarks. The target of smooth, consistently playable Flash games is not yet achieved.

## Download and install

Install the normal `Puffle3_01.09_v5.pkg` build on a PS3 with homebrew support (for example, HEN). Title ID: **`RFLP00001`**. This updates the original multi-game Puffle installation. The optional `_diagnostic.pkg` is intended for debugging and may be slower. It shares the same title ID, so install only one variant at a time.

Puffle3 **does not include any Flash games**. Copy your own legally obtained `.swf` files to:

```text
/dev_hdd0/ruffle/games/
```

You can also browse an existing supported USB games folder. The built-in browser supports choosing SWFs, folders, switching HDD/USB sources, and saving per-game control profiles.

## Controls

### Game browser

| PS3 control | Action |
|---|---|
| **D-pad** | Navigate games and folders |
| **X (Cross)** | Open a folder / launch selected SWF |
| **O (Circle)** | Go to the parent folder |
| **□ (Square)** | Edit controls for the selected game |
| **Start** | Switch between HDD and USB sources |
| **L3 + R3 (in game)** | Open the pause/settings overlay |

### Default controls inside games

Controls can be customized and saved **per SWF**. Generic games use left-stick mouse movement, X/R1 for left click and O/L1 for right click. For a game named `SuperMario63.swf` (also recognizing common punctuation/case differences), the current build supplies a Mario-specific default:

| PS3 control | Super Mario 63 default |
|---|---|
| **Left stick / D-pad** | Movement / arrow keys |
| **X (Cross)** | Up / jump |
| **□ (Square)** | X / spin |
| **O (Circle)** | Z / interact |
| **△ (Triangle)** | C / FLUDD |
| **Start** | P / pause |
| **Select** | Shift |
| **Right stick** | Mouse pointer |
| **R1 / L1** | Left / right mouse click |
| **L3 + R3** | In-game overlay |

Saved custom control profiles take priority over the defaults.

## Features and current limitations

Puffle3 uses the actual **Ruffle Flash runtime**, including AVM1/AVM2 interpreter components, SWF decoding, timeline playback, rendering, input events, and audio support. The port also provides a PS3 game browser, PS3 controller integration, per-game profiles, and local Flash SharedObject save storage.

The renderer uses CPU-side graphics processing and PS3 RSX presentation, with experimental optimization work involving draw caching, batching, memory management, and interpreter-path caching. Graphics are **not perfectly equivalent to desktop Flash/Ruffle**. Performance is especially limited by demanding ActionScript execution and garbage collection; unsupported Flash features and sound/input issues remain possible.

**What is not promised:** complete game compatibility, full-speed gameplay, perfect graphics, crash isolation, online Flash services, or a guaranteed 30/60 FPS experience.

## Saves and settings

```text
/dev_hdd0/ruffle/games/       Your SWF files
/dev_hdd0/ruffle/saves/       Flash SharedObject saves
/dev_hdd0/ruffle/profiles/    Per-game controller profiles
```

A game's path affects its save/profile identity. Preserve paths when updating, and back up important data.

## Source code and building

This project is **source available under its respective licenses**. The development archive contains the PS3 frontend, native C/Rust player, build/test scripts, pinned Ruffle patches, licenses, and validation records.

On Linux, a rebuild requires the pinned Rust toolchain, PS3DEV/PSL1GHT toolchain, the appropriate upstream Ruffle source and dependencies. The development archive documents:

```sh
cd source
python3 scripts/bootstrap-linux.py /absolute/tools
. /absolute/tools/env.sh
scripts/build-puffle.sh release
# For additional diagnostics:
scripts/build-puffle.sh diagnostic
```

Build dependencies, the proprietary console SDK/firmware, and third-party game SWFs are **not** included. The complete current C/Rust source, vendored build dependencies, locks, patches and tests are in [`source/`](source/). See [build instructions](source/docs/BUILD_CURRENT.md), [benchmark reproduction](source/docs/BENCHMARKING.md), and [v5 results](source/docs/PERFORMANCE_V5.md). [`v4-source/`](v4-source/) is a historical partial snapshot.

## Credits, licensing and development disclosure

- **[Ruffle contributors](https://github.com/ruffle-rs/ruffle)**: Original open-source Flash emulator and its runtime.
- **[PS3ME](https://github.com/trashy12-boop/PS3ME)** and **[PS2ME](https://github.com/Wellinator/PS2ME)**: PS3/PS2 project UI code and foundations used in this port's frontend.
- **[Salsa64 / rsxkit](https://github.com/ricowp81/Salsa64)** and other original developers: PS3 graphics/tooling components, with their own notices and licenses.
- **Trashy12** is the project owner and directs the project, integrates changes, and tests builds on PS3 hardware.
- **ChatGPT / OpenAI coding tools** assisted with implementation, C and Rust code generation and adaptation, debugging, optimization, and documentation. Runtime code is written in C and Rust; Shell is used for setup, build, and publishing scripts.

## Languages

The PS3 frontend and platform code use C, the Flash runtime integration and optimized AVM1 code use Rust, and Shell scripts automate builds and publishing. GitHub's language bar is generated from tracked source files; it does not identify project authorship.

## Licensing

Puffle3 combines code under different licenses. The PS3ME-derived native frontend is GPL-2.0-only; Puffle3-authored portions retain the MIT terms in `source/LICENSE`; Ruffle, rsxkit, fonts, and other dependencies retain their respective upstream licenses. See [`LICENSE`](LICENSE), [`COPYING.PS3ME`](COPYING.PS3ME), [`source/NOTICE.md`](source/NOTICE.md), and the complete third-party notices included in the development archive before redistributing builds or source. This is an independent homebrew project, **not affiliated with or endorsed by Sony, Adobe, Ruffle or the original game developers**.

*Flash game files are not distributed with Puffle3.*
