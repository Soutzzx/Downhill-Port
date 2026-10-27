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

The player confirmed customization works but reported that the following SELECT LEVEL
popup remains interactive and can start a race alone. `func_00139BF8` owns that modal;
`func_001F8920` is its level-screen update. The port now ends the modal at that update,
before accepting a level, and the existing post-modal hook returns to RACE ROOM with the
selected ride. This path needs an in-game retest.

The player also reported that the cutscene wait message did not appear. The first
implementation used a child control inside the flip-model D3D window. It has been
replaced with an owned, topmost, non-activating window painted by the window thread,
so it can remain visible while the game thread waits. The exact wait phase and
fullscreen behavior still need a two-PC retest.

The player confirmed the wait window appears, and the level screen is prevented from
starting a solo race, but the modal exit leaves an awkward mountain camera and the
rider carousel looks locked. At the player's request the modal customization and
level intercepts are removed, restoring the earlier menu-18 handoff. These edits
remain in commits d7c748c and 97aecaf if customization is revisited. The unlock
routine now clears the carousel's per-rider transition latches (D_004C6780), which
could still show the old locked state after the selectable flags were opened.
The wait window now centers a larger outlined, italic font over a transparent
background. It approximates the retail font because the game renderer is blocked
during this wait; the separate Win32 window cannot use its GS text packets.

The next player test showed the final italic `S` clipped and public rooms absent
from another Radmin member's list although code joining works. The wait label is
shorter and its measured rectangle has horizontal padding. Discovery now parses
the fixed four reply fields and then takes the complete remaining version string
without `%n`. It also sends directed IPv4 subnet broadcasts on each active adapter,
since limited broadcast can follow only one Windows route. The explicit Host IP
probe remains. Network references: Microsoft [GetAdaptersAddresses](https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses),
[OnLinkPrefixLength](https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_adapter_unicast_address_lh),
and [SO_BROADCAST](https://learn.microsoft.com/en-us/windows/desktop/WinSock/socket-options-and-ioctls-2).
The new discovery behavior still needs a two-PC Radmin retest.

Sources consulted: the local original shell and online hooks described in finding 065,
the LAN host/client protocol in `port/net/`, and the [PCSX2 source repository](https://github.com/PCSX2/pcsx2)
as a reference for PS2 movie behavior. No third-party code was copied. Validation here:
`git diff --check` passes. LLVM and a working Python installation were not available,
so no compiled or live two-PC claim is made.
