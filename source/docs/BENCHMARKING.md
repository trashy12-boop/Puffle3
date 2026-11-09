# Reproducible performance validation

Use `source/` on `main`. The upstream Ruffle revision and patch order are pinned
by `scripts/get-ruffle.sh`. No game assets are distributed. The uploaded Mario
movie used in development has SHA-256
`b1ddb2f7ac6c09e7154771ac2335d962d58536b8dba5720b8c5ff18bb65c917a`,
is Flash 8/AVM1, 450x300, and targets 32 simulation frames/s. Keep the inherited
320x240 viewport, controls, rendering settings and save state identical.

## Host comparison

```sh
. /absolute/tools/env.sh
scripts/build-host-benchmarks.sh
python3 scripts/benchmark-avm1.py /absolute/SuperMario63.swf \
  build/host-benchmarks/baseline build/host-benchmarks/optimized \
  build/measurements --pairs 2
python3 scripts/compare-avm1-fast-paths.py \
  build/host-benchmarks/baseline build/host-benchmarks/optimized build/compat
python3 scripts/compare-path-snapshots.py \
  build/host-benchmarks/baseline build/host-benchmarks/optimized build/path-compat
```

Both host binaries retain v4 path snapshots, shape/bitmap caches, batching and
compact meshes. Only the new `avm1-fast-paths` umbrella differs. It enables
`fast-properties` and `inline-args`, which can also be measured individually.
The older pre-path-snapshot comparison uses `path-baseline` in build-puffle.sh.
Build everything before timing. Run replays serially on an idle host. The
script alternates order (baseline/optimized, then optimized/baseline), retains
raw CSV/logs, checks VM counters, compares every RSX command row including its
vertex/texture hash, and checks all 115 sampled software images. These checks
are strict: a mismatch stops the run. Sampling does not prove every output
pixel in every frame is identical.

The replay fixes the Flash clock and RNG seed to 32 Hz during input/logic, but
uses an independent clock for instrumentation. It runs the real interpreter,
renderer, C command recorder and sound mixer. Rendering additionally produces
software reference images even when the C RSX command recorder accepts a
frame. Therefore `render_ms` is **not PS3 GPU time**, and reciprocal host frame
times are **not console FPS**. The host run is unpaced; simulation steps are
recorded as counts, not claimed real-time FPS. Profile VM time includes
builtins/collision; do not add collision to VM again. GC time is separate.
`p99_host_work_ms` is the nearest-rank percentile of logic + reference-render +
audio work, not presented-frame pacing.

For allocation accounting, build the example with `profile-alloc` in both
variants, without `profile-vm`, and run the same replay. The `-alloc.csv` file
counts successful Rust allocation/reallocation requests inside each logic
step. Live/peak values count requested payload across the entire benchmark;
they exclude allocator metadata, fragmentation, native C allocations and VRAM.
Atomic allocator counters affect performance, so keep allocation runs separate
from speed comparisons. The Linux runner uses `os.wait4` to record each child process peak RSS
independently, without an external timing utility.

## Native PS3 or RPCS3 procedure

```sh
scripts/build-puffle.sh release
scripts/build-puffle.sh diagnostic
# Diagnostic v4 baseline: retains path snapshots, disables v5 fast paths.
scripts/build-puffle.sh baseline
```

Use one package at a time: all share title ID RFLP00001 and keep the original
multi-game browser. Normal release omits VM/opcode counters. Diagnostic and
baseline builds both include identical instrumentation; compare like with like.
Back up saves/profiles before changing their state. Use a clean copied save
state for each run, the same SWF path, same input route, viewport, render mode,
firmware, emulator settings and host power/load state. Record at least three
runs per build after warmup. Test each scene for at least 20 seconds when it
can be held without changing progression.

| Scenario | Reproducible action | Required observation |
|---|---|---|
| Selection screen | Boot; leave one fixed directory highlighted | Original layout, white labels, navigation, no SWF startup |
| Mario title | Launch; wait for title animation to settle | Drawing and simulation rates reported separately |
| File selection/startup | Start Story; select the same copied empty slot | Same control path and save state |
| Castle introduction | Create the same file; play introduction | Animation, transparency/layer order, camera movement |
| Tutorial | Enter Mushroom Kingdom; stop before moving | Stable timing window without intro/loading |
| Movement/jump/coins | Follow the same route and jump/spin sequence | Position/coins, collision, camera, input responsiveness |
| Scrolling/overlap | Cross the bridge along the same route | Drawn versus simulation FPS, fallbacks, memory |
| Demanding scene | Use the same later level/checkpoint and route | Enemies, particles, overlaps; do not substitute title/menu |

