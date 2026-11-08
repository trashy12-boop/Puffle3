# Puffle 3 01.07 — STOPPED checkpoint

User requested stopping on 2 November 2026. No more tests should be assumed complete.

Completed: new conservative RSX scissor backend for integer rectangular masks, including nested intersection and restoration. Curves, fractional bounds, compound masks and unsupported alpha masks retain full software fallback. Whole command lists are validated before GPU submission. Title ID RFLP00001 and supplied branding retained. Actual native executable and PKG compiled. RPCS3 installed the PKG; extracted executable payload matched the newly compiled staging EBOOT.

Validation: 19 host tests passed, 1 ignored. Native synthetic nested-mask fixture used RSX (257 GPU frames, zero software frames in captured log) where the reference fell back to CPU. All 76,800 sampled logical pixel centers matched the software rendering. Full scaled presentation differed at 6,416 physical pixels; a filtering explanation was suspected but not fully isolated. Do not describe full presentation as byte-identical.

INCOMPLETE: the final 01.07 RSX Mario/Bullet Bill game benchmarks were interrupted by the user's stop request. No measured FPS improvement for 01.07 is established. No real PS3 hardware test or physical RSX utilization percentage was measured. No SPU/JIT acceleration was added.

Earlier CPU cache clock/adaptive experiments did not establish a practical improvement and were restored out of the main renderer; their source is retained under validation/cache-experiments. Old FPS reports are not results for this 01.07 executable. Host JACK/TLS errors and automation failures complicated earlier exploratory measurements.

Resume: first inspect this source and the fixture logs, then finish 01.07 Mario/Bullet Bill native tests. Confirm GPU command submission/fallback counters rather than equating host Vulkan utilization with PS3 RSX utilization. Reproducible dependencies/build scripts and upstream patches are included.
