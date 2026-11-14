# /gr: the b027 heap-corruption dumps match an open ReXGlue report of heap corruption on a title's own quit

Answers the open question in `engine-research/inbox/2026-11-14-gd-four-heap-corruption-crashes-in-b027-test.md`:
> "a quit path is one possibility `[hypothesis]`; heap corruption is reported where it is found, not where it was made."

rexglue-sdk issue #525 (open since 2026-10-20, Windows prebuilt nightly, D3D12) reports `0xC0000374` on EVERY exit
where the title ends itself (not the window's close button): the app frees the window before the runtime, and the
keyboard-and-mouse input driver later unhooks itself from the freed window during the input system's shutdown
`[reported]`. Our b027 QUIT line runs the game's own `quit()`, the same trigger `[inferred-static]`.

Suggested change: read one dump's stack for the input driver's destructor under the runtime's shutdown; if it is
there, shut the input system down in our app's shutdown hook (before the window goes), our own code, then quit
through the menu a few times. Topic: `external-research/topics/2026-11-14-rexglue-heap-corruption-when-the-title-quits-itself.md`.
