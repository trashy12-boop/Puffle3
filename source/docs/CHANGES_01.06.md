# Puffle 3 01.06 — development build

Actual code changes, not a repack of 01.04/01.05. Title ID RFLP00001 and supplied branding are preserved.

- AVM1: bounded, GC-traced cache of interned MovieClip paths. Reference object identity remains independent.
- AVM1: interned numeric property keys without temporary heap Strings.
- AVM1: guarded bulk dense shift/unshift; original semantics remain for sparse, watched, virtual or specially flagged properties. Enumeration order is preserved. Scratch allocation is fallible.
- Fatal AVM1 errors reach the frontend log rather than silently halting the VM.
- Script deadline restored from the port's 2 seconds to upstream's 15 seconds.
- Software rendering: exact mask identities enable cross-frame path snapshots under masks; bounded cache, collision-safe equality, partial-alpha/nested-mask tests.
- PS3 heap: under high native memory pressure, shrink optional pixel snapshots from 8 MiB to 2 MiB before collecting; the reduced budget stays bounded for the remainder of that movie.
- PS3 heap: explicit GC at safe embedder boundaries, with hysteresis; original 128 MiB allocation guard retained.
- JPEG decoder 0.3.2 vendored with its licenses: PS3 uses immediate workers, because std thread spawning is unsupported. Desktop targets retain upstream threading.
- Package recipe removes the previous output before invoking pkg and uses explicit --contentid= syntax, preventing an existing package from being extracted over fresh staging. Final installed EBOOT identity is checked.
- Native allocation, timing, fallback and heap diagnostics preserved for measured validation.

No SPU acceleration or JIT is claimed. No frames or script logic are intentionally skipped. Read the accompanying measured report for remaining limitations.
