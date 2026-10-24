"""Source guards for local public/private room routing (ISO-free)."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
menu = (ROOT / "port/hle/online_menu.c").read_text(encoding="utf-8")
net = (ROOT / "port/net/net.c").read_text(encoding="utf-8")
native = (ROOT / "port/net/native_host.c").read_text(encoding="utf-8")
host = (ROOT / "port/hle/host.c").read_text(encoding="utf-8")

assert 'Host A Public Race (Your PC)' in menu
assert 'Host A Private Race (Your PC)' in menu
assert 'enter_room(1, NULL, NULL)' in menu
assert 'player_host_set_public(public_room)' in menu
assert 'net_list_lobbies(list, LIST_MAX)' in menu
assert 'enter_room(0, list[row].code, list[row].address)' in menu
assert 'net_find_player_host(code,direct,sizeof direct)' in menu
assert 'Host code not online' not in menu
assert 'DHD-LAN-1' in native and 'DHD-LAN-1' in net
assert 'public_room&&member_count&&!racing' in native
assert 'DHD_HOST_ADDRESS' in net
assert 'DHD_HOST_DIRECTORY' not in native + net
assert 'dhd-online.downhill-revived.workers.dev' not in host + native + net + menu
print("Local public/private host routing: PASS")
