// Public lobby list: one global Durable Object. Rooms push their entry when it changes and
// refresh it every 30 s; entries not refreshed for 90 s are dropped (a room that died without
// saying so).
import { DurableObject } from "cloudflare:workers";

export const LISTING_REFRESH_MS = 30_000;
const LISTING_TTL_MS = 90_000;

export interface LobbyEntry {
  code: string;
  roomName: string;
  version: string;
  track: number;
  players: number;
  maxPlayers: number;
  hostName: string;
}

interface Stored extends LobbyEntry {
  updatedAt: number;
}

/* A direct player-hosted room is private and never appears in the public
   lobby list. Only its randomly generated code resolves to an IP:port.
   The 90s lease prevents dead hosts from leaving unusable codes behind. */
const HOST_TTL_MS = 90_000;
interface PlayerHost {
  code: string;
  address: string;
  port: number;
  token: string;
  updatedAt: number;
}

export class LobbyIndex extends DurableObject<Env> {
  async registerPlayerHost(code: string, address: string, port: number, token: string): Promise<boolean> {
    const key = `host:${code}`, now = Date.now();
    const old = await this.ctx.storage.get<PlayerHost>(key);
    /* No other user may silently replace a live registration. */
    if (old && now - old.updatedAt <= HOST_TTL_MS && old.token !== token)
      return false;
    await this.ctx.storage.put<PlayerHost>(key, { code, address, port, token, updatedAt: now });
    return true;
  }

  async resolvePlayerHost(code: string): Promise<{ url: string } | null> {
    const key = `host:${code}`;
    const host = await this.ctx.storage.get<PlayerHost>(key);
    if (!host) return null;
    if (Date.now() - host.updatedAt > HOST_TTL_MS) {
      await this.ctx.storage.delete(key);
      return null;
    }
    const addr = host.address.includes(":") ? `[${host.address}]` : host.address;
    return { url: `ws://${addr}:${host.port}` };
  }

  async upsert(entry: LobbyEntry): Promise<void> {
    await this.ctx.storage.put<Stored>(`room:${entry.code}`, { ...entry, updatedAt: Date.now() });
  }

  async remove(code: string): Promise<void> {
    await this.ctx.storage.delete(`room:${code}`);
  }

  async list(): Promise<LobbyEntry[]> {
    const now = Date.now();
    const rows = await this.ctx.storage.list<Stored>({ prefix: "room:" });
    const out: LobbyEntry[] = [];
    const stale: string[] = [];
    for (const [key, { updatedAt, ...entry }] of rows) {
      if (now - updatedAt > LISTING_TTL_MS) stale.push(key);
      else out.push(entry);
    }
    if (stale.length) await this.ctx.storage.delete(stale);
    return out;
  }
}
