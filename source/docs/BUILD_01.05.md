# Puffle 3 01.05 reproducible build

Target: PS3 homebrew, title ID RFLP00001, existing /dev_hdd0/ruffle paths.
The XMB artwork is the supplied artwork, not regenerated.

## Linux x86_64 recovery

Python 3.12+, GCC, make, curl, tar, xz, patch and standard host build tools are
required. The verified toolchain archives can be installed without root:

```
python3 scripts/bootstrap-linux.py /absolute/path/puffle-tools
. /absolute/path/puffle-tools/env.sh
./scripts/get-ruffle.sh
CARGO_BUILD_JOBS=2 ./scripts/build-player.sh --release
make package
```

Output: build/ruffle-ps3-dev.pkg. The build uses Rust 1.99.1 + rust-src,
PS3DEV nightly-2026-10-12, PSL1GHT, Cg 3.1, the included target/std patch,
and Ruffle 1125b0018c110fb7a1e51f080c7b47d5dc94d64f plus the included patch.
The bootstrap script pins and verifies every toolchain download. Cg is an
external compiler dependency, not redistributed in the app/source archive.

`get-ruffle.sh` accepts a correctly pinned Git checkout or its verified source
archive and applies the VM patch idempotently. Do not replace the dependency
with current Ruffle main. Cargo.lock is part of the build definition.

The source ZIP omits reproducible upstream dependencies and compiler binaries;
its scripts recover them by revision and checksum. It includes the port,
modified tiny-skia/RSXKit sources, upstream patch, fixtures, licenses, build
configuration, and validation evidence. User SWFs and Sony firmware are not
redistributed.

## Checks

```
./scripts/test-portable.sh
cargo test --locked --release --manifest-path player/Cargo.toml --lib
cargo test --locked --release --manifest-path player/Cargo.toml \
  -p 'path+file:///ABSOLUTE/REPO/deps/ruffle/swf#0.3.0' --lib
cargo build --locked --release --manifest-path player/Cargo.toml --examples
```

The benchmark example accepts SWF, frame count, cache/scalar/verify, output
prefix, and optional replay file. `verify` checks each cached frame against
an uncached render of the same command list; these are host tests, not PS3 FPS.
Replays under validation/performance04/replays are retained.

`RP_EXTRA_RUSTFLAGS` can pass diagnostic code-generation options to the PS3
build (for example `-C jump-tables=no`). This is an investigation facility;
a successful compile alone does not establish a performance improvement.
