# Gradient residency follow-up

The RSX backend now assigns immutable gradient LUTs persistent identities from the same allocator used for bitmaps. Color transforms remain draw uniforms; changing them does not invalidate the LUT. Resource reclamation follows the existing post-fence eviction mechanism. The old path uploaded the same 1 KiB LUT on every draw.

Portable C tests passed, including a new cross-frame gradient residency test with different color/opacity uniforms. Host player tests: 10 passed, 1 ignored. No FPS gain or Right Mix native fix has been established. Native rebuild validation is recorded separately when completed.

The bootstrap script now handles --help without starting tool downloads.

Native Rust/C/shader compilation and finalized PKG generation completed. Runtime gameplay was not tested in this follow-up. SFO remains version 01.05; this is a development variant.

PKG SHA256: `aed5f5529c2217f791caf56e348bd4150c6b55bf352a03fe6ac1cc5c623ed0ae`

EBOOT SHA256: `f291baf252047a59c8ec6200bf80b6da7bd173d3d6bf18427e430827c37496a6`
