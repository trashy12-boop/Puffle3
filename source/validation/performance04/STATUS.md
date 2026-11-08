# Checkpoint — optimization in progress

Implemented: bounded exact-destination path draw cache (8 MiB), PowerPC
portable SIMD in vendored tiny-skia, AltiVec code generation, host replay
runner, cache counters in native logs. Native executable/package cross-build
succeeded. Ten active host regression tests passed before the latest diagnostics
and replay-clock additions. Native fixture boots, accepts input, pauses,
restarts and exits in RPCS3. The Right Mix title screen measured roughly
17–18 draws/s versus 3.3–3.5 in 01.02 under software OpenGL/static PPU.
No physical-console optimization result. Host gameplay comparisons pending:
first attempts had different clocks and RNG seeds, so are not authoritative.
Host clock shim is Linux-only and must never enter native PS3 build.
