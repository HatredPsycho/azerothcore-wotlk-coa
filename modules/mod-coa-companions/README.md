# mod-coa-companions

Sends the health of a player's summoned companions to their client, so the **CoACompanions**
addon can draw a health bar for each of them.

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

The module is optional. With it disabled the addon falls back to the unit tokens and behaves as it
did before.

## Settings

Copy `conf/mod_coa_companions.conf.dist` to the server's `configs/modules/`.

| setting | default | meaning |
|---|---|---|
| `CoACompanions.Enable` | `1` | send the feed at all |
| `CoACompanions.Interval` | `1000` | milliseconds between updates for one player, minimum 250 |

## Verifying it

With a Necromancer holding minions, end a fight and walk away. The addon's bars keep moving and
stay bright green, and the `*` that marks a stale reading does not appear. Disabling the module
and reloading brings the `*` back once the companions leave every unit token.
