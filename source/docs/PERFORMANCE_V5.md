# Puffle3 01.09 v5 — measured incremental AVM1 optimization

The target of stable 30–32 FPS Super Mario 63 gameplay on physical PS3 hardware
is **not established**. No physical PS3 is connected here. v5 is an incremental
runtime improvement, not a complete port or a claim of 30 FPS gameplay.

## Baseline and retained work

Development started from GitHub `main` at
`81e4f3b6549281dad24c739baca319a4d7c97cdb`, whose runtime snapshot was 01.08 v4.
The complete matching local development tree, toolchain, old measurements and
native emulator environment were recovered and compared to the committed v4
files. The pinned Ruffle revision is
`1125b0018c110fb7a1e51f080c7b47d5dc94d64f`. Existing path snapshots, batching,
compact meshes, texture residency, software caches, memory-pressure collection,
original multi-game browser, white text, controls, saves and sound were retained.

GitHub previously contained only the build launchers and a partial v4 source
snapshot. `source/` now contains the complete **buildable** project, patches,
locks, notices and project tests. Fetched upstream Ruffle, toolchains, generated
outputs and unmodified third-party development image/photo suites remain outside
Git. This source restoration is not presented as a performance optimization.

The user's earlier 8–10 FPS physical-console gameplay report remains historical;
it is not a v4 or v5 hardware measurement. The previous v4 diagnostic RPCS3
record (~13 drawn FPS, ~27 simulation FPS) was reviewed, not reclassified as
real-console evidence or a matched v5 comparison.

## Implemented runtime changes

1. **Own stored-property fast path.** A visible, non-virtual own property can be
   returned after one property-map lookup, bypassing repeated getter/stored
   searches. Scope resolution also avoids a separate membership check for that
   case. There is no cached mutable value and no invalidation protocol: every
   read sees current data. Getters, hidden versioned properties, `super`, missing
   properties, prototype searches and removed-clip scope substitution retain the
   existing path. Case sensitivity and version rules remain active.
2. **Short argument buffers.** AVM1 call/constructor argument lists use
   `SmallVec<[Value; 4]>`. Up to four arguments remain on the PPU stack; longer
   calls spill normally. Original pop order, stack-length clamp and MovieClip
   reference unwrapping remain. This reduces allocator traffic without changing
   the interpreter, simulation frequency or game logic.
3. **Measurement tools.** Optional benchmark-only allocation accounting, serial
   replay/equality checking, broader AVM1 differential fixtures, and native draw
   interval mean/p99 logging were added. Normal builds omit VM/opcode counters.

The two runtime changes are independently selectable as `fast-properties` and
`inline-args`, or jointly as `avm1-fast-paths`. Patch 10 makes the upstream
modifications reproducible. The v4 baseline keeps path snapshots enabled and
turns off only these new fast paths.

## Host timing comparison

Same uploaded SWF, deterministic 32 Hz movie clock/input replay, 320x240 viewport,
sound mixer, C RSX command recorder and software reference renderer. Three paired
3400-frame replays compared optimized builds with identical diagnostic features.
Late gameplay is frames 2800–3399, including movement, jumping/spin input,
scrolling bridge scenery and overlapping objects. These are **x86 host work
timings**, not PS3 timings, GPU timings, or presented-frame rates.

| Late gameplay mean across three paired replays | v4 baseline | v5 | Change |
|---|---:|---:|---:|
| Logic work | 4.964 ms | 4.718 ms | 4.95% lower |
| AVM1 time, inside logic | 3.329 ms | 3.097 ms | 6.96% lower |
| GC | 0.932 ms | 0.944 ms | 1.35% higher |
| Host reference-render work | 21.960 ms | 21.928 ms | Essentially unchanged |

Individual logic comparisons ranged from roughly **9% lower to 1.8% higher**.
One later pair showed no timing gain. Shared-host load and timing variation
therefore limit the claim: the aggregate gain is modest, not proof of a
consistent physical-console FPS improvement. The earlier baseline-1 run overlapped
compilation and is excluded. No GC or GPU speedup is claimed.

