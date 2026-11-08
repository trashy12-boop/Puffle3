# Engineering state — native player 0.2

Ruffle revision: 1125b0018c110fb7a1e51f080c7b47d5dc94d64f.
PSL1GHT revision: f649a08fd536a9e27c08c7db2d93a2d7ee4c3bbe.
RPCS3 revision: 6c44fff7549e941389a4ce072a95c5ff9db7237c.
Rust: 1.99.1 (58cf8f9c3 2026-10-13), rust-src rebuilt with build-std.

The earlier unmodified custom-target std probe failed. This build resolves
that specific blocker with restricted platform modules, not a complete std
port. `runtime/patch_std.py` adds PS3 selection to allocation, clock, random,
logging, generic I/O errors and single-thread TLS. JSON target uses PPC64
big-endian ELFv1, CPU 970, static relocation and singlethread=true. All Rust
entry points, including the mixer proxy, execute on the main PPE thread.
Rust thread spawning, filesystem and networking remain unsupported.

The staticlib links with ppu-gcc and PSL1GHT. Rust calls C hooks for aligned
allocation, free, monotonic/real clocks, sysGetRandomNumber, logs, framebuffer
upload and SharedObject records. The runtime checks the Rust version before
patching. ELFv1 cross-calls were validated by allocating/freeing a Rust Vec,
formatting a clock, and executing actual Ruffle player calls in RPCS3.

PlayerBuilder uses the actual Ruffle core, custom RenderBackend, AudioMixer
backend and StorageBackend. Player::tick keeps Ruffle's timing behavior and
execution limits. PlayerEvent translates keyboard/mouse events from the C
mapper. Pausing stops calls to tick and stops the PS3 audio port; there is no
background Rust game thread. Closing drops the player and its resources.

Renderer path registration converts and caches Ruffle's DistilledShape paths.
Display transforms are applied when painting. Framebuffers are premultiplied
RGBA byte arrays, independent of native endianness, uploaded through rsxkit.
Only presentation/scaling runs on RSX. There is no SPU or JIT work and no claim
of measured optimization speedup. Rust release optimization level is 2.

Audio uses Ruffle's own software mixer and decoder implementations. A C main
loop polls the PS3 ring's read index and fills consumed blocks. The OS audio
server consumes that ring independently; it never calls into Rust. This avoids
using single-thread TLS/locks on an audio worker. Slow frames can underrun.

Profile updates and SharedObject replacement handle LV2's non-POSIX rename:
renaming over an existing destination returns CELL_EEXIST. The old record is
moved to a backup, the new record installed, and the backup removed after
success. Readers can recover a backup if the main record is missing. This is
not a power-loss durability guarantee. SharedObjects are path-namespaced.

See README for compatibility gaps and TEST_RESULTS for exactly what ran.
Historical frontend-only reports are retained under HISTORY_* and do not
describe this player build.
