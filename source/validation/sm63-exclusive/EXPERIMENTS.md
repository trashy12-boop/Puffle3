# SM63 exclusive work: experiments in progress

No new emulator gameplay FPS gain has been demonstrated at this checkpoint.
Physical v5 gameplay reference remains the existing report, not a matched v6
comparison. Do not request another physical test until the RPCS3 acceptance gate
is met or an actual hardware-specific blocker has been isolated.

## Compiler experiment

Existing source, same features and capture frontend in both variants:

- Baseline: release opt-level 2, 16 codegen units, no explicit LTO.
- Candidate: `CARGO_PROFILE_RELEASE_OPT_LEVEL=3
  CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1 CARGO_PROFILE_RELEASE_LTO=thin`
  applied to `scripts/build-puffle.sh release`.
- Rust 1.99.1, pinned upstream/patches and native C `-O2 -mcpu=cell` unchanged.
- Both native ELF/PKG builds succeeded. Candidate is not a default build mode.
- Preserve each ELF before building another variant. The build script's ordinary
  v6 output name is reused; filename alone does not establish build identity.
- Record executable SHA256 and a build manifest with every timing run.
- Before measuring, finish both RPCS3 cache warmups and stop all compilers and
  other emulator instances. Compare repeated gameplay routes, not startup.

The physical v5 profile has a large VM/update component; this experiment tests
whether broader inlining and compiler optimization can reduce it. Compiler
flags alone are not evidence of an improvement, and code-size growth can hurt
Cell instruction-cache behavior. Keep the default profile unless measurements
justify a change.

## Operand-free AVM1 dispatch experiment

`scripts/ruffle-ps3-11-direct-dispatch.patch` is initially an **unapplied,
unverified candidate**. It adds an opt-in upstream feature and intercepts
operand-free short instructions before constructing the generic SWF `Action`
enum. It calls the existing instruction handlers, advances exactly one byte,
and preserves the existing timeout boundary and profiling counts. Operands,
unknown bytecodes and malformed payloads keep the original reader. Debug AVM
tracing disables the shortcut.

This targets repeated bytecode decoding inside the measured VM bottleneck.
It does not cache values or object identities, alter simulation timing, remove
effects, skip frames, or assume that Mario's program never changes objects.

Required before enabling it: build feature-on and feature-off hosts with the
same existing fast paths, compare AVM1 fixture traces and the complete Mario
replay's VM counts/RSX rows/reference images, then compile and compare actual
RPCS3 gameplay. A host improvement alone must not be reported as console FPS.
