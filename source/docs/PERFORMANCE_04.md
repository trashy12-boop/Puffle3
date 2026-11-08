# Ruffle PS3 01.03 — measured rendering optimizations

This is a continuation of the working 01.02 player, not a new Flash implementation.
The actual pinned Ruffle core, its ActionScript interpreters, existing frontend,
controller profiles, saves, and audio backend remain in use. No game-specific
runtime hacks, frame skipping, resolution reduction, or disabled antialiasing
are introduced. This remains an experimental port, with substantial performance
and compatibility limitations.

## What changed

### PowerPC SIMD in the software raster pipeline

The previous custom Rust target explicitly disabled AltiVec. Tiny-skia's
four-lane float and integer dispatch also had no PowerPC vector branch.

The new native build enables `+altivec` and selects the vendored tiny-skia
`ps3-simd` feature. Four-lane arithmetic, float comparisons, bitwise operations,
integer comparisons/conversions, and shifts use Rust portable SIMD. Existing
array layouts and bytemuck representations are retained. Wider float pipeline
values reuse these four-lane operations. The scalar implementation remains
available when this feature is absent.

The implementation retains ordinary floating-point semantics and the original
fallbacks for square roots, reciprocal operations, rounding and other operations
that were not explicitly ported. It does not enable fast-math or replace exact
operations with estimate instructions. Comparison masks retain their all-bits-set
representation. Min/max ordering follows the previous scalar implementation.
The default native CPU remains the existing LLVM `970` target with AltiVec;
C platform code retains `-mcpu=cell -O2`. Rust remains opt-level 2 with 16
codegen units. No LTO/PGO improvement is claimed.

Disassembly of the native executable confirms vector instructions, including
911 `vaddfp`, 551 `vsubfp`, 1,067 `vmaddfp`, 417 `vsel`, and 412 `vcmpgtfp`
instructions in the inspected build. Counts describe code generation, not
execution frequency or proof of a speedup on hardware. Native RPCS3 runs exercise
the resulting executable without an illegal-instruction failure.

### Bounded, exact destination-dependent vector draw cache

`player/src/draw_cache.rs` memoizes eligible individual path draws. It stores
both the destination rectangle before the original rasterization and its exact
result afterward. A hit first compares every current destination byte with the
saved input. It copies the result only if those bytes match. This preserves
source-over rounding and translucent edges without treating a translucent draw
as an opaque sprite or compositing a second rasterized layer.

Keys include:

- A monotonically assigned registered path ID, avoiding recycled-pointer keys.
- Exact transform float bits and all color-transform fields.
- Source bitmap mutation generation, when a fill references a bitmap.
- Target width and height.

The cached rectangle conservatively encloses transformed path bounds, stroke
joins/caps and antialiasing expansion, clipped to the target. Draws inside masks
or with an active clip take the original rendering path. Unsupported or
unsuitable bounds also fall back. Bitmap texture updates and offscreen rendering
increment the bitmap generation, preventing reuse after mutable bitmap changes.

The retained pixel payload is capped at 8 MiB with at most 1,024 entries and
insertion-order eviction. Individual candidate rectangles are limited to
256 KiB. Metadata and temporary before/after snapshots use additional memory;
this is not an 8 MiB cap on total process memory. Cache contents are owned by the
renderer and released with the player. Existing Rust heap limits remain intact.

This helps repeated complex drawings with identical backgrounds. Moving shapes,
changing backgrounds, frequently mutated bitmaps, masks and cache eviction can
still require full rasterization. Cache comparisons and copies also have a cost.
This is not a universal scene cache or a substitute for GPU vector rendering.

### Measurements and reproducible host replay

Native periodic logs retain existing draw/logic/audio measurements and add path
calls, cache hits/misses and retained cache bytes. The frontend version banner
and SFO identify 01.03; the title ID remains `RFLP00001`.

The Linux host example runs the same player and renderer with full rendering,
input replays and audio mixing. It advances one movie frame per iteration and
updates timers by the declared frame duration. A Linux-only clock shim freezes
the movie clock during event delivery and updates; this makes `getTimer`, Date
and RNG seeds reproducible. Real clocks are used for benchmark measurements.
The shim is not compiled or linked into the native PS3 application. Earlier
uncontrolled runs had different time/RNG state and are not used as authoritative
pixel or gameplay comparisons.

`verify` mode executes each submitted frame's identical command list through
both cached and uncached rendering and checks the complete pixel buffers.
Its extra rendering time is validation overhead and is excluded from performance
tables. Separate scalar and SIMD executable runs check sampled frame bytes.

## Supplied SWFs

`validation/performance04/swf-structure.json` records checksums and nested tag
counts. These files are used only as local benchmarks; no supplied game is
included in the PKG or source archive.

