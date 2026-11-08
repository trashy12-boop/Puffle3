# Test results — 2026-10-28

## Host

The shared C code was compiled on x86-64 Linux with GCC, `-Wall -Wextra
-Werror`, AddressSanitizer and UndefinedBehaviorSanitizer. Tests passed for:

* overlapping button mappings and key/button release aggregation;
* menu chord and input suppression, solo L3 tap, cursor confinement;
* SWF extension filtering, basic header inspection and folder navigation;
* profile save/load and rejection of a damaged profile.

LeakSanitizer was disabled because this sandbox cannot enumerate `/proc` for
its thread scan. This does not establish the absence of leaks.

The unmodified, pinned Ruffle core ran upstream `avm1/trace` and `avm2/trace`
fixtures with exact expected trace output. Another AVM1 fixture accepted a
Space press/release sent through the C mapper and the limited Rust callback,
and emitted the expected `onPress` and `onRelease` traces. A truncated SWF
was rejected. These tests use null graphics/audio and memory storage, not the
PS3 backend, a real game, save data, video, or performance-heavy content.

One **host-only**, unoptimized debug run of the AVM2 trace fixture took
21.932 ms for the runner's single `run_frame` loop, 217.794 ms total process
elapsed, with 36,668 KiB maximum resident set size. It is a one-sample
measurement of a tiny test on this x86-64 container, not a benchmark or a
prediction of PS3 RAM use or frame rate. No optimization speedup was measured.

## RPCS3

RPCS3 `0.0.43-20415-6c44fff7`, preexisting installed firmware reported as
4.93, Xvfb, Mesa software OpenGL, static PPU interpreter and asynchronous
shader recompiler. Direct ELF boot and boot of the installed NPDRM EBOOT both
reached the application menu. Keyboard inputs emulated the PS3 controller.

Observed: HDD browser listed an SWF; folder entry and parent navigation worked;
SWF header details opened; a profile changed and was saved, then reloaded
after relaunch; the L3+R3 chord opened and closed the **input-test** menu
state. None of this exercises Flash playback. USB browsing, physical pads,
PS3 sound, game restart and sustained gameplay were not tested.

The first OpenGL attempt used RPCS3's shader interpreter and failed because
software OpenGL lacked `GL_ARB_bindless_texture`; selecting the asynchronous
recompiler let the menu render. Font glyphs remain surrounded by dark boxes
with this software renderer, even after the atlas workaround. Effects on real
RSX hardware have not been assessed.

The headless RPCS3 package installer extracted `PARAM.SFO`, `ICON0.PNG`, and
`USRDIR/EBOOT.BIN` to the expected title directory, but then exited with code
134 while reporting an unavailable emulation object during teardown. The
extracted EBOOT was subsequently booted and tested as described above.
Successful extraction/boot in RPCS3 does not establish successful PS3 HEN or
CFW installation. An attempt to obtain the official Sony update during this
session returned HTTP 503; no firmware is included in the release.

## Real PS3

Not tested. No supported Flash game or ActionScript version is demonstrated
on physical hardware or in the PS3 build.

## PS3ME menu adaptation revision

The user reports that the original development PKG launches on their physical
PS3. This only confirms launch; no Flash execution was present or tested.

The native settings page now adapts PS3ME's InGameMenu.cpp scrolling window,
layout, gradient, selection highlight, and Left/Right/Cross navigation to RSX.
Java VM, LCD resolution and audio hooks were not copied into Ruffle. Binding
slots remain available with L1/R1. Added working profile fields for cursor
stick, speed, deadzone and acceleration; edits persist in the existing format.
The input diagnostic reloads the selected file's profile. This does not add
SWF playback or an actual in-game pause menu.

Revision validation: native PPU ELF and finalized PKG rebuilt successfully
with warnings treated as errors. Existing ASan/UBSan C platform and actual
Ruffle host integration tests passed again. RPCS3 booted the revised ELF;
keyboard-emulated controller input entered the settings page, scrolled to
cursor settings and changed cursor stick and speed. The screen reported
"Profile saved". This revision was not installed or tested on a physical PS3.
Previously documented text backgrounds remain visible in software OpenGL.
No measured performance improvement or Flash compatibility change is claimed.

The initial revision check exposed LV2 CELL_EEXIST when renaming a new profile
over an existing record. The save path now moves the old record to a backup,
installs the new record, and removes the backup after success. Failure attempts
to restore the backup; loading also accepts the backup when the main record is
missing. This addresses repeat saves without deleting the only complete copy
first. Power-loss durability is not guaranteed. Added host regression coverage
for replacing a profile and loading the backup after an interrupted rename.
The corrected ELF was rerun in RPCS3 against an existing profile. Repeated
saves succeeded; the screenshot showed "Profile saved" and inspection of the
persisted record confirmed speed=430, deadzone=18, acceleration=50, stick=2.
