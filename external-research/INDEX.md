# Research index

**Last `/gr` pass: 2026-11-14 — FULL.** The four `0xC0000374` heap-corruption dumps of b027 match an open ReXGlue report (#525): heap corruption on exit whenever the title ends itself (as our new QUIT line does) because the keyboard-and-mouse driver unhooks from an already-freed window; app-side workaround described, pointer in `engine-research/inbox/`. portingpete's v0.1.10 (movement, FOV slider) and a main-branch fix for area-load freezes from worker-thread priorities; Estacado unchanged.

_Previous: **Last `/gr` pass: 2026-11-11 — FULL.** portingpete's v0.1.9 (today) shows the engine draws a motion-blur velocity pass even with blur off (17-23 % of draws, emitter 0x82276148), and the ReXGlue D3D12 upload path puts a whole-buffer barrier pair between draws, which two Vulkan-only pull requests fix; two topics, one pointer to the dossier inbox, one to the cross-engine library._

_Previous: **Last `/gr` pass: 2026-11-07 (estate sweep) — CHECK-IN.** Inbox drained earlier today (the DS3-VR topic). rexglue-sdk v0.10.1 (2026-11-06) is the stable cut of the three nightlies already noted on 10-04 (#449 vector pack, #425 input, #450 atomics and fences, #417 project names), nothing else; the launch-path fault in `xam_info.cpp` (board row of 10-15) is unchanged upstream (the same lines at v0.10.1, read online) `[inferred-static]`.portingpete's port: v0.1.8 (10-27: Steam Deck, 4:3 output, media packs) and a cheaper draw capture (#144, 10-30); Estacado: no release since 0.9.8, only a sponsor link. No VR work on either. Nothing new for us._

_Previous: **Last `/gr` pass: 2026-10-26 (estate sweep) — CHECK-IN.** Inbox empty. Found two public PC ports we did not know: portingpete's (draws through the engine's own D3D11 backend, F5 dev tools) and Estacado (same ReXGlue SDK, offsets the scene projection per frame, threading docs); one topic, pointer sent to the dossier._

_Previous: **Last `/gr` pass: 2026-10-04 (estate sweep) — CHECK-IN.** Inbox empty. Three rexglue-sdk nightlies since (2026-09-30 to 10-02) change generated atomics and memory fences, which bears on the world-pause question; pointer with a rebuild A/B sent to the dossier._

_Previous: **Last `/gr` pass: 2026-09-29 (estate sweep) — CHECK-IN.** Nothing new. rexglue-sdk has three nightlies since v0.10.0 (window/fullscreen without restart, input, one vector-pack codegen fix, #449); none touches guest threading, which is the current resume row._

_Previous: **Last `/gr` pass: 2026-09-23 (estate sweep) — CHECK-IN.** Checked phunkaeg's *VR Modding Playbook*: no Starbreeze or Xbox-360-recompile entry. Nothing new._

_Previous: **Last `/gr` pass: 2026-09-17 (estate sweep) — CHECK-IN.** First pass: folder bootstrapped; one topic on the Starbreeze sibling with a PC release (Riddick: Assault on Dark Athena: console key and `.xrg` debug menu) and on the ReXGlue ecosystem having no VR prior art._

Every research topic gathered for this project, newest first. Each row links to a self-contained
write-up in `topics/`. Status tags:

- 🆕 **new** — found, not yet acted on by the modding side.
- 👀 **looked at** — the modding side has read it; no verdict yet.
- ✅ **used / confirmed** — acted on, and it held.
- ❌ **dead end** — tried, and it did not work (kept so it is not re-proposed).

| Date | Topic | Status | Why it matters |
| --- | --- | --- | --- |
| 2026-11-14 | [ReXGlue: heap corruption (0xC0000374) when the title quits itself; two other open heap reports](topics/2026-11-14-rexglue-heap-corruption-when-the-title-quits-itself.md) | 🆕 | Issue #525 is the exact code and trigger of b027's four dumps (the QUIT line runs `quit()`); the fix is an app-side shutdown order, our own code |
| 2026-11-14 | [portingpete's port v0.1.10, and a fix for area-load freezes from worker-thread priorities](topics/2026-11-14-portingpete-v0110-and-the-area-load-freezes.md) | 🆕 | Resource-pool workers above the engine's priority starved it for seconds on few-core CPUs; the first thing to compare if a headset run hitches on entering an area |
| 2026-11-11 | [portingpete's port v0.1.9: the engine draws a velocity pass nobody reads with motion blur off (17-23 % of draws), plus OW depth fog, draw distance and FOV draw counts](topics/2026-11-11-portingpete-v019-fewer-draws-from-the-engine.md) | 🆕 | Fewer draws is the direct cure for the card's ~2-3 microseconds a draw; guest addresses given |
| 2026-11-11 | [ReXGlue SDK: every changed-memory upload between draws is a whole-buffer barrier pair on D3D12; Vulkan-only fixes #536 and #549; `skip_msr` already a manifest key; the 0.11 nightlies restructure the SDK](topics/2026-11-11-rexglue-sdk-per-draw-uploads-and-0-11-nightlies.md) | 🆕 | A measurable suspect for the size-independent per-draw cost (~560 uploads an eye), and when not to rebase |
| 2026-11-04 | [DS3-VR (Dark Souls III): geo-11 stereo frames sent to OpenXR, with a floating HUD panel and an in-headset menu; not a drop-in for our D3D12 build, but four ideas worth copying](topics/2026-11-04-ds3-vr-geo11-openxr-pattern.md) | 👀 | Hand-off from the modding session (Tefa's Discord tip): the HUD panel, the tabbed menu, the frame cap and per-eye DLSS are direct board candidates |
| 2026-10-26 | [Two public PC ports of The Darkness appeared in Sept/Oct 2026 (one with a native D3D11 renderer)](topics/2026-10-26-two-public-pc-ports-of-the-darkness.md) | 🆕 | portingpete's port draws through the engine's own D3D11 backend (route choice); Estacado (same ReXGlue SDK) offsets the scene projection per frame and documents its threading; both have FOV, motion-blur off and dev/unlock tools. |
| 2026-09-17 | [Riddick: Assault on Dark Athena (same studio, PC release) enables a debug menu through an `.xrg` file; no ReXGlue project has done VR](topics/2026-09-17-starbreeze-dark-athena-xrg-debug-menu-and-rexglue-landscape.md) | 🆕 | The Darkness uses the same `.xrg` config format; and this project would be first in its ecosystem |
