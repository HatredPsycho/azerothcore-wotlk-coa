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
