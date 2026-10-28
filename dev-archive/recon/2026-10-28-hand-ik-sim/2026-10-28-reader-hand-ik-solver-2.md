# Hand IK, part 2: the palette writer, the player's IK object, the target's space

From: the background reader of the 2026-10-28 live session (static reading only; nothing launched or attached).
Follows `2026-10-28-reader-hand-ik-solver.md`. All `[inferred-static 2026-10-28]` unless tagged otherwise.

⚠️ **Correction to part 1:** the IK object is NOT `sub_821CED18`'s r7. Callers pass r7 = `CWObject+80`
(something else). Inside `sub_821CED18`, r31 is set to **`ClientData+4384`** before the IK calls (the
`addi r31,r28,4384` and `addi r11,r27,4384; mr r31,r11` lines, just after `loc_821D1524` / at `loc_821D1620`). The `IK+...` offsets in part 1 are all relative to that object, and they still hold.

## 1. The palette writer: `sub_8224AB18`

- **Called from** `sub_82248C80` (the shader-program choice inside the pre-draw commit `sub_82249488`).
- **Argument:** r7 = `*(attr+640)`, where attr is the current render-attribute record. It re-uploads only when
  that pointer differs from the cached `RC+17192`.
- **Descriptor layout:**
  - +0 = pointer to an array of 64-byte 4x4 matrices
  - +4 = `u16*` index table, or null
  - +8 = count, capped at **52**
- **What it writes:**
  - It stores 96 into the byte at `RC+26` (the base register).
  - It writes from shadow offset `+3456` (= +1920 + 96*16), so **c96 .. c96+3*52-1 = c251** at most.
  - For each palette entry i it takes matrix `M = base[ remap ? remap[i] : i ]` and writes three constants,
    each one a COLUMN of M: `(M[0][k], M[1][k], M[2][k], M[3][k])` for k = 0, 1, 2. So the translation
    (row 3, at +48) lands in .w. This matches the live finding.
- **It copies verbatim (transpose only, no multiply).** So the palette is in exactly the space of the source
  matrices. The live translations were world-like (bone 0 near the camera), so **the source array is
  bone-to-world**. The code agrees that nothing else is folded in on the way.
- **The model-to-view matrix is set separately.** `sub_82299E30` sets the palette with `sub_82236B18(*(VB+8))`
  and calls `sub_82763D50(RC, *(VB+12), *(VB+44))`. That routine copies `*(VB+12)` into the attribute record's
  matrix slot, or identity if it is null. So for a skinned draw, VB+12 is most likely world-to-view
  `[hypothesis]`.
- **A remap is supported** (the `u16` table at desc+4). Whether the player's meshes use one is **not known**: I did
  not find the code that fills the descriptor (it is reached only through the VB at `VB+8`). So
  **palette index b = skeleton bone b only if desc+4 is null**.
- **One live log settles both questions.** Hook `sub_8224AB18` on entry and log, per call:
  - r7, `*(r7+0)`, `*(r7+4)`, `*(r7+8)`, and the u16 table if there is one
  - compare `*(r7+0)` with the player's `*(SI+36)` (SI = r5 at `sub_821BF988` entry)

  If they are equal, SI+36 is the palette source and therefore bone-to-world. The table, if present, maps
  palette slot -> skeleton bone (16 and 21 for the hands).

## 2. The player's IK object

- **Chain of objects:**
  - Character object = `CWObject` (r3 / r22 of `sub_821CED18`).
  - `ClientData = *(CWObject+396)` (a smart pointer; allocated in `sub_82143ED8`, 10448 bytes for the base class).
  - **`IK = ClientData + 4384`**. Its constructor is `sub_821B22E0`, called from `sub_821A3228`, the
    `CWO_Character_ClientData` constructor. The copy routine `sub_821A4820` copies it with `sub_821C4C48` at +4384.
- **Inside `sub_821CED18` the IK object is chosen two ways:**
  - `r27+4384`, with r27 = this character's ClientData.
  - `*(obj+396)+4384` for an object fetched by index `*(s16*)(CWObject+368)` through the world's vtable+288.
  - Either way, `sub_821BF988` gets it in r3.
- **Telling the player from an NPC:** read the ClientData vtable, `*(u32*)(IK-4384)`.

  | vtable | constructor | class, by the string just before it |
  | --- | --- | --- |
  | 0x82064B98 | `sub_821A3228` | `CWO_Character_ClientData` (base) |
  | 0x82061FE0 | `sub_82119668` | player branch, right after `CWObject_CharPlayer_P6` / inventory strings |
  | 0x82062190 | `sub_82117998` | subclass of 0x82061FE0 (10624 bytes); most likely the player you play `[hypothesis]` |
  | 0x820647C0 | `sub_82170768` | `CWO_CharDarkling_ClientData`, also derived from the player branch |
  | 0x82064FD0 | `sub_8211AB40` | `CWO_CharNPC_ClientData` |
  | 0x82065040 | `sub_8211AEF8` | `CWO_CharNPC_ClientData_P6` |
  | 0x820650F0 | `sub_821286F0` | `CWO_CharAngelus_ClientData` |

  `sub_8212F0D8` (10592 bytes) also derives from `sub_82119668`; I did not read its vtable. **Check live** which
  vtable the controlled character has. A test of "vtable is 0x82061FE0 or one of its subclasses" is the static
  best guess.
- **No global "the player" pointer was found in this pass.** The ClientData vtable is the cheapest test.
- **Not settled:** the first-person arms may be a second object. `sub_82165D20` calls `sub_821CED18` twice in a
  row, with two different objects. Log r3 at `sub_821BF988` per frame: one or two IK objects per frame tells
  which.

## 3. The explicit target: layout and space

- **`sub_821BF988` takes the IK object in r3** (it reads `IK+0`, +1448, +1452 straight off r3, and the
  caller passes the same r31 it tests at +1856). A hook on its entry has the IK object directly. Its other
  arguments:
  - r4 = skeleton
  - r5 = skeleton instance (so `*(r5+36)` = the bone matrices)
  - r6 = the game-object/world pointer (r21 in the caller)
  - r7 = the character pointer (r28 in the caller)
- **The slot layout:**
  - `IK+1864` = **vec3** (12 bytes, three floats). After it: +1876 and +1880 = the f1 argument, +1884 = 0.75.
  - `IK+1892` = vec3, then +1904 and +1908 = 0, +1912 = 0.75.
  - It is not a vec4: +1876 is a separate float.
- **What `sub_821BF880` writes:** the two points from message 0x34F4, copied unchanged (three words each); no
  transform anywhere on the way. `sub_821BF988` then puts the vec3 straight into the target matrix's position
  (+48), with the rotation rows copied from `*(SI+36)` bone 21 (A) or 16 (B). `sub_821BC360` blends it against a
  point made from the same `*(SI+36)` matrices.
- **So the target is in the space of `*(SI+36)`.** With the palette check in section 1 that is world space
  `[hypothesis until the one pointer comparison above is made live]`. The point is the grip point (bone 23 for A,
  22 for B), not the wrist.
- **What a hook writes, at `sub_821BF988` entry, every frame:**
  - the controller grip point in world units to `IK+1864` and/or `IK+1892`
  - `IK+0 |= 0x00100000` and/or `0x00200000`
  - clear 0x00400000 and/or 0x00800000
  - optionally state byte 3 at `IK+1448` / `IK+1450` and weight 1.0 at `IK+1452` / `IK+1456`, to skip the 0.2 s
    blend
  - a mode-1 note: the f1 value the game passes is 0.0 on the message path (+1876/+1880)
