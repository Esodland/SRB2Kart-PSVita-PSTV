# `K_CheckSpectateStatus`: re-entry cooldown reads the wrong player

**File:** `src/k_kart.c`, in `K_CheckSpectateStatus()`, the final "de-spectate everyone in the
list" loop.

```c
for (i = 0; i < numjoiners; i++)
{
    // Hit the in-game player cap while adding people? Get out of here
    if (cv_ingamecap.value > 0 && numingame >= cv_ingamecap.value)
        break;

    // This person has their reentry cooldown active.
    if (players[i].spectatorreentry > 0 && numingame > 0)   // <-- players[i]
        continue;

    P_SpectatorJoinGame(&players[respawnlist[i]]);          // <-- players[respawnlist[i]]
    numingame++;
}
```

`i` indexes **`respawnlist`** (the list of players who want to join), not the `players` array —
as the very next line makes clear. So the cooldown check consults **an unrelated player**.

## Symptom

Two players, both spectating, both pressing Item to rejoin. The first one joins. On the next
tic the second is the only candidate, so `i == 0`, and the code reads
`players[0].spectatorreentry` — the *first* player's cooldown, which is still running because
they just spectated. The second player is refused for as long as somebody else's cooldown
lasts, and the HUD sits on **"Cancel Join"** doing nothing.

Reproduced consistently on a 2-player LAN game.

## Fix

```c
    if (players[respawnlist[i]].spectatorreentry > 0 && numingame > 0)
        continue;
```

## ⚠️ Why we did NOT ship this fix

We applied it, and it **desynced every online game** the moment a spectator tried to rejoin.

`K_CheckSpectateStatus()` is called from `G_Ticker()` on **every peer** — it is part of the
deterministic lockstep simulation. Fixing the bug on the client makes it diverge from vanilla
servers, and you get *synch failure* exactly on the code path you "fixed".

So the bug is **harmless as long as everybody has it**, and cannot be fixed unilaterally. It
has to land upstream, in a release, for everyone at once. We reverted our fix and left a
comment in the source explaining why the obviously-wrong line must stay.

Filing it here so it can be fixed properly, on the right side of the wire.
