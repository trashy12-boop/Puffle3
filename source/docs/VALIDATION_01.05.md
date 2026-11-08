# Puffle 3 Emulator 01.05 — build, performance and compatibility report

Date: 1 November 2026. **Newly compiled development build; not a validated performance release.**

The package and source delivery are complete. The requested large practical PS3 performance improvement, full RSX compatibility fix, and pixel-identical PS3ME interface are **not established**. This report separates the implemented work from those remaining goals.

## Deliverables and executable identity

- `Puffle3_Emulator_01.05.pkg`: 8,009,184 bytes, produced by Rust/PPU compilation, Cg shader compilation, native linking, SELF creation, SFO generation, packaging and package finalization.
- Title: **Puffle 3 Emulator**, APP_VER **01.05**, title ID **RFLP00001**, existing content ID and storage paths preserved.
- PKG SHA-256: `7bc3cb51f3a2c2bb8ead1e85f443ab28b45e1c7286d507bec9a4261a3427d514`.
- New EBOOT SHA-256: `4d31783b721efe5d07fd9e4e8e4bab0ed73c60ad18e6e892757848fc5d28f1ab` (4,168,864 bytes).
- Original installed 01.03 EBOOT SHA-256: `a9083b5d2b15e9a53ffcee35897cdfc324369c3ccc57dcd27af812b0a9f60a6e`.
- Original installed 01.04 EBOOT SHA-256: `16684272f8c5987c923645066b0f12eb77fd077ceacf389c651e2289f4cd3ca5`.
- RPCS3's installer extracted the final package. Installed executable payload matches the new build byte for byte; the installer appended 16 bytes. PARAM.SFO title, APP_VER and title ID were checked. This validates package installation/extraction, **not PS3 gameplay**.

The package does not contain the old executable with a changed icon. The supplied ICON0/PIC1 artwork was retained. No artwork was regenerated.

## Baseline recovered

The supplied handoff contained branded advanced source, the original RSX and software source/package baselines, an optimization diff, artwork, all three SWFs, and earlier reports. The branded advanced tree was compared against original 01.04 source. Its prior bitmap-residency, conversion and PS3ME layout work was preserved, reviewed and compiled here. Those carried-forward changes are not newly invented 01.05 features.

The exact upstream dependency was recovered from GitHub: Ruffle `1125b0018c110fb7a1e51f080c7b47d5dc94d64f`. Rust **1.99.1**, rust-src, **PS3DEV nightly-2026-10-12**, PSL1GHT and NVIDIA **Cg 3.1** were downloaded and used. Initial tool absence was resolved. The repository now has a checksum-pinned bootstrap, dependency acquisition/patch script, locks, build guide and logs.

## Implemented changes

1. **AVM1 Push allocation:** the existing parser now uses `SmallVec<[Value; 2]>`. Common one/two-value pushes avoid the temporary heap allocation. Larger pushes spill normally; all existing decoding/version/error handling remains in the original parser. An earlier separate fast decoder was removed after full-game timing failed to justify it. Boundary checks cover 0, 1, 2, 3, 32 and 128 values and correct reader positioning. Two million two-value Push instructions caused **zero heap allocations** in the isolated host allocator test. This is an allocation measurement, not console speedup.
2. **Gradient preparation:** immutable paths retain one color-transform generation of their local gradient shader. Tiny-skia gradient stop storage is shared with copy-on-write, avoiding repeated stop copies when paint/shader objects are cloned. Opacity mutation keeps separate copies; color changes invalidate the cached shader.
3. **Software-cache admission:** one-use snapshots at least 16 KiB must be observed twice before being stored, reducing large transient insertions. Smaller draws keep immediate admission. Hash mixing includes the high bits where float translation entropy resides. Applying probation indiscriminately hurt The Right Mix and was rejected. Destination bytes are still checked before replay; no transparency shortcut was introduced.
4. **RSX texture reclamation:** the persistent cache no longer retains departed scene textures forever. Allocation pressure requests collection after the prior GPU fence. Previously used survivors are compacted safely and keep their IDs/revisions. Tests exercise metadata exhaustion, eviction, renewed residency, reuse and update invalidation. This addresses lifetime/capacity behavior; it is not a measured console FPS claim.
5. **Per-game renderer selection:** Controller settings now offers Automatic RSX/fallback or Software. Switching marks the stage dirty without restarting the VM. RPF2 stores the choice; legacy RPF1 files retain existing bindings and load with automatic rendering. Tests verify both formats and redraw on renderer change.
6. **Self-contained font:** DejaVuSans is bundled with its license and loaded from Puffle's own directory. The frontend no longer depends on PS3ME being installed. Branding, title ID, save/storage paths and prior controls remain intact.

## Runtime bottleneck assessment

