# Making the SRB2 / SRB2Kart OpenGL renderer actually work on the PS Vita

**For: [Rinnegatamante/srb2-vita](https://github.com/Rinnegatamante/srb2-vita) and anyone
porting a Doom-engine game (SRB2, SRB2Kart, Legacy…) to the Vita with vitaGL.**

The 2019 `srb2-vita` port shipped with the hardware renderer effectively unusable, and the
software renderer as the practical default. We got the **OpenGL renderer running at ~60 FPS**
on SRB2Kart, and every blocker turned out to be a *specific, small* incompatibility between
what the engine assumes about OpenGL and what vitaGL actually implements.

Writing them all down here, because none of them are discoverable without a crash dump and a
lot of patience — and because they apply to SRB2 itself, not just Kart.

---

## The seven blockers

### 1. vitaGL only reads **float** vertex attributes

Not "prefers". *Only.* Any `glVertexPointer` / `glColorPointer` / `glTexCoordPointer` with
`GL_SHORT`, `GL_UNSIGNED_BYTE`, etc. is read as garbage or ignored.

This bites the engine in two places:

- **MD2/MD3 models.** `hw_model.c` loads "tinyframes" as **shorts** (`useFloat = false`) and
  draws them with `GL_SHORT` pointers. On Vita they must be converted short→float into a
  scratch buffer *on every draw* (both the interpolated and non-interpolated paths).
- **The sky dome.** `pglColorPointer(..., GL_UNSIGNED_BYTE, ...)` is silently dropped, so the
  dome renders black. Our fix was to drop the colour array entirely and use a flat
  `glColor4ubv(white)` — the gradient is lost, but the sky is visible.

### 2. Model VBOs exhaust vitaGL's 128 buffer objects

`CreateModelVBOs()` creates **one VBO per model frame**. vitaGL has `BUFFERS_NUM = 128`
(`source/shared.h`). With a few characters on screen you blow past that instantly — and
`glGenBuffers` **fails silently** (see `vitagl/ISSUE-glGenBuffers-silent-exhaustion.md`):
it writes nothing, the caller keeps `vboID = 0`, `glBindBuffer(0)` sets
`vertex_array_unit = -40960`, and `glBufferData` corrupts memory by indexing
`gpu_buffers[-40960]`.

The crash then lands *somewhere else entirely*, minutes later. We chased phantom bugs for
hours before dumping the core.

**Fix:** make `CreateModelVBOs` a no-op on Vita and draw models from **client arrays**
(with the short→float conversion from §1).

### 3. `glDrawElements` indices are always cast to `uint16_t`

Whatever type you declare. The engine's batching path uses `GL_UNSIGNED_INT`, so anything
past index 65535 renders as garbage.

**Fix:** a `batchindex_t` typedef (`UINT16` on Vita), `GL_UNSIGNED_SHORT` as the batch index
type, and flush the batch *before* the vertex count can reach 65536.

### 4. `glCopyTexImage2D` is a **no-op** → black screen

The engine's `HWR_MakeScreenFinalTexture()` / `HWR_DrawScreenFinalTexture()` pass copies the
backbuffer into a texture and draws it fullscreen. On vitaGL that copy does nothing, so the
engine paints an **empty texture over the finished frame**: the game runs, the audio plays,
and the screen is black.

**Fix:** skip the whole "final texture" pass on Vita (we already render at native resolution,
so the rescale is pointless) and present the backbuffer directly with
`vglStopRendering()` / `vglStartRendering()`.

**Consequence:** screen wipes/fades use the same mechanism, so they must be disabled
(`F_RunWipe` returns immediately; `StartScreenWipe`/`EndScreenWipe`/`DoScreenWipe` become
no-ops). The intermission's `usebuffer` capture likewise has to be turned off — it draws a
ghost texture otherwise.

### 5. Texture 0 cannot be sampled → untextured polygons are invisible

The engine uses `NOTEXTURE_NUM = 0` to mean "no texture" (`PF_NoTexture`). vitaGL cannot
sample texture 0, so **every untextured polygon silently disappears** — the menu dimming
overlay, `V_DrawFill`, etc. This is why the title screen logo shows *through* the menus.

**Fix:** create a 1×1 white texture at init and bind *that* wherever the engine binds texture
0 (in `SetNoTexture()` **and** in the two binds inside the batching flush).

### 6. `glRotatef` ignores negative axes

`glRotatef(angle, 0.0f, -1.0f, 0.0f)` does **nothing** on vitaGL — it only tests `== 1.0f`
(see `vitagl/0002-glRotatef-honour-negative-axes.patch`). Models rotate with the camera or
appear upside down while sprites are correct.

**Workaround in the engine** (if you can't patch vitaGL): pass a **positive** axis and negate
the angle. Mathematically identical.

### 7. SDL's message boxes crash under vitaGL

`I_Error` → `SDL_ShowSimpleMessageBox` → SDL's Vita backend assumes an SDL **GXM renderer**
that does not exist when you render with vitaGL → `gxm_swap_for_common_dialog` data abort.

The crash *replaces* the real error, so you never learn why the game actually died.

**Fix:** write the `I_Error` message to a file instead (we use
`ux0:data/srb2kart/error.txt`). **Read that file first** whenever the game "just crashes".

---

## Performance, once it renders

Measured on a PS TV (SRB2Kart, in-race, 64-frame windows):

| | Before | After |
|---|---|---|
| Frame time | 26.2 ms (≈41 FPS) | **15.0 ms (≈66 FPS)** |

Two things mattered, and **neither was the CPU** — which is what everyone assumes:

1. **You are fill-rate bound.** Presentation cost **14.5 ms of a 26 ms frame**. Cutting CPU
   work (models off, draw distance) barely moved it. Cutting **pixels** did: rendering at
   640×368 instead of 960×544 is 45 % of the pixels and presentation dropped to 7.4 ms —
   exactly proportional. The Vita's display controller upscales to the panel **in hardware,
   for free**. `vglInitExtended()` accepts 480/640/720/960 widths; use them.

2. **CPU and GPU do not overlap by default.** vitaGL ships with `DISPLAY_BUFFER_COUNT 2`, so
   `displayQueueMaxPendingCount` is 1 and the CPU **blocks** in `vglStopRendering()` waiting
   for the GPU every frame — the two costs *add up* instead of overlapping. Building vitaGL
   with `DISPLAY_BUFFER_COUNT 3` (triple buffering) lets the CPU run a frame ahead. Costs
   ~2 MB of CDRAM.

### And the stalls are not the renderer at all

Frame spikes of up to **one full second** turned out to be the **memory card**: SRB2 reads a
music track out of the WAD *the moment it starts playing* (250–740 ms of I/O, mid-frame), and
each sound effect the first time it is played. Preload the raw lumps at startup — **without**
decoding them — and the stalls vanish entirely.

Do **not** just switch on the engine's `precachesound`: it precaches the *converted* sounds
(`ds2chunk` inflates 8-bit mono to 16-bit stereo 44 kHz, ~16×) and will blow the heap.

---

## Other engine-level traps on Vita (not vitaGL's fault)

- **`strlcpy`/`strlcat`.** SRB2 defines its own in `string.c` with a wrong return value. As
  explicit link objects they **override libc** — and VitaSDK's newlib calls `strlcpy` inside
  `__realpath()`, which runs on *every* `open()`. Every path gets truncated to `ux0:/`, which
  is a directory, so newlib "successfully" opens it with `sceIoDopen`. Result: `fopen()`
  returns valid handles that read nothing and **every WAD looks corrupt**. Define
  `SRB2_HAVE_STRLCPY` on Vita. *This traps any homebrew shipping its own `strlcpy`.*
- **`fopen(..., "a")` does not create the file** on this libc. Use raw `sceIo*` when in doubt.
- **`%zu` is not supported** by the libc — "Out of memory allocating zu bytes".
- **Non-blocking sockets**: there is no `<sys/ioctl.h>`, so `ioctl(s, FIONBIO)` — which sits
  behind `#ifdef FIONBIO` — is **silently skipped** and the game blocks forever in `recvfrom`.
  Use `setsockopt(SOL_SOCKET, SO_NONBLOCK)`.
- **`sendto(sock, NULL, 0, ...)`** (an empty datagram, used by `i_tcp.c` to validate a
  resolved address) is **rejected by sceNet** although BSD sockets return 0. Every address
  gets discarded, no node is created, and the client hangs on *"Connecting to server"*.

---

## Contact

This came out of the SRB2Kart PS Vita / PS TV port. Happy to help test any of it against
SRB2 proper — a lot of this should apply verbatim.

Thank you for vitaGL and for srb2-vita; this port is built directly on both.
