# /gs 2026-11-01: one invented tag in three places, and an OPEN heading the board reader cannot parse

From: `/gs` (inside `/fullscan`), 2026-11-01. Read-only finding; the modding lane owns every file named here.

1. **`[inferred 2026-10-28]` is not one of the eight tags**, so it counts as untagged to every tool. It is dated, so
   it is a real claim wearing an invented name; the fix is one word, `[inferred-static 2026-10-28]`, in all three
   copies:
   - `engine-research/ENGINE-DOSSIER.md` line 1299
   - `modding-notes/2026-10-28-the-camera-rows-were-named-wrong.md` line 77
   - `claude-memory/status/the-darkness-vr.md` line 67 (the "is the car-scene camera really on its side?" row)
2. **The board's OPEN heading is `OPEN (2026-10-30 17:50 — ⭐ PLAYED IN VR ...):`**; `gate-scan.sh --check` wants
   exactly `OPEN (YYYY-MM-DD):`, so the whole block is invisible to `/gates`. Move the headline to the line above
   (or into a 📌 NOTE row below the first row) and keep the heading bare.