The final workflow smoke pair separately recorded process peak RSS of 185500 KiB
(baseline) and 185464 KiB (optimized). This is effectively unchanged and is not a
PS3 memory measurement. Its p99 total host work was 38.217 ms / 38.903 ms;
that includes software reference drawing and is not native frame pacing.

Each accepted paired replay had identical per-frame opcode/get/set/call/hit-test/
simulation counts, all 3400 RSX command rows (including vertex/texture stream
hashes), and all **115 sampled software images**. Sampling does not prove every
pixel of every frame. All 600 late-gameplay command frames were RSX-eligible in
both variants; across the complete replay 2978/3400 frames were accepted. Thus
fractional masks remain a fallback issue, but they did not explain the measured
late-gameplay VM bottleneck in this replay.

## Allocation comparison, separate instrumented run

Counting successful Rust allocation/reallocation requests during logic steps:

| Frames 2800–3399, per simulation step | Baseline | v5 | Reduction |
|---|---:|---:|---:|
| Allocation requests | 10584.63 | 9245.32 | 12.65% |
| Requested allocation bytes | 483503.47 | 447952.48 | 7.35% |

Both allocation-profile replays had identical command CSVs and all 115 reference
images. Atomic allocator counters add overhead; those runs are not used for
speed claims. The reported live/peak payload is dominated by movie/renderer
assets and benchmark buffers, and was effectively unchanged. It excludes
allocator metadata, native C allocations, fragmentation and VRAM. Reduced
allocation traffic did not establish a GC-time reduction in the timing runs.

## Validation

- Fresh upstream acquisition applied all pinned patches successfully from
  `source/`, followed by successful portable C checks in the restored repository.
- C checks cover RGBA conversion, texture upload/reuse/update/eviction, batching,
  fences, platform controls/storage, browser layout, white external-font RGB and
  exact glyph coverage, plus bounded frame-interval percentile accounting.
- Rust player tests: **20 passed, 1 ignored, 0 failed**. The ignored test is the
  existing SIMD-specific test unavailable on this host. AVM1 input/rendering,
  mixer output and save/storage hooks remain covered by existing tests.
- **51 AVM1 before/after trace comparisons match**; 49 match upstream expected
  output. `watch_virtual_property` and `string_paths_reference_launder` retain
  their existing upstream-known failures. Five upstream tick-count fixtures are
  differential frame-step probes, not a replacement for the full upstream
  tick/input-aware runner.
- The existing 15-fixture path comparison also matches before/after, including
  the original rename/removal/recreation fixture; 14 match upstream expectations.
- Normal and diagnostic PPU ELF/SELF/PKG builds completed. Normal raw ELF has no
  `puffle_profile` symbols; the diagnostic has them. The finalized normal package
  installed in RPCS3 as Puffle 3 Emulator, RFLP00001, APP_VER 01.09.

## Rejected experiment and remaining bottlenecks

A separate payload-free opcode dispatcher passed the same differential fixtures
but did not improve the measured result beyond the retained two changes. It was
removed before packaging. Its measurements are labeled exploratory, not mixed
into the retained-change comparison.

VM execution and GC still dominate the earlier native gameplay profile. Collision
was a small subset of VM time, so no game physics/collision shortcuts were added.
Frequent MovieClip reference/scope construction, property hashing, function calls
and tracing of live GC data remain candidates for further profiles. CPU geometry
expansion/transformation, display-list traversal and software vector rasterization
also remain. Texture reuse, batching and safe fences remain active; the previous
native profile showed very small fence/synchronization totals relative to VM work.
No evidence justified removing those fences or changing upload lifetimes.

Fractional antialiased rectangular masks still use the inherited software path.
Rounding them to scissor edges would change coverage; no approximate replacement
was shipped. Accurate GPU coverage/mask textures and wider blend/filter support
remain future work. The host recorder validates commands, not RSX pixel output.
The renderer retains its earlier advanced-blend/filter/Stage3D and gradient
limitations; this iteration does not claim to fix them.