The host replay covers the title through tutorial bridge movement. It does
not establish later-level, boss or full-game performance. Keep those rows
explicitly unverified until they are actually exercised.

Read `/dev_hdd0/ruffle/ruffle-ps3-dev.log` (frontend) and `ruffle-core.log`
(runtime). Records include drawing rate, render CPU cost, tick cost, audio pump,
VM/GC/collision, memory payload, texture uploads/reuse, command batches, fence
waits, texture synchronization and cumulative fallback reasons. Tick cost can
contain multiple simulation steps. Submission/driver work is inside render
CPU cost; the port has no GPU execution timestamp query.

v5 adds draw-completion gap count, mean and nearest-rank p99 per roughly
two-second interval, plus `1000/p99_ms`, labeled **p99-equivalent**, not a mean
of the slowest 1%. Gaps measure completion of new Flash renders, not monitor
scanout. Startup and pause transitions reset the sample history. The 256-entry
buffer reports dropped samples if exceeded. Short intervals provide limited
tail resolution. Save all intervals rather than selecting only favorable ones.

```sh
python3 scripts/summarize-native.py ruffle-ps3-dev.log \
  --environment rpcs3 --scene tutorial --start-line 100 --end-line 700 \
  --output tutorial.json
# For a physical console, use --environment physical-ps3.
```

Choose line ranges using independently observed scene transitions. The summary
reports interval means and the worst per-interval p99; it cannot reconstruct a
whole-scene p99 from interval summaries. Preserve the raw log and selection.
For fallback counts subtract the cumulative counters at window boundaries.
Memory payload is not total 256 MB system RAM usage; retain the C heap lines
and account for RSX allocations separately. Do not extrapolate host or RPCS3
numbers to a real PS3. With RPCS3 Null audio, only mixer behavior can be checked;
audible output still requires a console or working audio backend.

## Automated RPCS3 route and raw draw capture

`benchmark-rpcs3.py` replaces the historical scripts' expired absolute paths
with explicit arguments. Prepare a **private** RPCS3 configuration tree, install
firmware and the package, and stage only the matching Mario SWF in
`dev_hdd0/ruffle/games`. Set the keyboard pad mappings and dismiss the initial
RPCS3 welcome dialog during setup. Keep configuration, profiles and saves
identical before each comparison. The runner records their hashes, executable
and emulator hashes, route, environment, screenshots, inputs and native/emulator
logs. Run builds serially after compilation and cache warmup have finished.

The runner creates `dev_hdd0/ruffle/benchmark-enable` for its run. Current C
frontend builds then write buffered `benchmark-frames.csv`, with guest draw
completion timestamp, gap from the previous completed draw and CPU render work
in microseconds. Capture is off when the marker is absent. The 64 KiB capture
buffer and optional writes add overhead: compare capture-enabled builds only
against equally instrumented builds. Remove the marker after interrupted runs.
No simulation scheduling, frame skipping or renderer behavior is changed.

A scenario JSON specifies `swf_name`, `swf_sha256`, and ordered `steps`:

```json
{
  "swf_name": "SuperMario63.swf",
  "swf_sha256": "b1ddb2f7ac6c09e7154771ac2335d962d58536b8dba5720b8c5ff18bb65c917a",
  "steps": [
    {"keys": ["x"], "hold": 0.3},
    {"wait_log": "Ruffle SWF launched", "timeout": 60},
    {"wait": 5},
    {"capture": "launched"}
  ]
}
```

This example is a **launch smoke test**, not a gameplay benchmark. A gameplay
route must first navigate to a visually verified scene, then set
`{"scene":"tutorial"}`, run its movement/jump/spin sequence, capture screenshots,
and set a different scene such as `{"scene":"end"}`. Each scene change waits
for an acknowledgment containing its guest timestamp (at the next two-second
native report). Input holds and waits are host wall time, **not deterministic
simulation-frame replay**. Slow runs can diverge; review screenshots and reject
mismatched routes rather than accepting their timing. Later demanding levels
remain separate required scenarios.

