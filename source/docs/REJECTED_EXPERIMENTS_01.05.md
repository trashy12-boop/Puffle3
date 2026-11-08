# Rejected / inconclusive experiments

- A separate AVM1 small-Push decoder passed functional checks but did not show a reliable full-game timing benefit. It was removed. The delivered parser uses SmallVec inline storage within the original decoder instead.
- Applying two-observation admission to all software draw-cache entries reduced resident memory but hurt The Right Mix reuse. Delivered admission applies only to snapshots at least 16 KiB; smaller draws use the existing immediate policy. Hash selection was corrected to mix high float bits.
- `-C jump-tables=no` was compiled as a diagnostic for RPCS3 executable-analysis memory growth. It did not resolve the analyzer exhaustion and is not the delivered default. Do not distribute the intermediate code-generation probe as the final build.
- RPCS3 v0.0.43-20551 was tested with LLVM and static PPU decoding, reduced LLVM worker counts, null audio, and Vulkan lavapipe. The original v1.03 package also hit analyzer memory exhaustion. This prevents a trustworthy native old/new FPS comparison in that setup.

No SPU acceleration, JIT, speculative property cache, altered Flash tick semantics, or disabled game logic was added.
