# Ruffle PS3 — development snapshot

**This is not a Flash player. It cannot launch or play any SWF on PS3.** The
PS3 executable is an experimental native browser and controller-profile
frontend. Actual Ruffle is compiled and exercised only by the separate
headless **host** test program. It has no console renderer, PS3 audio, or PS3
Rust standard library. There are no graphical or audio compatibility claims.

## What is implemented

* A PSL1GHT/RSX frontend with HDD and USB SWF/folder browsing, an eight-byte
  SWF header check, per-file mapping editor, and controller-input test page.
* Portable controller translation code for 16 physical buttons and 8 stick
  directions, up to three bindings each, keyboard/mouse button aggregation,
  cursor motion, dead zone, acceleration, and L3+R3 menu-chord state.
  The test page exercises this code; it is **not connected to Ruffle**.
* Per-file profile persistence with explicit little-endian fields, a path
  identity check, checksum, bounded records, and temporary-file replacement.
* A host-only Rust program built against Ruffle core, plus small AVM1/AVM2
  trace and AVM1 keyboard-event smoke tests using upstream test SWFs. The
  keyboard smoke test routes a Space binding from the C mapper into Ruffle.
  That narrow host bridge is not a complete keyboard/mouse bridge.

The frontend has no favorites, recent list, thumbnails, global preferences,
graphics menus, audio, in-game menu, or game launching. The profile editor
offers a limited set of keys and does not expose mouse speed/dead zone yet.
Profiles identify games by full path; moving a SWF creates a different profile.
The browser lists at most 256 entries and supports USB device `dev_usb000`.
L3/R3 actions are delayed briefly to detect the menu chord; if one stick is
held alone beyond that delay, it can produce its mapped action before the
other stick is pressed. Font rendering has dark boxes in the tested RPCS3
software-renderer setup.

## Running the experimental frontend

Only if you want to inspect the development UI: install the supplied
`ruffle-ps3-dev.pkg` via Package Manager on a HEN/CFW PS3. This binary has
**not been tested on physical hardware**. Start **Ruffle PS3 Dev** from the
XMB. It creates `/dev_hdd0/ruffle/games` and
`/dev_hdd0/ruffle/profiles`. Place your own SWFs in the games directory, or in
`/dev_usb000/ruffle/games` on a compatible USB drive. They appear in the
browser but cannot be played.

* D-pad: select; Cross: open folder or file details; Circle: parent/back.
* Start: switch HDD/USB. Select: controller-input test.
* Square on a SWF: edit its profile. D-pad changes the selected action;
  L1/R1 selects one of three simultaneous bindings. Changes save immediately.
* In the input test, L3+R3 toggles the protected menu state. Start returns to
  the browser. This is a diagnostic simulation, not an in-game settings menu.

The optional log is `/dev_hdd0/ruffle/ruffle-ps3-dev.log`. The package does
not include games, Sony firmware, the PS3 toolchain, or the host test binary.

## Host tests

Requirements: a recent Rust toolchain (tested with 1.99.1), Cargo, Java,
GCC, and network access to obtain crates and Ruffle. In the project directory:

```sh
./scripts/get-ruffle.sh
./scripts/test-host.sh
```

The Rust test runner uses Ruffle's null graphics, null audio, null navigator,
and in-memory storage. Its frame count and elapsed time do not represent
console performance. The 64 MiB limit is a preliminary file/header limit,
not a guarantee against decompression bombs or all malicious SWFs.

## Building the PS3 frontend

Install a PS3DEV/PSL1GHT toolchain with PPU compiler, `sprxlinker`, `sfo`,
`pkg`, `make_self_npdrm`, `cgcomp`, and the NVIDIA Cg runtime required by
`cgcomp`. Newer PSL1GHT graphics APIs are required; the 2020-era SDK alone is
too old. Set `PS3DEV`, `PSL1GHT`, `PATH`, and the host's library search path
for Cg. Then:

```sh
make                 # build/ruffle-ps3-dev.elf
make package         # build/ruffle-ps3-dev.pkg
```

If PPU tools and PSL1GHT are installed in separate prefixes, set both
`PS3DEV` and `PSL1GHT`; `PS3_OLD_LIBS`, `SFO`, `PKG`, and `SFO_XML` can be
overridden as needed. The SELF/PKG tools may introduce timestamps or random
values, so bit-for-bit reproducibility has not been established. The tested
SDK combination, revisions, limitations,
and emulator results are in `docs/ENGINEERING.md`.

## Licensing

The native executable includes GPL-2.0-only menu adaptations in
`src/ps3_main.c`, derived from PS3ME `ps3/src/InGameMenu.cpp`. The native
executable is distributed under GPL-2.0-only; see `COPYING.PS3ME`. Other
project-specific modules retain the MIT license in `LICENSE`. The original
menu reference is preserved in `vendor/ps3me-reference` for provenance and is
not built (it requires PS3ME/Java/SDL). `vendor/rsxkit` is MIT-licensed
upstream code from Salsa64 and retains its license and third-party notices.
Ruffle is fetched separately at the pinned revision under its own MIT OR
Apache-2.0 licensing. The pinned Ruffle checkout includes its own licenses.
No Salsa64 GPL frontend code, proprietary Cg binaries, game SWFs, or Sony
firmware are redistributed here.

## PS3ME menu revision

The controller settings page reuses PS3ME's eight-row scrolling layout and
navigation, rendered with RSX. Up/Down selects; Left/Right or Cross changes;
L1/R1 selects one of three binding slots; Circle returns. After the 24 control
rows are cursor stick, speed, deadzone, acceleration, save and return. Changes
save automatically to the selected SWF profile; the Save row allows retrying
a failed save. Select from the browser tests the last selected file's profile
(or defaults if no file was selected). While its diagnostic menu is open,
Square opens file settings. No SWF playback or Java-specific menu hooks exist.

The menu was adapted from the PS3ME 01.08 source snapshot supplied in the prior
project. Original reference SHA-256:
`06a22dd510621bfe648e1dbe2417d20a56c4299cc3fbe825092b10580983fee6` (see source archive for the complete reference).
