# mod-abyssal-storage-plus

**Account-wide interdimensional vault for all profession materials in AzerothCore (WoW WotLK 3.3.5)**

### How it works
**Storing** - Automatically deposits looted/created profession materials into a shared account-wide vault. Your vaulted materials are visible and easily managed via the included addon.  
<br>
**Crafting** - The profession crafting interface displays your vaulted materials seamlessly. Crafting automatically pulls required materials from the vault into your bags for that craft; cancelling a craft returns the materials to the vault.
<br>

<details>
  <summary>🔍 View Table of Contents</summary>

  ## Contents
  - [How it works](#how-it-works)
  - [Features](#features)
  - [Opening the Vault](#opening-the-vault)
  - [Depositing Materials](#depositing-materials)
  - [Withdrawing Materials](#withdrawing-materials)
  - [Abyssal Storage Plus Interface](#abyssal-storage-plus-interface)
  - [Commands](#commands)
  - [Configuration](#configuration)
  - [Installation](#installation)
  - [Changelog](#changelog)
</details>

---

**This mod is based on [mod-abyssal-storage](https://github.com/thanhtong89/mod-abyssal-storage)**  
If migrating from this mod, just replace the module & addon with mine. It still uses the same SQL database table name `abyssal_storage` so if you had items stored already, they will still be there.

<table>
  <tr>
    <td background-color="#14eebf">  

 ![New Feature](https://img.shields.io/badge/The%20Main%20Differences%20Include-blue)  
- <kbd>**MAJOR UPDATE**</kbd> A rewrite of how vaulted materials are handled in the crafting interface. Now completely integrates vaulted materials into the default UI instead of using an overlay.
  - <kbd>**OLD**</kbd> Only showed vaulted materials as an overlay if you clicked on each recipe one by one. Didn't show how many of a recipe you could craft with vaulted materials, or have a working "Have Materials" filter. 
  - <kbd>**NEW PLUS**</kbd> Now completely takes into account vaulted materials in the base blizzard UI, functions just like if you had those materials in your bags, so things like material counts, how many of a recipe you can craft and the "Have Materials" checkbox all work seamlessly with the vault.
   
     **BEFORE:** <img width="194" height="81" alt="TitaniumBarOLD1" src="https://github.com/user-attachments/assets/9f6fad52-d4bd-4cdb-b772-966f2233b494" />
     **AFTER:** <img width="193" height="78" alt="TitaniumBar 9" src="https://github.com/user-attachments/assets/d61ae12a-1a1c-4502-a7a7-e10b87fb37cf" />
- Added an addon settings menu to enable/disable vaulting materials based on aquisition method - (Loot/Craft/Mail/VendorBuy/QuestReward).
- Better Auto-Vault visibility - Whenever an item is looted and instantly vaulted, replaces default "You receive loot:" system messages with "You vaulted  material:" system message instead. Works for all enabled acquisition methods.
- The claimed quest integration (pull materials on quest turn-in) was non-functional. Added entirely new logic - Accepting a quest that requires materials will auto pull the full or partial amount of materials from your vault if they exist. Any looted materials that are required for active-quests are excluded from auto-deposit up to the required amount. Also prints an informative message, Example: "Abyssal Storage Plus: Pulled [Lean Wolf Flank] x8 from storage for quest [Seasoned Wolf Kabobs]; 2 still needed.".
- For recipes that required a target item e.g. Enchanting, socket item; the original mod only pulled materials and gave an error on clicking the modify/enchant button. Plus version now pulls the materials preemptively on hovering over the "Enchant/Modify" button so that it only requires a single click to function. Selecting another recipe or exiting the profession UI returns pulled materials to the vault.
- Cancelling a craft now instantly returns any pulled materials to your vault if they were unvaulted
- Added a new clean *modern interface* that sorts by profession instead of material type, this is the new default. see screenshots below, you can switch between Modern/Classic anytime by clicking the switch icon in the top-left.
- Improved abyssal storage addon interface and functionality (Classic UI). Thanks to [Tntdruid](https://github.com/Tntdruid) for some of his contributions.
- Now able to easily deposit per-material via right-click when abyssal storage interface is open (Deposit All was the only option before). Also added per item SFX feedback when depositing/withdrawing (same used for banks).
- No longer vaults profession tools on deposit all (if I missed anything, create an issue)
- Keybinds + Minimap icon allowing easier depositing/withdrawing
- If you use playerbots fork, automatically checks for and skips rndbots, significantly reducing unnecessary database queries. Altbots still included. 

*Currently working on other improvements, stay tuned*
    </td>
  </tr>
</table>

## Features
- **Auto-deposit**: Profession materials are automatically vaulted when picked up
- <kbd>**NEW**</kbd> **Crafting integration**: Seamlessly integrates vaulted materials when crafting, both the material counts and craftable recipe amounts take into account all vault stored materials, tested on default Blizzard interface and elvUI. Example screenshots below
- <kbd>**NEW**</kbd> **Vault-aware filtering**: The "Have Materials" checkbox filters craftable recipes based on total material counts in bags and abyssal storage plus
- <kbd>**NEW**</kbd> **Quest integration**: Accepting a quest that requires materials will auto pull the full or partial amount of materials from your vault if they exist. Any looted materials that are required for active-quests are excluded from auto-deposit up to the required amount.
- <kbd>**UPDATE**</kbd> **Vault Interface Addon**: A choice between modern or classic UI for displaying vaulted materials (images below). Includes fuzzy search and materials grouped by profession category (modern) or material type (classic)
- **Real-time sync**: Vault state syncs on login and updates incrementally
- <kbd>**NEW**</kbd> **Tools protected**: Profession tools and other outliers in the profession category are excluded from Deposit All (runed rods, Gnomish Army Knife, Jeweler's Kit etc.)
- <kbd>**NEW**</kbd> **Vault Shortcuts**: `Shift + B` or click minimap icon to open bags + vault **/** `Ctrl + B` or right-click minimap icon to deposit all materials.
- <kbd>**NEW**</kbd> **Dynamic Tooltip**: When hovering over a material in the vault, tooltip will dynamically show only if you own more of that item in your bags and how many you own.
- <kbd>**NEW**</kbd> **Auto-Vault Options**: Addon options menu to Enable/Disable auto-vaulting materials based on obtain method -  (Loot/Craft/Mail/VendorBuy/QuestReward).
- <kbd>**NEW**</kbd> **Auto-Vault Visibility**: Based on the Enabled options set above, you will get notified by a system message in chat whenever a material is vaulted. 
  - Example Message: <img width="340" height="29" alt="Vaulted Material Notifcation" src="https://github.com/user-attachments/assets/ee2dd4ac-0cc3-44c3-abce-47c1879536cb" />


## Opening the Vault
- Right-clicking the backpack
- Type `/asp`
- Press Open All Bags Keybind (default `Shift-B`); the vault opens alongside your bags
- <kbd>**NEW**</kbd> Left-click the minimap icon (drag it to reposition)

## Depositing Materials
- Items should automatically get deposited on pickup
- Click "Deposit All" in the abyssal storage interface
- <kbd>**NEW**</kbd> Right-click a material in your bags while the vault is open
- <kbd>**NEW**</kbd> Right-click the addon's minimap icon to Deposit All
- <kbd>**NEW**</kbd> Deposit All is available under **Key Bindings > AddOns > Abyssal Storage Plus**
   - New installs default to `Ctrl-B` if that key is unbound

## Withdrawing Materials
- Right-click a stored material in the vault to withdraw the full stored amount
- Shift-left-click a stored material to withdraw a specific amount

## Abyssal Storage Plus Interface

### <kbd>**NEW**</kbd> Modern UI (You can Toggle Between Modern/Classic Any time by clicking the icon in the top-left)
<details>
<summary> <b>Profession Category Info</b> </summary>

- <kbd>**MERGED**</kbd> **Cooking + Fishing**
- <kbd>**MERGED**</kbd> **Mining + Blacksmithing**
- <kbd>**MERGED**</kbd> **Skinning + Leatherworking**
- <kbd>**MERGED**</kbd> **Herbalism + Alchemy**
- **Engineering** Includes Gadgets
- **Jewelcrafting** includes uncut/cut gems
- Some materials in Azerothcore database were incorrectly categorized and have been remapped to the correct category
</details>
<img width="959" height="718" alt="Window 1586 01-11-2026 (World_of_Warcraft)" src="https://github.com/user-attachments/assets/9c404bdb-e81b-493b-a3d6-52214dce5d71" />

### ElvUI Frames + Classic UI
<img width="1583" height="562" alt="ElvUI-Abyssal-Storage" src="https://github.com/user-attachments/assets/a9e4ff26-75f8-4f6e-9fef-4c6e6ebf54e8" />

### Options Menu + Defaults
<img width="365" height="275" alt="Window 1654 10-11-2026 (World_of_Warcraft)" src="https://github.com/user-attachments/assets/66983f8e-07a6-4967-95cc-a52323a4296c" />


## Commands
| Command | Description |
|---|---|
| `/asp` | Toggle the vault UI |
| `/asp deposit` | Deposit all trade goods from inventory |
| `/asp deposit <itemId> [count]` | Deposit one material type |
| `/asp withdraw <itemId> [count]` | Withdraw items (omit count for all) |
| `/asp sync` | Force re-sync from server |

## Configuration
In `mod_abyssal_storage_plus.conf`:

```
AbyssalStoragePlus.Enable = 1
```

## Installation
1. Clone into `modules/mod-abyssal-storage-plus`
2. Re-run CMake and build
3. Rename `mod_abyssal_storage_plus.conf.dist` to `mod_abyssal_storage_plus.conf` & make sure it is in your server `/configs/` directory
4. The `abyssal_storage` table should be automatically created in your `acore_characters` database on server start. If the table is missing, run `data/sql/db-characters/abyssal_storage_plus.sql` against your `acore_characters` database
5. Copy the `addon/AbyssalStoragePlus` folder into your WoW `Interface/AddOns` directory

## Changelog

### 2026-11-10
- Added an addon options menu to enable/disable auto-vaulting materials based on acquisition method - (Loot/Craft/Mail/VendorBuy/QuestReward).
- Improved upon addon system messaging to handle all acquisition methods isntead of only looted materials.

### 2026-11-09
- Overwrite default blizzard system loot messages to show when a materials gets vaulted instead of looted to bags, improving clarity.

### 2026-11-08
- Performance improvements, Reduce vault UI redraws and quest-log scans.
- Second round of performance improvements & minor code improvements.

### 2026-11-07
- Refactor some logic and clean up code.
- Vault changes now instantly update every online character on the account, including altbots.
- Added safeguards against vault desync: deposits and withdrawals record exactly what moved and undo the transfer if it fails.
- Full vault syncs now clear stale entries reliably, and `/asp craft` rejects recipes the character hasn't learned.

### 2026-11-06
- Fix regressions for "Create" and "Create All" logic that got overwritten with a recent commit to improve handling for recipes that require a target e.g. enchanting

### 2026-11-05
- Fixed Enchanting/Socket item recipes not pulling their materials from the vault. Recipes that cast on an item (Enchant/Modify) now pull vault materials on hover and cast on a single click.
- Refactored right-click bag deposit to use `hooksecurefunc`, fixing the `Interface action failed because of an AddOn` error that blocked using on-use items (food, scrolls, essences, etc.).
- Replaced the broken quest turn-in auto-pull with a reliable quest-accept auto-unvault.
- Added a chat warning when a required material can't be pulled because of low bag space.

<details>
<Summary> Click to see older commits</summary>

### 2026-11-04
- Quest items are now better protected during Deposit All.
- Pulled materials are returned to the vault if bag space runs out mid-craft.
- Canceled crafts now revault reliably.
- Fixed a bug where a manual withdraw could disable auto-deposit until relog/manual deposit.
- Fixed craftable-quantity flicker when starting or cancelling a craft.

### 2026-11-03
- Fixed the "Craftable Quantity" counter sometimes not updating.
- Added revaulting of materials when cancelling a craft.

### 2026-11-02
- Improved tooltip item counts.
- Fixed the vault UI auto-showing with Auctionator.

### 2026-11-01
- Added the new modern vault interface (grouped by profession) with a minimal Modern/Classic switch icon.
- Added a generator script to correct profession materials with the wrong Trade Goods subclass in the AzerothCore database.

### 2026-10-31
- README and image preview updates.

### 2026-10-30
- Correct per-item sound effects on deposit/withdraw, plus a separate Deposit All sound and message.

### 2026-10-29
- Added per-item deposit/withdraw via right-click while the vault is open.
- Fixed the random-bot skip logic that caused very large database query overhead.

### 2026-10-25
- New "Deposit All" alternatives and extra skipping for outlier profession items.

### 2026-10-24
- Fully integrated the vault into the default Blizzard crafting UI (material counts, craftable amounts and the "Have Materials" filter).
- Fixed recipe selection with the "Have Materials" filter via dynamic row display and height.

### 2026-10-22
- Protected profession tools from auto-deposit.
- Added a minimap button, improved search, and fixed scrollbar, layout and visual issues.

### 2026-10-20
- UI updates; converted spell casting to `Unit` to match current AzerothCore.
</details>

## CoA changes

Imported from LightFrost/mod-abyssal-storage-plus at 7d030cd; the addon lives in
[client-addons/AbyssalStoragePlus](../../client-addons/AbyssalStoragePlus). It replaces mod-reagent-bank-account;
`rev_20261113_01_coa_abyssal_storage_migration.sql` creates `abyssal_storage` (the module's own SQL is
not shipped) and moves the stored reagents into it.

- Every vault change and the character's inventory save in one transaction, so a crash cannot duplicate deposited
  materials or lose withdrawn ones.
- The vault is account-wide: materials that bind on pickup or as quest items stay in the bags unless they are
  account-bound.
- Mail auto-vault uses the core's `OnPlayerTakeMailItem` hook and moves the taken material's whole bag count.
- Alt bots run `.asp deposit` from mod-playerbots (`AiPlayerbot.AltBotReagentDepositInterval`); random bots never
  use the vault.
