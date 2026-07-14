# `mempool_alloc()` returns NULL on exhaustion, and callers do not check

**Symptom:** a data abort **inside vitaGL** (`mem_utils.c`, `mempool_alloc`) when GPU
memory runs out — with a call stack that points at the renderer, while the actual
problem is simply "no memory left".

```c
void *mempool_alloc(size_t size, vglMemType type) {
	void *res = NULL;
	if (size <= tm_free[type])
		res = heap_alloc(type, size, MEM_ALIGNMENT);
	return res;          // <-- NULL when the pool is full
}
```

Returning NULL is correct. The problem is that **callers treat the result as always
valid**, so an exhausted pool turns into memory corruption rather than a clean
"texture could not be uploaded".

## How we hit it

Porting SRB2Kart to the PS Vita. The game loads community add-ons; every character
sprite becomes an RGBA texture. On a server distributing ~274 MB of add-ons, the GPU
pool fills up and the app dies with a data abort *inside vitaGL* — which sends you
looking for a renderer bug that does not exist.

This is the second instance of the same pattern we ran into (see also
`0001-glTexImage2D-do-not-upload-with-a-NULL-write-callback.patch`, and, in the game
itself, unchecked `malloc`s in the OpenGL batching code). The failure mode is always
the same: **a legitimate out-of-memory condition is reported as a crash somewhere
unrelated**, and the developer spends hours in the wrong file.

## Suggested fix

Two things, in order of usefulness:

1. **Check the result** at the call sites (texture upload, buffer creation, …) and
   fail gracefully — set `GL_OUT_OF_MEMORY`, skip the upload, keep running. A missing
   texture is survivable; a data abort is not.
2. Optionally, offer a way for the application to **query free GPU memory** before an
   upload (`vglMemFree()` exists — documenting it near the texture APIs would help),
   so an app that knows it is memory-constrained can degrade on its own terms.

We are not sending a patch for this one because the right behaviour at each call site
is a design decision that belongs to the maintainer, not to us.
