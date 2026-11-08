# 01.09 v5

- Retained v4 path snapshots and all working renderer/browser/control changes.
- Added feature-gated AVM1 stored-property lookup and inline short-argument paths.
- Added reproducible differential/serial benchmarks, separate allocation counting,
  and native mean/p99 draw-completion interval logging.
- Restored the complete buildable project to `source/` on GitHub main, with the
  pinned upstream patch chain, toolchain locks, tests, licenses and current docs.
- Built normal and diagnostic 01.09 packages for the existing RFLP00001 title.

See PERFORMANCE_V5.md for actual results, variability and remaining limitations.
Stable 30–32 FPS gameplay on physical PS3 is still not established.
