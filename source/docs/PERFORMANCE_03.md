# Performance revision 0.3 / SFO 01.02

User hardware report: Super Mario 63, Bullet Bill and The Right Mix initialize
in the 0.2 native player, but performance is extremely poor. This is not a
claim of full compatibility or playability. Developer does not possess the
user's exact SWFs and did not benchmark those games.

Changes:
- Identity-color bitmap commands borrow decoded premultiplied pixels, removing
  a full bitmap clone, unpremultiply/color loop and repremultiply for each draw.
  Real color transforms still use the prior processing path. Keeping identity
  pixels also avoids the previous unnecessary rounding damage to alpha edges.
- Player::needs_render controls new rasterization/uploads. Presentation and
  the virtual cursor can update at display refresh without repeating Flash
  rasterization. Paused games reuse the uploaded image.
- Tick scheduling uses Player::time_til_next_frame, which includes timer
  deadlines, bounded to 1–16 ms. Time passed to tick is accumulated wall time,
  including CPU work and display waits, and resets across pause/restart.
  Input events wake the next tick. No forced Flash frame-rate or speed change.
- Native log handle stays open with a 4 KiB buffer, flushed every two seconds
  and at orderly shutdown. Crash logs may lose their last buffered bytes.
- Pause menu and frontend log expose new rendered frames/s, average render
  cost (including upload), average tick cost, and average audio-pump call cost.
  Draw/s is not ActionScript FPS. Tick averages include calls without a timeline
  frame; audio averages include calls when no ring block needs filling. These
  values identify work categories but are not a full profiler.

Validation executed for this revision:
- Native Rust release staticlib and PPU ELF/NPDRM PKG build succeeded.
- Seven release host tests passed, covering real AVM1 input/rendering, dirty
  frame reuse, nested masks, offscreen readback, audio PCM, oversized movies,
  borrowed alpha pixels and bitmap texture updates. One benchmark is ignored
  by the default test run and was executed separately.
- Portable platform tests passed ASan/UBSan (LeakSanitizer disabled).
- Release host synthetic bitmap benchmark: 200 draws of a 1024x1024 opaque
  bitmap scaled to 64x64. Old loop 3261.304 ms; new loop 40.868 ms (~79.8x
  for this artificial workload only). Output bytes identical. Removes 4 MiB
  transient bitmap per draw in this case. It does NOT imply a 79.8x game gain,
  console measurement, or improvement in vector/script dominated games.
- Final PKG installed through RPCS3, then its installed EBOOT booted to browser,
  loaded the original legal interactive SWF, accepted keyboard movement, paused,
  resumed, restarted and returned to browser. Diagnostic timing was visible.
  Tiny fixture reports around 30 new draws/s, ~1 ms render, ~0.2–0.6 ms tick
  under the sandbox's software OpenGL/static PPU interpreter. These are NOT
  PS3 measurements and do not establish large-game performance.
- RPCS3 installer exits with its fixed_typemap teardown assertion after
  extraction, as in 0.2. The extracted installed EBOOT passed the smoke test.
- No real PS3 test of 0.3, audible physical audio test, or new actual-game
  benchmark was performed. CPU vector rendering remains the central graphics
  limitation. No SPU, JIT or RSX vector acceleration is claimed.

Build and install as README. New package filename includes 01.02 to distinguish
it from the menu-only prototype and 0.2 player. PKG title ID remains RFLP00001;
no games, firmware, SDK or emulator are distributed.
