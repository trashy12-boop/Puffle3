# Historical README through 01.08 v4

This retained text describes earlier snapshots. Use BUILD_CURRENT.md and PERFORMANCE_V5.md for the current build.

> Current development snapshot: Puffle 3 01.08 v4. The original multi-game browser is retained, external-font text is fixed, and optional validated AVM1 path snapshots reduce repeated allocation. See [README_FIRST.md](../README_FIRST.md) and [PERFORMANCE.md](../PERFORMANCE.md) for builds, measurements and limitations.

# Puffle 3 Emulator 01.07 — development checkpoint

Read docs/STATUS_STOPPED_01.07.md first. Final game benchmarks were interrupted; this is not a validated performance release.

# Puffle 3 Emulator 01.06 development build

See docs/CHANGES_01.06.md and docs/BUILD_01.06.md for this build. Historical 01.05 reports describe earlier artifacts and are not evidence that 01.06 meets the requested performance goal. Native RPCS3 measurements accompany the 01.06 delivery. Real PS3 performance remains unmeasured for 01.06.

> Native measurement update: see `docs/NATIVE_MEASUREMENTS_2026-11-01.md`. Executables now ran in RPCS3. Mario failed an allocation; Right Mix remains incorrect in both renderers. No practical FPS gain is established. This source includes allocation-failure diagnostics and is not a validated stable release.

# Puffle 3 Emulator 01.05 development build

See [the 01.05 build guide](docs/BUILD_01.05.md) and [current validation report](docs/VALIDATION_01.05.md) first. This is newly compiled source, not a rebranded 01.04 binary. A large real-PS3 performance increase and complete Right Mix RSX fix are not established.

The app bundles its own DejaVuSans font. In per-game Controller settings, select **Renderer: Software** to bypass experimental RSX drawing; the setting is saved per SWF. Default mouse controls are **left stick to move the pointer**, **Cross or R1 for left click**, **Circle or L1 for right click**. L3+R3 opens the in-game menu. Existing profile records remain readable.

The older documentation below is retained as historical context; it does not override the 01.05 validation status.

# Ruffle PS3 — experimental native player 0.4 / 01.03

This build runs the actual Ruffle core on the PS3 PPE. It replaces the earlier
menu-only preview. Both ActionScript interpreters are compiled, and the native
RPCS3 executable has executed AVM1 interaction and AVM2 trace fixtures.
**This is an early player, not a finished or broadly validated Flash port.**
The user reports Super Mario 63, Bullet Bill and The Right Mix boot on a physical
PS3 in 0.2, with extremely poor performance. Version 01.03 adds an AltiVec path and an exact draw cache. Supplied games
have host replay tests and RPCS3 startup checks; there is no physical-console
performance measurement for 01.03. See docs/PERFORMANCE_04.md.

## Install and play

1. Enable HEN if applicable, then install the PKG through Package Manager.
   It replaces title `RFLP00001`; the XMB title is now **Ruffle PS3**.
   The SFO application version is 01.03 to supersede the preview's 01.00.
2. Put your own SWFs in `/dev_hdd0/ruffle/games/`, or in
   `/dev_usb000/ruffle/games/` on a compatible USB drive.
3. Select a SWF with the D-pad and press **Cross to play**.
4. Press **L3 + R3 together** to pause. The menu offers Resume, Restart SWF,
   Controller settings, and Return to browser. Circle also resumes.

The game logic and PS3 audio port pause while this menu or its control editor
is open. Normal frame timing resumes afterward. Opening the chord releases
mapped buttons and suppresses input until the controls are neutral.

Browser controls: Cross opens a folder or SWF; Circle goes to the parent;
Start switches HDD/USB; Square opens the selected SWF's controller profile;
Select opens the old diagnostic input screen. The browser supports subfolders,
256 entries, and USB device `dev_usb000`.

