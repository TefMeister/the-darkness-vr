# The build scripts clear stale precompiled headers themselves (2026-11-10, evening)

**What was wrong.** `home_build_sdk.bat` failed on every file with `PCH file ... built from a different branch`.
The same fault stopped the whole port earlier the same day (b018 note, "the full headset view").

**Why it happened** `[verified-live 2026-11-10, n=1]`. The compiler on the home PC (LLVM clang) was updated on
2026-11-03. A build tree keeps "precompiled headers" (`*.pch`): header files the compiler digested once so each
source file starts faster. clang refuses a precompiled header made by another clang build, and ninja does not
track the compiler itself, so after a compiler update it never remakes them and every file fails. The SDK's own
tree (`rexglue-sdk/out/build/win-amd64`) still held ten of them from 2026-10-24 and 10-25; the port's tree had
been cleared by hand in the morning, the SDK's tree was missed.

**The repair.** New `dev-archive/build-scripts/home_stale_pch.bat <build tree>`: it keeps the compiler's own
version text in `<tree>/compiler-stamp.txt`; when that differs from the compiler on PATH (or there is no stamp),
it deletes every `*.pch` under the tree (ninja remakes them) and rewrites the stamp. Both `home_build_sdk.bat`
and `home_build_darkness.bat` call it just before they build. The compiler's version text is compared, not file
dates: an installer keeps the compiler's build date, so a date test can miss an update.

**Proof.**
- Run 1 of `home_build_sdk.bat`: printed "COMPILER CHANGED ... clearing them", then built 513 of 513 steps,
  0 failed, exit 0; all ten `*.pch` are new.
- Run 2 straight after: nothing cleared, `ninja: no work to do`, exit 0.
- The port's tree was built by this compiler the same day (b025), so it was given its stamp by hand and is not
  cleared for nothing. Its script's guard is `[compile-verified]` only in the sense that the same helper ran in
  the SDK script; its first real run comes with the next port build.

**Still true.** The graphics library can be built in either tree now; the port's tree stays the one the game is
built from.
