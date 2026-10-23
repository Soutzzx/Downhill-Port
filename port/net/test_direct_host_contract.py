"""ISO-free guards for Player Hosted's separate transport choices.

The native WebSocket integration is tested in test_native_host.py; the retail
menus need the ISO and cannot be exercised in CI. These source assertions
protect against reintroducing the accidental Cloudflare-host fallback.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
menu = (ROOT / "port/hle/online_menu.c").read_text(encoding="utf-8")
native = (ROOT / "port/net/native_host.c").read_text(encoding="utf-8")
worker = (ROOT / "online/server/src/index.ts").read_text(encoding="utf-8")

boot = menu.split("case HOST_BOOT: {", 1)[1].split("case ROOM: {", 1)[0]
join = menu.split("static void enter_room(", 1)[1].split("static void page_build(", 1)[0]
assert "enter_room(1, NULL)" in boot, "Host must create native room immediately"
assert "hosted_mode = 0" not in boot, "Host must never switch to cloud on directory failure"
assert 'hosted_mode && create ? "ws://127.0.0.1:8787" : server()' in join
assert "host_code_unresolved" in join and "return;" in join, "Unknown H-codes must not fall back to cloud"
assert "Host code not online" in join
assert "player_host_directory_state() == -2" in menu, "Report rejected directory registrations"
assert "status==200?1:-2" in native, "Check directory HTTP response"
assert "TICK_MS = 10" in native and "TCP_NODELAY" in native
assert "AF_INET6" in native and "IPV6_V6ONLY" in native
assert 'random.startsWith("H")' in worker, "Cloudflare must reserve H prefix for native host"
print("Native hosting and Cloudflare separation contract: PASS")
