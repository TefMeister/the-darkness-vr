# ReXGlue: a public report of heap corruption (0xC0000374) when the title ends itself, and two other open heap reports

**Status:** 🆕 new · **Priority:** high. Gold Digger shelved four crash dumps from the b027 test build of
2026-11-12, all `0xC0000374` (heap corruption) in `ntdll`, on the afternoon the main-menu QUIT line went in
(`engine-research/inbox/2026-11-14-gd-four-heap-corruption-crashes-in-b027-test.md`). The SDK's own issue tracker
has an open report of exactly that code on exactly that kind of exit.

## What is public

Read online on 2026-11-14 through the repository's issue viewer and API. Nothing was downloaded. All `[reported]`
unless marked.

### rexglue-sdk issue #525 (opened 2026-10-20, still open, no maintainer reply yet)

- **Who / what:** a recomp author on the Windows prebuilt SDK nightly 0.10.0.24 (D3D12, Xenos GPU plugin).
- **Symptom:** when the GAME ends itself (their case: a menu choice that ends the title), the process dies during
  shutdown with `STATUS_HEAP_CORRUPTION` (0xC0000374), right after the "Execution complete" log line, on every such
  exit. The window is already gone, so nothing shows; only a dump in the local crash-dump folder. Closing the window
  with its close button does NOT crash.
- **Their stack (from the minidump):** the heap check fires inside the keyboard-and-mouse input driver's detach
  step, reached from the input driver's destructor, from the input system's shutdown, from the runtime's
  destructor, from the app's destroy step, from the program's main function.
- **Their explanation, in our words:** the app's destroy step frees the window first and the runtime second. The
  keyboard-and-mouse driver keeps a pointer to the window it attached to, and that pointer is only cleared when
  the window gets its "closing" event, which happens only when the USER closes it. When the title ends by itself
  there is no closing event, so the driver later tries to unhook itself from a window that has already been freed,
  and that write into freed memory is what the heap check catches.
- **Their workaround, in our words:** shut the input system down from the app's own shutdown hook, which runs
  before anything is torn down while the window still exists; the later second shutdown call is harmless because
  it does nothing the second time.

### Two more open heap reports on the same tracker (for completeness)

- **#336** (opened 2026-05-09, last touched 2026-11-13): a non-graceful exit hitting a breakpoint inside the heap
  validator when the main window's close button is pressed (a Debug build of another recomp; fibers suspected).
  Still unchanged at v0.8.0 per the reporter.
- **#502** (opened 2026-10-11): the profile-settings write path can trash the heap when a title writes a
  zero-length binary setting, but the reporter shows this only with a newer compiler at high optimisation on
  Linux; the same report notes a settings class freed through a base pointer with no virtual destructor, which
  leaks on every repeated write `[reported]`. Whether either part bites a Windows build of ours is
  `[hypothesis]`; the first part needs that compiler behaviour.

## What it means for us

- Our port is built on the same app layer (`rex_app.cpp`, dossier line on the user folder) `[inferred-static]`,
  and the b027 QUIT line runs the game's own `quit()`, which ends the title by itself, not through the window's
  close button (`modding-notes/2026-11-12b-quit-from-the-main-menu.md`). That is the exact trigger in #525.
- So the strongest candidate for the four `0xC0000374` dumps is this shutdown-order fault, not anything in our VR
  code `[hypothesis]`. Separates it: (a) the dumps' stacks show the input driver's destructor / detach under the
  runtime's shutdown → #525; (b) whether the crashes all came at the END of a run that was quit through the menu
  (the run cards' end times against the quit), versus mid-play; (c) a quit through the window's close button
  never crashing.
- If it is #525: the fix lives in our app's shutdown hook (input system shut down first), our own code, no SDK
  patch needed; written fresh from the description, nothing copied.
- The known `0xC0000409` on the way out (2026-11-06, Control Flow Guard) is a different code; whether it shares
  the cause is open `[hypothesis]`.

## Sources

- rexglue-sdk issue #525: https://github.com/rexglue/rexglue-sdk/issues/525
- rexglue-sdk issue #336: https://github.com/rexglue/rexglue-sdk/issues/336
- rexglue-sdk issue #502: https://github.com/rexglue/rexglue-sdk/issues/502
- SDK state on 2026-11-14: newest stable v0.10.1 (2026-11-06), newest nightly 0.11.0-dev.g351bc94d (2026-11-11);
  no commit on the main branch since v0.10.1 `[measured 2026-11-14, API]`.
