# v6: work prompted by physical PS3 diagnostics

The owner confirmed the supplied 01.09 v5 diagnostic logs came from a physical
PS3. These supersede the earlier statement that no v5 console measurements
were available. The console model, firmware and exact route were not supplied.
The core log appends across launches; the frontend log starts fresh each launch.
Selected intervals and raw-file SHA256 values are in
`validation/v6/physical-v5-summary.json`. Report means are not complete-scene
frame-time percentiles or a controlled comparison against v4.

The selected RSX gameplay window (frontend lines 655–849) averages 10.26 drawn
and simulation frames/s: render CPU 17.3 ms, tick CPU 77.3 ms. VM totals average
958 ms and GC 349 ms per roughly two-second report. Collision is included
inside VM, not an additional disjoint category. Tick costs can contain multiple
simulation steps. This establishes a serious script/update bottleneck even
without new full-frame software fallbacks.

At the end (lines 1182–1416), the RSX frame total stalls at 2357 while software
frames and mask-fallback counters advance together. Rendering reaches 289.9 ms
in the final report and drawing declines to 2.4 frames/s. This confirms a
separate mask-triggered software-rendering bottleneck. The log identifies the
fallback category, not the precise mask geometry. Fractional rectangular masks
were seen in the prior host replay; they cannot be assumed to explain every
mask in this console route. RSX GPU execution time is not directly measured.

The C heap arena reaches roughly 175 MiB; Rust requested-payload peaks around
108 MiB. Rust allocations are backed by the C heap, so these numbers must not
be added. Heap free space is not total system RAM. GC pressure and long
collections remain concerns; this change does not relax the memory guard.

## Implemented

- Software draw-cache replacement no longer scans the entire (up to 1024-key)
  FIFO for each destination mismatch. Versioned queue records preserve eviction
  order, skip stale records, and compact at a bounded 2048 records. The pixel
  cache remains capped at 8 MiB, or 2 MiB under pressure. Queue bookkeeping can
  use more memory than the old queue, bounded by 2048 records.
- Masks retain their exact per-pixel alpha and record the bounds of all nonzero
  samples, including alpha=1 edges. Pixel snapshots/comparisons are restricted
  to those bounds; fully transparent masks skip vector path work. This reduces
  cache copying and allocation for narrowly masked paths without rounding edges.
- Diagnostic builds write the first 20 distinct rejected mask descriptions to the runtime
  log. This supplies actual console transforms/geometries for the next GPU work.

The 100,000-replacement host microbenchmark uses a full 1024-entry cache and
4x4 snapshots to expose bookkeeping overhead. Five baseline times are
0.716/0.685/0.705/0.649/0.635 seconds; changed times are
0.0282/0.0385/0.0275/0.0247/0.0245 seconds, with identical 131072 cached pixel
bytes. Median replacement throughput improves about 25x. This is deliberately
a cache stress test, not a game speedup or physical PS3 measurement.
Reproduce with `git show ef891e1:source/player/src/draw_cache.rs > /tmp/v5-cache.rs`
and `python3 scripts/benchmark-draw-cache.py /tmp/v5-cache.rs` with cargo on PATH.

## Remaining work and console validation

Full fractional/complex GPU masks are still unfinished. They retain software
fallback so this build does not promise to remove the 2–4 FPS section. AVM1,
GC, CPU path rasterization and allocation churn remain major targets. No new
SPU/JIT implementation or simulation timing change is included.

Use the same scene and route in the normal v5/v6 builds for drawn FPS and
responsiveness. For diagnosis install the v6 diagnostic build, reproduce the
slow mask section, exit, and copy both logs before launching again. Diagnostic
instrumentation adds overhead and cannot be compared directly to normal builds.
The original browser and white labels remain. Physical v6 improvement and stable
30–32 FPS gameplay require measurement; the target is not met by this report.

## Build and correctness checks

Portable C checks pass, including renderer mocks, texture reuse, controller and
storage behavior, UI atlas white text and frame interval statistics. Rust
release checks pass: 24 tests, one existing SIMD-specific ignored test, zero
failures. New cases cover stale queue records, FIFO eviction after replacement,
bounded bookkeeping under 10,000 replacements, low-alpha mask bounds, and
fractional/nested mask pixel equality. Normal and diagnostic PS3 packages and
PowerPC64 big-endian ELFs compile; the normal ELF has no VM profiler symbols,
while the diagnostic ELF contains them and distinct-mask logging. These are
build/host checks; a v6 console FPS or emulator boot result is not claimed.

Two paired 3400-frame host replays used the same v5 AVM1/path-snapshot features,
with order reversed in pair two. All 230 sampled software images, 6800 VM
counter rows and 6800 RSX command rows match. No host compilation overlapped the
replay measurements. Raw summaries are in `validation/v6/host-summary.json`.

| Host scene | v5 render ms | v6 render ms |
|---|---:|---:|
| Title | 6.375 | 6.334 |
| File selection | 15.051 | 15.093 |
| Startup | 12.172 | 12.119 |
| Castle intro | 16.099 | 15.748 |
| Tutorial | 17.423 | 17.192 |
| Moving/jumping/scrolling | 22.337 | 22.370 |

These full-route changes are small and variable. Late-gameplay software-reference
render time is effectively unchanged (+0.14%); logic time varies from 4.995 to
5.197 ms despite unchanged VM work and interpreter code. There is **no demonstrated
whole-game host speedup**. Host software-reference rendering is performed even
for accepted RSX command frames and must not be interpreted as RSX GPU time.
The console's late slow mask route was not reproduced by this deterministic
host route. V6 is a bounded cache optimization plus improved diagnosis, not a
verified fix for the 2–4 FPS collapse or the 10 FPS script bottleneck.
