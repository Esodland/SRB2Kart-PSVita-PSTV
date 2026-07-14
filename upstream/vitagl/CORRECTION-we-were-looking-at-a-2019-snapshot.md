# Correction: we filed bug reports against a six-year-old copy of vitaGL

This folder used to contain two patches and two bug reports for
[vitaGL](https://github.com/Rinnegatamante/vitaGL). **They were wrong, and they have been
removed.** This note is what stays, because deleting the mistake quietly would be worse than
owning it.

## What happened

This port builds against a **local copy of vitaGL dated November 2019** (commit `694b387`).
Upstream `master` is, at the time of writing, **1167 commits ahead** of it.

We read that 2019 copy, found real problems in it, fixed them locally — and then reported them
upstream as bugs "on master", **without ever fetching the current code**. All of them had been
fixed years ago:

| What we claimed | Reality in current vitaGL |
|---|---|
| `glRotatef` ignores negative axes (only `== 1.0f` is tested) | Fixed. It calls a general `matrix4x4_rotate(matrix, rad, x, y, z)`. |
| `glGenBuffers` silently exhausts a fixed pool of 128 buffers | Gone. Buffers are allocated dynamically (`vglMalloc`). |
| `mempool_alloc()` returns NULL and callers don't check | The function **does not exist any more**; memory handling was rewritten (`vgl_malloc`). |
| `DISPLAY_BUFFER_COUNT` is hardcoded to 2, so CPU and GPU never overlap | It is a runtime variable now (`gxm_display_buffer_count`). |

Three issues were opened on the vitaGL tracker on this basis. They were closed, and the
maintainer's reaction was blunt and entirely deserved: confidently-worded, well-formatted bug
reports that describe code which no longer exists are exactly the kind of AI-generated noise
that wastes maintainers' time. He was right.

## Why this is worth writing down

The rest of this repository is built on one rule, learned the hard way over a long day of
profiling: **measure, don't deduce.** We instrumented the console, timed every phase, symbolised
crash dumps, and refused to put a number in the README without checking it on hardware.

And then we posted public claims about someone else's project **without reading their code.**
The discipline was applied to our own work and abandoned the moment it concerned someone else's.
That is the actual lesson here, and it is a more useful one than any of the bugs.

## What this means for the port

The workarounds in this port (short→float vertex conversion for MD3 models, avoiding VBOs,
uint16 index batching, the `glRotatef` sign trick, the 1×1 white texture for untextured
polygons) target **the 2019 vitaGL**. Several of them are probably unnecessary against current
upstream. **Moving to a recent vitaGL is the right next step**, and it would likely delete a
good chunk of the glue in `src/hardware/r_opengl/r_opengl.c`.

Until that is done and tested on hardware, nothing in this folder should be sent to anyone.

## Still valid

The reports for [SRB2Kart](../srb2kart/) are against the **current** `master` of Kart-Public
(commit `44b4a685`, verified), and the VitaSDK notes were hit on the **current** toolchain.
Those stand. The vitaGL ones did not, and that is on us.
