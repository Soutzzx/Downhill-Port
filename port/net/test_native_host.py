"""Minimal ISO-free smoke test for the embedded Player Hosted WebSocket relay.
Runs on Windows from GitHub Actions or on a developer PC. Standard library only.
"""
import base64
import json
import os
import socket
import struct
import subprocess
import sys
import time

HOST = "127.0.0.1"
PORT = 8787


def read_exact(sock, count):
    out = b""
    while len(out) < count:
        chunk = sock.recv(count - len(out))
        if not chunk:
            raise AssertionError("server closed unexpectedly")
        out += chunk
    return out


def frame(sock):
    first = read_exact(sock, 2)
    kind = first[0] & 15
    size = first[1] & 127
    assert not first[1] & 128, "server WebSocket frames must not be masked"
    if size == 126:
        size = struct.unpack(">H", read_exact(sock, 2))[0]
    elif size == 127:
        size = struct.unpack(">Q", read_exact(sock, 8))[0]
    return kind, read_exact(sock, size)


def send_frame(sock, opcode, payload):
    mask = os.urandom(4)
    assert len(payload) < 4096
    if len(payload) < 126:
        head = bytes((0x80 | opcode, 0x80 | len(payload)))
    else:
        head = bytes((0x80 | opcode, 0xFE)) + struct.pack(">H", len(payload))
    sock.sendall(head + mask + bytes(v ^ mask[i % 4] for i, v in enumerate(payload)))


def send_json(sock, obj):
    send_frame(sock, 1, json.dumps(obj, separators=(",", ":")).encode())


def recv_type(sock, expected):
    until = time.monotonic() + 5
    while time.monotonic() < until:
        kind, payload = frame(sock)
        if kind == 9:
            send_frame(sock, 10, payload)
            continue
        if kind != 1:
            continue
        obj = json.loads(payload)
        if obj.get("type") == "ping":
            send_json(sock, {"type": "pong", "s": obj["s"]})
            continue
        if obj.get("type") == "error":
            raise AssertionError(obj)
        if obj.get("type") == expected:
            return obj
    raise AssertionError(f"missing {expected}")


def ws(path):
    s = socket.create_connection((HOST, PORT), 5)
    s.settimeout(5)
    key = base64.b64encode(os.urandom(16)).decode()
    req = (
        f"GET {path} HTTP/1.1\r\nHost: {HOST}:{PORT}\r\n"
        f"Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\n\r\n"
    )
    s.sendall(req.encode())
    result = b""
    while b"\r\n\r\n" not in result:
        result += s.recv(1024)
        assert len(result) < 4096
    assert result.startswith(b"HTTP/1.1 101"), result
    return s


def hello(sock, name, version="test-build"):
    send_json(sock, {
        "type": "hello", "protocol": 1, "version": version,
        "clientVersion": "0.2.7", "isoSha256": "a" * 64, "name": name
    })
    return recv_type(sock, "welcome")


def run():
    executable = sys.argv[1] if len(sys.argv) > 1 else "native_host_test.exe"
    proc = subprocess.Popen([executable], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    host = guest = None
    others = []
    try:
        assert proc.stdout.readline().strip() == "NATIVE_HOST_READY"
        with socket.create_connection((HOST, PORT), 3) as s:
            s.sendall(b"GET /v1/health HTTP/1.1\r\nHost: localhost\r\n\r\n")
            assert b'"ok":true' in s.recv(2048)

        host = ws("/v1/rooms/new")
        h = hello(host, "HOST")
        assert h["host"] and h["code"].startswith("H"), h
        code = h["code"]
        assert len(code) == 6
        room = recv_type(host, "room")
        assert len(room["members"]) == 1
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as discovery:
            discovery.settimeout(0.2)
            discovery.sendto(b"DHD-LAN-1", (HOST, 8788))
            try:
                discovery.recvfrom(180)
                raise AssertionError("private room appeared in LAN discovery")
            except socket.timeout:
                pass

        # A second EXE would resolve the same code through localhost.
        with socket.create_connection((HOST, PORT), 3) as s:
            s.sendall(f"GET /v1/player-host/{code} HTTP/1.1\r\nHost: localhost\r\n\r\n".encode())
            response = s.recv(2048)
            assert b"ws://127.0.0.1:8787" in response

        # Explicitly raise the room from its default 8 (and verify it).
        send_json(host, {"type": "settings", "maxPlayers": 8, "track": 2, "laps": 0})
        guest = ws("/v1/rooms/" + code)
        g = hello(guest, "FRIEND")
        assert not g["host"] and g["code"] == code

        # Six more guests: one host plus seven friends, without Node or ISO.
        for i in range(6):
            sock = ws("/v1/rooms/" + code)
            result = hello(sock, f"RIDER{i+3}")
            assert result["memberId"] >= 2
            others.append(sock)
        while True:
            members = recv_type(host, "room")["members"]
            if len(members) == 8:
                break

        send_json(host, {"type": "ready", "ready": True})
        for sock in [guest, *others]:
            send_json(sock, {"type": "ready", "ready": True})
        while True:
            snapshot = recv_type(host, "room")
            if len(snapshot["members"]) == 8 and all(x["ready"] for x in snapshot["members"]):
                break
        send_json(host, {"type": "start"})
        a = recv_type(host, "start")
        b = recv_type(guest, "start")
        assert len(a["players"]) == len(b["players"]) == 8
        assert a["yourSlot"] == 0 and b["yourSlot"] == 1
        for i, sock in enumerate(others):
            started = recv_type(sock, "start")
            assert len(started["players"]) == 8
            assert started["yourSlot"] == i + 2

        # A state-sync frame from the guest must arrive at the host with slot 1.
        frame_data = bytes((5, 0)) + b"DATA"
        send_frame(guest, 2, frame_data)
        while True:
            opcode, payload = frame(host)
            if opcode == 2 and payload[0] == 5:
                assert payload[1] == 1
                break
        print("Native 8-player room, join code, Ready/Start and binary relay: PASS")
    finally:
        for sock in [host, guest, *others]:
            if sock:
                sock.close()
        proc.terminate()
        try:
            proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


def public_discovery(executable):
    proc = subprocess.Popen([executable, "--public"], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    host = None
    try:
        assert proc.stdout.readline().strip() == "NATIVE_HOST_READY"
        host = ws("/v1/rooms/new")
        code = hello(host, "HOST")["code"]
        assert recv_type(host, "room")["public"]
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as discovery:
            discovery.settimeout(2)
            discovery.sendto(b"DHD-LAN-1", (HOST, 8788))
            answer, sender = discovery.recvfrom(180)
            assert answer.startswith(b"DHD-LAN-1 " + code.encode() + b" 1 8 "), answer
            assert sender[0] == HOST
        print("Public LAN discovery and private-room privacy: PASS")
    finally:
        if host:
            host.close()
        proc.terminate()
        try:
            proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    run()
    public_discovery(sys.argv[1] if len(sys.argv) > 1 else "native_host_test.exe")
