# Reproduce 01.06

Use scripts/bootstrap-linux.py (see --help), then source the emitted env.sh. The pinned tool versions are in scripts/toolchain-lock.json. The build used Rust 1.99.1, PS3DEV nightly 2026-10-12 and Cg 3.1.

Run scripts/get-ruffle.sh, scripts/build-player.sh --release, make package. The upstream revision is 1125b0018c110fb7a1e51f080c7b47d5dc94d64f; archive SHA-256 is checked. Both ruffle-ps3-05.patch and ruffle-ps3-06.patch are mandatory and applied idempotently. jpeg-decoder is pinned by a local Cargo patch and included under vendor with licenses.

Host checks: cargo test --locked --manifest-path player/Cargo.toml --lib; cargo test --locked --manifest-path player/Cargo.toml -p ruffle_core --lib index_format_tests. Build the benchmark example for SWF trace/pixel comparisons.

PS3 package keeps content ID UP0001-RFLP00001_00-0000000000000000. Firmware and benchmark game SWFs are not redistributed in this source archive. RPCS3 firmware 4.93 was installed separately.
