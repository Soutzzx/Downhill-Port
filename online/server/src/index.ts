// Worker entry: routes lobby-list requests to the LobbyIndex and WebSockets to a Room by code.
import { PROTOCOL, minVersionOf, randomCode } from "./protocol";

export { Room } from "./room";
export { LobbyIndex } from "./lobbies";

const CORS = { "Access-Control-Allow-Origin": "*" };

function json(body: unknown, status = 200): Response {
  return Response.json(body, { status, headers: CORS });
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const path = new URL(request.url).pathname.replace(/\/+$/, "");

    // minVersion: the oldest client release the server lets play (MIN_VERSION, wrangler.jsonc).
    const minVersion = minVersionOf(env.MIN_VERSION);
    if (path === "/v1/health") return json({ ok: true, protocol: PROTOCOL, minVersion });

    /* Player Hosted directory. Hosting is opt-in on the player's PC.
       The directory does NOT proxy game traffic; it only lets a friend
       translate the existing six-character private code to the host address.
       The address comes from Cloudflare's connection metadata, never from
       a JSON-provided address (which could be used for SSRF). */
    const hostCode = path.match(/^\/v1\/player-host\/([23456789ABCDEFGHJKMNPQRSTUVWXYZ]{6})$/);
    if (hostCode && request.method === "GET") {
      const entry = await env.LOBBIES.get(env.LOBBIES.idFromName("global")).resolvePlayerHost(hostCode[1]);
      return entry ? json(entry) : json({ error: "No active player host" }, 404);
    }
    if (path === "/v1/player-host/register" && request.method === "POST") {
      const raw = request.headers.get("CF-Connecting-IP") ?? "";
      const ipv4 = raw.match(/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/);
      const v4 = ipv4?.slice(1).map(Number);
      const privateV4 = v4 && (
        v4.some((x) => x > 255) || v4[0] === 0 || v4[0] === 10 ||
        v4[0] === 127 || v4[0] >= 224 ||
        (v4[0] === 100 && v4[1] >= 64 && v4[1] <= 127) ||
        (v4[0] === 169 && v4[1] === 254) ||
        (v4[0] === 172 && v4[1] >= 16 && v4[1] <= 31) ||
        (v4[0] === 192 && v4[1] === 168) ||
        (v4[0] === 198 && (v4[1] === 18 || v4[1] === 19))
      );
      const ipv6 = /^[0-9a-f:]+$/i.test(raw) && raw.includes(":") &&
        raw.length < 46 && !/^(::1|fe80:|fc|fd)/i.test(raw);
      if ((!v4 || privateV4) && !ipv6)
        return json({ error: "No public host IP (CGNAT or proxy?)" }, 400);
      let payload: { code?: unknown; port?: unknown; token?: unknown };
      try { payload = await request.json() as typeof payload; }
      catch { return json({ error: "Bad JSON" }, 400); }
      const { code, port, token } = payload;
      if (typeof code !== "string" || !/^[23456789ABCDEFGHJKMNPQRSTUVWXYZ]{6}$/.test(code) ||
          !Number.isInteger(port) || (port as number) < 1024 || (port as number) > 65535 ||
          typeof token !== "string" || !/^[a-f0-9-]{36}$/i.test(token))
        return json({ error: "Bad host registration" }, 400);
      const ok = await env.LOBBIES.get(env.LOBBIES.idFromName("global"))
        .registerPlayerHost(code, raw, port as number, token);
      return ok ? json({ ok: true, expiresInSeconds: 90 }) :
                  json({ error: "Host code already in use" }, 409);
    }

    if (path === "/v1/lobbies") {
      if (request.method !== "GET") return json({ error: "GET only" }, 405);
      const lobbies = await env.LOBBIES.get(env.LOBBIES.idFromName("global")).list();
      return json({ protocol: PROTOCOL, minVersion, lobbies });
    }

    const m = path.match(/^\/v1\/rooms\/([^/]+)$/);
    if (m) {
      if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket")
        return json({ error: "Expected a WebSocket upgrade" }, 426);
      const headers = new Headers(request.headers);
      headers.delete("x-dhd-create");
      if (m[1] === "new") {
        headers.set("x-dhd-create", "1");
        for (let attempt = 0; attempt < 8; attempt++) {
          /* Local Player Hosted rooms use Hxxxxx codes to avoid forcing
             every ordinary Cloudflare private-room join through a second
             directory lookup. They are still six standard characters. */
          const localHost = (env as Env & { DHD_PLAYER_HOST_PORT?: string }).DHD_PLAYER_HOST_PORT;
          const random = randomCode();
          const code = localHost ? "H" + random.slice(1) : random;
          const room = env.ROOM.get(env.ROOM.idFromName(code));
          if (await room.claim(code)) return room.fetch(new Request(request, { headers }));
        }
        return json({ error: "Could not allocate a room code" }, 503);
      }
      const code = m[1].toUpperCase();
      if (!/^[A-Z0-9]{1,12}$/.test(code)) return json({ error: "Bad room code" }, 404);
      return env.ROOM.get(env.ROOM.idFromName(code)).fetch(new Request(request, { headers }));
    }

    return json({ error: "Not found" }, 404);
  },
} satisfies ExportedHandler<Env>;
