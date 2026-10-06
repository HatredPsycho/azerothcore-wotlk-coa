# Extras

Client-side pieces that server features depend on. They are kept here so that a feature and the
addon it needs stay in one repository; nothing in this directory is built or installed by CMake.

## CoAScaleTooltips

Shows the amount an item proc really applies.

The 3.3.5a client builds a buff's tooltip from its own `Spell.dbc`, and `SMSG_AURA_UPDATE` carries
no effect amounts, so a proc from an item the realm scaled down keeps advertising the figure it was
authored with. Sharpened Twilight Scale, authored at item level 284, still promises 1472 attack
power to a level 60 character who is in fact given 498.

`mod-coa-content-scaling` sends the applied amount, together with the authored one, over the
`CoAScale` addon channel whenever it cuts an item-granted aura. This addon substitutes the one for
the other inside the finished tooltip line, which keeps it independent of how the description is
phrased and of the client's language. The normal buff display is used; no frame of its own is
created, and interfaces that replace the buff bars are covered as long as their tooltips go through
`GameTooltip`.

Install by copying the `CoAScaleTooltips` directory into the client's `Interface\AddOns`.

The server side is switched by `CoAContentScaling.PublishAuraAmounts` (default on). Without the
addon the messages are ignored; without the server option the addon has nothing to show and the
tooltip stays at the authored figure.

## CoACompanions

Unit frames for summoned companions: left click targets one, right click releases it.

A Necromancer's minions and other guardians are not the one pet, so the 3.3.5a client has no unit
token for them and cannot draw their health. `modules/mod-coa-companions` sends each companion's
entry and health over the `CoACompanions` addon channel, and this addon draws its bars from that.
Without the module it falls back to the unit tokens and only sees a companion while it happens to
be the target, focus or mouseover.

Install by copying the `CoACompanions` directory into the client's `Interface\AddOns`. `/coacomp`
(or `/coacompanions`) takes `lock`, `unlock`, `scale <0.5-2.0>` and `reset`.

## CoALFGMode

Sets how the Dungeon Finder forms your group, from a row above its queue button:

- **Group**: Matchmaking (wait for other players), Fill with bots (missing members are filled when you
  enter), or Start now (enter at once with the current group or alone, without a role check).
- **Bosses**: the group size encounters are scaled for; Adaptive follows the real group.

These are the per-character settings behind `.lfgmode` and `.lfgchallenge` in
`mod-coa-content-scaling`. The addon reads and changes them over the `CoALFG` addon channel, and the
server answers every request with the resulting state, so the row only ever shows what the server
holds. In a group the leader's setting decides. The tooltip explains the choices and says why a
setting has no effect: content scaling off, no bot provider, not the group leader, or no answer
from the server.

Install by copying the `CoALFGMode` directory into the client's `Interface\AddOns`. `/coalfg party`,
`/coalfg challenge 5` and so on set the same from the chat line; `/coalfg` alone shows the current
setting. The server side is switched by `CoACompanions.Enable` (default on) and sends at
`CoACompanions.Interval` milliseconds.
