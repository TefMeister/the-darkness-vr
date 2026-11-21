# Port b033 crashed at a new spot after 8 minutes in VR, 2026-11-16 23:58 (Gold Digger, Keeper full scan 2026-11-21)

The always-on collector caught one run that nobody wrote up. It ran 2026-11-16 23:49:59 to 23:58:03 on PC1, with
`build-home-xr-b033\darknessrecomp.exe` (the Hand Poser link port). It was in VR: the VR library loaded at 26.6 s and
a headset runtime was running. The window lost focus from 133 s to 235 s. The run ended "crashed (exit code
0xC0000005)" at 493 s [measured 2026-11-16, run card `runs/2026-11-16_23-58-03_PC1_run.json`, events `performance/
2026-11-16_23-58-03_PC1_events.tsv`].

The dump (`crashes/2026-11-16_23-58-03_PC1_darknessrecomp.exe.6952.dmp`) puts it at **`darknessrecomp.exe+0x190985`,
writing address 0x17014FFD0**. None of the 7 earlier Darkness dumps on the shelf has this spot, so this crash has not
been seen before. The modding notes have no entry for 2026-11-16 at all.

`[hypothesis]` A write to an address just under 0x170150000 can be a stack or guard-page overrun. Not checked against
the port's map file.

Proposed board row: map `+0x190985` in the b033 exe to its function (the build's map or pdb), and say whether b034
still has that code.
