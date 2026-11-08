# Ruffle PS3 01.04 RSX checkpoint

This checkpoint contains the experimental RSX mesh renderer work restored and built in October 2026. The renderer tessellates supported Ruffle vector paths into GPU triangles, supports solid, bitmap and RGB-interpolated gradient fills, and submits them through the vendored `rsxkit` RSX backend. It routes a whole unsupported command frame to the existing software renderer for masks, non-normal blends, line commands, perspective, Stage3D, unsupported gradient interpolation and offscreen bitmap-cache cases. The normal software path remains available.

The RSX backend is experimental. RPCS3 was used with Vulkan lavapipe, which is software rendering, so its timing is not a PS3 or hardware-accelerated FPS measurement. A simple interactive fixture rendered with both builds; screenshots differed only at 960 pixels around antialiased borders in a 960x960 comparison. The Right Mix was observed drawing at roughly 11.5–12.5 newly rendered frames per second in the RSX build versus roughly 18 in the 01.03 software build in this emulator setup. That result is a regression in this test environment, not evidence of real-console performance. The supplied-game comparisons are incomplete; Mario 63 and Bullet Bill still need matched captures and measurements. No physical PS3 validation has been performed.

## Source and build identity

- Application version: 01.04, title ID `RFLP00001`.
- Ruffle source: pinned commit `1125b0018c110fb7a1e51f080c7b47d5dc94d64f` in `deps/ruffle`.
- Restored `vendor/rsxkit/src/target.c` from Salsa64 revision `8ee85c3` (MIT).
- Rust: 1.99.1 with `rust-src`; PS3 target spec: `runtime/powerpc64-sony-ps3.json`.
- PS3DEV/PSL1GHT: nightly-2026-10-12.
- The generated build products are in `build/`; the package is `build/ruffle-ps3-dev.pkg` and the stripped executable is `build/ruffle-ps3-dev.elf`.

## Rebuild

Provide Rust 1.99.1 plus rust-src, PS3DEV/PSL1GHT (including Cg and `ppu-gcc`), Python 3, Cargo dependencies, and the Rust target support. Set `RUSTUP_HOME` and `CARGO_HOME` to the selected Rust installation. Put `$PS3DEV/bin`, `$PS3DEV/ppu/bin`, and the Rust toolchain's bin directory on `PATH`. If Cg libraries are outside the system linker path, add their directory to `LD_LIBRARY_PATH`.

From the project root:

```sh
CARGO_BUILD_JOBS=2 ./scripts/build-player.sh --release
make package
```

The build script applies the PS3 `std` runtime patch to that toolchain's `rust-src`, builds with `ps3-simd,ps3-rsx`, then `make package` links the RSX library and creates the installable PKG. Do not distribute Sony firmware or the supplied SWF games with this source bundle.

## Saved validation evidence

`validation/rsx-session/` retains the RPCS3 application and renderer logs and selected local screenshots captured during this work. `validation/README.md` describes the existing RPCS3 session tooling. The fixture screenshots are an interactive smoke check only. Renderer correctness, full-game visual parity, memory pressure on a 256 MB console, sustained console frame rate, audio behavior and long-session stability still require testing on hardware.
