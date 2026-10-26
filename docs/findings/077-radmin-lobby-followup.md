# Radmin lobby follow-up (2026-10-26)

The LAN discovery reply contains a build identity with spaces (`dhd-port VERSION HASH`).
`net_list_lobbies` previously parsed it with `%s`, retaining only `dhd-port`. The list
then disabled every room as another version even though joining by code used the full
identity and succeeded. Parse the remainder of the reply after the four fixed fields.

The room also treated another member's identical rider as a conflict and disabled Ready.
The host protocol does not impose that restriction. Each player's ride is carried by
their own network slot, so the lobby now permits duplicate rider choices.

At the countdown, `start_together` already waits for all players to finish the flyby and
load. It now shows a centered message on the game window while peers are missing and
hides it when they arrive or the wait ends. The message also mentions loading because
the existing event does not distinguish a peer watching the flyby from a slow loader.

The player clarified that after rider selection the shop briefly goes black and returns
to the lobby without input. An initial attempt to treat retail menu 16 differently
exposed the NEW / CONTINUE career screen and did not repair the shop. The real cause
is the early interception of menu 18: `func_001F1500` calls `func_00139BF8(0)` there,
which opens the modal bike/outfit shop. The online hook returned before that call.
Menu 16 is again bypassed. Menu 18 now runs retail code, and a build-time patch after
`func_00139BF8` returns records the choice and returns to the lobby. Cancelling the
shop continues through retail's back path. Finding 065 records a two-instance test
where outfit and bike previews changed; this correction still needs a live two-PC retest.

Sources consulted: the local original shell and online hooks described in finding 065,
the LAN host/client protocol in `port/net/`, and the [PCSX2 source repository](https://github.com/PCSX2/pcsx2)
as a reference for PS2 movie behavior. No third-party code was copied. Validation here:
`git diff --check` passes. LLVM and a working Python installation were not available,
so no compiled or live two-PC claim is made.
