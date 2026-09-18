# 2026-09-18 (night) — moving the eye out of the projection and into the camera

`/lm`, dev PC, Claude (Opus) driving, Tefa at work. Four live runs. Follows
`2026-09-18-first-stereo-pair-and-the-two-eyes-decision.md`, which left one open defect: with the
eye offset hidden in the projection P, two characters' heads rendered **red-lit in one eye and dark
in the other**.

## What the reader diagnosed

Only two functions ever read the shifted copy of P (`sub_82248A78`, which writes the vertex
constants, and the user-clip-plane path). **Everything else that needs the camera — per-light
scissor rectangles, frustum culling, the world-space eye position handed to the lighting shaders —
is computed on the host side from the viewport and camera, which never saw the shift**
`[measured 2026-09-18]`. Its named candidate: `sub_825C3918`, which builds each light's screen
rectangle from sphere tangents with the eye at the view-space origin, i.e. the *centre* eye. With
geometry displaced 47–190 px and the rectangle not displaced, a light or its shadow is cut in the
wrong place, in opposite directions for +e and −e. Its prescription: **put the offset in the camera
and keep P for projection only.** Full note in
`engine-research/inbox/2026-09-18-reader-per-eye-lighting-defect-candidates.md`.

## What I built and what it showed

A second hook, `REX_HOOK_RAW(sub_823F9B00)` — the client view build — that adds `e × right` to the
camera's position row after the original runs. `DK_CAM_EYE` selects it; `DK_STEREO_EYE` still
selects the old P route, so the two can be compared without a rebuild.

- ✅ **The row layout was a guess and it is right.** The one-shot log prints
  `right=(-0.0330 0.9472 -0.3191) |right|=1.0000, pos=(-1597.16 14192.65 -479.68)` — a unit vector
  and a plausible world position `[verified-live 2026-09-18, n=1]`.
- ✅ **The dossier's unproven "last link" is now PROVED.** §6 said nobody had traced
  `sub_823F9B00`'s output into the render matrices. At `DK_CAM_EYE=40` the camera plainly moves —
  the view ends up inside the scenery with the hands gone `[verified-live 2026-09-18, n=1]`. So the
  client view build does drive what is drawn, which also makes it the candidate for head tracking
  later.
- ✅ **It produces real per-eye geometry differences.** At `DK_CAM_EYE=4`, alternating eyes, six
  pairs: hands **133–134 px** apart at match quality 0.91–0.93, HUD **0 px** at 0.97–0.98
  `[verified-live 2026-09-18, n=6 pairs]`.
- ✅ **The lighting defect did not reappear.** Across 21 eye-labelled frames (10 L, 11 R) head
  redness stayed negative in both eyes — medians −2.7 and −3.8, nothing above +2 — against the P
  route, where it swung to +6.8 locked to the eye for 12 consecutive alternations
  `[verified-live 2026-09-18, n=21 frames]`.
  ⚠️ **Not the same scene moment**, so this is suggestive rather than settled. The scripted red
  light passes at its own pace and I could not hold the world still to line the two runs up. It
  wants re-confirming once the world-freeze step exists — and the earlier "lighting is fine" claim
  was retracted for exactly this kind of gap, so it is tagged accordingly rather than upgraded.

## The size of the offset is unknown, and cannot be measured yet

**Camera units are not the same as the P-offset units.** 0.6 in P gave 47–190 px of disparity; 0.6
at the camera gave nothing measurable at all. Bisecting: 4 units puts the eyes roughly a metre
apart (the left eye ends up behind the seat — see
`dev-archive/recon/2026-09-18-camera-shift-vs-p-shift/sbs_camera.png`), and 40 puts the camera
through the scenery. So the usable value is somewhere below 1 and above whatever 0.6 was.

⚠️ **Sharpening that number is blocked, not merely unfinished.** With the world running, the two
eyes are ~130 ms apart in a moving car, so everything in frame has moved between them and a
disparity measurement cannot separate "the eye moved" from "the car moved". **Holding the world
still for the second eye is now a prerequisite for measuring anything about the pair**, not just a
quality improvement. That reorders the build list.

## What is NOT established

- The right eye separation, in either units or millimetres. The reader's world-unit question is
  still open.
- Whether the lighting defect is truly cured, as opposed to not having been provoked.
- Whether `sub_825C3918`'s scissor rectangles were the actual mechanism — the fix is at the level of
  the whole class ("CPU camera decisions see the centre eye"), so a cure does not identify the
  culprit.
- Nothing about holding the world still; nothing in a headset.
