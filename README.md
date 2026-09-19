# mod-no-gems

A standalone AzerothCore module that suppresses gameplay effects of socketed gems and socket bonuses, blocks new gem insertion, and provides an optional one-time randomized persistent replacement affix system for gear containing sockets.

## Features

- **Gem Suppression**: While enabled, all inserted gems (Red, Blue, Yellow, Purple, Green, Orange, Meta, Simple, Prismatic, JC-exclusive, custom) grant zero stats, auras, procs, or combat ratings. Original gems remain persisted on items in the database.
- **Socket Bonus Suppression**: Socket matching bonuses are deactivated.
- **Socketing Blocked**: Rejects packet-level and server-level socketing attempts gracefully with player notification.
- **Reversibility**: Disabling the module restores normal AzerothCore socketing and gem effects immediately without any database restoration scripts.
- **Persistent Replacement Affixes**:
  - Eligible socketed items receive one-time randomized replacement stat affixes based on their socket count and colors.
  - Derived dynamically from installed DBC gem and enchantment data.
  - Sockets yield level-appropriate stat power matching item progression.
  - Persisted permanently by item GUID in module-owned character database tables (`mod_no_gems_item_affix` and `mod_no_gems_item_affix_effect`).
  - Affixes survive logout, server restart, trade, mail, auction, and banking without rerolling.
- **Jewelcrafting Preserved**: Recipes, prospecting, item crafting, and trade remain fully functional.
- **Inspection Command**: Players can use `.nogems item` to view the replacement affixes on their equipped items.

## Installation

1. Place or clone this directory into your AzerothCore `modules/` folder:
   ```bash
   git clone https://github.com/.../mod-no-gems.git modules/mod-no-gems
   ```
2. Re-run CMake and recompile your core.
3. Import the SQL table definition from `data/sql/db-characters/base/mod_no_gems_characters.sql` into your characters database (the module also automatically verifies/creates the tables on startup).
4. Copy `conf/mod_no_gems.conf.dist` to `mod_no_gems.conf` in your worldserver configuration folder and customize settings as desired.

## Configuration

| Option | Default | Description |
|---|---|---|
| `NoGems.Enable` | `1` | Master toggle for the module |
| `NoGems.DisableGemEffects` | `1` | Suppresses stats/auras from socketed gems |
| `NoGems.DisableSocketBonuses` | `1` | Suppresses matching socket bonuses |
| `NoGems.BlockSocketing` | `1` | Prevents inserting/socketing new gems |
| `NoGems.Replacement.Enable` | `1` | Enables one-time randomized socket replacement affixes |
| `NoGems.Replacement.AllowProfessionExclusive` | `0` | Allow JC-exclusive gems (e.g. Dragon's Eye) in replacement pool |
| `NoGems.Replacement.MinItemQuality` | `2` | Minimum item quality eligible for affixes (2 = Uncommon) |
| `NoGems.Replacement.MinItemLevel` | `1` | Minimum item level eligible for affixes |
| `NoGems.Replacement.MaxItemLevel` | `0` | Maximum item level eligible for affixes (0 = no limit) |
| `NoGems.DebugLogging` | `0` | Enables debug log output |