| Area | Evidence / remaining cost | Result in this build |
|---|---|---|
| AVM1 | The three supplied games are AVM1 workloads; Push parsing repeatedly allocates in upstream code | Common-payload allocation removed; no speculative property cache or semantic changes |
| AVM2 | Not exercised by the three benchmark movies | Interpreter unchanged; no AVM2 performance claim |
| Vector/software drawing | Host Mario runs spend substantially more time in rendering than logic; path drawing remains a major cost | Gradient reuse and more selective snapshot caching; vector rasterization remains on PPU |
| Hardware eligibility | Any unsupported mask/alpha mask, advanced blend, line, perspective or Stage3D operation can force the whole command list to software | This architectural restriction remains; it can erase the benefit of RSX acceleration |
| Geometry | CPU still transforms/expands tessellated geometry and submits per draw | Persistent local geometry, GPU transforms and broad batching were not completed |
| Textures | Prior source now compiled includes stable IDs, generations, cross-frame residency and transient uploads | Added pressure-driven reclamation; mocks validate ordering and lifetime behavior |
| Synchronization | Frame fences and texture synchronization still exist | No hardware timing evidence supports removing them safely |
| Audio / GC / display list | Tick includes VM, timeline, events and related runtime work; audio mixing remains PPU-side | PCM integration retained; no claimed audio/GC overhaul |
| Cell/SPUs/JIT | VM uses GC-managed object graphs; the custom Rust backend is single-threaded | No SPU job or JIT added. No claim of SPU use, accessible-SPU probing or hardware utilization measurement |

The source audit supports why a hardware renderer alone may not help: unsupported commands select the software renderer for an entire frame, while logic, display-list generation and CPU geometry work remain on the PPU. **The dominant cause of the user's real-console slowdown has not been measured conclusively.**

## Tests actually completed

- All **95 SWF library tests passed** with the storage change.
- Player tests: **10 passed, 1 ignored** (the inherited opt-in performance test). Includes actual AVM1 movement/rendering, PCM audio, offscreen readback, nested masks, mutable bitmap invalidation, cache correctness and renderer switching.
- Four portable C suites passed with UBSan: pixel conversion, RSX mocks, controls/profiles/browser, UI geometry.
- The RSX upload/reclamation mock additionally passed **AddressSanitizer + UBSan**.
- The three deterministic SWF replays covered **3,900 simulated frames** (Mario 2,100; Bullet Bill 1,200; Right Mix 600). Each submitted software frame in verify mode was checked against uncached rendering of the same command list.
- **133 sampled output images matched the incoming source byte for byte**: 71 Mario, 41 Bullet Bill, 21 Right Mix. Game input events were replayed unchanged.
- Dependency patch reproduction matched the working-tree dependency files. Final cross-build and package generation completed. Package installation/extraction and binary identity were verified.

These tests do not prove native RSX rendering correctness or real PS3 stability.

## Host benchmark method and results

Linux x86-64 deterministic replay at a **320×240 viewport**, original declared movie timing, full rendering and audio mixing. Replays are in `validation/performance04/replays/`. Timings use the real host clock while Flash timers/RNG timing are frozen during update/input. Source comparison uses the **incoming advanced source**, not the shipped 01.03 or 01.04 PS3 executable. Two runs per variant were attempted in before/new/new/before order. Mario also has an isolated repeat series.

Numbers below are **milliseconds per simulated frame** for stage averages; P95/worst are the sum of logic + render + audio per iteration. They are **not PS3 FPS, not an RSX benchmark, and not native console frame pacing**. Startup and menu transitions are included. Raw CSVs/logs are in the source archive. One earlier Mario baseline run suffered a 23-second host stall during RPCS3 memory exhaustion and is excluded from the summary rather than presented as an optimization win.

| Game | Source | Runs × frames | Logic ms/frame | Render ms/frame | Audio ms/frame | P95 total ms | Worst total ms |
|---|---|---:|---:|---:|---:|---:|---:|
| Super Mario 63 | Incoming advanced source | 2 × 2100 | 3.040 | 7.799 | 0.196 | 25.375 | 76.747 |
| Super Mario 63 | 01.05 source | 2 × 2100 | 2.696 | 7.008 | 0.175 | 20.967 | 93.741 |
| Bullet Bill | Incoming advanced source | 2 × 1200 | 1.850 | 1.319 | 0.057 | 6.703 | 14.507 |
| Bullet Bill | 01.05 source | 2 × 1200 | 1.929 | 1.298 | 0.058 | 7.039 | 13.219 |
| The Right Mix | Incoming advanced source | 2 × 600 | 0.937 | 1.257 | 0.131 | 3.587 | 292.552 |
| The Right Mix | 01.05 source | 2 × 600 | 0.927 | 1.200 | 0.130 | 3.678 | 282.340 |

