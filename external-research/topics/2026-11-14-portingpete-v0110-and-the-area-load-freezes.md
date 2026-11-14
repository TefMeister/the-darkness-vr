# portingpete's port since v0.1.9: v0.1.10 (movement, FOV slider) and a fix for area-load freezes from thread priorities

**Status:** 🆕 new · **Priority:** medium. Follows the topic of 2026-11-11 (v0.1.9). Nothing here is VR work; two
items bear on our port because they happen in the game's own threads, before any drawing.

## What is public

Read online on 2026-11-14 from the repository viewer and API (release notes, pull requests #158 to #168).
Nothing was downloaded. All `[reported 2026-11-14]`.

- **v0.1.10 (2026-11-12, pre-release):** an optional "Alternate Movement" (Original stays the default), a
  session-only speed slider in their F5 developer options, a field-of-view slider in Video Settings (Original up
  to 120), an optional light-flicker reduction, and a fog-shader fix for Proton. They claim no performance gain.
- **Area-load freezes, fixed on main after v0.1.10 (PRs #163, #164, proof in #167, 2026-11-12/13):** while an area
  loads, the engine's resource-pool worker threads could take every allowed CPU core at a Windows priority ABOVE
  the engine's own threads, so the engine and the application-system thread were ready to run but never got the
  CPU: multi-second stalls on CPUs with few cores (their issue #124; reproduced on the Chinatown stairs route, a
  6-second stall with the old behaviour against peaks of about 120 ms with the fix). The fix caps those workers at
  the engine's priority while keeping what the game itself sees (its priority value, affinity, processor number),
  plus a yield in the application system's frame poll when the host is contended. They also added an opt-in stall
  watcher that samples thread states and guest call stacks once a frame takes longer than a set threshold.
- Other main-branch fixes since: destroyed lamps kept destroyed with flicker reduction on (#166), scene pixels no
  longer leaking into the 1080p bloom (#168).

## What it means for us

- Our port maps the console's thread priorities onto Windows threads through the SDK too `[inferred-static]`. If a
  headset run ever shows a long hitch on entering an area (not a steady low frame rate), this is the first thing
  to compare: which of our threads are runnable but not running during the hitch `[hypothesis]`. The engine's
  resource-pool workers are the named suspects.
- Their stall watcher (thread states + guest stacks after a frame-time threshold) is a pattern worth having in our
  own logging for the "freeze" class of report; written fresh, nothing copied.
- The FOV slider changes the flat camera only; nothing for our per-eye projection.
- Estacado (the other public port) has had no release since 0.9.8 (2026-10-12) and no push since 2026-11-06
  `[measured 2026-11-14, API]`.

## Sources

- portingpete, The Darkness Recomp, release v0.1.10: https://github.com/portingpete/The-Darkness-Recomp/releases/tag/v0.1.10
- PR #164 (resource-pool worker priority): https://github.com/portingpete/The-Darkness-Recomp/pull/164
- PR #167 (in-game proof for issue #124): https://github.com/portingpete/The-Darkness-Recomp/pull/167
- Estacado: https://github.com/invinceble55-wq/Estacado-The-Darkness-Recomp
