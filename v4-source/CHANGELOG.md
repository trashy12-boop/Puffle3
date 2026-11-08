# Changelog — 01.08 v4

- Restored the default entry point to the original Puffle multi-game browser, title ID RFLP00001. The layout, game selection, settings and HDD/USB navigation remain.
- Fixed black external-font labels: initialize the RGBA glyph atlas with white RGB and transparent alpha, preserving exact glyph coverage. Fixed-font transparent texels also retain white RGB for filtered edges.
- Added a bounded AVM1 MovieClip path snapshot cache. Every hit validates each current ancestor name and root depth before reusing immutable text. Rename/removal/recreation behavior and each reference's mutable resolution cache remain independent. Cache entries retain strings, not display objects; a recycled address alone cannot produce a hit. Maximum 256 entries per player.
- Added separate path-snapshots and profile-vm features for controlled off/on builds. Pinned reproducible upstream patch 09; previous draw batching, compact meshes, texture caching and memory reductions remain.
- Added Mario defaults in the generic browser for recognized Mario filenames. Saved profiles override defaults; other games retain generic defaults.
- Added an external-font coverage regression test and an original AVM1 fixture for leaf rename, parent rename and removal/recreation, plus a runner comparing fourteen upstream reference tests.
- Produced normal and diagnostic installable packages and retained complete modified source, build scripts, symbols and measurements. Game assets are imported separately.

The prior v3 development results are included for context. This iteration makes no claim of 30 FPS physical-console gameplay or complete-game validation.
