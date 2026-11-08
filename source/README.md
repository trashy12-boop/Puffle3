# Puffle3 01.09 v5 — current source

This is the current C/Rust source tree for the Puffle3 PlayStation 3 port of
Ruffle. It keeps the original multi-game browser, white text, per-game controls,
saves, audio and existing renderer/path-cache optimizations. The new AVM1 fast
paths reduce repeated stored-property lookups and small call argument allocations.

The 30–32 FPS physical-console gameplay target remains unverified. Host work
measurements and RPCS3 observations must be kept separate from console FPS.

- [Build and test](docs/BUILD_CURRENT.md)
- [Reproduce benchmarks and interpret metrics](docs/BENCHMARKING.md)
- [v5 measurements and remaining limits](docs/PERFORMANCE_V5.md)
- [Licenses and attribution](NOTICE.md)

Build with `scripts/build-puffle.sh release` after bootstrapping the pinned
toolchain. The optional diagnostic build uses the same title ID, RFLP00001,
and adds VM/opcode/GC counters. No game SWFs or console firmware are included.
The original menus and gameplay simulation scheduling are preserved.

Code lives in `src/` and `include/` (PS3 C frontend/platform), `player/src/`
(Rust Ruffle integration/rendering), `runtime/` (PS3 Rust standard-library
hooks), `vendor/` (licensed third-party components), and `scripts/` (pinned
builds/patches/benchmarks). `deps/` and build outputs are regenerated locally.
Old reports remain under `docs/` and `validation/` with their historical dates.
The previous README is retained as HISTORICAL_README_01.08.md.
