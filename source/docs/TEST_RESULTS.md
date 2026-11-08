# Latest revision

See [0.3 validation](PERFORMANCE_03.md) for the new build. The following is
historical 0.2 evidence; its physical-console status is superseded by the
user report documented there.

# Native player 0.2 validation — 2026-10-28

## RPCS3 native execution

RPCS3 0.0.43-20415 / 6c44fff, Linux x86-64, Xvfb, Mesa software OpenGL,
static PPU interpreter, Async Recompiler (multi-threaded) shaders.
Official firmware 4.93 was downloaded from the HTTP link on Sony's support
page and installed locally; firmware is not distributed with this project.
The shader-interpreter mode requires an unavailable GL extension on this
software driver, so that mode was disabled.

Executed through the actual PS3 ELF, not the x86 host runner:

* Rust ABI/runtime probe: Vec allocation, formatting, clock and C callbacks.
* Upstream AVM1 trace SWF: native trace output emitted.
* Original vector/keyboard SWF: red shape rendered; mapped Square->Z changed
  its ActionScript-controlled position. Screenshots recorded before/after.
* Protected L3+R3 pause and resume; controller profile editor while paused;
  profile change; restart; return to browser and launch another SWF.
* Upstream AVM2 trace SWF: expected visible trace strings, including multi-arg
  values, emitted on the native PS3 player. This is not an AS3 game benchmark.
* Original PCM tone SWF: AUDIO nonzero PCM reached PS3 output ring logged.
  No physical speakers or PS3 audible-output verification was available.
* SharedObject SWF: first launch traced PS3_SAVE_READ:undefined; relaunch
  traced PS3_SAVE_READ:7. The C record persisted between player instances.
* CWS-compressed mouse SWF: shape followed virtual mouse movement; mapped
  Cross click emitted PS3_MOUSE_DOWN. No copyrighted game was used.

The Rust requested-payload heap peak in this small-fixture session was
16,280 KiB (about 15.9 MiB). After several player closes, the counter returned
to 1 KiB. These counters exclude the executable, stack, C/allocator overhead
and RSX memory. They are not total process memory or a large-game budget.
No reliable real-PS3 frame-rate measurement or before/after optimization
benchmark has been obtained. RPCS3 performance is not console performance.

RPCS3 headless package installation extracts files but exits with a
fixed_typemap teardown assertion (exit 134). This is an emulator installer
shutdown problem; it is not claimed as a clean installation result.
Installed-EBOOT boot verification is recorded below after execution.

## Host

`cargo test --manifest-path player/Cargo.toml`: five tests passed:

1. Actual Ruffle AVM1 controller input moves the software-rendered vector.
2. Nested masks clip and popping a mask restores its parent clip.
3. Oversized declared SWF length is rejected before Ruffle decoding.
4. An embedded PCM sound reaches Ruffle's mixer with finite, nonzero samples.
5. Offscreen bitmap rendering/readback has the expected RGBA bytes and restores
   the stage framebuffer.

Host SharedObject callbacks in these Rust tests are test hooks. Persistent C
storage was exercised in RPCS3, not established by those hooks.

The portable C input/browser/profile tests also run with address and
undefined-behavior sanitizers. LeakSanitizer is disabled because the sandbox
cannot enumerate its threads. This does not prove absence of leaks.

## Physical PS3

The user confirmed the preceding menu builds launched and worked. This new
native-player build has not been tested on physical hardware. Its SWF
compatibility, audio, speed, stability and memory behavior need console tests.
No demanding game, large SWF or sustained multi-hour session is validated.

The final package was extracted by RPCS3's headless installer. Its installed
`dev_hdd0/game/RFLP00001/USRDIR/EBOOT.BIN` reached the browser and launched the
upstream AVM2 trace fixture, producing native trace output. This verifies the
packaged executable as well as the direct ELF. It does not establish console
installation or game performance.

The final license-bearing PKG was installed again (same teardown assertion).
Its installed EBOOT completed the full automated session: vector/key input,
pause/settings/restart/exit, AVM2 trace, PCM ring, repeated SharedObject reads,
and CWS mouse input. Native logs and selected screenshots are under
validation/evidence/. No physical-console test is implied.
