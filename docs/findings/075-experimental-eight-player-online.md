# Experimental eight-player online rooms

This branch expands the existing native PC port's state-synchronized online mode from four toward eight human riders. Each client continues to run its own single-player camera and projects remote players onto the game's existing AI rider slots.

## What changed

- `port/hle/online.c`: room capacity follows `NET_MAX_SLOTS` (8); race setup no longer truncates remote participants to four; per-player GO timestamps index all eight players without aliasing indices 4–7.
- `port/hle/online_menu.c`: room creator requests eight seats; the lobby displays all eight members. The menu fits 12 rows (four controls + eight members + ready/start/leave) within its 16-row storage.
- `port/online/Online.ps1`: scripted friends launcher can start when 2–8 players join.
- `online/server/test/room.test.ts`: regression test for eight members, refusal of ninth, and eight-racer start events.
- `Build-Downhill.ps1`: local-only Windows build helper. It looks for a USA ISO in the gitignored `iso/` folder and extracts `SCUS_971.77` if necessary. It verifies the retail executable hash and invokes the existing clean builder.

## What was already present

- The network structures and server already declare eight slots (`NET_MAX_SLOTS`, `MAX_MEMBERS`).
- The original racing engine has ten rider slots; the state-sync architecture runs one human rider locally and maps remote human riders onto other slots.
- A separate physical camera per PC is already a property of the state-sync mode, not a new split-screen renderer.

## Still needs verification

**This is not yet a validated 8-player release.** The branch was analyzed and edited remotely; no executable has been built here because the retail `SCUS_971.77` is needed locally. The server test was added but has not run in the current environment.

1. On Windows, check prerequisites in `docs/BUILDING.md` (Git, Python, 7-Zip, LLVM, MSVC x86 libraries, Windows SDK, CMake). From the branch root run `powershell -ExecutionPolicy Bypass -File .\\Build-Downhill.ps1`. The 2 GB ISO stays locally in `iso/`, never on GitHub.
2. Run `cd online/server && npm ci && npm test && npm run typecheck`.
3. Verify two-client native play first, then 4, 6 and 8 distinct Windows machines. Test all players' grid positions, character/bike appearance, collisions, powerups, all finish orders, disconnections and rematches.
4. Use a private/self-hosted relay configured in `dhd.ini` while testing. The stock public server may enforce the released client version; do not assume it accepts this experimental build.
5. Confirm performance and bandwidth as each host broadcasts state snapshots to seven other PCs. No claim of production-ready latency or crash-free behavior is made.

## Potential limitations

- Two separate UI and scripted launchers expose room sizes differently; menu-created rooms presently choose eight, whereas the scripted launcher allows different start thresholds.
- The original auto-updater should not overwrite experimental builds during testing (`auto_update = off` in `dhd.ini`).
- The default relay currently has its own version policy; use an authorized test deployment rather than uploading or redistributing game data.
- Keep `src/` matching the retail decompilation. No retail game data belongs in git.
