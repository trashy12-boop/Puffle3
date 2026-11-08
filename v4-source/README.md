# Puffle 3 v4 source overlay

This branch holds the v4 project changes and supporting documentation for the Puffle3 PS3 port. It preserves the existing `main` branch and adds the v4 source under `v4-source/`.

The complete reproducible development archive, including the pinned Ruffle source tree, PS3 C/Rust code, normal and diagnostic PKGs/ELFs, build scripts, measurements, and captures, is the `Puffle3_PS3_Development_v4.zip` file attached in the ChatGPT conversation. The standalone installable is `Puffle3_01.08_v4.pkg`. The Mario SWF is intentionally not bundled.

The files in this GitHub branch are the v4 changes and key source files, not a replacement for the full upstream Ruffle dependency tree. Use the full archive for a clean rebuild. See [README_FIRST.md](README_FIRST.md), [CHANGELOG.md](CHANGELOG.md), and [PERFORMANCE.md](PERFORMANCE.md).

Test fixtures are synthetic AVM1 regression inputs and contain no game assets.
