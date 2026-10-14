# Zone Levels

The client half of the journey layout of `mod-coa-content-scaling` (from aspollon's mod-world-journey). A realm
on the journey moves zones, dungeons, trainers and items to new levels in the server's memory; the client still
holds the original levels in its DBCs and FrameXML. Without this addon the world map, the Dungeon Finder and
item tooltips show the original values, and the Dungeon Finder stays closed below level 15.

## Installation

Copy this entire `ZoneLevels` directory into the CoA client's `Interface/AddOns` directory and clear the client
cache once. Every player of a journey realm needs it. Realms that do not play the journey send nothing, so the
addon changes nothing there.

## What it shows

At login the server sends the realm's journey over the addon channel `ZoneLevels`; the addon keeps the last
complete set per realm in `ZoneLevelsDB`, so the values are right before the server has spoken.

- The world map shows the journey's zone levels.
- The Dungeon Finder lists every dungeon at its journey levels and opens from the lowest of them instead of
  level 15 (Ragefire Chasm at 7).
- Gem, enchantment and socket bonus lines show what they give on this realm.

The addon is kept identical to `modules/mod-coa-content-scaling/client/AddOns/ZoneLevels`, which the module
repository carries for its own installations.
