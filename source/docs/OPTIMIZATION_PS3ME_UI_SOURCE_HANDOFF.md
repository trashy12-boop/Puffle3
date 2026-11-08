# Ruffle PS3: post-v1.04 source optimization + PS3ME UI handoff

Status: **source changes and portable checks only. Not a PS3 release build.**
Base: user-supplied `Ruffle_PS3_01.04_RSX_Source.zip`. The original v1.04 PKG remains unmodified.
Reference UI: `https://github.com/trashy12-boop/PS3ME-a-PS2ME-Fork`, `ps3/src/BrowserUI.cpp`, `InGameMenu.cpp`, and `ControlMenu.cpp` (current `main` at time of work, October 30, 2026).

## Changes

### PS3ME-style UI
- Browser changed to PS3ME's 1280x720 header/path/rule/footer layout, selection-relative list positioning, selection rectangle, left-side icons, and selected game's right-side preview. Translucent gradient and sinusoidal band locations derive from PS3ME's `BrowserUI.cpp`.
- Ruffle pause/settings, input test, and error pages adapted to PS3ME's row spacing and color treatment. Controller bindings, SWF browsing, pause/restart, storage, and game loop remain Ruffle's own code.
- Ruffle tries to load PS3ME's *installed* `/dev_hdd0/game/PS3ME0001/USRDIR/ui/DejaVuSans.ttf` through PSL1GHT's `fontOpenFontFile`. If absent or loading fails, it falls back to the console system font or RSXKit's built-in bitmap font. No font asset is redistributed.
- PS3ME's SDL rendering stack was **not** copied into Ruffle. The RSXKit UI is batched. Decorative background bands run only on non-game browser pages, avoiding any added per-frame work in Flash playback.
- This is a close **layout/visual port**, not verified pixel-by-pixel identical: font rasterization and SWF thumbnails differ from PS3ME's JAR icon handling, and wave geometry is discretized every four pixels to respect a 4,096-quad RSXKit UI limit.

### RSX renderer and CPU hot paths
- Previous working-tree changes: exact 65,536-value lookup for RGBA premultiplication reversal; reusable Rust vertex staging for shape draws, plus stack-backed bitmap/rectangle quads. Instrumentation for GPU submissions, textures, sync, and fallback reasons.
- Added stable, monotonically increasing IDs to Ruffle bitmap handles and used existing bitmap update generations to track GPU texture cache validity.
- **12 MiB persistent RSX texture pool**: static bitmaps now stay in GPU-accessible mapped memory across frame boundaries; identical `(ID, generation)` draws avoid redundant CPU pixel conversion and memory uploads. Updating a bitmap invalidates and rewrites its matching slot at the next frame. A software fallback is used if the available arena is exhausted.
- **4 MiB transient RSX texture pool**: gradients and other nonpersistent data remain temporary and may be reused inside the current frame.
- Flush cache on SWF launch, after an RSX fence, so stale textures cannot collide across games. No invalidation by recycled source pointers.
- Changed the prior `cache_entries` behavior: offscreen bitmap-cache entries are rendered first, then the main frame is **still offered to RSX**. Previously any bitmap-cache entry forced an entire frame through software even if the final display list was RSX-compatible.
- Added separate interval telemetry for persistent RSX cache hits/uploads and transient uploads, in addition to existing fence waits, failed draws, and fallback reasons.

## Tests actually run
Run `./scripts/test-portable.sh` in a host environment with a C compiler. All four checks passed here:
1. RGBA conversion: every channel/alpha combination and representative RGBA pixels.
2. Mock RSX: vertex and upload paths, same-frame/cross-frame texture reuse, update-generation invalidation, reset, and synchronization ordering.
3. Portable platform: browser, profiles, controller/input logic.
4. PS3ME UI geometry: menu row positions, three band layers, highlight/icons and labels.
These are **not** game runs or tests on an RSX device. The Rust player, linked PS3 binary, installed font path, SWF compatibility and overall graphical appearance have not been runtime-validated.

## Unavailable in this environment
- `ppu-gcc`, PSL1GHT, the remaining PS3 toolchain tools, and RPCS3.
- The pinned upstream Ruffle checkout at revision `1125b0018c110fb7a1e51f080c7b47d5dc94d64f` under `deps/ruffle`.
- The three user benchmark SWFs (Bullet Bill, Super Mario 63, The Right Mix). Only small test fixtures are in this source archive.

Consequently **no new `.pkg`, console FPS numbers, RPCS3 claims, tested PS3ME font match, or verified fix for The Right Mix** are supplied. Do not relabel the user's unchanged 01.04 PKG as an optimized build.

## Known risks and future work
- PS3 target C/Rust compilation remains unverified. Validate the new `fontOpenFontFile` path on a console with PS3ME installed, and the fallback without PS3ME.
- Persistent textures are capped at 12 MiB; if many large or unique textures exceed the arena, the frame may fall back to CPU drawing. The replacement policy is intentionally conservative, and is not an unbounded LRU.
- Some masks, advanced blend modes, line operations, display lists with perspective or unsupported filters still trigger full software fallback. AVM1/AVM2 and GC optimization, SPU jobs, and timing investigations await the upstream dependency and benchmark SWFs.
- The offscreen-cache RSX optimization should be validated for frame correctness and performance. Host-mocked command tests do not prove compatibility or GPU synchronization behavior.
- Compare software v1.03 and RSX v1.04 against the new code on the physical PS3. Collect RSX logs to see `persistent hits`, `transient uploads`, fallback reason counts, and frame time. Reject changes causing regressions.

## Existing user-reported performance (not remeasured)
| Version | Renderer | Real PS3 user observation |
|---|---|---|
| 1.02 | earlier | significantly slower |
| 1.03 | software | improved vs 1.02, still slow |
| 1.04 | hardware | approximately equal to 1.03 |
| new modified source | RSX + fallback | **not built or tested on PS3** |

Specific user reports: Bullet Bill menu smooth but gameplay slow; Super Mario 63 menu approximately 2-3 FPS; The Right Mix black background and controls not responding in that game, despite other games accepting the controls.

## Changelog (source only)
- UI: ported PS3ME browser and settings layouts to native RSXKit, with optional font loading from installed PS3ME.
- GPU: implemented generation-safe cross-frame bitmap residency and finer texture-use telemetry.
- Render: allow RSX drawing after offscreen bitmap caches update, rather than always falling back to CPU.
- Tests: added PS3ME layout checks and portable test runner.

See `NOTICE.md`, `COPYING.PS3ME` and existing third-party notices for licensing.
