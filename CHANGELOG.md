# Changelog

Full version history for DungeonKit. The newest versions are logged at the top of each era below; older entries (1.7.0 onward) were logged oldest-first further down.

## 1.7.0 additions
- Apocalypse Level (Player page) uses the table's saved pointer paths; it only works on game versions where one of them still matches.
- Item Stat Viewer (read only): credit Hybrid.
- Tools > Advanced: names and object dumps written to the Desktop (developer use only).

## 1.8.0 additions
- Home: "Quick access" card. Finds the game's Paks folder (from the running game, otherwise the usual
  install locations), with buttons to open Paks, open/create the `~mods` folder, browse manually, or rescan.

## 1.9.0 additions
- New QUICK ACCESS category: Pak Directory (auto-find, paste/browse a path, mod count, close-the-game warning) and Websites.
- Removed the Home quick-access card; removed "Ruben Elia" from the Item Stat Viewer credit.

## 1.10.0 additions
- Pak Directory: pak mod safety notice.
- Websites: Website tools, Modding websites (Nexus Mods, CurseForge) and Discord servers sections.
- Customize: "Sidebar layout" card (locked until ticked) to reorder categories and pages; saved in QSettings (sidebar/layout), with Reset sidebar.

## 1.11.0 additions
- Quick Access: Backup Saves (copies saves to `Desktop/DungeonKit Backups/Saves <date>`), Changelog (with a placeholder for update checks).
- Pak Directory: installed mod list with size and date, disable/enable by renaming to `.pak.disabled`.
- Customize: LCD preset (slow dark colour drift; pauses when the window is in the background) and a Language picker
  (Spanish, French, Japanese, Chinese, Russian). Translations live in `src/ui/I18nData.cpp` (generated table: English, es, fr, ja, zh, ru);
  the UI is translated by matching its English text, so new strings need a table entry to be translated.

## 1.12.0 changes
- Apocalypse Level: confirmation popup removed, Freeze added.
- Enchant tiers are editable pickers (any number can be typed).
- Copy/Load Item Data clones item ID, rarity, power, gilded, gilded enchant/level, enchant IDs and enchant levels (not stats).
- Item stats: capacity limit raised to 4096 and read in one batch, so heavily modded items are detected.
- Player Model note; neutral changelog wording.

## 1.13.0-test
- Home: "Target game" choice (Minecraft Dungeons / Minecraft Dungeons 2). The choice is remembered.
- New TEST sidebar category with a Test page: Timer, Pause Game and a "Run check" connection report (copyable).
- With Minecraft Dungeons 2 selected, only Timer and Pause Game can be switched on. They use the same engine lookup as
  Minecraft Dungeons (written for Unreal 4.22-4.24 layouts), so they may report "engine data not found" until it is adapted.

## 1.13.1-test
- Minecraft Dungeons 2 target: DungeonKit attaches to Dungeons-WinGDK-Shipping.exe and Dungeons.exe at the same time.
  If only one of them is running it uses that one. Timer and Pause use whichever process has readable engine data.
- Test > Run check lists both processes (base, size, engine lookup result).

## 1.13.2-test
- Engine lookup: if the built-in property layouts don't fit a game build, it now searches for the right layout by checking the
  engine's "GameInstance" property, and logs what it tried.
- Test > Run check lists each step of the path to TimeDilation (GEngine, GameViewport, World, PersistentLevel, WorldSettings).

## 1.13.3-test
- Automatic layout search: property offsets up to 0xFFFF are accepted (the engine class is large), and the GameInstance check accepts the game's own subclass.

## 1.14.0-test
- Test page: Data dump card (names, classes and fields, objects). Files go to the Desktop; for Minecraft Dungeons 2 they are named "DungeonKit MCD2 ...".
- Object table: if the Dungeons 1 patterns do not match, the module is scanned for the table by its shape.
- Field lists now work for newer engine layouts (property offsets are read relative to the detected layout).

## 1.15.0-test
- Four new pages in the TEST category for Minecraft Dungeons 2 (pick it as the target game on Home first):
  - **MCD2 Player**: live position, teleport/save position, 14 gameplay attributes (health, emeralds, XP, level, speed ...) with Set and Freeze, and the character movement values (walk speed, jump, gravity ...).
  - **MCD2 Inventory**: read-only list of items with slot, rarity, level, stack count, equipped slot and enchantments.
  - **MCD2 World**: read-only map name, discovered minecart stations, activated gimmicks, played cutscenes, discovered dungeon doors.
  - **MCD2 Camera**: field of view and camera distance with Freeze and Reset.
- Inventory, World and Camera need "Refresh / Find camera" once, and again after changing level.

## 1.16.0
- Home > Target game now has three choices: **Default** (not connected to any game, and what DungeonKit always starts on), **Minecraft Dungeons**, and **Minecraft Dungeons II**.
- The TEST category is now **MINECRAFT DUNGEONS II** and sits above MISC. Its pages: Tools (Timer, Pause Game, Connection check and the Data dump, which carries a developers-only warning), Player Viewer, Inventory Viewer, World Viewer and Camera Editor, all with warning-sign icons. Older saved sidebar layouts are migrated automatically.
- Camera Editor presets: FOV Far (65 / 90 / 3700 / 3700), FOV Super (80 / 120 / 7000 / 7000) and FOV First Person (camera distance 0).
- Minecraft Dungeons II version 1.1.2 (Dungeons-WinGDK-Shipping.exe, module size 197660672) is recognised.

