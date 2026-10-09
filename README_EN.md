# ⚔️ Metin2 Playerbots — Modification Pack (mt2009 SP+)

[Polski (README.md)](README.md) | **English**

[![Discord](https://img.shields.io/badge/Discord-Join_the_community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.com/invite/vGE3T9gpm)
[![Website](https://img.shields.io/badge/WWW-metin2sp.pl-C8102E?style=for-the-badge&logo=googlechrome&logoColor=white)](https://metin2sp.pl/)
[![Wiki](https://img.shields.io/badge/Wiki-metin2sp.pl%2Fwiki-2E7D32?style=for-the-badge&logo=wikipedia&logoColor=white)](https://metin2sp.pl/wiki)
[![Support the project](https://img.shields.io/badge/Support_the_project_–_buy_a_coffee-1E88E5?style=for-the-badge&logo=buymeacoffee&logoColor=white)](https://buycoffee.to/mt2009plus)

A local Metin2 singleplayer world full of autonomous characters (Playerbots),
built on Tieru's official **Metin2 Playerbots** release (mt2009 server files,
version 2.2.0) — **extended with systems the official release does not have**:
costumes, hairstyles, weapon skins, sashes, mounts, pets and alchemy
(Dragon Soul), over 2,200 new items in total.

The bots behave exactly as in the official release: they level up, fight solo
and in parties, loot, refine gear at the Blacksmith, hunt Metin stones and
trade with each other. This pack adds new items and systems to that world,
plus a few bot improvements.

## 💬 Community

- **[Discord](https://discord.com/invite/vGE3T9gpm)** — help, bug reports, ideas and news about the pack.
- **[metin2sp.pl](https://metin2sp.pl/)** — project website.
- **[Wiki](https://metin2sp.pl/wiki)** — guides, FAQ, items and systems of MT2009 PLUS (in Polish).
- **[Support the project – buy a coffee](https://buycoffee.to/mt2009plus)** — if you would like to support the pack's development.

> [!IMPORTANT]
> You need **this pack's client** (with the new costumes, mounts, pets and
> items). Tieru's official client will not show the new items. This repository
> contains no game files — the full package (client + server) is under
> **Releases**, see [SERWER_PL.md](SERWER_PL.md).

---

## ✨ What this pack adds

The numbers below are counted from the ItemShop offers shipped with the pack
(`linux-port/docker/mariadb/playerbot/mod/`).

### 👘 Costumes and hairstyles (Gameforge 26.1.11)
- **860 costumes** (432 male and 432 female versions) — Dragon Knights,
  Valkyries, Desert Warriors, Light Bearer costumes and more.
- **848 hairstyles** ("Fryzury +" category) — helmets, masks, turbans, ears and diadems.
- Timed variants: 1 day, 7 days, 30 days.

### 🗡️ Weapon skins
- **109 skins** for every weapon type — swords, daggers, bows, glaives, bells,
  fans (Northern Dragon, Curse Bearer series and more).

### 🎀 Sashes
- Full sash system: **combining** two sashes into a stronger one and
  **absorbing bonuses** from an item into a sash — at **Uriel**.

### 🐎 Mounts
- **About 240 mount seals** (129 models) as mount costumes, with fixed
  movement speed; 136 different mounts in the ItemShop.
- Skills can be used on boards, clouds and boats.

### 🐾 Pets
- **190 pets** — one at a time, bonuses from the item, back after relogging.
- **11 "(łup)" loot pets** pick up items and yang by themselves (yours only).

### 🐉 Alchemy (Dragon Soul)
- Dragon Soul system from level 30 at the **Alchemist**: qualification quest,
  fragment farming, refining (grade / step / strength), attribute change and
  a material shop.

### 🎨 Costume bonuses
- **Transform Costume** (70063) — rolls 1–3 bonuses.
- **Enchant Costume** (70064) — changes bonus values.
- **Bonus Transfer** (70065) — at the Blacksmith, moves bonuses from one
  costume to another of the same kind (costume / hairstyle / weapon skin).
- All sold by the General Store saleswoman.

### 🛒 Shops and extras
- **Costume shop at Ah-Yu** — costumes, hairstyles and weapon skins for 1 yang.
- **In-game ItemShop** and **web ItemShop** with new categories (Hairstyles +,
  Costumes, Weapon skins, Pets, Mounts) — the web shop filters items by the
  character's class and gender.
- Lord of Death set, Diadems, Magma Manni.
- Trash bin, monster drop preview, Easter event toggle in the panel.

### 🤖 Bot changes
- Smoother server performance with many bots (per-tick time budget).
- Bots keep a stock of potions 27101/27104 instead of selling them in stalls.
- Bot "retirement" from the panel — replacing old bots with new ones.

### ⚙️ Other
- The server accepts the client regardless of its version.
- Updates come from **this repository**, never from the official one —
  an official package would overwrite the modifications ([AKTUALIZACJE_MOD.md](AKTUALIZACJE_MOD.md)).

Full change list (Polish): [MODS_PL.md](MODS_PL.md).

---

## 🌟 Bot features (from the official release)

Bots are **full player characters driven by AI inside the server engine** —
players see their natural movement, attack animations, skills and equipment
through the normal game protocol.

- ⚔️ **Combat**: every class (Warrior, Sura, Ninja, Shaman), combos, bows with arrows, buffs and skill rotations.
- 🗺️ **A\* navigation**: a collision grid built from map attributes — bots avoid mountains, rivers and walls.
- 🚪 **Map travel**: M1/M2/M3, Monkey Dungeon, Orc Valley, Yongbi Desert, Mount Sohan, Hwang Temple, Spider Dungeon, Doyyumhwaji.
- 🏹 **Quests**: hunting quests, the Biologist including Orc Tooth and Soul Stone, Horse Medal runs.
- 💎 **Metins and bosses**: Metin hunters, parties against the Orc Chief, Spider Queen and Fire King.
- 🎒 **Economy**: loot, better gear, potions, Blacksmith refining, stalls and bot-to-bot trade.
- 👥 **Parties and guilds**: 2–8 player parties, shared grinding, guild wars.
- 🎣 **Fishing** and bonus rerolling.
- 🧠 **Personality**: every bot has a character and ambition that decide what it does.
- 🎛️ **Live panel**: bot map, profiles with equipment, goal sliders and settings without restart.
- 💾 **Persistent saves**: every bot has its own account and character in MariaDB.

**All three kingdoms** are supported (Shinsoo, Chunjo, Jinno).

---

## 🚀 Installation (Windows)

1. Install and start **[Docker Desktop](https://www.docker.com/products/docker-desktop/)**.
2. Download the full package from **Releases** and unpack it, e.g. to
   `C:\Metin2Mod\` (a path without spaces or special characters is safest).
3. Run **`Metin2-Launcher-GUI.bat`**.
4. Click **1. INSTALUJ / PRZYGOTUJ**, then **2. GRAJ**.
   The first start takes a while (tens of minutes): Docker builds the server
   from source and creates the bot database.

Two browser panels run after start:
- `http://127.0.0.1:7788` — admin panel and bot map,
- `http://127.0.0.1:7790` — Metin2 Singleplayer Panel (seban latino): live map, profiles, rankings, economy.

**Linux / VPS:** see [PACZKA_INFO.txt](PACZKA_INFO.txt); update from the server
folder with `sh linux-port/tools/update.sh`.

Passwords (database, panel) are generated on first start and saved in
`linux-port\docker\.env` — on your computer only. Never share them.

## 🗄️ Database access

MariaDB is reachable locally only: `127.0.0.1`, port `3306`.
The launcher's **DANE DO BAZY (NAVICAT)** button shows host, port and passwords.
On `1045 - Access denied` use **NAPRAW DOSTĘP DO BAZY** — characters, items
and bots stay untouched.

## 🎮 GM commands

| Command | Description | Example |
|---|---|---|
| `/bot_spawn <id> <kingdom 1-3>` | Spawn a specific bot (`1` Shinsoo, `2` Chunjo, `3` Jinno). | `/bot_spawn 4 2` |
| `/bot_despawn <id>` | Remove a bot from the world. | `/bot_despawn 4` |
| `/bot_spawn_many <start_id> <count> <kingdom>` | Mass spawn. | `/bot_spawn_many 4 350 2` |
| `/bot_despawn_many <start_id> <count>` | Mass despawn. | `/bot_despawn_many 4 350` |
| `/bot_rank` | Bot level ranking (everyone). | `/bot_rank` |
| `/acce c` / `/acce a` | Sash combine / absorb window. | `/acce c` |

## 💾 World backup

The launcher's **KOPIA ŚWIATA** button saves the whole world to a zip file.
"Stop and save" never deletes characters or bot progress.

---

## 📚 Documentation

- [MODS_PL.md](MODS_PL.md) — what the modification pack changes (Polish).
- [AKTUALIZACJE_MOD.md](AKTUALIZACJE_MOD.md) — how updates work and are released (Polish).
- [docs/INSTALL_EN.md](docs/INSTALL_EN.md) — Docker, WSL2, Linux, `.env`, client.
- [docs/DEVELOPMENT_EN.md](docs/DEVELOPMENT_EN.md) — building, debugging and AI logs.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — bot architecture.
- [docs/ATTRIBUTION.md](docs/ATTRIBUTION.md) — project origin and licenses.

## 📜 License

MT2009 PLUS is a modification of Metin2 Playerbots (Tieru) and, from the
version based on Tieru 2.2.17, is shared under **CC BY-NC-SA 4.0**
(Attribution – NonCommercial – ShareAlike), see [LICENSE](LICENSE). You may
copy, share and modify the pack if you credit the authors, do not make money
from it and share a modified version under the same licence. Older parts under
MIT: [LICENSE-MIT.txt](LICENSE-MIT.txt). The game server code, game data and
client belong neither to the project authors nor to us – read
[NOTICE.md](NOTICE.md) before redistributing.

Metin2 Playerbots - Tieru - https://github.com/TieruYT/metin2-playerbots - CC BY-NC-SA 4.0

---

## 🤝 Credits and the original project

This pack is a modification of **Metin2 Playerbots** by **Tieru**, which is the
core of MT2009 PLUS. All additional changes are made by
**ZAXEP/SIZOWSKI**.

- **Original project (GitHub):** [TieruYT/metin2-playerbots](https://github.com/TieruYT/metin2-playerbots)

And the authors and helpers the official release builds on:
- **AzzlackSyndicate** — author of the original Linux port base, installers and panel. The source repository is currently private; Git history and full attribution are preserved.
- **OskarPWA** — the bot storage window and skill icons on the page come from the panel they built and shared for porting.
- **seban latino** — author of Metin2 Singleplayer Panel (`linux-port/docker/seban-panel`), the second panel in this install: live map, profiles, rankings, economy, telemetry and bulk item grants.
- **Iwakura** — help with shop pricing and naming systems, bot nicknames and item value algorithms.
- **ĹŌŞƬĒĶ** — the new client login screen (since 2.0.6): animated background, logo and Discord Rich Presence.
- **Colide** — the new Auto Hunt window in the client (since 2.0.17): 12 skills, 6 potions by % HP or MP, 6 timed items, waiting for HP after revival and skills independent of attacking.
- **Digi Rasta** — author of the systems of the "nowy-system" package (v0.16), ported into MT2009 PLUS as our own code (`server-patches/digirasta`): the Ritual of Awakening at the Blacksmith with the awakened weapons +0…+9, the soul stones up to +9, and the horse to level 30 with paid training at the Stable Boy, the Black Steed trial and saddlebags to level 30.
- [DadsMmoLab/dads-mmo-lab](https://github.com/DadsMmoLab/dads-mmo-lab) — inspiration for autonomous agents in MMO games.