```sh
python3 scripts/benchmark-rpcs3.py \
  --rpcs3 /absolute/rpcs3 --executable /absolute/build.elf \
  --swf /absolute/SuperMario63.swf --scenario /absolute/route.json \
  --config-home /absolute/private/config --cache-home /absolute/private/cache \
  --xvfb /absolute/Xvfb --xdotool /absolute/xdotool \
  --display 127.0.0.1:98 --output /absolute/new-run-directory
python3 scripts/summarize-draw-capture.py \
  /absolute/new-run-directory/benchmark-frames.csv \
  /absolute/new-run-directory/ruffle-ps3-dev.log \
  --scene tutorial --output /absolute/new-run-directory/draw-summary.json
```

Use the same inherited library/driver environment when local extracted Linux
libraries are required. The runner preserves it in `run.json`. It limits boot
and total run time, aborts on emulator fatal errors or excessive RSS, releases
held keys and preserves available evidence on failure. A completed input route
is labeled `route-completed-review-required`, never automatically certified as
correct gameplay. The summary rejects missing/repeated/open scene markers and
excludes every gap crossing a scene boundary. It calculates whole-scene median,
nearest-rank p99, worst draw gap and frames per elapsed gap time. These measure
native Flash draw completions, **not RPCS3's presentation FPS**, simulation FPS,
or RSX GPU execution time. The p99 reciprocal is explicitly p99-equivalent FPS,
not an average of the slowest 1%.

For a known empty starting state, run
`python3 scripts/make-sm63-benchmark-state.py /absolute/new-empty-state` and pass
`--state-template /absolute/new-empty-state` to the runner. Existing emulator
profiles/saves are backed up for the run and restored afterward. The measured
run's resulting state is kept under its output directory. Keep the same template
for every comparison. `--build-manifest build.json` records the compiler/profile
and feature choices alongside executable hashes. Locally extracted X11 packages
may also require `--xkb-dir /absolute/desktop/usr/share/X11/xkb`.

An `event` step sends an absolute native Flash input through the opt-in mailbox:

```json
{"event":{"kind":2,"x":148,"y":200}}
```

Kinds match the existing player ABI: 0 key, 1 mouse button, 2 mouse move. Keys
use virtual-key codes; mouse code 0 is left and 1 right. Use separate down/up
steps with `down:true` and `down:false`. The runner writes one monotonically
numbered command atomically and waits for its acknowledgment. The native reader
validates the kind, code, finite viewport coordinates and ID. It polls at the
existing approximately two-second report boundary. These steps prioritize
reliable menu navigation over short input pulses. Wall-time holds are still
not deterministic simulation-frame replay. Keep ordinary pad input neutral
while mailbox keys are held. Ordinary controller tests remain necessary.

For the bounded atlas experiment, explicitly compile a capable native executable
with `RP_EXTRA_CFLAGS=-DRP_RSX_GRADIENT_ATLAS=1`. With raw capture enabled the
renderer defaults to its old texture path. The runner's `--gradient-atlas on`
and `--gradient-atlas off` select both arms in **that identical executable** and
restore the prior marker afterward. Requested-on runs require an acknowledgment
that the atlas is enabled; a non-capable binary fails that check. Outside raw
benchmark capture an explicitly atlas-capable executable enables the experiment.
Ordinary builds leave the compile-time capability off until acceptance.

`wait_gameplay` can advance the intro with brief controller presses until the
calibrated Shine/Star HUD is visible at 1280x720. It is a navigation heuristic,
not gameplay-correctness certification. Review `gameplay-ready.png` and the
movement captures. Reject runs in a dialog, wrong scene, or different route.

The host `host-gradient-atlas` feature enables command-recording checks for the
same C atlas. Constant LUT rows and physical texture offsets are canonicalized;
actual texture pixel bytes, triangle order/geometry/UVs, blending, transforms and
scissor remain in the semantic hash. Compare those hashes and VM counters and
software reference images, allowing batch/upload/cache counts to change.
This checks submission semantics, not actual RSX filtering precision or GPU time.
