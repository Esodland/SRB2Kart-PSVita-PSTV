# Building the `vitagl-modern` branch

This branch builds against **current vitaGL** instead of the 2019 snapshot the
`master` branch uses. It is more work to set up, and it adds a runtime requirement
for the player, but it removes a pile of workarounds and runs well.

**If you just want a working build, use `master`.** This branch is the forward-looking
one.

## What it needs, and why

| Piece | Why |
|---|---|
| **vitaGL** (current `master`) | The whole point. Presentation is `vglSwapBuffers`, triple buffering is default, `glCopyTexImage2D` exists, indices can be 32-bit — none of which the 2019 version had. |
| **vitaShaRK** (`vdpm vitashark`) | Modern vitaGL compiles its shaders at runtime, even for the fixed-function pipeline. |
| **SceShaccCgExt** (github.com/bythos14/SceShaccCgExt) | vitaShaRK depends on it; the VitaSDK does not ship it. |
| **`libshacccg.suprx` on the console** | The actual shader compiler (from Sony's PSM runtime). Put it in `ur0:data/`. Without it, modern vitaGL cannot draw anything. This is the player-facing cost of this branch. A legal, hash-verified copy is at github.com/AnimMouse/SceShaccCg. |

## Build steps

```bash
# 1. vitaShaRK (dependency of the runtime shader compiler)
vdpm vitashark

# 2. SceShaccCgExt (not in the SDK)
git clone https://github.com/bythos14/SceShaccCgExt
cd SceShaccCgExt
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_C_FLAGS="-std=gnu11"
cmake --build build          # produces build/libSceShaccCgExt.a
cd ..

# 3. current vitaGL, WITHOUT its splash screen
#    (NO_SPLASHSCREEN is an official option; vitaGL is still credited in-game
#     and on the boot screen. It just should not interrupt our own loading screen.)
git clone https://github.com/Rinnegatamante/vitaGL
cd vitaGL
make HAVE_SBRK=1 NO_SPLASHSCREEN=1
cd ..

# 4. point the port at those two, then build as usual
#    (VITAGL_DIR and SHACCEXT_DIR in src/sdl/SRB2Vita/Makefile.cfg — override on the
#     command line if your paths differ)
cd Kart-Public/src
make PSP2=1 VITAGL_DIR=/path/to/vitaGL SHACCEXT_DIR=/path/to/SceShaccCgExt/build -j8
```

## What this branch changed vs `master`

- Presentation: the old `vglStartRendering`/`vglStopRendering` pair (gone from vitaGL)
  is now `vglSwapBuffers`; the on-screen keyboard's four-call composite loop collapses
  to `vglSwapBuffers(GL_TRUE)`.
- `G_GhostTicker` no longer crashes on an unreadable ghost (this fix is on `master` too).

## What was tried and reverted

- **Fades/wipes.** `glCopyTexImage2D` exists in modern vitaGL, so they *can* be
  re-enabled — but they rendered as a white flash, they overwrote our nicer custom
  race-loading screen, and activating the copy macros globally revived a per-frame
  2048×2048 framebuffer copy (`MakeScreenTexture`) that dropped the game to ~7 FPS.
  Not worth it; left disabled, same as `master`.
- **32-bit draw indices.** Modern vitaGL accepts `GL_UNSIGNED_INT`, but on this GPU
  16-bit indices are native and 32-bit take a slow path — measured ~7 FPS in a race.
  Kept 16-bit.
