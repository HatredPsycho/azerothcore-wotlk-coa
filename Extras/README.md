# Extras

Client-side pieces that server features depend on. They are kept here so that a feature and the
addon it needs stay in one repository; nothing in this directory is built or installed by CMake.

## CoACompanions

Unit frames for summoned companions: left click targets one, right click releases it.

A Necromancer's minions and other guardians are not the one pet, so the 3.3.5a client has no unit
token for them and cannot draw their health. `modules/mod-coa-companions` sends each companion's
entry and health over the `CoACompanions` addon channel, and this addon draws its bars from that.
Without the module it falls back to the unit tokens and only sees a companion while it happens to
be the target, focus or mouseover.

Install by copying the `CoACompanions` directory into the client's `Interface\AddOns`. `/coacomp`
(or `/coacompanions`) takes `lock`, `unlock`, `scale <0.5-2.0>` and `reset`.
The server side is switched by `CoACompanions.Enable` (default on) and sends at `CoACompanions.Interval`
milliseconds.
