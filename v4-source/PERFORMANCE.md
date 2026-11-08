# Performance and compatibility — v4

## Real-console evidence supplied by the user

The user reports that the previous optimizations improved Bartender's Right Mix, made Super Mario 63's menu run well, and left Mario gameplay at approximately **8–10 FPS**. This is user-reported hardware evidence for the previous build, not an independent measurement of v4. No physical PS3 is attached to this development environment. The 30 FPS gameplay target remains unmet/unverified.

## Controlled host comparison

Same uploaded SWF (SHA256 b1ddb2f7ac6c09e7154771ac2335d962d58536b8dba5720b8c5ff18bb65c917a), deterministic 32 Hz clock, identical input replay, sound mixer, 320×240 viewport and renderer. Two separate optimized host executables differ in the `path-snapshots` feature. Both enable diagnostic VM instrumentation. The host command recorder additionally generates software reference images; its render timings are not PS3 GPU timings.

| Late gameplay, frames 2800–3399 | Previous runtime | Path snapshots |
|---|---:|---:|
| Mean logic time | 5.427 ms | 4.665 ms |
| Mean VM time (inside logic) | 3.733 ms | 3.123 ms |
| Mean GC time | 1.023 ms | 0.884 ms |
| Mean hit-test time (inside VM) | 0.115 ms | 0.116 ms |

In this comparison, gameplay logic time fell **14.0%**, VM time **16.3%**, and GC time **13.6%**. These are one paired host measurement, with ordinary host timing variability; they do not predict a particular console FPS. Collision time was essentially unchanged. Across all 3400 frames, logic time fell 13.3% and VM time 16.5%. No renderer-time speedup is attributed to this VM change.

All 3400 RSX command rows are identical; all 115 sampled software reference images match byte-for-byte. Opcode/get/set/call/hit-test/simulation-frame counts match for every frame. No physics, assets, effects, simulation steps or gameplay mechanics were removed.

The cost eliminated is repeated path-string construction and interning for already-known MovieClips. The bounded cache validates the entire current ancestry before a hit, including root depth. It keeps immutable text separate from each reference's mutable weak-object resolution state. This reduces allocation/GC work while retaining rename and removed/recreated-clip behavior.

## Correctness checks

- Portable C checks passed: RGBA conversion, RSX mock upload/reuse/synchronization, platform controls/storage, browser layout and external-font RGB/alpha coverage.
- Rust: 20 passed, 1 ignored, 0 failed. The ignored test is the existing native SIMD-specific test unavailable on this host.
- Fourteen pinned upstream AVM1 reference fixtures produced identical before/after traces. Thirteen match upstream expected output. `string_paths_reference_launder` remains the same known upstream failure; it is not claimed fixed.
- One original fixture explicitly verifies leaf rename, ancestor rename and independent reference resolution after removal/recreation. Both binaries match all six expected traces.
- Normal and diagnostic PS3 ELF/SELF/PKG builds completed. The packages contain no game SWFs.

## Native emulator testing

See the appended test record below. RPCS3 is not physical PS3 performance. The emulator uses LLVM PPU recompilation and Vulkan with Mesa llvmpipe on a shared Linux host. Audio/Music handlers are Null, so audible behavior cannot be validated here.

## Remaining limits

The original SWF is Flash 8 / AVM1, 450×300 and 32 Hz. Its simulation timing remains intact; the inherited 320×240 viewport is unchanged. The renderer still has inherited advanced-blend/filter/Stage3D limitations, gradient approximations and software fallbacks. Full-game progression, all abilities, bosses, complete save/reload, physical-console audio, long-session stability and 30 FPS normal gameplay remain unverified. This is an incremental Puffle optimization build, not a finished native Mario port.

Previous architectural analysis and v3 tests are retained in `history/`; they are labeled historical and must not be treated as v4 measurements.

## v4 diagnostic RPCS3 record

The diagnostic executable booted the original browser with visibly white external-font labels, launched the uploaded Mario SWF, navigated the title/story/file-create screens, played the castle introduction and entered the Mushroom Kingdom tutorial. Movement/camera changes and jump input were exercised; spin input was sent without a separate complete attack test. A 5.5 KiB SharedObject file was written; this alone does not establish a successful game-save/reload cycle.

Across 34 selected tutorial timing intervals, observed drawing averaged **13.07 frames/s**, ranging **12.0–14.4**. Simulation averaged **27.11 frames/s**, ranging **25.66–28.83**. Mean native render time was **7.09 ms**, versus **26.51 ms per tick**; ticks can contain multiple simulation steps. VM accounting accumulated roughly **1.03 s** and GC **0.40 s** per approximately two-second report interval; collision (included in VM) was approximately **0.033 s**. These results identify VM/GC work as the main remaining cost in this emulator run. They do not demonstrate faster RPCS3 FPS than v3: the runs are not matched performance comparisons.

The original game's HUD can display a value near 30 while actual drawing is near 13; that HUD is not used as proof of 30 FPS. No new allocation failure or failed RSX draw was recorded in the captured v4 diagnostic run. Rust requested-payload peak was 111,461 KiB; this excludes allocator overhead, C memory and graphics allocations. The final package differs from the diagnostic gameplay-tested ELF by a cosmetic 01.07→01.08 browser version label; runtime code/features are unchanged. The final normal package is tested separately below.

The **final normal PKG installed successfully in RPCS3**, appearing as Puffle 3 Emulator, RFLP00001, version01.08. The installed EBOOT and bundled font are present. A separate normal-release boot/gameplay run was not completed before this checkpoint; gameplay evidence above belongs to the diagnostic runtime. No physical-console v4 FPS has been measured.
