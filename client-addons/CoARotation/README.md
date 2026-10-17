# CoARotation

One button for the server's rotation assistance ([docs/coa/rotation-assist.md](../../docs/coa/rotation-assist.md)),
without a macro. The button shows the spell the next press would cast, with its cooldown, and tints it red out of
range, blue without enough power and grey when it cannot be used. A click or the key binding casts that spell
through the server, exactly like one `.rotation`.

## Installation

Copy this entire `CoARotation` directory into the client's `Interface/AddOns` directory, then bind a key under
*Key Bindings → CoA Rotation → Cast the next rotation spell*. Drag the button to move it.

`/coarot` (or `/coarotation`) takes `lock`, `unlock`, `scale <0.5-2>`, `reset`, `hide` and `show`. A hidden
button stops the server's updates; the key binding still casts.

## Protocol

The addon whispers itself on the `COARotation` addon prefix: `ON` subscribes to updates, `OFF` ends them and
`PRESS` casts. The server answers `N;<spell id>`, `0` when no spell is available, and sends it again only when it
changes. The preview ignores the global cooldown, so the icon stays on the coming spell while the cooldown runs.
`CoA.RotationAssist.Enable` switches the feature and `CoA.RotationAssist.PreviewInterval` (default 250 ms) sets
how often the server re-evaluates the next spell for subscribed players.