Default game mappings include Cross/R1 = left mouse, Circle/L1 = right mouse,
Square = Z, Triangle = C, D-pad = arrows, Start = Enter. Left stick moves the
virtual mouse. In the profile editor, Up/Down selects, Left/Right or Cross
changes a value, L1/R1 chooses one of three simultaneous binding slots,
and Circle returns. Scrolling past the button/stick-direction rows reaches
cursor stick selection, speed, dead zone, and acceleration. Changes save to
`/dev_hdd0/ruffle/profiles/` automatically. Full SWF paths identify profiles.
Moving a SWF creates a separate profile and save namespace.

SharedObjects save under `/dev_hdd0/ruffle/saves/`. The Flash save key and the
SWF's full path are checked in each stored record. Maximum one MiB per object;
there is no global save quota or encryption. Logs are
`/dev_hdd0/ruffle/ruffle-core.log` and `ruffle-ps3-dev.log`.

## Implemented and limited

* Real upstream SWF decoding, timeline, AVM1/AVM2, events, embedded assets and
  software audio mixer. AS1/2 use AVM1; AS3 uses AVM2. Compiling an interpreter
  does not establish compatibility with every game using that version.
* CPU vector rasterization through tiny-skia: cached paths, solid fills,
  quadratic/cubic curves, basic strokes, gradients, bitmaps, normal alpha,
  masks, and bitmap offscreen rendering. RSX uploads and scales the result.
  This is **CPU rendering with RSX presentation**, not accelerated vector RSX
  rendering. Internal resolution is fixed at 320×240, displayed with 4:3 fit
  and bilinear scaling. Output mode selection is rsxkit's automatic default.
* Ruffle PCM/ADPCM/MP3 audio mixing to 48 kHz stereo floating-point PS3 output.
  Main-thread polling services a 16-block audio ring (about 85 ms). Heavy
  scripts/rendering can cause underruns. Nellymoser and AAC are not enabled.
* Keyboard and mouse events, per-game remapping, real pause/restart/exit, and
  persistent SharedObjects. Network and external asset loading are unavailable.

Known graphics gaps: Stage3D, PixelBender, filters, advanced blend modes,
linear-RGB gradient interpolation, focal gradient positioning, exact Flash
stroke scaling and all bitmap-fill color transforms are incomplete. Unsupported
blend modes currently render as normal source-over; focal gradients use a
centered radial approximation. Pixel snapping is basic. UI glyphs show dark
background boxes in the tested software OpenGL RPCS3 setup.

The supplied Mario, Bullet Bill and Right Mix SWFs have been exercised in
host replays and RPCS3 startup scenes. These are not proof of full game
compatibility or physical-console performance. No real-console FPS or audible
output has been measured by the developer. Favorites, recent games, thumbnails,
global/per-game graphics menus, output/resolution selection, volume menu,
performance presets, frame skipping, SPU work, JIT, and Flash video remain
unfinished. Some games will fail or render incorrectly.

File size is limited to 16 MiB, declared uncompressed size to 32 MiB, and the
Rust heap to 128 MiB of requested payload. Bitmap dimensions are bounded.
The heap counter excludes C allocations, allocator overhead, executable,
stack and RSX memory. A failing allocation or Rust panic can terminate the
app: this build does **not** provide complete faulty-SWF isolation. Script
execution has Ruffle's two-second limit. See `docs/TEST_RESULTS.md`.

## Build

Use Linux, a PS3DEV/PSL1GHT SDK with PPU compiler and packaging tools, the NVIDIA
Cg toolkit for rsxkit shaders, and **Rust 1.99.1 with rust-src**. This session
used the PS3DEV `nightly-2026-10-12` Linux release, GCC 13.2.0, and PSL1GHT
`f649a08fd536a9e27c08c7db2d93a2d7ee4c3bbe`. Set `PS3DEV`, `PSL1GHT`, and
PATH for the SDK. Make libCg.so available through LD_LIBRARY_PATH if needed.

Use a dedicated RUSTUP_HOME: the runtime script patches that toolchain's
rust-src. It is not a general-purpose PS3 standard library. Rust code must
remain on one PPE thread; unsupported filesystem/network/process/thread APIs
remain unavailable. The C frontend provides platform services instead.