## 1.17.0
New in the Minecraft Dungeons II section (all of these are tests; if one cannot write, it still works as a viewer):
- **Nearby Viewer**: chests (opened / unopened, discovered) and enemies near you, nearest first, with a radius, filters, auto-update, Teleport to selected, Save as waypoint and Copy list.
- **World Viewer > Waypoints**: your own named positions, saved between sessions (rename by double-click, teleport, delete, copy).
- **Camera Editor > Photo mode**: World speed with Freeze, plus Freeze time / Slow motion / Normal speed buttons. (The game's interface cannot be hidden from DungeonKit.)
- **Player Viewer > Fly**: switches the character to the flying movement mode; hold Page Up / Page Down to rise and sink.
- **Player Viewer > Threat level and difficulty**: the four threat attributes with Freeze, and a difficulty selector (Low / Normal / High) with Set, Keep and Unlock.

## 1.17.1
- Sidebar category MODDING renamed to MINECRAFT DUNGEONS (saved layouts are migrated).
- Nearby Viewer: double-click a row to teleport; note says it does not work online.
- Player Viewer notes: Fly works offline and somewhat online (online it pushes you to the ground but can still glitch you onto higher spots); Movement speed works offline only; Stats, Threat level and Difficulty work offline only.
- Stats: Souls, Max souls, Arrows (ammo), Max arrows and damage rows (base/melee/ranged/artifact multipliers, critical chance and multiplier, base damage bonus), read from ATR_Soul, ATR_RangedAttack and ATR_Damage.

## 1.18.0
- New **Inventory Editor** page in the Minecraft Dungeons II category (separate from the Inventory Viewer): select an item and change its item, rarity, level and stack, and edit its enchantments (which enchantment, strength, points invested). Writes are refused if the list moved since it was shown. Best used offline.
- Tools moved to the bottom of the Minecraft Dungeons II category (applied once to saved sidebar layouts).
- World Viewer note: teleporting only works offline.

## 1.18.1
- A **Disable all mods** button on every Minecraft Dungeons II page (Player Viewer, Inventory Viewer, Inventory Editor, World Viewer, Nearby Viewer, Camera Editor and Tools). It switches off Fly, Keep difficulty and every Freeze.
- The Minecraft Dungeons II pages, notes, buttons, stat labels and status messages are translated into the app's five languages (es, fr, ja, zh, ru). The changelog entries for these versions stay in English.

## 1.18.2
- Inventory Editor: **Power** (read from the item's generator data) is shown and editable up to 99,999, and Level now also goes up to 99,999.
- Only gear is listed: weapons, armor, artifacts, talismans and enchantment books. Cosmetics, audio, consumables, quest items, currencies and crafting materials are hidden unless **Show everything** is ticked.
- Effects (the item's stats and enchantments) appear in the table. Names are cleaned up (SW.Item.Club shows as Club) while the full tag is still what gets written.
- **Equipped only** filter and an **Equipped slots** picker that jumps to the item in each slot. Selecting an item by hovering over it in the game is not possible: the game's inventory screen does not expose the hovered item.

## 1.19.0
- Minecraft Dungeons II category order: Inventory Viewer, Inventory Editor, Player, World, Nearby Viewer, Camera Editor, Tools (applied once to saved sidebar layouts). Player Viewer and World Viewer are now simply Player and World.
- New icons for the Minecraft Dungeons II pages (chest, pencil, heart, globe, camera, pin, stopwatch) instead of the warning sign.
- Inventory Editor: double-click an item to load it into the Selected item section. The effect list is limited to the effects on your own items plus their I / II / III levels, and effect edits only write the values that changed. A game crash while editing effects was reported; changing which effect an entry is remains the risky part.

## 1.20.0
- **World > Missions (test)**: Scan missions lists the quests the game currently has loaded (name, state, ID) with a filter. **Unlock locked missions** changes every Locked or Unavailable mission to Available by writing the stored state. It is a test: the game may not refresh its screens and some gates may check other things. Map reveal, bosses and doors are not touched (they are stored in structures DungeonKit cannot safely grow).

## 1.21.0
- Home: three buttons (**General Information**, **Minecraft Dungeons FAQ**, **Minecraft Dungeons II FAQ**). General Information shows first. The Minecraft Dungeons II FAQ is new.
- Sidebar: **Items** is now **Item Editor** (Minecraft Dungeons). The Minecraft Dungeons II **Inventory Viewer** is no longer in the sidebar. Saved layouts are updated once.
- Minecraft Dungeons II sidebar order: Inventory Editor, Player, No Clip & Stats, World, Nearby Viewer, Camera Editor, Tools.
- New **No Clip & Stats** page for Minecraft Dungeons II: **No Clip** (flying plus the pawn's collision switched off; restored when switched off or on Disable all mods), **Fly**, **Movement** and **Stats**. Player now holds Status, Location and Threat level and difficulty.

## 1.21.1
- Player > **Stats** again holds health, max health, potion charges, emeralds, spring stones, XP, level, enchantment points, souls, max souls, arrows and max arrows.
- No Clip & Stats > **Game stats** holds movement speed, gravity, air control, roll charges, interaction range and the damage multipliers, next to Fly, No Clip and Movement.
- New **God Mode** (No Clip & Stats): refills health to its maximum several times a second. Offline only.

## 1.22.0
- The sidebar page list now sits in a scroll area, so the buttons are no longer squeezed together on short windows.
- Removed the animated **LCD** theme preset and its timer (the colour changes made the interface lag). Any saved LCD state is cleared; the other presets and colour settings are unchanged.
- Translations: the Minecraft Dungeons II notes (quests, inventory, world, nearby), window title text and the whole 1.14 - 1.21 changelog now exist in Spanish, French, Japanese, Chinese and Russian.

## 1.22.1
- Loading screen: steps are now Steam, Xbox, Launcher and Saves, with the folder being checked shown under the bar. It is cosmetic; the real folder search is still on the Home page. The old "Injecting DLL" wording is gone (DungeonKit does not inject).

## 1.23.0
- Minecraft Dungeons II > No Clip & Stats: **No Clip** and **Fly** are now a single **No Clip** switch (flying plus collision off), with the rise / sink speed and Up / Down buttons, like Minecraft Dungeons. Page Up and Page Down still rise and sink.
- New **Soul Storm (finder)** on the same page. The soul storm timer is not a known field, so **Scan storms** lists the numeric and on/off values of every live object with "storm" in its name. **Live values** shows them changing; the one that counts by itself is the timer. Select a row, type a value and press **Set**, or tick **Keep it there** to hold it. **Copy report** copies the list. Test only: the game may overwrite the value, and it is meant for offline use.

## 1.23.1
- Customize > **Import / export style** is explained in plain words (Import style, Export style, Remove imported style, Reset everything to default), shows a message after each action, refuses empty files, and lists the parts that can be styled (sidebar, sidebar scroll area, Home tabs, Disable all button, cards, hints). Exported files start with the same list as a comment.

## 1.23.2
- Fixed a build error in the Soul Storm (finder) text.

## 1.24.0
- Home > Target game: new **Close game processes** button. It asks first, switches the mods off, disconnects, then ends every `Dungeons.exe`, `Dungeons-Win64-Shipping.exe` and `Dungeons-WinGDK-Shipping.exe` process, so a game that stays running in the background after closing is no longer treated as open. It reports how many were ended and how many were refused (run DungeonKit as administrator if any were).

## 1.25.0
- Inventory Editor (Minecraft Dungeons II): the list now allows picking several items (Ctrl / Shift click, or **Select all shown**). **Delete selected** asks first, then removes the chosen entries from the inventory list (the entries after them are shifted down and the slot count is reduced). Equipped items are skipped, and an entry is only deleted if it still holds the item it had when listed. This is a test: the game may still point at a removed entry, so use it offline and keep a backup of your save.

## 1.26.0
- Backup Saves: new **Debug: what DungeonKit accesses** card. It lists every save folder the page looks at (found or missing, with file counts), the single folder it writes to (the Desktop `DungeonKit Backups`), the registry key where settings are kept, and a line per step of the last backup (`READ`, `WRITE`, `COPY`, `FAILED`). **Refresh**, **Copy** and **Clear log** buttons. The report only looks; it changes nothing. The log text itself is in English.

## 1.26.1
- Fixed a build error in the Backup Saves debug section.

## 1.26.2
- Fixed a build error in the Inventory Editor delete confirmation.

## 1.27.0
- Minecraft Dungeons II > Player: new **Never run out** card. **Infinite arrows** keeps arrows at their maximum. **Infinite enchantment points** puts back any points you spend (it never goes below the highest value seen since the switch was turned on). **No cooldown** keeps potion charges and roll charges at the highest value seen and souls at maximum, so potions, rolls and artifacts can be spammed. These refill values many times a second, so a cooldown the game keeps as its own timer (some artifacts) may remain. Offline only; **Disable all mods** switches them off. Untested in game.
- Customize > Presets: a second row with **Sandstone** (desert gold), **Slime** (acid lime), **Oxidized** (copper and teal), **Sunset** (coral and amber over plum), **Neon** (magenta and cyan), **Royal** (royal blue on navy) and **Ash** (warm grey and ivory).

## 1.27.1
- Minecraft Dungeons II > Player is reordered: **Status**, **Player**, **Stats**, **Threat level and difficulty**, **Location** (last).
- The "Never run out" card is now a **Player** card that looks like the Minecraft Dungeons one: three checkboxes (Infinite Arrows, Infinite Enchant Points, No Cooldown), each with a short grey line under it. Same behaviour as 1.27.0.

## 1.28.0
- Minecraft Dungeons II > Player: the note at the top is shorter (the rest is unchanged). A small note under the Player card says **Infinite Arrows** and the rolling part of **No Cooldown** also work online.
- Customize > Presets, third row: **Randomize Theme** picks random dark colours straight away (corner size, font size and any imported style are kept). **Random Startup** is a switch (lit = on): when on, DungeonKit picks a random theme every time it opens and every time it closes.

## 1.29.0
- Camera Editor now has seven presets: FOV Far (65 / 90 / 3700 / 3700), FOV Super (80 / 120, distance untouched), Quake Pro (95 / 105, distance untouched), Minecraft (70 / 70 / 2300 / 2300), Close Up (50 / 60 / 1800 / 1800), Tactical (55 / 70 / 5500 / 5500) and Wide Angle (110 / 110, distance untouched). The distance values are estimates; use Reset to original to go back.
- FOV First Person is removed.
- Removed all remaining notes about the account being put at risk.

## 1.29.1
- Minecraft Dungeons II > No Clip & Stats: shorter, simpler text on every card.

## 1.30.0
- Minecraft Dungeons > Player > No Cooldown: if the game code pattern is not found (Steam build), it now falls back to a slot cooldown mode. It reads your equipment slots through the game object tables and raises each slot's CooldownReduction to 600. The original values are put back when you switch it off. Untested on Steam: after switching it on, the message shows how many slots were changed and their previous values. Please report what you see.

## 1.30.1
- Minecraft Dungeons > Tools > Advanced: new **Dump classes and fields** button. It saves `DungeonKit ClassesDump.txt` to your Desktop.

## 1.31.0
- Minecraft Dungeons > Player > Apocalypse Level: if none of the saved pointer paths matches (Steam), it now looks for the mission map's threat slider through the game object tables and reads/writes its `selectedStruggleLevel`. Open the mission map first, then switch it on, and keep the map open while you set the level. Untested on Steam. The class dumps show Steam and Xbox have identical layouts.
- Unlimited Gifting is not added: the gift-wrap merchant and its restock quest have no readable fields in the game's class dump.

## 1.121.0

Minecraft Dungeons II > Inventory Editor > Selected item: added an **Enchants** section that reads whichever enchant(s) are actually on the picked item (matched by the same `SW.Enchantment.` tag prefix the data catalog uses) and lets you change which enchant it is, its tier (I/II/III), and its enchantment-point cost. Editing only - it can't add an enchant slot an item doesn't already have.

## 1.120.0

Minecraft Dungeons II: removed **Blur chat** entirely - the capture-exclusion fix in 1.119.0 still didn't work well enough, so it's gone rather than left half-working. Camera & Display is back to just the Display mode stats overlay (Health, Damage, DPS, Walk speed, Coordinates, Quest, Deaths, Emeralds, Echo Shards, Enchantment points, Souls, Nearest blueprint) with no chat-related feature.

## 1.119.0

Minecraft Dungeons II > Blur chat: on Windows 10's May 2020 update or newer, it no longer needs to hide itself to refresh - it marks its own window as excluded from screen capture (`SetWindowDisplayAffinity`, a documented Windows API), so a screen grab sees straight through it to the real game, meaning it stays live with no flicker. On an older Windows build where that call isn't available, it falls back automatically to the previous brief hide-and-show. Also raised **Blur strength** and **See-through** to their full 0-100 range (previously capped lower), and added a **Darken / Lighten** slider that washes the patch toward black or white, centered at neutral.

## 1.118.0

Minecraft Dungeons II: removed **Hide chat** - testing showed it didn't work, so it's gone rather than left broken. Added **Blur chat** instead, a completely different approach: a see-through, blurred patch you park over the chat log (or anywhere else on screen), with its own corner, width/height (as a % of the game window), nudge, blur strength and a see-through amount so it reads as frosted glass rather than a dark box. Unlike Hide chat, it never touches the game at all - it only takes a screenshot of its own small patch of the screen and blurs that, so it works the same regardless of what the game's own chat widgets are called or how they behave. It briefly hides itself every time it re-grabs (a few times a second) so it captures the live game underneath instead of its own last blurred frame, which can show as a brief flicker.

## 1.117.0

Minecraft Dungeons II > Camera & Display: added **Emeralds**, **Echo Shards**, **Enchantment points** and **Souls** as toggleable, color-coded stats, plus a **Nearest blueprint** line that names the closest enemy's class name when one is within range (adjustable). The panel now groups what's switched on into Combat / Position / Currency / Status, with a blank line between sections instead of one long list. Also added a **Hide chat** switch - removed in 1.118.0 after testing showed it didn't work.

## 1.116.0

Minecraft Dungeons II: removed **Teleport to free camera** - testing showed it didn't work, so it's gone rather than left in broken. Kept **Camera offset** (confirmed working by testing) with a shorter note. Console commands: replaced the two guessed free-camera-speed commands with a short note that scrollwheel speeds the free camera up or down while `toggledebugcamera` is on.

## 1.115.0

Minecraft Dungeons II > Camera & Display: added **Camera offset X/Y/Z**, shifting the camera from its normal spot while it keeps following you around. Also added an experimental **Teleport to free camera** and two guessed console commands for free-camera speed - see 1.116.0 for what happened to those.

## 1.108.0

Minecraft Dungeons II > Inventory Editor: simplified the notes around deleting items and editing an item.

## 1.109.0

Minecraft Dungeons II > World > Soul Storm: added an experimental, off-by-default **Force Storminator reward** card alongside the existing safe Active flag/Timer picker. High risk - only tested against one specific build's code layout, not against a live game.

## 1.110.0

Minecraft Dungeons II: removed Soul Storm entirely (it never worked out). Added **Camera Editor > Display mode**: an on-screen overlay with Health, Damage, Damage per second, Walk speed and Active quest, each switched on independently.

## 1.111.0

Minecraft Dungeons II > **Camera & Display** (renamed from **Camera Editor**): fixed **Damage per second** getting stuck at 0. Added a **Coordinates (X, Y, Z)** line and **Nudge** controls to reposition the overlay. Simplified the Display mode note.

## 1.112.0

Minecraft Dungeons II > Camera & Display: added an **Enable Display mode** master switch to turn every stat on/off together. An FPS reading was **not** added - not something this app's safe, memory-only reading technique can get at reliably.

## 1.113.0

Minecraft Dungeons II > Camera & Display: **Damage per second** is now self-contained, so the **Raw damage numbers** and **Speedometer** cards were removed from Experiments. Fixed the invisible `&` in the Camera & Display sidebar label.

## 1.114.0

Minecraft Dungeons II > Camera & Display: added a **Deaths** stat. Uses a real session counter when the game has one; otherwise falls back to counting your Health hitting zero.

## 1.107.0

Minecraft Dungeons II > Inventory Editor: cleaned up the layout - filters, Jump to equipped and the list are now one tidy stack up top, and both delete tools (Delete selected and Delete entire category) moved into their own Delete items card below the list with one short note instead of two long ones.

## 1.106.0

Minecraft Dungeons II > Inventory Editor: added a Category dropdown with Delete entire category (keep equipped) - wipes every item in that category (Helmet, Melee, ...) across your whole inventory in one go, skipping whatever's currently equipped there.

## 1.105.0

Minecraft Dungeons II > No Clip & Stats > Soul Storm: rebuilt the old generic finder into two picks - an Active flag (Force start, Force stop, Keep storm active) and a Timer (Set, Freeze time) - instead of one plain select-a-field-and-set tool. The real switch and timer still aren't confirmed fields, so Scan storms and picking the right rows from the list is still needed first.

## 1.104.0

Minecraft Dungeons II > World > Item rarity drops: removed the Luck row from 1.103.0 - testing found it didn't help. Renamed Force this rarity only to Attempt force and reworded its note to be clear it's an experimental nudge, not a guarantee.

## 1.103.0

Minecraft Dungeons II > World > Item rarity drops: added a Luck row (the game's rarity bonus chance stat) with its own Set, Freeze, and a one-click Max luck button.

## 1.102.0

Minecraft Dungeons II > World > Item rarity drops: added Force [rarity] only - zeroes every other rarity's weight and maxes the one you pick, with a Keep forcing option that reapplies it automatically.

## 1.101.0

Minecraft Dungeons II > Inventory Editor: added a Rarity filter (Common, Rare, Special, Unique) to narrow the item list before picking what to edit.

## 1.100.0

Customize > Click sounds: added two more choices, Stone and Loud Click.

## 1.99.2

Minecraft Dungeons II > Nearby Viewer > Update automatically: now also re-scans the game every 6 seconds on its own, so newly loaded chests/enemies appear without a manual Refresh.

## 1.99.1

Minecraft Dungeons II > Nearby Viewer: fixed it sometimes showing/listing nothing, caused by an overly strict class-name match for enemies.

## 1.99.0

Minecraft Dungeons and Minecraft Dungeons II > Tools > Console: added a Macro button linking to Auto Dungeoneer on dungeoncollector.co.uk.

Customize > Import / export style: fixed the Import style... button rendering all dark/black.

## 1.98.1

Minecraft Dungeons > Item Editor: moved Item presets below Enchantments and shortened its note, flagging it as a test feature that doesn't always work.

## 1.98.0

Minecraft Dungeons > Item Editor: added Legacy Mode and Modern Mode - Modern Mode lists your whole inventory and storage chest straight from the game's data instead of needing an in-game highlight.

Modern Mode should work on every platform, Steam included, even where Legacy Mode's hook can't find anything.

## 1.97.0

Minecraft Dungeons > Tools > Console: Steam support is on hold for now after two attempts risked crashing the game; Minecraft Launcher and Xbox app are unaffected.

Experiments: removed the mob-summon-fix Console section - that idea is being shelved.

Home > Getting started: updated to cover both games.

## 1.96.2

Minecraft Dungeons > Tools > Console: reverted the Steam SpawnObject guess from 1.96.1 - it crashed the game on testing.

## 1.96.1

Minecraft Dungeons > Tools > Console: added a best-guess SpawnObject match for Steam 1.17.0.0 from the user's own log, picked out from 3 call-site candidates by which one looks like a real function (not a tiny field/flag check) and is only called once. Not yet confirmed by a successful test - please report back either way.

## 1.96.0

Minecraft Dungeons > Tools > Console: now tries to find the engine's object-creation function dynamically (the same technique Minecraft Dungeons II's console already uses) when the known build's hardcoded address doesn't match, instead of simply refusing. Should help on Steam - if it still doesn't connect, Refresh log and send it over so a confirmed build can be added.

Mod toggles (Minecraft Dungeons): the on-screen message no longer includes the long "how it's hooked in" technical explanation, which used to resize the message area and shift the page layout every time a mod was switched on. That text still gets logged - find it in Backup > Debug.

## 1.95.1

Experiments > Console (mob-summon fix, both Safe Test and Blind Writes): the log was silent unless a donor was both found and in use at the same moment, so a run that found nothing left no trail at all. Both DLLs now also log a line the first time they see any object's ability system and its attribute-set count, so the next test tells us something concrete either way.

## 1.95.0

Experiments: new Console: experimental mob-summon fix (Minecraft Dungeons II), with Safe Test and Blind Writes DLLs that try copying a live mob's combat attribute sets onto a console-summoned one. Untested and separate from the regular Console DLL so it can't be affected.

Customize: new App sounds section under Click sounds - an optional looping background track (Futuristic - Pursuit), off by default, volume customizable (5% default). Avoid it while recording, in case of copyright.

Minecraft Dungeons II: every page note about offline-only features now starts with "Only works in OFFLINE - ".

Changelog: shortened older multi-paragraph entries to a sentence or two.

## 1.94.1

Minecraft Dungeons II > No Cooldown: fixed Potions and Artifacts not being reusable instantly - their own cooldown-duration attributes are now zeroed too, not just charges/souls. Rolling was unaffected.

## 1.94.0

Minecraft Dungeons II > Inventory Editor: removed the Effects (enchantments) editing section - it kept risking item corruption. Item names and other fields (Rarity, Power, Level, Stack, XP, rerolls) still work as before.

## 1.93.2

Fixed a real MinGW/GCC build failure in the Inventory Editor's data catalog (`Mcd2Catalog.cpp`) caused by giant inline string/vector construction. Rewritten as small plain-data tables built via a loop - no behavior change.

## 1.93.1

Fixed a build error in the Inventory Editor's new data catalog (`Mcd2Catalog.cpp`) - a few whole-number tier/level values were written as invalid C++ (e.g. `9f` instead of `9.0f`), which failed to compile under MinGW/GCC. No behavior change once it compiles; this just makes 1.93.0's catalog overhaul actually build.

## 1.93.0

Minecraft Dungeons II > Inventory Editor: items and effects now show real in-game names (from a community data catalog) instead of raw tags. Strength and Points invested are now bounded to values the game can actually produce.

## 1.92.0

Minecraft Dungeons II > Inventory Editor > Effects (enchantments): the Effect slot list now labels each slot **(Enchant)** or **(Base roll)** - only the (Enchant) one is the item's actual enchantment slot; the others are its built-in signature ability and reroll effects, which the game itself never lets you retune. Added a short warning that editing effects can corrupt a character, since a built-in or reroll slot's Strength isn't meant to be changed and doing so risks corrupting the item.

## 1.91.1

Minecraft Dungeons II > Inventory Editor: Talisman Level is now capped at 3 - typing or scrolling higher just snaps back down to it, since 3 is the real max. Shortened the hint text under Selected item and Effects (enchantments).

## 1.91.0

Minecraft Dungeons II > Inventory Editor: item fields now apply instantly (no Apply button). Level/Item XP only show for Talismans. Effects (enchantments) got a Level field and auto-filled slots. The editor now refreshes itself automatically.

## 1.90.2

Information > Honorable mentions: added JustGaming101 to the list.

## 1.90.1

Information > Honorable mentions: replaced the whole list with Terrarings, Dweem, Calamari, Hybrid, Maz, Arno, Liaa, Moonlitfoxi, Lucas, Shadow, Inspirity and Jaded_Book.

## 1.90.0

Minecraft Dungeons > Tools: turning on a mod now shows how it actually reaches the game (patch, hook, or reflection) and the real address found. Backup Saves > Debug: removed the long folder/registry/log listing.

## 1.89.0

Minecraft Dungeons > Tools > Console: added a Close game button and Freecam & HUD commands, matching Minecraft Dungeons II's console. Removed the Dump functions tool added in 1.88.0 - it didn't lead anywhere useful.

## 1.88.0

Fixed 1.87's summoned-mob visibility fix only checking newly created objects - Unreal reuses freed object slots, so it rarely ran. It now checks every loaded object each pass, kept cheap by caching per class.

## 1.87.0

Minecraft Dungeons II > Tools > Console: summoned mobs used to appear invisible since stock summon skips the game's spawn-in logic. The DLL now makes them visible; it can't yet grant combat attribute sets, and logs how many each mob got.

## 1.86.0

Minecraft Dungeons II > Tools > Console: simplified the hint text, and added a Console key picker just like Minecraft Dungeons already has, so the console key can be changed from Tilde to F1-F12 or several other keys. The DLL now supports it too - it writes the chosen key into the game the same way the Minecraft Dungeons console does, and picks up changes within a couple of seconds.

## 1.85.0

Minecraft Dungeons II > Tools > Console: new Close game button next to Open folder. It ends every Minecraft Dungeons II app by process name (Microsoft Store/Xbox App, Steam, and the Minecraft Launcher), which is useful since closing the game normally doesn't work while the console is injected. Also cleaned up the wording on some older update notes that talked about things being untested.

## 1.84.0

Minecraft Dungeons II > Tools > Console: the freecam command group is now just toggledebugcamera (confirmed by the user to work fine on its own, so fly/ghost/walk were removed) plus a new showhud command to hide the on-screen HUD. Both are toggles - type the same command again to turn the camera or the HUD back to normal.

## 1.83.0

Minecraft Dungeons II > Tools > Console: added a Freecam & Noclip group to the command list - toggledebugcamera for a true detached flying camera, or fly/ghost/walk to make your own character fly through walls. These are built into Unreal Engine itself, so they should work the same on the Microsoft Store/Xbox build and Steam.

## 1.82.0

Minecraft Dungeons II > Streamer mode (BETA) moved out of Tools and into its own collapsible section under Account > BETA > Experiments, matching how Minecraft Dungeons II's other experimental features are organized. Same beta caveats still apply - it may not work yet.

## 1.81.1

Minecraft Dungeons II > Tools > Console: confirmed working on Steam (user-tested) - its SpawnObject code turned out to be byte-identical to the Microsoft Store/Xbox build, so no new signature was even needed. Updated the hint text to say so.

## 1.81.0

Minecraft Dungeons II > Tools > Console: it now at least attempts to start on the Steam build (previously it refused immediately, since it only recognised the Microsoft Store/Xbox exe name). Getting the console object itself made still needs the DLL to recognise the Steam build's exact compiled code, which it doesn't yet - if it refuses with "not a build this DLL knows yet", the log (DungeonKitConsole2.log) now records the exact bytes needed to add it properly.

## 1.80.1

Minecraft Dungeons II > Steam: added the real version-table entry (module size 216018944 -> 1.2.0), from a user-reported build. Home and the status bar now recognise it immediately on attach, on top of the live read added last version.

## 1.80.0

Minecraft Dungeons II: the shown version (Home and the status bar) is now read live from the game itself instead of only a fixed table, so it stays correct on Steam, Microsoft Store/Xbox, or any future patch without needing an update here. Checked every property name the Minecraft Dungeons II features use (teleports, quests, difficulty, ability system, etc.) against a fresh dump from the Steam build - all of them still match, so no mod code needed to change; the name-based lookup these already use is what makes that possible.

## 1.79.1

Fixed a build break in 1.79.0 (a stray edit had dropped the line declaring the changelog list itself, so the whole file failed to compile). No other changes.

## 1.79.0

Minecraft Dungeons II now also connects to the Steam release (same exe names as the original game). The version table only recognizes the Store build so far, so Steam may show as unrecognized - mods should still work by name/pattern.

## 1.78.3

Minecraft Dungeons > Tools > Hide my name: removed the BETA label added last version - that one isn't affected by the main-menu issue, only the Minecraft Dungeons II copy inside Streamer mode is.

## 1.78.2

Minecraft Dungeons II > Tools > Streamer mode, and Minecraft Dungeons > Tools > Hide my name: labeled BETA. Hide my name still doesn't reliably work (confirmed not working on the Minecraft Dungeons II main menu even after the 1.78.1 attempt at a fix) - still being worked on. Hide join ID and Hide version text are unaffected.

## 1.78.1

Minecraft Dungeons II > Streamer mode > Hide my name: fixed it not working on the main menu by also scanning for the player name directly, the same way Hide join ID does. Widened the list of name fields it checks.

## 1.78.0

Reserved names: once you type a reserved name's password correctly, this computer remembers it, so typing that same name again signs you in automatically with no password prompt. Works on the startup name screen and on the Account page.

## 1.77.1

Information > Credits: added JustGaming101 for providing the blueprint files used to expand the Console commands lists for both games.

## 1.77.0

Minecraft Dungeons > Tools > Console commands: added about 2,495 more commands (2,914 total) in 7 new alphabetized groups. Weapon/armor/artifact blueprints left out, as with Minecraft Dungeons II's pass.

## 1.76.0

Minecraft Dungeons II > Tools > Console commands: added about 710 more commands found in the game's own blueprint list, sorted alphabetically within each group (now 1,253 commands in the same 19 groups). Most of it is scenery, level gimmicks and mob variants; things like raw animation, weapon and armor blueprints were left out since they don't do anything useful on their own.

## 1.75.5

Languages: added translations for everything new this update (Experiments reorder, damage-per-second, Speedometer, the Supporters section and tag). Customize: the Experiments section titles you click to open or close can now be styled too (QToolButton#CollapsibleHeader), and the style-editor hint lists it.

## 1.75.4

Account > Notes: the tag note now also mentions supporters who've donated money and content creators, not just the honorable mentions.

## 1.75.3

The Content Creator reserved name is now JustGaming101 (was JustGaming), same password as before.

## 1.75.2

Information > Supporters: JustGaming101 and 51/49 are two different people, so they're back to being separate entries, filling out the whole list.

## 1.75.1

Information > Supporters: fixed a typo (51/49, not 51/47) and moved that entry to the empty spot at the end of the list.

## 1.75.0

- Account: a reserved name's role tag in the sidebar now shrinks its text a little if the name makes it too wide to fit on one line (never by more than a few pixels). Short tags like "Plushy - DEV" look exactly the same as before.
- Information: new Supporters section under Honorable mentions, crediting everyone who's supported the project.
- New Supporter tag, with its own set of reserved names and its own password, shown the same way as the other role tags.

## 1.74.0

Experiments page reorganized and made more compact.
- New order: WASD Movement, Raw damage numbers, Speedometer, Summonable things, Mod Maker, Game data (dump files) at the bottom.
- Every section can now be collapsed - click its title to open or close it. The state is remembered.
- Raw damage numbers: shorter note, and it now also shows a rolling damage-per-second reading alongside the hit list.
- New Speedometer card: shows how fast you're walking right now, in game units per second.
- Raw damage numbers and the new Speedometer both now work in Minecraft Dungeons as well as Minecraft Dungeons II (previously damage numbers only worked in Minecraft Dungeons II).

## 1.73.0

Minecraft Dungeons > No Clip & Stats > Player stats: added 30 more attributes across Melee, Ranged, Health, Resistance and Movement. Some of these names aren't confirmed to exist on Minecraft Dungeons specifically, but that's safe - an unavailable one just won't do anything rather than causing harm.

## 1.72.0

Experiments > Item Editor (overhaul): rebuilt after it caused instant crashes from writing to the wrong kind of object. Editing now goes through the same verified object the main Item Editor uses. New: all enchant levels shown at once, named pickers instead of raw numbers, presets with names.

## 1.71.1

Fixed a build error in the new Item Editor (overhaul) card.

## 1.71.0

Experiments: new Item Editor (overhaul) card, a second item editor for Minecraft Dungeons that lists equipped items without highlighting and shares presets with the main editor. Edits ID, power, rarity and gilding; enchants only via presets.

## 1.70.7

Minecraft Dungeons Item presets: Copy preset code reads the highlighted item directly (like Copy item data) instead of requiring a saved preset to be selected first. Added a crash-risk note for applying a preset across very different item types.

## 1.70.6

Minecraft Dungeons Item presets: Add preset from code now opens a paste dialog instead of reading the system clipboard directly.

## 1.70.5

Removed an incorrect line from the Backup saves debug info claiming DungeonKit never injects anything into the game.

## 1.70.4

Minecraft Dungeons II console commands: Farm & Wild Animals merged into Pets, Villagers merged with Merchants, Weapons/Armor/Artifacts & Other Gear/Enchantments & Buffs removed. 542 commands in 18 groups. Both console hint notes shortened.

## 1.70.3

Both Console cards (Minecraft Dungeons and Minecraft Dungeons II) now note that Windows Defender may flag the DLL injection and that the DungeonKit folder should be added as an exclusion if needed.

## 1.70.2

Minecraft Dungeons II console commands: 595 `summon` commands in 25 groups (Mobs, Bosses, Pets, Farm & Wild Animals, Weapons, Armor ...), merged from three scans.

## 1.70.1

Minecraft Dungeons II console commands: 402 `summon` commands in 20 groups (`ConsoleCommands2.h`), merged from both scans.

## 1.70.0

Experiments > **Raw damage numbers** (`DamageLogCard`): polls the Health attribute (ATR_Health.Health +0xC, reached through AbilitySystemComponent.SpawnedAttributes) of every loaded MobCharacter, and optionally the player, ~30x/s and logs each drop with decimals. Optional always-on-top overlay.

## 1.69.0

Minecraft Dungeons II > Tools > **Console commands**: same drop-down groups as Minecraft Dungeons, 195 `summon` commands from a real scan, plus `enablecheats 1` under Important (`ConsoleCommands2.h`). The Console commands card is now one shared function (`addCommandsCard`).

Experiments > **Summonable things**: Find more in the game scans the running game for every loaded Blueprint Actor class and lists them as `summon <name>` with Copy, search, Copy all and Clear list. Remembered between runs.

## 1.68.0
- Minecraft Dungeons II > Tools > **Console** now creates the game's console (press ~) and a cheat manager for each level, like the Minecraft Dungeons one, through the game's own `SpawnObject` function. Only for Minecraft Dungeons II 1.1.2.0; on other builds it does nothing. Use it offline or in solo play only.

## 1.67.1
- Console (test) for Minecraft Dungeons II also looks up the game's SpawnObject function and logs the code around it. Still read-only.

## 1.67.0
- Minecraft Dungeons II > Tools: Hide my name, Hide version text and Hide join ID are now one **Streamer mode** card, with an **Update automatically** switch (on by default) and an **Apply now** button.
- New **Console (test)** card for Minecraft Dungeons II. Its DLL (`DungeonKitConsole2.dll`) only looks at the game, writes a log and shows a popup. It cannot create the console yet.

## 1.66.1
- Home: rewrote the *What DungeonKit does* text.

## 1.66.0
- Minecraft Dungeons II > Tools: new **Hide version text** and **Hide join ID** cards (show, hide completely, Default, Custom). Best effort: they find the game's on-screen text widgets and collapse or rewrite them, but the game may not redraw them.

## 1.65.0
- Hide my name is now also in Minecraft Dungeons II > Tools, under Pause Game (show, hide completely, Default (Player), Custom, and the lobby-list option).
- Hiding the version text and the join ID is not built yet.

## 1.64.2
- Console commands: the Important category is always open. The others still start closed.

## 1.64.1
- Console commands: every category starts closed, and is closed again each time DungeonKit starts.

## 1.64.0
- New **Hide my name** card in Minecraft Dungeons > Tools, under Pause Game. Choose: show your real name, hide it completely, Default (Player), or Custom. A checkbox also hides everyone else's names (lobby list).
- It overwrites the name text in the game's memory, so it only changes what your own game shows. Other players still see your real name. Menus that already copied your name may only update after a level loads. A short real name limits how long the replacement can be.

## 1.63.0
- Experiments: removed the UUU card and the bundled UUU files. The Console in Minecraft Dungeons > Tools replaces it.

## 1.62.2
- Console: note that the game can't be closed the normal way while injected. Close the game process from Home.

## 1.62.1
- Console commands: note that they only work in your camp, and other players can join and see the same things.

## 1.62.0
- Console commands: search box, no more Copy group buttons, note about typing enablecheats 1 only once.
- New command lists: Bosses, Mobs, Pets & Animals, Effects & Projectiles, Players & Other (about 390 commands, sorted by name, so a few may be in the wrong list).

## 1.61.0
- Console: button fixed, new console key picker (default ~), commands grouped (Important, Chests, Merchants) with Copy group.
- Removed the Probe DLL. Advanced (dumps) moved to the bottom of Tools.

## 1.60.0
- The Console moved to Minecraft Dungeons > Tools, under Advanced, with a shorter note.
- New list of console commands with Copy buttons.

## 1.59.0
- Console DLL shows a popup in the game when it is connected, or when it could not start.
- Console DLL makes a cheat manager for each level, so slomo and summon BP_DeluxeChest_Hidden_c may work.

## 1.58.0
- Experiments (Minecraft Dungeons): new **Console DLL (test)** card. The probe log from the Xbox app / Minecraft Launcher build (1.17.0.0) showed the game has no console object (`ViewportConsole` is empty) although the console key is set. **Inject console DLL** loads `DungeonKitConsole.dll`, which creates one with the game's own Console class and stores it in the game viewport, the way the engine itself does outside shipping builds. After that, `~` (Tilde) opens the console. The log is shown in the card.
  - It hooks nothing and patches no code. It calls the engine's object-creation function, whose address is known for 1.17.0.0 and is checked against a call site before use; on any other build it stops and changes nothing.
  - Many console commands are removed from the shipping game, so some will do nothing.
  - Inspired by the Universal UE4 Unlocker by Otis_Inf.

## 1.57.0
- Experiments: removed the built-in UUU injection card, which crashed the game. The *Camera unlocker (UUU), use the UUU app* card stays.
- Experiments: new **Probe DLL (test)** card. **Inject probe DLL** loads `DungeonKitProbe.dll` (built with DungeonKit, in the `Probe` folder next to the exe) into the running game. The DLL only reads: it hooks nothing, changes nothing and calls no game code. It writes `DungeonKitProbe.log`, which the card shows: whether the engine was found, the game viewport, whether a console already exists, whether the Console class exists, and the console keys in the input settings. It is a first step toward our own console DLL, inspired by the Universal UE4 Unlocker by Otis_Inf.

## 1.56.1
- Websites: added Dungeon Collector for Minecraft Dungeons II under the main Dungeon Collector, and the DungeonKit Discord server at the top of the Discord servers.
- Websites: fixed the buttons looking super dark.

## 1.56.0
- New **Content Creator** tag (reserved names plus a password, theme coloured like the other tags).
- Websites page redesigned: each site has a round logo of the same size, its address under the name, and a theme coloured button.
- Minecraft Dungeons II > Inventory Editor: shorter, simpler note at the top.

## 1.55.3
- Getting started: added a short line and an **Open Information** button that jumps to the Information section.

## 1.55.2
- Minecraft Dungeons FAQ and Getting started corrected: DungeonKit no longer attaches on its own, so they now tell you to choose Minecraft Dungeons under Target game on Home. New FAQ entry for "Open, no game connected". Updated the troubleshooting, supported platforms and gifting answers.

## 1.55.1
- Camera Editor (both games): the presets no longer change or freeze the camera distance (arm length), which could glitch the camera. They only set the field of view and default field of view. You can still set the distance yourself.
- Minecraft Dungeons > No Clip and Stats: removed "Treat them as experimental" from the note.

## 1.55.0
- Experiments (BETA), Minecraft Dungeons only: two new cards above the Mod Maker for the **Universal UE4 Unlocker (UUU)** by Otis_Inf, which gives a free camera, pause, slow motion and a console.
  - *Camera unlocker (UUU), built in*: DungeonKit comes with UUU (the `UUU` folder next to `DungeonKit.exe`) and loads `UniversalUE4Unlocker.dll` into the game for you with **Inject UUU**. Load into a level first. Insert = free camera, Numpad 0 = pause, Page Down = slow motion, `~` = console.
  - *Camera unlocker (UUU), use the UUU app*: **Run UuuClient** starts the bundled UuuClient.exe (or one you choose), **Open UUU folder** opens the folder.
  - The UUU files live in `third_party/UUU` in the source and are copied beside the exe when you build. UUU is a third-party tool, bundled unchanged. Offline use only. Antivirus programs may flag any DLL injection.

## 1.54.0
- Camera Editor (both games): removed the Minecraft, Close Up and Wide Angle presets.
- Minecraft Dungeons > Camera Editor: new **Photo mode** card (World speed, Freeze time, Slow motion, Normal speed), like Minecraft Dungeons II. It reaches the game through the same object tables as the rest of the Camera Editor, so it is meant to work on Steam, the Xbox App and the Minecraft Launcher. If Timer or Pause Game is switched on in Tools, they fight over the same value.

## 1.53.2
- Item presets: the instructions now include how to share a preset.

## 1.53.1
- Item presets: shorter, simpler instructions.

## 1.53.0
- Minecraft Dungeons > Item Editor: **Item presets** rebuilt as a saved list. Press *Save item as preset* with an item highlighted, and the preset stays in DungeonKit. Pick it later and press *Apply preset* on any highlighted item. It uses exactly the same code as Copy / Load item data (everything except stats), and checks the result. Presets can be deleted, and shared with *Copy preset code* / *Add preset from code*. The file save and import buttons were removed.

## 1.52.0
- MCD1 Item Editor: presets and Copy/Load item data share one code path, include enchant levels, and are verified after loading.

## 1.51.1
- Build fix for Item presets (a variable named slots clashed with a Qt keyword).

## 1.51.0
- MCD1 Item Editor: Item presets (save/import as .dkitem file or DKITEM1 code; everything except stats) and a Cloned item row.

## 1.50.0
- MCD2 No Clip and Stats laid out like Minecraft Dungeons (No Clip, God Mode and Overkill in one Movement card). MCD2 Player: Enemies Ignore You (experimental).

## 1.49.0
- MCD2 No Clip and Stats: stats split into groups (General combat, Melee, Ranged, Artifacts, Elemental damage, Movement and rolling, Defense, Healing, Companions and status) with about 100 new stats such as attack speeds.

## 1.48.1
- MCD2: region shown next to the map (Sift), mission state editing / force complete, camera presets fixed.

## 1.48.0
- Minecraft Dungeons II > World: new **Item rarity drops** card (loot attributes plus the game's rarity weights and chances).

## 1.47.2
- Home > Status stays in English (fixes flicker when another language is selected). Experiments has a clearer warning: no guides or help are planned for these features.

## 1.47.1
- The DEV, Assistant and Elite tags use the theme colours (same passwords). Minecraft Dungeons II > Inventory Editor: Merchant prices moved to the bottom. New Elite names: Maz, Terrarings, Tomcat.

## 1.47.0
- Minecraft Dungeons II > Inventory Editor: new **Merchant prices** card. Whatever you spend on a purchase (emeralds, spring stones, enchantment points) is partly or fully given back, using the percentage you set.

## 1.46.2

- Fixed a build error in the Mod Maker.

## 1.46.1

- Experiments: the Mod Maker and the game data buttons now sit at the top of the page, above WASD Movement. The page scrolls.

## 1.46.0

- **Experiments > Mod Maker**: browse the live game's objects and properties (the same data as the class dump) and make your own mods. Pick a root (player character, controller, world, world settings ...) or find any object by class name, click a number or on/off property, then **Set once** or **Save as mod**. A saved mod holds its value while ticked, starts switched off after every restart, and can be shared as a short code (Copy code / Paste code). Only works in offline play; a wrong value can crash the game. Untested.
- **Experiments > Game data**: buttons to save the Names, Classes and fields, and Objects dump files to the Desktop, plus a button that opens the Desktop folder.

## 1.45.1

- Fixed the status card at the bottom of the sidebar flickering between English and the chosen language. It now stays in English.

## 1.45.0

- **Account > BETA**: a new switch that adds an **Experiments** tab under Home, for features that are still being worked on. It is always off when the app starts. WASD Movement moved from Account to that tab. Turning BETA off also switches WASD off.

## 1.44.0

- Minecraft Dungeons > Item Editor > Merchant prices: now covers gold as well as emeralds, with the same percentage. Untested.

## 1.43.3

- Minecraft Dungeons > Item Editor > Merchant prices: shorter, simpler text.

## 1.43.2

- Minecraft Dungeons > Item Editor > **Merchant prices**: remade again, since the game stores no price/discount value - it now refunds the part of a purchase you chose not to pay, through the Currency Editor. Untested.

## 1.43.1

- Minecraft Dungeons > World > Current mission: the Scaling list is removed again.
- Minecraft Dungeons > Item Editor > **Merchant prices**: remade. The game ships the shop price multiplier switched off (x1, starting at level 999), so the card now switches it on for every level with the price you pick. It shows the original values and has a **Freeze** checkbox that keeps the price. Untested; Disable all puts the original back.

## 1.43.0

- Minecraft Dungeons > World > Current mission: the mission screen multipliers now use the live widget (the earlier version picked the wrong copy) and read from two places. If the text cannot be read, the note shows counts you can send back.
- New **Scaling** list in the same card: the game's own difficulty table (enemy health, damage, speed, endurance = stagger resistance, pushback, shop prices ...) from the lowest to the highest Apocalypse level.
- Minecraft Dungeons > Item Editor: new **Merchant prices** card. Presets Free, 25%, 50%, 75%, Normal, plus a custom percentage. It edits the shop price row of the game's difficulty table, so it behaves the same on Steam, the Xbox App and the Minecraft Launcher. Untested; Disable all puts the original price back.

## 1.42.2

- Minecraft Dungeons > World > Teleports: shorter note at the top about mission teleports.

## 1.42.1

- Minecraft Dungeons > World > Current mission: the mission screen multipliers are now read automatically (no button). Open a mission on the map and they appear within a few seconds. Untested.

## 1.42.0

- Minecraft Dungeons > World > Current mission: new **Read multipliers from the mission screen** button. Open a mission on the map (where you pick the threat level), press it, and the list the game shows there is displayed. It only works while that screen is open.

## 1.41.2

- Minecraft Dungeons > World > Teleports: the Teleport button is gone; double-click a location to teleport.

## 1.41.1

- Minecraft Dungeons > World: the explanatory text at the top is gone.
- **Current mission** (and **Tower**) showed only dashes. The class dump shows `mReplicatedLevelSettings_ONLY_FOR_INITIAL_REPLICATION`, `mReplicatedEmergentDifficulty` and `mReplicatedTowerMissionState` are fields of the `GameBP` actor (an `Info`), not of the game mode the page was reading. The page now finds `GameBP` through the player character (`BP_PlayerCharacter_C` has a `GameBP` pointer at +1248), and if that fails by scanning the level's actors for a `GameBP` / `LobbyBP`. Because the actor also exists on clients, the "not the host" limit is gone.
- Untested. If the section is still empty, copy the line at the top of the page ("Level found...") and tell me what it says.

## 1.41.0

- Minecraft Dungeons: the **Teleports** page was moved into the top of **World** (one less sidebar button). Saved sidebar layouts are updated automatically. The Position / Teleport switch and the mission list work exactly as before.
- New **My locations** group in that list: type a name and press **Save my position** while in the game, then double-click it (or press Teleport) to come back. **Rename** and **Delete** manage them. They are stored in the app's settings (`teleports/custom`) and load again on the next start.
- The same warning applies as for the mission list: levels rotate and change, so a saved spot only lines up when the level is the same.

## 1.40.1

- Minecraft Dungeons II > Tools: the pinned note now uses the Minecraft Dungeons Tools wording, plus a sentence that the tools were used to test whether DungeonKit could connect to Minecraft Dungeons II and work with its data.

## 1.40.0

- Customize > **Click sounds**: No click sounds (default), Soft Click, Futuristic or Combined, plus a **Volume** slider. Every enabled button plays the sound when pressed (radio buttons play a preview of the chosen sound instead). The choice and the volume are saved (`sound/mode`, `sound/volume`) and come back on the next start.
- The three sounds are built into the exe (mono 16-bit WAV in `resources/sounds`, converted from the supplied mp3 files). They play through Windows `PlaySound` from memory with the volume applied to the samples, so no extra Qt module is needed (the project now links `winmm`). A new click cuts off the previous one.

## 1.39.2

- WASD Movement: all four directions were inverted (D moved you left). The signs are swapped in both games.
- Minecraft Dungeons: while a key is held it also writes the player controller's `mLastMoveX` / `mLastMoveY` (the last stick input, which the game's camera and facing read), in case the camera needs it to follow. In Minecraft Dungeons II the dump shows no such field, so only the pawn input is written there.
- If the camera still does not follow in 1.39.1 or later, tell me what it does (stays put, lags behind, or snaps back).

## 1.39.1

- WASD Movement was rewritten and no longer shares code with No Clip. It writes the pawn's **ControlInputVector** (the class dumps show it in both games: floats at +0374 in Minecraft Dungeons, doubles at +02F0 in Minecraft Dungeons II) while a key is held. The game's movement component reads that as normal walking input, so gravity, walls and walking speed still apply.
- The Speed box is gone (walking speed is the game's own). **Direction offset** stays.
- It can only work if the game does not clear that vector before reading it; it is written every 8 ms to make that likely. Untested.

## 1.39.0

- Account > **Experimental**: a new card for features that are still in testing and may change or be removed.
- First feature: **WASD Movement**, one switch for Minecraft Dungeons and one for Minecraft Dungeons II. While on and the game window is active, W A S D glide your character (same axes as the original table's No Clip: W = -X, S = +X, A = +Y, D = -Y). **Speed** and **Direction offset** (degrees) are shared and remembered. Walls still block you (collision is not touched). It does nothing while No Clip is on. Offline only. The Minecraft Dungeons II switch turns off with Disable all mods and when the game closes.
- Untested in game. The movement works by writing the character's position many times a second, so the game may fight it, and if the game already moves you with WASD you will move faster than normal.

## 1.38.1

- Account: the Rich Presence status line shows the DungeonKit username you are logged in as, and updates when you change it.

## 1.38.0

- Minecraft Dungeons II: Inventory Viewer, World, Nearby Viewer and Inventory Editor have a **Turn on** switch in front of Refresh. Off = nothing is read and the controls are disabled. On = the game is scanned and the lists fill. Refresh only reloads the page. The switches turn off when the game closes.

## 1.37.3
- Minecraft Dungeons II > No Clip & Stats: the top card is now **God Mode & Overkill**, and both descriptions sit under their switches.

## 1.37.2
- Minecraft Dungeons II > Overkill: the description is now under the switch (same look as the Minecraft Dungeons switches). It is a plain switch and never needs Refresh.

## 1.37.1
- Minecraft Dungeons II > No Clip & Stats: new **Overkill** switch in the top card, under God Mode. While on, it keeps your base, melee, ranged and artifact damage multipliers at 1,000,000 (re-applied after level changes). Switching it off, closing the game or "Disable all mods" puts the game's own values back. Offline only.

## 1.37.0
- Minecraft Dungeons > No Clip & Stats: new **Overkill** switch under God Mode. While on, it keeps the melee and ranged damage multipliers (and the affector DealDamageMul, when the character has one) at 1,000,000 so nearly everything dies in one hit. Switching it off writes back the values the game had.
- Found by name through the object tables, so it is meant for Steam, the Xbox App and the Minecraft Launcher alike. Untested in game; bosses with damage caps or immunity phases may still survive.

## 1.36.2
- The honorable mention tag is now called **Elite** (it was Helper, briefly Sage in a build).
- New **Assistant** tag with a blue gradient (#005c99 to #48dae7). Moonlitfoxi moved from DEV to Assistant. It uses the same password as DEV.

## 1.36.1
- Backup Saves: the Back up now button now uses the same style as the buttons next to it.
- The Helper tag is now called **Sage** (same yellow-to-gold look).
- Account > Notes: honorable mentions are told to contact Plushy on Discord to claim their tag.
- Discord Rich Presence stays off until you switch it on, and once on it stays on across restarts.

## 1.36.0
- Discord Rich Presence (Settings > Account > Discord). Off by default. When on, your Discord profile shows "DungeonKit vX.Y.Z" and either "Connected to <game> (vX)" or "Open, no game connected", with the time the app has been open.
- It talks only to the Discord app on the same computer (its local pipe) using the public application ID. No token, webhook or account link is used, and no items, stats or mods are shown.
- Optional: upload a Rich Presence art asset named `logo` in the Discord developer portal to show a picture.

## 1.35.0
- Backup Saves now covers Minecraft Dungeons II as well as Minecraft Dungeons, with a Choose folder button for each game.
- New option: also back up the ~mods folder (found from the running game or the usual install locations).
- Each backup is one folder per game, named like `Minecraft Dungeons - 2026-10-12 12.30 - v1.17.0.0`, with a short `Backup info.txt` inside (game, game version, date, DungeonKit version, what was copied). The version is left out when the game is not running.
- The Minecraft Dungeons II save locations are best guesses (Saved Games, AppData, Xbox app packages); whatever exists on your computer is used, and Choose folder covers the rest.
- Fixed the Back up now button (and other accent buttons) looking dark on dark themes: the text colour now follows the theme.
- Debug text: removed the "NOT TOUCHED" line and the "only read, never changed" wording.

## 1.34.0
- Minecraft Dungeons II > Nearby Viewer: radius presets Normal (6,000), Far (750,000) and Super far (3,000,000); the radius box now goes up to 5,000,000.

## 1.33.2
- Minecraft Dungeons II: simpler notes under Selected item, Effects, Threat level and difficulty, Waypoints and Missions (the power note stays). The Missions card is no longer marked as a test.

## 1.33.1
- Home > Target game: the selected game now shows a dot in your theme colour instead of a black one.
- Home > What DungeonKit does: new text.

## 1.33.0
- Minecraft Dungeons II Inventory Editor: effects are now much more detailed.
  - Every effect on an item is listed, including the ones unlocked at each item level (shown as L1, L2 ...), not only the item's own.
  - New fields: Item XP, effect rerolls used and the item's properties (read-only).
  - Buttons to set one strength or one points value on every effect of the item at once.
  - Option to list every effect and enchantment the game has, not only the ones on your items.
  - An effect is re-checked against the item right before it is written, so a stale list cannot hit the wrong data.

## 1.32.0
- **Minecraft Dungeons > Camera Editor**: field of view, default field of view and camera distance, each with Set and Freeze, and seven presets (FOV Far, FOV Super, Quake Pro, Minecraft, Close Up, Tactical, Wide Angle). The presets scale your normal camera, so they work whatever the game's own values are. Reset to original puts everything back.
- **Minecraft Dungeons > World**: live mission read-out (lives, enemies spawned/defeated, bosses, parts, team power), current mission info (level, difficulty, threat, Apocalypse+ level, seed, raid difficulty), tower info (id, floors, seed, players, rewards, floor list) and a best-effort list of unlocked merchant types and save-state count.
- **Minecraft Dungeons > Player > Enemies Ignore You**: clears your character's `targetable` flag so enemies stop aiming at you. Experimental.
- All three read the game through its object tables by name (offsets are identical on Steam and Xbox/Launcher), so none depends on a code pattern. None has been tested in game yet.
