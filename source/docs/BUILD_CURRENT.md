# Build the current Puffle3 source

The complete buildable tree is now in `source/`, with C frontend/platform code,
Rust integration, vendored libraries, original tests, locks and upstream
patches. The old `v4-source/` directory is a partial historical snapshot and is
not the current build root.

From a Linux checkout:

```sh
cd source
python3 scripts/bootstrap-linux.py /absolute/tools
. /absolute/tools/env.sh
scripts/get-ruffle.sh
scripts/test-portable.sh
cargo test --locked --release --manifest-path player/Cargo.toml \
  --features host-rsx-probe,profile-vm,path-snapshots,avm1-fast-paths \
  -- --test-threads=1
scripts/build-puffle.sh release
# Optional instrumentation:
scripts/build-puffle.sh diagnostic
```

The bootstrap pins Rust 1.99.1 and rust-src, PS3DEV nightly-2026-10-12,
PSL1GHT and NVIDIA Cg 3.1. It verifies artifact checksums; it does not bundle a
proprietary Sony SDK. Dependency acquisition pins Ruffle
`1125b0018c110fb7a1e51f080c7b47d5dc94d64f` and applies patches 05, 06, 08, 09,
then 10. A recorded patch changing in place fails safely; recreate only the
local `deps/ruffle` working copy to reapply the chain. Build outputs and fetched
dependencies are intentionally excluded from Git.

Outputs are `build/Puffle3_01.10_v6.pkg`, `.elf` and `.raw.elf`. The diagnostic
variant has `_diagnostic` in its name. Keep `.raw.elf` for symbols. The PKG is
finalized for homebrew installation, uses APP_VER 01.10 and title ID RFLP00001,
and contains no game SWFs. It opens the original selection screen; there is no
Mario-only auto-launch. Saves, profile identity, controller settings, sound and
32 Hz simulation scheduling remain unchanged.

The normal and diagnostic builds enable v4 path snapshots and the v5 own
stored-property lookup / short call argument optimizations. `baseline` retains
v4 path snapshots but disables the v5 AVM1 optimizations, with matching diagnostics.
It includes the current renderer/cache changes; use historical source checkouts
for actual historical-version comparisons.
`path-baseline` also disables path snapshots for older comparisons.

Licenses and attribution remain per component. See root LICENSE,
source/LICENSE, COPYING.PS3ME, NOTICE.md and licenses/. The native combined
frontend remains GPL-2.0-only; original Puffle3 components and upstream code
retain their stated terms. `collect-licenses.py` assembles build dependency
notices into the PKG. No game assets, console firmware or proprietary SDK are
part of the repository.

Use [BENCHMARKING.md](BENCHMARKING.md) for timing definitions and reproduction.
A successful build is not a claim of 30 FPS gameplay or complete compatibility.
