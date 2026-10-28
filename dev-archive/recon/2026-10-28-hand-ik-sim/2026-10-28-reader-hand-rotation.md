# Giving the IK hand the controller's rotation

From: the background reader of the 2026-10-28 live session. Static only. All `[inferred-static 2026-10-28]`
unless tagged otherwise.

## 1. Where the target is built, and where to hook

- **`sub_821BF988` builds the target on its own stack, at `r1+96`.** One side at a time:
  - **Rotation:** rows +0/+16/+32 are copied from `*(SI+36) + 1344` (bone 21, side A) or `+1024` (bone 16,
    side B).
  - **Position:** row +48 is copied from the same bone matrix, then its x, y, z are replaced by `IK+1864`
    or `IK+1892`.
  - Then it calls **`sub_821BC360(IK, side, &target, skel, SI)`**, which moves only the position (+48..+56)
    for the blend.
  - Then it calls **the solver `sub_821EE4E0` with r6 = `&target`, inside `sub_821BF988`**.
  - This happens twice per call, once per side.
  - So **a write after `sub_821BF988` returns is too late**: the solve has already happened.
- **Cheapest hook: `sub_821EE4E0` on entry.**
  - r6 = the 64-byte target.
  - r3 = `IK+32`.
  - `*(u8*)(r3+8)` = 0 (chain A, right hand, `[verified-live 2026-10-28]` by the live session) or 1 (chain B,
    left hand).
  - To touch only the explicit path, also require `*(u32*)(r3-32) & 0x00100000` (A) or `& 0x00200000` (B).
    When `sub_821BF988` ran, the normal hand IK `sub_821B8300` is skipped that update. The character IK
    `sub_821BD498` uses chain bytes 4..9, so the byte test keeps it out.
  - Overwrite rows +0, +16 and +32 (x, y, z each; leave .w) and leave +48 alone.
- **Second choice: `sub_821BC360` on entry** (r5 = target, r4 = side). It never touches the rows, so a rotation
  written there survives into the solver. But `sub_821B8300` also calls it.

## 2. What the rows mean, and the hand bone's axes

- **Row-major, rows = axes.**
  - `sub_82114670` (point transform) computes `out = v.x*row0 + v.y*row1 + v.z*row2 + row3`, with rows at
    +0, +16, +32, +48. So row k = bone-local axis k expressed in world, and row 3 = position. Vectors
    multiply as row vectors (`world = local * M`).
  - The palette writer transposes these same matrices into columns, which fits the 4x3 seen at c96.
- **The target rows become the hand bone's world rotation exactly.** The end of `sub_821EE4E0`:
  1. after the two-bone solve (`sub_821EF450` + `sub_824208D8` on root and mid), it re-reads the hand's new
     parent world matrix;
  2. inverts it (`sub_82114838`, a rigid inverse);
  3. stores `target(rows 0..2) * inverse(parent)` as the hand's local matrix in `*(SI+32)` (translation 0);
  4. rebuilds world matrices from there down (`sub_82420678`).

  So hand world rotation = target rows 0..2. That means the 3x3 to store is simply **the three world directions
  we want the hand bone's local X, Y and Z axes to point**.
- **Which local axis runs along the fingers, and which out of the palm: not readable statically.** It is
  skeleton data, not code; no bone names exist. Two live reads give it cheaply:
  - **(a)** the grip point's offset in the hand's frame: bone 23's local pivot `*(skel+12) + 48*23 + 16`
    (vec3) for hand A, and bone 22's (`+48*22+16`) for hand B. `*(skel+12) + 48*b + 44` (u16) is each bone's
    parent; check it is 21 or 16. The dominant component of the pivot is the wrist-to-grip direction, roughly
    "along the fingers". `skel` = r4 at `sub_821BF988`.
  - **(b)** in the idle first-person pose, read bone 21's rows (`*(SI+36) + 1344`, three vec3 at +0/+16/+32)
    beside the camera rows. Whichever row is close to camera-forward is the finger axis. Whichever is close to
    plus or minus camera-down (or right, for a flat palm) is the palm normal.

## 3. The formula

Inputs:
- head-turned camera rows in world: **r** = right, **d** = screen-down, **f** = forward;
- head orientation `qh` and grip orientation `qg` from rex_xr_sample_head / rex_xr_sample_hands (OpenXR local,
  column vectors).

Steps:
1. **Head-relative grip:** `R = Rot(qh)^T * Rot(qg)`, with Rot(q) for q = (x, y, z, w):

   ```
   [1-2(y²+z²)   2(xy-zw)     2(xz+yw)  ]
   [2(xy+zw)     1-2(x²+z²)   2(yz-xw)  ]
   [2(xz-yw)     2(yz+xw)     1-2(x²+y²)]
   ```

   Its columns c0, c1, c2 are the controller's +X, +Y and +Z in head axes.
2. **Head axes to game world:** `W(v) = v.x*r - v.y*d - v.z*f` (head +X = right, +Y = up = -d, -Z = forward = f).
3. **Controller axes in world:**
   - `X = W(c0)` (right)
   - `Y = W(c1)` (up, out of the top)
   - `F = -W(c2)` (pointing direction)
4. **Hand rows:** for k = 0, 1, 2, `row_k = O[k][0]*X + O[k][1]*Y + O[k][2]*F`. O is a fixed 3x3 per hand that says
   where each hand-bone axis sits relative to the controller.
   - Once section 2's reads have named the finger and palm axes, O is a signed permutation. Example:
     if local +X = fingers and local +Z = out of the palm (palm facing left for the right hand), then
     `row0 = F`, `row2 = -X`, `row1 = row2 × row0`. Pick the sign of the cross product so that
     det(rows) = det(the animation's own rows).
   - Or calibrate O in one step: hold the controller the way the idle animation holds the hand, read the
     animation rows H (rows of bone 21) and the controller basis C (rows X, Y, F) at that moment, and use
     `O = H * C^T`. Re-orthonormalise O, then keep it.
5. **Write the result:** row_k goes to target +16k (x, y, z). Keep the rows orthonormal; the solver does not
   renormalise `[hypothesis]`.

⚠️ Check `det[r; d; f]` once live. If it is -1 (a mirrored basis), the mapping in step 2 mirrors too, and only
the calibrated-O route (or flipping one axis) gives a proper rotation `[hypothesis]`.

**Measured vs inferred:** everything above was read from code. The only measured or live facts are the coordinator's
(chain A = right hand; the positions work). The finger and palm axes still need the two live reads in section 2.
