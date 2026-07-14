# Giving back: reports & patches for the projects we built on

This port exists because of other people's work. While making it we found real bugs in those
projects — and worked around them locally, which helps nobody but us.

**This folder is what we owe them.** Everything here is written to be sent upstream as-is.

**Read this first:** the vitaGL section below was **wrong** — we reported bugs from a 2019
snapshot as if they were current, without checking upstream. Those reports are withdrawn. It is
a cautionary tale, and it is kept here on purpose rather than quietly deleted.

The SRB2Kart fixes were verified against the **current** `master` and are submitted as
[merge request !365](https://git.do.srb2.org/KartKrew/Kart-Public/-/merge_requests/365).

---

## ~~vitaGL~~ — WE GOT THIS WRONG

**The vitaGL patches and reports that used to be here have been withdrawn. They were written
against a copy of vitaGL from November 2019 — 1167 commits behind upstream — and every single
one of them had already been fixed years ago.** Three issues were opened on that basis and
rightly closed.

The full accounting is in [`vitagl/CORRECTION-we-were-looking-at-a-2019-snapshot.md`](vitagl/CORRECTION-we-were-looking-at-a-2019-snapshot.md).

Nothing here should be sent to vitaGL. The port should move to a current vitaGL instead — that
would probably remove several of the workarounds in `src/hardware/r_opengl/r_opengl.c`.

## [srb2-vita](https://github.com/Rinnegatamante/srb2-vita) — Rinnegatamante

`srb2-vita/OPENGL-ON-VITA.md`

The big one. The 2019 port shipped with the hardware renderer effectively unusable; we got it
running at **~60 FPS**. This document lists **all seven blockers** and how each was solved —
float-only vertex attributes, VBO exhaustion, uint16-only indices, `glCopyTexImage2D` being a
no-op (→ black screen), texture 0 being unsamplable (→ invisible untextured polygons),
`glRotatef` and negative axes, and SDL message boxes crashing under vitaGL.

It also covers the performance findings (you are **fill-rate bound**, not CPU bound) and the
engine-level Vita traps (`strlcpy` destroying all file I/O, `FIONBIO` being skipped, audio
stalls from the memory card).

Most of it should apply **verbatim to SRB2 proper**.

## [SRB2Kart](https://git.do.srb2.org/KartKrew/Kart-Public) — Kart Krew

| File | What | Status |
|---|---|---|
| `srb2kart/0001-d_clisrv-guard-CL_DOWNLOADHTTPFILES-with-HAVE_CURL.patch` | Build fails when networking is on and curl is off — the normal situation on a console. | ✅ applies cleanly on `master` |
| `srb2kart/0002-http-mserv-guard-logstream-with-LOGMESSAGES.patch` | Same, for `logstream`, which only exists on Windows/Unix/macOS. | ✅ applies cleanly on `master` |
| `srb2kart/0003-v_video-do-not-dereference-missing-credit-font-glyphs.patch` | **Opening the credits can crash.** `hu_stuff.c` sets `cred_font[c] = NULL` for characters with no `CRFNTxxx` lump, but the two credit-font routines check only that the character is *in range* before dereferencing it. Any credit line containing `/`, `+`, `%`… is an instant null dereference. Latent in vanilla (its credits happen to avoid those characters); we hit it the moment we added a line of our own. | ✅ applies cleanly on `master` |
| `srb2kart/0004-r_opengl-check-malloc-in-DrawPolygon-batching.patch` | The batching arrays double themselves with **unchecked `malloc`s**, then `memcpy` into them. When the heap is exhausted the crash lands *in the renderer*, pointing at a phantom OpenGL bug while the real cause is elsewhere. A dropped polygon beats memory corruption. | ✅ applies cleanly on `master` |
| `srb2kart/0005-hw_cache-do-not-follow-out-of-range-patch-columns.patch` | `HWR_DrawPatchInCache()` follows a patch's column offsets **without checking the column index against the patch width**. One entry past `columnofs[]` and you take arbitrary bytes for an offset and dereference them — a data abort *inside the texture cache*, with the zone allocator and the WAD loader both reporting success. Corrupt data should cost a texture, not the process. | ✅ applies cleanly on `master` |
| `srb2kart/ISSUE-ogg-sfx-decoded-inside-the-game-tic.md` | **Sound effects are OGG, and SDL_mixer decodes them in full on first play — inside `P_Ticker`.** Measured on a 444 MHz ARM: 164 ms for the last-lap sound, 243 ms at the finish line, up to 2.4 s for the worst one; 19 frames per race over budget, with zero disk/GPU/texture cost on those frames. Invisible on a modern PC, brutal on anything slow — and it lands exactly on the big moments of a race. Includes the traps we fell into (`S_ClearSfx` in `P_SetupLevel` throws away any precache; `S_GetSfxLumpNum` falls back to `dsthok`, so naive "decode everything" loops decode one sound 9800 times). | report |
| `srb2kart/ISSUE-spectator-reentry-wrong-index.md` | `K_CheckSpectateStatus()` reads `players[i]` where `i` indexes the **respawn list** — a joining player is blocked by *someone else's* cooldown. **Must be fixed upstream, not downstream**: the function runs inside the deterministic lockstep, so fixing it client-side desyncs you from vanilla servers. We tried; it broke every online game. | report |

## [VitaSDK](https://github.com/vitasdk)

`vitasdk/NOTES.md`

- `fopen(path, "a")` **does not create the file** (silently drops all writes).
- `__realpath()` uses `strlcpy` — so an app that ships its own `strlcpy` (every Doom engine
  does) **destroys all file I/O** in a way that looks like asset corruption. This deserves to
  be documented loudly, if not fixed.
- No `<sys/ioctl.h>`: `#ifdef FIONBIO` blocks compile to nothing and sockets stay blocking.
- `sendto(sock, NULL, 0, …)` is rejected although BSD sockets accept it.

---

## Suggested order

1. **vitaGL patches** — smallest, clearest, purely beneficial.
2. **SRB2Kart portability patches** — two one-line guards, no behaviour change on desktop.
3. **srb2-vita notes** — offer to help test against SRB2 proper.
4. **SRB2Kart spectator issue** — needs a maintainer's decision (it's a netcode-visible fix).
5. **VitaSDK notes** — the `strlcpy` one especially; it will save someone weeks.
