# Defender Wacatac.B!ml triage (2026-10-24)

The user reported that Windows Defender quarantines recent `dhd.exe` builds. The
local Defender Operational event log confirms `Trojan:Win32/Wacatac.B!ml`
(threat ID 2147735505), including the build from commit `324f1d59` and a
developer-folder copy. An explicit custom-file scan of the clean-tree link
output also found one threat and quarantined that file. Its SHA-256 before
quarantine was `4f4a7fcfcf6f3d305ab97eede68ca228db4ab7f248f767fc9200cb8ae5d9b88a`;
Defender intelligence was 1.461.183.0, engine 1.1.26090.9. This is a real
Defender detection. Whether the file is malicious or falsely detected is not
established by the label alone.

For comparison, an explicit custom-file scan of the earlier clean build
`build/port/clean/a1a6bcbf/dhd.exe` found no threats with the same local
Defender installation. Its manifest names commit `a1a6bcbf`, SHA-256
`822348b21fc33096826411593a4eb203f905e7982eb9ef9719a99ca3946bd2b6`.
That commit already includes the native WebSocket host, direct VPN/IP joining,
and UPnP code. The later `559e7c4` host-IP change and version metadata, update,
release-check, and GUI changes separate it from the detected build. Therefore
the available comparison does not support blaming the native host, UPnP, or
the official Cloudflare server. Removing Cloudflare would change working online
behavior without an identified detection cause.

The Radmin path uses a direct connection to the native host. A room code can
be resolved with the host address configured locally; race packets do not
travel through Cloudflare. Preserve that path during further diagnosis.

Static source review of the post-`a1a6bcbf` delta found no newly added
process injection, persistence, antivirus manipulation, or download-and-run
path. The `559e7c4` change adds local `host_address` input and direct host
lookup. Later changes add Windows VERSIONINFO, tighten update transport to
HTTPS, validate PE metadata, and alter the Python build GUI. The game already
uses `CreateProcessA(... CREATE_SUSPENDED ...)` plus `VirtualAllocEx` to reserve
PS2 memory addresses in a child copy of itself (`port/hle/host.c`), and its
updater can replace its own EXE (`port/hle/update.c`). These are plausible
heuristic signals, but the clean older build has them too. Source review alone
cannot certify the compiled binary or identify the Defender model's feature.

Next steps: build and scan specific intervening commits in a controlled
environment to find the first detected artifact; inspect that delta; submit
the exact detected EXE as a software developer to Microsoft for a verdict.
Do not distribute the detected build or add antivirus exclusions.

Sources: Microsoft [software developer FAQ](https://learn.microsoft.com/en-us/defender-xdr/developer-faq)
and [sample submission guide](https://learn.microsoft.com/en-us/defender-xdr/submission-guide).
