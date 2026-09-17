# /gs 2026-09-17: one off-vocabulary tag, and the inbox README is the short template

**From:** `/gs` sweep, home PC, 2026-09-17. **Owner:** modding.

## 1. `[verified-static 2026-09-16]` is not a vocabulary name

`modding-notes/2026-09-16-camera-constants-and-background-input.md:47` carries
`[verified-static 2026-09-16]`. It reads as a strong claim but counts as untagged to every tool
(`/gs` check 3b, DATED group) `[verified-numerically 2026-09-17, n=1]`.

**Fix:** retag by what was actually done. A static read of the binary or files is
`[inferred-static 2026-09-16]`; a number checked against a dump is `[verified-numerically 2026-09-16]`.
Line numbers can drift; search for `verified-static`.

## 2. `engine-research/inbox/README.md`

Check 5 flags it as missing the `Supersedes:` protocol and the tag rules. Copy
`unreal-gold-vr/engine-research/inbox/README.md`.
