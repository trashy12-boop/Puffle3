#!/bin/sh
# Build first; do not compile concurrently with timing measurements.
set -eu
cd "$(dirname "$0")/.."
out=${1:-build/host-benchmarks}
mkdir -p "$out"
./scripts/get-ruffle.sh
cargo build --locked --release --manifest-path player/Cargo.toml --example benchmark \
 --features host-rsx-probe,profile-vm,path-snapshots
cp player/target/release/examples/benchmark "$out/baseline"
cargo build --locked --release --manifest-path player/Cargo.toml --example benchmark \
 --features host-rsx-probe,profile-vm,path-snapshots,avm1-fast-paths
cp player/target/release/examples/benchmark "$out/optimized"
echo "Run scripts/benchmark-avm1.py GAME.swf $out/baseline $out/optimized OUTPUT"
