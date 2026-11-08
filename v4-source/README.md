# Puffle 3 v4 source overlay

This branch holds the v4 project changes and supporting documentation for the Puffle3 PS3 port. It preserves the existing `main` branch and adds the v4 source under `v4-source/`.

The complete reproducible development archive, including the pinned Ruffle source tree, PS3 C/Rust code, normal and diagnostic PKGs/ELFs, build scripts, measurements, and captures, is the `Puffle3_PS3_Development_v4.zip` file attached in the ChatGPT conversation. The standalone installable is `Puffle3_01.08_v4.pkg`. The Mario SWF is intentionally not bundled.

The repository includes the v4 C frontend, Rust runtime changes, build scripts, synthetic regression fixtures, and performance documentation. Shell scripts automate build/publishing tasks; they are not the game runtime. GitHub determines the Languages bar from tracked source extensions, so it should include C and Rust as well as Shell after its stats refresh. The full archive is still required for a clean rebuild because it contains the pinned upstream Ruffle dependency tree.

License scope is documented in [LICENSE](../LICENSE), [COPYING.PS3ME](../COPYING.PS3ME), [NOTICE.md](NOTICE.md), and the component-specific license files under `source/licenses/` and `source/vendor/`. Puffle3's original MIT notice is scoped to original code; it does not replace GPL-2.0-only terms on PS3ME-derived files or upstream dependency licenses.

See [README_FIRST.md](README_FIRST.md), [CHANGELOG.md](CHANGELOG.md), and [PERFORMANCE.md](PERFORMANCE.md).

Test fixtures are synthetic AVM1 regression inputs and contain no game assets.
