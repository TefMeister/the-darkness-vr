# The port-wide lock in every atomic step, and what limits the frame rate now (2026-11-10 afternoon, Fable, home PC, simulator; b024)

Tefa: "carry on" (after b023, one pass a frame). The plan was to design "both eyes from one game frame". Before
designing, one question: which threads are busy, and with what? The answer changed the plan.

## What the three "100 %" threads were doing (b023, the alley) `[measured 2026-11-10, simulator]`

`dev-archive/build-scripts/dk_threads.py` (new): finds the busiest threads, samples each one's instruction pointer,
names the module, and with the linker's map (`-Xlinker /MAP:darknessrecomp.map`, now in the build folder's CMake
cache) the game function. Call stacks came from the Silent Hill 2 project's `waitstacks.py`.

| Thread | What the samples said |
| --- | --- |
| the game's drawing thread, 97 % of a core | 55 % in `sub_8285DEB8` and 14 % in `sub_8285D568` (+ ~10 % in their register save / restore helpers): the console graphics library's "wait until the graphics chip has caught up" loop (`sub_8285D568` compares against a pointer the chip updates and calls `sub_8285DEB8`, a pause loop of `db16cyc` with a 5 s timeout). About 80 % of this thread is WAITING for the port's graphics thread |
| a second game thread, 98 % | 44 % the game's own code, about 40 % inside `Mtx_lock` / `Mtx_unlock` called from the game's code |
| the port's graphics thread, 95 % | about 37 % waiting in `Mtx_lock`, 25 % its own work, 12 % copying memory |

Where the lock time came from (1,083 samples inside the lock, all threads): the graphics thread 34 % (the victim),
`sub_820C87F8` 17 %, `sub_820C88B0` 17 %, `sub_820CB5C0` 9 %, `sub_8265F8A0` 5 %, `sub_82170478` 4 %, `sub_821F0A40`
3 %, `sub_82170178` 2 %.

## Why `[inferred-static 2026-11-10]`, then `[verified-live 2026-11-10, simulator]`

`sub_820C87F8` / `sub_820C88B0` are the game's own lock (try-enter and leave). Like all 360 code they bracket their
one atomic step with "interrupts off / on":

    mfmsr r7 ; mtmsrd r13,1 ; lwarx r8,0,r31 ; cmpw ; bne ; stwcx. r10,0,r31 ; mtmsrd r7,1 ; bne (again)

The generated header (`darknessrecomp_pch.h`, "Interrupt Locks") turns `mfmsr` into a lock + unlock of the port's ONE
shared lock (`rex::thread::global_critical_region`), `mtmsrd r13,1` into a lock and the restoring `mtmsrd` into an
unlock: three lock operations for one compare-and-swap, on the lock the graphics thread also needs for every memory
watch. And the generator already makes `stwcx.` a real compare-and-swap on the host
(`__sync_bool_compare_and_swap`), so the step is atomic without any lock.

Checked over the whole generated source: **981 bracketed sections (in 114 functions), every one holding only one
`lwarx` / `stwcx.` (978) or `ldarx` / `stdcx.` (3) step with compares, sums and branches: no call, no other store.**

## The fix (b024)

`dev-archive/build-scripts/patch_msr_lock.py`, run by `home_build_darkness.bat` after the code generation and before
the compile (the generator rewrites the header every time): the three macros become lock-free
(`REX_CHECK_GLOBAL_LOCK()` answers "interrupts on", enter and leave do nothing). `DK_KEEP_MSR_LOCK` at compile time
keeps the old behaviour. A full recompile of the game code (183 steps).

## What it did (the alley, simulator, nobody in a headset; `[measured 2026-11-10, n=1 each]`)

| Drawn | b022 | b023 (one pass) | b024 (+ no shared lock) | The card in b024 |
| --- | --- | --- | --- | --- |
| 2 x 3 larger (as installed) | ~40 | 72 | 76 (72-76 over four runs) | 99 % |
| 2 x 2 | | 76 | 89 | 99 % |
| the game's own size | | 99 | 124 | 95 % |

Game frames a second; the headset gets half (one eye per frame): 38 / 45 / 62 pairs. After the fix the second game
thread's time in Windows' lock code fell from ~50 % to ~10 % and the graphics thread went from 95 % to 66-86 % of a
core. Three runs to the level, no hang, the usual 6 log errors, no failed draws.

## What limits it now, and what that means for "both eyes from one frame"

- **The graphics card, at every size** (95-99 % busy even at the game's own 1280 x 720). The port's exact drawing
  method (`render_target_path_d3d12 = "rov"`, the console's video memory emulated inside the pixel shader) is what
  costs: about 13 ms of card time per eye at 2 x 3, 11 at 2 x 2, 8 at 1 x 1. The other method (`rtv`) was slower
  still in b023 (58 against 72). Occlusion queries off, the memory read-back off, one submit a frame: no change
  (72, 72, 72).
- So **drawing both eyes from one game frame would not help yet**: it removes the game's work for the second eye,
  but the card would draw twice as much per game frame and the card is already the limit. First the card's cost per
  eye has to come down; then the both-eyes design pays (the game side now has room: 124 frames a second at the small
  size).
- **The next step needs a real per-pass picture of one frame on the card.** Nsight's GPU trace starts the port fine
  (it is a plain program) but stops with "GPU Performance Counters unavailable. Please enable access to GPU
  performance counters": a setting in the NVIDIA Control Panel (Developer > Manage GPU Performance Counters >
  allow access for all users) that only Tefa can switch. With it: `ngfx_gputrace_launched` on the b024 exe with the
  start script's settings as `env_pairs`, the output folder made first, the trace started after 115 s (the presses
  to the alley take ~95 s).

## Traps met

- A build folder's `Assets` is a link (junction) to the whole game. `rm -rf` on the folder removed only the link here
  (checked: 499 game files, 6.8 GB, still there), but delete the link by itself first (`cmd /c rmdir Assets`) rather
  than trust that.
- Nsight refuses an output folder that does not exist ("Invalid general options").
- Run-to-run spread in the alley is about 4 game frames a second (72-76 for the same build).
