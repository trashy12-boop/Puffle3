# Attribution

Native settings layout/navigation is adapted from the user's PS3ME 01.08
project, ps3/src/InGameMenu.cpp (GPL-2.0-only). The original reference and full
GPL text are included. That source snapshot's SHA-256 is documented in the
reference history. The combined native application is GPL-2.0-only.

Ruffle: https://github.com/ruffle-rs/ruffle, revision
1125b0018c110fb7a1e51f080c7b47d5dc94d64f; upstream licenses retained.
Salsa64 rsxkit: https://github.com/ricowp81/Salsa64, revision 8ee85c3;
vendor/rsxkit retains its MIT and third-party notices.
PS3 custom-target research: https://github.com/ZephyrCodesStuff/rust-ps3,
revision 476856b950a0bc77995cf0dbde6b0a1193b039b8.
Rust 1.99.1: https://github.com/rust-lang/rust. The std time and stdio module
files derive from that release's unsupported-platform implementations and
retain Rust's MIT OR Apache-2.0 terms; the new PS3 hooks and patcher are MIT.
PSL1GHT: https://github.com/ps3dev/PSL1GHT, revision
f649a08fd536a9e27c08c7db2d93a2d7ee4c3bbe.

Dependency notices are bundled under licenses/ for the crates downloaded in
this build; this collection includes build-time crates as well as libraries.
Cargo.lock records resolved versions. Noto Sans/Noto Sans Math subsets are
included by Ruffle's default_font feature and retain their OFL notices.
The compiler/SDK and firmware are separate installation dependencies.

Tiny-skia 0.11.4 is now vendored under vendor/tiny-skia with its BSD license.
The portable SIMD dispatch added in wide/{f32x4,i32x4,u32x4}_t.rs is an
original modification; scalar dispatch remains available without ps3-simd.
The new draw cache and Linux-only benchmark clock shim use the MIT license.

Puffle 01.05: bundled DejaVuSans.ttf follows the Bitstream Vera / DejaVu
license in licenses/DejaVu-LICENSE.txt. The app uses its own bundled font and
does not require a PS3ME installation. BrowserUI.cpp (PS3ME 01.23 reference)
was inspected on 2026-11-01 and retained under its GPL-2.0-only header.
Tiny-skia gradient stop sharing uses copy-on-write Arc storage; existing BSD
terms and attribution remain intact. The AVM1 decoder patch and cache
admission / residency changes retain the surrounding source licenses.