| Game | Flash | Stage | Declared rate | Shape definitions | MovieClips | Action tags |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| Super Mario 63 | 8 | 450×300 | 32 FPS | 6,177 | 2,314 | 2,611 |
| Bullet Bill | 8 | 750×500 | 32 FPS | 78 | 78 | 171 |
| The Right Mix | 7 | 600×440 | 18 FPS | 334 | 162 | 74 |

Action counts include DoAction and DoInitAction. No DoABC tags are present in
these supplied movies: this is an AVM1 workload suite, not an AVM2 game benchmark.
Mario also defines 1,218 lossless bitmaps and 1,173 sound-stream blocks. Its
compressed file is 15,295,370 bytes; the declared uncompressed size is 19,339,460.
Asset decoding, display-list work, audio and scripting therefore remain relevant
in addition to rasterization.

## Controlled host comparison

Both variants use 320×240 output, antialiasing, rendering, audio and identical
recorded inputs. The baseline uses the original scalar tiny-skia operations with
caching disabled; the optimized executable uses SIMD and caching. Per-path
profiling is enabled in both. Each row is one controlled run, not a confidence
interval. Host scheduling can affect timings. Logic contains ActionScript,
display-list/timer work and GC; it is not an isolated interpreter measurement.

| Replay | Simulated frames | Baseline render time | Optimized render time | Render-work ratio |
| --- | ---: | ---: | ---: | ---: |
| Mario title / attract sequence | 2,100 | 18.008 s | 11.204 s | 1.61× |
| Bullet Bill startup + gameplay | 1,200 | 1.970 s | 1.426 s | 1.38× |
| The Right Mix startup + mixing screen | 600 | 8.289 s | 0.601 s | 13.79× |

| Replay | Baseline logic | Optimized logic | Baseline audio | Optimized audio |
| --- | ---: | ---: | ---: | ---: |
| Mario title / attract | 5.915 s | 5.721 s | 0.429 s | 0.386 s |
| Bullet Bill | 2.779 s | 2.737 s | 0.076 s | 0.078 s |
| The Right Mix | 0.766 s | 0.597 s | 0.102 s | 0.087 s |

These totals are work required to process a fixed replay. They are not achieved
console FPS, display-refresh FPS, or evidence that entire games are playable.
The changes do not optimize the interpreter or audio decoder, and small
variations in those categories must not be represented as interpreter/audio
speedups. Bullet Bill's logic is the largest measured category after these
rendering changes. Mario still spends substantial time rendering and updating.

All submitted frames passed cache-versus-uncached validation across these 3,900
simulated frames. Across the separate scalar/SIMD runs, all 133 sampled RGBA
frames matched byte for byte. This is about 40.9 MB of compared frame pixels.
The Right Mix recorded 139,881 cache hits from 151,864 path calls; Bullet Bill
5,144/16,344; Mario 51,599/370,339. Counts include paths that bypass caching.

An additional Mario tutorial gameplay replay is recorded separately below;
the title/attract row is not mislabeled as player-controlled gameplay.

The 3,400-frame Mario replay selected Story, created a save, reached the tutorial, and sent movement and jump inputs. Total host render time fell from **35.236 s to 20.617 s (1.71×)**. All 115 sampled frames matched between scalar and SIMD runs. The replay includes menus and the opening sequence, so this is not a gameplay-only FPS measurement.

## Native RPCS3 validation

Environment: RPCS3 0.0.43-20506-0a3adbc5, static PPU interpreter, OpenGL using
software graphics, `Shader Interpreter only`, firmware 4.93 obtained from Sony.
The default shader recompiler failed because the software driver lacks
GL_ARB_bindless_texture; the shader-interpreter configuration rendered correctly.
RPCS3 timings are not representative of real PS3 execution or RSX performance.

- The existing 01.02 ELF and new packaged executable both booted.
- The original interactive fixture launched; keyboard-emulated controller input,
  pause/resume, restart and return to browser were exercised.
- The finalized PKG was extracted through RPCS3. Its installed EBOOT booted and
  passed the same lifecycle smoke sequence. The headless installer aborts during
  its fixed_typemap teardown after extraction, as in previous revisions. This is
  not reported as a clean installer-process exit.
- The installed executable matches the build EBOOT payload, with an additional
  48-byte trailer; hashes and byte counts are in package-integrity.json.
- The Right Mix title scene measured about 3.3–3.5 new draws/s and 273–289 ms per
  draw in 01.02, versus about 17–18 new draws/s and 15–17 ms per draw in 01.03.
  The scene reaches its declared rate in this emulator setup only. Later mixing
  gameplay and physical-console performance are not established by that result.