```sh
rustup toolchain install 1.99.1 --profile minimal --component rust-src
rustup override set 1.99.1
./scripts/get-ruffle.sh
./scripts/build-player.sh --release
make package
```

The Ruffle checkout is pinned to `1125b0018c110fb7a1e51f080c7b47d5dc94d64f`.
Ruffle includes the pinned PS3 patches in scripts/; tiny-skia 0.11.4 is vendored with portable SIMD changes; the new frontend, renderer, runtime and backend
modules are in this archive. Cargo.lock pins the resolved dependencies.
Rebuilding requires fetching upstream source and dependencies; those and the
SDK/firmware/proprietary Cg binaries are not duplicated in this archive.

Host checks:

```sh
python3 tests/make_interactive_swf.py
cargo test --manifest-path player/Cargo.toml
# The original C sanitizer and headless upstream regressions:
./scripts/test-host.sh
```

The small original fixtures and their generator are CC0 test material, included
only in the source archive, not in the installable PKG. No copyrighted game is
bundled. `validation/` preserves the sandbox RPCS3 test procedure and evidence.

## Licensing

The combined native application is GPL-2.0-only because its menu adapts PS3ME
`ps3/src/InGameMenu.cpp`; see `COPYING.PS3ME` and the preserved reference in
`vendor/ps3me-reference/`. Other original modules retain MIT licensing.
Salsa64's rsxkit is MIT and retains its notices. Ruffle and Rust components
retain their upstream licenses; tiny-skia uses BSD-style licensing. The
`licenses/` directory includes downloaded dependency notices, Ruffle/SWF
notices and Noto font licensing. See `NOTICE.md` and `LICENSE`.

## Performance revision 0.3

See [the performance notes](docs/PERFORMANCE_03.md). Ordinary bitmap draws borrow
source pixels when no color transform is needed. Repeated presentation of the
same SWF frame reuses the last texture. Main-loop updates use Ruffle frame/timer
deadlines, capped at 16 ms between updates, and retain real elapsed time. Input
and audio polling continue between ticks. Resolution, antialiasing and interpreter
semantics are unchanged. The pause menu retains the last timing sample; periodic
samples also go to `ruffle-ps3-dev.log`. Logging uses a 4 KiB buffer flushed every
two seconds and at normal shutdown; the final buffered messages can be lost on
a fatal crash. A native RSX vector renderer remains unimplemented.

## Performance revision 0.4 / 01.03

The player keeps 320×240 antialiased software rendering and RSX presentation.
PowerPC AltiVec is now enabled, with portable SIMD arithmetic/comparison dispatch
in tiny-skia. The renderer memoizes eligible vector draws in an 8 MiB cache.
Replay requires the same registered path, transform, color, bitmap version,
target size and exact destination pixels. Masked draws use the original path.
No frame skipping, animation-speed change, SPU offload, RSX vector backend or
interpreter rewrite is included. Native logs report cache activity.

Detailed measurements, test scope, replay instructions, and remaining work
are in [docs/PERFORMANCE_04.md](docs/PERFORMANCE_04.md).

Linux host full-render replay (requires your own legally obtained SWFs):

```sh
export RUSTC_BOOTSTRAP=1
cargo build --release --features ps3-simd --example benchmark --manifest-path player/Cargo.toml
mkdir -p build/replay
player/target/release/examples/benchmark /path/to/bulletbill.swf 1200 verify \
  build/replay/bill validation/performance04/replays/bill.txt
```

The Linux-only benchmark clock freezes movie time during events/updates, making
getTimer/Date/random seeds repeatable. It is inactive for measurements and is
never compiled into the PS3 application. `verify` performs an extra uncached
render of each identical command list and checks all frame pixels. Its timing
is validation overhead, not a performance result. `cache` measures normal
optimized rendering; `scalar` disables caching. To reproduce the scalar baseline,
build the example without `--features ps3-simd` and use `scalar` mode.
