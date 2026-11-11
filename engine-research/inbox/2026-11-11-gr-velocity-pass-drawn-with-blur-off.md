# /gr 2026-11-11: the engine may draw its velocity pass with motion blur off (17-23 % of draws in the other port)

**From:** `/gr the-darkness-vr`, 2026-11-11. **For:** the dossier, §2 screen-effects table (the "Motion blur" row) and
§6f (one frame on the graphics card), and the `[PD]` board row "THE CARD'S COST PER DRAW".

**The open question it answers, quoted from §6f:** *"About 2-3 microseconds of card time per draw on either path
(the 979-draw pre-pass ...) `[hypothesis on measured]`: the size-independent ~7 ms."* and from §2: *"that this gate IS
motion blur `[hypothesis]`"* (the motion-blur switch only gates the post-process, nothing says the pass that FEEDS it
stops being drawn).

**The finding** `[reported 2026-11-11]`: portingpete's public port (v0.1.9, `RENDERING.md` "Redundant geometry passes")
found the "MotionMap" pass redraws moving and static geometry into a velocity target that only the motion-blur input of
the final composite reads, and returns early from its emitter at guest **0x82276148** when motion blur is off: 17-23 %
fewer draws, 8-25 % more frames a second on their village route. Their port checks for one retail `default.xex`
revision; that it is ours is `[hypothesis]`.

**Suggested change:** add to §2's motion-blur row: "the velocity pass that feeds it is drawn separately, emitter
0x82276148 per portingpete `[reported 2026-11-11]`; not yet checked in our image". Then the static check (is
0x82276148 a function start, called from the frame build, writing a velocity target) and a counted, switchable
early-return hook in the alley.

Full topic: `external-research/topics/2026-11-11-portingpete-v019-fewer-draws-from-the-engine.md` (also: OW depth fog
drawn per object, draw distance at 0x82498C48, FOV 95 = +6 % draws, LOD changes triangles not draws).
Link: <https://github.com/portingpete/The-Darkness-Recomp/blob/main/RENDERING.md>