- Bullet Bill and Mario both launched from their supplied SWFs. Mario's selected
  native startup window remained around 6–8 new draws/s; render and audio-pump
  costs were both substantial. Neither native run establishes sustained gameplay
  or completion. Earlier mislabeled browser-selection attempts are excluded.
- Fixture close/restart returned the Rust requested-payload counter to about
  1 KiB, with about 16 MiB peak. That is a small-fixture allocator observation,
  not a proof of leak freedom or a large-game memory measurement.

Native "draws/s" counts newly rasterized images. Logic timing averages tick
calls, which can include calls without a timeline frame. Audio timing averages
pump calls and can include empty calls. None of these counters alone is a count
of completed ActionScript game frames or an underrun-free audible-audio test.

## Other executed checks

- Ten release host regression tests passed with SIMD enabled; one pre-existing
  synthetic bitmap benchmark remains ignored in the default test run.
- Tests cover AVM1 mapped input and dirty-frame reuse, PCM audio, bitmap identity
  pixels and updates, offscreen readback, nested masks, changing colors/
  transforms/backgrounds, mutable bitmap-fill invalidation, cache eviction and
  memory bounds, and oversized movie rejection.
- Upstream AVM1 and AVM2 trace fixtures matched their expected text output in the
  host player. This does not establish AVM2 game performance on PS3.
- Portable C browser/profile/input checks passed AddressSanitizer and UBSan.
  LeakSanitizer was disabled for the sandbox; Rust/native code is not claimed to
  have received whole-program sanitizer coverage.
- Native Rust release static library, big-endian PPU ELF, shader library and
  installable PKG all built with the pinned toolchain. Build logs are included.

## Hardware use, decisions and remaining bottlenecks

| Component | Used in 01.03 | Remaining work |
| --- | --- | --- |
| PPE | Ruffle AVM1/AVM2, updates, GC, parsing, software rendering, audio | Interpreter/display-list profiling and CPU optimization |
| AltiVec/VMX | Raster-pipeline SIMD and compiler vector code generation | Wider coverage and physical-console profiling |
| RSX | Existing texture upload, scaling and frontend | Native vector tessellation/rasterization, batching, GPU compositing |
| SPUs | No offloading implemented | Measured job sizing, DMA/local-store design and actual speedup validation |
| Extra PPE thread | No Rust player work moved off the main thread | Runtime synchronization and ownership support before parallelization |

The existing Rust platform backend is explicitly single-threaded. Moving player,
GC, RefCell-backed images or the audio proxy to a worker without redesigning
ownership and runtime services would be unsafe. Consequently, no unverified
threading change was added simply to increase apparent hardware utilization.

SPU pixel transforms were not the first target because repeated path raster work
was the large measured cost in the supplied startup scenes. A useful SPU worker
would need coarse jobs that amortize task/DMA overhead and fit its local store;
stateful display traversal and ActionScript cannot just be divided into independent
jobs. No SPU prototype or measured SPU speedup is claimed. RSX vector acceleration
remains the larger architectural rendering project. The present cache avoids
some work but does not move Flash vector rasterization to the GPU.

Audio is still decoded/mixed on the main PPE thread. The native Mario startup
window shows that it remains important. Long updates/renders can still starve
the existing audio ring. No unsupported audio-thread shortcut or blanket
predecode allocation was introduced.

Known pre-existing gaps remain: complex filters and advanced blend modes,
Stage3D, PixelBender, Flash video, network loading, full bitmap-fill color
transforms, exact focal/linear-RGB gradients and some stroke behavior. These
changes preserve the previous renderer's output; matching it does not make those
features Flash-accurate. Resolution remains the previous 320×240.

No physical PS3 test of 01.03 was performed in this session. Its frame rates,
audible output, long-session stability, and behavior across arbitrary SWFs still
need console verification. The 32/32/18 FPS goals are not claimed as achieved on
hardware. This is a substantial measured rendering revision, not proof that
every possible bottleneck has been removed.

## Rebuild and install

Use the README's pinned Rust 1.99.1 + rust-src and PS3DEV/PSL1GHT instructions.
`./scripts/get-ruffle.sh` fetches upstream Ruffle at
1125b0018c110fb7a1e51f080c7b47d5dc94d64f. All modified port/runtime/renderer
and vendored tiny-skia code, licenses, lockfiles, replay events and evidence are
in the source archive. Toolchains, firmware and games are separate dependencies.

Install the 01.03 PKG over Ruffle PS3 on HEN/CFW. Game/profile/save directories
and controls are unchanged: SWFs go in `/dev_hdd0/ruffle/games/` or
`/dev_usb000/ruffle/games/`, Cross launches, and L3+R3 pauses. Logs are in
`/dev_hdd0/ruffle/`. Keep your existing profiles and saves.
