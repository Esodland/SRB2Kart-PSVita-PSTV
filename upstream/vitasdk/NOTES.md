# Notes for VitaSDK

Two things we hit porting a large C codebase (SRB2Kart) that cost us a lot of time and are, we
think, worth either fixing or documenting.

---

## 1. `fopen(path, "a")` does not create the file

Append mode on a **non-existent** file silently does nothing useful: no file is created, and
subsequent writes are lost. (`"w"` works.)

We lost an evening to this: a profiler wrote its results with `fopen(..., "a")`, the code ran
perfectly, computed everything correctly — and threw the results away every time. The
workaround is to drop to the raw API:

```c
SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
```

Either newlib's `_open_r` mapping should pass the create flag for `"a"`, or this deserves a
prominent line in the docs.

---

## 2. `strlcpy` / `strlcat` are used by `__realpath()` — and can be overridden by the app

This one is nastier, because it corrupts **all file I/O** in a way that looks like WAD/asset
corruption.

Many game engines (SRB2, and any Doom derivative) ship their own `strlcpy`/`strlcat` in a
`string.c`. Linked as **explicit objects** they take precedence over libc's. If the app's
version has a different return-value convention (SRB2's returns something other than the
source length), then:

- newlib's `__realpath()` — which VitaSDK calls on **every `open()`** — uses `strlcpy`,
- every path gets truncated to `ux0:/`,
- `ux0:/` is a *directory*, so `_open_r` opens it with `sceIoDopen` and returns a **valid fd**,
- `fopen()` therefore "succeeds" and reads nothing.

Result: every single data file appears corrupt, with no error anywhere. It took a raw-`sceIo`
diagnostic harness to find it.

Suggestions:

- Use internal, non-overridable symbols (`__strlcpy` / `_strlcpy`) inside `__realpath()`, or
- make `__realpath()` not depend on them, or
- at minimum, document this loudly — *"do not ship your own `strlcpy` on Vita"* is a rule
  every homebrew porter should know, and none do.

---

## 3. Minor

- `%zu` is not supported by the libc's printf family (prints the literal `zu`).
- There is no `<sys/ioctl.h>`. Code guarded by `#ifdef FIONBIO` (which is *very* common for
  setting sockets non-blocking) silently compiles to nothing, and the app then blocks forever
  in `recvfrom`. `setsockopt(SOL_SOCKET, SO_NONBLOCK)` works — worth documenting next to the
  socket layer.
- `sendto(sock, NULL, 0, ...)` (a zero-length datagram with a NULL buffer) is rejected by
  sceNet although BSD sockets accept it and return 0. Real code does this to probe an address.

---

Thank you for VitaSDK — none of this port would exist without it. These are exactly the kind
of papercuts that are invisible once you know them and lethal when you don't.
