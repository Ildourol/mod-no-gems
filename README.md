# mod-no-gems

A hardcoded server-side Lua module for AzerothCore (`mod-ale` / Eluna) that suppresses gameplay effects of socketed gems and socket bonuses, blocks new gem insertion, and provides a persistent randomized replacement affix system for gear containing sockets with in-game tooltip visibility.

## Features

- **Gem Suppression**: While enabled, all inserted gems (Red, Blue, Yellow, Purple, Green, Orange, Meta, Simple, Prismatic, JC-exclusive, custom) grant zero stats, auras, procs, or combat ratings. Original gems remain persisted on items in the database.
- **Socket Bonus Suppression**: Socket matching bonuses are deactivated.
- **Socketing Blocked**: Rejects packet-level socketing attempts (`CMSG_SOCKET_GEMS` opcode `0x347`) gracefully with a player notification.
- **Zero Configuration File**: All rules, budgets, and stat mappings are hardcoded directly in the Lua module.
- **Reversibility**: Disabling the module restores normal AzerothCore socketing and gem effects immediately without any database restoration scripts.
- **Persistent Replacement Affixes**:
  - Eligible socketed items receive one-time randomized replacement stat affixes based on their socket count and colors.
  - Authentic stat budgets modeled on Blizzard's original TBC & WotLK gem itemization and progression tiers (Tiers 1 to 5).
  - Socket color themes:
    - **Red**: Strength, Agility, Attack Power, Spell Power, Armor Penetration, Expertise.
    - **Yellow**: Critical Strike Rating, Haste Rating, Hit Rating, Defense Rating, Resilience Rating, Intellect.
    - **Blue**: Stamina, Spirit, Dodge Rating, Parry Rating.
    - **Meta**: High-budget primary stats, Attack Power, Spell Power, Stamina, Crit.
    - **Prismatic**: Any non-meta stat.
  - Persisted permanently by item GUID in module-owned character database tables (`mod_no_gems_item_affix` and `mod_no_gems_item_affix_effect`).
  - Affixes survive logout, server restart, trade, mail, auction, and banking without rerolling.
- **Tooltip Visibility**:
  - Companion client addon (`mod-no-gems-tooltip`) displays the randomized socket replacement affixes directly on item hover tooltips in gold/green text.
  - Server transmits affix information seamlessly via hidden addon messages (`MOD_NOGEMS`).
- **Inspection Command**: Players can use `.nogems item` to view all replacement affixes on their equipped items in chat.
- **Jewelcrafting Preserved**: Recipes, prospecting, item crafting, and trade remain fully functional.

## Installation

### 1. Server-Side Lua Script
The script is located at:
```text
lua_scripts/mod-no-gems/mod-no-gems.lua
```
In your AzerothCore server directory, ensure this script is placed in `Server/bin/lua_scripts/mod-no-gems/mod-no-gems.lua` (loaded automatically by `mod-ale`).

### 2. Client-Side Tooltip Addon (Optional but Recommended)
To view affixes directly on item tooltips in-game:
Copy `client-addon/mod-no-gems-tooltip` into your WoW 3.3.5a client's `Interface/AddOns/` folder:
```text
World of Warcraft/Interface/AddOns/mod-no-gems-tooltip/
  mod-no-gems-tooltip.toc
  mod-no-gems-tooltip.lua
```

### 3. Database
The database tables are automatically verified and created by the script on startup in the characters database (`acore_characters`). The SQL definition is also available in:
```text
data/sql/db-characters/base/mod_no_gems_characters.sql
```
