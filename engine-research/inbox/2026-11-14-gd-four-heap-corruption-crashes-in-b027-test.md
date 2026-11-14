# Gold Digger: four heap-corruption crashes in the b027 test build (2026-11-12, 16:40 to 17:32)

From the full scan of 2026-11-14 (Keeper PC). Shelf `crashes`, items
`2026-11-12_16-40-58_PC1_darknessrecomp.exe.29556.dmp`, `..._17-15-48_...27276.dmp`, `..._17-27-40_...6444.dmp`,
`..._17-32-51_...12656.dmp`; run cards on the `runs` shelf at the same times.

- All four have the same crash record: **`0xC0000374` heap corruption in `ntdll.dll+0x112FC5`**, 99 modules loaded,
  7 unloaded `[measured 2026-11-12, the dumps, n=4]`.
- The exe was `C:/NonSteam/the-darkness/build-test-b027/darknessrecomp.exe` (built 16:20:49); the 16:40 run lasted
  106 s, ended "crashed", with the VR runtime loaded (`vr: true`).
- This is a different code from the known crash on the way out after a quit (`0xC0000409`, Control Flow Guard,
  2026-11-06, board). The notes and board do not mention a `0xC0000374`. The same afternoon's work was the Quit line
  from the main menu (`2026-11-12b`), so a quit path is one possibility `[hypothesis]`; heap corruption is reported
  where it is found, not where it was made.

Proposed board row: `[PD]` read one dump's stack (which thread, what freed or wrote the block) and say whether it is
the quit path; if it still happens in b029+, a `[VR CLAUDE]` run with page heap on (`gflags /p /enable
darknessrecomp.exe /full`) finds the writer.
