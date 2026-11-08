#!/bin/sh
# Reproduce the minimal, experimental std check; failure is expected.
set -eu
probe_dir=$(mktemp -d "${TMPDIR:-/tmp}/ruffle-ps3-std.XXXXXXXX")
trap 'rm -rf "$probe_dir"' EXIT HUP INT TERM
git init -q "$probe_dir/rust-ps3"
git -C "$probe_dir/rust-ps3" remote add origin https://github.com/ZephyrCodesStuff/rust-ps3.git
git -C "$probe_dir/rust-ps3" fetch --depth 1 origin 476856b950a0bc77995cf0dbde6b0a1193b039b8
git -C "$probe_dir/rust-ps3" checkout --detach FETCH_HEAD
mkdir -p "$probe_dir/app/src"
printf '[package]\nname = "ps3-std-probe"\nversion = "0.0.0"\nedition = "2024"\n[workspace]\n' > "$probe_dir/app/Cargo.toml"
printf 'fn main() { println!("PS3 std probe"); }\n' > "$probe_dir/app/src/main.rs"
rustup component add rust-src
RUSTC_BOOTSTRAP=1 cargo check -Zjson-target-spec -Zbuild-std=std,panic_abort \
    --manifest-path "$probe_dir/app/Cargo.toml" --target "$probe_dir/rust-ps3/powerpc64-sony-ps3.json"
