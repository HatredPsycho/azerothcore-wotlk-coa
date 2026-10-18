# Client Lua loader

Some CoA interface pieces are not installed as addons. The server loads them into the client as Lua through
`src/server/coa/AscensionClientLua.cpp`, which wraps the code in a challenge requirement formatter that the
Ascension client runs (`SMSG_COA_PATCH_CHALLENGE_REQUIREMENT_TYPE`, 0x5BC) and then restores the real challenge
list.

`AscensionClientLuaLoader.cpp` decides when. It sends every registered piece five seconds after login and again
1.5 seconds after a `/reload`, which a measurement showed the client announces with `CMSG_SET_ACTIONBAR_TOGGLES`
(0x2BF) once its new interface has loaded. Requests that meet are merged into one send at the latest of their
times. Bots never receive it. Each piece guards itself against being run twice in the same interface, so a send
without a reload, for example after the action bar options changed, is harmless.

| Piece | Registered in | Type / row | Token |
|---|---|---|---|
| CoARotation button | `src/server/coa/CoARotationAssist.cpp` | 25 / 16264 | `COA_ROTATION_UI` |
| CoACompanions frame | `mod-coa-companions/src/companion_status.cpp` | 26 / 16265 | `COA_COMPANIONS_UI` |
| Manastorm UI fixes | `mod-manastorm-endless/src/ManastormClientUIFixes.cpp` | 24 / 16262 | `MANASTORM_UI_FIXES` |

Fel Commutation's window (23 / 16263) uses the same sender on demand and is not part of the loader.

## Settings

A piece reads its stored settings from `CoAUI.Settings.<name>` and saves them with `CoAUI.Save(name, values)`,
which whispers `COAUI<TAB>SAVE<TAB><name><TAB><key=value;...>` to the player. The loader keeps them per character
in `coa_client_ui_settings` (characters database) and sends them with the piece. Names are letters, digits and
underscores up to 32 characters; values are letters, digits and `_ . - = ;` up to 200 characters. Deleting a
character deletes its settings.

Key bindings stay in the client: `/coarot bind <key>` binds the button with `SetBindingClick` and saves the
binding set, so it survives relogs although the button itself is loaded anew each time.

## Adding a piece

Register a provider with `RegisterAscensionClientLuaProvider` from the script loader, use a requirement type and
row no other piece uses, start the Lua with `AscensionClientUILua(name, settings)` and make it return early when
its frame already exists. Cover it with a gameplay scenario that counts 0x5BC packets after login and after a
`CMSG_SET_ACTIONBAR_TOGGLES`.
