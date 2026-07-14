# Sound effects are decoded inside the game tic (frame spikes on slow hardware)

**Affects:** every platform; only *visible* where CPU is slow (consoles, handhelds,
low-end PCs). Reported from the PS Vita / PS TV port, but nothing here is Vita-specific.

## What happens

`S_StartSoundAtVolume` (s_sound.c) loads a sound the first time it is played:

```c
// cache data if necessary
if (!sfx->data)
    sfx->data = I_GetSfx(sfx);
```

SRB2Kart's sound effects are **not DoomSounds** — they are **OGG**. `ds2chunk()`
returns NULL for them, and `I_GetSfx` falls through to:

```c
// Try to load it as a WAVE or OGG using Mixer.
rw = SDL_RWFromMem(lump, sfx->length);
chunk = Mix_LoadWAV_RW(rw, 1);
```

`Mix_LoadWAV_RW` **fully decodes** the Vorbis stream, right there — which means
inside `S_StartSound`, inside `P_Ticker`, inside the game tic.

## Why it matters

On a 444 MHz ARM (PS Vita) we measured, with a frame profiler:

| Event | Frame time | of which OGG decode |
|---|---|---|
| First bump (`s3k49`) | 55 ms | 32 ms |
| First lap sound (`s3k68`) | 198 ms | **164 ms** |
| Finish line (`NWIN`) | 289 ms | **243 ms** |
| Worst single sound (`s3kdal`) | — | **2404 ms** |

19 frames per race blew the 16.6 ms budget purely from sound decoding. Disk I/O,
texture uploads and GPU time were all **zero** on those frames — the whole cost is
the Vorbis decode. It is invisible on a modern PC, and brutal anywhere else: the
stutter lands exactly on the most dramatic moments of a race (first item, last lap,
finish line), because that is when a sound is heard for the first time.

## Notes for anyone attempting a fix

Decoding everything up front is **not** a viable answer: we measured **926 unique
sounds = 224 MB** of decoded PCM (16-bit stereo 44.1 kHz). Note also that
`S_GetSfxLumpNum` falls back to `dsthok` for every sfx without a lump of its own,
so a naive "decode all NUMSFX entries" loop decodes the same sound ~9800 times and
reports absurd totals — that trap cost us an afternoon.

Two other things worth knowing:

- **`P_SetupLevel` calls `S_ClearSfx()`**, which frees *every* decoded sound on each
  level load. Any precaching done outside that path is silently thrown away before
  the race starts. (Sounds decoded by SDL_mixer live in SDL's heap, not the zone, so
  they do *not* need to be freed before `Z_FreeTags` — only the `ds2chunk`/`Z_Malloc`
  ones do. `Mix_Chunk->allocated` distinguishes them, and `I_FreeSfx` already relies
  on this.)
- Decoding is CPU-bound and blocking, and SDL_mixer decodes the **music** on the audio
  thread. Any long decode on the main thread starves it and makes the background music
  stutter.

What we ended up doing in the port (for reference, not a suggestion for upstream):
the game records every sound it actually had to decode into a small list, and decodes
that list once at startup. A race touches ~80 sounds out of 926, so the working set is
tiny; keeping them alive across level loads makes restarting a race instant.

A cleaner upstream fix would probably be to decode sound effects off the game thread,
or to ship them in a format that does not need decoding.
