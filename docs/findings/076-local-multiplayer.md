# Local multiplayer with public and private rooms (2026-10-24)

The in-game ONLINE menu now creates rooms only on the embedded native host.
Public rooms respond to a `DHD-LAN-1` UDP probe on port 8788 and appear in
Public Races. The response includes the H-code, player count, track and build
version; the guest connects directly to the responding host on TCP 8787. The
same probe can be sent to the `host_address` IPv4 setting when VPN broadcast
does not reach the host. The list needs no code entry.

Private rooms do not respond to discovery. A guest supplies the host's LAN or
Radmin IP in Host IP and enters its six-character H-code. Both room types use
the same local WebSocket relay and the same version/ISO checks. Two instances
on one PC can resolve a code at `127.0.0.1` without setting an address.

The native host no longer registers H-codes with a Cloudflare directory.
The in-game menu no longer creates Cloudflare rooms or lists its public races;
the official address is removed from the executable's default settings. UPnP
router port mapping is excluded from the link because LAN/Radmin needs no
router exposure. The old online server and its development tools remain as
source for future work, but the player flow does not use them.

Follow-up audit: the headless `DHD_NET_CREATE`/`DHD_NET_JOIN` route now starts
the embedded host or resolves the code at `DHD_HOST_ADDRESS`; `net_init` no
longer reads `DHD_NET_SERVER` implicitly. The race harness uses localhost, the
release packager rejects every `workers.dev` address, and the native-host CI
workflow no longer watches the old Worker source. The historic `online/server/`
source remains in the repository, outside the executable and release ZIP.

Windows may send limited broadcasts on only one adapter. When a VPN does not
carry discovery packets, entering the host's Radmin IPv4 address enables
directed discovery without typing the room code. Firewall rules must allow
TCP 8787 and, for public discovery, UDP 8788 on the host.

Sources: Microsoft Winsock [SO_BROADCAST](https://learn.microsoft.com/en-us/windows/win32/winsock/socket-options-and-ioctls-2)
and [sendto broadcast behavior](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-sendto).

Validation: source and `git diff --check` review completed. This environment
has neither a working Python runtime nor LLVM/compiler, so clean build,
native socket smoke test and Defender scan of a new binary are still pending.