Host timing differences are modest and noisy. They do **not** demonstrate the requested large practical improvement. The strongest directly measured interpreter result is removal of allocations in the common Push payload test. Do not extrapolate these x86 times into PS3 FPS or claim all costs were profiled separately; VM, display-list and GC substage timings are not separately instrumented here.

## RPCS3 and real-PS3 comparison

| Version | Real PS3 evidence | Fresh native result in this session |
|---|---|---|
| 01.03 software | User: considerably better than 01.02, still slow | Package recovered and installed; fresh benchmark blocked by executable-analysis memory exhaustion |
| 01.04 RSX | User: approximately equal to 01.03 | Original package/source recovered; no fresh matched native FPS comparison |
| 01.05 | No real-PS3 run performed here | Newly compiled/packaged and installer-validated; final executable analysis exceeded host memory before app entry |

RPCS3 **0.0.43-20551-ad468a13** and recovered reference **0.0.43-20506-0a3adbc5** were tried with firmware 4.93, Xvfb and Vulkan lavapipe. LLVM/static PPU decoding, reduced compiler-worker settings and null audio were investigated. Analyzer RAM grew past **8 GB** and processes exited with **-9** before the app entered main. The original 01.03 package also exhibited the problem. An intermediate freshly compiled build did reach the browser and initialize RSX/font loading; **that is not final-package gameplay validation**.

A separate no-jump-table code-generation probe was compiled and did not resolve exhaustion. It was rejected and is **not** the delivered compiler configuration. Logs are retained. The emulator's analyzer failure is distinct from an observed PS3 app crash; the current evidence cannot determine final real-PS3 runtime behavior. No matched native average FPS, frame distribution, RSX upload time, GPU utilization, long-session memory or audio-stall comparison is available.

Historical user observation: Super Mario 63 reached its menu at roughly **2–3 FPS**. That number was not remeasured. Bullet Bill's smooth-menu/slow-gameplay contrast and Right Mix's console issues are retained as unresolved native acceptance tests.

## Compatibility status

| Item | Verified here | Still unverified / incomplete |
|---|---|---|
| Super Mario 63 | 2,100-frame host replay, input/state preservation and sampled visual parity | Practical PS3 menu/gameplay FPS and long-session stability |
| Bullet Bill | 1,200-frame host replay with recorded pointer actions; visual parity | Console gameplay-specific slowdown remains unresolved |
| The Right Mix | 600-frame host software replay reaches the mixing screen, shows the background, responds to recorded mouse actions, and matches prior output | Console RSX black background and input complaint are not claimed fixed |
| Existing functionality | AVM1 movement, PCM, storage hooks, offscreen rendering, controls, profile migration and package paths checked | Full native audio/controller/save lifecycle, AVM2, all Flash filters/blends, real-hardware regression coverage |
| PS3ME identity | Matching bundled font, palette and main layout conventions; current BrowserUI reference inspected | Button artwork, rasterization, animations and every menu are not verified pixel-identical |

For Right Mix, **Software** is a practical option to try, not a verified native fix. Open **L3+R3 → Controller settings → Renderer → Software**. Default pointer movement is the **left stick**; **Cross/R1** is left click. Save the profile. The host evidence confirms mouse-driven behavior; it does not establish why the user's controller failed on PS3.

## Changelog

- Built a new 01.05 PS3 executable and installable package from the modified source.
- Restored the exact Rust/PS3DEV/Cg build environment and added pinned recovery scripts.
- Removed common AVM1 Push allocations using inline storage in the original parser.
- Reused gradient shaders/stops and refined large software-cache admission.
- Reclaimed departed RSX bitmap residents after a safe fence.
- Added persistent per-game software/automatic renderer selection and legacy-profile loading.
- Bundled the frontend font, retained Puffle artwork/title ID and compiled the prior UI/cache work.
- Added regression, allocator, sanitizer, patch-reproduction and package-identity evidence.

This changelog intentionally makes **no large console speedup, fixed native Right Mix, SPU, JIT, full compatibility, or complete PS3ME visual-equivalence claim**.

## Rebuild / handoff

See `docs/BUILD_01.05.md`. `scripts/bootstrap-linux.py`, `scripts/toolchain-lock.json`, `scripts/get-ruffle.sh`, `scripts/ruffle-ps3-05.patch`, Cargo locks and `runtime/` recover the build. The source ZIP includes changed port and vendored sources, original fixture tests, build configuration, licenses, evidence and current documentation. Pinned upstream Ruffle/toolchains are recovered externally; user SWFs and Sony firmware are excluded.

Remaining engineering work is substantial: native frame profiling, masks/filters/blends and software fallback architecture, GPU geometry residency/batching, native Right Mix validation, and real-console acceptance testing. The delivered build is concrete and reviewable, but it is **not a completed extreme-optimization release**.
