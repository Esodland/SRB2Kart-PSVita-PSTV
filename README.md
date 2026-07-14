# SRB2Kart — PS Vita / PS TV port

A native port of **[SRB2Kart](https://srb2.org/mods/)** to the PlayStation Vita and PlayStation TV.

Hardware-accelerated OpenGL renderer, full online multiplayer (including the official
Kart Krew master server), on-screen keyboard, LiveArea assets, and a Performance mode
that targets 60 FPS.

> SRB2Kart is a kart racing mod based on **[Sonic Robo Blast 2](https://srb2.org/)**, a 3D
> Sonic the Hedgehog fangame built on a modified version of
> **[Doom Legacy](http://doomlegacy.sourceforge.net/)**.

---

## Two builds — which vitaGL, and which branch

This repository ships the port in **two parallel versions**. You are looking at the default one.

| Branch | Built against vitaGL… | Extra file on the console | Resolution change |
|---|---|---|---|
| **`vitagl-modern`** *(default — this branch)* | the **latest** vitaGL (upstream `master`, as of **2026‑07‑14**) | **needs `libshacccg.suprx`** in `ur0:data/` | **live, no restart** |
| [`master`](../../tree/master) | the **old 2019** vitaGL that shipped with `srb2-vita` (commit `694b387`) | none | requires a restart |

The port was **originally compiled against the 2019 vitaGL** — that is the version `srb2-vita`
came with, and it needs nothing extra at runtime. That build lives on the `master` branch and is
still fully supported.

This `vitagl-modern` branch **recompiles the exact same port against current vitaGL**, which is
over a thousand commits newer. The payoff: a pile of old rendering workarounds disappear, the
video resolution can be changed **live, without restarting the game**, and it runs at least as
well. The only cost is **one extra file** on the console — `libshacccg.suprx`, the runtime shader
compiler that modern vitaGL relies on (a legal, hash‑verified copy is linked from
[`BUILDING-MODERN-VITAGL.md`](BUILDING-MODERN-VITAGL.md)).

**If you just want a working VPK with nothing extra to install, use [`master`](../../tree/master).**
**If you want the newest engine and live resolution switching, use this branch.**

---

## Credits — please read this first

**This port stands almost entirely on other people's work.** It is a thin layer of glue on
top of decades of engineering by others:

| Who | What |
|---|---|
| **[Kart Krew](https://srb2.org/mods/)** | SRB2Kart itself — the game, its physics, its art, its netcode. |
| **[Sonic Team Junior](https://srb2.org/)** | Sonic Robo Blast 2, the engine SRB2Kart is built on. |
| **[Doom Legacy](http://doomlegacy.sourceforge.net/) & id Software** | The Doom engine underneath it all. |
| **[Rinnegatamante](https://github.com/Rinnegatamante)** | **[vitaGL](https://github.com/Rinnegatamante/vitaGL)** (the OpenGL implementation this port renders with) and the original **[srb2-vita](https://github.com/Rinnegatamante/srb2-vita)** port of SRB2 (2019), which is where the "Vita glue" for this engine came from. Without these, this port would not exist. |
| **[Ryo](https://github.com/RyoSaeba89)** | Help and guidance throughout the port. |
| **[TheFloW](https://github.com/TheOfficialFloW)** | HENkaku, taiHEN, VitaShell — the homebrew platform everything runs on. |
| **[VitaSDK](https://vitasdk.org/) contributors** | The toolchain, newlib, SDL2, and the `vdpm` package manager. |

Kart Krew is in no way affiliated with SEGA or Sonic Team. No ownership is claimed over any
of SEGA's intellectual property used in SRB2.

---

## Status

| | |
|---|---|
| **Renderer** | OpenGL via **current vitaGL** (hardware accelerated) — needs `libshacccg.suprx` on the console |
| **Resolution** | Switchable **live, no restart** (960×544 / 720×408 / 640×368 / 480×272) |
| **Performance** | ~60 FPS average in *Performance* mode (640×368); ~40 FPS in *Quality* (960×544) |
| **Single player** | ✅ Race, Time Attack, Battle |
| **Local multiplayer** | ✅ Splitscreen (untested with 2+ controllers, feedback welcome) |
| **LAN multiplayer** | ✅ Host and join by direct IP |
| **Online multiplayer** | ✅ **Official Kart Krew master server** (server browser works) |
| **On-screen keyboard** | ✅ For IP address, server name, player name |
| **Known gaps** | Screen wipes/fades disabled; sky gradient flat; heavily-modded servers can exceed the console's memory (see *Limits*) |

---

## Installing

### Requirements

- A PS Vita or PS TV with **HENkaku / Enso** (custom firmware).
- **~600 MB free** on `ux0:` (the game assets are large).
- The **SRB2Kart v1.6 game data**. This port ships *no* game assets — you must supply them
  from the [official SRB2Kart release](https://srb2.org/mods/).
- **`libshacccg.suprx` in `ur0:data/`** — the runtime shader compiler modern vitaGL needs.
  This is **only required by this `vitagl-modern` build**; the [`master`](../../tree/master)
  build does not need it. A legal, hash‑verified way to install it is
  [AnimMouse/SceShaccCg](https://github.com/AnimMouse/SceShaccCg). *Without it, the game shows a
  black screen.*

### Steps

1. Copy `srb2kart.vpk` to your console and install it with **VitaShell**.
2. Copy the SRB2Kart data files into **`ux0:data/srb2kart/`**:
   ```
   srb2.srb  gfx.kart  textures.kart  chars.kart  maps.kart
   sounds.kart  music.kart  bonuschars.kart
   (optional: mdls.dat + mdls/ for 3D models)
   ```
3. Launch it from the LiveArea.

> ⚠️ **Booting takes about 75 seconds.** Most of it is textures; the rest is audio, decoded
> up front on purpose — see below.
>
> A **progress bar** keeps you company throughout, and it tells the truth: each step's share of
> the bar is proportional to how long that step actually takes (measured on the console), and a
> dedicated thread keeps it moving even during the long silent stretch where the engine loads
> textures and prints nothing. If a step runs longer than expected the bar *slows down* and
> creeps towards the next mark without ever reaching it — so it can never fill up before the
> game is ready, and it never freezes. A frozen screen looks like a crash; a moving bar looks
> like a loading screen.
>
> Level loads get the same treatment.

### Why loading is slow (and why we chose it that way)

**Short version: we pay once, at the door, so the race itself never stutters.**

SRB2Kart was written for a PC: it loads audio **on demand**, at the exact moment it is needed.
On a PC that is invisible. On a Vita it is not, and it bites twice.

**Music** is pulled out of `music.kart` (a **105 MB** file) *the frame a track starts playing* —
we measured **250–740 ms of disk I/O, mid-race, inside a single frame**. **Sound effects** are
OGG, and SDL_mixer **decodes them in full the first time they are played** — inside the game
tic. Measured on the console: **164 ms** for the last-lap sound, **243 ms** at the finish line,
up to **2.4 s** for the worst one. Nineteen frames per race blew the budget, and — this is the
cruel part — always on the big moments: first item, last lap, crossing the line.

So the port does that work at startup instead: the fixed music jingles are read into RAM, and
the sound effects a race actually uses are **decoded** there. In exchange, **in-race disk I/O
and sound decoding both drop to zero**, and the stalls are gone.

The trade is intentional and, we think, obviously the right way round: **~20 seconds of loading,
once per session, buys you a race that does not freeze.** A frozen frame mid-corner costs you
the race; a loading bar costs you patience.

Three details make it work at all — all three learned the hard way (see *Performance work*):

- Decoding *every* sound is impossible: **926 unique sounds = 224 MB** of PCM, on a 256 MB heap.
  So the game keeps a list of the sounds it has **actually had to decode**
  (`ux0:data/srb2kart/sfxwarm.txt`, ~80 entries) and decodes only those. It grows the list as
  you play, so a sound can only ever surprise you once. Deleting the file makes it relearn.
- Decoded sounds are **kept across level loads**, which is why restarting a race is instant
  (~2 s) rather than paying for everything again.
- Only the **current** map's music track is loaded (map tracks are 92 MB of that 105 MB file).

### Updating

If you only replace `eboot.bin`, the LiveArea keeps its **cached** background. To refresh the
LiveArea, **uninstall the app first**, then install the new VPK. Your data in
`ux0:data/srb2kart/` survives uninstalling.

---

## Controls

Buttons come through **SDL_GameController**, so a DualShock 4 on PS TV and the handheld Vita
produce the *same* indices. L2/R2/L3/R3 only exist on a DS4 — those bindings are simply inert
on a handheld.

| Action | Button |
|---|---|
| Accelerate | ✕ |
| Brake | ▢ |
| Drift | ○ · R1 · R2 |
| Item | L1 · L2 |
| Look behind | △ |
| Camera | R3 |
| Menu | Options |
| Scores | D-pad ↑ |

Menus: ✕ confirms, ○ cancels. **✕ also opens the on-screen keyboard** on any text field
(server IP, server name, player name).

> `kartconfig.cfg` **overrides the defaults**. If bindings look wrong after an update, delete
> `ux0:data/srb2kart/kartconfig.cfg`.

---

## Graphics modes

**Options → Video Options → Graphics Mode**

| | Quality | Performance |
|---|---|---|
| Render resolution | 960×544 (native) | **640×368** (hardware-upscaled) |
| 3D models | On | Off (sprites) |
| Skybox | On | Off |
| Sprite draw distance | Infinite | 2048 |
| Measured frame time | ~25 ms (≈40 FPS) | **~15 ms (≈66 FPS)** |

### Resolution — changed live (this branch)

**Options → Video Options → Set Resolution…** — on this `vitagl-modern` branch the change
applies **live, without restarting the game** (via modern vitaGL's `vglSwapResolution`). Only
the four resolutions the Vita's display controller can actually scan out are offered — **960×544,
720×408, 640×368, 480×272** — and the display hardware upscales the smaller ones to the panel for
free. The choice is remembered across launches.

> On the [`master`](../../tree/master) branch (2019 vitaGL) this instead **requires a restart** —
> the render surface is fixed at startup there. Live switching is one of the reasons this branch
> exists.

A quirk worth knowing: SRB2's HUD/text is drawn on a fixed **320×200** canvas and blown up by an
*integer* factor (`width ÷ 320`). So a **higher** resolution gives **bigger *and* sharper** text
(960 = ×3, 640 = ×2, 480 = ×1) — the opposite of a PC, where more pixels usually means a smaller
UI. This is normal Doom‑engine behaviour, not a bug.

---

## Multiplayer

- **Online**: the server browser works and talks to the official master server over HTTPS.
- **LAN / direct**: *Specify IPv4 address* — press ✕ to bring up the on-screen keyboard.
- **Hosting**: *Internet/LAN…* works from the console.
- **Add-ons** download automatically when you join (over HTTP when the server offers a mirror).

### Limits — heavily-modded servers

**A server distributing more than roughly 100 MB of add-ons will run out of memory.** We tested
one with **274 MB** (dozens of custom characters): the files download fine, the game loads them,
and then the level fails to start — the GPU runs out of texture memory, because every character
sprite becomes an RGBA texture, and a Vita has **128 MB of VRAM** against the several gigabytes
these servers assume.

There is no setting that fixes this; it is the hardware. Vanilla servers and lightly-modded ones
work normally.

> Since the Doom engine **cannot unload a WAD**, changing servers means restarting the game —
> true on PC too. On the Vita the game offers to do it for you: answer **Y** and it relaunches
> itself, add-ons cleared. There is also a `restart` console command.

> **The netcode is deterministic lockstep.** Every peer simulates the game from player inputs
> alone and must reach a *bit-identical* result. This is why this port fixes **nothing** in
> the simulation, not even obvious upstream bugs — see *Rules for contributors* below.

---

## Licence — and why the TLS library matters

This port is a derivative work of SRB2Kart, which is **GPL-2.0**. So is everything here: the
source, and the binary. The original copyright headers are untouched, and **no game asset is
redistributed** — you supply your own.

One consequence deserves its own paragraph, because it dictated a real engineering decision.

The master server is HTTPS, so the game needs a TLS library. The obvious choice — the `curl`
package built against **OpenSSL** — makes the binary **undistributable**: OpenSSL's licence is
GPL-incompatible, and GPLv2's "system library" exception does not apply when you *bundle* the
library yourself. Shipping that VPK would have been a licence violation, quietly.

We tried the console's own TLS stack (`SceHttp`/`SceSsl`), which *would* qualify as a system
library. It does not work: `ms.kartkrew.org` requires TLS 1.2, and the stock Vita stack is too
old to negotiate with it — the handshake dies before certificate verification. (That is exactly
what the *iTLS-Enso* plugin exists to fix, and we did not want to force it on players.)

The answer is **mbedTLS**, which is dual-licensed **Apache-2.0 or GPL-2.0-or-later**: taking the
GPL option makes everything compatible. The VitaSDK ships a `curl-mbedtls` package, so the port
links that. No OpenSSL, no plugin required, and the add-on HTTP downloads keep working.

## Building from source

### Toolchain

```bash
# VitaSDK: https://vitasdk.org/
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH

# Packages (vdpm)
vdpm SDL2 SDL2_mixer vita2d libvorbis libogg libmodplug flac libxmp mpg123 opusfile
vdpm zlib libpng jpeg freetype
vdpm curl openssl zstd     # master server (HTTPS) — curl here is built against OpenSSL
```

**vitaGL** must be built with `HAVE_SBRK=1` (it needs newlib's heap symbols).

> 🧭 **Two branches, two vitaGL versions — build the right one:**
>
> - **This branch (`vitagl-modern`)** links **current upstream vitaGL** and also needs
>   **vitaShaRK** + **SceShaccCgExt** (modern vitaGL compiles its shaders at runtime). The full
>   recipe — including the `libshacccg.suprx` the console then needs — is in
>   **[`BUILDING-MODERN-VITAGL.md`](BUILDING-MODERN-VITAGL.md)**.
>   ```bash
>   git clone https://github.com/Rinnegatamante/vitaGL && cd vitaGL
>   make HAVE_SBRK=1 NO_SPLASHSCREEN=1        # latest master
>   ```
> - **The [`master`](../../tree/master) branch** links the **2019** vitaGL that came with
>   `srb2-vita` and needs no runtime shader compiler. Pin the commit and patch triple buffering:
>   ```bash
>   git clone https://github.com/Rinnegatamante/vitaGL && cd vitaGL
>   git checkout 694b387                       # the 2019 version master was developed against
>   # set DISPLAY_BUFFER_COUNT 3 in source/shared.h (triple buffering)
>   make HAVE_SBRK=1 install
>   ```
>
> `VITAGL_DIR` / `SHACCEXT_DIR` in `src/sdl/SRB2Vita/Makefile.cfg` point the build at your
> checkout — override them on the `make` command line if your paths differ.

### Compile

```bash
cd src
make PSP2=1 -j8
# -> sdl/SRB2Vita/build/eboot.bin (already signed)
```

`make PSP2=1 NOHW=1` builds the software renderer instead (480×272, much slower).

### Package

`vita-pack-vpk` (not a plain zip — the promoter validates the package):

```bash
vita-pack-vpk -s sce_sys/param.sfo -b eboot.bin \
  -a icon0.png=sce_sys/icon0.png \
  -a bg.png=sce_sys/livearea/contents/bg.png \
  -a startup.png=sce_sys/livearea/contents/startup.png \
  -a template.xml=sce_sys/livearea/contents/template.xml \
  srb2kart.vpk
```

> ⚠️ **All PNGs in `sce_sys/` must be indexed (PNG color type 3).** A truecolor PNG makes the
> installer fail at the very end with `0x8010113D` and no explanation. Every homebrew icon on
> the system is indexed; this is not documented anywhere.

---

## Rules for contributors

**Never change the game simulation.** Anything reachable from `G_Ticker` / `P_Ticker`
(`p_*.c`, `k_kart.c`, `g_game.c`) must stay bit-identical to upstream, *even to fix an obvious
bug*. The netcode is deterministic lockstep: a "fix" makes your client diverge from vanilla
servers and you get *synch failure* the moment the fixed code path runs.

This was learned the hard way: correcting an off-by-one index in `K_CheckSpectateStatus`
(it reads `players[i]` where `i` indexes the *respawn list*, not the player array) desynced
every online game as soon as a spectator tried to rejoin. The bug is still there, on purpose,
with a comment explaining why.

Platform code — rendering, audio, input, file I/O, networking transport — is fair game.

---

## What it took: notes on the port

These are the non-obvious things. If you are porting another Doom-engine game to the Vita,
this is the part worth reading.

### File I/O silently lied

`fopen()` returned valid handles that read nothing, and *every* WAD looked corrupt.

Cause: the game defines its own `strlcpy`/`strlcat` in `string.c` with a **wrong return
value**. Linked as explicit objects, they *override the libc ones* — and VitaSDK's newlib
calls `strlcpy` inside `__realpath()`, which runs on **every single `open()`**. Every path got
truncated to `ux0:/`, which is a directory, so newlib opened it with `sceIoDopen` and happily
returned a "valid" file descriptor.

**Fix**: define `SRB2_HAVE_STRLCPY` for `__vita__` so the game uses the libc versions.
*This is a generic trap for any Vita homebrew that ships its own `strlcpy`.*

Related: `fopen(..., "a")` (append) **does not create the file** on this libc. When file I/O
looks wrong, drop to the raw `sceIo*` API — it never lies.

### Rendering: the 2019 vitaGL, and moving off it

> 🧭 **This branch (`vitagl-modern`) builds against current vitaGL** — the move described below as
> "the obvious next step" **is done here.** Most of the workarounds in this section are therefore
> **gone on this branch** and only apply to the [`master`](../../tree/master) build (2019 vitaGL,
> commit `694b387`). They are kept here as the history of what the engine assumes about OpenGL,
> and because `master` still relies on them.

> ⚠️ **A lesson worth keeping.** The 2019 vitaGL is **more than 1100 commits behind** upstream, and
> several limitations worked around below **were fixed years ago** — `glRotatef` with negative
> axes, the fixed 128‑buffer pool, the hardcoded display‑buffer count. We first reported these
> upstream as if they were current bugs, **without reading the current code**; they were rightly
> rejected. The full accounting is in
> [`upstream/vitagl/`](upstream/vitagl/CORRECTION-we-were-looking-at-a-2019-snapshot.md).

What the *2019* vitaGL needed — i.e. what the engine assumes about OpenGL that it did not provide
(**all handled natively by the current vitaGL this branch uses**):

- It accepts **float vertex attributes only**. MD3 models load as shorts (`useFloat=false`) →
  converted short→float on every draw. Colour arrays of `GL_UNSIGNED_BYTE` are ignored, which is
  why the sky gradient is flat white.
- `glDrawElements` indices are **always cast to `uint16_t`** → index type forced to
  `GL_UNSIGNED_SHORT`, and the batcher flushes before 65536 vertices.
- Only **128 buffer objects** exist *(fixed upstream since)*. The engine created one VBO *per
  model frame* and silently exhausted them. Models draw from client arrays instead.
- `glCopyTexImage2D` is a **no-op** → the engine's "final screen texture" pass painted an empty
  texture over the frame (black screen). Skipped; wipes/fades are disabled as a result.
- Texture 0 cannot be sampled → any untextured polygon was invisible (menu dimming, fills).
  A 1×1 white texture is bound instead.
- `glRotatef` mishandles **negative axes** *(fixed upstream since)* — we pass a positive axis and
  negate the angle.

### The display is not the framebuffer

`vglStopRendering()` only *queues* the frame (`sceGxmDisplayQueueAddEntry`); a system thread
calls `sceDisplaySetFrameBuf` later. Anything that wants to own the screen (like the boot
loading screen) must wait for **`sceGxmDisplayQueueFinish()`** first, or the GPU frame lands
on top of it a moment later.

Never call SDL's message boxes: their Vita backend assumes an SDL GXM renderer that does not
exist under vitaGL (`gxm_swap_for_common_dialog` → data abort). `I_Error` writes to
`ux0:data/srb2kart/error.txt` instead — **read that file first when the game "just crashes".**

### Networking

Modern VitaSDK *does* have `netdb.h`, `arpa/inet.h`, and a real BSD socket layer in `libc.a`
(over sceNet). The 2019 port's hand-rolled `struct addrinfo` is obsolete. But:

- There is **no `sys/ioctl.h`** → `ioctl(s, FIONBIO)` is silently skipped (it sits behind
  `#ifdef FIONBIO`) and the game would block forever in `recvfrom`. Use
  **`setsockopt(SOL_SOCKET, SO_NONBLOCK)`**.
- sceNet is **IPv4 only** (`NO_IPV6=1`).
- `SOCK_GetAddr` validates each resolved address with `sendto(sock, **NULL**, 0, ...)` — an
  empty datagram from a NULL pointer. BSD sockets return 0; **sceNet rejects it**, so *every*
  address was discarded, no node was ever created, and the client sat on
  *"Connecting to server"* forever. That test is skipped on Vita.

### HTTPS / master server

`libcurl` is available via `vdpm` and is built against **OpenSSL**, so it never touches
`SceSsl` — iTLS-Enso is irrelevant here. But it has **no CA store**, so the master server's
certificate is rejected. The Mozilla CA bundle ships in `ux0:data/srb2kart/cacert.pem` and
`CURLOPT_CAINFO` points at it.

### Performance work

Everything below was **measured on the console**, not guessed. Every guess was wrong.

1. **The game is GPU-bound, not CPU-bound.** Frame presentation cost **14.5 ms** of a 26 ms
   frame. Cutting CPU work (models, draw distance) barely moved the needle. Cutting *pixels*
   did: 640×368 is 45 % of 960×544 and presentation fell to 7.4 ms — exactly proportional.
   The display controller upscales in hardware, for free.
2. **CPU and GPU did not overlap.** vitaGL shipped with `DISPLAY_BUFFER_COUNT 2`, so
   `displayQueueMaxPendingCount` was 1 and the CPU blocked waiting for the GPU every frame —
   the two costs *added up*. Triple buffering lets the CPU run a frame ahead.
3. **The stalls were the memory card, not the renderer.** Frame spikes of up to **1 second**
   were traced to `TryRunTics`, then to audio: each music track is read from `music.kart`
   (105 MB) *the moment it starts playing* — 250–740 ms of disk I/O, mid-race, mid-frame.
   Decoding it costs almost nothing by comparison (2–130 ms).

   **Audio preloading** (this is why booting is slow): at startup the port reads **all sound
   lumps** and **all non-map music lumps** into RAM — *raw, without converting them*. The
   conversion (`ds2chunk`) inflates 8-bit mono into 16-bit stereo 44 kHz (~16×), which is
   exactly why the engine's own `precachesound` is off by default and would blow the heap.
   Reading the raw lumps costs only ~23 MB and removes every disk stall.

   The map's own track (up to 3 MB) is preloaded when the level loads. Music lumps are named
   `O_KMAP*` for map tracks and anything else for the fixed jingles — that naming rule is what
   makes the preload generic instead of a hand-written list (a hand-written list missed
   `kstart` and the finish jingles).

4. **The last stalls were sound *decoding*, not sound *loading*.** After the audio preload,
   frame spikes of 60–290 ms remained, and the profiler put them squarely inside `P_Ticker` —
   with **zero disk I/O, zero texture work, zero GPU time**. Kart's sound effects are not
   DoomSounds: they are **OGG**, and SDL_mixer decodes them *in full, on first play* — inside
   `S_StartSound`, inside the game tic. Preloading the raw lumps had removed the *read*, not
   the *decompression*.

   Decoding everything up front is impossible: **926 unique sounds = 224 MB** of PCM (the heap
   is 256 MB — one attempt died in `vorbis_synthesis`). And no static rule picks the right
   subset: the engine's "fixed" sounds alone are 215 MB, character voices only 8 MB. But **a
   race only ever touches ~80 of them.**

   So the game keeps its own list: every sound it actually decodes is recorded in
   `ux0:data/srb2kart/sfxwarm.txt`, and that list is decoded **at startup**. It is
   self-correcting (a new item, character or add-on adds itself), bounded (80 MB cap), and
   needs no hand-written list — which is the point: *every* hand-written list in this port
   turned out to be wrong. A pre-populated list ships with the game data.

   ⚠️ **`P_SetupLevel` calls `S_ClearSfx()`**, which frees every decoded sound on each level
   load. Precaching anywhere else is silently thrown away before the starting line. Sounds
   decoded by SDL_mixer live in SDL's heap, not the zone, so they do **not** need freeing
   before `Z_FreeTags` — only the `ds2chunk`/`Z_Malloc` ones do (`Mix_Chunk->allocated` tells
   them apart). Keeping them alive is what makes restarting a race instant.

   ⚠️ Decoding is blocking, and SDL_mixer decodes the **music** on the audio thread: a long
   decode on the main thread starves it and the background music stutters. During a level load
   the music is therefore stopped; during a title-screen demo — where it must keep playing and
   nobody is waiting — the decoder yields and runs at lower priority instead.

**Result:** 26.2 ms → **15.0 ms** average frame; in-race disk I/O **1613 ms → 0 ms**; in-race
sound decoding **0 ms**; frame spikes **19 per race → 0**. Boot ~3 min → **~75 s**; race load
**11.1 s → ~2.7 s**; restarting a race **~2 s**.

Two upstream bugs fell out of this and are written up in `upstream/`: the credits screen
dereferences missing font glyphs (any credit line with a `/` crashes the game), and the OpenGL
batching arrays grow with **unchecked `malloc`s** — so heap exhaustion crashes *in the renderer*,
which is a wonderful way to spend an afternoon chasing a phantom GPU bug.

### Memory

The app runs with **extended memory** (`ATTRIBUTE2=12` in `param.sfo`, ~365 MB instead of
256 MB) and a 256 MB newlib heap. This *requires installing the VPK* — pushing a `param.sfo`
over FTP does not enable it, and the game will not boot.

Watch out: OpenGL caches patches as **RGBA (4× the software 8-bit)**, and `malloc()` failures
are not checked everywhere in the engine (a failed demo-buffer allocation crashed inside
`G_SaveDemo`, writing to address 16). Precache carefully.

---

## License

**GPL-2.0**, like SRB2Kart itself. See [`LICENSE`](LICENSE).

Game assets are **not** included and are not covered by this license.
