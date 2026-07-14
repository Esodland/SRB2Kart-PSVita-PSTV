From: SRB2Kart PS Vita port
Subject: [PATCH 3/3] glGenBuffers: report exhaustion instead of returning garbage

NOTE: this one is a REPORT, not a mechanical patch -- the right API behaviour is
for upstream to decide (GL_OUT_OF_MEMORY? grow BUFFERS_NUM? both?).

There are only BUFFERS_NUM (128) buffer objects. When they run out, the loop
simply ends: the caller's `res[]` array is left **untouched** and no error is
set. Callers reasonably assume `glGenBuffers` succeeded and use whatever was in
that memory -- typically 0.

`glBindBuffer(0)` then computes `vertex_array_unit = -40960`, and `glBufferData`
happily indexes `gpu_buffers[-40960]`. That silently corrupts memory, and the
crash lands somewhere completely unrelated minutes later.

How we hit it: SRB2's OpenGL renderer creates one VBO *per model frame*. With a
handful of characters on screen it blows past 128 immediately.

Suggested behaviour:
  - zero-fill `res[0..n-1]` up front,
  - if fewer than `n` names are available: set `error = GL_OUT_OF_MEMORY`,
    release the ones already taken, and return.

At minimum, callers must be able to tell that it failed.

---
