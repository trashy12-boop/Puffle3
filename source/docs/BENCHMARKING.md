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
