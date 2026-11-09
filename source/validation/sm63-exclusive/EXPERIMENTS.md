# SM63 exclusive work: experiments in progress

No new emulator gameplay FPS gain has been demonstrated at this checkpoint.
Physical v5 gameplay reference remains the existing report, not a matched v6
comparison. Do not request another physical test until the RPCS3 acceptance gate
is met or an actual hardware-specific blocker has been isolated.

## Compiler experiment

Existing source, same features and capture frontend in both variants:

- Baseline: release opt-level 2, 16 codegen units, no explicit LTO.
- Candidate: `CARGO_PROFILE_RELEASE_OPT_LEVEL=3
  CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1 CARGO_PROFILE_RELEASE_LTO=thin`
  applied to `scripts/build-puffle.sh release`.
- Rust 1.99.1, pinned upstream/patches and native C `-O2 -mcpu=cell` unchanged.
- Both native ELF/PKG builds succeeded. Candidate is not a default build mode.
- Preserve each ELF before building another variant. The build script's ordinary
  v6 output name is reused; filename alone does not establish build identity.
- Record executable SHA256 and a build manifest with every timing run.
- Before measuring, finish both RPCS3 cache warmups and stop all compilers and
  other emulator instances. Compare repeated gameplay routes, not startup.

The physical v5 profile has a large VM/update component; this experiment tests
whether broader inlining and compiler optimization can reduce it. Compiler
flags alone are not evidence of an improvement, and code-size growth can hurt
Cell instruction-cache behavior. Keep the default profile unless measurements
justify a change.

## Operand-free AVM1 dispatch experiment

`scripts/ruffle-ps3-11-direct-dispatch.patch` is now applied by the pinned patch chain, with its feature **off by default**. It adds an opt-in upstream feature and intercepts
operand-free short instructions before constructing the generic SWF `Action`
enum. It calls the existing instruction handlers, advances exactly one byte,
and preserves the existing timeout boundary and profiling counts. Operands,
unknown bytecodes and malformed payloads keep the original reader. Debug AVM
tracing disables the shortcut.

This targets repeated bytecode decoding inside the measured VM bottleneck.
It does not cache values or object identities, alter simulation timing, remove
effects, skip frames, or assume that Mario's program never changes objects.

Feature-on and feature-off host builds succeeded. All 51 AVM1 fixture traces
match; 49 match upstream expected output in both variants. The feature-on
Rust suite passes 24 tests, with one existing ignored test. The complete Mario
replay comparison and actual native RPCS3 performance comparison remain pending.
A host improvement alone must not be reported as console FPS.

Enable only for an explicit experiment with `--features direct-dispatch` for a
host Cargo build, or `RP_EXTRA_PLAYER_FEATURES=direct-dispatch` for
`scripts/build-puffle.sh`. Normal release behavior keeps the feature off.

## Native input and timing infrastructure

The native v6 + capture frontend booted RPCS3 and displayed the original browser
with white labels. Super Mario 63 reached START, Story and empty file selection.
This calibration run included concurrent compilation and is not a valid timing
comparison. Slow mask transitions also made analog-stick menu positioning vary.

The next capture frontend adds an opt-in absolute event mailbox, checked at
the existing two-second sample boundary. It calls the same native player event
API as ordinary controls and acknowledges event IDs with guest timestamps.
It is disabled without `benchmark-enable`; there is no simulation-clock or
frame-skipping change. Both baseline and compiler-candidate native ELFs compile
with this identical instrumentation. Mailbox runtime validation is pending.

The Python runner can restore an explicit starting state template, preserve its
final state, and put the original state back. The new state generator emits the
normal Mario bindings with an 80 px/s right-stick menu cursor and empty saves.
Its generated binary profile was loaded by the real C profile loader and matched
the native Mario bindings and intended cursor settings byte-for-byte.

## Bounded linear-gradient atlas experiment

Native calibration reaches actual tutorial gameplay; movement, jump/spin inputs,
bridge/camera scrolling and a coin counter change from 0 to 2 were observed.
One manual capture window reports 4.04 drawn frames/s over 210 complete gaps in
51.92 seconds. It includes only a short movement sequence plus idle gameplay,
is not a repeated accepted benchmark, and must not be compared against the
game's internal FPS HUD or the older emulator session. No improvement is claimed.

The native CPU render-submission costs are much smaller than draw gaps, while
the renderer still submits many batches. Together with flip-ready gating this
suggests asynchronous rendering/driver work may constrain this software-GPU host.
It is an inference, not a GPU execution-timestamp measurement.

The next experiment packs immutable straight-alpha 256x1 linear gradient LUTs
into exact row centers of one 256x1024 texture. Neighboring fills can share GPU
state and merge without changing painter order. Radial/focal, revised textures,
and incompatible coordinates retain the original renderer. An extra mapped
1 MiB buffer and bounded metadata are used only when enabled. Allocation failure
falls back to ordinary textures. Page exhaustion never overwrites in-flight rows;
reset happens after the next frame fence. Dirty rows are synchronized before
submission. Existing texture/vertex budgets and Rust memory guards are preserved.

The portable atlas command/texture mock passes LUT-byte and horizontal filtering
checks for pad/repeat/mirror, uniform/scissor splits, adjacent batching, cache
reuse, page exhaustion, frame fencing, mapping failure/recovery and mode-off
behavior. Address/undefined sanitizers passed with LeakSanitizer disabled because
this execution environment cannot run it; explicit mapped-buffer release checks
pass. These are mock checks, not GPU pixel or performance validation.

An atlas-capable native ELF compiles with
`RP_EXTRA_CFLAGS=-DRP_RSX_GRADIENT_ATLAS=1`, using the original Rust opt-level 2
library. The same executable supports both benchmark arms: capture-enabled runs
default off; `gradient-atlas-enable` selects on. Normal playback in this explicitly
atlas-capable build enables the experiment. Ordinary release builds do not compile
the atlas by default. No experimental package has been requested for physical testing.

The host probe now normalizes physical texture offsets and constant linear-LUT
row coordinates while hashing actual texture pixel bytes, triangle geometry/UVs,
state and painter order. This allows strict semantic command comparison despite
atlas placement and batching differences. It still does not execute GPU shaders.
The full Mario comparison and the new native replay run remain pending here.