No SPU offload or JIT was added. The dominant VM/GC work operates on a serial,
mutable object graph; turning it into DMA jobs or generating machine code would
require a much larger correctness/toolchain effort. Geometry jobs could be more
suitable, but no measured DMA/synchronization break-even point is available.
These are investigated opportunities, not delivered acceleration claims.

Audible console audio, later demanding levels, bosses, all abilities, full
save/reload progression, long sessions and physical-console 30 FPS remain
unverified. The replay exercises tutorial scenery and input, not the entire game.
Simulation scheduling remains the original SWF's 32 Hz. No artificial 60 Hz game
speedup, frame skipping policy change, removed effects or custom launcher was
introduced. See BENCHMARKING.md for all eight scene procedures and metric limits.

## v5 final normal-package RPCS3 gameplay record

The final normal PKG was installed and booted as 01.09/RFLP00001. The original
browser and white labels were visible, and Mario's title/Story/file-selection
screens were navigated. Creating File A progressed through startup into the
Mushroom Kingdom tutorial. Walking, jumping, camera scrolling and spin input
were exercised. A screenshot records the coin counter increasing from 0 to 1.
A 5605-byte SharedObject was written; that alone does not verify a complete
save/reload cycle.

The emulator is RPCS3 0.0.43-20572-e61d5eaf, LLVM PPU recompilation, Vulkan with
Mesa llvmpipe on the shared Linux host, firmware 4.93, Null audio/music handlers.
This is not physical-console performance or an audible-output test.

| 39 observed tutorial timing intervals, final normal package | Result |
|---|---:|
| Mean reported drawing rate | 14.27 frames/s |
| Mean reported render CPU work | 5.60 ms per render |
| Mean reported tick CPU work | 24.01 ms per tick |
| Mean reported audio pump work | 0.38 ms |
| Worst per-interval p99 draw-completion gap | 142.697 ms |
| Recorded draw-gap samples / dropped samples | 1125 / 0 |
| Rust requested-payload peak | 111621 KiB |
| New RSX final frames / full software fallback frames | 1125 / 0 |
| Failed RSX draws in the selected intervals | 0 |

The normal build does not enable VM/simulation counters, so no simulation FPS
is inferred from the game's HUD or RPCS3's presentation FPS. Ticks can run
multiple simulation steps. This normal run is not matched against the earlier
v4 diagnostic run; it does not demonstrate a native FPS gain. The observed
14.27 drawn FPS remains below the target. The percentile is the worst short
report-window p99, not the p99 of the entire scene.

No new full-game Right Mix or Bullet Bill benchmark was run: those SWFs were
not available in this session. The AVM1 fixture suite checks general runtime
semantics, not complete compatibility or performance acceptance for every game.

## v5 diagnostic ELF RPCS3 gameplay record

The diagnostic ELF also booted the original browser with white labels and
entered tutorial gameplay. Walking/camera movement, jump and spin inputs were
exercised. A separate observed window contains 42 timing intervals:

| Tutorial, diagnostic ELF | Result |
|---|---:|
| Mean reported drawing rate | 13.86 frames/s |
| Mean reported simulation rate | 28.40 frames/s |
| Mean render CPU / tick CPU work | 6.88 / 21.31 ms |
| VM time per roughly two-second report | 0.970 s |
| GC time per roughly two-second report | 0.382 s |
| Collision time, included inside VM | 0.046 s per report |
| Worst interval p99 draw gap | 133.885 ms |
| RSX final frames / full software fallback frames | 1171 / 0 |
| Rust requested-payload peak | 113846 KiB |
| Failed RSX draws | 0 |

VM and GC remain the largest measured costs in this run. The reason counter for
lines/cache also records offscreen cache work; it is not a full-frame software
fallback count. Neither this run nor the normal run is a matched v4 native
comparison. Different diagnostic overhead and input routes prevent treating the
normal/diagnostic FPS difference as a controlled comparison. Audible output,
physical-console FPS and later demanding levels remain unverified.
