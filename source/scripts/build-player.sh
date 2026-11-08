#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
case "$(rustc --version)" in
  "rustc 1.99.1 "*) ;;
  *) echo 'This runtime patch requires Rust 1.99.1 and rust-src.' >&2; exit 1 ;;
esac
export CARGO_BUILD_JOBS="${CARGO_BUILD_JOBS:-2}"
export RUSTC_BOOTSTRAP=1
export CARGO_TARGET_POWERPC64_SONY_PS3_RUSTFLAGS="--cfg getrandom_backend=\"custom\" -C target-feature=+altivec ${RP_EXTRA_RUSTFLAGS:-}"
python3 runtime/patch_std.py "$(rustc --print sysroot)/lib/rustlib/src/rust"
cargo build --locked --target-dir player/target-ps3 -Zjson-target-spec -Zbuild-std=std,panic_abort --manifest-path player/Cargo.toml --target "$(pwd)/runtime/powerpc64-sony-ps3.json" --features "ps3-simd,ps3-rsx${RP_PLAYER_FEATURES:+,$RP_PLAYER_FEATURES}" "$@"
