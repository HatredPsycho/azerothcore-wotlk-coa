# mod-coa-companions

Loads the **CoACompanions** frame into the player's client and sends it the health of their summoned
companions, so it can draw a health bar for each of them. No addon is installed: the frame arrives through
the shared client Lua loader (`docs/coa/client-lua.md`) a few seconds after login and again after every
`/reload`. `/coacomp` takes `lock`, `unlock`, `scale <0.5-2.0>` and `reset`; position and scale are stored
on the server.

## Why it exists

The 3.3.5a client can only read a unit it holds a token for: `pet`, `target`, `focus`,
`mouseover`, `targettarget` and `pettarget`. A Necromancer's minions are guardians rather than the
single pet, so the client has no token for them. The addon could therefore only read a companion
while it happened to occupy one of those tokens - in practice while it was the target's target
during a fight, and never once the fight ended. No client-side workaround exists: automating
targeting to poll health is a protected action and would take the player's target away.

## What it sends

Once per `CoACompanions.Interval` the module walks `Player::m_Controlled`, which covers pets,
guardians and charmed units alike, and whispers the player one `LANG_ADDON` message:

```
CoACompanions<TAB><entry>:<current>:<maximum>;<entry>:<current>:<maximum>;...
```

That is the shape the core's own `AddonChannelCommandHandler` uses, and the client raises it as
`CHAT_MSG_ADDON`. Nothing is registered client side: prefix registration only arrived in 4.x.

A message goes out when the payload changed, plus a keep alive every three seconds so the addon
can tell a live reading from a stale one, and one final empty message when the last companion is
gone. A player with no companions costs one integer comparison per update. The body is capped so
a full army stays inside a single message.

With the module disabled neither the frame nor the feed is sent.

## Settings

Copy `conf/mod_coa_companions.conf.dist` to the server's `configs/modules/`.

| setting | default | meaning |
|---|---|---|
| `CoACompanions.Enable` | `1` | load the frame and send the feed |
| `CoACompanions.Interval` | `1000` | milliseconds between updates for one player, minimum 250 |

## Verifying it

With a Necromancer holding minions, end a fight and walk away. The frame's bars keep moving and
stay bright green, and the `*` that marks a stale reading does not appear. A `/reload` brings the
frame back at its stored position after about two seconds.
