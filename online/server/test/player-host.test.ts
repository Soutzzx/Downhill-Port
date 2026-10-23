import { env } from "cloudflare:workers";
import { describe, expect, it } from "vitest";

describe("private-code player-host directory", () => {
  it("registers and resolves a live host without publishing it to public lobbies", async () => {
    const index = env.LOBBIES.get(env.LOBBIES.idFromName("global"));
    const code = "Q8P7JM";
    const token = "9f6232af-1b93-4c1d-a20f-9bb913dc156e";
    expect(await index.registerPlayerHost(code, "198.51.100.42", 8787, token)).toBe(true);
    expect(await index.resolvePlayerHost(code)).toEqual({ url: "ws://198.51.100.42:8787" });
    expect((await index.list()).some((r) => r.code === code)).toBe(false);
    expect(await index.registerPlayerHost(code, "198.51.100.43", 8787,
      "118fa11c-46b3-486a-9e56-b429cde42adc")).toBe(false);
    expect(await index.registerPlayerHost(code, "198.51.100.42", 8787, token)).toBe(true);
  });

  it("brackets IPv6 host addresses in ws URLs", async () => {
    const index = env.LOBBIES.get(env.LOBBIES.idFromName("global"));
    const code = "Q8P7JN";
    const token = "9f6232af-1b93-4c1d-a20f-9bb913dc156f";
    expect(await index.registerPlayerHost(code, "2001:db8::1", 8787, token)).toBe(true);
    expect(await index.resolvePlayerHost(code)).toEqual({ url: "ws://[2001:db8::1]:8787" });
  });
});
