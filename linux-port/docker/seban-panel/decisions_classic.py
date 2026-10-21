"""The bots' decision explanations, carried over from the classic panel
(panel/app/admin_panel.py) when the two panels became one (LEXIW_ONE_PANEL_V1).

Copied, not rewritten: every definition explain_equip, explain_listing, the
equipment history's "decisions" tab and the decisions page need, with what
they need, in the classic panel's own order - so an explanation reads exactly
as it did there. Only the Flask parts are swapped for this panel's: db() is
app.py's (set by install), the language is Polish, and the request-bound
helpers are stubs.
"""
import logging
import os

os.environ.setdefault("M2PANEL_ENGINE", "mt2009")


class _App:
    logger = logging.getLogger("seban.decisions_classic")


app = _App()
db = None  # app.py's db(), set by decisions_page.install


def lang():
    return "pl"


def has_request_context():
    return False


ITEMS = []  # names come from item_proto (_load_item_proto)

import base64, datetime, json, os, random, re, socket, sys, threading, time, hashlib, hmac, secrets, sqlite3, subprocess

# A skill book is vnum 50300 (or a named book) with the skill id in socket0;
# the bag showed only "Ksiega Umiejetnosci" and nobody could tell which
# skill it was (Tieru, 13 September). Flatten the per-class skill tables
# into one id -> name map so a book can spell its skill out.
SKILL_ID_NAMES = {}


SKILL_ID_NAMES_PL = {}


PLAYERBOT_STATUS_PATHS = (
    "/opt/metin2/var/channel1/first/playerbot_status.tsv",
    "/opt/metin2/var/channel1/game1/playerbot_status.tsv",
    "/opt/metin2/var/channel1/game2/playerbot_status.tsv",
    # The second channel's cores, when the server runs one (M2_PLAYERBOT_CH2).
    "/opt/metin2/var/channel2/first/playerbot_status.tsv",
    "/opt/metin2/var/channel2/game1/playerbot_status.tsv",
    "/opt/metin2/var/channel2/game2/playerbot_status.tsv",
)


_PLAYERBOT_STATUS_LOCK = threading.Lock()


_PLAYERBOT_STATUS_CACHE_KEY = None


_PLAYERBOT_STATUS_CACHE = {}


# EPlayerBotPersonality in playerbot_types.h is the source of truth and its
# order is BOT_PERSONALITY_MERCHANT = 5, BOT_PERSONALITY_WANDERER = 6. These
# two were the other way round here, in both languages, so a keeper minding its
# stall was labelled "Wedrowiec" and a wanderer "Handlarz" - reported against
# botcobra2 (PID 129, personality 5) on 10 September. The stored personality of
# every bot is unchanged; only the words for it were wrong.
BOT_PERSONALITY_LABELS = {
    "pl": {
        0: "Wytrwały poszukiwacz", 1: "Pogromca Metinów", 2: "Towarzysz drużyny",
        3: "Mistrz ekwipunku", 4: "Rozważny zbieracz", 5: "Handlarz",
        6: "Wędrowiec", 7: "Dropek Metinów", 8: "Dropek z M3",
        9: "Dropek z M2", 10: "Dropek medali", 11: "Dropek surowców",
        # MT2009_PLUS_L30_WEAPON_DROPPER_V1
        12: "Dropek broni 30 lv",
    },
    "en": {
        0: "Steady adventurer", 1: "Metin breaker", 2: "Team companion",
        3: "Gear specialist", 4: "Careful collector", 5: "Merchant",
        6: "Wanderer", 7: "Metin dropper", 8: "M3 weapon dropper",
        9: "M2 Bestial dropper", 10: "Medal dropper",
        11: "Guild materials dropper", 12: "Level-30 weapon dropper",
    },
}


# The status file's columns before Iwakura's personalities (2.0.85): a core of
# that age writes these fourteen and no header row the parser can read by.
PLAYERBOT_STATUS_LEGACY_COLUMNS = (
    "pid", "personality", "ambition", "role", "in_party", "goal", "action",
    "updated_ms", "map", "x", "y", "hp", "max_hp", "status",
)


# playerbot_persona::PERSONA_NONE: what the persona and mood columns carry
# while the PERSONA switch is off.
PLAYERBOT_PERSONA_NONE = 255


# Iwakura's personalities, "SYSTEM OSOBOWOSCI v2.0" (playerbot_persona_rules.h,
# EPersona - the order is the interface). Under the switch the bot's old
# personality is its character, shown beside these.
BOT_PERSONA_LABELS = {
    "pl": {
        0: "Grinder", 1: "Zdobywca", 2: "Handlarz", 3: "Hazardzista",
        4: "Perfekcjonista", 5: "Pogromca metinów", 6: "Górnik", 7: "Rybak",
        8: "Najemnik", 9: "Towarzysz",
        # Iwakura's Patch 3, point 7: the rare ones, drawn in red over a bot.
        10: "Metinolog", 11: "Nałogowiec", 12: "Szalony Naukowiec",
        13: "Egzekutor", 14: "Szalony Wędkarz",
        # Community Patch 5, point 1: the four gamblers, drawn in purple.
        15: "Młodszy Hazardzista", 16: "Starszy Hazardzista",
        17: "Naczelny Hazardzista", 18: "Szalony Hazardzista",
        # Baek-Go's herbalist (kuszaa, 30 September).
        19: "Zielarz",
    },
    "en": {
        0: "Grinder", 1: "Conqueror", 2: "Trader", 3: "Gambler",
        4: "Perfectionist", 5: "Metin slayer", 6: "Miner", 7: "Fisherman",
        8: "Mercenary", 9: "Companion",
        10: "Metinologist", 11: "Addict", 12: "Mad Scientist",
        13: "Executioner", 14: "Mad Angler",
        15: "Junior Gambler", 16: "Senior Gambler",
        17: "Chief Gambler", 18: "Mad Gambler",
        19: "Herbalist",
    },
}


BOT_MOOD_LABELS = {
    "pl": {0: "Słaby", 1: "Normalny", 2: "Bardzo dobry"},
    "en": {0: "Poor", 1: "Normal", 2: "Very good"},
}


BOT_MOOD_LOCK_LABELS = {
    "pl": {1: "euforia po ulepszeniu", 2: "kapitulacja (Anty-PK)"},
    "en": {1: "refine euphoria", 2: "capitulation (anti-PK)"},
}


BOT_AMBITION_LABELS = {
    "pl": {
        0: "Poziom", 1: "Ekwipunek", 2: "Metiny", 3: "Koń",
        4: "Biolog", 5: "Umiejętności", 6: "Handel",
    },
    "en": {
        0: "Level", 1: "Equipment", 2: "Metins", 3: "Horse",
        4: "Biologist", 5: "Skills", 6: "Trade",
    },
}


BOT_GOAL_LABELS = {
    "pl": {
        0: "Zdobywanie poziomu", 1: "Przetrwanie", 2: "Wybór profesji",
        3: "Zdobycie ekwipunku", 4: "Uzupełnienie zapasów", 5: "Ulepszanie EQ",
        6: "Rozwój umiejętności", 7: "Polowanie na Metiny", 8: "Silne cele w PT",
        9: "Misja Biologa", 10: "Misja Polowania", 11: "Rozwój konia",
    },
    "en": {
        0: "Gain levels", 1: "Survive", 2: "Choose profession",
        3: "Get equipment", 4: "Restock", 5: "Refine equipment",
        6: "Improve skills", 7: "Hunt Metins", 8: "Party challenges",
        9: "Biologist mission", 10: "Hunting mission", 11: "Develop horse",
    },
}


BOT_ACTION_LABELS = {
    "pl": {
        0: "Planuje następny ruch", 1: "Podróżuje", 2: "Walczy", 3: "Podnosi łup",
        4: "Regeneruje się", 5: "Wybiera profesję", 6: "Handluje", 7: "Ulepsza EQ",
        8: "Czyta KU", 9: "Wkłada KD", 10: "Organizuje PT", 11: "Robi Biologa",
        12: "Odwiedza Stajennego", 13: "Prowadzi stragan",
        # 14-18 were missing here while playerbot_types.h had carried them for
        # months: a bot whose status text was empty fell back to "Planuje
        # nastepny ruch" whatever it was really doing.
        14: "Łowi ryby", 15: "Przegląda stragany", 16: "Wabi potwory",
        17: "Odpoczywa w mieście", 18: "Kopie rudę",
        # MT2009_PLUS_BOT_HERBALIST_FIX_V1: the herb bushes and Baek-Go's
        # board, which walked under 6 ("Handluje") before.
        19: "Zbiera zioła",
    },
    "en": {
        0: "Planning next move", 1: "Travelling", 2: "Fighting", 3: "Picking up loot",
        4: "Recovering", 5: "Choosing profession", 6: "Trading", 7: "Refining gear",
        8: "Reading a skill book", 9: "Socketing a spirit stone", 10: "Organising a party",
        11: "Doing Biologist mission", 12: "Visiting the Stable Boy",
        13: "Keeping a stall", 14: "Fishing", 15: "Browsing stalls",
        16: "Luring monsters", 17: "Resting in town", 18: "Mining ore",
        19: "Picking herbs",
    },
}


def read_playerbot_live_status():
    """Return the newest atomic status snapshots written by all game cores."""
    global _PLAYERBOT_STATUS_CACHE_KEY, _PLAYERBOT_STATUS_CACHE
    now = time.time()
    cache_key = tuple(
        (path, os.path.getmtime(path) if os.path.exists(path) else 0)
        for path in PLAYERBOT_STATUS_PATHS
    )
    with _PLAYERBOT_STATUS_LOCK:
        if cache_key == _PLAYERBOT_STATUS_CACHE_KEY:
            return dict(_PLAYERBOT_STATUS_CACHE)

        result = {}
        for path, modified in cache_key:
            if not modified or now - modified > 20:
                continue
            skipped = 0
            # The channel a bot is on is its file's: "teleport me" has to
            # move a character on another channel there (api_admin_warp_me).
            channel_match = re.search(r"/channel(\d+)/", path)
            channel = int(channel_match.group(1)) if channel_match else 1
            try:
                # The r40250 core and Polish locale tables use Windows-1250.
                with open(path, "r", encoding="cp1250", errors="replace") as stream:
                    header = None
                    for line in stream:
                        # By the header, since Iwakura's personalities added
                        # columns (persona, mood, mood_lock, lock_level); a
                        # core from before them writes the old fourteen, and
                        # the status text is the last column in both.
                        if line.startswith("pid\t"):
                            header = line.rstrip("\r\n").split("\t")
                            continue
                        columns = header or PLAYERBOT_STATUS_LEGACY_COLUMNS
                        parts = line.rstrip("\r\n").split("\t", len(columns) - 1)
                        if len(parts) != len(columns) or columns[-1] != "status":
                            skipped += 1
                            continue
                        # One bad row costs one row.
                        #
                        # This int() used to sit inside a try that wrapped the
                        # whole file, so a single unparseable line - a torn read
                        # while the core rewrites the snapshot is enough - threw
                        # away every remaining line in it. An operator with 999
                        # bots in the world saw 399 here and the right number in
                        # the other panel, which is what a truncated parse looks
                        # like from the outside.
                        try:
                            row = {name: int(value) for name, value in zip(columns[:-1], parts[:-1])}
                        except ValueError:
                            skipped += 1
                            continue
                        if "pid" not in row:
                            skipped += 1
                            continue
                        pid = row["pid"]
                        persona = row.get("persona", PLAYERBOT_PERSONA_NONE)
                        mood = row.get("mood", PLAYERBOT_PERSONA_NONE)
                        result[pid] = {
                            "pid": pid, "personality_id": row.get("personality", 0),
                            "ambition_id": row.get("ambition", 0), "role": row.get("role", 0),
                            "in_pt": bool(row.get("in_party", 0)), "goal_id": row.get("goal", 0),
                            "action_id": row.get("action", 0), "updated_ms": row.get("updated_ms", 0),
                            "map_index": row.get("map", 0), "x": row.get("x", 0), "y": row.get("y", 0),
                            "hp": row.get("hp", 0), "max_hp": row.get("max_hp", 0),
                            "persona_id": None if persona == PLAYERBOT_PERSONA_NONE else persona,
                            "mood_id": None if mood == PLAYERBOT_PERSONA_NONE else mood,
                            "mood_lock": row.get("mood_lock", 0),
                            "lock_level": row.get("lock_level", 0),
                            "channel": channel,
                            "status": parts[-1],
                        }
            except OSError:
                continue
            if skipped:
                app.logger.warning(
                    "playerbot status: %s dropped %d unreadable rows", path, skipped)

        _PLAYERBOT_STATUS_CACHE_KEY = cache_key
        _PLAYERBOT_STATUS_CACHE = result
        return dict(result)


def localize_playerbot_status(entry, language):
    if not entry:
        return None
    if language == "pl":
        return entry.get("status") or BOT_ACTION_LABELS["pl"].get(
            entry.get("action_id"), BOT_ACTION_LABELS["pl"][0])
    text = entry.get("status") or ""
    replacements = (
        ("Nieprzytomny - czekam na wstanie", "Unconscious - waiting to revive"),
        ("Odpoczywam po smierci", "Recovering after death"),
        ("Uciekam - mam malo HP", "Retreating - low HP"),
        ("Rozbijam ", "Breaking "), ("Walcze z ", "Fighting "),
        ("Gonie ", "Chasing "), ("Podnosze lup", "Picking up loot"),
        ("Regeneruje HP", "Recovering HP"),
        ("Szukam przeciwnika", "Looking for a target"),
        ("Szukam miejsca do expa", "Looking for a levelling spot"),
        ("Ulepszam ekwipunek", "Refining equipment"),
    )
    for source, target in replacements:
        text = text.replace(source, target)
    return text or BOT_ACTION_LABELS["en"].get(
        entry.get("action_id"), BOT_ACTION_LABELS["en"][0])


def playerbot_mood_label(entry, language):
    """ "Słaby", or "Bardzo dobry (euforia po ulepszeniu)" while a lock holds;
    "" while the PERSONA switch is off."""
    mood = entry.get("mood_id") if entry else None
    if mood is None:
        return ""
    text = BOT_MOOD_LABELS[language].get(mood, BOT_MOOD_LABELS[language][1])
    lock = BOT_MOOD_LOCK_LABELS[language].get(entry.get("mood_lock") or 0)
    return "%s (%s)" % (text, lock) if lock else text


def playerbot_live_labels(entry, language):
    language = language if language in BOT_PERSONALITY_LABELS else "en"
    if not entry:
        return {
            "personality": BOT_PERSONALITY_LABELS[language][0],
            "charakter": "",
            "mood": "",
            "hold": "",
            "ambition": BOT_AMBITION_LABELS[language][0],
            "goal": BOT_GOAL_LABELS[language][0],
            "action": BOT_ACTION_LABELS[language][0],
        }
    old = BOT_PERSONALITY_LABELS[language].get(
        entry.get("personality_id"), BOT_PERSONALITY_LABELS[language][0])
    persona = entry.get("persona_id")
    # Under Iwakura's personalities the one that claims the bot now is its
    # personality, and the draw it has had since login is its character.
    personality = BOT_PERSONA_LABELS[language].get(persona, old) if persona is not None else old
    hold = ""
    if persona is not None and entry.get("lock_level"):
        hold = map_i18n(language)["exp_hold"].format(n=entry["lock_level"])
    return {
        "personality": personality,
        "charakter": old if persona is not None else "",
        "mood": playerbot_mood_label(entry, language),
        "hold": hold,
        "ambition": BOT_AMBITION_LABELS[language].get(
            entry.get("ambition_id"), BOT_AMBITION_LABELS[language][0]),
        "goal": BOT_GOAL_LABELS[language].get(
            entry.get("goal_id"), BOT_GOAL_LABELS[language][0]),
        "action": localize_playerbot_status(entry, language),
    }


SKILL_GROUP_NAMES = {
    (0, 1): "BODY", (0, 2): "MENTAL",
    (1, 1): "Sztylety", (1, 2): "Łucznik",
    (2, 1): "Magia Broni", (2, 2): "Czarna Magia",
    (3, 1): "Smok", (3, 2): "Leczenie",
}


SKILL_GROUP_NAMES_EN = {
    (0, 1): "Body", (0, 2): "Mental",
    (1, 1): "Dagger", (1, 2): "Archer",
    (2, 1): "Weaponry", (2, 2): "Black Magic",
    (3, 1): "Dragon", (3, 2): "Healing",
}


PLAYER_SKILLS = {
    (0, 1): ((1, "Trzystronne Cięcie"), (2, "Wir Miecza"),
             (3, "Berserk"), (4, "Aura Miecza"), (5, "Szarża")),
    (0, 2): ((16, "Duchowe Uderzenie"), (17, "Tąpnięcie"),
             (18, "Uderzenie Miecza"), (19, "Silne Ciało"), (20, "Walnięcie")),
    (1, 1): ((31, "Zasadzka"), (32, "Szybki Atak"),
             (33, "Wirujący Sztylet"), (34, "Krycie się"), (35, "Trująca Chmura")),
    (1, 2): ((46, "Powtarzalny Strzał"), (47, "Deszcz Strzał"),
             (48, "Ognista Strzała"), (49, "Bezszelestny Chód"), (50, "Trująca Strzała")),
    (2, 1): ((61, "Uderzenie Palcem"), (62, "Smoczy Wir"),
             (63, "Czarowane Ostrze"), (64, "Strach"),
             (65, "Czarowana Zbroja"), (66, "Rozproszenie Magii")),
    (2, 2): ((76, "Mroczne Uderzenie"), (77, "Ogniste Uderzenie"),
             (78, "Ognisty Duch"), (79, "Mroczna Ochrona"),
             (80, "Duchowy Cios"), (81, "Mroczna Sfera")),
    (3, 1): ((91, "Latający Talizman"), (92, "Strzelający Smok"),
             (93, "Smoczy Skowyt"), (94, "Błogosławieństwo"),
             (95, "Odbicie"), (96, "Pomoc Smoka")),
    (3, 2): ((106, "Błyskawiczny Rzut"), (107, "Przywołanie Błyskawicy"),
             (108, "Burzowy Szpon"), (109, "Leczenie"),
             (110, "Zwinność"), (111, "Zwiększenie Ataku")),
}


PLAYER_SKILLS_EN = {
    (0, 1): ((1, "Three-Way Cut"), (2, "Sword Spin"), (3, "Berserk"),
             (4, "Aura of Sword"), (5, "Dash")),
    (0, 2): ((16, "Spirit Strike"), (17, "Bash"), (18, "Stump"),
             (19, "Strong Body"), (20, "Sword Strike")),
    (1, 1): ((31, "Ambush"), (32, "Fast Attack"), (33, "Rolling Dagger"),
             (34, "Stealth"), (35, "Poison Cloud")),
    (1, 2): ((46, "Repetitive Shot"), (47, "Arrow Shower"),
             (48, "Fire Arrow"), (49, "Feather Walk"), (50, "Poison Arrow")),
    (2, 1): ((61, "Finger Strike"), (62, "Dragon Swirl"),
             (63, "Enchanted Blade"), (64, "Fear"),
             (65, "Enchanted Armour"), (66, "Dispel")),
    (2, 2): ((76, "Dark Strike"), (77, "Flame Strike"),
             (78, "Flame Spirit"), (79, "Dark Protection"),
             (80, "Spirit Strike"), (81, "Dark Orb")),
    (3, 1): ((91, "Flying Talisman"), (92, "Shooting Dragon"),
             (93, "Dragon Roar"), (94, "Blessing"),
             (95, "Reflect"), (96, "Dragon's Aid")),
    (3, 2): ((106, "Lightning Throw"), (107, "Summon Lightning"),
             (108, "Lightning Claw"), (109, "Cure"),
             (110, "Swiftness"), (111, "Attack Up")),
}


# Flatten the per-class skill tables into one id -> name map, so a skill book
# (skill id in socket0) can spell its skill out in the inventory (item_full_name).
for _grp in PLAYER_SKILLS.values():
    for _sid, _nm in _grp:
        SKILL_ID_NAMES_PL[_sid] = _nm


for _grp in PLAYER_SKILLS_EN.values():
    for _sid, _nm in _grp:
        SKILL_ID_NAMES[_sid] = _nm


# By the page's language, for the explanations of the bots' decisions.
SKILL_NAMES_BY_LANG = {"pl": SKILL_ID_NAMES_PL, "en": SKILL_ID_NAMES}


SKILL_GROUP_NAMES_BY_LANG = {"pl": SKILL_GROUP_NAMES, "en": SKILL_GROUP_NAMES_EN}


# ---- where everything lives -------------------------------------------------
# The panel was written for a FreeBSD box where install.sh puts its files in
# /usr/local/m2panel and its config in /usr/local/etc. Those are still the
# defaults, so an existing installation keeps working with no changes at all.
# Inside a container none of those paths necessarily exist, so every one of
# them can be moved with an environment variable. Set M2PANEL_DIR alone and
# all four files below follow it; set an individual variable to place just
# that one file somewhere else.
def _env_path(name, default):
    """Value of environment variable 'name', or 'default' when it is unset/empty."""
    return os.environ.get(name, "").strip() or default


# Which engine this panel is looking at (M2PANEL_ENGINE from compose). Read
# here, ahead of everything that differs by engine: the item tables, the
# update check, the attribute numbering further down.
PANEL_ENGINE = os.environ.get("M2PANEL_ENGINE", "r40250").strip().lower()


ENGINE_MT2009 = PANEL_ENGINE == "mt2009"


# Files shipped next to admin_panel.py itself; that is where install.sh puts
# them and where they sit in the source tree, so this needs no variable to
# work — but a read-only image may want them elsewhere.
_HERE      = os.path.dirname(os.path.abspath(__file__))


ITEMS_PATH = _env_path("M2PANEL_ITEMS", os.path.join(_HERE, "items.json"))


# ---- the language the game speaks -------------------------------------------
# Not the language of THIS page -- that is the switcher at the top right, and it
# only changes what the panel says. This is what the SERVER says: quest text,
# system messages, and the item and mob names it hands out. The pack ships all
# of it in fifteen languages and the switch is four files; m2-lang in the game
# container does the swapping and restarts the cores, because they read those
# files only while they boot.
#
# Same spool, same shape as the rates and the game master ranks.
# ---- how the bots behave ----------------------------------------------------
# The same spool once more, but this one needs no helper on the other side: the
# game core reads this file itself, every five seconds, and applies whatever it
# says on the next planning tick. Nothing restarts and nobody is disconnected,
# which is the entire point -- "fewer anglers" used to cost a rebuild.
#
# 100 is neutral for every weight and is what the core assumes for anything the
# file does not mention, so deleting the file puts the world back exactly as the
# build shipped it.
AI_SPOOL     = _env_path("M2PANEL_AI_SPOOL", "/opt/m2spool")


AI_WEIGHTS   = os.path.join(AI_SPOOL, "playerbot_weights.tsv")


AI_W_MIN, AI_W_MAX, AI_W_NEUTRAL = 25, 250, 100


# MT2009_PLUS_SALE_TAX_V1: the sale tax slider's top, the core's own
# (playerbot_sale_tax.h MAX_PERCENT).
SALE_TAX_MAX = 50


# Name, emoji, and the order they are shown in -- which is the order the core
# tests them in, so the page reads top to bottom like the bot decides.
AI_WEIGHT_KEYS = [
    ("RESTOCK", "🧪"),
    ("REFINE",  "🔨"),
    ("SKILL",   "📖"),
    ("HORSE",   "🐎"),
    ("BIOLOG",  "🧬"),
    ("METIN",   "🗿"),
    ("PARTY",   "👥"),
    ("HUNTING", "🏹"),
    ("LEVEL",   "⚔️"),
    ("FISHING", "🎣"),
    ("TRADE",   "🏪"),
    ("MINING",  "⛏️"),
    ("HERB",    "🌿"),
]


def read_ai_weights():
    """What the file says now. Anything missing or unreadable is neutral."""
    vals = {k: AI_W_NEUTRAL for k, _ in AI_WEIGHT_KEYS}
    vals["CHAT"] = 1
    vals["BOOKS"] = 1
    vals["NIGHT"] = 1
    # "Boty graja jak zywi ludzie": sessions and rests. Experimental, off.
    vals["LIFE"] = 0
    # Its hours of play a day (LIFE_HOURS, playerbot_life_rules.h); 0 is the
    # key unset, the sessions and rests of before.
    vals["LIFE_HOURS"] = 0
    # Guild wars between the bots' guilds (playerbot_guild_war.h). On.
    vals["WARS"] = 1
    vals["TOWER"] = 1
    # The bots' raids on Azrael in the Devil's Catacomb (playerbot_catacomb.h,
    # mt2009 only). On.
    vals["CATACOMB"] = 1
    # The bots' ItemShop purchases (playerbot_itemshop.h). On.
    vals["ISHOP"] = 1
    # Whether a bot's stand may stand in a second village too. Off: the stands
    # belong in the first villages (Iwakura's idea, 22 September).
    vals["SHOP_M2"] = 0
    # Iwakura's personalities and moods (playerbot_persona.h). On: the operator
    # asked for them (19 September); off is the world as it was before.
    vals["PERSONA"] = 1
    # The three shouters of the first villages (playerbot_shouters.h,
    # MT2009_PLUS_SHOUTERS_V1) and the three medal droppers among them, Tieru's
    # (MT2009_PLUS_MEDAL_SHOUTERS_V1): on top of the bot count. On.
    vals["SHOUTERS"] = 1
    # MT2009_PLUS_LEGENDS_V1: the bots' legends (playerbot_legends.h) - tiers,
    # their built-in bonuses, the titles over their heads, the Kingdom
    # Champions and the notices. On; off keeps the tiers in the database.
    vals["LEGENDS"] = 1
    # A bot's haggle with a person over a line of the person's offline shop
    # too dear to buy at once (playerbot_haggle.h, MT2009_PLUS_BOT_HAGGLE_V1).
    # On; off is the market as it was.
    vals["HAGGLE"] = 1
    vals["SCRAP"] = 0
    # MT2009_PLUS_SALE_TAX_V1: the percent of a sale between players and bots
    # that leaves the game (playerbot_sale_tax.h, server-patches/saletax);
    # 0 is no tax, the world as it was.
    vals["SALE_TAX"] = 0
    # Percent of bots that rest on the market ring after a town errand; 100 is
    # the author's town, 0 is "every bot hunting".
    vals["REST"] = 100
    # Percent of bots that pick fights with bots of another kingdom; 0 is off.
    vals["KINGDOMPVP"] = 0
    # The three wills (playerbot_config.h): percent of what the build does -
    # the Battle Pass errands, the sash keepers, the Dragon Soul users.
    vals["BATTLEPASS"] = 100
    vals["SASH"] = 100
    vals["ALCHEMY"] = 100
    # The lowest plus a refine under a Blessing or Dragon God scroll may land
    # on; 1 is no floor, which is also what the core starts from.
    vals["SCROLL_FROM"] = 1
    # The bots' guild wars: minutes a war lasts (15 or 30) and hours from the
    # start of one to the next in a kingdom (1-4). DUDU's ask; the defaults are
    # what the core did before there was a choice.
    vals["WAR_MINUTES"] = 30
    vals["WAR_HOURS"] = 2
    # And the kills that win a war with a bot guild on a side before its clock
    # runs out (0: the clock alone; MT2009_PLUS_GUILD_WAR_KILLS_V1).
    vals["WAR_KILLS"] = 100
    # The chest event's two figures. None until the file says: the panel does
    # not know what CONFIG holds, and must not write a guess over it.
    vals["CHEST"] = None
    vals["CHEST_STONE"] = None
    # How many days the core keeps its explanations of the bots' decisions
    # (log.playerbot_listing, log.playerbot_equip; mt2009). None while the key
    # is absent, which the core reads as seven days; 0 is off.
    vals["EXPLAIN"] = None
    try:
        with open(AI_WEIGHTS, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                line = line.split("#", 1)[0].strip()
                if not line:
                    continue
                parts = line.replace("\t", " ").split()
                if len(parts) < 2:
                    continue
                name = parts[0].upper()
                if name == "CHAT":
                    vals["CHAT"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "BOOKS":
                    vals["BOOKS"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "NIGHT":
                    vals["NIGHT"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "LIFE":
                    vals["LIFE"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "LIFE_HOURS":
                    try:
                        vals["LIFE_HOURS"] = max(0, min(LIFE_HOURS_MAX, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name == "WARS":
                    vals["WARS"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "TOWER":
                    vals["TOWER"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "CATACOMB":
                    vals["CATACOMB"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "ISHOP":
                    vals["ISHOP"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "SHOP_M2":
                    vals["SHOP_M2"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "PERSONA":
                    vals["PERSONA"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "SHOUTERS":
                    vals["SHOUTERS"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "LEGENDS":
                    vals["LEGENDS"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "HAGGLE":
                    vals["HAGGLE"] = 0 if parts[1].strip() in ("0", "off", "no") else 1
                    continue
                if name == "SCRAP":
                    try:
                        vals["SCRAP"] = max(0, min(100, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name == "SALE_TAX":
                    try:
                        vals["SALE_TAX"] = max(0, min(SALE_TAX_MAX, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name == "REST":
                    try:
                        vals["REST"] = max(0, min(100, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name == "KINGDOMPVP":
                    try:
                        vals["KINGDOMPVP"] = max(0, min(100, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name in ("BATTLEPASS", "SASH", "ALCHEMY"):
                    try:
                        vals[name] = max(0, min(250, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name == "SCROLL_FROM":
                    try:
                        vals["SCROLL_FROM"] = max(1, min(9, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name == "WAR_MINUTES":
                    try:
                        vals["WAR_MINUTES"] = 15 if int(parts[1]) <= 15 else 30
                    except ValueError:
                        pass
                    continue
                if name == "WAR_HOURS":
                    try:
                        vals["WAR_HOURS"] = max(1, min(4, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name == "WAR_KILLS":
                    try:
                        vals["WAR_KILLS"] = max(0, min(1000, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name in ("CHEST", "CHEST_STONE"):
                    try:
                        vals[name] = max(0, min(1000, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                # Days, not a weight: without this `continue` the slider branch
                # below would clamp it to 25..250 and the next save would write
                # that ("A switch read in a loop of sliders has to continue").
                if name == "EXPLAIN":
                    try:
                        vals["EXPLAIN"] = max(0, min(EXPLAIN_MAX_DAYS, int(parts[1])))
                    except ValueError:
                        pass
                    continue
                if name not in vals:
                    continue
                try:
                    vals[name] = max(AI_W_MIN, min(AI_W_MAX, int(parts[1])))
                except ValueError:
                    pass
    except OSError:
        pass
    return vals


# item database (built from item_proto): [{v:vnum, n:name, k:search keywords, c:category}, ...]
try:
    with open(ITEMS_PATH, encoding="utf-8") as f:
        ITEMS = json.load(f)
except Exception:
    ITEMS = []


# vnum -> display name, for the read-only inventory view
ITEM_NAMES = {it["v"]: it["n"] for it in ITEMS}


ITEM_NAMES_PL = {}


# The stock serverfiles already ship a complete Polish locale table.  The
# context-preparation script places it next to this module so the map does not
# have to maintain a second, inevitably incomplete, hand-written item list.
# r40250's Polish table is Windows-1250 encoded.
try:
    _pl_item_names_path = os.path.join(os.path.dirname(__file__), "item_names_pl.txt")
    with open(_pl_item_names_path, "r", encoding="cp1250", errors="replace") as _pl_names:
        for _line in _pl_names:
            _parts = _line.rstrip("\r\n").split("\t", 1)
            if len(_parts) != 2 or not _parts[0].isdigit():
                continue
            ITEM_NAMES_PL[int(_parts[0])] = _parts[1].strip()
except (OSError, UnicodeError):
    ITEM_NAMES_PL = {}


_ITEM_PL_EXACT = {
    "Full Moon Sword": "Miecz Pełni Księżyca",
    "Red Iron Blade": "Czerwone Żelazne Ostrze",
    "Black Leaf Dirk": "Sztylet Czarnego Liścia",
    "Bull's Horn Bow": "Łuk z Rogu Byka",
    "Antique Bell": "Antyczny Dzwon",
    "Autumn Wind Fan": "Wachlarz Jesiennego Wiatru",
    "Battle Shield": "Tarcza Bojowa",
    "Pentagon Shield": "Tarcza Pięciokątna",
    "Horse Medal": "Medal Konny",
    "Red Potion": "Czerwona Mikstura",
    "Blue Potion": "Niebieska Mikstura",
    "Green Potion": "Zielona Mikstura",
    "Purple Potion": "Fioletowa Mikstura",
}


_ITEM_PL_WORDS = (
    ("Skill Book", "Księga Umiejętności"), ("Armour", "Zbroja"),
    ("Armor", "Zbroja"), ("Earrings", "Kolczyki"),
    ("Bracelet", "Bransoleta"), ("Necklace", "Naszyjnik"),
    ("Helmet", "Hełm"), ("Shield", "Tarcza"), ("Boots", "Buty"),
    ("Shoes", "Buty"), ("Sword", "Miecz"), ("Blade", "Ostrze"),
    ("Dagger", "Sztylet"), ("Dirk", "Sztylet"), ("Bow", "Łuk"),
    ("Bell", "Dzwon"), ("Fan", "Wachlarz"), ("Potion", "Mikstura"),
    ("Stone", "Kamień"), ("Book", "Księga"),
)


def translate_item_name_pl(name):
    """Translate stock item families while preserving refinement suffixes."""
    name = str(name or "")
    for source, translated in _ITEM_PL_EXACT.items():
        if name == source or name.startswith(source + "+") or name.startswith(source + " ("):
            return translated + name[len(source):]
    for source, translated in _ITEM_PL_WORDS:
        name = re.sub(r"\b%s\b" % re.escape(source), translated, name,
                      flags=re.IGNORECASE)
    return name


# On mt2009 the files above are r40250's: items.json carries that engine's
# English names and cell sizes, item_names_pl.txt its Polish table, and the
# panel fell back on word-by-word translation for the rest - "Leather Buty",
# "Wooden Kolczyki", "Azure Suit" beside "Sztylet", "Przedmiot #30347" for
# an item the other engine never had, and a two-cell dagger drawn in one
# cell. The db core mirrors the package's item_proto.txt and item_names.txt
# into player.item_proto at every boot (locale_name is the Polish name on
# this package), so that table is the one source that is always right for
# this world. Read once the database answers, and again every hour.
ITEM_SIZES = {}


ITEM_TYPES = {}


# What the tooltip's base lines are computed from, per vnum: the proto's
# type/subtype, value0..5, the fixed applies and the level limit - the same
# fields the static item_defs.json carries for r40250, so the tooltip code
# needs no second path. Without this an mt2009 world showed the other
# engine's attack values on a vnum both have, and nothing on the rest.
ITEM_BASE = {}


_PROTO = {"loaded": 0.0, "tried": 0.0}


_PROTO_LOCK = threading.Lock()


def _load_item_proto():
    """The world's items into ITEMS / ITEM_NAMES / ITEM_NAMES_PL / ITEM_SIZES."""
    global ITEMS
    with _PROTO_LOCK:
        now = time.time()
        if now - _PROTO["tried"] < 60:
            return
        _PROTO["tried"] = now
        try:
            with db() as c, c.cursor() as cur:
                # Both name columns are cp1250_polish_ci on this package and
                # the panel's connection is latin1, so the server converts on
                # the way out and every letter latin1 lacks - l with a stroke,
                # s/z/n with an acute, e with an ogonek - arrives as "?"
                # ("Skrzyd?a Demona", "Zw?j B?ogos?awie?stwa" in 2.0.13). The
                # bytes are asked for as they are and decoded here.
                cur.execute("SELECT vnum, CAST(name AS BINARY) AS name, "
                            "CAST(locale_name AS BINARY) AS locale_name, "
                            "type, subtype, size, "
                            "value0, value1, value2, value3, value4, value5, "
                            "applytype0, applyvalue0, applytype1, applyvalue1, "
                            "applytype2, applyvalue2, "
                            "limittype0, limitvalue0, limittype1, limitvalue1 "
                            "FROM player.item_proto")
                rows = cur.fetchall()
        except Exception:
            return
        if not rows:
            return
        items = []
        for r in rows:
            vnum = int(r["vnum"] or 0)
            pl = log_text(r.get("locale_name")).strip()
            en = log_text(r.get("name")).strip() or pl
            if not pl:
                continue
            ITEM_NAMES_PL[vnum] = pl
            ITEM_NAMES[vnum] = pl
            ITEM_SIZES[vnum] = max(1, min(3, int(r.get("size") or 1)))
            ITEM_TYPES[vnum] = (int(r.get("type") or 0), int(r.get("subtype") or 0))
            base = {"type": ITEM_TYPES[vnum][0], "subtype": ITEM_TYPES[vnum][1]}
            for k in range(6):
                base["value%d" % k] = int(r.get("value%d" % k) or 0)
            # The proto's applies are POINT numbers here like the bonus
            # lines, and the tooltip puts them through the same table.
            base["apply"] = [{"type": int(r.get("applytype%d" % k) or 0),
                              "val": int(r.get("applyvalue%d" % k) or 0)}
                             for k in range(3) if int(r.get("applytype%d" % k) or 0)]
            # LIMIT_LEVEL is 1 on both engines (item_length.h).
            level = 0
            for k in range(2):
                if int(r.get("limittype%d" % k) or 0) == 1:
                    level = int(r.get("limitvalue%d" % k) or 0)
            base["level"] = level
            ITEM_BASE[vnum] = base
            items.append({"v": vnum, "n": pl, "k": (pl + " " + en).lower(),
                          "c": ITEM_CATEGORY_BY_TYPE.get(int(r.get("type") or 0), "other")})
        ITEMS = items
        _PROTO["loaded"] = now


# items.json sorts by a category word; the proto only has the type number.
ITEM_CATEGORY_BY_TYPE = {1: "weapon", 2: "armor", 3: "use", 4: "autouse", 5: "material",
                         6: "special", 7: "tool", 8: "lottery", 9: "elk", 10: "metin",
                         11: "container", 12: "fish", 13: "rod", 14: "resource", 15: "campfire",
                         16: "unique", 17: "skillbook", 18: "quest", 19: "polymorph",
                         20: "treasure_box", 21: "treasure_key", 22: "skillforget",
                         23: "giftbox", 24: "pick", 26: "hair", 27: "totem", 28: "blend",
                         29: "costume", 30: "des", 31: "ring", 32: "belt"}


def item_proto_ready():
    """On mt2009: the world's tables, loaded on first use and refreshed hourly.
    Called on the request path; a database that is still starting costs one
    failed query a minute and the r40250 files stand in until it answers."""
    if not ENGINE_MT2009:
        return
    if time.time() - _PROTO["loaded"] > 3600:
        _load_item_proto()


def localized_item_name(vnum, language=None):
    item_proto_ready()
    language = language or (lang() if has_request_context() else "en")
    if language == "pl" and int(vnum or 0) in ITEM_NAMES_PL:
        return ITEM_NAMES_PL[int(vnum or 0)]
    name = ITEM_NAMES.get(vnum, "")
    if not name:
        return map_i18n(language)["item_n"].format(n=int(vnum or 0))
    if ENGINE_MT2009:
        return name          # the package's own name, in the package's language
    return translate_item_name_pl(name) if language == "pl" else name


# ---- UI translations -------------------------------------------------------
LANGS = {"pl": "Polski", "en": "English", "de": "Deutsch", "tr": "Türkçe"}


T = {
 "acc_back":     {"pl":"← Wstecz","en":"← Back","de":"← Zurück","tr":"← Geri"},
 "acc_title":    {"pl":"Moje konto","en":"My account","de":"Mein Konto","tr":"Hesabım"},
 "acc_login_hint": {"pl":"Zaloguj się nazwą użytkownika i hasłem z gry.","en":"Log in with your game username and password.","de":"Melde dich mit deinem Spiel-Benutzernamen und Passwort an.","tr":"Oyun kullanıcı adın ve şifrenle giriş yap."},
 "acc_ph_user":  {"pl":"Nazwa użytkownika","en":"Username","de":"Benutzername","tr":"Kullanıcı adı"},
 "acc_ph_pw":    {"pl":"Hasło","en":"Password","de":"Passwort","tr":"Şifre"},
 "acc_logout":   {"pl":"Wyloguj z konta","en":"Log out of account","de":"Vom Konto abmelden","tr":"Hesaptan çıkış yap"},
 "acc_chars":    {"pl":"Twoje postacie:","en":"Your characters:","de":"Deine Charaktere:","tr":"Karakterlerin:"},
 "acc_no_chars": {"pl":"Nie ma jeszcze postaci — zaloguj się do gry i stwórz jedną! 🙂","en":"No characters yet — log into the game and create one! 🙂","de":"Noch keine Charaktere — melde dich im Spiel an und erstelle einen! 🙂","tr":"Henüz karakter yok — oyuna gir ve bir tane oluştur! 🙂"},
 "acc_pw_title": {"pl":"🔒 Zmień hasło","en":"🔒 Change password","de":"🔒 Passwort ändern","tr":"🔒 Şifreyi değiştir"},
 "acc_pw_old":   {"pl":"Obecne hasło","en":"Current password","de":"Aktuelles Passwort","tr":"Mevcut şifre"},
 "welcome":      {"pl":"Witaj!","en":"Welcome!","de":"Willkommen!","tr":"Hoş geldin!"},
 "admin_hint":   {"pl":"Jeśli jesteś administratorem, wpisz hasło panelu.","en":"If you're the admin, enter your passphrase.","de":"Wenn du der Admin bist, gib deine Passphrase ein.","tr":"Yöneticiysen gizli kelimeni yaz."},
 "passphrase":   {"pl":"Hasło panelu","en":"Passphrase","de":"Passphrase","tr":"Gizli kelime"},
 "login":        {"pl":"Zaloguj","en":"Log in","de":"Einloggen","tr":"Giriş yap"},
 "logout":       {"pl":"Wyloguj","en":"Log out","de":"Abmelden","tr":"Çıkış"},
 "player_q":     {"pl":"Jesteś graczem?","en":"Are you a player?","de":"Bist du ein Spieler?","tr":"Oyuncu musun?"},
 # The frame around the two ways into the game, and the heading of the second
 # of them. Both are set in the same voice as play_title, because they sit
 # next to it and a quieter one would read as the lesser option.
 # Said on the card that comes FIRST, because the account is the step before
 # either way in -- and both ways use the same one, which is not obvious when
 # they are presented as a choice.
 "acc_needed":   {"pl":"Grasz na koncie gry — tym samym w przeglądarce i w pobranym kliencie. Założenie zajmuje chwilę.","en":"You play with a game account — the same one in the browser and in the download. Making one takes a moment.",
                  "de":"Zum Spielen brauchst du ein Spiel-Konto — dasselbe im Browser wie im Download. Erstellen dauert einen Moment.",
                  "tr":"Oynamak için bir oyun hesabı gerekir — tarayıcıda ve indirmede aynısı. Oluşturmak bir dakika sürer."},
 "ways_t":       {"pl":"JAK ZAGRAĆ","en":"HOW TO PLAY","de":"SO KOMMST DU INS SPIEL","tr":"OYUNA NASIL GİRERSİN"},
 "dl_now_t":     {"pl":"POBIERZ TERAZ","en":"DOWNLOAD IT NOW","de":"JETZT HERUNTERLADEN","tr":"ŞiMDİ İNDİR"},
 "dl_hint":      {"pl":"Pobierz, rozpakuj gdziekolwiek chcesz i uruchom Metin2Distribute.exe — bez instalacji, wszystko jest już skonfigurowane. Gra jest w pełni przenośna: skopiuj folder na pendrive i graj z dowolnego komputera.","en":"Download it, extract it anywhere you prefer and just run Metin2Distribute.exe — no installation, everything comes pre-configured. The game is fully portable: copy the folder onto a flash drive and play from any machine you like.",
                  "de":"Herunterladen, an einen beliebigen Ort entpacken und einfach Metin2Distribute.exe starten — keine Installation, alles ist vorkonfiguriert. Das Spiel ist komplett portabel: Kopiere den Ordner auf einen USB-Stick und spiele von jedem Rechner, den du magst.",
                  "tr":"İndir, istediğin yere çıkart ve sadece Metin2Distribute.exe'yi çalıştır — kurulum yok, her şey hazır gelir. Oyun tamamen taşınabilir: klasörü bir USB belleğe kopyala, istediğin bilgisayardan oyna."},
 "download":     {"pl":"📥 Pobierz grę","en":"📥 Download the Game","de":"📥 Spiel herunterladen","tr":"📥 Oyunu İndir"},
 "game_account": {"pl":"Konto gry","en":"Game account","de":"Spiel-Konto","tr":"Oyun hesabı"},
 "create_acc":   {"pl":"📝 Załóż konto","en":"📝 Create account","de":"📝 Konto erstellen","tr":"📝 Kayıt ol"},
 "my_acc":       {"pl":"👤 Moje konto","en":"👤 My account","de":"👤 Mein Konto","tr":"👤 Hesabım"},
 "players":      {"pl":"Gracze","en":"Players","de":"Spieler","tr":"Oyuncular"},
 "acc_col":      {"pl":"Konto","en":"Account","de":"Konto","tr":"Hesap"},
 "srv_online":   {"pl":"Serwer online","en":"Server online","de":"Server online","tr":"Sunucu çevrimiçi"},
 # --- download steps & facts ---
 "dl_how":       {"pl":"Jak to działa","en":"How it works","de":"So geht's","tr":"Nasıl çalışır"},
 "dl_st1":       {"pl":"Pobierz archiwum zip","en":"Download the zip","de":"Zip herunterladen","tr":"Zip'i indir"},
 "dl_st2":       {"pl":"Rozpakuj gdziekolwiek — pendrive też się nadaje","en":"Extract it anywhere you like — a flash drive works too","de":"Irgendwohin entpacken — auch ein USB-Stick geht","tr":"İstediğin yere çıkart — USB bellek de olur"},
 "dl_st3":       {"pl":"Uruchom Metin2Distribute.exe i graj","en":"Run Metin2Distribute.exe and play","de":"Metin2Distribute.exe starten und spielen","tr":"Metin2Distribute.exe'yi çalıştır ve oyna"},
 "dl_sha":       {"pl":"Tym odciskiem sprawdzisz, czy plik dotarł w całości — porównaj go z tym, co pokaże Twoje narzędzie do sum kontrolnych.","en":"With this fingerprint you can verify the download arrived intact — compare it with what your checksum tool says.",
                  "de":"Mit diesem Fingerabdruck kannst du prüfen, ob der Download heil angekommen ist — vergleiche ihn mit dem, was dein Prüfsummen-Tool sagt.",
                  "tr":"Bu parmak iziyle indirmenin sağlam geldiğini doğrulayabilirsin — sağlama aracının söylediğiyle karşılaştır."},
 # --- the client that runs in a browser ---
 "play_title":   {"pl":"ZAGRAJ TERAZ W PRZEGLĄDARCE","en":"PLAY IN YOUR BROWSER NOW","de":"JETZT IM BROWSER SPIELEN","tr":"ŞiMDİ TARAYICINDA OYNA"},
 "play_hint":    {"pl":"Bez pobierania i bez instalacji — gra otworzy się w karcie przeglądarki!","en":"No download and no installation, the game will open as a Browser Tab!",
                  "de":"Kein Download, keine Installation — das Spiel öffnet sich als Browser-Tab!",
                  "tr":"İndirme yok, kurulum yok — oyun bir tarayıcı sekmesinde açılır!"},
 "play_btn":     {"pl":"🎮 Graj w przeglądarce","en":"🎮 Play in the browser","de":"🎮 Im Browser spielen","tr":"🎮 Tarayıcıda oyna"},
 "tip_play":     {"pl":"Otwiera grę w nowej karcie, więc ta strona dalej działa. To ten sam serwer co w pobranej grze i to samo konto — postać założona w jednej jest w drugiej. Pobrana gra działa lepiej; ta nie wymaga niczego instalować.","en":"Opens the game in a new tab, so this page keeps running. It is the same server as the downloaded game and the same account — a character made in one is there in the other. The downloaded game runs better; this one needs nothing installed.",
                  "de":"Öffnet das Spiel in einem neuen Tab, diese Seite läuft weiter. Es ist derselbe Server wie beim heruntergeladenen Spiel und dasselbe Konto — ein Charakter aus dem einen ist auch im anderen da. Das heruntergeladene Spiel läuft flüssiger; dieses hier braucht keine Installation.",
                  "tr":"Oyunu yeni bir sekmede açar, bu sayfa açık kalır. İndirilen oyunla aynı sunucu ve aynı hesap — birinde yaptığın karakter diğerinde de vardır. İndirilen oyun daha akıcı çalışır; bu ise hiçbir kurulum istemez."},
 # --- registration polish ---
 "reg_title":    {"pl":"Załóż konto gry","en":"Create your game account","de":"Spiel-Konto erstellen","tr":"Oyun hesabı oluştur"},
 "reg_hint":     {"pl":"Tym kontem logujesz się do samej gry.","en":"This is the account you'll use to log into the game itself.","de":"Mit diesem Konto meldest du dich im Spiel selbst an.","tr":"Oyuna bu hesapla giriş yapacaksın."},
 "reg_ph_user":  {"pl":"Nazwa użytkownika (4-16 liter/cyfr)","en":"Username (4-16 letters/numbers)","de":"Benutzername (4-16 Buchstaben/Zahlen)","tr":"Kullanıcı adı (4-16 harf/rakam)"},
 "reg_ph_pw":    {"pl":"Hasło (co najmniej 6 znaków)","en":"Password (at least 6 characters)","de":"Passwort (mindestens 6 Zeichen)","tr":"Şifre (en az 6 karakter)"},
 "reg_ph_pw2":   {"pl":"Hasło jeszcze raz","en":"Password again","de":"Passwort wiederholen","tr":"Şifre (tekrar)"},
 "reg_ph_social":{"pl":"Kod usuwania — dowolne 7 cyfr","en":"Delete code — pick any 7 digits","de":"Löschcode — beliebige 7 Ziffern","tr":"Silme kodu — 7 rakam seç"},
 "reg_social_hint":{"pl":"💡 O kod usuwania gra pyta przy kasowaniu postaci. Wybierz 7 cyfr, które zapamiętasz (np. 1234567).","en":"💡 The delete code is asked by the game when you delete a character. Pick 7 digits you'll remember (e.g. 1234567).",
                  "de":"💡 Den Löschcode fragt das Spiel ab, wenn du einen Charakter löschst. Wähle 7 Ziffern, die du dir merkst (z. B. 1234567).",
                  "tr":"💡 Silme kodunu oyun, karakter silerken sorar. Hatırlayacağın 7 rakam seç (örn. 1234567)."},
 "reg_free":     {"pl":"✓ Nazwa jest wolna","en":"✓ Name is free","de":"✓ Name ist frei","tr":"✓ İsim boş"},
 "reg_taken":    {"pl":"✗ Już zajęta","en":"✗ Already taken","de":"✗ Schon vergeben","tr":"✗ Zaten alınmış"},
 "reg_pw_match": {"pl":"✓ Hasła są zgodne","en":"✓ Passwords match","de":"✓ Passwörter stimmen überein","tr":"✓ Şifreler uyuşuyor"},
 "reg_pw_diff":  {"pl":"✗ Hasła się różnią","en":"✗ Passwords differ","de":"✗ Passwörter unterscheiden sich","tr":"✗ Şifreler farklı"},
 "reg_done_title":{"pl":"Twoje konto jest gotowe!","en":"Your account is ready!","de":"Dein Konto ist fertig!","tr":"Hesabın hazır!"},
 "reg_done_next":{"pl":"Stąd do gry:","en":"From here to the game:","de":"Von hier bis ins Spiel:","tr":"Buradan oyuna:"},
 "reg_done_login":{"pl":"Zaloguj się nazwą użytkownika i hasłem, które przed chwilą wybrałeś","en":"Log in with the username and password you just chose","de":"Melde dich mit dem eben gewählten Namen und Passwort an","tr":"Az önce seçtiğin ad ve şifreyle giriş yap"},
 # Shown after registering when this server offers BOTH ways in. The account is
 # the same either way, and saying so is the whole point: without it the two
 # cards read as two different games.
 "reg_both":    {"pl":"Dwie drogi do gry — wybierz dowolną. To ten sam serwer i to samo konto.","en":"Two ways in — take either one. It is the same server and the same account.","de":"Zwei Wege ins Spiel — nimm einen davon. Gleicher Server, gleiches Konto.","tr":"Oyuna iki yol — hangisini istersen. Aynı sunucu, aynı hesap."},
 "reg_or":      {"pl":"ALBO","en":"OR","de":"ODER","tr":"VEYA"},
 "reg_dl_t":    {"pl":"Pobierz grę","en":"Download the game","de":"Spiel herunterladen","tr":"Oyunu indir"},
 # --- admin: player search, activity, inventory ---
 "search_players":{"pl":"Filtruj po postaci lub koncie…","en":"Filter by character or account…","de":"Nach Charakter oder Konto filtern…","tr":"Karakter veya hesaba göre süz…"},
 "tip_active":   {"pl":"Był w grze w ciągu ostatnich kilku minut.","en":"Was in the game within the last few minutes.","de":"War in den letzten Minuten im Spiel.","tr":"Son birkaç dakika içinde oyundaydı."},
 "inv_title":    {"pl":"Ekwipunek","en":"Inventory","de":"Inventar","tr":"Envanter"},
 "tip_inv":      {"pl":"Co ta postać ma przy sobie, prosto z bazy danych — tylko do odczytu. Przydatne, żeby nie podarować czegoś dwa razy.","en":"What this character carries, straight from the database — read-only. Handy for checking before you gift something twice.",
                  "de":"Was dieser Charakter bei sich trägt, direkt aus der Datenbank — nur lesend. Praktisch, um nicht doppelt zu schenken.",
                  "tr":"Bu karakterin üzerindekiler, doğrudan veritabanından — salt okunur. Bir şeyi iki kez hediye etmemek için kullanışlı."},
 "inv_empty":    {"pl":"W ekwipunku nie ma jeszcze nic.","en":"Nothing in the inventory yet.","de":"Noch nichts im Inventar.","tr":"Envanterde henüz bir şey yok."},
 "srv_playing":  {"pl":"teraz w grze","en":"in game right now","de":"gerade im Spiel","tr":"şu an oyunda"},
 "srv_offline":  {"pl":"Serwer gry jest w tej chwili wyłączony — zwykle zaraz wraca. Pobieranie i strony kont działają dalej.","en":"The game server is down at the moment — it usually comes right back. Downloads and account pages keep working.",
                  "de":"Der Spielserver ist gerade aus — meist ist er gleich wieder da. Download und Konto-Seiten funktionieren weiter.",
                  "tr":"Oyun sunucusu şu an kapalı — genellikle hemen geri gelir. İndirme ve hesap sayfaları çalışmaya devam eder."},
 "tip_srv":      {"pl":"Sprawdzane na żywo w samej grze, najwyżej co 30 sekund: zielony znaczy, że odpowiada serwer logowania i pierwszy kanał. Liczba to postacie, które gra zapisała w ostatnich minutach, więc ktoś, kto właśnie wszedł, może pojawić się z opóźnieniem, a kto właśnie wyszedł, chwilę jeszcze widnieje.","en":"Checked live against the game itself, at most every 30 seconds: green means the login server and the first channel both answer. The number is characters the game has saved in the last few minutes, so somebody who just logged in may take a moment to appear, and somebody who just left lingers a little.",
                  "de":"Live am Spiel selbst geprüft, höchstens alle 30 Sekunden: Grün heißt, Login-Server und erster Kanal antworten beide. Die Zahl sind Charaktere, die das Spiel in den letzten Minuten gespeichert hat — wer sich gerade erst eingeloggt hat, erscheint also etwas verzögert, und wer eben gegangen ist, bleibt kurz stehen.",
                  "tr":"Doğrudan oyunun kendisinden, en fazla 30 saniyede bir kontrol edilir: yeşil, giriş sunucusunun ve ilk kanalın yanıt verdiği anlamına gelir. Sayı, oyunun son birkaç dakikada kaydettiği karakterlerdir; yeni giren biri biraz gecikmeyle görünür, yeni çıkan biri ise kısa süre sayılmaya devam eder."},
 "tip_acc_col":  {"pl":"Konto gry, do którego należy ta postać — nazwa, którą gracz wpisuje przy logowaniu. Jedno konto może mieć kilka postaci.","en":"The game account this character belongs to — the name the player types at the game login. One account can hold several characters.",
                  "de":"Das Spiel-Konto, zu dem dieser Charakter gehört — der Name, den der Spieler beim Spiel-Login eintippt. Ein Konto kann mehrere Charaktere haben.",
                  "tr":"Bu karakterin bağlı olduğu oyun hesabı — oyuncunun oyun girişinde yazdığı ad. Bir hesapta birden çok karakter olabilir."},
 "tap_hint":     {"pl":"Kliknij nazwę gracza, aby nim zarządzać.","en":"Tap a player's name to manage them.","de":"Tippe auf einen Spielernamen, um ihn zu verwalten.","tr":"Yönetmek için oyuncunun adına dokun."},
 "character":    {"pl":"Postać","en":"Character","de":"Charakter","tr":"Karakter"},
 "level":        {"pl":"Poziom","en":"Level","de":"Level","tr":"Seviye"},
 "last_seen":    {"pl":"Ostatnio widziany","en":"Last seen","de":"Zuletzt online","tr":"Son görülme"},
 "no_chars":     {"pl":"Nie ma jeszcze postaci. Pojawią się tu po pierwszym zalogowaniu.","en":"No characters yet. They appear here after the first login.","de":"Noch keine Charaktere. Sie erscheinen nach dem ersten Login.","tr":"Henüz karakter yok. İlk girişten sonra görünecek."},
 "back_players": {"pl":"← Wróć do graczy","en":"← Back to players","de":"← Zurück zu den Spielern","tr":"← Oyunculara dön"},
 "give_item":    {"pl":"🎁 Daj przedmiot","en":"🎁 Give an item","de":"🎁 Gegenstand geben","tr":"🎁 Eşya ver"},
 "search_item":  {"pl":"Wpisz, aby wyszukać przedmiot (np. miecz, mikstura)…","en":"Type to search item (e.g. sword, potion)…","de":"Zum Suchen tippen (z.B. Schwert, Trank)…","tr":"Aramak için yaz (örn. kılıç, iksir)…"},
 "category":     {"pl":"Kategoria","en":"Category","de":"Kategorie","tr":"Kategori"},
 "qty":          {"pl":"Ile sztuk?","en":"How many?","de":"Wie viele?","tr":"Kaç adet?"},
 "send_item":    {"pl":"🎁 Wyślij przedmiot","en":"🎁 Send item","de":"🎁 Senden","tr":"🎁 Gönder"},
 "give_gold":    {"pl":"💰 Daj yang","en":"💰 Give yang","de":"💰 Yang geben","tr":"💰 Yang ver"},
 "amount":       {"pl":"Kwota (ujemna zabiera yang)","en":"Amount (negative takes yang away)","de":"Menge (negativ = abziehen)","tr":"Miktar (eksi = al)"},
 "send_gold":    {"pl":"💰 Wyślij yang","en":"💰 Send yang","de":"💰 Yang senden","tr":"💰 Yang gönder"},
 "set_level":    {"pl":"⭐ Ustaw poziom","en":"⭐ Set level","de":"⭐ Level setzen","tr":"⭐ Seviye ayarla"},
 "new_level":    {"pl":"Nowy poziom (1-{max})","en":"New level (1-{max})","de":"Neues Level (1-{max})","tr":"Yeni seviye (1-{max})"},
 "level_range":  {"pl":"Najwyższy poziom na tym serwerze to {max}, więc nic nie zmieniono. Podnieś M2_MAX_LEVEL w .env i zrestartuj, aby pójść wyżej — sama gra kończy się na 120.","en":"This server's highest level is {max}, so nothing was changed. Raise M2_MAX_LEVEL in .env and restart to go higher — the game itself stops at 120.",
                  "de":"Das höchste Level dieses Servers ist {max}, es wurde nichts geändert. Für mehr M2_MAX_LEVEL in der .env erhöhen und neu starten — das Spiel selbst endet bei 120.",
                  "tr":"Bu sunucunun en yüksek seviyesi {max}, bu yüzden hiçbir şey değiştirilmedi. Daha yükseği için .env dosyasındaki M2_MAX_LEVEL değerini artırıp yeniden başlat — oyunun kendisi 120'de biter."},
 "change_level": {"pl":"⭐ Zmień poziom","en":"⭐ Change level","de":"⭐ Level ändern","tr":"⭐ Seviyeyi değiştir"},
 "teleport":     {"pl":"🗺️ Teleportuj","en":"🗺️ Teleport","de":"🗺️ Teleportieren","tr":"🗺️ Işınla"},
 "ingame_only":  {"pl":"(działa, gdy gracz jest w grze)","en":"(works while the player is in game)","de":"(nur wenn der Spieler online ist)","tr":"(oyuncu oyundayken çalışır)"},
 "speed":        {"pl":"🏃 Szybkość biegu","en":"🏃 Running speed","de":"🏃 Laufgeschwindigkeit","tr":"🏃 Koşma hızı"},
 "apply":        {"pl":"🏃 Zastosuj","en":"🏃 Apply","de":"🏃 Anwenden","tr":"🏃 Uygula"},
 "srch_more":    {"pl":"⌄ Pokaż więcej","en":"⌄ Show more","de":"⌄ Mehr anzeigen","tr":"⌄ Daha fazla göster"},
 "srch_down":    {"pl":"Nie udało się wczytać listy przedmiotów. Odśwież stronę; jeśli to się powtarza, przeglądarka może blokować zapytanie.","en":"The item list could not be loaded. Reload the page; if it keeps happening, your browser may be blocking the request.",
                  "de":"Die Item-Liste konnte nicht geladen werden. Lade die Seite neu; wenn es weiter passiert, blockiert dein Browser die Anfrage womöglich.",
                  "tr":"Eşya listesi yüklenemedi. Sayfayı yenile; devam ederse tarayıcın isteği engelliyor olabilir."},
 # --- the language the game speaks (not the language of this page) ---
 "sect_setup":   {"pl":"Ustawienia serwera","en":"Server setup","de":"Servereinrichtung","tr":"Sunucu kurulumu"},
 "sect_setup_hint":{"pl":"Jak ten serwer jest ustawiony, a nie co się na nim dzieje. Rzadko będą potrzebne, a oba dotyczą wszystkich.","en":"How this server is set up, rather than what happens on it. You will not need these often, and both of them affect everyone.",
                  "de":"Wie dieser Server eingerichtet ist — nicht, was auf ihm passiert. Beides brauchst du selten, und beides betrifft alle.",
                  "tr":"Bu sunucunun nasıl kurulduğu — üzerinde ne olduğu değil. İkisine de nadiren ihtiyacın olur ve ikisi de herkesi etkiler."},
 "lang_title":   {"pl":"🌍 Język gry","en":"🌍 Game language","de":"🌍 Spielsprache","tr":"🌍 Oyun dili"},
 "lang_now":     {"pl":"Gra jest w języku: {lang}.","en":"The game is in {lang}.","de":"Das Spiel läuft auf {lang}.","tr":"Oyun {lang} dilinde."},
 "lang_intro":   {"pl":"To język, którym mówi serwer: teksty zadań, komunikaty systemowe, nazwy przedmiotów i potworów. To nie język tej strony — ten przełącza się w prawym górnym rogu. Zmiana restartuje grę na mniej niż minutę, więc grający zostają na chwilę rozłączeni.","en":"This is the language the server speaks: quest text, system messages, and the names of items and monsters. It is not the language of this page — that is the switcher at the top right. Changing it restarts the game for well under a minute, so anyone playing is briefly disconnected.",
                  "de":"Das ist die Sprache, die der Server spricht: Questtexte, Systemmeldungen und die Namen von Gegenständen und Monstern. Es ist nicht die Sprache dieser Seite — die stellst du oben rechts um. Beim Wechsel startet das Spiel für deutlich unter einer Minute neu, Spieler fliegen also kurz raus.",
                  "tr":"Bu, sunucunun konuştuğu dildir: görev metinleri, sistem mesajları ve eşya ile canavar adları. Bu sayfanın dili değildir — onu sağ üstten değiştirirsin. Değiştirmek oyunu bir dakikadan çok kısa süre yeniden başlatır, oyuncular kısa süre düşer."},
 "lang_apply":   {"pl":"🌍 Zmień język gry","en":"🌍 Change the game language","de":"🌍 Spielsprache ändern","tr":"🌍 Oyun dilini değiştir"},
 "lang_asked":   {"pl":"Przełączam grę na {lang}. Serwer za chwilę sam się zrestartuje; odśwież tę stronę, aby zobaczyć wynik.","en":"Switching the game to {lang}. The server restarts itself in a moment; reload this page to see it done.",
                  "de":"Das Spiel wird auf {lang} umgestellt. Der Server startet gleich neu; lade diese Seite neu, um es bestätigt zu sehen.",
                  "tr":"Oyun {lang} diline geçiriliyor. Sunucu birazdan yeniden başlıyor; tamamlandığını görmek için bu sayfayı yenile."},
 "lang_same":    {"pl":"Gra jest już w języku {lang}, więc nic nie zmieniono.","en":"The game is already in {lang}, so nothing was changed.",
                  "de":"Das Spiel läuft bereits auf {lang}, es wurde nichts geändert.",
                  "tr":"Oyun zaten {lang} dilinde, hiçbir şey değiştirilmedi."},
 "lang_nospool": {"pl":"Nie udało się połączyć z serwerem gry, więc język nie został zmieniony.","en":"The game server could not be reached, so the language was not changed.",
                  "de":"Der Spielserver war nicht erreichbar, die Sprache wurde nicht geändert.",
                  "tr":"Oyun sunucusuna ulaşılamadı, dil değiştirilmedi."},
 "lang_busy":    {"pl":"Język jest właśnie zmieniany. Gra restartuje się w ramach tej zmiany.","en":"The language is being changed. The game restarts as part of it.",
                  "de":"Die Sprache wird gerade umgestellt. Das Spiel startet dabei neu.",
                  "tr":"Dil değiştiriliyor. Oyun bu sırada yeniden başlıyor."},
 "lang_failed":  {"pl":"Nie udało się zmienić języka, więc pozostał bez zmian.","en":"The language could not be changed, so it is unchanged.",
                  "de":"Die Sprache konnte nicht geändert werden, sie bleibt also unverändert.",
                  "tr":"Dil değiştirilemedi, bu yüzden değişmedi."},
 "lang_unsup":   {"pl":"Te pliki serwera nie mają tego języka w całości, więc nic nie zmieniono.","en":"These server files do not carry that language in full, so nothing was changed.",
                  "de":"Diese Serverdateien enthalten diese Sprache nicht vollständig, es wurde nichts geändert.",
                  "tr":"Bu sunucu dosyaları o dili eksiksiz içermiyor, hiçbir şey değiştirilmedi."},
 # --- what a player has to do to their own copy of the game ---
 "lang_cl_t":    {"pl":"Gracze, którzy już pobrali grę","en":"Players who already downloaded the game","de":"Spieler, die das Spiel schon heruntergeladen haben","tr":"Oyunu zaten indirmiş oyuncular"},
 "lang_cl":      {"pl":"Pobieranie na tej stronie jest zbudowane tak, by pasowało, więc każdy, kto pobierze grę od teraz, ma już {lang}. Kto pobrał wcześniej, zmienia własną kopię w dwóch krokach, w folderze gry:","en":"The download on this page is built to match, so anybody who gets the game from now on is already in {lang}. Somebody who downloaded it earlier changes their own copy in two steps, in the folder the game is in:",
                  "de":"Der Download auf dieser Seite wird passend gebaut — wer das Spiel ab jetzt holt, hat bereits {lang}. Wer es früher heruntergeladen hat, stellt seine eigene Kopie in zwei Schritten um, im Ordner des Spiels:",
                  "tr":"Bu sayfadaki indirme buna uygun hazırlanır; oyunu bundan sonra alan herkes zaten {lang} dilinde olur. Daha önce indirmiş olan kendi kopyasını, oyunun bulunduğu klasörde iki adımda değiştirir:"},
 "lang_cl_1":    {"pl":"Zmień nazwę locale.cfg na locale_old.cfg","en":"Rename locale.cfg to locale_old.cfg","de":"locale.cfg in locale_old.cfg umbenennen","tr":"locale.cfg dosyasını locale_old.cfg olarak yeniden adlandır"},
 "lang_cl_2":    {"pl":"Zmień nazwę {file} na locale.cfg","en":"Rename {file} to locale.cfg","de":"{file} in locale.cfg umbenennen","tr":"{file} dosyasını locale.cfg olarak yeniden adlandır"},
 "lang_cl_3":    {"pl":"Każdy język jest już w folderze, więc nie trzeba niczego pobierać ponownie. Gra musi być w tym czasie zamknięta.","en":"Every language is already in the folder, so nothing has to be downloaded again. The game has to be closed while you do it.",
                  "de":"Alle Sprachen liegen bereits im Ordner, es muss also nichts erneut heruntergeladen werden. Das Spiel muss dabei geschlossen sein.",
                  "tr":"Bütün diller zaten klasörde, bu yüzden hiçbir şeyi yeniden indirmek gerekmez. Bunu yaparken oyun kapalı olmalı."},
 "dl_lang":      {"pl":"Gra jest w języku: {lang}.","en":"The game is in {lang}.","de":"Das Spiel ist auf {lang}.","tr":"Oyun {lang} dilinde."},
 # Deliberately NOT "tip_lang": that one belongs to the switcher at the top of
 # the page, which changes this panel and nothing else. Two settings with almost
 # the same name is exactly how somebody ends up restarting a server they only
 # meant to read in German.
 "tip_gamelang": {"pl":"Zmienia język, którym mówi serwer — teksty zadań, komunikaty, nazwy przedmiotów i potworów. Gra restartuje się na chwilę.","en":"Changes the language the server speaks — quest text, system messages, item and monster names. The game restarts briefly.",
                  "de":"Ändert die Sprache, die der Server spricht — Questtexte, Systemmeldungen, Item- und Monsternamen. Das Spiel startet dabei kurz neu.",
                  "tr":"Sunucunun konuştuğu dili değiştirir — görev metinleri, sistem mesajları, eşya ve canavar adları. Oyun kısa süre yeniden başlar."},
 "gm_title":     {"pl":"🛡️ Mistrz gry","en":"🛡️ Game master","de":"🛡️ Spielleiter","tr":"🛡️ Oyun yöneticisi"},
 "gm_apply":     {"pl":"🛡️ Nadaj rangę","en":"🛡️ Set rank","de":"🛡️ Rang setzen","tr":"🛡️ Rütbeyi ayarla"},
 "gm_none":      {"pl":"Zwykły gracz (bez rangi)","en":"Normal player (no rank)","de":"Normaler Spieler (kein Rang)","tr":"Normal oyuncu (rütbe yok)"},
 "gm_now":       {"pl":"Obecnie: {rank}","en":"Currently: {rank}","de":"Aktuell: {rank}","tr":"Şu an: {rank}"},
 "gm_granted":   {"pl":"{name} ma teraz rangę {rank}. Działa od razu, w grze.","en":"{name} is now {rank}. It applies right away, in game.",
                  "de":"{name} ist jetzt {rank}. Das gilt sofort, im laufenden Spiel.",
                  "tr":"{name} artık {rank}. Bu, oyun içinde hemen geçerli olur."},
 "gm_removed":   {"pl":"{name} jest znów zwykłym graczem. Komendy zachowuje do wylogowania i ponownego zalogowania.","en":"{name} is a normal player again. They keep the commands until they log out and back in.",
                  "de":"{name} ist wieder normaler Spieler. Die Befehle bleiben ihm, bis er sich aus- und wieder einloggt.",
                  "tr":"{name} yeniden normal oyuncu. Çıkıp tekrar girene kadar komutlar onda kalır."},
 "gm_noreload":  {"pl":"Zapisano, ale nie udało się powiadomić serwera gry. Zadziała przy następnym uruchomieniu serwera.","en":"Saved, but the game server could not be told. It will take effect the next time the server starts.",
                  "de":"Gespeichert, aber der Spielserver konnte nicht benachrichtigt werden. Es wirkt beim nächsten Serverstart.",
                  "tr":"Kaydedildi, ancak oyun sunucusuna bildirilemedi. Sunucu bir sonraki başlangıçta geçerli olacak."},
 "cat_all":      {"pl":"Wszystkie","en":"All","de":"Alle","tr":"Hepsi"},
 "cat_weapon":   {"pl":"Bronie","en":"Weapons","de":"Waffen","tr":"Silahlar"},
 "cat_armor":    {"pl":"Zbroje","en":"Armor","de":"Rüstung","tr":"Zırhlar"},
 "cat_usable":   {"pl":"Mikstury/Użytkowe","en":"Potions/Usable","de":"Tränke/Nutzbar","tr":"İksir/Kullanılabilir"},
 "cat_ds":       {"pl":"Smocze Kamienie","en":"Dragon Soul","de":"Drachenseele","tr":"Ejder Ruhu"},
 "cat_metin":    {"pl":"Kamienie Metin","en":"Metin Stones","de":"Metinsteine","tr":"Metin Taşları"},
 "cat_special":  {"pl":"Specjalne","en":"Special","de":"Spezial","tr":"Özel"},
 "cat_other":    {"pl":"Inne","en":"Other","de":"Sonstige","tr":"Diğer"},
 # --- status & error messages ---
 "db_down":      {"pl":"Nie można teraz połączyć się z bazą gry. Serwer może być wyłączony — spróbuj za chwilę. 🙏","en":"The game database can't be reached right now. The server may be down — please try again in a bit. 🙏",
                  "de":"Die Spiel-Datenbank ist gerade nicht erreichbar. Der Server ist vielleicht aus — bitte versuche es gleich noch einmal. 🙏",
                  "tr":"Oyun veritabanına şu anda ulaşılamıyor. Sunucu kapalı olabilir — birazdan tekrar dene. 🙏"},
 "csrf_bad":     {"pl":"Dla bezpieczeństwa to żądanie zostało zablokowane — nie przyszło z tej strony. Otwórz panel ponownie i spróbuj jeszcze raz. 🔒","en":"For your safety that request was blocked — it didn't come from this page. Please open the panel again and retry. 🔒",
                  "de":"Zu deiner Sicherheit wurde diese Anfrage blockiert — sie kam nicht von dieser Seite. Bitte öffne das Panel neu und versuche es erneut. 🔒",
                  "tr":"Güvenliğin için bu istek engellendi — bu sayfadan gelmedi. Lütfen paneli yeniden aç ve tekrar dene. 🔒"},
 # {max} is MAX_ITEM_COUNT, which depends on the server files (see there):
 # 65,535 on most of them, 255 on the [40250] reference files.
 "qty_range":    {"pl":"Podaj ilość od 1 do {max} — gra nie zmieści więcej w jednym stosie. 🙂","en":"Please enter a quantity between 1 and {max} — the game can't store more than that in one stack. 🙂",
                  "de":"Bitte gib eine Menge zwischen 1 und {max} ein — mehr passt im Spiel nicht in einen Stapel. 🙂",
                  "tr":"Lütfen 1 ile {max} arasında bir adet gir — oyun tek yığında bundan fazlasını saklayamaz. 🙂"},
 # {conf} is the config file this panel actually read — it is not always
 # /usr/local/etc/m2panel.conf any more (see M2PANEL_CONF).
 "inv_full":     {"pl":"Ekwipunek jest pełny — poproś gracza o zwolnienie miejsca. 🎒 (Jeśli jego serwer ma więcej stron ekwipunku, podnieś \"inventory_slots\" w {conf}.)","en":"The inventory is full — ask the player to free up some space. 🎒 (If their server has more inventory pages, raise \"inventory_slots\" in {conf}.)",
                  "de":"Das Inventar ist voll — bitte den Spieler, Platz zu schaffen. 🎒 (Hat der Server mehr Inventarseiten, erhöhe \"inventory_slots\" in {conf}.)",
                  "tr":"Envanter dolu — oyuncudan yer açmasını iste. 🎒 (Sunucunda daha fazla envanter sayfası varsa {conf} içindeki \"inventory_slots\" değerini artır.)"},
 "ingame_offline":{"pl":"🙂 {name} nie jest teraz w grze, a ta akcja działa tylko, gdy gracz jest online. Nic nie zmieniono — poproś o zalogowanie i spróbuj ponownie.","en":"🙂 {name} isn't in game right now, and this action only works while they're online. Nothing was changed — ask them to log in and try again.",
                  "de":"🙂 {name} ist gerade nicht im Spiel, und diese Aktion funktioniert nur online. Es wurde nichts geändert — bitte einloggen lassen und erneut versuchen.",
                  "tr":"🙂 {name} şu anda oyunda değil ve bu işlem yalnızca oyundayken çalışır. Hiçbir şey değiştirilmedi — giriş yapmasını isteyip tekrar dene."},
 "ingame_timeout":{"pl":"⏳ Pomocnik w grze nie odpowiedział, więc nie udało się dostarczyć tej akcji graczowi {name}. Nic nie zmieniono — sprawdź, czy serwer gry działa, i spróbuj za chwilę.","en":"⏳ The in-game helper didn't answer, so this action couldn't be delivered to {name}. Nothing was changed — check that the game server is running and try again in a moment.",
                  "de":"⏳ Der Ingame-Helfer hat nicht geantwortet, deshalb konnte die Aktion {name} nicht zugestellt werden. Es wurde nichts geändert — prüfe, ob der Spielserver läuft, und versuche es gleich noch einmal.",
                  "tr":"⏳ Oyun içi yardımcı yanıt vermedi, bu yüzden işlem {name} oyuncusuna iletilemedi. Hiçbir şey değiştirilmedi — oyun sunucusunun çalıştığını kontrol et ve az sonra tekrar dene."},
 "act_done":     {"pl":"✅ Gotowe! Akcja została natychmiast zastosowana dla gracza {name}.","en":"✅ Done! The action was applied instantly for {name}.",
                  "de":"✅ Fertig! Die Aktion wurde sofort für {name} angewendet.",
                  "tr":"✅ Tamam! İşlem {name} için anında uygulandı."},
 # Used when nothing is listening in the game, rather than when the player is
 # away -- saying "they weren't in game" to somebody who is standing in it
 # sends them looking for a problem with their character.
 "act_nohelper": {"pl":"✅ Zapisano na koncie gracza {name} — pojawi się przy następnym zalogowaniu.","en":"✅ Written to {name}'s account — it appears the next time they log in.",
                  "de":"✅ Auf das Konto von {name} geschrieben — es erscheint beim nächsten Einloggen.",
                  "tr":"✅ {name} adlı oyuncunun hesabına yazıldı — bir sonraki girişte görünür."},
 "ingame_nohelper":{"pl":"⛔ Nic w grze nie odpowiedziało, więc {name} nie został przeniesiony i nic nie zmieniono. Teleport i szybkość biegu działają tylko na zalogowanej postaci, na serwerze z uruchomionym pomocnikiem w grze.","en":"⛔ Nothing in the game answered, so {name} was not moved and nothing was changed. Teleport and running speed only work on a character who is logged in, on a server whose in-game helper is running.",
                  "de":"⛔ Aus dem Spiel kam keine Antwort, {name} wurde also nicht bewegt und nichts geändert. Teleportieren und Laufgeschwindigkeit wirken nur auf einen eingeloggten Charakter, auf einem Server, dessen In-Game-Helfer läuft.",
                  "tr":"⛔ Oyundan yanıt gelmedi, bu yüzden {name} taşınmadı ve hiçbir şey değişmedi. Işınlama ve koşma hızı yalnızca giriş yapmış bir karakterde ve oyun içi yardımcısı çalışan bir sunucuda işe yarar."},
 "act_offline":  {"pl":"✅ {name} nie był teraz w grze — zapisano na koncie i będzie gotowe po zalogowaniu! 🎉","en":"✅ {name} wasn't in game right now — it was applied to their account and will be ready when they log in! 🎉",
                  "de":"✅ {name} war gerade nicht im Spiel — es wurde direkt auf dem Konto angewendet und ist beim nächsten Login da! 🎉",
                  "tr":"✅ {name} şu anda oyunda değildi — işlem hesabına uygulandı, giriş yaptığında hazır olacak! 🎉"},
 "act_late_done":{"pl":"✅ W samą porę! {name} odebrał to w grze, gdy czekaliśmy, więc zastosowano tam — nie dwa razy. 🎉","en":"✅ Just in time! {name} picked it up in game while we were waiting, so it was applied there — not twice. 🎉",
                  "de":"✅ Gerade noch rechtzeitig! {name} hat es im Spiel erhalten, während wir gewartet haben — es wurde also nur einmal angewendet. 🎉",
                  "tr":"✅ Tam zamanında! {name} beklerken bunu oyun içinde aldı, yani işlem iki kez değil bir kez uygulandı. 🎉"},
 "act_late_other":{"pl":"ℹ️ Pomocnik w grze przejął to, gdy czekaliśmy (stan: {status}), więc nic nie zastosowano dwa razy. Sprawdź gracza {name} i w razie potrzeby spróbuj ponownie.","en":"ℹ️ The in-game helper took this over while we were waiting (status: {status}), so nothing was applied twice. Please check {name} and try again if needed.",
                  "de":"ℹ️ Der Ingame-Helfer hat das während des Wartens übernommen (Status: {status}) — es wurde also nichts doppelt angewendet. Bitte prüfe {name} und versuche es bei Bedarf erneut.",
                  "tr":"ℹ️ Oyun içi yardımcı bunu beklerken devraldı (durum: {status}), bu yüzden hiçbir şey iki kez uygulanmadı. Lütfen {name} oyuncusunu kontrol et ve gerekirse tekrar dene."},
 "act_error":    {"pl":"Coś poszło nie tak ({status}), ale bez obaw — po prostu spróbuj ponownie.","en":"Something went wrong ({status}), but no worries — you can just try again.",
                  "de":"Etwas ist schiefgelaufen ({status}), aber keine Sorge — versuche es einfach noch einmal.",
                  "tr":"Bir şeyler ters gitti ({status}) ama merak etme — tekrar deneyebilirsin."},
 "act_novalue":  {"pl":"Wygląda na to, że nie wpisano wartości — spróbujesz jeszcze raz? 🙂","en":"Looks like you forgot to enter a value — could you try again? 🙂",
                  "de":"Da fehlt wohl noch ein Wert — magst du es noch einmal versuchen? 🙂",
                  "tr":"Görünüşe göre bir değer girmeyi unuttun — tekrar dener misin? 🙂"},
 "act_unexpected":{"pl":"Wystąpił nieoczekiwany problem. Spróbuj ponownie; jeśli się powtarza, powiedz osobie, która postawiła serwer. 🙏","en":"An unexpected problem occurred. Try again; if it keeps happening, tell the person who set up the server. 🙏",
                  "de":"Es gab ein unerwartetes Problem. Versuche es erneut; wenn es bleibt, sag der Person Bescheid, die den Server eingerichtet hat. 🙏",
                  "tr":"Beklenmeyen bir sorun oluştu. Tekrar dene; devam ederse sunucuyu kuran kişiye haber ver. 🙏"},
 "not_found":    {"pl":"Nie znaleziono gracza.","en":"Player not found.","de":"Spieler nicht gefunden.","tr":"Oyuncu bulunamadı."},
 # --- server rates ---
   # --- discord ---
   "dc_foot":      {"pl":"Discord — nowości, aktualizacje i pomoc","en":"Discord — news, updates and help",
                    "de":"Discord — Neuigkeiten, Updates und Hilfe",
                    "tr":"Discord — haberler, güncellemeler ve yardım"},
   "dc_title":     {"pl":"💬 Wpadnij na Discorda","en":"💬 Come to the Discord",
                    "de":"💬 Komm auf den Discord",
                    "tr":"💬 Discord'a gel"},
   "dc_body":      {"pl":"Wszystko, co aktualne, jest tam najpierw: co zmieniła ostatnia aktualizacja, kiedy serwer ma przerwę techniczną, i ludzie, którzy odpowiedzą szybciej, niż zdążysz poszukać. Znalazłeś błąd? Zgłoś go tam — to najszybsza droga do poprawki.","en":"Everything current is there first: what changed in the last update, when the server is down for maintenance, and the people who can answer a question faster than you can search for it. Found a bug? Report it there — it is the quickest way to get it fixed.",
                    "de":"Alles Aktuelle steht dort zuerst: was sich mit dem letzten Update geändert hat, wann der Server für Wartungen weg ist, und die Leute, die eine Frage schneller beantworten, als du sie suchen kannst. Fehler gefunden? Melde ihn dort — das ist der schnellste Weg, ihn loszuwerden.",
                    "tr":"Güncel olan her şey önce orada: son güncellemede ne değişti, sunucu bakım için ne zaman kapalı olacak ve bir soruyu aramandan daha hızlı yanıtlayacak insanlar. Hata mı buldun? Oraya bildir — düzeltilmesinin en hızlı yolu."},
   "dc_btn":       {"pl":"💬 Dołącz do Discorda","en":"💬 Join the Discord",
                    "de":"💬 Discord beitreten",
                    "tr":"💬 Discord'a katıl"},
   "coffee_btn":   {"pl":"☕ Postaw kawkę","en":"☕ Buy me a coffee",
                    "de":"☕ Spendiere einen Kaffee",
                    "tr":"☕ Bir kahve ısmarla"},
 "rates_nav":    {"pl":"⚙️ Mnożniki serwera","en":"⚙️ Server rates","de":"⚙️ Server-Raten","tr":"⚙️ Sunucu oranları"},
 "rates_open":   {"pl":"⚙️ Otwórz mnożniki serwera","en":"⚙️ Open server rates","de":"⚙️ Server-Raten öffnen","tr":"⚙️ Sunucu oranlarını aç"},
 "rates_dash_hint":{"pl":"Spraw, by cały serwer dawał więcej doświadczenia, przedmiotów i yang — wygodne, jeśli wolisz robić zadania niż grindować.","en":"Make the whole server give more experience, more items and more yang — handy if you would rather do quests than grind.",
                  "de":"Lass den ganzen Server mehr Erfahrung, mehr Gegenstände und mehr Yang geben — praktisch, wenn du lieber Quests machst als zu grinden.",
                  "tr":"Tüm sunucunun daha çok tecrübe, daha çok eşya ve daha çok yang vermesini sağla — grind yerine görev yapmayı seviyorsan çok işine yarar."},
 "rates_intro":  {"pl":"Te trzy liczby decydują, jak szybko toczy się cały serwer. 100% to dokładnie tak, jak gra została stworzona — wyżej znaczy szybciej. Zapis restartuje serwer gry, więc grający zostają na chwilę rozłączeni.","en":"These three numbers decide how quickly the whole server moves. 100% is exactly how the game was made — higher means faster. Saving restarts the game server, so anyone playing gets dropped for a moment.",
                  "de":"Diese drei Zahlen bestimmen, wie schnell der ganze Server läuft. 100% ist genau so, wie das Spiel gemacht wurde — höher heißt schneller. Beim Speichern wird der Spielserver neu gestartet, wer gerade spielt fliegt also kurz raus.",
                  "tr":"Bu üç sayı tüm sunucunun ne kadar hızlı ilerlediğini belirler. 100%, oyunun yapıldığı hâlidir — yüksek olması daha hızlı demektir. Kaydetmek oyun sunucusunu yeniden başlatır, o sırada oynayan varsa kısa bir süre düşer."},
 "rates_exp":    {"pl":"Doświadczenie","en":"Experience","de":"Erfahrung","tr":"Tecrübe"},
 "rates_exp_help":{"pl":"Ile doświadczenia dają potwory. 300% oznacza, że poziomy rosną trzy razy szybciej.","en":"How much experience monsters give. 300% means levelling up goes three times as fast.",
                  "de":"Wie viel Erfahrung Monster geben. 300% heißt, du levelst dreimal so schnell.",
                  "tr":"Canavarların verdiği tecrübe. 300% seviye atlamanın üç kat hızlanması demek."},
 "rates_drop":   {"pl":"Drop przedmiotów","en":"Item drop","de":"Gegenstände","tr":"Eşya düşme"},
 "rates_drop_help":{"pl":"Jak często potwory upuszczają przedmioty. 200% to dwa razy więcej dropu. Szansa nie może przekroczyć pewności, więc to, co zawsze wypadało, po prostu wypada dalej.","en":"How often monsters drop items. 200% means twice as many drops. A chance can never go above certain, so items that always dropped simply keep dropping.",
                  "de":"Wie oft Monster Gegenstände fallen lassen. 200% heißt doppelt so viele. Mehr als sicher geht nicht — was immer gedroppt ist, droppt einfach weiter.",
                  "tr":"Canavarların ne sıklıkla eşya düşürdüğü. 200% iki katı düşme demek. Bir ihtimal kesinliğin üstüne çıkamaz, yani zaten hep düşen eşyalar düşmeye devam eder."},
 "rates_yang":   {"pl":"Yang","en":"Yang","de":"Yang","tr":"Yang"},
 "rates_yang_help":{"pl":"Ile pieniędzy upuszczają potwory po zabiciu.","en":"How much money monsters drop when you kill them.",
                  "de":"Wie viel Geld Monster fallen lassen, wenn du sie besiegst.",
                  "tr":"Canavarları öldürdüğünde düşen para miktarı."},
 "rates_percent":{"pl":"Procent — 100 to normalnie","en":"Percent — 100 is normal","de":"Prozent — 100 ist normal","tr":"Yüzde — 100 normaldir"},
 "rates_current":{"pl":"Aktywne teraz","en":"Active right now","de":"Gerade aktiv","tr":"Şu anda geçerli"},
 "rates_presets":{"pl":"Albo wybierz jeden z tych","en":"Or take one of these","de":"Oder nimm eine davon","tr":"Ya da bunlardan birini seç"},
 "rates_presets_hint":{"pl":"Jedno kliknięcie wypełnia trzy pola — i tak musisz zapisać.","en":"One tap fills the three boxes in — you still have to save.",
                  "de":"Ein Tipp füllt die drei Felder aus — speichern musst du trotzdem noch.",
                  "tr":"Bir dokunuş üç kutuyu doldurur — yine de kaydetmen gerekir."},
 "rates_p_normal":{"pl":"🎯 Normalnie — dokładnie jak w oryginalnej grze","en":"🎯 Normal — exactly like the original game",
                  "de":"🎯 Normal — genau wie im Originalspiel",
                  "tr":"🎯 Normal — orijinal oyundaki gibi"},
 "rates_p_relaxed":{"pl":"🌿 Spokojne zadania — doświadczenie 300%, przedmioty 200%, yang 200%","en":"🌿 Relaxed questing — experience 300%, items 200%, yang 200%",
                  "de":"🌿 Entspannt questen — Erfahrung 300%, Gegenstände 200%, Yang 200%",
                  "tr":"🌿 Rahat görev — tecrübe 300%, eşya 200%, yang 200%"},
 "rates_p_fast": {"pl":"🚀 Szybko — doświadczenie 1000%, przedmioty 500%, yang 500%","en":"🚀 Fast — experience 1000%, items 500%, yang 500%",
                  "de":"🚀 Schnell — Erfahrung 1000%, Gegenstände 500%, Yang 500%",
                  "tr":"🚀 Hızlı — tecrübe 1000%, eşya 500%, yang 500%"},
 "rates_save":   {"pl":"💾 Zapisz i zrestartuj serwer","en":"💾 Save and restart the server","de":"💾 Speichern und Server neu starten","tr":"💾 Kaydet ve sunucuyu yeniden başlat"},
 "easter_nav":   {"pl":"🐇 Event wielkanocny","en":"🐇 Easter event","de":"🐇 Oster-Event","tr":"🐇 Paskalya etkinliği"},
 "easter_open":  {"pl":"🐇 Otwórz event wielkanocny","en":"🐇 Open the Easter event","de":"🐇 Oster-Event öffnen","tr":"🐇 Paskalya etkinliğini aç"},
 "tip_easter":   {"pl":"Metiny wielkanocne, jajka i Wielkanocny Zając. Włącza się od razu, bez restartu serwera.","en":"Easter metins, eggs and the Easter Bunny. Switches on immediately, no server restart.",
                  "de":"Oster-Metins, Eier und der Osterhase. Schaltet sich sofort ein, kein Server-Neustart.",
                  "tr":"Paskalya metinleri, yumurtalar ve Paskalya Tavşanı. Sunucu yeniden başlatmadan hemen açılır."},
 "easter_dash_hint":{"pl":"Włącz albo wyłącz sezonowe metiny wielkanocne i Wielkanocnego Zająca.","en":"Turn the seasonal Easter metins and the Easter Bunny on or off.",
                  "de":"Schalte die saisonalen Oster-Metins und den Osterhasen ein oder aus.",
                  "tr":"Mevsimsel Paskalya metinlerini ve Paskalya Tavşanı'nı aç veya kapat."},
 "easter_intro": {"pl":"Dwa niezależne przełączniki dla sezonowego eventu wielkanocnego. Zapis działa natychmiast, bez restartu serwera — nikt nie zostaje rozłączony.","en":"Two independent switches for the seasonal Easter event. Saving takes effect immediately, no server restart — nobody gets disconnected.",
                  "de":"Zwei unabhängige Schalter für das saisonale Oster-Event. Speichern wirkt sofort, kein Server-Neustart — niemand wird getrennt.",
                  "tr":"Mevsimsel Paskalya etkinliği için iki bağımsız anahtar. Kaydetmek hemen etkili olur, sunucu yeniden başlatılmaz — kimsenin bağlantısı kesilmez."},
 "easter_drop":  {"pl":"Metiny wielkanocne","en":"Easter metins","de":"Oster-Metins","tr":"Paskalya metinleri"},
 "easter_drop_help":{"pl":"Główny przełącznik eventu: zabicie metina lub zwykłego potwora ma szansę zespawnować metina wielkanocnego, który dropi jajka.","en":"The event's main switch: killing a metin or a regular monster has a chance to spawn an Easter metin, which drops eggs.",
                  "de":"Der Hauptschalter des Events: das Töten eines Metins oder eines normalen Monsters kann einen Oster-Metin spawnen, der Eier fallen lässt.",
                  "tr":"Etkinliğin ana anahtarı: bir metin ya da normal bir canavarı öldürmek, yumurta düşüren bir Paskalya metini spawn etme şansı verir."},
 "easter_rabbit":{"pl":"Wielkanocny Zając","en":"Easter Bunny","de":"Osterhase","tr":"Paskalya Tavşanı"},
 "easter_rabbit_help":{"pl":"Dodaje questodawcę na mapach 1/21/41, u którego gracze wymieniają zebrane jajka na nagrody.","en":"Adds a quest NPC on maps 1/21/41 where players trade collected eggs for rewards.",
                  "de":"Fügt einen Quest-NPC auf den Karten 1/21/41 hinzu, bei dem Spieler gesammelte Eier gegen Belohnungen eintauschen.",
                  "tr":"1/21/41 haritalarına, oyuncuların topladıkları yumurtaları ödüllerle takas ettiği bir görev NPC'si ekler."},
 "easter_enable":{"pl":"Włączony","en":"Enabled","de":"Aktiviert","tr":"Etkin"},
 "easter_on":    {"pl":"włączone","en":"on","de":"an","tr":"açık"},
 "easter_off":   {"pl":"wyłączone","en":"off","de":"aus","tr":"kapalı"},
 "easter_save":  {"pl":"💾 Zapisz — działa od razu","en":"💾 Save — takes effect immediately","de":"💾 Speichern — wirkt sofort","tr":"💾 Kaydet — hemen etkili olur"},
 "easter_saved_live":{"pl":"Zapisano i włączono na żywo, przez pomocnika w grze. 🐇","en":"Saved and switched live, through the in-game helper. 🐇",
                  "de":"Gespeichert und live umgeschaltet, über den In-Game-Helfer. 🐇",
                  "tr":"Kaydedildi ve oyun içi yardımcı aracılığıyla canlı olarak değiştirildi. 🐇"},
 "easter_saved_persisted":{"pl":"Zapisano w bazie. Nikt teraz online nie potwierdził zmiany na żywo — zadziała najpóźniej po najbliższym restarcie serwera gry.","en":"Saved to the database. Nobody confirmed it live just now — it will take effect at the latest on the game server's next restart.",
                  "de":"In der Datenbank gespeichert. Gerade hat niemand die Änderung live bestätigt — sie wirkt spätestens beim nächsten Neustart des Spielservers.",
                  "tr":"Veritabanına kaydedildi. Şu anda kimse canlı olarak onaylamadı — en geç oyun sunucusunun bir sonraki yeniden başlatmasında etkili olacak."},
 "rare_nav":     {"pl":"🐉 Alchemia i szarfy","en":"🐉 Alchemy and sashes"},
 "rare_open":    {"pl":"🐉 Otwórz alchemię i szarfy","en":"🐉 Open alchemy and sashes"},
 "tip_rare":     {"pl":"Włącz albo wyłącz zdobywanie Cor Draconis i szarf. Działa od razu, bez restartu serwera.","en":"Switch getting Cor Draconis and sashes on or off. Takes effect immediately, no server restart."},
 "rare_dash_hint":{"pl":"Czy na tym świecie wypadają Cor Draconis (alchemia) i szarfy.","en":"Whether Cor Draconis (alchemy) and sashes drop in this world."},
 "rare_intro":   {"pl":"Dwa niezależne przełączniki. Wyłączenie zatrzymuje tylko nowe przedmioty — to, co gracze już mają, zostaje, a plecak alchemii dalej działa. Zapis działa od razu, bez restartu. Ustawienie z panelu zostaje po restarcie, dopóki ktoś nie zmieni M2_ALCHEMY / M2_SASHES w .env.","en":"Two independent switches. Switching off stops only new items — what players already have stays, and the alchemy bag keeps working. Saving takes effect immediately, no restart. The panel's setting survives restarts until M2_ALCHEMY / M2_SASHES in .env is changed."},
 "rare_alchemy": {"pl":"Alchemia (Cor Draconis)","en":"Alchemy (Cor Draconis)"},
 "rare_alchemy_help":{"pl":"Cor Draconis z Metinów i bossów, Odłamki Smoczego Kamienia z potworów oraz ich wymiana na Cory u Alchemika (quest od 30 poziomu).","en":"Cor Draconis from Metins and bosses, Dragon Stone Shards from monsters and their exchange for Cors at the Alchemist (the level-30 quest)."},
 "rare_sashes":  {"pl":"Szarfy","en":"Sashes"},
 "rare_sashes_help":{"pl":"Szarfy z bossów i ze skrzyń bossów. Łączenie szarf u Uriela działa zawsze.","en":"Sashes from bosses and boss chests. Combining sashes at Uriel always works."},
 "rare_saved_live":{"pl":"Zapisano i przełączono na żywo, przez pomocnika w grze. 🐉","en":"Saved and switched live, through the in-game helper. 🐉"},
 "rare_ds_title":{"pl":"Odłamki Smoczego Kamienia (Alchemik)","en":"Dragon Stone Shards (the Alchemist)"},
 "rare_ds_help": {"pl":"Od 30 poziomu Alchemik (M1 każdego królestwa) daje Moc Smoczego Oka: potwory upuszczają Odłamki, a każde 10 podniesionych odłamków samo zamienia się w Cor Draconis — tyle razy dziennie, ile ustawisz niżej, odnawiane raz dziennie u Alchemika. Boty liczą tak samo (bez odłamków w plecaku). Działa od razu.","en":"From level 30 the Alchemist (each kingdom's M1) grants the Power of the Dragon Eye: monsters drop Shards, and every 10 shards picked up turn into a Cor Draconis by themselves — as many times a day as set below, renewed once a day at the Alchemist. Bots count the same way (without shards in the bag). Applies at once."},
 "rare_ds_drop": {"pl":"Szansa na odłamek z potwora, % (1–100, domyślnie 10):","en":"Shard chance per kill, % (1-100, default 10):"},
 "rare_ds_day":  {"pl":"Cor Draconis z odłamków dziennie (1–20, domyślnie 5):","en":"Cor Draconis from shards a day (1-20, default 5):"},
 "rare_ds_range":{"pl":"Szansa na odłamek 1–100%, Cor dziennie 1–20. Nic nie zmieniono.","en":"Shard chance 1-100%, Cors a day 1-20. Nothing was changed."},
 "az_nav":       {"pl":"🗺️ Moduł Arezzo","en":"🗺️ Arezzo module"},
 "az_open":      {"pl":"🗺️ Otwórz moduł Arezzo","en":"🗺️ Open the Arezzo module"},
 "tip_az":       {"pl":"Włącz albo wyłącz nowe mapy i lochy z Arezzo. Działa od razu, bez restartu serwera.","en":"Switch the new Arezzo maps and dungeons on or off. Takes effect immediately, no server restart."},
 "az_dash_hint": {"pl":"Czy na tym świecie są dostępne mapy i lochy z Arezzo (moduł dobrowolny).","en":"Whether the Arezzo maps and dungeons are open in this world (an optional module)."},
 "az_intro":     {"pl":"Moduł dobrowolny: Dolina Cyklopów, Pustkowie Faraona, Zaczarowany Las oraz lochy Biblioteka Wiedzy, Wzgórze Wukonga, Ruiny Skorpiona i Starożytna Dżungla. Klient (2.0.30 i nowszy) ma wszystkie pliki zawsze — ten przełącznik decyduje tylko, czy serwer do nich prowadzi. Zapis działa od razu, bez restartu. Ustawienie z panelu zostaje po restarcie, dopóki ktoś nie zmieni M2_AREZZO w .env.","en":"An optional module: Dolina Cyklopow, Pustkowie Faraona, Zaczarowany Las and the dungeons Biblioteka Wiedzy, Wzgorze Wukonga, Ruiny Skorpiona and Starozytna Dzungla. The client (2.0.30 and newer) always has the files - this switch only decides whether the server leads there. Saving takes effect immediately, no restart. The panel's setting survives restarts until M2_AREZZO in .env is changed."},
 "az_enable":    {"pl":"Moduł Arezzo włączony","en":"Arezzo module on"},
 "az_help":      {"pl":"Włączony: strony w Teleporterze i Pierścieniu, portal do Zaczarowanego Lasu po Strażniku En-Tai, strażnicy wejść do lochów i lochy Arezzo w oknie „Wyprawy” (X). Wyłączony: nic z tego nie jest widoczne, a gracz, który stoi na mapie Arezzo albo jest w jej lochu, w ciągu kilku sekund wraca do miasta (GM może zostać).","en":"On: the Teleporter and ring pages, the Enchanted Forest portal after the En-Tai Guardian, the dungeon entrance guards and the Arezzo lines in the dungeon window (X). Off: none of it is visible, and a player standing on an Arezzo map or in one of its dungeons is sent to the town within seconds (a GM may stay)."},
 "az_saved_live":{"pl":"Zapisano i przełączono na żywo, przez pomocnika w grze. 🗺️","en":"Saved and switched live, through the in-game helper. 🗺️"},
 "seon_nav":     {"pl":"💎 Seon-Hae: 6. i 7. bonus","en":"💎 Seon-Hae: 6th and 7th bonus"},
 "seon_open":    {"pl":"💎 Otwórz Seon-Hae","en":"💎 Open Seon-Hae"},
 "tip_seon":     {"pl":"Włącz albo wyłącz dodawanie 6. i 7. bonusu u Seon-Hae i ustaw czas, przez jaki trzyma przedmiot. Działa od razu, bez restartu serwera.","en":"Switch Seon-Hae's 6th and 7th bonus on or off and set how long he keeps an item. Takes effect immediately, no server restart."},
 "seon_dash_hint":{"pl":"Czy Seon-Hae (NPC 20095) dodaje graczom 6. i 7. bonus (system dobrowolny).","en":"Whether Seon-Hae (NPC 20095) adds the 6th and 7th bonus for players (an optional system)."},
 "seon_intro":   {"pl":"System dobrowolny (z Owsapa): gracz oddaje Seon-Hae (NPC 20095 w pierwszych wioskach) broń albo zbroję/biżuterię z pięcioma bonusami, Odłamki poziomu przedmiotu (39070–39077, 39081; 2% szansy za każdy, do 10) i Suplementy (72064–72067; do 5, do +50%). Seon-Hae trzyma przedmiot przez ustawiony czas i oddaje go z nowym bonusem albo bez. Wymaga klienta z oknem Seon-Hae. Zapis działa od razu, bez restartu. Włączenie z panelu zostaje po restarcie, dopóki ktoś nie zmieni M2_SEONHAE w .env.","en":"An optional system (from Owsap): a player hands Seon-Hae (NPC 20095 in the first villages) a weapon or an armour/jewel with five bonuses, Powershards of the item's level (39070-39077, 39081; 2% chance each, up to 10) and Additives (72064-72067; up to 5, up to +50%). Seon-Hae keeps the item for the set time and gives it back with a new bonus or without. Needs the client with the Seon-Hae window. Saving takes effect immediately, no restart. Switching on here survives restarts until M2_SEONHAE in .env is changed."},
 "seon_enable":  {"pl":"Seon-Hae przyjmuje przedmioty","en":"Seon-Hae takes items"},
 "seon_help":    {"pl":"Wyłączenie zatrzymuje tylko nowe zlecenia — przedmiot, który Seon-Hae już trzyma, zawsze można odebrać. Suplementy wypadają z Metinów i bossów, a Odłamki ze zwykłych potworów w Grocie Wygnańców, Świątyni Ochao i Zaczarowanym Lesie (reguły niżej).","en":"Switching off stops new hand-ins only - an item Seon-Hae already keeps can always be collected. The Additives drop from Metins and bosses, the Powershards from ordinary monsters, in the Grotto of Exile, the Temple of Ochao and the Enchanted Forest (the rules below)."},
 "seon_wait":    {"pl":"Czas pracy Seon-Hae (minuty, 0 = 24 godziny)","en":"Seon-Hae's working time (minutes, 0 = 24 hours)"},
 "seon_saved_live":{"pl":"Zapisano i przełączono na żywo, przez pomocnika w grze. 💎","en":"Saved and switched live, through the in-game helper. 💎"},
 "seon_lv":      {"pl":"Szanse poziomów 6. i 7. bonusu","en":"Odds of each level of the 6th and 7th bonus"},
 "seon_lv_help": {"pl":"Jak często wypada wartość lv1…lv5 z tabeli bonusów 6/7 (Seon-Hae i Zaczarowanie 71051). Liczby to wagi, najlepiej w sumie 100. Poziom nie przekroczy „maks. poziomu” bonusu dla typu przedmiotu. Działa od razu.","en":"How often the lv1…lv5 value of the 6/7 bonus table drops (Seon-Hae and the Enchant 71051). Weights, best summing to 100. Never above the bonus's max level for the item type. Works at once."},
 "seon_drops":   {"pl":"Drop Odłamków i Suplementów","en":"Powershard and Additive drops"},
 "seon_drops_help":{"pl":"Tylko dla prawdziwych graczy (nie botów) i tylko gdy Seon-Hae jest włączony. Suplementy: z Metinów i bossów map z wiersza additive_map, każde zabicie losuje additive_chance (%); wiersz mob <vnum potwora> <vnum suplementu> <liczba> dotyczy jednego potwora (dowolnej rangi, na każdej mapie) zamiast reguły jego mapy. Odłamki: ze zwykłych potworów (nie Metinów, nie bossów) map z wiersza shard_map, szansa shard_chance (%, ułamki dozwolone, np. 0.5), 1 odłamek, kolor według wag z wierszy shard <vnum> <waga>. Brak danego rodzaju wierszy = wartości domyślne; puste pole = same domyślne. Serwer czyta plik w ciągu kilku sekund, bez restartu.","en":"Real players only (not bots) and only while Seon-Hae is on. Additives: from Metins and bosses on the maps of an additive_map line, each kill rolling additive_chance (%); a mob <mob vnum> <additive vnum> <count> line names one monster (any rank, any map) and replaces its map's rule. Shards: from ordinary monsters (not Metins, not bosses) on the maps of a shard_map line, shard_chance (%, fractions allowed, e.g. 0.5), one shard, its colour by the weights of the shard <vnum> <weight> lines. A kind of line missing = the defaults; an empty box = the defaults only. The server reads the file within seconds, no restart."},
 "seon_drops_bad":{"pl":"Nie zapisano reguł dropu - błędne wiersze: %s","en":"Drop rules not saved - wrong lines: %s"},
 # MT2009_PLUS_FLOWER_V1: the Flower Event's settings and rewards (playerbot_flower.h).
 "fl_nav":       {"pl":"🌸 Dzieci Kwiaty","en":"🌸 Flower Event"},
 "fl_open":      {"pl":"🌸 Otwórz ustawienia Dzieci Kwiatów","en":"🌸 Open the Flower Event settings"},
 "tip_fl":       {"pl":"Szanse, koszty i nagrody eventu Dzieci Kwiaty. Działa w ciągu 5 sekund, bez restartu.","en":"Chances, costs and rewards of the Flower Event. Takes effect within 5 seconds, no restart."},
 "fl_dash_hint": {"pl":"Nasiona z potworów, wymiana na latorośle i nagrody za kwiaty. Kiedy event trwa, ustawiasz w „Eventach” (rodzaj „flower”).","en":"Seeds from monsters, the exchange for shoots and the rewards for flowers. When it runs is set on the Events page (kind \"flower\")."},
 "fl_intro":     {"pl":"Gdy event trwa, każdy potwór zabity przez gracza (nie bota) może dać Nasiona Kwiatów. W oknie eventu nasiona zamieniają się w losowe latorośle pięciu kwiatów, a latorośle jednego kwiatu – w nagrodę za ten kwiat. Po końcu eventu przez 7 dni można jeszcze wymieniać to, co zostało. Zapis działa w ciągu 5 sekund, bez restartu.","en":"While the event runs, every monster a player (not a bot) kills may give Flower Seeds. In the event window seeds become random shoots of five flowers, and shoots of one flower become that flower's reward. For 7 days after the event what is left can still be exchanged. Saving takes effect within 5 seconds, no restart."},
 "fl_drop":      {"pl":"Nasiona","en":"Seeds"},
 "fl_seed_chance":{"pl":"Szansa na nasiono z potwora (setne części procenta, 100 = 1%; przy równym poziomie)","en":"Seed chance per monster (hundredths of a percent, 100 = 1%; at an equal level)"},
 "fl_min_level": {"pl":"Najniższy poziom gracza","en":"Lowest player level"},
 "fl_counter_max":{"pl":"Najwięcej nasion / latorośli jednego rodzaju","en":"Most seeds / shoots of one kind"},
 "fl_exchange":  {"pl":"Wymiana","en":"Exchange"},
 "fl_seeds_per_shoot":{"pl":"Nasion za 1 latorośl","en":"Seeds per shoot"},
 "fl_shoots_per_reward":{"pl":"Latorośli jednego kwiatu za 1 nagrodę","en":"Shoots of one flower per reward"},
 "fl_rewards":   {"pl":"Nagrody za kwiaty","en":"Rewards for the flowers"},
 "fl_rewards_help":{"pl":"Co gracz dostaje za latorośle danego kwiatu (numer przedmiotu i ilość za jedną wymianę). Domyślnie pudełka 83023–83027; co jest w pudełkach, zmieniasz w panelu Sebana → „Szkatułki” (grupy 83023–83027, działa po restarcie rdzeni).","en":"What a player gets for the shoots of each flower (item number and count per exchange). By default the boxes 83023-83027; what the boxes hold is the Seban panel's \"Szkatulki\" page (groups 83023-83027, applied at a core restart)."},
 "fl_item":      {"pl":"Przedmiot (VNUM)","en":"Item (VNUM)"},
 "fl_count":     {"pl":"Ilość","en":"Count"},
 "fl_buff":      {"pl":"Kwiaty (bonus na 12 h)","en":"Flowers (a 12 h bonus)"},
 "fl_buff_help": {"pl":"Jeden bonus kwiatu naraz. Szansa w procentach; nieudana próba też zużywa kwiat. Wartości bonusów to wiersze item_proto 25121–25125 (value2 = poziom 1, value4 = za każdy poziom).","en":"One flower bonus at a time. Chances in percent; a failed try uses the flower up too. The bonus values are the item_proto rows 25121-25125 (value2 = level 1, value4 = per level)."},
 "fl_add_rate":  {"pl":"Szansa – pierwszy kwiat (%)","en":"Chance - first flower (%)"},
 "fl_change_rate":{"pl":"Szansa – zamiana na inny kwiat (%)","en":"Chance - switching to another flower (%)"},
 "fl_upgrade_rate":{"pl":"Szansa – ten sam kwiat, poziom wyżej (%)","en":"Chance - the same flower one level up (%)"},
 "fl_max_level": {"pl":"Najwyższy poziom bonusu","en":"Highest bonus level"},
 "fl_use_after": {"pl":"Kwiaty działają też przez 7 dni po evencie","en":"Flowers work for 7 days after the event too"},
 "fl_saved":     {"pl":"Zapisano – serwer użyje nowych ustawień w ciągu 5 sekund. 🌸","en":"Saved - the server uses the new settings within 5 seconds. 🌸"},
 "fl_bad_item":  {"pl":"Nie ma przedmiotu o numerze {v} – ta nagroda nie została zmieniona.","en":"There is no item {v} - that reward was not changed."},
 "fl_write_fail":{"pl":"Nie udało się zapisać pliku ustawień.","en":"Could not write the settings file."},
 "fl_flower_1":  {"pl":"Chryzantema","en":"Chrysanthemum"},
 "fl_flower_2":  {"pl":"Konwalia","en":"May Bell"},
 "fl_flower_3":  {"pl":"Narcyz","en":"Daffodil"},
 "fl_flower_4":  {"pl":"Lilia","en":"Lily"},
 "fl_flower_5":  {"pl":"Słonecznik","en":"Sunflower"},
 "regen_title": {"pl":"Czas odradzania Metinów, bossów i potworów",
                 "en":"Respawn time of Metin stones, bosses and monsters"},
 "regen_help":  {"pl":"Procent zwykłego czasu odradzania: 100 = jak w grze, 50 = dwa razy szybciej, 10 = dziesięć razy szybciej. Działa od razu (przez pomocnika w grze), a po restarcie zostaje. Osobno dla Metinów i bossów, osobno dla zwykłych potworów.",
                 "en":"Percent of the normal respawn time: 100 = as in the game, 50 = twice as fast, 10 = ten times as fast. Live at once (through the in-game helper) and kept across a restart. Stones and bosses apart from ordinary monsters."},
 "regen_boss":  {"pl":"Metiny i bossowie (% czasu)", "en":"Metin stones and bosses (% of time)"},
 "regen_mob":   {"pl":"Zwykłe potwory (% czasu)", "en":"Ordinary monsters (% of time)"},
 "regen_faster": {"pl":"Szybciej:", "en":"Faster:"},
 "regen_mult":  {"pl":"≈ ×{n} szybciej niż w grze", "en":"≈ ×{n} faster than the game"},
 "regen_save":  {"pl":"Zapisz czasy odradzania", "en":"Save the respawn times"},
 "regen_range": {"pl":"Obie wartości muszą być liczbą całkowitą od 10 do 100. Nic nie zmieniono.",
                 "en":"Both have to be whole numbers between 10 and 100. Nothing was changed."},
 "regen_saved_live": {"pl":"✅ Zapisano! Nowe czasy odradzania działają już w grze, bez restartu.",
                      "en":"✅ Saved! The new respawn times are live in game, no restart needed."},
 "regen_saved_restart": {"pl":"Zapisano. Nikt nie jest zalogowany, więc pomocnik w grze nie odpowiedział — nowe czasy zadziałają po restarcie serwera (albo zapisz jeszcze raz, gdy ktoś będzie w grze).",
                         "en":"Saved. Nobody is logged in, so the in-game helper did not answer — the new times apply after a server restart (or save again while somebody is in game)."},
 "count_title": {"pl":"Liczba potworów w respie", "en":"Monsters per respawn"},
 "count_help":  {"pl":"Ile potworów stoi w każdym miejscu respu: ×1 = jak w grze, ×2 = dwa razy więcej, aż do ×4. Nie trzeba restartu, a po restarcie ustawienie zostaje; dodatkowe potwory dochodzą przy najbliższym respie danego miejsca (Metiny i bossowie po swoim czasie odradzania, zwykle 15–25 minut). Osobno dla Metinów i bossów, osobno dla zwykłych potworów. Postacie niezależne (także żyły rud i krzaki ziół), portale, lochy i jednorazowe respy z misji zostają bez zmian.",
                 "en":"How many monsters stand at each spawn point: ×1 = as in the game, ×2 = twice as many, up to ×4. No restart needed and kept across one; the extra monsters come at each spot's next respawn (stones and bosses after their own respawn time, usually 15-25 minutes). Stones and bosses apart from ordinary monsters. NPCs (ore veins and herb bushes too), portals, dungeons and a quest's one-off spawns are left alone."},
 "count_warn":  {"pl":"Uwaga: ×2 to dwa razy więcej potworów na każdej mapie — serwer i boty mają przez to więcej pracy. Po zmniejszeniu mnożnika nadmiarowe potwory znikają dopiero, gdy ktoś je zabije.",
                 "en":"Mind: ×2 is twice as many monsters on every map, and the server and the bots work that much harder. After lowering it, the extra monsters go only as they are killed."},
 "count_boss":  {"pl":"Metiny i bossowie", "en":"Metin stones and bosses"},
 "count_mob":   {"pl":"Zwykłe potwory", "en":"Ordinary monsters"},
 "count_save":  {"pl":"Zapisz liczbę potworów", "en":"Save the monster counts"},
 "count_range": {"pl":"Wybierz mnożnik od ×1 do ×4. Nic nie zmieniono.",
                 "en":"Pick a multiplier from ×1 to ×4. Nothing was changed."},
 "count_saved_live": {"pl":"✅ Zapisano! Nowa liczba potworów działa już w grze — dosypie się przy najbliższym respie.",
                      "en":"✅ Saved! The new counts are live in game and fill in at the next respawn."},
 "count_saved_restart": {"pl":"Zapisano. Nikt nie jest zalogowany, więc pomocnik w grze nie odpowiedział — nowa liczba potworów zadziała po restarcie serwera (albo zapisz jeszcze raz, gdy ktoś będzie w grze).",
                         "en":"Saved. Nobody is logged in, so the in-game helper did not answer — the new counts apply after a server restart (or save again while somebody is in game)."},
 "diff_title":  {"pl":"Poziom trudności", "en":"Difficulty"},
 "diff_help":   {"pl":"Ile gracz czeka u Biologa między oddaniami i u Stajennego (kucyk, Księgi Konia, treningi medalami), i ile gracze oraz boty czekają między dwiema księgami tej samej umiejętności i między dwoma Kamieniami Duchowymi (G1 → P, najwyżej 12 h) — Zwój Egzorcyzmu pomija to czekanie. Zmiana działa od razu, gdy ktoś jest w grze, i zostaje po restarcie, dopóki nie zmienisz poziomu trudności w launcherze (przycisk POZIOM TRUDNOŚCI).",
                 "en":"How long a player waits at the Biologist between hand-ins and at the stable keeper (the pony, the Horse Books, the medal trainings), and how long players and bots wait between two books of one skill and between two Soul Stones (G1 to P, at most 12 h) - an Exorcism Scroll skips that wait. A change is live at once while somebody is in game, and it stays across a restart until the difficulty is changed in the launcher (the DIFFICULTY button)."},
 "diff_now":    {"pl":"Teraz", "en":"Now"},
 "diff_level":  {"pl":"Poziom", "en":"Level"},
 "diff_level_easy":   {"pl":"Łatwy — bez czekania", "en":"Easy — no waiting"},
 "diff_level_medium": {"pl":"Średni — Biolog 8 h, koń 4–7 h, księgi i Kamienie Duchowe 7 h", "en":"Medium — Biologist 8 h, horse 4-7 h, books and Soul Stones 7 h"},
 "diff_level_hard":   {"pl":"Trudny — jak w oryginale: Biolog 24 h, koń 12–21 h, księgi 21 h, Kamienie Duchowe 12 h", "en":"Hard — as the original: Biologist 24 h, horse 12-21 h, books 21 h, Soul Stones 12 h"},
 "diff_level_custom": {"pl":"Własny — godziny poniżej", "en":"Custom — the hours below"},
 "diff_bio":    {"pl":"Biolog", "en":"Biologist"},
 "diff_horse":  {"pl":"Stajenny (kucyk / Księga / treningi)", "en":"stable keeper (pony / book / trainings)"},
 "diff_books":  {"pl":"księgi", "en":"books"},
 "diff_players":{"pl":"gracze", "en":"players"},
 "diff_bots":   {"pl":"boty", "en":"bots"},
 "diff_custom_help": {"pl":"Godziny poniżej liczą się tylko przy poziomie „Własny” (ułamki dozwolone, 0 = bez czekania). Stajenny ma jedną liczbę na każde czekanie: kucyka, Księgę Konia i treningi.",
                      "en":"The hours below count for the Custom level only (fractions allowed, 0 = no waiting). The stable keeper takes one number for every wait: the pony, the Horse Book and the trainings."},
 "diff_bio_h":  {"pl":"Biolog — godzin między oddaniami:", "en":"Biologist — hours between hand-ins:"},
 "diff_horse_h":{"pl":"Stajenny — godzin na kucyka, Księgę i trening:", "en":"Stable keeper — hours for the pony, the book and a training:"},
 "diff_book_h": {"pl":"Księgi umiejętności i Kamienie Duchowe — gracze, godzin:", "en":"Skill books and Soul Stones — players, hours:"},
 "diff_bot_book_h": {"pl":"Księgi umiejętności i Kamienie Duchowe — boty, godzin:", "en":"Skill books and Soul Stones — bots, hours:"},
 "diff_save":   {"pl":"Zapisz poziom trudności", "en":"Save the difficulty"},
 # MT2009_PLUS_EXCHANGE_CHANCE_V1: the NPC exchanges' chances by the level.
 "diff_exchange_now": {"pl":"Szanse wymiany u NPC (Magiczny Pył / Pergamin / Materiały Rzemieślnicze)",
                       "en":"NPC exchange chances (Magic Dust / Parchment / Crafting Materials)"},
 "diff_exchange": {"pl":"Poziom ustawia też szanse wymiany u NPC — kamieni duszy na Magiczny Pył, ksiąg na Pergamin i ulepszaczy na Materiały Rzemieślnicze: łatwy 100 / 100 / 55% (jak w paczce), średni 90 / 45 / 55%, trudny 55 / 40 / 55%. Poziom „Własny” bierze trzy liczby z .env serwera (M2_EXCHANGE_DUST_CHANCE, M2_EXCHANGE_PARCHMENT_CHANCE, M2_EXCHANGE_MATERIAL_CHANCE; 0 = jak w paczce), wczytywane przy każdym starcie. Okno wymiany pokazuje obowiązującą szansę, a boty wymieniają z tą samą.",
                   "en":"The level also sets the NPC exchange chances - soul stones to Magic Dust, skill books to Parchment and upgrade items to Crafting Materials: easy 100 / 100 / 55% (as the package), medium 90 / 45 / 55%, hard 55 / 40 / 55%. The Custom level takes three numbers from the server's .env (M2_EXCHANGE_DUST_CHANCE, M2_EXCHANGE_PARCHMENT_CHANCE, M2_EXCHANGE_MATERIAL_CHANCE; 0 = as the package), read at every start. The exchange window shows the chance in force, and the bots exchange at the same one."},
 "diff_range":  {"pl":"Wybierz poziom i podaj godziny od 0 do 720. Nic nie zmieniono.",
                 "en":"Pick a level and hours from 0 to 720. Nothing was changed."},
 "diff_saved_live": {"pl":"✅ Zapisano! Nowy poziom trudności działa już w grze.",
                     "en":"✅ Saved! The new difficulty is live in game."},
 "diff_saved_restart": {"pl":"Zapisano. Nikt nie jest zalogowany, więc pomocnik w grze nie odpowiedział — nowe czasy zadziałają po restarcie serwera (albo zapisz jeszcze raz, gdy ktoś będzie w grze).",
                        "en":"Saved. Nobody is logged in, so the in-game helper did not answer — the new waits apply after a server restart (or save again while somebody is in game)."},
 "ah_title":    {"pl":"Panel Autołowy", "en":"Auto Hunt panel"},
 "ah_help":     {"pl":"Kto może używać okna Auto Łowy w kliencie. „Tylko po kupnie” — gracz kupuje w ItemShopie „Auto Lowy (8h)” i używa go z ekwipunku: 8 godzin, które lecą tylko wtedy, gdy postać jest w grze; kolejne bilety się sumują (najwyżej 30 dni). Zmiana działa od razu, gdy ktoś jest w grze, i zostaje po restarcie, dopóki nie zmienisz jej w launcherze (przycisk POZIOM TRUDNOŚCI).",
                 "en":"Who may use the client's Auto Hunt window. \"Only after buying\": a player buys \"Auto Lowy (8h)\" in the ItemShop and uses it from the inventory - eight hours that run only while the character is in the game; further tickets add up (thirty days at most). A change is live at once while somebody is in game, and it stays across a restart until it is changed in the launcher (the DIFFICULTY button)."},
 "ah_all":      {"pl":"Panel Autołowy dostępny dla każdego", "en":"Auto Hunt panel available to everybody"},
 "ah_item":     {"pl":"Panel Autołowy dostępny tylko po kupnie przedmiotu z ItemShop", "en":"Auto Hunt panel only after buying the ItemShop item"},
 "ah_off":      {"pl":"Autołowy są na tym świecie wyłączone (launcher: M2_AUTOHUNT=0), więc ten wybór zadziała dopiero, gdy je włączysz.",
                 "en":"Auto Hunt is switched off on this world (launcher: M2_AUTOHUNT=0), so this choice applies once it is switched on."},
 "ah_save":     {"pl":"Zapisz dostęp do Autołowów", "en":"Save the Auto Hunt access"},
 "ah_saved_live": {"pl":"✅ Zapisano! Nowy dostęp do Autołowów działa już w grze.",
                   "en":"✅ Saved! The new Auto Hunt access is live in game."},
 "ah_saved_restart": {"pl":"Zapisano. Nikt nie jest zalogowany, więc pomocnik w grze nie odpowiedział — zmiana zadziała po restarcie serwera (albo zapisz jeszcze raz, gdy ktoś będzie w grze).",
                      "en":"Saved. Nobody is logged in, so the in-game helper did not answer — the change applies after a server restart (or save again while somebody is in game)."},
 "sc_title":    {"pl":"Skrzynia Ucznia", "en":"Apprentice Chest"},
 "sc_help":     {"pl":"Łańcuch Skrzyń Ucznia (od Skrzyni Ucznia I po Skrzynię Arcymistrza: mikstury, eliksiry, zielone kamienie i następna skrzynia) — jeden przełącznik dla graczy i botów. Włączona: nowa postać gracza dostaje ją przy pierwszym logowaniu (do 5. poziomu), bot ma swoją od początku i otwiera kolejne skrzynie na ich poziomach. Wyłączona: nie dostaje jej nikt; boty tracą nieotwarte skrzynie z łańcucha, a nowe boty rodzą się bez niej. Skrzyń graczy nic nie rusza. Zmiana działa od razu i zostaje po restarcie, dopóki nie zmienisz jej w launcherze (przycisk POZIOM TRUDNOŚCI, M2_STARTER_CHEST w .env).",
                 "en":"The Apprentice Chest chain (from Apprentice Chest I to the Grand Master Chest: potions, elixirs, green stones and the next chest) - one switch for players and bots. On: a player's new character gets it at its first login (up to level 5), a bot has its own from the start and opens the next chests at their levels. Off: nobody gets it; the bots lose the unopened chests of the chain and new bots are made without one. Players' chests are never touched. A change is live at once and stays across a restart until it is changed in the launcher (the DIFFICULTY button, M2_STARTER_CHEST in .env)."},
 "sc_on":       {"pl":"Skrzynia Ucznia w grze (gracze i boty)", "en":"Apprentice Chest in the game (players and bots)"},
 "sc_off":      {"pl":"Bez Skrzyni Ucznia (ani dla graczy, ani dla botów)", "en":"No Apprentice Chest (for neither players nor bots)"},
 "sc_save":     {"pl":"Zapisz Skrzynię Ucznia", "en":"Save the Apprentice Chest"},
 "sc_saved_live": {"pl":"✅ Zapisano! Zmiana działa już w grze.",
                   "en":"✅ Saved! The change is live in game."},
 "sc_saved_restart": {"pl":"Zapisano. Gra nie odpowiedziała (serwer jest wyłączony albo dopiero startuje) — zmiana zadziała przy następnym starcie serwera.",
                      "en":"Saved. The game did not answer (the server is down or still starting) - the change applies at the next server start."},
 "mh_title":    {"pl":"Wytrzymałość potworów", "en":"Monster health"},
 "mh_help":     {"pl":"Ile życia mają potwory, bossowie i Metiny: 100% = jak w grze, 80% = łatwiej (każdy pada po mniejszej liczbie ciosów; doświadczenie i drop z jednego zabicia bez zmian, więc gra idzie szybciej — także botom). Postacie niezależne, żyły rud, krzaki ziół i bramy w lochach zostają bez zmian. Zmiana działa od razu — także na potwory, które już stoją (zachowują ten sam procent życia) — i zostaje po restarcie, dopóki nie zmienisz jej w launcherze (przycisk POZIOM TRUDNOŚCI, M2_MONSTER_HP w .env).",
                 "en":"How much health monsters, bosses and Metin stones have: 100% = as in the game, 80% = easier (each one falls to fewer blows; the experience and the drop of one kill stay the same, so the game goes faster - for the bots too). NPCs, ore veins, herb bushes and dungeon gates stay as they are. A change is live at once - on the monsters already standing too, which keep the same share of their health - and stays across a restart until it is changed in the launcher (the DIFFICULTY button, M2_MONSTER_HP in .env)."},
 "mh_100":      {"pl":"100% — jak w grze (domyślnie)", "en":"100% — as in the game (default)"},
 "mh_80":       {"pl":"80% — łatwiej", "en":"80% — easier"},
 "mh_custom":   {"pl":"{n}% — własne (z .env)", "en":"{n}% — your own (from .env)"},
 "mh_save":     {"pl":"Zapisz wytrzymałość potworów", "en":"Save the monster health"},
 "mh_range":    {"pl":"Wybierz wytrzymałość od 10 do 300%. Nic nie zmieniono.",
                 "en":"Pick a health from 10 to 300%. Nothing was changed."},
 "mh_saved_live": {"pl":"✅ Zapisano! Nowa wytrzymałość potworów działa już w grze.",
                   "en":"✅ Saved! The new monster health is live in game."},
 "mh_saved_restart": {"pl":"Zapisano. Gra nie odpowiedziała (serwer jest wyłączony albo dopiero startuje) — zmiana zadziała przy następnym starcie serwera.",
                      "en":"Saved. The game did not answer (the server is down or still starting) - the change applies at the next server start."},
 "ai_books_moved": {"pl":"Na tym serwerze ustawia to poziom trudności (Mnożniki serwera → Poziom trudności): osobno czas dla graczy, osobno dla botów; 0 = od razu.",
                    "en":"On this server the difficulty sets it (Server rates → Difficulty): one wait for the players, one for the bots; 0 = at once."},
 "ch2_title":   {"pl":"Drugi kanał (CH2)", "en":"Second channel (CH2)"},
 "ch2_help":    {"pl":"Drugi kanał gry. Część botów gra na CH2, więc serwer rozkłada je na dwa rdzenie procesora i udźwignie więcej botów naraz. Wszystkie sklepy (botów i graczy) stoją tylko na CH1: bot z CH2, który chce otworzyć albo obsłużyć swój sklep albo coś kupić, przechodzi na CH1, a wolny bot z CH1 przechodzi na jego miejsce na CH2. Ustawiony udział to najmniej tyle botów na CH2; gdy nikt nie czeka na przejście, CH2 może przejąć do 10 punktów więcej. Domyślnie wyłączony.",
                 "en":"A second game channel. Some of the bots play on CH2, so the server spreads them over two CPU cores and carries more bots at once. Every shop, bots' and players', stands on CH1: a bot on CH2 that wants to open or serve its shop or to buy something moves to CH1, and a free bot of CH1 moves to CH2 in its place. The share set here is the least CH2 carries; with nobody waiting to move, CH2 may take up to ten points more. Off by default."},
 "ch2_enable":  {"pl":"Włącz drugi kanał (CH2)", "en":"Switch the second channel on (CH2)"},
 "ch2_share":   {"pl":"Botów na CH2", "en":"Bots on CH2"},
 "ch2_save":    {"pl":"Zapisz (zadziała po restarcie serwera)", "en":"Save (applies after a server restart)"},
 "ch2_now_off": {"pl":"Teraz: CH2 wyłączony - wszystkie boty grają na CH1.", "en":"Now: CH2 is off - every bot plays on CH1."},
 "ch2_now_on":  {"pl":"Teraz: CH2 włączony, na CH2 gra {share}% botów.", "en":"Now: CH2 is on, {share}% of the bots play on it."},
 "ch2_ports":   {"pl":"Gracze wejdą na CH2 po najbliższym uruchomieniu serwera z launchera (GRAJ) - launcher otworzy wtedy porty {ports}. Boty grają na CH2 już teraz.",
                 "en":"Players reach CH2 after the next start from the launcher (GRAJ), which opens ports {ports} then. The bots play on CH2 already."},
 "ch2_pending": {"pl":"Zapisano w panelu: {what}. Zadziała po restarcie serwera.", "en":"Saved in the panel: {what}. Applies after a server restart."},
 "ch2_on_word": {"pl":"CH2 włączony, {share}% botów", "en":"CH2 on, {share}% of the bots"},
 "ch2_off_word": {"pl":"CH2 wyłączony", "en":"CH2 off"},
 "ch2_saved":   {"pl":"✅ Zapisano. Zmiana kanałów zadziała po restarcie serwera (GRAJ w launcherze albo restart kontenera gry).",
                 "en":"✅ Saved. The channel change applies after a server restart (GRAJ in the launcher or a restart of the game container)."},
 "ch2_bad":     {"pl":"Udział botów na CH2 musi być liczbą od 10 do 90. Nic nie zmieniono.", "en":"The share of bots on CH2 has to be between 10 and 90. Nothing was changed."},
 "ch2_failed":  {"pl":"Nie udało się zapisać ustawienia kanałów w katalogu wymiany z serwerem.", "en":"Could not write the channel setting into the spool shared with the server."},
 "rates_yang_range": {"pl":"Yang może być najwyżej 1000%. Nic nie zmieniono. 🙂","en":"Yang can be at most 1000%. Nothing was changed. 🙂",
                  "de":"Yang darf höchstens 1000% sein. Es wurde nichts geändert. 🙂",
                  "tr":"Yang en fazla %1000 olabilir. Hiçbir şey değiştirilmedi. 🙂"},
 "rates_range":  {"pl":"Każda z trzech wartości musi być liczbą całkowitą od 1 do 10000. Nic nie zmieniono. 🙂","en":"Each of the three has to be a whole number between 1 and 10000. Nothing was changed. 🙂",
                  "de":"Alle drei müssen ganze Zahlen zwischen 1 und 10000 sein. Es wurde nichts geändert. 🙂",
                  "tr":"Üçü de 1 ile 10000 arasında tam sayı olmalı. Hiçbir şey değiştirilmedi. 🙂"},
 "rates_saved_live":{"pl":"✅ Zapisano! Nowe mnożniki działają już w grze, bez restartu.",
                  "en":"✅ Saved! The new rates are live in game, no restart needed.",
                  "de":"✅ Gespeichert! Die neuen Raten gelten sofort im Spiel, ohne Neustart.",
                  "tr":"✅ Kaydedildi! Yeni oranlar oyunda hemen geçerli, yeniden başlatma gerekmez."},
 "rates_intro_mt2009":{"pl":"Te trzy liczby decydują, jak szybko toczy się cały serwer. 100% to dokładnie tak, jak gra została stworzona — wyżej znaczy szybciej. Gdy ktoś jest zalogowany w grze, zapis działa od razu; w przeciwnym razie serwer gry restartuje się sam i grający zostają na chwilę rozłączeni.",
                  "en":"These three numbers decide how fast the whole server runs. 100% is exactly as the game was made — higher means faster. While somebody is logged in, saving applies at once; otherwise the game server restarts itself and players are briefly disconnected.",
                  "de":"Diese drei Zahlen bestimmen, wie schnell der ganze Server läuft. 100% ist genau so, wie das Spiel gemacht wurde — höher heißt schneller. Ist jemand eingeloggt, gilt das Speichern sofort; sonst startet der Spielserver von selbst neu und die Spieler werden kurz getrennt.",
                  "tr":"Bu üç sayı tüm sunucunun ne kadar hızlı ilerlediğini belirler. %100, oyunun yapıldığı haliyle aynıdır — daha yüksek, daha hızlı demektir. Biri oyundaysa kayıt hemen geçerli olur; aksi halde oyun sunucusu kendini yeniden başlatır ve oyuncular kısa süre bağlantıyı kaybeder."},
 "gm_granted_restart":{"pl":"{name} ma teraz rangę {rank}. Serwer wczyta nowe uprawnienia przy najbliższym restarcie — albo od razu, gdy zalogowany GM wpisze w grze /reload a.",
                  "en":"{name} is now {rank}. The server reads the new rights at its next restart — or right away when a logged-in GM types /reload a in game.",
                  "de":"{name} ist jetzt {rank}. Der Server liest die neuen Rechte beim nächsten Neustart — oder sofort, wenn ein eingeloggter GM im Spiel /reload a eingibt.",
                  "tr":"{name} artık {rank}. Sunucu yeni yetkileri bir sonraki yeniden başlatmada okur — ya da oyundaki bir GM /reload a yazdığında hemen."},
 "gm_removed_restart":{"pl":"{name} jest znów zwykłym graczem. Komendy zachowuje do najbliższego restartu serwera (albo do /reload a wpisanego przez zalogowanego GM) i ponownego zalogowania.",
                  "en":"{name} is a normal player again. The commands stay until the next server restart (or /reload a typed by a logged-in GM) and a relogin.",
                  "de":"{name} ist wieder normaler Spieler. Die Befehle bleiben bis zum nächsten Server-Neustart (oder /reload a eines eingeloggten GM) und einem erneuten Einloggen.",
                  "tr":"{name} yeniden normal oyuncu. Komutlar bir sonraki sunucu yeniden başlatmasına (ya da oyundaki bir GM /reload a yazana) ve tekrar girişe kadar kalır."},
 "rates_saved":  {"pl":"✅ Zapisano! Serwer gry właśnie się restartuje i powinien wrócić w niecałą minutę. Odśwież tę stronę za chwilę, aby zobaczyć wynik.","en":"✅ Saved! The game server is restarting now and should be back in under a minute. Give this page a reload in a moment to see how it went.",
                  "de":"✅ Gespeichert! Der Spielserver startet gerade neu und sollte in weniger als einer Minute wieder da sein. Lade diese Seite gleich neu, um das Ergebnis zu sehen.",
                  "tr":"✅ Kaydedildi! Oyun sunucusu şimdi yeniden başlıyor, bir dakikadan kısa sürede geri gelmeli. Sonucu görmek için birazdan bu sayfayı yenile."},
 "rates_no_script":{"pl":"Ten serwer został postawiony, zanim istniała funkcja mnożników, więc brakuje małego pomocnika, który je stosuje. Uruchom instalator ponownie na serwerze — doda pomocnika i nie zmieni nic innego. 🙂","en":"This server was set up before the rates feature existed, so the little helper that applies them is missing. Run the installer again on the server — it adds the helper and changes nothing else. 🙂",
                  "de":"Dieser Server wurde eingerichtet, bevor es die Raten gab, deshalb fehlt das kleine Hilfsprogramm dafür. Führe das Installationsprogramm auf dem Server noch einmal aus — es ergänzt nur dieses Hilfsprogramm. 🙂",
                  "tr":"Bu sunucu oran özelliği eklenmeden önce kurulmuş, bu yüzden oranları uygulayan küçük yardımcı yok. Sunucuda kurulumu tekrar çalıştır — sadece bu yardımcıyı ekler, başka bir şeye dokunmaz. 🙂"},
 "rates_no_table":{"pl":"Nie udało się zapisać mnożników, bo tabela, w której są przechowywane, jeszcze nie istnieje. Ponowne uruchomienie instalatora na serwerze ją tworzy. 🙏","en":"The rates could not be saved, because the table they live in is not there yet. Running the installer again on the server creates it. 🙏",
                  "de":"Die Raten konnten nicht gespeichert werden, weil die zugehörige Tabelle noch fehlt. Ein erneuter Lauf des Installationsprogramms legt sie an. 🙏",
                  "tr":"Oranlar kaydedilemedi, çünkü bulundukları tablo henüz yok. Sunucuda kurulumu tekrar çalıştırmak bu tabloyu oluşturur. 🙏"},
 "rates_st":     {"pl":"Jak poszła ostatnia zmiana","en":"How the last change went","de":"Wie die letzte Änderung lief","tr":"Son değişiklik nasıl gitti"},
 "rates_st_running":{"pl":"⏳ Mnożniki są właśnie stosowane, a serwer się restartuje. Zwykle trwa to znacznie mniej niż minutę.","en":"⏳ The rates are being applied and the server is restarting. This normally takes well under a minute.",
                  "de":"⏳ Die Raten werden angewendet und der Server startet neu. Das dauert normalerweise deutlich unter einer Minute.",
                  "tr":"⏳ Oranlar uygulanıyor ve sunucu yeniden başlıyor. Bu genelde bir dakikadan çok kısa sürer."},
 "rates_st_ok":  {"pl":"✅ Te mnożniki działają na serwerze.","en":"✅ These rates are live on the server.",
                  "de":"✅ Diese Raten sind auf dem Server aktiv.",
                  "tr":"✅ Bu oranlar sunucuda geçerli."},
 "rates_st_unsupported":{"pl":"⚠️ Te pliki serwera nie pozwalają zmieniać mnożników, więc nic nie zastosowano — to, co tu wpiszesz, nie ma wpływu na grę. To nie Twój błąd; ten zestaw plików serwera po prostu tego nie obsługuje.","en":"⚠️ These server files cannot have their rates changed, so nothing was applied — whatever you type here will have no effect in the game. This is not something you did wrong; the set of server files simply does not support it.",
                  "de":"⚠️ Bei diesen Serverdateien lassen sich die Raten nicht ändern, es wurde also nichts angewendet — was du hier einträgst, wirkt sich im Spiel nicht aus. Du hast nichts falsch gemacht, diese Serverdateien können es einfach nicht.",
                  "tr":"⚠️ Bu sunucu dosyalarının oranları değiştirilemiyor, bu yüzden hiçbir şey uygulanmadı — buraya ne yazarsan yaz oyunda bir etkisi olmaz. Senin hatan değil; bu sunucu dosyaları bunu desteklemiyor."},
 "rates_st_failed":{"pl":"⚠️ Coś poszło nie tak przy stosowaniu mnożników, więc mogą nie wszystkie działać. Szczegóły są w /var/log/m2rates.log na serwerze.","en":"⚠️ Something went wrong while applying the rates, so they may not all be live. The details are in /var/log/m2rates.log on the server.",
                  "de":"⚠️ Beim Anwenden der Raten ist etwas schiefgelaufen, vielleicht sind nicht alle aktiv. Die Einzelheiten stehen auf dem Server in /var/log/m2rates.log.",
                  "tr":"⚠️ Oranlar uygulanırken bir şeyler ters gitti, hepsi geçerli olmayabilir. Ayrıntılar sunucudaki /var/log/m2rates.log dosyasında."},
 "rates_st_no_restart":{"pl":"ℹ️ Nowe mnożniki są zapisane, ale serwer gry nie mógł zrestartować się sam. Zrestartuj go ręcznie, a zadziałają od razu.","en":"ℹ️ The new rates are saved, but the game server could not be restarted on its own. Restart it yourself and they take effect right away.",
                  "de":"ℹ️ Die neuen Raten sind gespeichert, aber der Spielserver konnte nicht selbst neu gestartet werden. Starte ihn von Hand neu, dann gelten sie sofort.",
                  "tr":"ℹ️ Yeni oranlar kaydedildi ama oyun sunucusu kendi kendine yeniden başlatılamadı. Sen yeniden başlatınca hemen geçerli olurlar."},

 # --- what this project is (shown to everyone, no login needed) ---
 "about_title":  {"pl":"Co to jest?","en":"What is this?","de":"Was ist das hier?","tr":"Bu nedir?"},
 "about_goal":   {"pl":"To Metin2 takie, jakie było w 2014 — oryginalna gra, bez zmian — na serwerze, który ktoś postawił dla siebie. Bez sklepu z przedmiotami, bez niczego do kupienia i bez grindu, który istnieje tylko po to, by coś Ci sprzedać. Grasz we własnym tempie.","en":"This is Metin2 as it was in 2014 — the original game, unchanged — running on a server somebody set up for themselves. No item shop, nothing to buy, and none of the grind that only exists to sell you something. You play at your own pace.",
                  "de":"Das hier ist Metin2, wie es 2014 war — das Originalspiel, unverändert — auf einem Server, den sich jemand selbst eingerichtet hat. Kein Item-Shop, nichts zu kaufen, und nichts von dem Grind, den es nur gibt, um dir etwas zu verkaufen. Du spielst in deinem eigenen Tempo.",
                  "tr":"Burada 2014'teki hâliyle Metin2 var — orijinal oyun, değiştirilmemiş — birinin kendisi için kurduğu bir sunucuda çalışıyor. Item shop yok, satın alınacak bir şey yok ve sadece sana bir şey satmak için var olan o grind yok. Kendi hızında oynarsın."},
 "about_hobby":  {"pl":"To projekt hobbystyczny. Nikt na nim nie zarabia, nie ma nic do kupienia i nigdy nie będzie.","en":"This is a hobby project. Nobody earns anything from it, there is nothing to buy, and there never will be.",
                  "de":"Das hier ist ein Hobbyprojekt. Niemand verdient daran etwas, es gibt nichts zu kaufen, und das wird auch so bleiben.",
                  "tr":"Burası bir hobi projesi. Kimse bundan para kazanmıyor, satın alınacak bir şey yok ve olmayacak da."},
 # Only shown when this server really has a browser client -- a server that
 # offers the download alone must not promise a link that is not there.
 "about_web":    {"pl":"Możesz grać prosto w przeglądarce — jedno kliknięcie, nic do pobrania i nic do instalowania. Jeśli wolisz pełnego klienta, pobierz go: ten sam serwer, to samo konto, a postać założona w jednym jest w drugim.","en":"You can play straight in your browser — one click, nothing to download and nothing to install. If you would rather have the proper client, download it instead: same server, same account, and a character made in one is there in the other.",
                  "de":"Spielen kannst du direkt im Browser — ein Klick, kein Download, keine Installation. Wer lieber den richtigen Client möchte, lädt ihn herunter: derselbe Server, dasselbe Konto, und ein Charakter aus dem einen ist auch im anderen da.",
                  "tr":"Doğrudan tarayıcında oynayabilirsin — tek tık, indirme yok, kurulum yok. Gerçek istemciyi tercih edersen onu indir: aynı sunucu, aynı hesap; birinde yaptığın karakter diğerinde de vardır."},
 "about_uptime": {"pl":"Postacie żyją w bazie tego serwera i nigdzie indziej — nikt nie ma drugiej kopii i nic nie jest nigdzie wysyłane. To znaczy też, że to, jak długo istnieje ten świat, zależy wyłącznie od tego, kto go prowadzi.","en":"Characters live in this server's own database and nowhere else — nobody has a second copy, and nothing is sent anywhere. Which also means how long this world lasts is entirely up to whoever runs it.",
                  "de":"Charaktere liegen in der Datenbank dieses Servers und sonst nirgends — niemand hat eine zweite Kopie, und es wird nichts irgendwohin übertragen. Das heißt aber auch: Wie lange es diese Welt gibt, entscheidet allein, wer den Server betreibt.",
                  "tr":"Karakterler yalnızca bu sunucunun kendi veritabanında durur — kimsede ikinci bir kopya yok ve hiçbir yere bir şey gönderilmiyor. Bu aynı zamanda şu demek: bu dünyanın ne kadar süreceğine yalnızca sunucuyu işleten kişi karar verir."},
 "about_oss":    {"pl":"Wszystko, czego trzeba, by to uruchomić, jest otwarte. Każdy może postawić dokładnie taki sam serwer na własnym komputerze jedną komendą — bez znajomości Metin2.","en":"Everything needed to run this is open source. Anyone can set up exactly the same server on their own machine with a single command — no Metin2 knowledge required.",
                  "de":"Alles, was dafür nötig ist, ist Open Source. Jeder kann sich mit einem einzigen Befehl genau denselben Server selbst aufsetzen — ganz ohne Metin2-Vorwissen.",
                  "tr":"Bunu çalıştırmak için gereken her şey açık kaynak. İsteyen herkes tek bir komutla aynı sunucuyu kendi makinesinde kurabilir — Metin2 bilgisi gerekmez."},
 "about_contact":{"pl":"Jeśli uważasz, że serwer wymaga zmian — mnożniki, szybkość poruszania, cokolwiek innego — napisz do","en":"If you feel the server needs adjusting — the rates, movement speed, or anything else — write to",
                  "de":"Wenn du findest, dass am Server etwas angepasst werden sollte — die Raten, die Laufgeschwindigkeit oder irgendetwas anderes — schreib an",
                  "tr":"Sunucuda bir şeyin ayarlanması gerektiğini düşünüyorsan — oranlar, hareket hızı ya da başka herhangi bir şey — şu adrese yaz:"},
 # --- local install: the game is on this machine, there is nothing to fetch ---
 # --- first-run prompt: no account exists on this server yet ---
 "ob_title":     {"pl":"Prawie gotowe do gry","en":"Almost ready to play","de":"Fast startklar","tr":"Oynamaya neredeyse hazır"},
 "ob_none":      {"pl":"Na tym serwerze nie ma jeszcze żadnego konta — Twoje byłoby pierwsze. Potrzeba nazwy użytkownika i hasła, nic więcej.","en":"There is no account on this server yet — yours would be the first. It takes a username and a password, nothing else.",
                  "de":"Auf diesem Server gibt es noch kein Konto — deines wäre das erste. Es braucht einen Benutzernamen und ein Passwort, sonst nichts.",
                  "tr":"Bu sunucuda henüz hiç hesap yok — seninki ilki olur. Bir kullanıcı adı ve bir şifre yeter, başka bir şey değil."},
 "ob_s1":        {"pl":"Załóż konto","en":"Create an account","de":"Konto anlegen","tr":"Hesap aç"},
 "ob_s2":        {"pl":"Pobierz grę","en":"Get the game","de":"Spiel holen","tr":"Oyunu al"},
 "ob_s3":        {"pl":"Graj","en":"Play","de":"Spielen","tr":"Oyna"},
 "ob_go":        {"pl":"Załóż pierwsze konto","en":"Create the first account","de":"Erstes Konto anlegen","tr":"İlk hesabı oluştur"},
 "admin_hint_local":{"pl":"Ten serwer słucha tylko tego komputera, więc nie trzeba wpisywać hasła.","en":"This server only listens to this computer, so there is no passphrase to type.",
                  "de":"Dieser Server lauscht nur auf diesem Computer, es gibt also keine Passphrase einzugeben.",
                  "tr":"Bu sunucu yalnızca bu bilgisayarı dinliyor, bu yüzden girilecek bir parola yok."},
 "admin_open":   {"pl":"🛠️ Zarządzaj serwerem","en":"🛠️ Manage the server","de":"🛠️ Server verwalten","tr":"🛠️ Sunucuyu yönet"},
 "back_front":   {"pl":"← Strona główna","en":"← Front page","de":"← Startseite","tr":"← Ana sayfa"},
 "dl_local_t":   {"pl":"Gra jest na tym komputerze","en":"The game is on this computer","de":"Das Spiel liegt auf diesem Computer","tr":"Oyun bu bilgisayarda"},
 "dl_local":     {"pl":"Nie ma czego pobierać: ten serwer działa na komputerze, przy którym siedzisz, więc gra została rozpakowana tutaj. Poszukaj <b>Metin2 Singleplayer</b> na pulpicie i stamtąd ją uruchom.","en":"Nothing to download: this server runs on the machine you are sitting at, so the game was unpacked here directly. Look for <b>Metin2 Singleplayer</b> on your Desktop and start it from there.",
                  "de":"Nichts herunterzuladen: Dieser Server läuft auf dem Rechner, an dem du sitzt, das Spiel wurde also gleich hier ausgepackt. Auf dem Desktop findest du <b>Metin2 Singleplayer</b> — von dort startest du es.",
                  "tr":"İndirilecek bir şey yok: bu sunucu şu an başında oturduğun makinede çalışıyor, oyun da doğrudan buraya açıldı. Masaüstünde <b>Metin2 Singleplayer</b> kısayolunu bul ve oradan başlat."},
 "dl_local_w":   {"pl":"Wciąż się rozpakowuje. To ponad gigabajt, więc daj temu kilka minut — skrót pojawi się na pulpicie, gdy skończy.","en":"Still unpacking. It is well over a gigabyte, so give it a few minutes — the shortcut appears on the Desktop when it is done.",
                  "de":"Wird noch ausgepackt. Es ist deutlich über ein Gigabyte, gib ihm also ein paar Minuten — die Verknüpfung erscheint auf dem Desktop, sobald es fertig ist.",
                  "tr":"Hâlâ açılıyor. Bir gigabayttan epey büyük, birkaç dakika ver — bitince kısayol masaüstünde belirir."},
 # --- the operator's orientation, shown once they are logged in ---
 # This is the first thing the person who installed the server sees. It exists
 # because the dashboard used to open straight onto three cards with no
 # explanation of what had just been built or what to do with it.
 "op_title":     {"pl":"Twój serwer działa","en":"Your server is running","de":"Dein Server läuft","tr":"Sunucun çalışıyor"},
 "op_intro":     {"pl":"Wszystko jest uruchomione: gra, baza danych i ten panel. Plakietka u góry mówi, czy sama gra przyjmuje połączenia — jeśli kiedyś pokaże offline, choć jesteś pewien, że nie powinna, to pierwsze miejsce, w które warto zajrzeć.","en":"Everything is up: the game, the database and this panel. The badge at the top says whether the game itself is accepting connections — if it ever reads offline while you are sure it should not, that is the first place to look.",
                  "de":"Alles läuft: das Spiel, die Datenbank und dieses Panel. Die Anzeige oben sagt dir, ob das Spiel selbst Verbindungen annimmt — falls dort einmal „offline“ steht, obwohl du sicher bist, dass es nicht so sein sollte, schau zuerst dort nach.",
                  "tr":"Her şey ayakta: oyun, veritabanı ve bu panel. Üstteki rozet oyunun bağlantı kabul edip etmediğini gösterir — bir gün „çevrimdışı“ yazıyorsa ve bundan emin değilsen, önce oraya bak."},
 "op_share":     {"pl":"Aby ktoś mógł zagrać, daj mu adres tej strony. Zakłada tu konto i pobiera grę z tej samej strony — ona już wskazuje na Twój serwer, więc nie musi niczego konfigurować.","en":"To let someone play, give them the address of this page. They register an account here and download the game from the same page — it already points at your server, so there is nothing for them to configure.",
                  "de":"Damit jemand spielen kann, gib ihm die Adresse dieser Seite. Er registriert sich hier und lädt das Spiel von derselben Seite — es zeigt bereits auf deinen Server, es gibt für ihn nichts einzustellen.",
                  "tr":"Birinin oynaması için ona bu sayfanın adresini ver. Hesabını burada açar ve oyunu aynı sayfadan indirir — oyun zaten senin sunucunu gösteriyor, ayarlaması gereken bir şey yok."},
 # Shown instead of op_share when the installer reported a loopback-only setup.
 "op_local_t":   {"pl":"Ten serwer jest tylko dla Ciebie","en":"This server is for you alone","de":"Dieser Server ist nur für dich","tr":"Bu sunucu yalnızca sana ait"},
 "op_local":     {"pl":"Zainstalowano go jako serwer lokalny, więc wszystko słucha tylko na tym komputerze. Nikt inny nie dołączy — ani przez internet, ani z innego urządzenia w tej samej sieci. Żaden port nie został otwarty i nie powstała żadna reguła zapory. Załóż tu konto, pobierz grę z tej strony i graj.","en":"This was installed as a local server, so everything listens on this computer only. Nobody else can join — not over the internet, and not from another device on the same network. No port was opened and no firewall rule was created. Register an account here, download the game from this page, and play.",
                  "de":"Das hier wurde als lokaler Server installiert, es lauscht also alles ausschließlich auf diesem Computer. Niemand sonst kann mitspielen — weder über das Internet noch von einem anderen Gerät im selben Netzwerk. Es wurde kein Port geöffnet und keine Firewallregel angelegt. Registriere hier ein Konto, lade das Spiel von dieser Seite und spiel los.",
                  "tr":"Bu, yerel bir sunucu olarak kuruldu; yani her şey yalnızca bu bilgisayarda dinliyor. Başka kimse katılamaz — ne internet üzerinden ne de aynı ağdaki başka bir cihazdan. Hiçbir port açılmadı ve hiçbir güvenlik duvarı kuralı oluşturulmadı. Buradan bir hesap aç, oyunu bu sayfadan indir ve oyna."},
 "op_local_hint":{"pl":"Jeśli później zechcesz grać ze znajomymi, nie otwieraj portów na domowym routerze: to oddaje Twój domowy adres każdemu graczowi, Twój upload staje się wąskim gardłem, a serwer znika, gdy tylko ten komputer jest wyłączony. Wynajmij mały serwer Linux i uruchom tam instalator — dokumentacja projektu ma na to jedną komendę.","en":"If you later want friends to play, do not open ports on your home router: that hands your home address to every player, your upload speed becomes the bottleneck, and the server is gone whenever this computer is. Rent a small Linux server instead and run the installer there — the project's documentation has the one command for it.",
                  "de":"Wenn später Freunde mitspielen sollen, öffne keine Ports an deinem Heimrouter: Das gibt deine Heimadresse an jeden Spieler weiter, dein Upload wird zum Flaschenhals, und der Server ist weg, sobald dieser Rechner aus ist. Miete stattdessen einen kleinen Linux-Server und führe den Installer dort aus — der eine Befehl dafür steht in der Dokumentation des Projekts.",
                  "tr":"İleride arkadaşlarının da oynamasını istersen, ev yönlendiricinde port açma: bu, ev adresini her oyuncuya verir, yükleme hızın darboğaz olur ve bu bilgisayar kapandığında sunucu da gider. Bunun yerine küçük bir Linux sunucu kirala ve kurulumu orada çalıştır — bunun tek komutu projenin belgelerinde."},
 "op_rates":     {"pl":"Mnożniki decydują, jak szybko toczy się cały serwer: doświadczenie, drop przedmiotów i yang. 100% to gra dokładnie taka, jaką wydano. Zapis restartuje grę na mniej niż minutę, więc gracze zostają na chwilę rozłączeni.","en":"Rates decide how fast the whole server plays: experience, item drops and yang. 100% is the game exactly as it shipped. Saving restarts the game for well under a minute, so players are briefly disconnected.",
                  "de":"Die Raten bestimmen, wie schnell sich der ganze Server spielt: Erfahrung, Item-Drops und Yang. 100 % ist das Spiel genau so, wie es ausgeliefert wurde. Beim Speichern startet das Spiel für deutlich unter einer Minute neu, Spieler fliegen also kurz raus.",
                  "tr":"Oranlar tüm sunucunun ne kadar hızlı oynandığını belirler: tecrübe, eşya düşüşü ve yang. %100, oyunun çıktığı hâlidir. Kaydettiğinde oyun bir dakikadan çok kısa süre yeniden başlar, oyuncular kısa süre düşer."},
 "op_players":   {"pl":"W zakładce Gracze znajdziesz każdą postać z kontem, do którego należy. Otwórz ją, aby dać przedmioty lub yang, albo ustawić poziom. To trafia prosto do bazy, więc gracz musi się wylogować i zalogować ponownie, zanim to zobaczy.","en":"Under Players you find every character with the account it belongs to. Open one to give items or yang, or to set a level. Those go straight into the database, so the player has to log out and back in before they see them.",
                  "de":"Unter „Spieler“ findest du jeden Charakter mit dem Konto, zu dem er gehört. Öffne einen, um Gegenstände oder Yang zu geben oder ein Level zu setzen. Das geht direkt in die Datenbank — der Spieler muss sich also aus- und wieder einloggen, bevor er es sieht.",
                  "tr":"„Oyuncular“ altında her karakteri, ait olduğu hesapla birlikte görürsün. Eşya ya da yang vermek veya seviye ayarlamak için birini aç. Bunlar doğrudan veritabanına yazılır, yani oyuncunun görmesi için çıkıp yeniden girmesi gerekir."},
 "op_limits":    {"pl":"Teleport i Szybkość biegu działają inaczej: działają na postaci, która jest w tej chwili online, przez skrypt pomocnika w grze. Gracz musi więc być naprawdę zalogowany — dla każdego, kto nie jest, oba przyciski mówią to wprost, zamiast udawać, że zadziałały.","en":"Teleport and Running speed are different: they act on a character who is online right now, through a helper script inside the game. So they need the player to actually be logged in — for anybody who is not, the two buttons say so instead of pretending it worked.",
                  "de":"Teleportieren und Laufgeschwindigkeit sind anders: Sie wirken auf einen gerade eingeloggten Charakter, über ein Hilfsskript im Spiel. Sie brauchen also einen Spieler, der wirklich online ist — bei allen anderen sagen die beiden Schaltflächen das, statt Erfolg vorzutäuschen.",
                  "tr":"Işınla ve Koşma hızı farklıdır: o anda çevrimiçi olan bir karaktere, oyunun içindeki bir yardımcı betik üzerinden etki ederler. Yani oyuncunun gerçekten oyunda olması gerekir — olmayanlar için iki düğme de başarılı gibi davranmak yerine bunu söyler."},
 "op_forgot":    {"pl":"Gdy gracz zapomni hasła, robisz mu poniżej link do resetu. Działa raz i wygasa po dniu — nigdy nie widzisz ani nie ustawiasz jego hasła.","en":"When a player forgets their password, you make them a reset link below. It works once and expires after a day — you never see or set their password yourself.",
                  "de":"Wenn ein Spieler sein Passwort vergisst, erzeugst du ihm unten einen Reset-Link. Er funktioniert einmal und verfällt nach einem Tag — du siehst oder setzt sein Passwort nie selbst.",
                  "tr":"Bir oyuncu şifresini unutursa, aşağıda ona bir sıfırlama bağlantısı oluşturursun. Bir kez çalışır ve bir gün sonra geçersiz olur — şifresini asla sen görmez ya da belirlemezsin."},
 "op_more":      {"pl":"Wszystko inne — kopie zapasowe, przenoszenie serwera, przebudowa klienta pod nowy adres — jest w dokumentacji projektu.","en":"Everything else — backups, moving the server, rebuilding the client for a new address — is in the project's documentation.",
                  "de":"Alles Weitere — Sicherungen, Serverumzug, den Client für eine neue Adresse neu bauen — steht in der Dokumentation des Projekts.",
                  "tr":"Geri kalan her şey — yedekler, sunucu taşıma, yeni bir adres için istemciyi yeniden derleme — projenin belgelerinde."},
 # --- changing the admin passphrase from here ---
 "op_pp":        {"pl":"Hasło do tego panelu to to, które instalator wypisał na koniec. Możesz wybrać własne poniżej — instalator pokaże aktualne za każdym razem, gdy go ponownie uruchomisz.","en":"The passphrase for this panel is the one the installer printed when it finished. You can pick your own below — the installer will show whichever one is current every time you run it again.",
                  "de":"Die Passphrase für dieses Panel ist die, die der Installer am Ende ausgegeben hat. Unten kannst du eine eigene wählen — der Installer zeigt dir bei jedem weiteren Lauf die jeweils aktuelle.",
                  "tr":"Bu panelin gizli kelimesi, kurulum bittiğinde yazdırdığı kelimedir. Aşağıdan kendi seçebilirsin — kurulumu her yeniden çalıştırdığında sana güncel olanı gösterir."},
 "pp_title":     {"pl":"🔑 Hasło administratora","en":"🔑 Admin passphrase","de":"🔑 Admin-Passphrase","tr":"🔑 Yönetici gizli kelimesi"},
 "pp_old":       {"pl":"Obecne hasło","en":"Current passphrase","de":"Aktuelle Passphrase","tr":"Mevcut gizli kelime"},
 "pp_new":       {"pl":"Nowe hasło (co najmniej {n} znaków)","en":"New passphrase (at least {n} characters)","de":"Neue Passphrase (mindestens {n} Zeichen)","tr":"Yeni gizli kelime (en az {n} karakter)"},
 "pp_new2":      {"pl":"Nowe hasło jeszcze raz","en":"New passphrase again","de":"Neue Passphrase wiederholen","tr":"Yeni gizli kelimeyi tekrar"},
 "pp_save":      {"pl":"🔑 Zmień hasło","en":"🔑 Change passphrase","de":"🔑 Passphrase ändern","tr":"🔑 Gizli kelimeyi değiştir"},
 "pp_local":     {"pl":"Ten serwer słucha tylko tego komputera, więc panel nie pyta o hasło. Ustawienie go teraz oznacza, że jest gotowe, gdyby ten serwer trafił kiedyś tam, gdzie dosięgną go inni.","en":"This server only listens to this computer, so the panel does not ask for a passphrase. Setting one now means it is ready if you ever put this server somewhere others can reach.",
                  "de":"Dieser Server lauscht nur auf diesem Rechner, deshalb fragt das Panel gar nicht nach einer Passphrase. Wenn du jetzt eine setzt, ist sie bereit, falls du den Server später irgendwo hinstellst, wo andere ihn erreichen.",
                  "tr":"Bu sunucu yalnızca bu bilgisayarı dinliyor, bu yüzden panel gizli kelime sormuyor. Şimdi bir tane belirlersen, sunucuyu ileride başkalarının erişebileceği bir yere koyduğunda hazır olur."},
 "pp_done":      {"pl":"Hasło zmienione. Od teraz używaj nowego — instalator pokaże je przy następnym uruchomieniu.","en":"Passphrase changed. Use the new one from now on — the installer will show it the next time you run it.",
                  "de":"Passphrase geändert. Ab jetzt gilt die neue — der Installer zeigt sie dir beim nächsten Lauf an.",
                  "tr":"Gizli kelime değiştirildi. Bundan sonra yenisi geçerli — kurulumu bir dahaki çalıştırmanda onu gösterecek."},
 "pp_done_unsaved":{"pl":"Hasło zmienione — od teraz używaj nowego. Nie udało się zapisać go dla instalatora, więc zanotuj je sobie.","en":"Passphrase changed — use the new one from now on. It could not be written down for the installer, so make a note of it yourself.",
                  "de":"Passphrase geändert — ab jetzt gilt die neue. Sie konnte für den Installer nicht hinterlegt werden, notier sie dir also selbst.",
                  "tr":"Gizli kelime değiştirildi — bundan sonra yenisi geçerli. Kurulum için kaydedilemedi, bu yüzden kendin not al."},
 "pp_bad_old":   {"pl":"To nie jest obecne hasło, więc nic nie zmieniono.","en":"That is not the current passphrase, so nothing was changed.",
                  "de":"Das ist nicht die aktuelle Passphrase, es wurde nichts geändert.",
                  "tr":"Bu, mevcut gizli kelime değil; hiçbir şey değiştirilmedi."},
 "pp_short":     {"pl":"Hasło musi mieć co najmniej {n} znaków. Nic nie zmieniono.","en":"A passphrase needs at least {n} characters. Nothing was changed.",
                  "de":"Eine Passphrase braucht mindestens {n} Zeichen. Es wurde nichts geändert.",
                  "tr":"Gizli kelime en az {n} karakter olmalı. Hiçbir şey değiştirilmedi."},
 "pp_mismatch":  {"pl":"Dwa nowe hasła nie są takie same, więc nic nie zmieniono.","en":"The two new passphrases are not the same, so nothing was changed.",
                  "de":"Die beiden neuen Passphrasen sind nicht gleich, es wurde nichts geändert.",
                  "tr":"İki yeni gizli kelime aynı değil; hiçbir şey değiştirilmedi."},
 "pp_failed":    {"pl":"Nie udało się zapisać hasła, więc pozostało bez zmian. Plik ustawień panelu nie jest zapisywalny.","en":"The passphrase could not be saved, so it is unchanged. The panel's settings file is not writable.",
                  "de":"Die Passphrase konnte nicht gespeichert werden, sie bleibt also unverändert. Die Einstellungsdatei des Panels ist nicht beschreibbar.",
                  "tr":"Gizli kelime kaydedilemedi, bu yüzden değişmedi. Panelin ayar dosyası yazılabilir değil."},
 "pp_env":       {"pl":"Ten panel bierze hasło ze środowiska, nie z pliku ustawień, więc nie da się go tu zmienić. Zmień je tam, gdzie ta wartość jest ustawiona.","en":"This panel takes its passphrase from its environment, not from its settings file, so it cannot be changed here. Change it where that value is set.",
                  "de":"Dieses Panel bezieht seine Passphrase aus der Umgebung, nicht aus seiner Einstellungsdatei — hier lässt sie sich deshalb nicht ändern. Ändere sie dort, wo dieser Wert gesetzt wird.",
                  "tr":"Bu panel gizli kelimesini ayar dosyasından değil ortam değişkeninden alıyor, bu yüzden burada değiştirilemez. Bu değerin ayarlandığı yerden değiştir."},
 "tip_pp":       {"pl":"Zmienia hasło do tego panelu administratora. Nie dotyczy żadnego konta gry.","en":"Changes the passphrase for this admin panel. It does not affect any game account.",
                  "de":"Ändert die Passphrase für dieses Admin-Panel. Auf Spielkonten hat das keine Auswirkung.",
                  "tr":"Bu yönetim panelinin gizli kelimesini değiştirir. Oyun hesaplarını etkilemez."},
 "op_hide":      {"pl":"Rozumiem — ukryj to","en":"Got it — hide this","de":"Verstanden — ausblenden","tr":"Anladım — gizle"},
 "op_show":      {"pl":"Pokaż wprowadzenie ponownie","en":"Show the introduction again","de":"Einführung wieder anzeigen","tr":"Tanıtımı yeniden göster"},
 # --- admin-made password reset links ---
 "reset_title":  {"pl":"Link do resetu hasła","en":"Password reset link","de":"Passwort-Reset-Link","tr":"Şifre sıfırlama bağlantısı"},
 "reset_hint":   {"pl":"Gracz, który zapomniał hasła, pisze do Ciebie (adres jest na stronie głównej). Wpisz tu jego nazwę użytkownika, wyślij mu utworzony link, a nowe hasło wybierze sam. Każdy link działa raz i wygasa po 24 godzinach; utworzenie nowego unieważnia stary.","en":"A player who forgot their password writes to you (the address is on the front page). Type their username here, send them the link this creates, and they choose a new password themselves. Each link works once and expires after 24 hours; making a new one cancels the old.",
                  "de":"Ein Spieler, der sein Passwort vergessen hat, schreibt dir (die Adresse steht auf der Startseite). Trage hier seinen Benutzernamen ein, schicke ihm den erzeugten Link, und er wählt selbst ein neues Passwort. Jeder Link funktioniert einmal und verfällt nach 24 Stunden; ein neuer Link macht den alten ungültig.",
                  "tr":"Şifresini unutan oyuncu sana yazar (adres ana sayfada). Buraya kullanıcı adını yaz, oluşan bağlantıyı ona gönder, yeni şifresini kendisi seçer. Her bağlantı bir kez çalışır ve 24 saat sonra geçersiz olur; yenisi eskisini iptal eder."},
 "reset_user_ph":{"pl":"Nazwa użytkownika gracza","en":"Player's username","de":"Benutzername des Spielers","tr":"Oyuncunun kullanıcı adı"},
 "reset_make":   {"pl":"🔗 Utwórz link do resetu","en":"🔗 Create reset link","de":"🔗 Reset-Link erstellen","tr":"🔗 Bağlantı oluştur"},
 "reset_noacc":  {"pl":"Nie ma konta o tej nazwie użytkownika.","en":"There is no account with that username.","de":"Es gibt kein Konto mit diesem Benutzernamen.","tr":"Bu kullanıcı adıyla bir hesap yok."},
 "reset_made":   {"pl":"Wyślij ten link graczowi — działa raz i wygasa po 24 godzinach:","en":"Send this link to the player — it works once and expires in 24 hours:",
                  "de":"Schick diesen Link an den Spieler — er funktioniert einmal und verfällt in 24 Stunden:",
                  "tr":"Bu bağlantıyı oyuncuya gönder — bir kez çalışır ve 24 saat sonra geçersiz olur:"},
 "reset_set_title":{"pl":"Ustaw nowe hasło","en":"Set a new password","de":"Neues Passwort setzen","tr":"Yeni şifre belirle"},
 "reset_for":    {"pl":"Nowe hasło dla konta","en":"New password for account","de":"Neues Passwort für das Konto","tr":"Yeni şifre belirlenecek hesap:"},
 "reset_ph1":    {"pl":"Nowe hasło (co najmniej 6 znaków)","en":"New password (at least 6 characters)","de":"Neues Passwort (mindestens 6 Zeichen)","tr":"Yeni şifre (en az 6 karakter)"},
 "reset_ph2":    {"pl":"Nowe hasło jeszcze raz","en":"New password again","de":"Neues Passwort wiederholen","tr":"Yeni şifre (tekrar)"},
 "reset_set_btn":{"pl":"🔒 Zapisz nowe hasło","en":"🔒 Save new password","de":"🔒 Neues Passwort speichern","tr":"🔒 Yeni şifreyi kaydet"},
 "reset_bad_link":{"pl":"Ten link jest już nieważny — został użyty, wygasł albo zastąpił go nowszy. Poproś administratora o nowy link.","en":"This link is not valid any more — it was already used, has expired, or was replaced by a newer one. Ask the admin for a fresh link.",
                  "de":"Dieser Link ist nicht mehr gültig — er wurde schon benutzt, ist abgelaufen oder wurde durch einen neueren ersetzt. Bitte den Admin um einen frischen Link.",
                  "tr":"Bu bağlantı artık geçerli değil — zaten kullanıldı, süresi doldu ya da yenisiyle değiştirildi. Yöneticiden yeni bir bağlantı iste."},
 "reset_short":  {"pl":"Nowe hasło musi mieć co najmniej 6 znaków. 🙂","en":"The new password must be at least 6 characters. 🙂","de":"Das neue Passwort muss mindestens 6 Zeichen haben. 🙂","tr":"Yeni şifre en az 6 karakter olmalı. 🙂"},
 "reset_mismatch":{"pl":"Hasła nie są takie same — spróbuj ponownie. 🙂","en":"The two passwords don't match — try again. 🙂","de":"Die beiden Passwörter stimmen nicht überein — versuch es noch einmal. 🙂","tr":"İki şifre birbirini tutmuyor — tekrar dene. 🙂"},
 "reset_done":   {"pl":"Twoje hasło zostało zmienione — możesz się nim od razu zalogować do gry. 🎉","en":"Your password has been changed — you can log into the game with it right away. 🎉",
                  "de":"Dein Passwort wurde geändert — du kannst dich damit sofort im Spiel anmelden. 🎉",
                  "tr":"Şifren değiştirildi — hemen oyuna girebilirsin. 🎉"},
 "tip_reset":    {"pl":"Tworzy jednorazowy link, którym ten gracz sam ustawi nowe hasło. Na koncie nic się nie zmienia, dopóki link nie zostanie użyty.","en":"Creates a one-time link that lets this player set a new password themselves. Nothing changes on the account until the link is used.",
                  "de":"Erzeugt einen Einmal-Link, mit dem der Spieler selbst ein neues Passwort setzt. Am Konto ändert sich nichts, bis der Link benutzt wird.",
                  "tr":"Oyuncunun kendisinin yeni şifre belirlemesini sağlayan tek kullanımlık bir bağlantı oluşturur. Bağlantı kullanılana dek hesapta hiçbir şey değişmez."},
 "dl_limit_title":{"pl":"Limit pobrań osiągnięty","en":"Download limit reached","de":"Download-Limit erreicht","tr":"İndirme sınırına ulaşıldı"},
 # Shown when the whole server has hit its daily ceiling rather than the visitor
 # -- otherwise the reader concludes they did something wrong, and they didn't.
 "dl_limit_all": {"pl":"Gra została dziś pobrana maksymalną liczbę razy na całym serwerze. To nie o Ciebie chodzi — ktoś musi być tym, kto przyszedł po ostatnim miejscu. Zwolni się za około {h} h, a przerwane pobieranie zawsze można wznowić, co nic nie kosztuje.","en":"The game has been downloaded the maximum number of times across the whole server today. This is not about you — somebody has to be the one who arrives after the last slot. It frees up again in about {h} h, and an interrupted download can always be resumed, which costs nothing.",
                  "de":"Das Spiel wurde heute serverweit schon so oft heruntergeladen, wie erlaubt ist. Das liegt nicht an dir — irgendwer muss der sein, der nach dem letzten freien Platz ankommt. In etwa {h} Std. wird wieder einer frei. Ein abgebrochener Download lässt sich jederzeit fortsetzen, das kostet nichts.",
                  "tr":"Oyun bugün sunucu genelinde izin verilen en yüksek sayıda indirildi. Bu senden kaynaklanmıyor — birinin son boş yerden sonra gelmesi gerekiyordu. Yaklaşık {h} saat içinde yeniden yer açılır. Yarım kalan bir indirme her zaman kaldığı yerden sürdürülebilir, bu sınırdan sayılmaz."},
 "dl_limit":     {"pl":"Gra została już pobrana 3 razy z Twojego adresu w ciągu ostatnich 24 godzin — to limit, żeby łącze serwera zostało wolne do grania. Spróbuj ponownie za około {h} h. Przerwane pobieranie zawsze można wznowić, to nic nie kosztuje.","en":"The game was already downloaded 3 times from your address in the last 24 hours — that is the limit, so the server's bandwidth stays free for playing. Please try again in about {h} h. An interrupted download can always be resumed, that costs nothing.",
                  "de":"Das Spiel wurde von deiner Adresse in den letzten 24 Stunden schon 3-mal heruntergeladen — mehr geht nicht, damit die Bandbreite des Servers zum Spielen frei bleibt. Versuche es in etwa {h} Std. wieder. Ein abgebrochener Download lässt sich jederzeit fortsetzen, das kostet nichts.",
                  "tr":"Oyun son 24 saatte senin adresinden zaten 3 kez indirildi — sunucunun bant genişliği oyuna kalsın diye sınır bu. Yaklaşık {h} saat sonra tekrar dene. Yarım kalan bir indirme her zaman kaldığı yerden sürdürülebilir, bu sınırdan sayılmaz."},

 # --- mouseover explanations (title="..." on the elements themselves) ---
 "tip_lang":     {"pl":"Przełącza język tego panelu. W grze nic się nie zmienia.","en":"Switch the language of this panel. Nothing in the game changes.",
                  "de":"Ändert die Sprache dieses Panels. Im Spiel ändert sich nichts.",
                  "tr":"Bu panelin dilini değiştirir. Oyunda hiçbir şey değişmez."},
 "tip_logout":   {"pl":"Kończy Twoją sesję administratora w tym panelu. Twoje konto gry pozostaje bez zmian.","en":"End your admin session on this panel. Your game account is not affected.",
                  "de":"Beendet deine Admin-Sitzung in diesem Panel. Dein Spiel-Konto ist davon nicht betroffen.",
                  "tr":"Bu paneldeki yönetici oturumunu kapatır. Oyun hesabın etkilenmez."},
 "tip_passphrase":{"pl":"Hasło administratora wybrane przy instalacji serwera. Gracze go nie potrzebują — tylko osoba prowadząca serwer.","en":"The admin passphrase chosen when the server was installed. Players do not need this — only the person running the server.",
                  "de":"Die Admin-Passphrase, die bei der Installation des Servers gewählt wurde. Spieler brauchen sie nicht — nur wer den Server betreibt.",
                  "tr":"Sunucu kurulurken seçilen yönetici parolası. Oyuncuların buna ihtiyacı yok — sadece sunucuyu işleten kişinin."},
 "tip_login":    {"pl":"Otwiera obszar administratora. Działa tylko z hasłem panelu, nie z hasłem do gry.","en":"Opens the admin area. Only works with the admin passphrase, not with your game password.",
                  "de":"Öffnet den Admin-Bereich. Funktioniert nur mit der Admin-Passphrase, nicht mit deinem Spiel-Passwort.",
                  "tr":"Yönetici alanını açar. Sadece yönetici parolasıyla çalışır, oyun şifrenle değil."},
 "tip_download": {"pl":"Pobiera kompletną grę, około 1,2 GB. Adres serwera jest już wpisany, więc nie musisz nic konfigurować — rozpakuj i uruchom grę. Najwyżej 3 pobrania dziennie; wznowienie przerwanego jest zawsze wolne od limitu.","en":"Downloads the complete game, around 1.2 GB. The server address is already filled in, so you do not have to configure anything — unpack it and start the game. At most 3 downloads per day; resuming an interrupted one is always free.",
                  "de":"Lädt das komplette Spiel herunter, etwa 1,2 GB. Die Serveradresse ist schon eingetragen, du musst nichts einstellen — entpacken und starten. Höchstens 3 Downloads pro Tag; einen abgebrochenen fortzusetzen ist immer frei.",
                  "tr":"Oyunun tamamını indirir, yaklaşık 1,2 GB. Sunucu adresi zaten girili, hiçbir ayar yapman gerekmiyor — çıkart ve başlat. Günde en fazla 3 indirme; yarım kalanı sürdürmek her zaman serbest."},
 "tip_create_acc":{"pl":"Zakłada konto, którym logujesz się do gry. Jest osobne od tego panelu i zajmuje około pół minuty.","en":"Creates the account you log into the game with. It is separate from this panel and takes about half a minute.",
                  "de":"Erstellt das Konto, mit dem du dich im Spiel anmeldest. Es ist unabhängig von diesem Panel und dauert etwa eine halbe Minute.",
                  "tr":"Oyuna giriş yapacağın hesabı oluşturur. Bu panelden bağımsızdır ve yarım dakika sürer."},
 "tip_my_acc":   {"pl":"Zaloguj się tu do konta gry, aby zmienić jego hasło.","en":"Log into your game account here to change its password.",
                  "de":"Melde dich hier mit deinem Spiel-Konto an, um dessen Passwort zu ändern.",
                  "tr":"Oyun hesabının şifresini değiştirmek için burada giriş yap."},
 "tip_players":  {"pl":"Każda postać istniejąca na tym serwerze. Postacie pojawiają się po tym, jak ktoś się zaloguje i którąś stworzy.","en":"Every character that exists on this server. Characters show up after someone has logged in and created one.",
                  "de":"Alle Charaktere, die es auf diesem Server gibt. Sie erscheinen, sobald jemand sich eingeloggt und einen erstellt hat.",
                  "tr":"Bu sunucudaki tüm karakterler. Biri giriş yapıp karakter oluşturduktan sonra burada görünürler."},
 "tip_player":   {"pl":"Otwórz tę postać, aby dać przedmioty lub yang, ustawić poziom, teleportować ją albo zmienić jej szybkość.","en":"Open this character to give items or yang, set the level, teleport them or change their speed.",
                  "de":"Öffnet diesen Charakter, um Gegenstände oder Yang zu geben, das Level zu setzen, ihn zu teleportieren oder seine Geschwindigkeit zu ändern.",
                  "tr":"Bu karakteri açar: eşya veya yang verebilir, seviyesini ayarlayabilir, ışınlayabilir veya hızını değiştirebilirsin."},
 "tip_search_item":{"pl":"Wpisz część nazwy albo numer przedmiotu, jeśli go znasz. Można przeszukać około 9 800 przedmiotów.","en":"Type part of a name, or an item number if you know it. Around 9,800 items are searchable.",
                  "de":"Tippe einen Teil des Namens oder die Item-Nummer, falls du sie kennst. Rund 9.800 Gegenstände sind durchsuchbar.",
                  "tr":"Adının bir kısmını yaz, ya da biliyorsan eşya numarasını. Yaklaşık 9.800 eşya aranabilir."},
 "tip_category": {"pl":"Zawęża wyszukiwanie do jednego rodzaju przedmiotów, żeby nie przewijać całej reszty.","en":"Narrows the search to one kind of item, so you do not have to scroll past everything else.",
                  "de":"Schränkt die Suche auf eine Art von Gegenstand ein, damit du nicht an allem anderen vorbeiscrollen musst.",
                  "tr":"Aramayı tek bir eşya türüyle sınırlar, böylece diğer her şeyi kaydırmak zorunda kalmazsın."},
 "tip_qty":      {"pl":"Ile sztuk tego przedmiotu dać. Jeden stos mieści najwyżej 65 535.","en":"How many of this item to give. One stack holds at most 65,535.",
                  "de":"Wie viele von diesem Gegenstand gegeben werden. In einen Stapel passen höchstens 65.535.",
                  "tr":"Bu eşyadan kaç adet verilecek. Bir yığında en fazla 65.535 durur."},
 "tip_send_item":{"pl":"Wkłada przedmiot do ekwipunku postaci. Jeśli gracz jest offline, czeka na niego przy następnym logowaniu.","en":"Puts the item into the character's inventory. If they are offline it is waiting at their next login.",
                  "de":"Legt den Gegenstand in das Inventar des Charakters. Ist er offline, liegt er beim nächsten Login bereit.",
                  "tr":"Eşyayı karakterin çantasına koyar. Çevrimdışıysa bir sonraki girişinde onu bekliyor olur."},
 "tip_amount":   {"pl":"Ile yang dodać. Wpisz liczbę ujemną, aby zamiast tego zabrać yang.","en":"How much yang to add. Enter a negative number to take yang away instead.",
                  "de":"Wie viel Yang hinzukommt. Gib eine negative Zahl ein, um stattdessen Yang abzuziehen.",
                  "tr":"Ne kadar yang ekleneceği. Yang almak için eksi bir sayı gir."},
 "tip_level":    {"pl":"Ustawia postać wprost na ten poziom. Nie dodaje poziomów, zastępuje obecny.","en":"Sets the character straight to this level. It does not add levels, it replaces the current one.",
                  "de":"Setzt den Charakter direkt auf dieses Level. Es wird nicht dazugezählt, sondern ersetzt.",
                  "tr":"Karakteri doğrudan bu seviyeye ayarlar. Seviye eklemez, mevcut olanı değiştirir."},
 "tip_teleport": {"pl":"Przenosi postać na inną mapę. Działa tylko wtedy, gdy gracz naprawdę jest w grze.","en":"Moves the character to another map. This only works while they are actually in game.",
                  "de":"Bewegt den Charakter auf eine andere Karte. Das geht nur, solange er wirklich im Spiel ist.",
                  "tr":"Karakteri başka bir haritaya taşır. Bu sadece gerçekten oyundayken çalışır."},
 "tip_speed":    {"pl":"Zmienia, jak szybko postać biega. 100 to normalnie; działa tylko, gdy gracz jest w grze.","en":"Changes how fast the character runs. 100 is normal; this only works while they are in game.",
                  "de":"Ändert, wie schnell der Charakter läuft. 100 ist normal; das geht nur, solange er im Spiel ist.",
                  "tr":"Karakterin ne kadar hızlı koştuğunu değiştirir. 100 normaldir; sadece oyundayken çalışır."},
 "tip_gm":       {"pl":"Daje tej postaci komendy administratora w grze, wpisywane w czacie jako /komenda. Nadanie działa od razu; odebranie wymaga wylogowania i ponownego zalogowania gracza.","en":"Gives this character the in-game admin commands, typed into the chat box as /command. Granting works immediately; taking it away needs the player to log out and back in.",
                  "de":"Gibt diesem Charakter die Admin-Befehle im Spiel, die als /befehl ins Chatfenster getippt werden. Vergeben wirkt sofort; Entziehen wirkt erst, wenn der Spieler sich aus- und wieder einloggt.",
                  "tr":"Bu karaktere, sohbet kutusuna /komut olarak yazılan oyun içi yönetici komutlarını verir. Vermek hemen etkilidir; almak için oyuncunun çıkıp tekrar girmesi gerekir."},
 "tip_rates":    {"pl":"Doświadczenie, drop przedmiotów i yang dla całego serwera. Zapis restartuje grę na mniej niż minutę.","en":"Experience, item drops and yang for the whole server. Saving restarts the game for under a minute.",
                  "de":"Erfahrung, Item-Drops und Yang für den ganzen Server. Beim Speichern startet das Spiel für weniger als eine Minute neu.",
                  "tr":"Tüm sunucu için tecrübe, eşya düşme oranı ve yang. Kaydettiğinde oyun bir dakikadan kısa bir süre yeniden başlar."},
 "tip_delcode":  {"pl":"Siedem cyfr, o które gra pyta przy kasowaniu postaci. Wybierz coś, co zapamiętasz.","en":"Seven digits the game asks for when you delete a character. Pick something you will remember.",
                  "de":"Sieben Ziffern, nach denen das Spiel fragt, wenn du einen Charakter löschst. Wähle etwas, das du dir merkst.",
                  "tr":"Bir karakteri silerken oyunun soracağı yedi rakam. Hatırlayacağın bir şey seç."},
 # --- version, patch log, updates -------------------------------------------
 "ver_label":    {"pl":"Wersja","en":"Version","de":"Version","tr":"Sürüm"},
 "ver_unknown":  {"pl":"nieznana","en":"unknown","de":"unbekannt","tr":"bilinmiyor"},
 "ver_unknown_why":{"pl":"Ta kompilacja nie ma pliku wersji, więc panel nie potrafi powiedzieć, która to — i nie będzie zgadywał. Sprawdzanie aktualizacji jest wyłączone, dopóki nie będzie mógł.","en":"This build does not carry a version file, so the panel cannot say which one it is — and will not guess. Update checks stay off until it can.",
                  "de":"Dieser Build enthält keine Versionsdatei, also kann das Panel nicht sagen, welche Version es ist — und rät nicht. Solange bleibt die Update-Prüfung ohne Wirkung.",
                  "tr":"Bu yapıda sürüm dosyası yok, bu yüzden panel hangi sürüm olduğunu söyleyemez — ve tahmin etmez. O zamana kadar güncelleme kontrolü sonuç vermez."},
 "pl_nav":       {"pl":"Lista zmian","en":"Patch log","de":"Änderungsprotokoll","tr":"Sürüm notları"},
 "pl_dash_hint": {"pl":"Co zmieniła ta wersja i co zmieniłaby nowsza.","en":"What changed in this build, and what a newer one would change.",
                  "de":"Was sich in diesem Build geändert hat — und was ein neuerer ändern würde.",
                  "tr":"Bu yapıda neler değişti ve daha yenisi neleri değiştirir."},
 "tip_patchlog": {"pl":"Lista zmian projektu: wersja, którą masz uruchomioną, i — jeśli jest — wersja opublikowana od tego czasu.","en":"The project's changelog: the version you are running, and — when there is one — the version that has been published since.",
                  "de":"Das Änderungsprotokoll des Projekts: die Version, die du fährst, und — falls vorhanden — die inzwischen veröffentlichte.",
                  "tr":"Projenin değişiklik günlüğü: çalıştırdığın sürüm ve — varsa — o zamandan beri yayımlanan sürüm."},
 "pl_none":      {"pl":"Lista zmian nie jest częścią tej kompilacji, więc nie ma tu nic do pokazania.","en":"The changelog is not part of this build, so there is nothing to show here.",
                  "de":"Das Änderungsprotokoll ist in diesem Build nicht enthalten, hier gibt es also nichts zu zeigen.",
                  "tr":"Değişiklik günlüğü bu yapıda yok, bu yüzden burada gösterilecek bir şey yok."},
 "upd_avail_t":  {"pl":"Opublikowano nowszą wersję","en":"A newer version has been published","de":"Es wurde eine neuere Version veröffentlicht","tr":"Daha yeni bir sürüm yayımlandı"},
 "upd_avail":    {"pl":"Masz uruchomioną {cur}. Dostępna jest {new}.","en":"You are running {cur}. {new} is available.",
                  "de":"Bei dir läuft {cur}. Verfügbar ist {new}.",
                  "tr":"Sende {cur} çalışıyor. {new} mevcut."},
 "upd_avail_short":{"pl":"dostępna aktualizacja","en":"update available","de":"Update verfügbar","tr":"güncelleme var"},
 "upd_see":      {"pl":"Zobacz, co przynosi","en":"See what it brings","de":"Ansehen, was es bringt","tr":"Neler getirdiğine bak"},
 "pl_card_t":    {"pl":"Wersja i zmiany","en":"Version and changes","de":"Version und Änderungen","tr":"Sürüm ve değişiklikler"},
 "pl_new_t":     {"pl":"Co przyniosłaby aktualizacja","en":"What an update would bring","de":"Was ein Update bringen würde","tr":"Güncelleme ne getirir"},
 "pl_have_t":    {"pl":"Co masz uruchomione","en":"What you are running","de":"Was bei dir läuft","tr":"Çalıştırdığın sürüm"},
 "pl_check":     {"pl":"🔄 Sprawdź najnowszą wersję","en":"🔄 Check for the latest version","de":"🔄 Nach der neuesten Version suchen","tr":"🔄 En son sürümü kontrol et"},
 "pl_check_new": {"pl":"Dostępna jest nowsza wersja: {new}.","en":"A newer version is available: {new}.","de":"Eine neuere Version ist verfügbar: {new}.","tr":"Daha yeni bir sürüm var: {new}."},
 "pl_check_ok":  {"pl":"Sprawdzono. Masz już najnowszą opublikowaną wersję.","en":"Checked. You already have the newest published version.","de":"Geprüft. Du hast bereits die neueste veröffentlichte Version.","tr":"Kontrol edildi. Zaten en yeni yayımlanan sürüme sahipsin."},
 "pl_check_bad": {"pl":"Nie udało się połączyć z serwerem aktualizacji. Na Twoim serwerze nic się nie zmieniło.","en":"The update server could not be reached. Nothing on your server changed.","de":"Zugriff auf den Update-Server war nicht möglich. An deinem Server hat sich nichts geändert.","tr":"Güncelleme sunucusuna ulaşılamadı. Sunucunda hiçbir şey değişmedi."},
 "pl_check_off": {"pl":"Sprawdzanie aktualizacji jest wyłączone, więc o nic nie zapytano. Ustaw M2_UPDATE_CHECK=1 w .env, aby je włączyć.","en":"The update check is switched off, so this asked nothing. Set M2_UPDATE_CHECK=1 in .env to turn it back on.","de":"Die Update-Prüfung ist ausgeschaltet, es wurde also nichts abgefragt. Setze M2_UPDATE_CHECK=1 in .env, um sie wieder einzuschalten.","tr":"Güncelleme kontrolü kapalı, bu yüzden hiçbir sorgu yapılmadı. Yeniden açmak için .env içinde M2_UPDATE_CHECK=1 ayarla."},
 "pl_check_wait":{"pl":"Sprawdzono przed chwilą — daj temu minutę.","en":"Just checked a moment ago — give it a minute.","de":"Gerade eben schon geprüft — gib ihm eine Minute.","tr":"Az önce kontrol edildi — bir dakika bekle."},
 "pl_open":      {"pl":"📜 Otwórz listę zmian","en":"📜 Open the patch log","de":"📜 Patchlog öffnen","tr":"📜 Sürüm notlarını aç"},
 "upd_none":     {"pl":"To najnowsza opublikowana wersja.","en":"This is the newest published version.","de":"Das ist die neueste veröffentlichte Version.","tr":"Bu, yayımlanan en yeni sürüm."},
 "upd_next_line": {"pl":"Istnieje też linia 2.x (silnik mt2009, wersja {new}). To osobna paczka z Discorda, nie aktualizacja tej instalacji — ten launcher i klient 1.x z nią nie działają.","en":"There is also the 2.x line (mt2009 engine, version {new}). It is a separate package from the Discord, not an update of this install — this launcher and the 1.x client do not work with it.","de":"Es gibt auch die 2.x-Linie (mt2009-Engine, Version {new}). Das ist ein eigenes Paket vom Discord, kein Update dieser Installation — dieser Launcher und der 1.x-Client funktionieren damit nicht.","tr":"Ayrıca 2.x hattı var (mt2009 motoru, sürüm {new}). Bu, Discord'dan ayrı bir pakettir, bu kurulumun güncellemesi değil — bu başlatıcı ve 1.x istemcisi onunla çalışmaz."},
 "upd_never":    {"pl":"Jeszcze nie sprawdzono — pierwsze sprawdzenie następuje kilka minut po starcie panelu.","en":"Not checked yet — the first check happens a couple of minutes after the panel starts.",
                  "de":"Noch nicht geprüft — die erste Prüfung läuft ein paar Minuten nach dem Start des Panels.",
                  "tr":"Henüz kontrol edilmedi — ilk kontrol panel başladıktan birkaç dakika sonra yapılır."},
 "upd_failed":   {"pl":"Nie udało się połączyć z serwerem aktualizacji.","en":"The update server could not be reached.",
                  "de":"Zugriff auf den Update-Server war nicht möglich.",
                  "tr":"Güncelleme sunucusuna ulaşılamadı."},
 "upd_checked":  {"pl":"Ostatnio sprawdzono","en":"Last checked","de":"Zuletzt geprüft","tr":"Son kontrol"},
 "upd_off_t":    {"pl":"Sprawdzanie aktualizacji jest wyłączone","en":"The update check is switched off","de":"Die Update-Prüfung ist ausgeschaltet","tr":"Güncelleme kontrolü kapalı"},
 "upd_off":      {"pl":"Ten panel nie łączy się z niczym. Nie dowiesz się, gdy pojawi się nowsza wersja; zajrzyj na stronę projektu, gdy zechcesz sprawdzić. Aby włączyć z powrotem, ustaw M2_UPDATE_CHECK=1 w .env i zrestartuj panel.","en":"This panel is not contacting anything at all. You will not be told when a newer version appears; look at the project page when you want to know. To switch it back on, set M2_UPDATE_CHECK=1 in .env and restart the panel.",
                  "de":"Dieses Panel kontaktiert überhaupt nichts. Du wirst nicht erfahren, wenn eine neuere Version erscheint; schau auf der Projektseite nach, wenn du es wissen willst. Zum Einschalten: M2_UPDATE_CHECK=1 in der .env setzen und das Panel neu starten.",
                  "tr":"Bu panel hiçbir yere bağlanmıyor. Yeni bir sürüm çıktığında haber verilmez; öğrenmek istediğinde proje sayfasına bak. Açmak için .env dosyasında M2_UPDATE_CHECK=1 yap ve paneli yeniden başlat."},
 "upd_phone_t":  {"pl":"Ta strona łączy się z internetem","en":"This page reaches the internet","de":"Diese Seite geht ins Internet","tr":"Bu sayfa internete çıkıyor"},
 "upd_phone":    {"pl":"Raz dziennie, w tle, panel pobiera dwa pliki tekstowe z repozytorium projektu na GitHubie: numer opublikowanej wersji i — tylko gdy jest nowsza od Twojej — listę zmian. To jedyna rzecz w całym projekcie, która sama z czymś się łączy, i oznacza, że GitHub widzi adres Twojego serwera i czas zapytania, mniej więcej raz dziennie, tak jak przy każdym wejściu na stronę. Nic o Twoim serwerze, graczach ani bazie nie jest wysyłane, nic z tego, co wraca, nie jest wykonywane, i żadna strona na to nie czeka.","en":"Once a day, in the background, the panel fetches two text files from the project's repository on GitHub: the published version number, and — only when it is newer than yours — the changelog. That is the only thing in this whole project that contacts anything by itself, and it means GitHub sees your server's address and the time of the request, roughly once a day, the same as any visit to a web page would. Nothing about your server, your players or your database is sent, nothing that comes back is executed, and no page ever waits for it.",
                  "de":"Einmal am Tag holt das Panel im Hintergrund zwei Textdateien aus dem Projekt-Repository auf GitHub: die veröffentlichte Versionsnummer und — nur wenn sie neuer ist als deine — das Änderungsprotokoll. Das ist das Einzige in diesem ganzen Projekt, das von sich aus irgendwo Kontakt aufnimmt, und es bedeutet: GitHub sieht die Adresse deines Servers und den Zeitpunkt der Anfrage, ungefähr einmal täglich, genau wie bei jedem Aufruf einer Webseite. Nichts über deinen Server, deine Spieler oder deine Datenbank wird übertragen, nichts davon wird ausgeführt, und keine Seite wartet darauf.",
                  "tr":"Panel günde bir kez, arka planda, GitHub'daki proje deposundan iki metin dosyası alır: yayımlanan sürüm numarası ve — yalnızca seninkinden yeniyse — değişiklik günlüğü. Bu, tüm projede kendi başına bir yere bağlanan tek şeydir ve şu anlama gelir: GitHub, günde yaklaşık bir kez, sunucunun adresini ve isteğin zamanını görür — herhangi bir web sayfasını açmakla aynı. Sunucun, oyuncuların veya veritabanın hakkında hiçbir şey gönderilmez, gelen hiçbir şey çalıştırılmaz ve hiçbir sayfa bunu beklemez."},
 "upd_phone_off":{"pl":"Aby to wyłączyć: ustaw M2_UPDATE_CHECK=0 w .env i zrestartuj panel. Wszystko inne działa dokładnie tak jak teraz.","en":"To stop it: set M2_UPDATE_CHECK=0 in .env and restart the panel. Everything else keeps working exactly as it does now.",
                  "de":"So schaltest du es ab: M2_UPDATE_CHECK=0 in der .env setzen und das Panel neu starten. Alles andere funktioniert genau wie bisher.",
                  "tr":"Kapatmak için: .env dosyasında M2_UPDATE_CHECK=0 yap ve paneli yeniden başlat. Diğer her şey aynen çalışmaya devam eder."},
 # --- installing an update from the panel ------------------------------------
 "upd_nav":      {"pl":"Aktualizuj ten serwer","en":"Update this server","de":"Diesen Server aktualisieren","tr":"Bu sunucuyu güncelle"},
 "upd_page_t":   {"pl":"Zainstaluj aktualizację","en":"Install the update","de":"Update installieren","tr":"Güncellemeyi kur"},
 "upd_open":     {"pl":"⬆️ Zainstaluj stąd","en":"⬆️ Install it from here","de":"⬆️ Von hier installieren","tr":"⬆️ Buradan kur"},
 "upd_warn_t":   {"pl":"Zanim zaczniesz","en":"Before you start","de":"Bevor du startst","tr":"Başlamadan önce"},
 "upd_warn":     {"pl":"Gra jest przebudowywana i restartowana, więc wszyscy grający zostają rozłączeni i nie mogą się zalogować, dopóki nie wstanie ponownie. Zwykle dwie–trzy minuty; dłużej, gdy sama gra musi zostać przekompilowana. Konta, postacie, przedmioty i gildie pozostają nietknięte — żyją w bazie danych, a nic w aktualizacji się do niej nie zbliża.","en":"The game is rebuilt and restarted, so everyone playing is disconnected and cannot log back in until it is up again. Usually two or three minutes; longer when the game itself has to be recompiled. Accounts, characters, items and guilds are not touched — they live in the database, and nothing in an update goes near it.",
                  "de":"Das Spiel wird neu gebaut und neu gestartet, alle Spielenden fliegen dabei raus und kommen erst wieder rein, wenn es läuft. Meist zwei bis drei Minuten; länger, wenn das Spiel selbst neu übersetzt werden muss. Konten, Charaktere, Gegenstände und Gilden bleiben unangetastet — sie liegen in der Datenbank, und kein Update fasst sie an.",
                  "tr":"Oyun yeniden derlenip yeniden başlatılır; oynayan herkesin bağlantısı kesilir ve sunucu geri gelene kadar giriş yapamazlar. Genellikle iki üç dakika; oyunun kendisi yeniden derlenmesi gerekiyorsa daha uzun. Hesaplar, karakterler, eşyalar ve loncalar etkilenmez — onlar veritabanında durur ve hiçbir güncelleme oraya dokunmaz."},
 "upd_warn_panel":{"pl":"Panel też się restartuje, więc ta strona pod koniec straci na minutę kontakt. Zostaw ją otwartą — sama się odnajdzie i pokaże, jak poszło.","en":"The panel restarts too, so this page will lose contact for a minute near the end. Leave it open — it picks up again by itself and shows how it finished.",
                  "de":"Auch das Panel startet neu, diese Seite verliert also gegen Ende kurz den Kontakt. Lass sie offen — sie meldet sich von selbst zurück und zeigt, wie es ausgegangen ist.",
                  "tr":"Panel de yeniden başlar, bu yüzden bu sayfa sona doğru bir dakikalığına bağlantıyı kaybeder. Açık bırak — kendiliğinden geri döner ve nasıl bittiğini gösterir."},
 "upd_start":    {"pl":"Rozpocznij aktualizację","en":"Start the update","de":"Update starten","tr":"Güncellemeyi başlat"},
 "upd_started":  {"pl":"Aktualizacja została zlecona. Zaczyna się w ciągu kilku sekund.","en":"The update has been requested. It starts within a few seconds.",
                  "de":"Das Update wurde angefordert. Es startet in wenigen Sekunden.",
                  "tr":"Güncelleme istendi. Birkaç saniye içinde başlar."},
 "upd_no_watcher_t":{"pl":"Aktualizator nie działa","en":"The updater is not running","de":"Der Updater läuft nicht","tr":"Güncelleyici çalışmıyor"},
 "upd_no_watcher":{"pl":"Aktualizowanie z panelu jest włączone, ale nie ma małego pomocnika, który je wykonuje — więc przycisk nic by nie zrobił. Uruchom go na serwerze poleceniem:","en":"Updating from the panel is switched on, but the small helper that carries it out is not there — so the button would do nothing. Start it on the server with:",
                  "de":"Updates aus dem Panel sind eingeschaltet, aber das kleine Hilfsprogramm, das sie ausführt, läuft nicht — der Knopf würde also nichts tun. Starte es auf dem Server mit:",
                  "tr":"Panelden güncelleme açık, ama işi yapan küçük yardımcı çalışmıyor — yani düğme hiçbir şey yapmazdı. Sunucuda şununla başlat:"},
 # Deliberately does not name a shell: the same page is served by a server on a
 # Linux VPS and by one on somebody's PC, and the command shown underneath is
 # whichever of the two installed this machine.
 "upd_manual":   {"pl":"Uruchom tę samą komendę, której użyto do instalacji tego serwera. Pobiera opublikowaną wersję, przebudowuje i restartuje:","en":"Run the same command you used to install this server. It fetches the published version, rebuilds and restarts:",
                  "de":"Führe denselben Befehl aus, mit dem du diesen Server installiert hast. Er holt die veröffentlichte Version, baut neu und startet neu:",
                  "tr":"Bu sunucuyu kurarken kullandığın komutu tekrar çalıştır. Yayımlanan sürümü indirir, yeniden derler ve yeniden başlatır:"},
 "upd_manual_keeps":{"pl":"Twoja baza pozostaje nietknięta: konta, postacie, przedmioty i gildie zostają, tak samo jak Twoje ustawienia. Zanim cokolwiek się zmieni, pokazuje, którą wersję masz, a która jest opublikowana, i pyta. Gracze są rozłączeni na czas restartu serwera, zwykle minutę lub dwie.","en":"Your database is not touched: accounts, characters, items and guilds all stay, and so do your settings. Before anything changes it shows which version you have and which one is published, and asks. Players are disconnected while the server restarts, usually for a minute or two.",
                  "de":"Deine Datenbank wird nicht angefasst: Konten, Charaktere, Gegenstände und Gilden bleiben, ebenso deine Einstellungen. Bevor sich etwas ändert, zeigt er dir, welche Version du hast und welche veröffentlicht ist, und fragt nach. Während des Neustarts fliegen Spieler kurz raus, meist für ein bis zwei Minuten.",
                  "tr":"Veritabanına dokunulmaz: hesaplar, karakterler, eşyalar ve loncalar kalır, ayarların da öyle. Herhangi bir şey değişmeden önce hangi sürümde olduğunu ve hangisinin yayımlandığını gösterir ve sorar. Sunucu yeniden başlarken oyuncular kısa süre düşer, genelde bir iki dakika."},
 "upd_progress": {"pl":"Postęp","en":"Progress","de":"Fortschritt","tr":"İlerleme"},
 "upd_waiting":  {"pl":"Czekam, aż aktualizator to podejmie…","en":"Waiting for the updater to pick this up…","de":"Warte darauf, dass der Updater das aufnimmt…","tr":"Güncelleyicinin bunu almasını bekliyorum…"},
 "upd_lost":     {"pl":"Brak kontaktu z panelem — prawdopodobnie się restartuje. Ta strona próbuje dalej.","en":"No contact with the panel — it is probably restarting. This page keeps trying.",
                  "de":"Kein Kontakt zum Panel — es startet vermutlich gerade neu. Diese Seite versucht es weiter.",
                  "tr":"Panelle bağlantı yok — muhtemelen yeniden başlıyor. Bu sayfa denemeye devam ediyor."},
 "upd_st_queued":{"pl":"Zlecono — aktualizator jeszcze nie ruszył.","en":"Requested — the updater has not started yet.","de":"Angefordert — der Updater hat noch nicht begonnen.","tr":"İstendi — güncelleyici henüz başlamadı."},
 "upd_st_running":{"pl":"W toku. Nie wyłączaj serwera, gdy to trwa.","en":"Running. Do not close the server down while this is happening.","de":"Läuft. Fahre den Server währenddessen nicht herunter.","tr":"Çalışıyor. Bu sırada sunucuyu kapatma."},
 "upd_st_ok":    {"pl":"Gotowe. Serwer działa w nowej wersji.","en":"Done. The server is running the new version.","de":"Fertig. Der Server läuft auf der neuen Version.","tr":"Bitti. Sunucu yeni sürümle çalışıyor."},
 "upd_st_failed":{"pl":"Nie dokończono. Twój serwer został w wersji, którą miał — nic nie usunięto i żadne dane nie zostały tknięte. Log poniżej mówi, gdzie się zatrzymało.","en":"It did not finish. Your server was left running the version it had — nothing was removed and no data was touched. The log below says where it stopped.",
                  "de":"Es wurde nicht fertig. Dein Server läuft weiter auf der bisherigen Version — nichts wurde entfernt und keine Daten angefasst. Das Protokoll unten sagt, wo es stehen geblieben ist.",
                  "tr":"Tamamlanmadı. Sunucun eski sürümüyle çalışmaya devam ediyor — hiçbir şey silinmedi, hiçbir veriye dokunulmadı. Aşağıdaki günlük nerede durduğunu söylüyor."},
 "upd_req_failed":{"pl":"Nie udało się zapisać żądania — wspólny katalog aktualizatora nie jest podmontowany w tym kontenerze.","en":"The request could not be written — the shared updater directory is not mounted in this container.",
                  "de":"Die Anfrage konnte nicht geschrieben werden — das gemeinsame Updater-Verzeichnis ist in diesem Container nicht eingebunden.",
                  "tr":"İstek yazılamadı — paylaşılan güncelleyici dizini bu kapsayıcıda bağlı değil."},
 "upd_not_now":  {"pl":"W tej chwili nie ma nic do zainstalowania.","en":"There is nothing to install right now.","de":"Im Moment gibt es nichts zu installieren.","tr":"Şu anda kurulacak bir şey yok."},
}


# The live map can now be switched to Polish.  These shared-frame strings are
# the only values from the older panel dictionary that the map renders.
T["logout"]["pl"] = "Wyloguj"


T["back_front"]["pl"] = "← Strona główna"


T["tip_lang"]["pl"] = "Zmień język tego panelu. Język w grze pozostaje bez zmian."


T["tip_logout"]["pl"] = "Zakończ sesję administratora w panelu. Konto w grze pozostaje bez zmian."


# MT2009 Plus: what this server is, in place of the upstream project's words.
T["about_goal"] = {
    "pl": "MT2009 PLUS to lokalny świat Metin2 singleplayer na plikach serwerowych mt2009, pełen autonomicznych postaci (Playerbots): boty zdobywają poziomy, walczą solo i w grupach, ulepszają ekwipunek, polują na Metiny i handlują między sobą.",
    "en": "MT2009 PLUS is a local singleplayer Metin2 world on the mt2009 server files, full of autonomous characters (Playerbots): the bots level up, fight solo and in parties, refine their gear, hunt Metin stones and trade with each other.",
    "de": "MT2009 PLUS ist eine lokale Singleplayer-Welt von Metin2 auf den mt2009-Serverdateien, voller autonomer Charaktere (Playerbots): Die Bots steigen auf, kämpfen allein und in Gruppen, verbessern ihre Ausrüstung, jagen Metinsteine und handeln miteinander.",
    "tr": "MT2009 PLUS, mt2009 sunucu dosyaları üzerinde çalışan yerel bir tek oyunculu Metin2 dünyasıdır ve otonom karakterlerle (Playerbots) doludur: botlar seviye atlar, tek başına ve grupla savaşır, ekipmanını geliştirir, Metin taşı avlar ve birbirleriyle ticaret yapar.",
}


T["about_hobby"] = {
    "pl": "To paczka modyfikacji oficjalnego wydania Metin2 Playerbots od Tieru. Dodaje ponad 2200 nowych przedmiotów i systemów: kostiumy, fryzury, nakładki na broń, szarfy, mounty, pety i alchemię (Smocze Kamienie), a do tego ItemShop w grze i w przeglądarce.",
    "en": "It is a modification pack of Tieru's official Metin2 Playerbots release. It adds over 2,200 new items and systems: costumes, hairstyles, weapon skins, sashes, mounts, pets and alchemy (Dragon Soul), plus an ItemShop in the game and in the browser.",
    "de": "Es ist ein Modifikationspaket der offiziellen Metin2-Playerbots-Veröffentlichung von Tieru. Es bringt über 2.200 neue Gegenstände und Systeme: Kostüme, Frisuren, Waffenskins, Schärpen, Reittiere, Haustiere und Alchemie (Drachenstein), dazu einen ItemShop im Spiel und im Browser.",
    "tr": "Tieru'nun resmî Metin2 Playerbots sürümünün bir modifikasyon paketidir. 2.200'den fazla yeni eşya ve sistem ekler: kostümler, saç modelleri, silah kaplamaları, kuşaklar, binekler, evcil hayvanlar ve simya (Ejderha Taşı), ayrıca oyun içi ve tarayıcıda ItemShop.",
}


T["about_oss"] = {
    "pl": "Aktualizacje przychodzą przez launcher z repozytorium MT2009 PLUS, a nowości i pomoc znajdziesz na stronie projektu i na Discordzie.",
    "en": "Updates arrive through the launcher from the MT2009 PLUS repository; news and help are on the project's website and on the Discord.",
    "de": "Updates kommen über den Launcher aus dem MT2009-PLUS-Repository; Neuigkeiten und Hilfe gibt es auf der Projektseite und im Discord.",
    "tr": "Güncellemeler başlatıcı üzerinden MT2009 PLUS deposundan gelir; haberler ve yardım proje sitesinde ve Discord'da.",
}


T["site_link"] = {"pl": "🌐 Strona projektu: metin2sp.pl", "en": "🌐 Project website: metin2sp.pl",
                  "de": "🌐 Projektseite: metin2sp.pl", "tr": "🌐 Proje sitesi: metin2sp.pl"}


# --- Module 5: the goal weights. Four languages like the rest of the panel;
#     the labels are one word each on purpose, because eleven sliders with a
#     paragraph apiece is a wall nobody reads.
T.update({
 "ai_nav":       {"en":"🧠 Bot behaviour","pl":"🧠 Zachowanie botów",
                  "de":"🧠 Bot-Verhalten","tr":"🧠 Bot davranışı"},
 "ai_open":      {"en":"🧠 Open bot behaviour","pl":"🧠 Otwórz zachowanie botów",
                  "de":"🧠 Bot-Verhalten öffnen","tr":"🧠 Bot davranışını aç"},
 "ai_dash_hint": {"en":"Decide what the bots spend their time on \u2014 more metin hunters, fewer anglers, a busier market. Takes effect within five seconds, nothing restarts.",
                  "pl":"Zdecyduj, na co boty poświęcają czas \u2014 więcej łowców metinów, mniej wędkarzy, ruchliwszy targ. Działa w ciągu pięciu sekund, nic się nie restartuje.",
                  "de":"Entscheide, womit die Bots ihre Zeit verbringen \u2014 mehr Metin-Jäger, weniger Angler, ein belebterer Markt. Wirkt binnen fünf Sekunden, nichts startet neu.",
                  "tr":"Botların vaktini neye harcayacağını sen seç \u2014 daha çok metin avcısı, daha az balıkçı, daha hareketli pazar. Beş saniyede etkili olur, hiçbir şey yeniden başlamaz."},
 "ai_intro":    {"en":"The settings below are live: the game core reads them every five seconds. Under each one it says what it moves and how soon - some change how many bots take an errand up, some when they set out, and four can only make an errand rarer, because at 100 the bots already take it at every chance. Surviving a losing fight, choosing a profession and finding a weapon are never affected — those are not preferences.",
                 "pl":"Poniższe ustawienia działają na żywo: rdzeń gry czyta je co pięć sekund. Pod każdym jest napisane, co zmienia i jak szybko - jedne zmieniają, ilu botów czymś się zajmuje, inne, kiedy wyruszają, a cztery mogą sprawę tylko rozrzedzić, bo przy 100 boty już biorą ją przy każdej okazji. Ucieczka z przegranej walki, wybór profesji i zdobycie broni nigdy nie podlegają tym suwakom — to nie są preferencje.",
                 "de":"Die Einstellungen unten wirken live: der Spielkern liest sie alle fünf Sekunden. Unter jeder steht, was sie bewegt und wie schnell - manche ändern, wie viele Bots eine Besorgung übernehmen, manche, wann sie aufbrechen, und vier können eine Besorgung nur seltener machen, weil die Bots sie bei 100 schon bei jeder Gelegenheit erledigen. Überleben, Berufswahl und Waffensuche bleiben unberührt — das sind keine Vorlieben.",
                 "tr":"Aşağıdaki ayarlar canlıdır: oyun çekirdeği onları beş saniyede bir okur. Her birinin altında neyi ve ne kadar çabuk değiştirdiği yazar - bazıları kaç botun bir işi üstlendiğini, bazıları ne zaman yola çıktıklarını değiştirir, dördü ise bir işi yalnızca seyrekleştirebilir, çünkü 100'de botlar onu zaten her fırsatta yapar. Hayatta kalma, meslek seçimi ve silah bulma bunlardan etkilenmez — onlar tercih değildir."},
 "ai_chat":      {"en":"Bots talk over their heads","pl":"Boty piszą nad głową, co robią",
                  "de":"Bots reden über ihren Köpfen","tr":"Botlar başlarının üstünde konuşur"},
 "ai_chat_help": {"en":"The line over a bot's head (hunting, off to the blacksmith, fishing). Off for players who call it spam. The shout on the world channel about a +7/+8/+9 refine stays either way.",
                  "pl":"Napis nad głową bota (poluje, idzie do kowala, łowi). Wyłącz dla graczy, którym to przeszkadza. Wołanie na czacie świata o ulepszeniu na +7/+8/+9 zostaje niezależnie od tego.",
                  "de":"Die Zeile über dem Kopf eines Bots (jagt, geht zum Schmied, angelt). Aus für Spieler, die es Spam nennen. Der Ruf im Weltkanal über ein +7/+8/+9-Upgrade bleibt so oder so.",
                  "tr":"Botun başının üstündeki satır (avlanıyor, demirciye gidiyor, balık tutuyor). Spam diyen oyuncular için kapatın. Dünya kanalındaki +7/+8/+9 bağırışı her halükârda kalır."},
 "ai_chat_on":   {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_books":     {"en":"Skill books without the day's wait","pl":"Księgi umiejętności bez dobowej przerwy","de":"Fertigkeitsbücher ohne Tageswartezeit","tr":"Günlük bekleme olmadan beceri kitapları"},
 "ai_books_help":{"en":"The game makes a character wait about a day between two reads of the same skill, so a bot needs a month of books to take a skill from M1 to G1 and the books pile up in its bag meanwhile. On, a bot reads a book the moment it has one; the only pace left is the game's own (20 000 experience and a roll per read). Off keeps the game's daily wait.",
                  "pl":"Gra każe czekać około doby między dwoma czytaniami tej samej umiejętności, więc bot potrzebuje miesiąca, by przeczytać skill z M1 na G1, a księgi tymczasem zalegają w plecaku. Włączone: bot czyta księgę od razu, gdy ją ma — jedyny hamulec to sama gra (20 000 doświadczenia i rzut przy każdej lekturze). Wyłączone: dobowa przerwa gry bez zmian.",
                  "de":"Das Spiel lässt zwischen zwei Lesungen derselben Fertigkeit etwa einen Tag warten, also braucht ein Bot einen Monat, um eine Fertigkeit von M1 auf G1 zu lesen, und die Bücher stapeln sich derweil. An: der Bot liest ein Buch, sobald er eines hat; nur das Spiel selbst bremst (20 000 Erfahrung und ein Wurf pro Lesung). Aus: die tägliche Wartezeit des Spiels.",
                  "tr":"Oyun aynı becerinin iki okuması arasında yaklaşık bir gün bekletir; bot bir beceriyi M1'den G1'e çıkarmak için bir ay kitap okur ve kitaplar bu arada çantada birikir. Açık: bot yarım saat sonra tekrar okur, Ayin Parşömeni kullanan bir oyuncu gibi. Kapalı: oyunun kendi temposu."},
 "ai_books_on":  {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_night":     {"en":"Night on the server","pl":"Noc na serwerze","de":"Nacht auf dem Server","tr":"Sunucuda gece"},
 "ai_night_help":{"en":"Between 22:00 and 05:59 of the server's clock (M2_TZ) the core raises the night flag - the same one a GM sets with /xmas_snow 1 - and lowers it in the morning. The client shows the night sky and, as the flag is the Christmas one, snow.",
                  "pl":"Między 22:00 a 05:59 czasu serwera (M2_TZ) rdzeń podnosi flagę nocy - tę samą, którą GM ustawia komendą /xmas_snow 1 - a rano ją opuszcza. Klient pokazuje nocne niebo i, bo to flaga świąteczna, śnieg.",
                  "de":"Zwischen 22:00 und 05:59 Serverzeit (M2_TZ) setzt der Kern die Nacht-Flagge - dieselbe, die ein GM mit /xmas_snow 1 setzt - und nimmt sie morgens zurück. Der Client zeigt den Nachthimmel und, weil es die Weihnachtsflagge ist, Schnee.",
                  "tr":"Sunucu saatine göre (M2_TZ) 22:00-05:59 arasında çekirdek gece bayrağını kaldırır - GM'in /xmas_snow 1 ile ayarladığı bayrağın aynısı - ve sabah indirir. İstemci gece gökyüzünü ve, bayrak Noel bayrağı olduğu için, kar gösterir."},
 "ai_night_on":  {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_life":      {"en":"Bots play like people","pl":"Boty grają jak żywi ludzie","de":"Bots spielen wie Menschen","tr":"Botlar insan gibi oynar"},
 "ai_life_help": {"en":"Experimental. Each bot plays a session of 3-6 hours (the first after a start from half an hour up), logs out, rests 3-9 hours and comes back; at most two bots in five rest at once, so at least three in five are online. Off, every bot stays in the world as before. Applies within a minute; switching it off brings the resting bots back within a few minutes. A bot in a player's party waits before logging out.",
                  "pl":"Eksperymentalne. Każdy bot gra sesję 3–6 godzin (pierwszą po starcie serwera od pół godziny wzwyż), wylogowuje się, odpoczywa 3–9 godzin i wraca; naraz odpoczywają najwyżej dwa boty na pięć, więc online są co najmniej trzy na pięć. Wyłączone: wszystkie boty są w świecie jak dotąd. Działa w ciągu minuty; wyłączenie sprowadza odpoczywające boty z powrotem w kilka minut. Bot w drużynie gracza czeka z wylogowaniem.",
                  "de":"Experimentell. Jeder Bot spielt eine Sitzung von 3-6 Stunden (die erste nach einem Start ab einer halben Stunde), loggt sich aus, ruht 3-9 Stunden und kommt zurück; höchstens zwei von fünf Bots ruhen gleichzeitig, also sind mindestens drei von fünf online. Aus: alle Bots bleiben wie bisher in der Welt. Greift innerhalb einer Minute; Ausschalten holt die ruhenden Bots in wenigen Minuten zurück. Ein Bot in der Gruppe eines Spielers wartet mit dem Ausloggen.",
                  "tr":"Deneysel. Her bot 3-6 saatlik bir oturum oynar (başlangıçtan sonraki ilki yarım saatten itibaren), çıkış yapar, 3-9 saat dinlenir ve geri gelir; aynı anda en fazla beşte iki bot dinlenir, yani en az beşte üçü çevrimiçidir. Kapalıyken tüm botlar eskisi gibi dünyada kalır. Bir dakika içinde uygulanır; kapatmak dinlenen botları birkaç dakika içinde geri getirir. Bir oyuncunun grubundaki bot çıkış yapmadan bekler."},
 "ai_life_on":   {"en":"Enabled (experimental)","pl":"Włączone (eksperymentalne)","de":"Eingeschaltet (experimentell)","tr":"Açık (deneysel)"},
 "ai_life_hours": {"en":"Hours of play a day","pl":"Godziny gry na dobę","de":"Spielstunden pro Tag","tr":"Günlük oyun saati"},
 "ai_life_hours_default": {"en":"as before","pl":"jak dotąd","de":"wie bisher","tr":"eskisi gibi"},
 "ai_life_hours_all_day": {"en":"all day, no rests","pl":"cała doba, bez przerw","de":"den ganzen Tag, ohne Pausen","tr":"bütün gün, molasız"},
 "ai_life_hours_help": {"en":"Works while the switch above is on. How many hours a day each bot plays: sessions of 4 hours (the whole of a shorter day) with a rest between them that makes the day add up - 12 is about three sessions of 4 hours with 4 hours off, 2 is one session of 2 hours a day. Each session and rest is drawn a quarter either way, and at most the day's share of resting bots plus a tenth rest at once. 0 keeps the sessions of 3-6 hours and rests of 3-9 hours.",
                  "pl":"Działa, gdy przełącznik wyżej jest włączony. Ile godzin na dobę gra każdy bot: sesje po 4 godziny (przy krótszej dobie cała doba w jednej sesji) z przerwą, która dopełnia dobę - 12 to około trzech sesji po 4 godziny z 4 godzinami przerwy, 2 to jedna dwugodzinna sesja na dobę. Każda sesja i przerwa losuje się o ćwierć w górę lub w dół, a naraz odpoczywa najwyżej tyle botów, ile wynika z doby, i jeszcze jedna dziesiąta. 0 zostawia sesje 3–6 godzin i przerwy 3–9 godzin.",
                  "de":"Wirkt, solange der Schalter oben an ist. Wie viele Stunden am Tag jeder Bot spielt: Sitzungen von 4 Stunden (bei kürzerem Tag der ganze Tag in einer) mit einer Pause dazwischen, die den Tag auffüllt - 12 sind etwa drei Sitzungen von 4 Stunden mit 4 Stunden Pause, 2 eine zweistündige Sitzung am Tag. Jede Sitzung und Pause wird um ein Viertel nach oben oder unten gezogen, und gleichzeitig ruhen höchstens so viele Bots, wie der Tag vorgibt, plus ein Zehntel. 0 behält Sitzungen von 3-6 Stunden und Pausen von 3-9 Stunden.",
                  "tr":"Yukarıdaki anahtar açıkken çalışır. Her botun günde kaç saat oynadığı: 4 saatlik oturumlar (daha kısa bir günde günün tamamı tek oturum) ve günü tamamlayan molalar - 12, 4 saat molayla yaklaşık üç adet 4 saatlik oturumdur, 2 günde tek bir 2 saatlik oturumdur. Her oturum ve mola dörtte bir yukarı veya aşağı çekilir ve aynı anda en fazla günün payı kadar bot, bir de onda biri dinlenir. 0, 3-6 saatlik oturumları ve 3-9 saatlik molaları korur."},
 "ai_wars":      {"en":"Guild wars between the bots","pl":"Wojny gildii botów","de":"Gildenkriege der Bots","tr":"Botların lonca savaşları"},
 "ai_wars_help": {"en":"Two bot guilds of one kingdom fight a field war on that kingdom's guild map, for as long and as often as set below (thirty minutes every two hours by default), with the game's own declaration and scoring and a notice on the chat when it starts. A guild needs eight bots online to be picked. Off: no new war is declared, and the bots of a war under way leave it within a minute.",
                  "pl":"Dwie gildie botów z jednego królestwa toczą wojnę polową na mapie gildyjnej tego królestwa, tak długo i tak często, jak ustawiono niżej (domyślnie trzydzieści minut co dwie godziny), z wypowiedzeniem i punktacją gry i komunikatem na czacie przy starcie. Gildia musi mieć osiem botów online, żeby ją wylosowano. Wyłączone: nowa wojna nie jest wypowiadana, a boty z trwającej wojny wycofują się w ciągu minuty.",
                  "de":"Zwei Bot-Gilden eines Königreichs führen einen Feldkrieg auf der Gildenkarte dieses Königreichs, so lange und so oft wie unten eingestellt (standardmäßig dreißig Minuten alle zwei Stunden), mit Kriegserklärung und Wertung des Spiels und einer Meldung im Chat beim Start. Eine Gilde braucht acht Bots online, um gewählt zu werden. Aus: kein neuer Krieg wird erklärt, und die Bots eines laufenden Krieges ziehen sich binnen einer Minute zurück.",
                  "tr":"Aynı krallıktan iki bot loncası, o krallığın lonca haritasında aşağıda ayarlanan süre ve sıklıkta (varsayılan olarak iki saatte bir otuz dakika), oyunun kendi ilanı ve puanlamasıyla bir saha savaşı yapar; başlangıçta sohbette bir duyuru çıkar. Bir loncanın seçilmesi için sekiz botu çevrimiçi olmalı. Kapalı: yeni savaş ilan edilmez ve süren savaşın botları bir dakika içinde çekilir."},
 "ai_wars_on":   {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_war_minutes": {"en":"A war lasts","pl":"Wojna trwa","de":"Ein Krieg dauert","tr":"Bir savaş sürer"},
 "ai_war_hours":   {"en":"A war in each kingdom every","pl":"Wojna w każdym królestwie co","de":"Ein Krieg in jedem Königreich alle","tr":"Her krallıkta bir savaş, her"},
 "ai_war_min_unit": {"en":"minutes","pl":"minut","de":"Minuten","tr":"dakika"},
 "ai_war_hour_unit": {"en":"h","pl":"h","de":"Std.","tr":"saat"},
 "ai_war_kills": {"en":"A war is won at","pl":"Wojnę wygrywa","de":"Ein Krieg ist gewonnen bei","tr":"Savaşı kazandıran"},
 "ai_war_kills_unit": {"en":"kills (0: by the clock alone)","pl":"zabójstw (0: tylko czas)","de":"Kills (0: nur nach Zeit)","tr":"öldürme (0: yalnızca süre)"},
 "ai_war_len_help": {"en":"A war with a bot guild on a side is scored by its kills, one a kill. The first guild to the kills set here wins at once; otherwise at the end of the time the guild with more kills wins (a draw on a tie). The war's board in the game shows the target, the kills each side still needs and who leads. At 15 minutes the bots end their war early; a player's guild at war with the bots always has the game's thirty.",
                     "pl":"Wojna z gildią botów po jednej ze stron jest liczona w zabójstwach, jedno za każde zabicie. Gildia, która pierwsza zabije ustawioną liczbę wrogów, wygrywa od razu; inaczej po upływie czasu wygrywa ta z większą liczbą zabójstw (remis przy równej). Tablica wojny w grze pokazuje cel, ile zabójstw brakuje każdej stronie i kto prowadzi. Przy 15 minutach boty kończą swoją wojnę wcześniej; gildia gracza w wojnie z botami zawsze ma pełne trzydzieści minut gry.",
                     "de":"Ein Krieg mit einer Bot-Gilde auf einer Seite zählt Kills, einen pro Kill. Die Gilde, die zuerst die hier eingestellten Kills erreicht, gewinnt sofort; sonst gewinnt am Ende der Zeit die Gilde mit mehr Kills (Unentschieden bei Gleichstand). Die Kriegstafel im Spiel zeigt das Ziel, die fehlenden Kills jeder Seite und wer führt. Bei 15 Minuten beenden die Bots ihren Krieg früher; eine Spielergilde im Krieg mit den Bots hat immer die dreißig Minuten des Spiels.",
                     "tr":"Bir tarafında bot loncası olan savaş öldürmelerle sayılır, her öldürme bir puan. Burada ayarlanan öldürme sayısına ilk ulaşan lonca hemen kazanır; aksi halde süre sonunda daha çok öldüren lonca kazanır (eşitlikte beraberlik). Oyundaki savaş tablosu hedefi, her tarafın eksik öldürmelerini ve kimin önde olduğunu gösterir. 15 dakikada botlar savaşlarını erken bitirir; botlarla savaşan bir oyuncu loncası her zaman oyunun otuz dakikasına sahiptir."},
 "ai_tower":     {"en":"Bot guilds climb the Demon Tower","pl":"Gildie botów chodzą do Wieży Demonów","de":"Bot-Gilden steigen in den Dämonenturm","tr":"Bot loncaları Şeytan Kulesi'ne çıkar"},
 "ai_tower_help": {"en":"About every hour and a half one bot guild of this core calls its members of level 55 and up to the tower's ground floor (the game says it on the chat), they break the Metin of Toughness together after four minutes and climb the floors: monsters, stones, keys and seals as in the game; from the sixth floor on a bot of 75 is needed, as for players. Whoever stands on the ground floor when the stone breaks - a bot on its errand, a player who came to watch - goes in with them. 'Now' calls a raid on the core's next check when none is under way.",
                  "pl":"Mniej więcej co półtorej godziny jedna gildia botów tego rdzenia zwołuje członków od 55. poziomu na parter Wieży (ogłoszenie na czacie), po czterech minutach razem rozbijają Metin Twardości i przechodzą piętra: potwory, kamienie, klucze i pieczęcie jak w grze; od 6. piętra potrzebny jest bot z 75. poziomem, tak jak u graczy. Kto stoi na parterze, gdy pęka kamień — bot na własnej misji albo gracz, który przyszedł popatrzeć — wchodzi razem z nimi. „Teraz” zwołuje wyprawę przy najbliższym sprawdzeniu rdzenia, jeśli żadna nie trwa.",
                  "de":"Etwa alle anderthalb Stunden ruft eine Bot-Gilde dieses Kerns ihre Mitglieder ab Stufe 55 ins Erdgeschoss des Turms (Ansage im Chat), nach vier Minuten zerschlagen sie gemeinsam den Metin der Härte und steigen die Etagen hinauf: Monster, Steine, Schlüssel und Siegel wie im Spiel; ab der sechsten Etage wird ein Bot mit Stufe 75 gebraucht, wie bei Spielern. Wer beim Zerbrechen des Steins im Erdgeschoss steht - ein Bot auf seinem Botengang, ein zuschauender Spieler - geht mit hinein. 'Jetzt' ruft beim nächsten Check des Kerns eine Expedition, wenn keine läuft.",
                  "tr":"Yaklaşık her bir buçuk saatte bu çekirdeğin bir bot loncası 55 ve üzeri üyelerini kulenin zemin katına çağırır (sohbette duyurulur), dört dakika sonra Sertlik Metini'ni birlikte kırar ve katları çıkarlar: canavarlar, taşlar, anahtarlar ve mühürler oyundaki gibi; 6. kattan itibaren oyuncularda olduğu gibi 75 seviye bir bot gerekir. Taş kırıldığında zemin katta duran herkes - görevindeki bir bot, izlemeye gelen bir oyuncu - onlarla girer. 'Şimdi', hiçbiri sürmüyorsa çekirdeğin bir sonraki kontrolünde bir sefer çağırır."},
 "ai_tower_on":  {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_catacomb":  {"en":"Bot raids on Azrael (the Devil's Catacomb)","pl":"Rajdy botów na Azraela (Katakumby Diabła)","de":"Bot-Raids auf Azrael (Katakomben des Teufels)","tr":"Azrael'e bot baskınları (Şeytan Yeraltı Mezarı)"},
 "ai_catacomb_help": {"en":"About every two hours a party of four to eight bots of one kingdom - level 80 and up, the Demon Tower's ninth floor done and a Dried Head in the bag, as the dungeon asks of a player - gathers at the Catacomb's Guardian in Hwang Temple (the game says it on the chat), hunts the key on the first floor and goes down the floors as in the game: the gates, the Metins of Revenge, the maze, Tartar, Charon and Azrael. With a player inside, the player clicks and the bots fight. 'Now' calls a raid on the core's next check (within a minute) if none is under way and there are bots who may go.",
                  "pl":"Mniej więcej co dwie godziny drużyna od czterech do ośmiu botów jednego królestwa - od 80. poziomu, z przejściem 9. piętra Wieży Demonów i Zasuszoną Głową w plecaku, jak loch wymaga od gracza - zbiera się przy Strażniku Katakumb w Świątyni Hwang (ogłoszenie na czacie), na pierwszym piętrze zdobywa klucz i schodzi piętrami jak w grze: wrota, Metiny Zemsty, labirynt, Tartar, Charon i Azrael. Gdy w środku jest gracz, to on klika, a boty walczą. „Teraz” zwołuje rajd przy najbliższym sprawdzeniu rdzenia (do minuty), jeśli żaden nie trwa i są boty, które mogą iść.",
                  "de":"Etwa alle zwei Stunden sammelt sich eine Gruppe von vier bis acht Bots eines Königreichs - ab Stufe 80, mit geschaffter neunter Etage des Dämonenturms und einem Getrockneten Kopf im Inventar, wie der Dungeon es von Spielern verlangt - beim Wächter der Katakomben im Hwang-Tempel (Ansage im Chat), holt im ersten Stock den Schlüssel und steigt die Etagen hinab wie im Spiel: Tore, Metine der Rache, Labyrinth, Tartar, Charon und Azrael. Ist ein Spieler drin, klickt der Spieler und die Bots kämpfen. 'Jetzt' ruft beim nächsten Check des Kerns (binnen einer Minute) einen Raid, wenn keiner läuft und Bots gehen dürfen.",
                  "tr":"Yaklaşık iki saatte bir, bir krallığın dört ila sekiz botundan oluşan bir grup - 80 ve üzeri seviye, Şeytan Kulesi'nin 9. katını bitirmiş ve çantasında Kurutulmuş Kafa olan, zindanın oyuncudan istediği gibi - Hwang Tapınağı'ndaki Yeraltı Mezarı Muhafızı'nda toplanır (sohbette duyurulur), birinci katta anahtarı bulur ve oyundaki gibi katları iner: kapılar, İntikam Metinleri, labirent, Tartar, Charon ve Azrael. İçeride bir oyuncu varsa tıklayan oyuncudur, botlar savaşır. 'Şimdi', hiçbiri sürmüyorsa ve gidebilecek botlar varsa çekirdeğin bir sonraki kontrolünde (bir dakika içinde) bir baskın çağırır."},
 "rates_reprice_title": {"en":"Bots' shop prices","pl":"Ceny w sklepach botów","de":"Preise in den Läden der Bots","tr":"Botların dükkân fiyatları"},
 "rates_reprice_help": {"en":"After a change of the rates or an update the bots reprice their counters slowly, on their own visits. This makes every bot reprice all its listed lines to the current price rules right away: the core edits the prices in place, a few bots a second, and leaves lines whose price is already within 1%. It also runs by itself a few minutes after an update and a minute after the yang rate changes.","pl":"Po zmianie rat lub aktualizacji boty przeliczają swoje sklepy powoli, przy własnych wizytach. Ten przycisk każe każdemu botowi od razu przeliczyć wszystkie wystawione przedmioty według obecnych zasad cen: rdzeń zmienia ceny na miejscu, kilka botów na sekundę, i pomija linie, których cena różni się o mniej niż 1%. Uruchamia się też sam kilka minut po aktualizacji i minutę po zmianie raty yang.","de":"Nach einer Änderung der Raten oder einem Update passen die Bots ihre Preise langsam an, bei ihren eigenen Besuchen. Damit berechnet jeder Bot sofort alle ausgestellten Posten nach den aktuellen Preisregeln neu: der Kern ändert die Preise an Ort und Stelle, einige Bots pro Sekunde, und lässt Posten aus, deren Preis schon innerhalb von 1% liegt. Läuft auch von selbst einige Minuten nach einem Update und eine Minute nach einer Änderung der Yang-Rate.","tr":"Oranlar değiştikten veya bir güncellemeden sonra botlar dükkânlarını kendi ziyaretlerinde yavaşça yeniden fiyatlar. Bu düğme her botun listelediği tüm ürünleri şimdiki fiyat kurallarına göre hemen yeniden fiyatlamasını sağlar: çekirdek fiyatları yerinde değiştirir, saniyede birkaç bot, ve fiyatı zaten %1 içinde olanları atlar. Bir güncellemeden birkaç dakika ve yang oranı değiştikten bir dakika sonra kendiliğinden de çalışır."},
 "rates_reprice_now": {"en":"Reprice the bots' shops now","pl":"Przelicz ceny w sklepach botów teraz","de":"Preise in den Läden der Bots jetzt neu berechnen","tr":"Botların dükkân fiyatlarını şimdi yeniden hesapla"},
 "rates_reprice_done": {"en":"Requested: every core starts repricing its bots' shops within a few seconds; the game log says PLAYERBOT_REPRICE_NOW: done with the bots, the lines changed and the time it took.","pl":"Zlecone: każdy rdzeń w ciągu kilku sekund zaczyna przeliczać sklepy swoich botów; w logu gry PLAYERBOT_REPRICE_NOW: done podaje boty, zmienione linie i czas.","de":"Angefordert: jeder Kern beginnt binnen weniger Sekunden, die Läden seiner Bots neu zu bepreisen; das Spiellog meldet PLAYERBOT_REPRICE_NOW: done mit Bots, geänderten Posten und Dauer.","tr":"İstendi: her çekirdek birkaç saniye içinde botlarının dükkânlarını yeniden fiyatlamaya başlar; oyun günlüğü PLAYERBOT_REPRICE_NOW: done ile botları, değişen satırları ve süreyi yazar."},
 "ai_catacomb_now": {"en":"Call a raid on Azrael now","pl":"Rajd na Azraela teraz","de":"Jetzt einen Azrael-Raid rufen","tr":"Şimdi bir Azrael baskını çağır"},
 "ai_catacomb_now_done": {"en":"Requested: the core calls a raid on its next check (within a minute) if none is under way and there are bots who may go.","pl":"Zlecone: rdzeń zwoła rajd przy najbliższym sprawdzeniu (do minuty), jeśli żaden nie trwa i są boty, które mogą iść.","de":"Angefordert: der Kern ruft beim nächsten Check (binnen einer Minute) einen Raid, wenn keiner läuft und Bots gehen dürfen.","tr":"İstendi: hiçbiri sürmüyorsa ve gidebilecek botlar varsa çekirdek bir sonraki kontrolde (bir dakika içinde) bir baskın çağırır."},
 "ai_bots_held_title": {"pl":"Boty czekają przy drzwiach","en":"The bots are waiting at the door",
  "de":"Die Bots warten an der Tür","tr":"Botlar kapıda bekliyor"},
 "ai_bots_held_help": {"pl":"Ten świat powstał przed chwilą i nie ma w nim jeszcze ani jednego bota - tak, jak poprosiłeś przy zakładaniu. Ustaw teraz spokojnie stawki, respawny i zachowanie botów, a potem wpuść je. Wejdą stopniowo, tak jak po zwykłym starcie.",
  "en":"This world was made a moment ago and has not one bot in it yet - as you asked when you made it. Set the rates, the respawns and the bots' behaviour in peace, then let them in. They walk in gradually, as after any start.",
  "de":"Diese Welt wurde gerade erstellt und hat noch keinen einzigen Bot - so wie gewünscht. Stelle in Ruhe Raten, Respawns und Bot-Verhalten ein und lasse sie dann herein.",
  "tr":"Bu dünya az önce kuruldu ve içinde henüz tek bir bot yok. Oranları, respawn sürelerini ve bot davranışını rahatça ayarla, sonra onları içeri al."},
 "ai_bots_release": {"pl":"Wpuść boty do świata","en":"Let the bots in",
  "de":"Bots hereinlassen","tr":"Botları içeri al"},
 "ai_bots_released": {"pl":"✅ Boty wchodzą do świata - pojawią się w ciągu kilku minut, tak jak po zwykłym starcie.",
  "en":"✅ The bots are coming in - they will appear over the next few minutes, as after any start.",
  "de":"✅ Die Bots kommen herein - sie erscheinen in den nächsten Minuten.",
  "tr":"✅ Botlar geliyor - birkaç dakika içinde belirecekler."},
 "ai_tower_now": {"en":"Call a Demon Tower raid now","pl":"Wyprawa do Wieży Demonów teraz","de":"Jetzt eine Turm-Expedition rufen","tr":"Şimdi bir Kule seferi çağır"},
 "ai_tower_now_done": {"en":"Requested: the core calls a raid on its next check (within a minute) if none is under way.","pl":"Zlecone: rdzeń zwoła wyprawę przy najbliższym sprawdzeniu (do minuty), jeśli żadna nie trwa.","de":"Angefordert: der Kern ruft beim nächsten Check (binnen einer Minute) eine Expedition, wenn keine läuft.","tr":"İstendi: hiçbiri sürmüyorsa çekirdek bir sonraki kontrolde (bir dakika içinde) bir sefer çağırır."},
 "gl_tower":     {"en":"in the Demon Tower","pl":"w Wieży Demonów","de":"im Dämonenturm","tr":"Şeytan Kulesi'nde"},
 "ai_ishop":     {"en":"Bots buy in the ItemShop","pl":"Boty kupują w ItemShopie","de":"Bots kaufen im ItemShop","tr":"Botlar ItemShop'tan alır"},
 "ai_ishop_help":{"en":"A bot cashes the Kupon SM vouchers it finds (Metin stones and bosses drop them, M2_DRAGON_COIN_*_PERMILLE) into its account's Dragon Coins and buys, at most once an hour, only what its own rules would use: a Kamień Duchowy for a Grand Master skill, a change stone for a worn weapon still worth rerolling, with Dragon Marks a Blessing Scroll or the Dragon God's attack potions, and one bot in four a hairstyle, once. No VIP items and no pass: every bot already holds the premium subscription. Off: the vouchers stay in the bags.",
                  "pl":"Bot wymienia znalezione Kupony SM (dropią z metinów i bossów, M2_DRAGON_COIN_*_PERMILLE) na Smocze Monety swojego konta i kupuje, najwyżej raz na godzinę, tylko to, z czego jego własne reguły korzystają: Kamień Duchowy do umiejętności Wielkiego Mistrza, kamień zmiany bonusów do noszonej broni wartej jeszcze losowania, za Smocze Znaki Zwój Błogosławieństwa albo mikstury ataku Boga Smoków, a jeden bot na czterech fryzurę, raz. Bez przedmiotów VIP i bez przepustki: każdy bot ma już subskrypcję premium. Wyłączone: kupony zostają w torbach.",
                  "de":"Ein Bot löst die gefundenen Kupon-SM-Gutscheine (Metinsteine und Bosse lassen sie fallen, M2_DRAGON_COIN_*_PERMILLE) in Drachenmünzen seines Kontos ein und kauft höchstens einmal pro Stunde nur, was seine eigenen Regeln nutzen: einen Kamień Duchowy für eine Großmeister-Fertigkeit, einen Bonus-Wechselstein für die getragene Waffe, wenn sie noch neu gewürfelt würde, mit Drachenmarken eine Segensrolle oder die Angriffstränke des Drachengottes, und jeder vierte Bot einmal eine Frisur. Keine VIP-Gegenstände und kein Pass: jeder Bot hat das Premium-Abo bereits. Aus: die Gutscheine bleiben im Inventar.",
                  "tr":"Bot bulduğu Kupon SM kuponlarını (Metin taşları ve boss'lar düşürür, M2_DRAGON_COIN_*_PERMILLE) hesabının Ejderha Parasına çevirir ve saatte en fazla bir kez, yalnızca kendi kurallarının kullanacağı şeyi alır: Büyük Usta becerisi için Kamień Duchowy, hâlâ yeniden atılmaya değer takılı silah için bonus değiştirme taşı, Ejderha İşaretleriyle Kutsama Parşömeni ya da Ejderha Tanrısı saldırı iksirleri ve dört bottan biri bir kez bir saç modeli. VIP eşya ve geçiş kartı yok: her bot zaten premium aboneliğe sahip. Kapalı: kuponlar çantada kalır."},
 "ai_ishop_on":  {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_haggle":    {"en":"Bots haggle at people's shops","pl":"Boty targują się na sklepach graczy","de":"Bots feilschen an den Läden der Spieler","tr":"Botlar oyuncu dükkânlarında pazarlık yapar"},
 "ai_haggle_help":{"en":"A bot that wants a finished piece of gear (+6 and up) on a person's offline shop, but finds it dearer than it would pay (the bots' cap on a person's price included: at most 1.5 to 2 times the market's price for the piece), whispers the owner - once, with the piece linked - its offer, and goes up to its last price, the most it would pay, if the owner asks more. The owner answers \"ok\", \"nie\"/\"no\" or a price of his own (\"5kk\"), or simply lowers the price on the shop; once the piece asks the price agreed (or less), the bot comes and buys it, within ten minutes. Only an owner online on the bot's core is asked, one haggle at a time with a person, three offers an hour to one person, one offer about the same piece in three hours (twelve after a no), three haggles at once on a core. What a bot buys at once is unchanged. Off: no bot whispers an offer, and every deal is forgotten.",
                  "pl":"Bot, który chce gotowy przedmiot (od +6) ze sklepu offline gracza, ale uznaje go za droższy niż zapłaciłby (z limitem botów na cenę gracza: najwyżej 1,5-2 razy cena rynkowa przedmiotu), pisze do właściciela szeptem - raz, z podlinkowanym przedmiotem - swoją ofertę, a gdy właściciel chce więcej, podnosi do ostatniej ceny, najwyższej, jaką zapłaci. Właściciel odpisuje „ok”, „nie” albo własną cenę („5kk”) lub po prostu obniża cenę na sklepie; gdy przedmiot kosztuje tyle, ile uzgodniono (albo mniej), bot przychodzi i kupuje - w ciągu dziesięciu minut. Pyta tylko właściciela zalogowanego na rdzeniu bota; najwyżej jeden targ naraz z jedną osobą, trzy oferty na godzinę do jednej osoby, jedna oferta o ten sam przedmiot na trzy godziny (dwanaście po „nie”), trzy targi naraz na rdzeń. To, co bot kupuje od razu, się nie zmienia. Wyłączone: żaden bot nie proponuje ceny, a zawarte umowy przepadają.",
                  "de":"Ein Bot, der ein fertiges Ausrüstungsteil (ab +6) im Offline-Laden eines Spielers will, es aber teurer findet, als er zahlen würde (mit der Obergrenze der Bots für den Preis eines Spielers: höchstens das 1,5- bis 2-Fache des Marktpreises), flüstert dem Besitzer - einmal, mit verlinktem Gegenstand - sein Angebot und geht bis zu seinem letzten Preis, dem Höchsten, das er zahlt, wenn der Besitzer mehr will. Der Besitzer antwortet \"ok\", \"no\" oder mit einem eigenen Preis (\"5kk\") oder senkt einfach den Preis im Laden; sobald der Gegenstand den vereinbarten Preis (oder weniger) kostet, kommt der Bot und kauft ihn, innerhalb von zehn Minuten. Gefragt wird nur ein Besitzer, der auf dem Kern des Bots online ist; höchstens ein Feilschen gleichzeitig mit einer Person, drei Angebote pro Stunde an eine Person, ein Angebot zum selben Gegenstand in drei Stunden (zwölf nach einem Nein), drei gleichzeitig auf einem Kern. Was ein Bot sofort kauft, bleibt unverändert. Aus: kein Bot flüstert ein Angebot, und alle Abmachungen werden vergessen.",
                  "tr":"Bir oyuncunun çevrimdışı dükkânında bitmiş bir ekipman (+6 ve üstü) isteyen ama onu ödeyeceğinden pahalı bulan bot (botların oyuncu fiyatı sınırı dahil: eşyanın piyasa fiyatının en fazla 1,5-2 katı), sahibine bir kez, eşyanın bağlantısıyla teklifini fısıldar ve sahip daha fazlasını isterse son fiyatına, ödeyeceği en yüksek fiyata çıkar. Sahip \"ok\", \"no\" ya da kendi fiyatıyla (\"5kk\") cevap verir veya dükkândaki fiyatı düşürür; eşya anlaşılan fiyata (ya da daha azına) indiğinde bot gelip on dakika içinde satın alır. Yalnızca botun çekirdeğinde çevrimiçi olan sahibe sorulur; bir kişiyle aynı anda en fazla bir pazarlık, bir kişiye saatte üç teklif, aynı eşya için üç saatte bir teklif (bir hayırdan sonra on iki), bir çekirdekte aynı anda üç pazarlık. Botun hemen satın aldığı şeyler değişmez. Kapalı: hiçbir bot teklif fısıldamaz ve tüm anlaşmalar unutulur."},
 "ai_haggle_on": {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_shop_m2":   {"en":"Bots open their offline shops in the second villages (M2) too","pl":"Boty wystawiają sklepy offline również w M2","de":"Bots stellen ihre Offline-Läden auch in den zweiten Dörfern (M2) auf","tr":"Botlar çevrimdışı tezgahlarını ikinci köylerde (M2) de açar"},
 "ai_shop_m2_help": {"en":"Only about where the bots' offline shops (stands) stand - not buying from NPC merchants. Off (the default): a bot opens its stand only in the first villages, where the players shop; an expired stand that stood in a second village is renewed on the first village's market ring at its owner's next service visit. On: the stands stand in both villages, as before.",
                  "pl":"Dotyczy tylko miejsca, w którym boty wystawiają swoje sklepy offline (stragany) - nie zakupów u sprzedawców NPC. Wyłączone (domyślnie): bot otwiera sklep tylko w pierwszych wioskach, tam gdzie kupują gracze; sklep, który stał w drugiej wiosce, po wygaśnięciu zostaje odnowiony na rynku pierwszej wioski przy najbliższej wizycie właściciela. Włączone: sklepy stoją w obu wioskach, jak wcześniej.",
                  "de":"Betrifft nur, wo die Bots ihre Offline-Läden (Stände) aufstellen - nicht das Einkaufen bei NPC-Händlern. Aus (Standard): ein Bot eröffnet seinen Stand nur in den ersten Dörfern, wo die Spieler einkaufen; ein abgelaufener Stand aus einem zweiten Dorf wird beim nächsten Besuch seines Besitzers auf dem Marktring des ersten Dorfes erneuert. An: die Stände stehen in beiden Dörfern, wie bisher.",
                  "tr":"Yalnızca botların çevrimdışı tezgahlarını nereye kurduğuyla ilgilidir - NPC tüccarlardan alışverişle değil. Kapalı (varsayılan): bot tezgahını yalnızca oyuncuların alışveriş yaptığı ilk köylerde açar; ikinci köyde duran ve süresi dolan tezgah, sahibinin bir sonraki ziyaretinde ilk köyün pazar halkasında yenilenir. Açık: tezgahlar eskisi gibi iki köyde de durur."},
 "ai_shop_m2_on": {"en":"Allowed","pl":"Dozwolone","de":"Erlaubt","tr":"İzin verildi"},
 "ai_shouters":  {"en":"Shouters (6 bots)","pl":"Krzykacze (6 botów)","de":"Schreier (6 Bots)","tr":"Bağıranlar (6 bot)"},
 "ai_shouters_help": {"en":"Three extra bots, one per kingdom (apka2009, appka2009, apppka2009), on top of the bot count. They level up to 15, then stand without weapon and armour by the general store of their kingdom's first village and now and then shout a line on the shout channel (once per 20-30 channel lines, at least 10 minutes apart). They answer nothing: no whispers, parties, trades or guilds. Three more, Tieru, Tiieru and Tiiieru, one per kingdom, are medal droppers: they farm the Monkey Dungeon with their experience locked like the other medal droppers, sell medals at their counter and stall, and shout their own lines about medals on the same rules. Off: all six log out at once and do not come back until switched on again.",
                  "pl":"Trzy dodatkowe boty, po jednym na królestwo (apka2009, appka2009, apppka2009), ponad liczbę botów. Wbijają 15 poziom, potem stoją bez broni i zbroi przy Handlarce w pierwszej wiosce swojego królestwa i co jakiś czas wołają na kanale wołaj (raz na 20–30 wiadomości kanału, nie częściej niż co 10 minut). Nie odpowiadają na szepty, grupy, handel ani gildie. Trzy kolejne, Tieru, Tiieru i Tiiieru, po jednym na królestwo, to dropki medali: z blokadą expa jak inne dropki farmią loch małp, sprzedają medale w ladzie i na straganie i wołają o medalach na tych samych zasadach. Wyłączone: wszystkie sześć od razu się wylogowuje i nie wraca, dopóki ich nie włączysz.",
                  "de":"Drei zusätzliche Bots, einer pro Reich, über der Botanzahl. Sie leveln bis 15 und stehen dann ohne Waffe und Rüstung beim Gemischtwarenhändler im ersten Dorf und rufen ab und zu etwas in den Ruf-Kanal. Drei weitere, Tieru, Tiieru und Tiiieru, sind Medaillen-Dropper im Affen-Dungeon und rufen ihre eigenen Sprüche über Medaillen. Aus: alle sechs loggen sofort aus.",
                  "tr":"Her krallık için bir tane olmak üzere bot sayısının üstünde üç ek bot. 15. seviyeye kadar kasarlar, sonra ilk köyün genel mağazasının yanında silahsız ve zırhsız durup ara sıra bağırma kanalına yazarlar. Üç tane daha, Tieru, Tiieru ve Tiiieru, Maymun Zindanı'nda madalya düşürücüdür ve madalyalar hakkında kendi sözlerini bağırırlar. Kapalı: altısı da hemen çıkış yapar."},
 "ai_persona":   {"en":"Bot personalities (Iwakura v2)","pl":"Osobowości botów (Iwakura v2)","de":"Bot-Persönlichkeiten (Iwakura v2)","tr":"Bot kişilikleri (Iwakura v2)"},
 "ai_persona_help": {"en":"Iwakura's personality system. A bot's personality follows its situation (Grinder, Conqueror, Trader, Gambler, Perfectionist, Metin slayer, Miner, Herbalist, Fisherman, Mercenary, Companion) and it has a mood (poor, normal, very good) shown on its card. A Grinder holds its level at its tier (15, 23, 30-35, 40-48, 55-62) until it wears a weapon +7, an armour +6 and a shield +6 for its level, and only then may level on as a Conqueror. A bot in a poor mood pauses between packs and goes AFK now and then; only such bots rest in town. Now and then a bot becomes one of five rare personalities for a while, shown in red: Metinologist, Addict, Mad Scientist, Executioner, Mad Angler - or, among the richest bots, one of four gamblers shown in purple: Junior, Senior, Chief and Mad Gambler. Off: the bots play as they did before, with their old personalities.",
                  "pl":"System osobowości Iwakury. Osobowość bota wynika z jego sytuacji (Grinder, Zdobywca, Handlarz, Hazardzista, Perfekcjonista, Pogromca metinów, Górnik, Zielarz, Rybak, Najemnik, Towarzysz), a bot ma nastrój (słaby, normalny, bardzo dobry) widoczny na jego karcie. Grinder trzyma poziom swojego progu (15, 23, 30-35, 40-48, 55-62), dopóki nie założy broni +7, zbroi +6 i tarczy +6 na swój poziom - dopiero wtedy może dalej expić jako Zdobywca. Bot w słabym nastroju robi przerwy między grupami mobów i co jakiś czas odchodzi od komputera; tylko takie boty odpoczywają w mieście. Od czasu do czasu bot staje się na pewien czas jedną z pięciu rzadkich osobowości, widocznych na czerwono: Metinolog, Nałogowiec, Szalony Naukowiec, Egzekutor, Szalony Wędkarz - a wśród najbogatszych botów jednym z czterech Hazardzistów, widocznych na fioletowo: Młodszym, Starszym, Naczelnym i Szalonym. Wyłączone: boty grają jak wcześniej, ze starymi osobowościami.",
                  "de":"Iwakuras Persönlichkeitssystem. Die Persönlichkeit eines Bots folgt seiner Lage (Grinder, Eroberer, Händler, Spieler, Perfektionist, Metinjäger, Bergmann, Kräuterkundiger, Fischer, Söldner, Gefährte), und er hat eine Stimmung (schlecht, normal, sehr gut), die auf seiner Karte steht. Ein Grinder hält die Stufe seines Tiers (15, 23, 30-35, 40-48, 55-62), bis er eine Waffe +7, eine Rüstung +6 und einen Schild +6 für seine Stufe trägt, und erst dann darf er als Eroberer weiterleveln. Ein Bot in schlechter Stimmung macht Pausen zwischen den Gruppen und ist ab und zu AFK; nur solche Bots ruhen in der Stadt. Aus: die Bots spielen wie früher, mit ihren alten Persönlichkeiten.",
                  "tr":"Iwakura'nın kişilik sistemi. Bir botun kişiliği durumuna göre değişir (Grinder, Fatih, Tüccar, Kumarbaz, Mükemmeliyetçi, Metin avcısı, Madenci, Bitkici, Balıkçı, Paralı asker, Yoldaş) ve kartında görünen bir ruh hali vardır (kötü, normal, çok iyi). Bir Grinder, seviyesine uygun +7 silah, +6 zırh ve +6 kalkan giyene kadar kademesinin seviyesinde (15, 23, 30-35, 40-48, 55-62) kalır, ancak ondan sonra Fatih olarak seviye atlayabilir. Kötü ruh halindeki bot gruplar arasında durur ve ara sıra AFK olur; yalnızca bu botlar şehirde dinlenir. Kapalı: botlar eski kişilikleriyle önceki gibi oynar."},
 "ai_persona_on": {"en":"Enabled","pl":"Włączone","de":"Eingeschaltet","tr":"Açık"},
 "ai_experimental": {"en":"experimental","pl":"eksperymentalne","de":"experimentell","tr":"deneysel"},
 "ai_scrap":     {"en":"Scrap keepers","pl":"Boty złomiarze","de":"Schrotthändler-Bots","tr":"Hurdacı botlar"},
 "ai_scrap_help":{"en":"The share of stall keepers that put their low refines (+0 to +3) on the counter, cheaply, instead of vendoring them - fodder for burning at the blacksmith, the way the hard servers play. Off by default.",
                  "pl":"Udział straganiarzy, którzy wystawiają na ladę swoje słabe ulepszenia (+0 do +3) za grosze zamiast sprzedawać je NPC - złom do palenia u kowala, jak na serwerach hard. Domyślnie wyłączone.",
                  "de":"Anteil der Standbetreiber, die ihre schwachen Verbesserungen (+0 bis +3) billig auf den Tresen legen statt sie zu verkaufen - Futter für den Schmied, wie auf den Hard-Servern. Standardmäßig aus.",
                  "tr":"Tezgâhçıların, düşük yükseltmelerini (+0 ile +3) NPC'ye satmak yerine ucuza tezgâha koyan payı - demircide yakmalık, hard sunuculardaki gibi. Varsayılan olarak kapalı."},
 "ai_scrap_off": {"en":"off","pl":"wyłączone","de":"aus","tr":"kapalı"},
 "ai_scrap_all": {"en":"every keeper","pl":"każdy straganiarz","de":"jeder Händler","tr":"her tezgâhçı"},
 "ai_sale_tax":  {"en":"Tax on sales between players (%)","pl":"Podatek od sprzedaży między graczami (%)","de":"Steuer auf Verkäufe zwischen Spielern (%)","tr":"Oyuncular arası satış vergisi (%)"},
 "ai_sale_tax_help": {"en":"A part of the price of an item sold to another player or bot (shops, the market, the bots' counters) leaves the game - it limits the amount of yang in circulation. 0% = no tax. Sales to NPCs are not taxed. The offline counters also keep their own tax (5% by default). Applies within seconds, no restart.",
                  "pl":"Część ceny przedmiotu sprzedanego innemu graczowi lub botowi (sklepy, targ, lady botów) znika z gry - ogranicza ilość yang w obiegu. 0% = bez podatku. Sprzedaż NPC bez podatku. Lady offline pobierają ponadto swój własny podatek (domyślnie 5%). Działa po kilku sekundach, bez restartu.",
                  "de":"Ein Teil des Preises eines Gegenstands, der an einen anderen Spieler oder Bot verkauft wird (Läden, Markt, Theken der Bots), verschwindet aus dem Spiel - das begrenzt die Menge an Yang im Umlauf. 0% = keine Steuer. Verkäufe an NPCs sind steuerfrei. Die Offline-Theken behalten zusätzlich ihre eigene Steuer (standardmäßig 5%). Wirkt nach wenigen Sekunden, ohne Neustart.",
                  "tr":"Başka bir oyuncuya veya bota satılan bir eşyanın fiyatının bir kısmı (dükkanlar, pazar, botların tezgâhları) oyundan silinir - dolaşımdaki yang miktarını sınırlar. %0 = vergi yok. NPC'ye satışlar vergilendirilmez. Çevrimdışı tezgâhlar ayrıca kendi vergilerini alır (varsayılan %5). Birkaç saniye içinde, yeniden başlatmadan geçerli olur."},
 "ai_sale_tax_off": {"en":"no tax","pl":"bez podatku","de":"keine Steuer","tr":"vergi yok"},
 "ai_rest":      {"en":"Resting in town","pl":"Odpoczynek w mieście","de":"Ausruhen in der Stadt","tr":"Şehirde dinlenme"},
 "ai_rest_help": {"en":"The share of bots that stay on the market ring for about three minutes after finishing their business in the first village, strolling between the stalls. 0 - nobody rests: the bots hunt all the time and only come to town on errands. Whatever the slider says, a bot under level 18 never rests, and with no stall open nobody browses stalls. With the bot personalities on, only bots in a poor mood rest, and the slider is the share of them.",
                  "pl":"Udział botów, które po załatwieniu spraw w pierwszej wiosce zostają na rynku około trzech minut i spacerują między straganami. 0 - nikt nie odpoczywa: boty cały czas expią, a do miasta przychodzą tylko w sprawach. Niezależnie od suwaka bot poniżej 18 poziomu nie odpoczywa nigdy, a bez wystawionego straganu nikt nie ogląda straganów. Przy włączonych osobowościach botów odpoczywają tylko boty w słabym nastroju, a suwak to ich udział.",
                  "de":"Anteil der Bots, die nach erledigten Besorgungen im ersten Dorf rund drei Minuten auf dem Marktring bleiben und zwischen den Ständen bummeln. 0 - niemand ruht sich aus: die Bots jagen die ganze Zeit und kommen nur für Besorgungen in die Stadt. Unabhängig vom Regler ruht ein Bot unter Stufe 18 nie, und ohne offenen Stand schaut niemand Stände an. Mit eingeschalteten Bot-Persönlichkeiten ruhen nur Bots in schlechter Stimmung, und der Regler ist ihr Anteil.",
                  "tr":"İlk köydeki işlerini bitirdikten sonra yaklaşık üç dakika pazar halkasında kalıp tezgâhlar arasında dolaşan botların payı. 0 - kimse dinlenmez: botlar sürekli avlanır, şehre yalnızca iş için gelir. Kaydırıcı ne derse desin 18. seviyenin altındaki bot asla dinlenmez, açık tezgâh yokken kimse tezgâhlara bakmaz. Bot kişilikleri açıkken yalnızca kötü ruh halindeki botlar dinlenir ve kaydırıcı onların payıdır."},
 "ai_rest_off":  {"en":"nobody rests","pl":"nikt nie odpoczywa","de":"niemand ruht","tr":"kimse dinlenmez"},
 "ai_rest_all":  {"en":"every bot","pl":"każdy bot","de":"jeder Bot","tr":"her bot"},
 "ai_will_bp":   {"en":"Will to do the Battle Pass","pl":"Chęć robienia Battle Passa"},
 "ai_will_bp_help": {"en":"The chance a bot with nothing to do deliberately takes a Battle Pass mission on (Metin, fishing, blacksmith, boss). 100 = as before (15-70% by personality), 250 = 2.5x that (at most every time), 0 = only progress by the way. Applies at once.",
                  "pl":"Szansa, że bot bez zajęcia celowo weźmie misję Battle Passa (metin, ryby, kowal, boss). 100 = jak dotąd (15–70% wg osobowości), 250 = 2,5× tyle (najwyżej za każdym razem), 0 = tylko postęp przy okazji. Od razu."},
 "ai_will_bp_off": {"en":"only by the way","pl":"tylko przy okazji"},
 "ai_will_all":  {"en":"as before","pl":"jak dotąd"},
 "ai_will_sash": {"en":"Will to make sashes","pl":"Chęć robienia szarf"},
 "ai_will_sash_help": {"en":"The pool of level 30+ bots that build sashes. 100 = 80% of them, 125 and up = all, 0 = none (the rest sell their sashes). At the next sash check (3-6 min).",
                  "pl":"Pula botów od 30 lv, które budują szarfy. 100 = 80% z nich, od 125 = wszystkie, 0 = nikt (reszta sprzedaje szarfy). Przy następnym sprawdzeniu szarf (3–6 min)."},
 "ai_will_sash_off": {"en":"nobody builds, all sell","pl":"nikt nie buduje, wszyscy sprzedają"},
 "ai_will_alch": {"en":"Will to do alchemy","pl":"Chęć robienia alchemii"},
 "ai_will_alch_help": {"en":"The pool of level 30+ bots that use Dragon Soul alchemy. 100 = 75% of them, 135 and up = all, 0 = none (the rest sell Cors and spare stones). Applies at once.",
                  "pl":"Pula botów od 30 lv, które używają alchemii smoka. 100 = 75% z nich, od 135 = wszystkie, 0 = nikt (reszta sprzedaje Cory i zbędne kamienie). Od razu."},
 "ai_will_alch_off": {"en":"nobody uses, all sell","pl":"nikt nie używa, wszyscy sprzedają"},
 "ai_kpvp":      {"en":"Hostility between kingdoms","pl":"Wrogość między królestwami","de":"Feindschaft zwischen Königreichen","tr":"Krallıklar arası düşmanlık"},
 "ai_kpvp_help": {"en":"The share of bots that will start a duel with a bot of another kingdom when they meet on shared ground - Orc Valley, the desert, Mount Sohan, the dungeons. Never in a village, never against a player, and never against a bot that is hurt or already fighting one. Which bots are the aggressive ones is fixed per character, so the same ones quarrel after every restart. Off by default. It works only with the shared world layout (unified, the default up to 1500 bots): under split every core holds one kingdom's bots, so there is nobody to fight.",
                  "pl":"Udział botów, które zaczepią bota z innego królestwa, gdy spotkają go na wspólnym terenie - w Dolinie Orków, na pustyni, na Górze Sohan, w lochach. Nigdy w wiosce, nigdy na graczu i nigdy na bocie rannym albo już walczącym. To, które boty są agresywne, jest przypisane na stałe do postaci, więc po każdym restarcie zaczepiają te same. Domyślnie wyłączone. Działa tylko przy wspólnym układzie świata (unified, domyślny do 1500 botów): przy układzie split każdy rdzeń ma boty jednego królestwa i nie ma z kim walczyć.",
                  "de":"Anteil der Bots, die einen Bot eines anderen Königreichs zum Duell fordern, wenn sie ihn auf gemeinsamem Gebiet treffen - Orktal, Wüste, Berg Sohan, die Verliese. Nie in einem Dorf, nie gegen einen Spieler und nie gegen einen verletzten oder schon kämpfenden Bot. Welche Bots aggressiv sind, ist pro Figur fest, also streiten nach jedem Neustart dieselben. Standardmäßig aus. Wirkt nur mit dem gemeinsamen Weltlayout (unified, Standard bis 1500 Bots): bei split hält jeder Kern die Bots eines Königreichs, und es gibt niemanden zum Kämpfen.",
                  "tr":"Ortak arazide - Ork Vadisi, çöl, Sohan Dağı, zindanlar - başka krallıktan bir botla karşılaştığında düello başlatacak botların oranı. Asla köyde, asla bir oyuncuya karşı ve asla yaralı ya da zaten dövüşen bir bota karşı değil. Hangi botların saldırgan olduğu karaktere sabittir, yani her yeniden başlatmadan sonra aynıları kavga eder. Varsayılan olarak kapalı. Yalnızca ortak dünya düzeninde çalışır (unified, 1500 bota kadar varsayılan): split'te her çekirdek tek krallığın botlarını tutar, dövüşecek kimse yoktur."},
 "ai_kpvp_off":  {"en":"peace","pl":"pokój","de":"Frieden","tr":"barış"},
 "ai_kpvp_all":  {"en":"every bot","pl":"każdy bot","de":"jeder Bot","tr":"her bot"},
 "ai_scroll":    {"en":"Blessing and Dragon God Scrolls","pl":"Zwoje Błogosławieństwa i Boga Smoków","de":"Segens- und Drachengott-Schriftrollen","tr":"Kutsama ve Ejderha Tanrısı parşömenleri"},
 "ai_scroll_help":{"en":"The lowest plus a bot upgrades to under a Blessing Scroll or a Dragon God Scroll. At +7 a scroll goes only on the upgrades to +7, +8 and +9, and every lower one is done at the blacksmith without a scroll, like a player who has none - so the item can burn. At +1 nothing is restricted and the bots use scrolls as before: from +7, and earlier on a worn item that could burn and on an item with valuable bonuses. Applies within five seconds.",
                  "pl":"Najniższy plus, na jaki bot ulepsza pod Zwojem Błogosławieństwa albo Zwojem Boga Smoków. Przy +7 zwój idzie tylko na ulepszenia na +7, +8 i +9, a każde niższe bot robi u kowala bez zwoju, jak gracz, który zwojów nie ma - więc przedmiot może spłonąć. Przy +1 nie ma ograniczenia i boty używają zwojów tak jak dotąd: od +7, a wcześniej na założonym przedmiocie, który mógłby spłonąć, i na przedmiocie z cennymi bonusami. Działa w pięć sekund.",
                  "de":"Das niedrigste Plus, auf das ein Bot unter einer Segens- oder Drachengott-Schriftrolle verbessert. Bei +7 geht eine Schriftrolle nur auf die Verbesserungen auf +7, +8 und +9; jede niedrigere macht der Bot beim Schmied ohne Schriftrolle, wie ein Spieler ohne Schriftrollen - der Gegenstand kann also verbrennen. Bei +1 gibt es keine Einschränkung. Wirkt innerhalb von fünf Sekunden.",
                  "tr":"Botun Kutsama veya Ejderha Tanrısı parşömeniyle yükselttiği en düşük artı. +7'de parşömen yalnızca +7, +8 ve +9 yükseltmelerinde kullanılır; daha düşük her yükseltmeyi bot demircide parşömensiz yapar, parşömeni olmayan bir oyuncu gibi - yani eşya yanabilir. +1'de kısıtlama yoktur. Beş saniye içinde uygulanır."},
 "ai_scroll_off":{"en":"no restriction","pl":"bez ograniczenia","de":"keine Einschränkung","tr":"kısıtlama yok"},
 "ai_scroll_top":{"en":"only the upgrade to +9","pl":"tylko ulepszenie na +9","de":"nur die Verbesserung auf +9","tr":"yalnızca +9 yükseltmesi"},
 "ai_chest":     {"en":"Moonlight Treasure Chests","pl":"Szkatułki Księżycowe","de":"Mondschein-Schatztruhen","tr":"Ay Işığı Sandıkları"},
 "ai_chest_help":{"en":"How often a chest drops while a chest event runs (the Events page), in thousandths: per monster kill, and per broken Metin stone. Outside an event no chest drops. The game default is 10‰ (1%) and 300‰ (30%); more chests mean more bonus scrolls, speed potions and Blessing Scrolls for the bots. Applies within five seconds, to bots and players alike.",
                  "pl":"Jak często wypada szkatułka, gdy trwa event szkatułek (strona Eventy), w promilach: z zabitego potwora i z rozbitego Metina. Poza eventem szkatułki nie wypadają. Domyślnie w grze 10‰ (1%) i 300‰ (30%); więcej szkatułek to więcej zwojów bonusów, mikstur szybkości i Zwojów Błogosławieństwa u botów. Działa w pięć sekund, dla botów i graczy tak samo.",
                  "de":"Wie oft eine Truhe fällt, solange ein Truhen-Event läuft (Seite Events), in Promille: pro getötetem Monster und pro zerstörtem Metin. Außerhalb eines Events fällt keine Truhe. Spielstandard 10‰ (1%) und 300‰ (30%); mehr Truhen heißt mehr Bonusrollen, Tempotränke und Segensrollen bei den Bots. Gilt binnen fünf Sekunden, für Bots wie Spieler.",
                  "tr":"Bir sandık etkinliği sürerken (Etkinlikler sayfası) sandığın ne sıklıkla düştüğü, binde olarak: öldürülen canavar başına ve kırılan Metin başına. Etkinlik dışında sandık düşmez. Oyun varsayılanı 10‰ (%1) ve 300‰ (%30); daha çok sandık, botlarda daha çok bonus parşömeni, hız iksiri ve Kutsama Parşömeni demek. Beş saniye içinde, bot ve oyuncu için aynı şekilde uygulanır."},
 "ev_nav":       {"en":"\U0001F389 Events","pl":"\U0001F389 Eventy","de":"\U0001F389 Events","tr":"\U0001F389 Etkinlikler"},
 "ev_open":      {"en":"\U0001F389 Open events","pl":"\U0001F389 Otw\u00f3rz eventy","de":"\U0001F389 Events \u00f6ffnen","tr":"\U0001F389 Etkinlikleri a\u00e7"},
 "gl_nav":       {"en":"\U0001F6E1 Guilds","pl":"\U0001F6E1 Gildie","de":"\U0001F6E1 Gilden","tr":"\U0001F6E1 Loncalar"},
 "gl_open":      {"en":"\U0001F6E1 Open guilds","pl":"\U0001F6E1 Otwórz gildie","de":"\U0001F6E1 Gilden öffnen","tr":"\U0001F6E1 Loncaları aç"},
 "gl_dash_hint": {"en":"The bot guilds by tier: elite, strong, medium and ordinary, with their level, members, ladder and the war they are in.","pl":"Gildie botów wg klasy: elitarne, silne, średnie i zwykłe, z poziomem, członkami, rankingiem i toczoną wojną.","de":"Die Bot-Gilden nach Stufe: Elite, stark, mittel und gewöhnlich, mit Level, Mitgliedern, Rangliste und laufendem Krieg.","tr":"Bot loncaları kademeye göre: elit, güçlü, orta ve sıradan; seviye, üyeler, sıralama ve süren savaşla."},
 "gl_intro":     {"en":"A bot guild has a tier. Every bot's strength is one number (level, weapon, skills, horse, armour); every ten minutes each kingdom's bots are cut into percentiles, and a guild is founded at the tier its founder's percentile puts it in: the top 3% found an elite guild, the top 15% a strong one, the top half a medium one, the rest an ordinary one. A master recruits only above its tier's floor, strongest first; a member that outgrows its guild leaves for a better one. Members offer the guild a share of their experience every hour, so a guild levels and its master spends the skill points.",
                  "pl":"Gildia botów ma klasę. Siła każdego bota to jedna liczba (poziom, broń, umiejętności, koń, zbroja); co dziesięć minut boty każdego królestwa dzielone są na percentyle, a gildia powstaje w klasie, do której trafia percentyl założyciela: górne 3% zakłada gildię elitarną, górne 15% silną, górna połowa średnią, reszta zwykłą. Mistrz rekrutuje tylko powyżej progu swojej klasy, od najsilniejszych; członek, który przerósł gildię, odchodzi do lepszej. Członkowie co godzinę oddają gildii część zdobytego expa, więc gildia wbija poziomy, a mistrz wydaje punkty umiejętności.",
                  "de":"Eine Bot-Gilde hat eine Stufe. Die Stärke jedes Bots ist eine Zahl (Level, Waffe, Fertigkeiten, Pferd, Rüstung); alle zehn Minuten werden die Bots jedes Königreichs in Perzentile geteilt, und eine Gilde wird in der Stufe gegründet, in die das Perzentil ihres Gründers fällt: die oberen 3% gründen eine Elite-Gilde, die oberen 15% eine starke, die obere Hälfte eine mittlere, der Rest eine gewöhnliche. Ein Meister rekrutiert nur über der Schwelle seiner Stufe, die Stärksten zuerst; ein Mitglied, das seiner Gilde entwachsen ist, wechselt in eine bessere. Mitglieder geben der Gilde stündlich einen Teil ihrer Erfahrung, so steigt die Gilde auf und der Meister verteilt die Fertigkeitspunkte.",
                  "tr":"Bir bot loncasının kademesi vardır. Her botun gücü tek bir sayıdır (seviye, silah, beceriler, at, zırh); her on dakikada bir her krallığın botları yüzdelik dilimlere ayrılır ve lonca, kurucusunun dilimine denk gelen kademede kurulur: en üst %3 elit, en üst %15 güçlü, üst yarı orta, geri kalanı sıradan bir lonca kurar. Usta yalnızca kademesinin eşiğinin üzerinden, en güçlülerden başlayarak üye alır; loncasını aşan üye daha iyisine geçer. Üyeler her saat tecrübelerinin bir kısmını loncaya verir, böylece lonca seviye atlar ve usta beceri puanlarını harcar."},
 "gl_stale":     {"en":"No game core has written a guild status yet (it does so within a minute of starting with this version).","pl":"Żaden rdzeń gry nie zapisał jeszcze statusu gildii (robi to w ciągu minuty od startu z tą wersją).","de":"Noch kein Spielkern hat einen Gildenstatus geschrieben (er tut es binnen einer Minute nach dem Start mit dieser Version).","tr":"Henüz hiçbir oyun çekirdeği lonca durumu yazmadı (bu sürümle başladıktan bir dakika içinde yazar)."},
 "gl_summary":   {"en":"Bot guilds","pl":"Gildie botów","de":"Bot-Gilden","tr":"Bot loncaları"},
 "gl_tier_elite":    {"en":"Elite","pl":"Elitarna","de":"Elite","tr":"Elit"},
 "gl_tier_strong":   {"en":"Strong","pl":"Silna","de":"Stark","tr":"Güçlü"},
 "gl_tier_medium":   {"en":"Medium","pl":"Średnia","de":"Mittel","tr":"Orta"},
 "gl_tier_ordinary": {"en":"Ordinary","pl":"Zwykła","de":"Gewöhnlich","tr":"Sıradan"},
 "gl_empire_shinsoo":{"en":"Shinsoo","pl":"Shinsoo","de":"Shinsoo","tr":"Shinsoo"},
 "gl_empire_chunjo": {"en":"Chunjo","pl":"Chunjo","de":"Chunjo","tr":"Chunjo"},
 "gl_empire_jinno":  {"en":"Jinno","pl":"Jinno","de":"Jinno","tr":"Jinno"},
 "gl_empire_unknown":{"en":"?","pl":"?","de":"?","tr":"?"},
 # MT2009_PLUS_LEGENDS_V1: the bots' legends (playerbot_legends.h).
 "ai_legends":   {"en":"Bot Legend system","pl":"System Legend botów","de":"Legendensystem der Bots","tr":"Bot efsane sistemi"},
 "ai_legends_help": {"en":"Some bots get a tier for good: Distinguished (~4%), Special (~2%), two Walking Legends per kingdom and at most one Kingdom Champion (the Legend whose guild leads its kingdom's guild ranking, checked hourly). Tiers carry built-in bonuses (HP, strong vs humans and monsters, experience, skill books and Spirit Stones counting more, refine chance), better PvP and gear choices, Legends found and lead strong guilds and treat each other as rivals. Players see a coloured title above the nick, and notable events are announced in chat. Off: no bonuses, titles or announcements - the tiers and reputation stay in the database.","pl":"Część botów dostaje stałą rangę: Wyróżniający się (~4%), Specjalny (~2%), dwie Chodzące Legendy na królestwo i najwyżej jeden Czempion Królestwa (Legenda, której gildia prowadzi w rankingu gildii królestwa, sprawdzane co godzinę). Rangi dają wbudowane bonusy (PŻ, silny na ludzi i potwory, exp, księgi i Kamienie Duchowe liczone kilkukrotnie, szansa ulepszenia), lepsze PvP i dobór ekwipunku; Legendy zakładają i prowadzą silne gildie i traktują się jak rywale. Gracze widzą kolorowy tytuł nad nickiem, a ważne wydarzenia są ogłaszane na czacie. Wyłączone: bez bonusów, tytułów i ogłoszeń – rangi i reputacja zostają w bazie.","de":"Einige Bots erhalten dauerhaft einen Rang (Ausgezeichnet, Speziell, Wandelnde Legende, Champion des Königreichs) mit eingebauten Boni, Titeln über dem Namen und Ansagen im Chat. Aus: keine Boni, Titel oder Ansagen.","tr":"Bazı botlar kalıcı bir rütbe alır (Seçkin, Özel, Yürüyen Efsane, Krallık Şampiyonu): yerleşik bonuslar, isim üstü unvan ve sohbet duyuruları. Kapalı: bonus, unvan ve duyuru yok."},
 "lg_nav":       {"en":"\U0001F3C6 Legends","pl":"\U0001F3C6 Legendy","de":"\U0001F3C6 Legenden","tr":"\U0001F3C6 Efsaneler"},
 "lg_open":      {"en":"Open the Legend ranking","pl":"Otwórz ranking Legend","de":"Legenden-Rangliste öffnen","tr":"Efsane sıralamasını aç"},
 "lg_dash_hint": {"en":"The bots' tiers, the Kingdom Champions and Walking Legends, reputation and the latest legend events.","pl":"Rangi botów, Czempioni i Chodzące Legendy królestw, reputacja i ostatnie wydarzenia legend.","de":"Ränge der Bots, Champions und Legenden, Ruf und letzte Ereignisse.","tr":"Bot rütbeleri, şampiyonlar, efsaneler, itibar ve son olaylar."},
 "lg_intro":     {"en":"Tiers are kept in player.playerbot_legend and given by the game core; reputation grows with won guild wars, killed players, boss kills and achievements.","pl":"Rangi są w player.playerbot_legend i nadaje je rdzeń gry; reputacja rośnie za wygrane wojny gildii, zabitych graczy, bossów i osiągnięcia.","de":"Ränge stehen in player.playerbot_legend; Ruf wächst durch Gildenkriege, getötete Spieler, Bosse und Erfolge.","tr":"Rütbeler player.playerbot_legend tablosunda; itibar lonca savaşları, oyuncu ve boss öldürmeleri ile artar."},
 "lg_missing":   {"en":"The table does not exist yet (start the server with the new version).","pl":"Tabela jeszcze nie istnieje (uruchom serwer z nową wersją).","de":"Die Tabelle existiert noch nicht (Server mit der neuen Version starten).","tr":"Tablo henüz yok (sunucuyu yeni sürümle başlatın)."},
 "lg_off":       {"en":"The Legend system is switched off in the bot behaviour settings: tiers are kept, but no bonuses, titles or announcements.","pl":"System Legend jest wyłączony w zachowaniu botów: rangi zostają, ale bez bonusów, tytułów i ogłoszeń.","de":"Das Legendensystem ist ausgeschaltet.","tr":"Efsane sistemi kapalı."},
 "lg_tier1":     {"en":"Distinguished","pl":"Wyróżniający się","de":"Ausgezeichnet","tr":"Seçkin"},
 "lg_tier2":     {"en":"Special","pl":"Specjalny","de":"Speziell","tr":"Özel"},
 "lg_tier3":     {"en":"Walking Legend","pl":"Chodząca Legenda","de":"Wandelnde Legende","tr":"Yürüyen Efsane"},
 "lg_tier4":     {"en":"Champion","pl":"Czempion","de":"Champion","tr":"Şampiyon"},
 "lg_heads":     {"en":"Champions and Legends","pl":"Czempioni i Legendy","de":"Champions und Legenden","tr":"Şampiyonlar ve Efsaneler"},
 "lg_no_champion":{"en":"no Champion","pl":"brak Czempiona","de":"kein Champion","tr":"şampiyon yok"},
 "lg_no_legend": {"en":"no Legends yet","pl":"brak Legend","de":"keine Legenden","tr":"efsane yok"},
 "lg_ranking":   {"en":"Ranking","pl":"Ranking","de":"Rangliste","tr":"Sıralama"},
 "lg_all":       {"en":"all","pl":"wszystkie","de":"alle","tr":"hepsi"},
 "lg_filter":    {"en":"Show","pl":"Pokaż","de":"Zeigen","tr":"Göster"},
 "lg_col_tier":  {"en":"Tier","pl":"Ranga","de":"Rang","tr":"Rütbe"},
 "lg_col_nick":  {"en":"Nick","pl":"Nick","de":"Name","tr":"İsim"},
 "lg_col_rep":   {"en":"Reputation","pl":"Reputacja","de":"Ruf","tr":"İtibar"},
 "lg_col_kills": {"en":"Players killed","pl":"Zabici gracze","de":"Getötete Spieler","tr":"Öldürülen oyuncu"},
 "lg_col_deaths":{"en":"Died to players","pl":"Polegli od graczy","de":"Von Spielern getötet","tr":"Oyunculara ölüm"},
 "lg_col_wars":  {"en":"Wars won/lost","pl":"Wojny wygr./przegr.","de":"Kriege gew./verl.","tr":"Savaş kaz./kay."},
 "lg_col_boss":  {"en":"Bosses","pl":"Bossowie","de":"Bosse","tr":"Bosslar"},
 "lg_col_ach":   {"en":"Achievements","pl":"Osiągnięcia","de":"Erfolge","tr":"Başarılar"},
 "lg_col_since": {"en":"Tier since","pl":"Ranga od","de":"Rang seit","tr":"Rütbe tarihi"},
 "lg_none":      {"en":"No tiered bots yet.","pl":"Jeszcze żaden bot nie ma rangi.","de":"Noch keine Bots mit Rang.","tr":"Henüz rütbeli bot yok."},
 "lg_events":    {"en":"Latest legend events","pl":"Ostatnie wydarzenia legend","de":"Letzte Ereignisse","tr":"Son olaylar"},
 "lg_no_events": {"en":"Nothing has happened yet.","pl":"Jeszcze nic się nie wydarzyło.","de":"Noch nichts passiert.","tr":"Henüz bir şey olmadı."},
 "lg_ach_plus9": {"en":"+9 weapon","pl":"broń +9","de":"Waffe +9","tr":"+9 silah"},
 "lg_ach_lv75":  {"en":"level 75","pl":"poziom 75","de":"Stufe 75","tr":"seviye 75"},
 "lg_ach_lv99":  {"en":"level 99","pl":"poziom 99","de":"Stufe 99","tr":"seviye 99"},
 "lg_ach_boss":  {"en":"first boss","pl":"pierwszy boss","de":"erster Boss","tr":"ilk boss"},
 "lg_ach_kills": {"en":"100 players killed","pl":"100 zabitych graczy","de":"100 getötete Spieler","tr":"100 oyuncu"},
 "lg_ach_champ": {"en":"was a Champion","pl":"był Czempionem","de":"war Champion","tr":"şampiyon oldu"},
 # MT2009_PLUS_WEEKLY_RANKING_V1: the weekly ranking and its titles.
 "wr_nav":       {"en":"\U0001F947 Weekly ranking","pl":"\U0001F947 Ranking tygodniowy","de":"\U0001F947 Wochenrangliste","tr":"\U0001F947 Haftalık sıralama"},
 "wr_open":      {"en":"Open the weekly ranking","pl":"Otwórz ranking tygodniowy","de":"Wochenrangliste öffnen","tr":"Haftalık sıralamayı aç"},
 "wr_dash_hint": {"en":"Weekly seasons in eight categories; the top three of each get a title with a bonus for the whole next season.","pl":"Tygodniowe sezony w ośmiu kategoriach; najlepsza trójka każdej dostaje tytuł z bonusem na cały następny sezon.","de":"Wöchentliche Saisons in acht Kategorien; die besten drei erhalten einen Titel mit Bonus für die nächste Saison.","tr":"Sekiz kategoride haftalık sezonlar; ilk üç, sonraki sezon için bonuslu bir unvan alır."},
 "wr_missing":   {"en":"The weekly ranking tables do not exist yet - start the game server (the new version creates them).","pl":"Tabele rankingu tygodniowego jeszcze nie istnieją – uruchom serwer gry (nowa wersja je tworzy).","de":"Die Tabellen existieren noch nicht - den Spielserver starten.","tr":"Tablolar henüz yok - oyun sunucusunu başlatın."},
 "wr_settings":  {"en":"Settings","pl":"Ustawienia","de":"Einstellungen","tr":"Ayarlar"},
 "wr_enabled":   {"en":"Weekly ranking on","pl":"Ranking tygodniowy włączony","de":"Wochenrangliste an","tr":"Haftalık sıralama açık"},
 "wr_days":      {"en":"Season length (days, 1-28)","pl":"Długość sezonu (dni, 1-28)","de":"Saisonlänge (Tage, 1-28)","tr":"Sezon uzunluğu (gün, 1-28)"},
 "wr_save":      {"en":"Save","pl":"Zapisz","de":"Speichern","tr":"Kaydet"},
 "wr_end_now":   {"en":"End the season now","pl":"Zakończ sezon teraz","de":"Saison jetzt beenden","tr":"Sezonu şimdi bitir"},
 "wr_end_confirm":{"en":"End the current season now? The game hands out the titles within about 2 minutes.","pl":"Zakończyć bieżący sezon teraz? Gra rozda tytuły w ciągu ok. 2 minut.","de":"Die Saison jetzt beenden?","tr":"Sezon şimdi bitirilsin mi?"},
 "wr_help":      {"en":"A changed length takes effect from the next season. By default the season resets on Monday at 00:00 (7 days). Off: nothing is counted, no bonuses, the titles are hidden.","pl":"Zmieniona długość obowiązuje od następnego sezonu. Domyślnie sezon resetuje się w poniedziałek o 00:00 (7 dni). Wyłączony: nic nie jest liczone, brak bonusów, tytuły są ukryte.","de":"Eine geänderte Länge gilt ab der nächsten Saison. Standard: Reset Montag 00:00 (7 Tage). Aus: keine Zählung, keine Boni, Titel verborgen.","tr":"Değişen uzunluk sonraki sezondan geçerlidir. Varsayılan: pazartesi 00:00 (7 gün). Kapalı: sayım yok, bonus yok, unvanlar gizli."},
 "wr_saved":     {"en":"Saved. The game applies it within about 2 minutes.","pl":"Zapisano. Gra zastosuje to w ciągu ok. 2 minut.","de":"Gespeichert.","tr":"Kaydedildi."},
 "wr_ended":     {"en":"The season ends now - the game rolls it over within about 2 minutes and announces the results.","pl":"Sezon kończy się teraz – gra przełączy go w ciągu ok. 2 minut i ogłosi wyniki.","de":"Die Saison endet jetzt.","tr":"Sezon şimdi bitiyor."},
 "wr_failed":    {"en":"Could not save - the weekly ranking tables do not exist yet (start the game server).","pl":"Nie udało się zapisać – tabele rankingu tygodniowego jeszcze nie istnieją (uruchom serwer gry).","de":"Speichern fehlgeschlagen.","tr":"Kaydedilemedi."},
 "wr_off":       {"en":"The weekly ranking is switched off: nothing is counted, no bonuses, titles hidden.","pl":"Ranking tygodniowy jest wyłączony: nic nie jest liczone, brak bonusów, tytuły ukryte.","de":"Die Wochenrangliste ist aus.","tr":"Haftalık sıralama kapalı."},
 "wr_season":    {"en":"Current season","pl":"Bieżący sezon","de":"Aktuelle Saison","tr":"Mevcut sezon"},
 "wr_season_no": {"en":"Season","pl":"Sezon","de":"Saison","tr":"Sezon"},
 "wr_start":     {"en":"Start","pl":"Początek","de":"Beginn","tr":"Başlangıç"},
 "wr_end":       {"en":"End","pl":"Koniec","de":"Ende","tr":"Bitiş"},
 "wr_left":      {"en":"Time left","pl":"Pozostało","de":"Verbleibend","tr":"Kalan"},
 "wr_end_unset": {"en":"not set yet (the game sets the next Monday 00:00)","pl":"jeszcze nie ustawiony (gra ustawi najbliższy poniedziałek 00:00)","de":"noch nicht gesetzt (nächster Montag 00:00)","tr":"henüz yok (sonraki pazartesi 00:00)"},
 "wr_ending":    {"en":"ending (within ~2 min)","pl":"kończy się (w ciągu ~2 min)","de":"endet (~2 Min.)","tr":"bitiyor (~2 dk)"},
 "wr_holders":   {"en":"Current title holders","pl":"Obecni posiadacze tytułów","de":"Aktuelle Titelträger","tr":"Mevcut unvan sahipleri"},
 "wr_no_holders":{"en":"no titles this season","pl":"brak tytułów w tym sezonie","de":"keine Titel","tr":"unvan yok"},
 "wr_live":      {"en":"Live ranking (top 50)","pl":"Ranking na żywo (top 50)","de":"Live-Rangliste (Top 50)","tr":"Canlı sıralama (ilk 50)"},
 "wr_category":  {"en":"Category","pl":"Kategoria","de":"Kategorie","tr":"Kategori"},
 "wr_col_place": {"en":"Place","pl":"Miejsce","de":"Platz","tr":"Sıra"},
 "wr_col_type":  {"en":"Type","pl":"Typ","de":"Typ","tr":"Tür"},
 "wr_col_value": {"en":"Value","pl":"Wynik","de":"Wert","tr":"Değer"},
 "wr_col_exp":   {"en":"Experience","pl":"Doświadczenie","de":"Erfahrung","tr":"Tecrübe"},
 "wr_col_title": {"en":"Title","pl":"Tytuł","de":"Titel","tr":"Unvan"},
 "wr_col_bonus": {"en":"Bonus","pl":"Bonus","de":"Bonus","tr":"Bonus"},
 "wr_bot":       {"en":"Bot","pl":"Bot","de":"Bot","tr":"Bot"},
 "wr_player":    {"en":"Player","pl":"Gracz","de":"Spieler","tr":"Oyuncu"},
 "wr_none":      {"en":"Nobody has scored in this category yet this season.","pl":"W tej kategorii nikt jeszcze nie zdobył punktów w tym sezonie.","de":"Noch keine Punkte in dieser Kategorie.","tr":"Bu kategoride henüz puan yok."},
 "wr_cat1":      {"en":"Monsters killed","pl":"Zabite potwory","de":"Getötete Monster","tr":"Öldürülen canavar"},
 "wr_cat2":      {"en":"Metin stones destroyed","pl":"Zniszczone metiny","de":"Zerstörte Metinsteine","tr":"Yok edilen metin taşı"},
 "wr_cat3":      {"en":"Bosses defeated","pl":"Pokonane bossy","de":"Besiegte Bosse","tr":"Yenilen boss"},
 "wr_cat4":      {"en":"Players killed (PvP)","pl":"Zabici gracze (PvP)","de":"Getötete Spieler (PvP)","tr":"Öldürülen oyuncu (PvP)"},
 "wr_cat5":      {"en":"Dungeons completed","pl":"Ukończone wyprawy (dungeony)","de":"Abgeschlossene Dungeons","tr":"Tamamlanan zindan"},
 "wr_cat6":      {"en":"Successful item upgrades","pl":"Udane ulepszenia przedmiotów","de":"Erfolgreiche Verbesserungen","tr":"Başarılı yükseltme"},
 "wr_cat7":      {"en":"Alchemy (successful Dragon Stone upgrades)","pl":"Alchemia (udane ulepszenia Smoczych Kamieni)","de":"Alchemie (Drachensteine verbessert)","tr":"Simya (ejderha taşı yükseltme)"},
 "wr_cat8":      {"en":"Level","pl":"Poziom","de":"Stufe","tr":"Seviye"},
 "wr_b_monsters":{"en":"strong against monsters","pl":"silny przeciwko potworom","de":"stark gegen Monster","tr":"canavarlara karşı güçlü"},
 "wr_b_bosses":  {"en":"strong against bosses","pl":"silny przeciwko bossom","de":"stark gegen Bosse","tr":"bosslara karşı güçlü"},
 "wr_b_humans":  {"en":"strong against humans","pl":"silny przeciwko ludziom","de":"stark gegen Menschen","tr":"insanlara karşı güçlü"},
 "wr_b_maxhp":   {"en":"max HP","pl":"max PŻ","de":"max. TP","tr":"maks. HP"},
 "wr_b_attack":  {"en":"attack value","pl":"wartość ataku","de":"Angriffswert","tr":"saldırı değeri"},
 "wr_b_both":    {"en":"strong against monsters and humans","pl":"silny przeciwko potworom i ludziom","de":"stark gegen Monster und Menschen","tr":"canavar ve insanlara karşı güçlü"},
 "gl_col_name":  {"en":"Guild","pl":"Gildia","de":"Gilde","tr":"Lonca"},
 "gl_col_kingdom":{"en":"Kingdom","pl":"Królestwo","de":"Königreich","tr":"Krallık"},
 "gl_col_tier":  {"en":"Tier","pl":"Klasa","de":"Stufe","tr":"Kademe"},
 "gl_col_level": {"en":"Level","pl":"Poziom","de":"Level","tr":"Seviye"},
 "gl_col_members":{"en":"Members","pl":"Członkowie","de":"Mitglieder","tr":"Üyeler"},
 "gl_col_online":{"en":"Online","pl":"Online","de":"Online","tr":"Çevrimiçi"},
 "gl_col_master":{"en":"Master","pl":"Mistrz","de":"Meister","tr":"Usta"},
 "gl_col_strength":{"en":"Avg. strength","pl":"Śr. siła","de":"Ø Stärke","tr":"Ort. güç"},
 "gl_col_ladder":{"en":"Ladder","pl":"Ranking","de":"Rangliste","tr":"Sıralama"},
 "gl_col_record":{"en":"W/D/L","pl":"Z/R/P","de":"S/U/N","tr":"G/B/M"},
 "gl_col_exp":   {"en":"Exp received","pl":"Otrzymany exp","de":"Erhaltene Erfahrung","tr":"Alınan tecrübe"},
 "gl_col_war":   {"en":"War","pl":"Wojna","de":"Krieg","tr":"Savaş"},
 "gl_players":   {"en":"Player guilds","pl":"Gildie graczy"},
 "gl_players_note": {"en":"Guilds whose master is a person, from the database: no tier (that is the bots' percentile) and no online count.","pl":"Gildie, których mistrzem jest człowiek, z bazy danych: bez klasy (to percentyl botów) i bez liczby osób online."},
 "gl_players_none": {"en":"No guild is led by a person yet.","pl":"Żadnej gildii nie prowadzi jeszcze człowiek."},
 "gl_war_with":  {"en":"vs","pl":"z","de":"gegen","tr":"vs"},
 "gl_next_war":  {"en":"Next bot guild war","pl":"Następna wojna gildii botów","de":"Nächster Bot-Gildenkrieg","tr":"Sonraki bot lonca savaşı"},
 "gl_next_war_now": {"en":"under way now (guild map)","pl":"trwa teraz (mapa gildyjna)","de":"läuft jetzt (Gildenkarte)","tr":"şu anda sürüyor (lonca haritası)"},
 "gl_next_war_off": {"en":"not scheduled","pl":"niezaplanowana","de":"nicht geplant","tr":"planlanmadı"},
 "gl_next_war_in": {"en":"in about","pl":"za ok.","de":"in etwa","tr":"yaklaşık"},
 "gl_exp_note":  {"en":"Exp received counts what the bots have offered since the cores started; the guild's own exp column in the game is the same number, kept by the DB.","pl":"„Otrzymany exp” liczy, co boty oddały od startu rdzeni; kolumna expa gildii w grze to ta sama liczba, trzymana przez bazę.","de":"„Erhaltene Erfahrung“ zählt, was die Bots seit dem Start der Kerne gegeben haben; die Erfahrungsspalte der Gilde im Spiel ist dieselbe Zahl, von der Datenbank geführt.","tr":"„Alınan tecrübe“ çekirdekler başladığından beri botların verdiğini sayar; oyundaki lonca tecrübe sütunu veritabanının tuttuğu aynı sayıdır."},
 "ev_dash_hint": {"en":"Timed windows: Moonlight chests drop only while their event runs; more experience, drop or yang at chosen hours. \u201cActivate now\u201d switches an event on for a number of minutes.","pl":"Okna czasowe: Szkatu\u0142ki Blasku Ksi\u0119\u017cyca dropi\u0105 tylko wtedy, gdy trwa ich event; wi\u0119cej expa, dropu albo yang o wybranych porach. \u201eAktywuj teraz\u201d w\u0142\u0105cza event na podan\u0105 liczb\u0119 minut.","de":"Zeitfenster: Mondschein-Truhen fallen nur w\u00e4hrend ihres Events; mehr Erfahrung, Drop oder Yang zu gew\u00e4hlten Stunden. \u201eJetzt aktivieren\u201c schaltet ein Event f\u00fcr einige Minuten ein.","tr":"Zaman pencereleri: Ay I\u015f\u0131\u011f\u0131 Sand\u0131klar\u0131 yaln\u0131zca etkinlik s\u00fcrerken d\u00fc\u015fer; se\u00e7ilen saatlerde daha fazla tecr\u00fcbe, drop veya yang. \u201c\u015eimdi etkinle\u015ftir\u201d bir etkinli\u011fi belirli dakika a\u00e7ar."},
 "ev_intro":     {"en":"A row is a weekly window: which days, from what hour to what hour, and for a rate how many percent over the server's own rates (50 = +50%). The game core reads this within five seconds; nothing restarts. A window past midnight (22:00-02:00) runs into the next day.","pl":"Wiersz to okno tygodniowe: w jakie dni, od kt\u00f3rej do kt\u00f3rej, a dla rat o ile procent ponad ustawione raty serwera (50 = +50%). Rdze\u0144 gry odczytuje to w pi\u0119\u0107 sekund; nic si\u0119 nie restartuje. Okno przez p\u00f3\u0142noc (22:00-02:00) trwa do nast\u0119pnego dnia.","de":"Eine Zeile ist ein w\u00f6chentliches Fenster: welche Tage, von wann bis wann, und bei einer Rate wie viel Prozent \u00fcber den Serverraten (50 = +50%). Der Spielkern liest das binnen f\u00fcnf Sekunden; nichts startet neu. Ein Fenster \u00fcber Mitternacht (22:00-02:00) l\u00e4uft in den n\u00e4chsten Tag.","tr":"Bir sat\u0131r haftal\u0131k bir penceredir: hangi g\u00fcnler, saat ka\u00e7tan ka\u00e7a ve oran i\u00e7in sunucu oranlar\u0131n\u0131n y\u00fczde ka\u00e7 \u00fcst\u00fc (50 = +%50). Oyun \u00e7ekirde\u011fi bunu be\u015f saniyede okur; hi\u00e7bir \u015fey yeniden ba\u015flamaz. Gece yar\u0131s\u0131n\u0131 ge\u00e7en pencere (22:00-02:00) ertesi g\u00fcne sarkar."},
 "ev_chest_note":{"en":"Once a single chest window is in the schedule, the chests drop only inside the windows; the two sliders on the bot behaviour page say how often they drop then.","pl":"Gdy w harmonogramie jest cho\u0107 jedno okno szkatu\u0142ek, poza oknami szkatu\u0142ki nie dropi\u0105 wcale; dwa suwaki na stronie zachowania bot\u00f3w m\u00f3wi\u0105, jak cz\u0119sto dropi\u0105 w oknie.","de":"Sobald ein Truhenfenster im Plan steht, fallen die Truhen nur innerhalb der Fenster; die zwei Regler auf der Seite Bot-Verhalten sagen, wie oft sie dann fallen.","tr":"Planda tek bir sand\u0131k penceresi bile varsa sand\u0131klar yaln\u0131zca pencereler i\u00e7inde d\u00fc\u015fer; bot davran\u0131\u015f\u0131 sayfas\u0131ndaki iki kayd\u0131r\u0131c\u0131 o s\u0131rada ne s\u0131kl\u0131kta d\u00fc\u015ft\u00fc\u011f\u00fcn\u00fc s\u00f6yler."},
 "ev_notice_note":{"en":"The chat gets a notice when an event starts, every fifteen minutes while it runs, and when it ends.","pl":"Na czacie pojawia si\u0119 og\u0142oszenie na pocz\u0105tku eventu, co pi\u0119tna\u015bcie minut w jego trakcie i na ko\u0144cu.","de":"Der Chat bekommt eine Meldung zum Start eines Events, alle f\u00fcnfzehn Minuten w\u00e4hrenddessen und zum Ende.","tr":"Etkinlik ba\u015flad\u0131\u011f\u0131nda, s\u00fcrerken her on be\u015f dakikada ve bitti\u011finde sohbete duyuru d\u00fc\u015fer."},
 "ev_status_title":{"en":"Right now","pl":"Teraz","de":"Gerade jetzt","tr":"\u015eu anda"},
 "ev_status_stale":{"en":"The game core has not written an event status yet (it does so within a minute of starting with this version).","pl":"Rdze\u0144 gry nie zapisa\u0142 jeszcze statusu event\u00f3w (robi to w ci\u0105gu minuty od startu z t\u0105 wersj\u0105).","de":"Der Spielkern hat noch keinen Event-Status geschrieben (er tut es binnen einer Minute nach dem Start mit dieser Version).","tr":"Oyun \u00e7ekirde\u011fi hen\u00fcz etkinlik durumu yazmad\u0131 (bu s\u00fcr\u00fcmle ba\u015flad\u0131ktan bir dakika i\u00e7inde yazar)."},
 "ev_active":    {"en":"Active until","pl":"Aktywny do","de":"Aktiv bis","tr":"\u015eu saate kadar aktif:"},
 "ev_next":      {"en":"Next","pl":"Nast\u0119pny","de":"N\u00e4chstes","tr":"Sonraki"},
 "ev_inactive":  {"en":"Not running","pl":"Nieaktywny","de":"L\u00e4uft nicht","tr":"\u00c7al\u0131\u015fm\u0131yor"},
 "ev_none":      {"en":"No window scheduled","pl":"Brak zaplanowanych okien","de":"Kein Fenster geplant","tr":"Planlanm\u0131\u015f pencere yok"},
 "ev_kind_chest":{"en":"Moonlight chests","pl":"Szkatu\u0142ki Blasku Ksi\u0119\u017cyca","de":"Mondschein-Truhen","tr":"Ay I\u015f\u0131\u011f\u0131 Sand\u0131klar\u0131"},
 "ev_kind_bossloot":{"en":"Double boss loot","pl":"Podw\u00f3jny loot z boss\u00f3w","de":"Doppelte Boss-Beute","tr":"\u00c7ift boss ganimeti"},
 "ev_kind_metinloot":{"en":"Double Metin loot","pl":"Podw\u00f3jny loot z Metin\u00f3w","de":"Doppelte Metin-Beute","tr":"\u00c7ift Metin ganimeti"},
 "ev_kind_goblin":{"en":"Treasure Hunt (Goblin)","pl":"Poszukiwanie skarb\u00f3w (Goblin)","de":"Schatzsuche (Goblin)","tr":"Hazine Av\u0131 (Goblin)"},
 "ev_kind_catchking":{"en":"Catch the King","pl":"Z\u0142ap Kr\u00f3la","de":"Fang den K\u00f6nig","tr":"Kral\u0131 Yakala"},
 "ev_kind_rumi":{"en":"Rumi (Okey)","pl":"Rumi (Okey)","de":"Rumi (Okey)","tr":"Okey"},
 "ev_kind_yutnori":{"en":"Yut Nori","pl":"Yut Nori","de":"Yut Nori","tr":"Yut Nori"},
 "ev_kind_flower":{"en":"Flower Event","pl":"Dzieci Kwiaty","de":"Blumen-Event","tr":"\u00c7i\u00e7ek Etkinli\u011fi"},
 "ev_kind_easter":{"en":"Easter event","pl":"Event wielkanocny","de":"Oster-Event","tr":"Paskalya etkinli\u011fi"},
 "ev_kind_exp":  {"en":"Experience","pl":"Do\u015bwiadczenie","de":"Erfahrung","tr":"Tecr\u00fcbe"},
 "ev_kind_drop": {"en":"Item drop","pl":"Drop przedmiot\u00f3w","de":"Item-Drop","tr":"E\u015fya d\u00fc\u015fmesi"},
 "ev_kind_yang": {"en":"Yang","pl":"Yang","de":"Yang","tr":"Yang"},
 "ev_schedule":  {"en":"Schedule","pl":"Harmonogram","de":"Zeitplan","tr":"Zaman \u00e7izelgesi"},
 "ev_col_kind":  {"en":"Event","pl":"Event","de":"Event","tr":"Etkinlik"},
 "ev_col_days":  {"en":"Days","pl":"Dni","de":"Tage","tr":"G\u00fcnler"},
 "ev_col_from":  {"en":"From","pl":"Od","de":"Von","tr":"Ba\u015flang\u0131\u00e7"},
 "ev_col_to":    {"en":"To","pl":"Do","de":"Bis","tr":"Biti\u015f"},
 "ev_col_value": {"en":"+% (rates)","pl":"+% (raty)","de":"+% (Raten)","tr":"+% (oranlar)"},
 "ev_col_on":    {"en":"On","pl":"W\u0142.","de":"An","tr":"A\u00e7\u0131k"},
 "ev_col_del":   {"en":"Delete","pl":"Usu\u0144","de":"L\u00f6schen","tr":"Sil"},
 "ev_days":      {"en":"Mo,Tu,We,Th,Fr,Sa,Su","pl":"Pn,Wt,\u015ar,Cz,Pt,Sb,Nd","de":"Mo,Di,Mi,Do,Fr,Sa,So","tr":"Pt,Sa,\u00c7a,Pe,Cu,Ct,Pz"},
 "ev_save":      {"en":"Save the schedule","pl":"Zapisz harmonogram","de":"Zeitplan speichern","tr":"Zaman \u00e7izelgesini kaydet"},
 "ev_saved":     {"en":"Schedule saved; the game core reads it within five seconds.","pl":"Harmonogram zapisany; rdze\u0144 gry odczyta go w pi\u0119\u0107 sekund.","de":"Zeitplan gespeichert; der Spielkern liest ihn binnen f\u00fcnf Sekunden.","tr":"Zaman \u00e7izelgesi kaydedildi; oyun \u00e7ekirde\u011fi be\u015f saniyede okur."},
 "ev_failed":    {"en":"Could not write the events file.","pl":"Nie uda\u0142o si\u0119 zapisa\u0107 pliku event\u00f3w.","de":"Die Event-Datei konnte nicht geschrieben werden.","tr":"Etkinlik dosyas\u0131 yaz\u0131lamad\u0131."},
 "ev_bad_row":   {"en":"Row %d: the hours must be HH:MM (the start before 24:00) and the value 0-1000.","pl":"Wiersz %d: godziny musz\u0105 by\u0107 HH:MM (pocz\u0105tek przed 24:00), a warto\u015b\u0107 0-1000.","de":"Zeile %d: die Stunden m\u00fcssen HH:MM sein (Beginn vor 24:00), der Wert 0-1000.","tr":"Sat\u0131r %d: saatler SS:DD olmal\u0131 (ba\u015flang\u0131\u00e7 24:00 \u00f6ncesi), de\u011fer 0-1000."},
 "ev_value_help":{"en":"The value is a percentage over the server's rates (50 = +50% experience); the chest rows ignore it. Untick every day to keep a row without running it.","pl":"Warto\u015b\u0107 to procent ponad raty serwera (50 = +50% expa); wiersze szkatu\u0142ek j\u0105 ignoruj\u0105. Odznacz wszystkie dni, by zachowa\u0107 wiersz bez uruchamiania.","de":"Der Wert ist ein Prozentsatz \u00fcber den Serverraten (50 = +50% Erfahrung); Truhenzeilen ignorieren ihn. Alle Tage abw\u00e4hlen, um eine Zeile zu behalten, ohne sie laufen zu lassen.","tr":"De\u011fer, sunucu oranlar\u0131n\u0131n \u00fcst\u00fcndeki y\u00fczdedir (50 = +%50 tecr\u00fcbe); sand\u0131k sat\u0131rlar\u0131 bunu yok sayar. Bir sat\u0131r\u0131 \u00e7al\u0131\u015ft\u0131rmadan saklamak i\u00e7in t\u00fcm g\u00fcnlerin i\u015faretini kald\u0131r\u0131n."},
 "ev_now_minutes":{"en":"for minutes","pl":"na minut","de":"f\u00fcr Minuten","tr":"dakika boyunca"},
 "ev_now_value": {"en":"+%","pl":"+%","de":"+%","tr":"+%"},
 "ev_now_go":    {"en":"Activate now","pl":"Aktywuj teraz","de":"Jetzt aktivieren","tr":"\u015eimdi etkinle\u015ftir"},
 "ev_now_started":{"en":"Event switched on for %d minutes; the chat is told within seconds.","pl":"Event w\u0142\u0105czony na %d minut; czat dowie si\u0119 w kilka sekund.","de":"Event f\u00fcr %d Minuten eingeschaltet; der Chat erf\u00e4hrt es binnen Sekunden.","tr":"Etkinlik %d dakikal\u0131\u011f\u0131na a\u00e7\u0131ld\u0131; sohbet saniyeler i\u00e7inde \u00f6\u011frenir."},
 "ev_stop":      {"en":"End now","pl":"Zako\u0144cz","de":"Jetzt beenden","tr":"\u015eimdi bitir"},
 "ev_stopped":   {"en":"The event switched on by hand is over.","pl":"Event w\u0142\u0105czony r\u0119cznie zako\u0144czony.","de":"Das von Hand eingeschaltete Event ist beendet.","tr":"Elle a\u00e7\u0131lan etkinlik bitti."},
 "ev_kind_tanaka": {"en":"Pirate Tanaka","pl":"Pirat Tanaka","de":"Pirat Tanaka","tr":"Korsan Tanaka"},
 "ev_kind_zuo": {"en":"Zuo: Metin rain","pl":"Zuo: deszcz metin\u00f3w","de":"Zuo: Metinregen","tr":"Zuo: Metin ya\u011fmuru"},
 "ev_world_note": {"en":"Pirate Tanaka runs from whoever hits him, scatters yang as he loses health and leaves his ear to the one who beat him; Yonah in the first village gives a Purple Ebony Chest for an ear. Zuo drops a wave of Metin stones of the map's level every five minutes (the chat gives the coordinates) and from half the event on calls bosses. Bots come too; a war, the Demon Tower, a raid or a player's party come first for them.","pl":"Pirat Tanaka ucieka przed ka\u017cdym, kto go bije, sypie yang, gdy traci \u017cycie, i zostawia ucho temu, kto go pokona\u0142; Yonah w pierwszej wiosce daje za ucho Fioletow\u0105 Hebanow\u0105 Szkatu\u0142k\u0119. Zuo co pi\u0119\u0107 minut zrzuca fal\u0119 metin\u00f3w poziomu mapy (czat podaje koordynaty), a od po\u0142owy eventu przyzywa boss\u00f3w. Przychodz\u0105 te\u017c boty; wojna, Wie\u017ca Demon\u00f3w, rajd albo party gracza maj\u0105 dla nich pierwsze\u0144stwo.","de":"Pirat Tanaka flieht vor jedem, der ihn schl\u00e4gt, verstreut Yang, wenn er Leben verliert, und l\u00e4sst dem Sieger sein Ohr; Yonah im ersten Dorf gibt f\u00fcr ein Ohr eine Violette Ebenholztruhe. Zuo wirft alle f\u00fcnf Minuten eine Welle Metinsteine der Kartenstufe ab (der Chat nennt die Koordinaten) und ruft ab der H\u00e4lfte des Events Bosse. Auch Bots kommen; Krieg, Teufelsturm, Raid oder die Gruppe eines Spielers gehen f\u00fcr sie vor.","tr":"Korsan Tanaka kendisine vuran herkesten ka\u00e7ar, can kaybettik\u00e7e yang sa\u00e7ar ve onu yenene kula\u011f\u0131n\u0131 b\u0131rak\u0131r; ilk k\u00f6ydeki Yonah bir kulak kar\u015f\u0131l\u0131\u011f\u0131nda Mor Abanoz Sand\u0131k verir. Zuo her be\u015f dakikada haritan\u0131n seviyesinde bir Metin ta\u015f\u0131 dalgas\u0131 d\u00fc\u015f\u00fcr\u00fcr (sohbet koordinatlar\u0131 verir) ve etkinli\u011fin yar\u0131s\u0131ndan itibaren boss \u00e7a\u011f\u0131r\u0131r. Botlar da gelir; sava\u015f, \u015eeytan Kulesi, bask\u0131n veya bir oyuncunun grubu onlar i\u00e7in \u00f6nce gelir."},
 "ev_world_value_help": {"en":"For Pirate Tanaka the value is how many pirates roam at once (1-20, 0 = 3); for Zuo how many Metin stones fall in a wave (1-30, 0 = 8). The map column counts for these two only.","pl":"Dla Pirata Tanaki warto\u015b\u0107 to liczba pirat\u00f3w naraz (1-20, 0 = 3); dla Zuo liczba metin\u00f3w w fali (1-30, 0 = 8). Kolumna mapy liczy si\u0119 tylko dla tych dw\u00f3ch.","de":"Bei Pirat Tanaka ist der Wert die Zahl der Piraten gleichzeitig (1-20, 0 = 3); bei Zuo die Zahl der Metinsteine je Welle (1-30, 0 = 8). Die Kartenspalte gilt nur f\u00fcr diese beiden.","tr":"Korsan Tanaka i\u00e7in de\u011fer ayn\u0131 anda dola\u015fan korsan say\u0131s\u0131d\u0131r (1-20, 0 = 3); Zuo i\u00e7in bir dalgadaki Metin ta\u015f\u0131 say\u0131s\u0131 (1-30, 0 = 8). Harita s\u00fctunu yaln\u0131zca bu ikisi i\u00e7in ge\u00e7erlidir."},
 "ev_col_map": {"en":"Map (Tanaka, Zuo)","pl":"Mapa (Tanaka, Zuo)","de":"Karte (Tanaka, Zuo)","tr":"Harita (Tanaka, Zuo)"},
 "ev_map_auto": {"en":"the event picks","pl":"wybiera event","de":"das Event w\u00e4hlt","tr":"etkinlik se\u00e7er"},
 "ev_now_tanaka": {"en":"pirates at once","pl":"pirat\u00f3w naraz","de":"Piraten gleichzeitig","tr":"ayn\u0131 anda korsan"},
 "ev_now_zuo": {"en":"stones a wave","pl":"metin\u00f3w w fali","de":"Steine je Welle","tr":"dalga ba\u015f\u0131na ta\u015f"},
 "ev_world_alive": {"en":"standing","pl":"na mapie","de":"auf der Karte","tr":"haritada"},
 "ev_world_killed": {"en":"beaten","pl":"pokonane","de":"besiegt","tr":"yenilen"},
 "ev_world_bots": {"en":"bots taking part","pl":"boty w evencie","de":"Bots dabei","tr":"kat\u0131lan botlar"},
 "ev_phase_stones": {"en":"stones falling","pl":"spadaj\u0105 metiny","de":"Steine fallen","tr":"ta\u015flar d\u00fc\u015f\u00fcyor"},
 "ev_phase_bosses": {"en":"bosses","pl":"bossowie","de":"Bosse","tr":"bosslar"},
 "ev_bots_title": {"en":"Bots at Tanaka and Zuo","pl":"Boty na Tanace i Zuo","de":"Bots bei Tanaka und Zuo","tr":"Tanaka ve Zuo'da botlar"},
 "ev_bots_help": {"en":"What share of the bots that could come - their level suits the map and nothing more pressing holds them (a war, the Demon Tower, a raid, a duel, a player's party) - takes part. 0 keeps every bot out; the same bots answer every time, and a few chase each pirate.","pl":"Jaka cz\u0119\u015b\u0107 bot\u00f3w, kt\u00f3re mog\u0142yby przyj\u015b\u0107 - poziom pasuje do mapy i nic wa\u017cniejszego ich nie trzyma (wojna, Wie\u017ca Demon\u00f3w, rajd, pojedynek, party gracza) - bierze udzia\u0142. 0 = boty nie bior\u0105 udzia\u0142u; przychodz\u0105 za ka\u017cdym razem te same boty, a ka\u017cdego pirata goni kilka.","de":"Welcher Anteil der Bots, die kommen k\u00f6nnten - ihre Stufe passt zur Karte und nichts Wichtigeres h\u00e4lt sie (Krieg, Teufelsturm, Raid, Duell, Gruppe eines Spielers) - teilnimmt. 0 h\u00e4lt alle Bots fern; es kommen jedes Mal dieselben Bots, und jeden Piraten jagen einige.","tr":"Gelebilecek botlar\u0131n - seviyesi haritaya uyan ve daha \u00f6nemli bir \u015fey taraf\u0131ndan tutulmayan (sava\u015f, \u015eeytan Kulesi, bask\u0131n, d\u00fcello, bir oyuncunun grubu) - ne kadar\u0131n\u0131n kat\u0131ld\u0131\u011f\u0131. 0 t\u00fcm botlar\u0131 d\u0131\u015far\u0131da tutar; her seferinde ayn\u0131 botlar gelir ve her korsan\u0131 birka\u00e7\u0131 kovalar."},
 "ev_bots_save": {"en":"Save","pl":"Zapisz","de":"Speichern","tr":"Kaydet"},
 "ev_bots_saved": {"en":"Saved; the game core reads it within five seconds.","pl":"Zapisano; rdze\u0144 gry odczyta to w pi\u0119\u0107 sekund.","de":"Gespeichert; der Spielkern liest es binnen f\u00fcnf Sekunden.","tr":"Kaydedildi; oyun \u00e7ekirde\u011fi be\u015f saniyede okur."},
 "ev_world_many": {"en":"Events on different maps run at the same time; starting one on a map that already has an event restarts it there.","pl":"Eventy na r\u00f3\u017cnych mapach trwaj\u0105 jednocze\u015bnie; uruchomienie na mapie, na kt\u00f3rej event ju\u017c trwa, zaczyna go tam od nowa.","de":"Events auf verschiedenen Karten laufen gleichzeitig; ein Start auf einer Karte, auf der schon eines l\u00e4uft, startet es dort neu.","tr":"Farkl\u0131 haritalardaki etkinlikler ayn\u0131 anda s\u00fcrer; zaten etkinlik olan bir haritada ba\u015flatmak onu orada yeniden ba\u015flat\u0131r."},
 "ai_chest_off":  {"en":"Turn the Moonlight chest drop off","pl":"Wyłącz drop Szkatułek Blasku Księżyca","de":"Mondschein-Truhen nicht fallen lassen","tr":"Ay Işığı Sandığı düşmesini kapat"},
 "ai_chest_off_help":{"en":"Ticked and saved, no chest drops from monsters or Metin stones (both figures go to 0‰); the sliders keep what you set and come back when you untick. Unticking is not the same as chests falling: outside a chest event none drops whatever these say, so what brings them back is a window on the Events page. Applies within five seconds.",
                  "pl":"Zaznaczone i zapisane: żadna szkatułka nie wypada z potworów ani z Metinów (obie wartości idą na 0‰); suwaki pamiętają Twoje ustawienie i wracają po odznaczeniu. Odznaczenie to jeszcze nie szkatułki: poza eventem szkatułek nie wypada żadna, cokolwiek mówią te suwaki — żeby leciały, potrzebne jest okno na stronie Eventy. Działa w pięć sekund.",
                  "de":"Angehakt und gespeichert fällt keine Truhe mehr von Monstern oder Metins (beide Werte auf 0‰); die Regler behalten deine Werte und kommen nach dem Abhaken zurück. Gilt binnen fünf Sekunden.",
                  "tr":"İşaretleyip kaydedince canavarlardan ve Metinlerden sandık düşmez (iki değer de 0‰ olur); kaydırıcılar ayarını hatırlar ve işareti kaldırınca geri gelir. Beş saniye içinde uygulanır."},
 "ai_chest_kill": {"en":"per monster kill","pl":"z zabitego potwora","de":"pro getötetem Monster","tr":"öldürülen canavar başına"},
 "ai_chest_stone":{"en":"per broken Metin stone","pl":"z rozbitego Metina","de":"pro zerstörtem Metin","tr":"kırılan Metin başına"},
 "ai_chest_note": {"en":"Saving writes both values; until then the game keeps what .env says.",
                   "pl":"Zapis ustawia obie wartości; do tego czasu gra trzyma to, co mówi .env.",
                   "de":"Speichern setzt beide Werte; bis dahin gilt, was .env sagt.",
                   "tr":"Kaydetmek iki değeri de yazar; o zamana kadar oyun .env'deki değeri kullanır."},
 "ai_live":     {"en":"Saved. The game core reads it within five seconds — no restart, nobody is disconnected. Some settings reach a bot at its next decision (see the text under each).",
                 "pl":"Zapisano. Rdzeń gry odczyta to w ciągu pięciu sekund — bez restartu, nikt nie zostaje rozłączony. Niektóre ustawienia docierają do bota przy jego następnej decyzji (opis pod każdym).",
                 "de":"Gespeichert. Der Spielkern liest das binnen fünf Sekunden — kein Neustart, niemand fliegt raus. Manche Einstellungen erreichen einen Bot erst bei seiner nächsten Entscheidung (siehe den Text darunter).",
                 "tr":"Kaydedildi. Oyun çekirdeği bunu beş saniye içinde okur — yeniden başlatma yok, kimse düşmez. Bazı ayarlar bota bir sonraki kararında ulaşır (her birinin altındaki metne bakın)."},
 "ai_failed":    {"en":"Could not write the file \u2014 the shared spool directory is not mounted in this container.",
                  "pl":"Nie udało się zapisać pliku \u2014 współdzielony katalog spool nie jest podmontowany w tym kontenerze.",
                  "de":"Datei konnte nicht geschrieben werden \u2014 das gemeinsame Spool-Verzeichnis ist in diesem Container nicht eingebunden.",
                  "tr":"Dosya yazılamadı \u2014 paylaşılan spool dizini bu kapsayıcıda bağlı değil."},
 "ai_save":      {"en":"Save","pl":"Zapisz","de":"Speichern","tr":"Kaydet"},
 "ai_items_open": {"en":"📦 What the bots may sell","pl":"📦 Co boty mogą sprzedawać"},
 "ai_items_nav":  {"en":"📦 Item policy: merchant or stall","pl":"📦 Polityka przedmiotów: handlarz czy stragan"},
 "ai_items_intro":{"en":"One line per item: the item number (vnum) or a whole type (type:19), a space or tab, and one word. keep - never leaves the bag; stall - counter goods, ahead of everything else, but at most three lines of one item on a counter (marbles: three, each of another monster) while the rest waits in the bag; merchant - sold to the NPC merchant on the next town visit; drop - thrown away at the merchant visit without a sale. Anything not listed here follows the bots' own rules. Saved, it reaches every bot within five seconds.",
                  "pl":"Jedna linia na przedmiot: numer przedmiotu (vnum) albo cały typ (type:19), spacja lub tabulator i jedno słowo. keep (zostaw) - nigdy nie opuszcza plecaka; stall (stragan) - towar na ladę, przed wszystkim innym, ale najwyżej 3 linie jednego przedmiotu na ladzie (marmury: 3, każdy innego potwora), a reszta czeka w plecaku; merchant (handlarz) - sprzedany handlarzowi NPC przy najbliższej wizycie w mieście; drop (wyrzuc) - wyrzucony przy wizycie u handlarza, bez sprzedaży. Czego tu nie ma, podlega własnym regułom botów. Po zapisie dociera do każdego bota w ciągu pięciu sekund."},
 "ai_items_format":{"en":"Item numbers: the item search on the give-item page shows them; item types: 5 materials, 18 quest items, 19 polymorph marbles, 17 skill books, 3 usable items (scrolls, stones). A # starts a comment.",
                  "pl":"Numery przedmiotów pokazuje wyszukiwarka na stronie nadawania przedmiotów; typy: 5 materiały, 18 przedmioty questowe, 19 marmury polimorfii, 17 księgi, 3 przedmioty użytkowe (zwoje, kamienie). Znak # zaczyna komentarz."},
 "ai_items_bad":  {"en":"Not saved: line(s) {n} are not '<vnum or type:N> <keep|stall|merchant|drop>'.",
                  "pl":"Nie zapisano: linie {n} nie mają postaci '<vnum albo type:N> <keep|stall|merchant|drop>'."},
 "ai_items_live": {"en":"Saved. The bots read the file within five seconds.","pl":"Zapisano. Boty czytają plik w ciągu pięciu sekund."},
 "ai_reset":    {"en":"Goals back to 100", "pl":"Cele z powrotem na 100", "de":"Ziele zurück auf 100", "tr":"Hedefleri 100'e döndür"},
 "ai_every_chance": {"en":"every chance (as built)", "pl":"każda okazja (jak w grze)", "de":"jede Gelegenheit (wie gebaut)", "tr":"her fırsat (yapıldığı gibi)"},
 "ai_rare":      {"en":"rarely","pl":"rzadko","de":"selten","tr":"nadiren"},
 "ai_often":     {"en":"often","pl":"często","de":"oft","tr":"sık"},
 "ai_neutral":   {"en":"as built","pl":"jak w grze","de":"wie gebaut","tr":"yapıldığı gibi"},

 "aiw_RESTOCK":  {"en":"Buying potions","pl":"Kupowanie mikstur","de":"Tränke kaufen","tr":"İksir alma"},
 "aih_RESTOCK": {"en":"When a bot goes back to town for potions: at 100 when it has fewer than 300 red or 200 blue left (30/20 up to level 10). 25 waits for a quarter of that, 250 goes at two and a half times as many - at most 480/360, as the merchant fills the belt to 800/600. Applies at once.",
                 "pl":"Kiedy bot wraca do miasta po mikstury: przy 100, gdy zostaje mu mniej niż 300 czerwonych lub 200 niebieskich (do 10. poziomu 30/20). 25 czeka do ćwiartki tego, 250 idzie już przy dwa i pół raza tylu - najwyżej przy 480/360, bo handlarz dopełnia do 800/600. Działa od razu.",
                 "de":"Wann ein Bot für Tränke in die Stadt geht: bei 100, wenn er weniger als 300 rote oder 200 blaue hat (bis Stufe 10: 30/20). 25 wartet auf ein Viertel davon, 250 geht schon beim Zweieinhalbfachen - höchstens bei 480/360, weil der Händler auf 800/600 auffüllt. Wirkt sofort.",
                 "tr":"Botun iksir için kasabaya ne zaman döndüğü: 100'de 300 kırmızıdan veya 200 maviden azı kaldığında (10. seviyeye kadar 30/20). 25 bunun dörtte birini bekler, 250 iki buçuk katında gider - tüccar 800/600'e tamamladığı için en fazla 480/360'ta. Hemen uygulanır."},
 "aiw_REFINE":   {"en":"The blacksmith","pl":"Kowal","de":"Der Schmied","tr":"Demirci"},
 "aih_REFINE":  {"en":"Trips to the blacksmith to upgrade. At 100 a bot goes at every chance, as the game was built, so the slider stops at 100: below it a share of the bots leaves the blacksmith alone for half an hour at a time - at 50 a bot upgrades in every other half hour. A bot already standing at the anvil upgrades what it can, and a gambler's session does not count. Applies at once.",
                 "pl":"Wyprawy do kowala po ulepszenie. Przy 100 bot idzie przy każdej okazji, jak w grze, dlatego suwak kończy się na 100: poniżej część botów omija kowala po pół godziny - przy 50 bot ulepsza w co drugiej półgodzinie. Bot, który już stoi przy kowadle, ulepsza, co może, a sesja hazardzisty się nie liczy. Działa od razu.",
                 "de":"Gänge zum Schmied zum Aufwerten. Bei 100 geht ein Bot bei jeder Gelegenheit, wie im Spiel, darum endet der Regler bei 100: darunter lässt ein Teil der Bots den Schmied jeweils eine halbe Stunde links liegen - bei 50 wertet ein Bot in jeder zweiten halben Stunde auf. Ein Bot, der schon am Amboss steht, wertet auf, was er kann, und die Sitzung eines Glücksspielers zählt nicht. Wirkt sofort.",
                 "tr":"Yükseltme için demirciye gidişler. 100'de bot oyundaki gibi her fırsatta gider, bu yüzden kaydırıcı 100'de biter: altında botların bir kısmı demirciyi yarım saatliğine bırakır - 50'de bir bot her iki yarım saatin birinde yükseltir. Örsün başında duran bot yapabildiğini yükseltir, kumarbazın oturumu sayılmaz. Hemen uygulanır."},
 "aiw_SKILL":    {"en":"Skill books","pl":"Księgi umiejętności","de":"Skillbücher","tr":"Yetenek kitapları"},
 "aih_SKILL":   {"en":"Reading skill books. At 100 a bot reads at every chance, as the game was built, so the slider stops at 100: below it a share of the bots leaves its books in the bag for half an hour at a time - at 50 a bot reads in every other half hour. The game's own wait between two books is set elsewhere. Applies at once.",
                 "pl":"Czytanie ksiąg umiejętności. Przy 100 bot czyta przy każdej okazji, jak w grze, dlatego suwak kończy się na 100: poniżej część botów zostawia księgi w plecaku po pół godziny - przy 50 bot czyta w co drugiej półgodzinie. Przerwę samej gry między dwiema księgami ustawia się osobno. Działa od razu.",
                 "de":"Fertigkeitsbücher lesen. Bei 100 liest ein Bot bei jeder Gelegenheit, wie im Spiel, darum endet der Regler bei 100: darunter lässt ein Teil der Bots seine Bücher jeweils eine halbe Stunde im Beutel - bei 50 liest ein Bot in jeder zweiten halben Stunde. Die eigene Wartezeit des Spiels zwischen zwei Büchern wird anderswo eingestellt. Wirkt sofort.",
                 "tr":"Yetenek kitabı okumak. 100'de bot oyundaki gibi her fırsatta okur, bu yüzden kaydırıcı 100'de biter: altında botların bir kısmı kitaplarını yarım saatliğine çantada bırakır - 50'de bir bot her iki yarım saatin birinde okur. Oyunun iki kitap arasındaki kendi beklemesi başka yerde ayarlanır. Hemen uygulanır."},
 "aiw_HORSE":    {"en":"The horse","pl":"Koń","de":"Das Pferd","tr":"At"},
 "aih_HORSE":   {"en":"The trip to the Monkey Dungeon for horse medals: how many of the bots that could go set out. The stable itself (the horse's levels and books) does not listen to it, nor do the medal droppers and bots over level 64. Applies at the next travel check.",
                 "pl":"Wyprawy do Lochu Małp po medale konne: ilu botów spośród tych, które mogą, wyrusza. Sama stajnia (poziomy konia i księgi) nie słucha tego suwaka, tak samo dropperzy medali i boty powyżej 64 poziomu. Działa przy następnym sprawdzeniu podróży.",
                 "de":"Die Reise ins Affenverlies für Pferdemedaillen: wie viele der Bots, die könnten, aufbrechen. Der Stall selbst (Pferdestufen und Bücher) hört nicht darauf, ebenso wenig die Medaillen-Dropper und Bots über Stufe 64. Wirkt bei der nächsten Reiseprüfung.",
                 "tr":"At madalyaları için Maymun Zindanı'na yolculuk: gidebilecek botlardan kaçının yola çıktığı. Ahırın kendisi (atın seviyeleri ve kitapları) bunu dinlemez; madalya dropper'ları ve 64. seviyenin üstündeki botlar da dinlemez. Bir sonraki yolculuk kontrolünde uygulanır."},
 "aiw_BIOLOG":   {"en":"The Biologist","pl":"Biolog","de":"Der Biologe","tr":"Biyolog"},
 "aih_BIOLOG":  {"en":"Hunting the Biologist's specimens: the trip to the mission monster's map and the hunt there. At 100 every bot with an open mission does it, as the game was built, so the slider stops at 100: below it a share of the bots hunts whatever its map offers for half an hour at a time. What a bot already carries is handed in all the same. Applies at once.",
                 "pl":"Polowanie na okazy dla Biologa: podróż na mapę potwora z misji i polowanie tam. Przy 100 robi to każdy bot z otwartą misją, jak w grze, dlatego suwak kończy się na 100: poniżej część botów po pół godziny bije to, co jest na jego mapie. To, co bot już niesie, i tak oddaje. Działa od razu.",
                 "de":"Die Jagd nach den Proben des Biologen: die Reise zur Karte des Missionsmonsters und die Jagd dort. Bei 100 tut das jeder Bot mit offener Mission, wie im Spiel, darum endet der Regler bei 100: darunter jagt ein Teil der Bots jeweils eine halbe Stunde, was seine Karte bietet. Was ein Bot schon trägt, gibt er trotzdem ab. Wirkt sofort.",
                 "tr":"Biyolog'un örneklerini avlamak: görev canavarının haritasına yolculuk ve orada av. 100'de açık görevi olan her bot bunu oyundaki gibi yapar, bu yüzden kaydırıcı 100'de biter: altında botların bir kısmı yarım saatliğine haritasında ne varsa onu avlar. Botun zaten taşıdığını yine de teslim eder. Hemen uygulanır."},
 "aiw_METIN":    {"en":"Metin stones","pl":"Kamienie Metin","de":"Metinsteine","tr":"Metin taşları"},
 "aih_METIN":   {"en":"Metin expeditions: once an hour every bot from level 15 rolls whether the next half hour goes to the stones - 25% at 100 (twice that on a battle horse), multiplied by the slider. Metin hunters by role always hunt stones and do not roll. A change reaches a bot at its next roll, within about an hour.",
                 "pl":"Wyprawy na metiny: raz na godzinę każdy bot od 15 poziomu losuje, czy najbliższe pół godziny spędzi na kamieniach - 25% przy 100 (na koniu bojowym dwa razy tyle), razy suwak. Łowcy metinów z roli polują na nie zawsze i nie losują. Zmiana dociera do bota przy jego następnym losowaniu, w ciągu mniej więcej godziny.",
                 "de":"Metin-Expeditionen: einmal pro Stunde würfelt jeder Bot ab Stufe 15, ob die nächste halbe Stunde den Steinen gehört - 25% bei 100 (auf einem Kampfpferd doppelt so viel), mal dem Regler. Metinjäger von Rolle jagen immer Steine und würfeln nicht. Eine Änderung erreicht einen Bot bei seinem nächsten Wurf, binnen etwa einer Stunde.",
                 "tr":"Metin seferleri: saatte bir, 15. seviyeden itibaren her bot sonraki yarım saati taşlara ayırıp ayırmayacağına zar atar - 100'de %25 (savaş atında iki katı), kaydırıcıyla çarpılır. Rolü metin avcısı olan botlar her zaman taş avlar ve zar atmaz. Değişiklik bota bir sonraki zarında, yaklaşık bir saat içinde ulaşır."},
 "aiw_PARTY":    {"en":"Parties","pl":"Grupy (PT)","de":"Gruppen","tr":"Gruplar"},
 "aih_PARTY":   {"en":"The share of bots that may join a party: in the villages 20% at 100 (5% at 25, 50% at 250); on the frontier (Orc Valley, the desert, Sohan, the Spider Dungeons) every bot already may at 100, so there the slider only works downwards (25% at 25). A bot left out leaves its party within a minute; new parties form in 1-3 minutes.",
                 "pl":"Jaka część botów może wchodzić do grup: w wioskach 20% przy 100 (5% przy 25, 50% przy 250); na froncie (Dolina Orków, pustynia, Sohan, Lochy Pająków) przy 100 już każdy bot, więc tam suwak działa tylko w dół (25% przy 25). Bot, który wypada z puli, opuszcza grupę w ciągu minuty; nowe grupy tworzą się w 1–3 minuty.",
                 "de":"Welcher Anteil der Bots Gruppen beitreten darf: in den Dörfern 20% bei 100 (5% bei 25, 50% bei 250); an der Front (Orktal, Wüste, Sohan, Spinnenverliese) darf es bei 100 schon jeder Bot, dort wirkt der Regler also nur nach unten (25% bei 25). Wer herausfällt, verlässt seine Gruppe binnen einer Minute; neue Gruppen bilden sich in 1-3 Minuten.",
                 "tr":"Botların ne kadarının gruba girebileceği: köylerde 100'de %20 (25'te %5, 250'de %50); cephede (Ork Vadisi, çöl, Sohan, Örümcek Zindanları) 100'de zaten her bot girebilir, yani orada kaydırıcı yalnızca aşağı doğru çalışır (25'te %25). Dışarıda kalan bot bir dakika içinde grubundan ayrılır; yeni gruplar 1-3 dakikada kurulur."},
 "aiw_HUNTING":  {"en":"Hunting missions","pl":"Misje polowania","de":"Jagdmissionen","tr":"Av görevleri"},
 "aih_HUNTING": {"en":"The level-up hunt mission (r40250 only): hunting the monster the mission names. At 100 every bot with a mission does it, so the slider stops at 100: below it a share of the bots hunts whatever its map offers for half an hour at a time. Applies at once.",
                 "pl":"Misja polowania na awans (tylko r40250): bicie potwora, którego wskazuje misja. Przy 100 robi to każdy bot z misją, dlatego suwak kończy się na 100: poniżej część botów po pół godziny bije to, co jest na jego mapie. Działa od razu.",
                 "de":"Die Aufstiegsjagd (nur r40250): das Monster jagen, das die Mission nennt. Bei 100 tut das jeder Bot mit Mission, darum endet der Regler bei 100: darunter jagt ein Teil der Bots jeweils eine halbe Stunde, was seine Karte bietet. Wirkt sofort.",
                 "tr":"Seviye avı görevi (yalnızca r40250): görevin gösterdiği canavarı avlamak. 100'de görevi olan her bot bunu yapar, bu yüzden kaydırıcı 100'de biter: altında botların bir kısmı yarım saatliğine haritasında ne varsa onu avlar. Hemen uygulanır."},
 "aiw_LEVEL":    {"en":"Plain grinding","pl":"Zwykłe bicie potworów","de":"Schlichtes Grinden","tr":"Düz grind"},
 "aih_LEVEL":   {"en":"Plain grinding, what a bot does when nothing else is asking for it. Raised, the level goal wins over the other goals, and a grinder with that goal pulls several monsters at once and a third of them drink attack-speed potions; the errands themselves (potions, blacksmith, books, the Biologist) have their own sliders and this one does not hold them back. Lowering it changes nothing.",
                 "pl":"Zwykłe bicie potworów, czyli to, co bot robi, gdy nic innego się nie dopomina. Podniesiony sprawia, że cel „poziom” wygrywa z innymi celami, a grinder z tym celem przyciąga kilka potworów naraz i co trzeci pije mikstury szybkości ataku; same sprawunki (mikstury, kowal, księgi, Biolog) mają własne suwaki i ten ich nie wstrzymuje. Obniżenie niczego nie zmienia.",
                 "de":"Schlichtes Grinden, was ein Bot tut, wenn nichts anderes ruft. Erhöht gewinnt das Stufenziel gegen die anderen Ziele, und ein Grinder mit diesem Ziel zieht mehrere Monster auf einmal, und ein Drittel trinkt Angriffstempo-Tränke; die Besorgungen selbst (Tränke, Schmied, Bücher, der Biologe) haben eigene Regler, und dieser hält sie nicht auf. Senken ändert nichts.",
                 "tr":"Düz grind, başka bir şey çağırmadığında botun yaptığı şey. Yükseltilince seviye hedefi diğer hedefleri geçer ve bu hedefteki grinder aynı anda birkaç canavar çeker, üçte biri saldırı hızı iksiri içer; işlerin kendisi (iksir, demirci, kitap, Biyolog) kendi kaydırıcılarına sahiptir ve bu onları durdurmaz. Düşürmek bir şey değiştirmez."},
 "aiw_FISHING":  {"en":"Fishing","pl":"Wędkowanie","de":"Angeln","tr":"Balık tutma"},
 "aih_FISHING": {"en":"How many bots fish. With the personalities on (the default) a bot of level 30 and up, outside a party, rolls again every half hour against the slider and its mood - most often a bot in a poor mood; with them off a fixed share of the bots fishes (collectors more often). Raising it works within seconds; lowering it lets running sessions finish first (up to an hour). A bot sent to the water after five deaths in one place (the capitulation) goes whatever the slider says.",
                 "pl":"Ilu botów łowi ryby. Przy włączonych osobowościach (domyślnie) bot od 30 poziomu, poza grupą, losuje od nowa co pół godziny według suwaka i swojego nastroju - najczęściej bot w słabym nastroju; przy wyłączonych łowi stały udział botów (zbieracze częściej). Podniesienie działa w kilka sekund; obniżenie pozwala dokończyć trwające sesje (do godziny). Bot wysłany nad wodę po pięciu śmierciach w jednym miejscu (kapitulacja) idzie niezależnie od suwaka.",
                 "de":"Wie viele Bots angeln. Mit eingeschalteten Persönlichkeiten (Standard) würfelt ein Bot ab Stufe 30, außerhalb einer Gruppe, jede halbe Stunde neu gegen den Regler und seine Stimmung - am häufigsten ein Bot in schlechter Stimmung; ohne sie angelt ein fester Anteil der Bots (Sammler öfter). Erhöhen wirkt binnen Sekunden; Senken lässt laufende Sitzungen erst zu Ende gehen (bis zu einer Stunde). Ein Bot, der nach fünf Toden an einem Ort ans Wasser geschickt wird (die Kapitulation), geht unabhängig vom Regler.",
                 "tr":"Kaç botun balık tuttuğu. Kişilikler açıkken (varsayılan) 30. seviye ve üstündeki, grupta olmayan bot yarım saatte bir kaydırıcıya ve ruh haline göre yeniden zar atar - en sık kötü ruh halindeki bot; kapalıyken botların sabit bir payı balık tutar (toplayıcılar daha sık). Yükseltmek saniyeler içinde işler; düşürmek süren oturumların önce bitmesine izin verir (bir saate kadar). Bir yerde beş kez öldükten sonra suya gönderilen bot (kapitülasyon) kaydırıcıdan bağımsız gider."},
 "aiw_TRADE":    {"en":"Market stalls","pl":"Stragany","de":"Marktstände","tr":"Pazar tezgahları"},
 "aiw_MINING":   {"en":"Mining","pl":"Górnictwo","de":"Bergbau","tr":"Madencilik"},
 "aih_MINING":  {"en":"How many bots dig ore and how long they rest from the veins. With the personalities on (the default) every bot of level 30 with a pickaxe digs a vein in sight - under 100 the slider closes the veins to a share of them, half an hour at a time; with them off a fixed share of the bots mines (collectors more often). The rest between two sessions shrinks as the slider goes up and grows as it goes down.",
                 "pl":"Ilu botów kopie rudę i jak długo odpoczywa od żył. Przy włączonych osobowościach (domyślnie) każdy bot od 30 poziomu z kilofem kopie żyłę w zasięgu wzroku - poniżej 100 suwak zamyka żyły dla części z nich, na pół godziny; przy wyłączonych kopie stały udział botów (zbieracze częściej). Przerwa między sesjami skraca się, gdy suwak idzie w górę, i wydłuża, gdy idzie w dół.",
                 "de":"Wie viele Bots Erz abbauen und wie lange sie von den Adern ruhen. Mit eingeschalteten Persönlichkeiten (Standard) baut jeder Bot ab Stufe 30 mit einer Spitzhacke eine Ader in Sichtweite ab - unter 100 schließt der Regler die Adern für einen Teil von ihnen, jeweils eine halbe Stunde; ohne sie baut ein fester Anteil der Bots ab (Sammler öfter). Die Pause zwischen zwei Sitzungen wird kürzer, wenn der Regler steigt, und länger, wenn er sinkt.",
                 "tr":"Kaç botun cevher kazdığı ve damarlardan ne kadar dinlendiği. Kişilikler açıkken (varsayılan) kazması olan 30. seviye ve üstündeki her bot görüş alanındaki bir damarı kazar - 100 altında kaydırıcı damarları bir kısmına yarım saatliğine kapatır; kapalıyken botların sabit bir payı kazar (toplayıcılar daha sık). İki oturum arasındaki mola kaydırıcı yükseldikçe kısalır, düştükçe uzar."},
 "aiw_HERB":     {"en":"Herbalism","pl":"Zielarstwo","de":"Kräuterkunde","tr":"Bitkicilik"},
 "aih_HERB":    {"en":"How many bots pick herbs and brew at Baek-Go, and how often. Picking is an activity like fishing and mining: every half hour each bot from level 15 (outside a party, not a dropper) rolls against a share - 8% at 100, 25% for collectors, a little more once it owns a knife - stretched or shrunk by the slider; then it buys the Herbalist's Knife, picks the herb bushes of its map for 12-25 minutes as a Herbalist (Zielarz), brews what it picked at Baek-Go and goes back to its normal play (levelling, hunting, quests, gear, market). The rest between two sessions (40-90 minutes) shrinks as the slider goes up. The brewed potions over the bot's own reserve go on its counter at the price list's prices. With the personalities on a Conqueror from level 45 also brews the herbs he drops; under 100 the board and the bushes are closed to a share of the bots, half an hour at a time, and over 100 a share of the bots from level 15 brews too (all of them at 250).",
                 "pl":"Ilu botów zbiera zioła i warzy mikstury u Baek-Go, i jak często. Zbieranie ziół to zajęcie jak wędkarstwo i górnictwo: co pół godziny każdy bot od 15 poziomu (poza grupą, nie dropper) losuje według udziału - 8% przy 100, 25% dla zbieraczy, trochę więcej, gdy ma już nożyk - rozciągniętego lub zmniejszonego suwakiem; wtedy kupuje nożyk zielarza, przez 12-25 minut jako Zielarz zbiera krzaki ziół na swojej mapie, warzy zebrane zioła u Baek-Go i wraca do normalnej gry (expienie, polowanie, misje, ekwipunek, handel). Przerwa między sesjami (40-90 minut) skraca się, gdy suwak idzie w górę. Uwarzone mikstury ponad własny zapas bota trafiają na jego sklep po cenach z cennika. Przy włączonych osobowościach Zdobywca od 45 poziomu też warzy zioła z dropu; poniżej 100 stół i krzaki są zamknięte dla części botów, na pół godziny, a powyżej 100 warzy też część botów od 15 poziomu (przy 250 wszystkie).",
                 "de":"Wie viele Bots Kräuter sammeln und bei Baek-Go brauen, und wie oft. Kräutersammeln ist eine Beschäftigung wie Angeln und Bergbau: jede halbe Stunde würfelt jeder Bot ab Stufe 15 (außerhalb einer Gruppe, kein Dropper) gegen einen Anteil - 8% bei 100, 25% für Sammler, etwas mehr, wenn er schon ein Messer hat -, den der Regler streckt oder verkleinert; dann kauft er das Kräutermesser, sammelt 12-25 Minuten lang als Kräuterkundiger die Kräuterbüsche seiner Karte, braut das Gesammelte bei Baek-Go und spielt danach normal weiter (Leveln, Jagen, Quests, Ausrüstung, Markt). Die Pause zwischen zwei Sitzungen (40-90 Minuten) wird kürzer, je höher der Regler steht. Gebraute Tränke über dem eigenen Vorrat kommen zu den Preisen der Preisliste in seinen Laden. Unter 100 sind Tisch und Büsche für einen Teil der Bots je eine halbe Stunde geschlossen, über 100 braut auch ein Teil der Bots ab Stufe 15 (bei 250 alle).",
                 "tr":"Kaç botun bitki topladığı ve Baek-Go'da iksir yaptığı, ne sıklıkla. Bitki toplamak balıkçılık ve madencilik gibi bir uğraştır: her yarım saatte 15. seviyeden itibaren her bot (grupta değilse, dropper değilse) bir paya karşı zar atar - 100'de %8, toplayıcılar için %25, bıçağı varsa biraz daha - ve kaydırıcı bu payı büyütür ya da küçültür; sonra Bitkici Bıçağını alır, 12-25 dakika boyunca Bitkici olarak haritasındaki bitki çalılarını toplar, topladıklarını Baek-Go'da iksire çevirir ve normal oyununa döner (seviye, avlanma, görevler, ekipman, pazar). İki oturum arasındaki mola (40-90 dakika) kaydırıcı yükseldikçe kısalır. Botun kendi stokunun üzerindeki iksirler fiyat listesindeki fiyatlarla dükkanına konur. 100'ün altında masa ve çalılar botların bir kısmına yarım saatliğine kapanır, 100'ün üstünde 15. seviyeden botların bir kısmı da iksir yapar (250'de hepsi)."},
 "aih_TRADE":   {"en":"How many bots keep a stall. The slider does not touch a Merchant personality, a bot that cannot afford its potions, a full bag, a dropper under bag pressure, nor valuable spares and a gambler's goods (the counter is how they get rid of them). It moves the rest: the surplus-books stall (already every such bot at 100, so it only goes down), the dropper's roll and the one-in-ten. On r40250 standing stalls re-check within five minutes; on the 2.x line an offline stand is never closed early - a lower slider only stops its renewal when its eight hours run out. The status says why each one is open.",
                 "pl":"Ilu botów trzyma stragan. Suwak nie rusza osobowości Handlarz, bota bez yang na mikstury, pełnego plecaka, droppera pod presją plecaka ani cennych zapasowych rzeczy i towaru hazardzisty (lada to sposób, żeby się ich pozbyć). Rusza resztę: stragan z nadmiaru ksiąg (przy 100 ma go już każdy taki bot, więc działa tylko w dół), los droppera i „jeden na dziesięciu”. Na r40250 stojące stragany sprawdzają się ponownie do pięciu minut po zmianie; na linii 2.x sklep offline nie jest zamykany wcześniej - niższy suwak tylko wstrzymuje jego odnowienie po ośmiu godzinach. Status mówi, dlaczego każdy jest otwarty.",
                 "de":"Wie viele Bots einen Stand führen. Der Regler berührt weder die Händler-Persönlichkeit noch einen Bot ohne Yang für Tränke, einen vollen Beutel, einen Dropper unter Beuteldruck oder wertvolle Ersatzstücke und die Ware eines Glücksspielers (der Stand ist ihr Weg, sie loszuwerden). Er bewegt den Rest: den Bücher-Stand (bei 100 schon jeder solche Bot, also nur nach unten), den Dropper-Wurf und den Einen-von-zehn. Auf r40250 prüfen stehende Stände sich binnen fünf Minuten neu; auf der 2.x-Linie wird ein Offline-Laden nie früher geschlossen - ein niedrigerer Regler stoppt nur seine Verlängerung nach acht Stunden. Der Status sagt, warum jeder offen ist.",
                 "tr":"Kaç botun tezgah tuttuğu. Kaydırıcı Tüccar kişiliğine, iksir parası olmayan bota, dolu çantaya, çanta baskısındaki dropper'a, değerli yedek eşyalara ve kumarbazın mallarına dokunmaz (tezgah onlardan kurtulmanın yoludur). Gerisini oynatır: fazla kitap tezgahı (100'de zaten her böyle bot açar, yani yalnızca aşağı iner), dropper zarı ve onda bir. r40250'de açık tezgahlar beş dakika içinde yeniden bakar; 2.x hattında çevrimdışı dükkân asla erken kapanmaz - düşük kaydırıcı yalnızca sekiz saat dolunca yenilenmesini durdurur. Durum her birinin neden açık olduğunu söyler."},
})


# --- Module 6: the weekly season and the all-time records.
T.update({
 "se_nav":       {"en":"🏆 Weekly season","pl":"🏆 Sezon tygodniowy",
                  "de":"🏆 Wochensaison","tr":"🏆 Haftalık sezon"},
 "se_open":      {"en":"🏆 Open the season","pl":"🏆 Otwórz sezon",
                  "de":"🏆 Saison öffnen","tr":"🏆 Sezonu aç"},
 "se_dash_hint": {"en":"Who did the most this week, and every record the world has ever set.",
                  "pl":"Kto zrobił najwięcej w tym tygodniu i wszystkie rekordy, jakie świat ustanowił.",
                  "de":"Wer diese Woche am meisten geschafft hat, und jeder Rekord, den die Welt je aufgestellt hat.",
                  "tr":"Bu hafta en çok kim başardı ve dünyanın şimdiye kadarki tüm rekorları."},
 "se_intro":     {"en":"Seven days of metins, bosses and refines that landed on +7 or better. Level, horse and gold are shown for context but are deliberately not scored: they are what a character IS, not what it did this week.",
                  "pl":"Siedem dni metinów, bossów i ulepszeń, które weszły na +7 lub wyżej. Poziom, koń i yang są pokazane dla kontekstu, ale celowo nie liczą się do wyniku: to jest to, KIM postać jest, a nie co zrobiła w tym tygodniu.",
                  "de":"Sieben Tage Metins, Bosse und Aufwertungen, die auf +7 oder besser gelandet sind. Level, Pferd und Yang stehen als Kontext dabei, zählen aber bewusst nicht: sie sagen, was ein Charakter IST, nicht was er diese Woche getan hat.",
                  "tr":"Yedi günlük metin, boss ve +7 ve üzerine oturan yükseltmeler. Seviye, at ve yang bağlam için gösterilir ama bilerek puana girmez: onlar karakterin NE olduğunu söyler, bu hafta ne yaptığını değil."},
 "se_back_map":  {"en":"Live map","pl":"Mapa na żywo","de":"Live-Karte","tr":"Canlı harita"},
 "se_records":   {"en":"Server records","pl":"Rekordy serwera","de":"Serverrekorde","tr":"Sunucu rekorları"},
 "se_records_h": {"en":"All time, since the world was created.","pl":"Od początku istnienia świata.",
                  "de":"Seit Bestehen der Welt.","tr":"Dünyanın kurulduğu günden beri."},
 "se_empty":     {"en":"Nobody has done anything countable in the last seven days.",
                  "pl":"Nikt nie zrobił niczego policzalnego w ciągu ostatnich siedmiu dni.",
                  "de":"In den letzten sieben Tagen hat niemand etwas Zählbares getan.",
                  "tr":"Son yedi günde kimse sayılabilir bir şey yapmadı."},
 "se_score":     {"en":"Score","pl":"Wynik","de":"Punkte","tr":"Puan"},
 "se_metins":    {"en":"Metins","pl":"Metiny","de":"Metins","tr":"Metinler"},
 "se_bosses":    {"en":"Bosses","pl":"Bossy","de":"Bosse","tr":"Bosslar"},
 "se_refines":   {"en":"+7 and up","pl":"+7 i wyżej","de":"+7 und mehr","tr":"+7 ve üzeri"},
 "se_deaths":    {"en":"Deaths","pl":"Zgony","de":"Tode","tr":"Ölümler"},
 "se_level":     {"en":"Level","pl":"Poziom","de":"Level","tr":"Seviye"},
 "se_horse":     {"en":"Horse","pl":"Koń","de":"Pferd","tr":"At"},
 "se_bot":       {"en":"Bot","pl":"Bot","de":"Bot","tr":"Bot"},
 "se_r_level":   {"en":"Highest level","pl":"Najwyższy poziom","de":"Höchstes Level","tr":"En yüksek seviye"},
 "se_r_metins":  {"en":"Most metins","pl":"Najwięcej metinów","de":"Meiste Metins","tr":"En çok metin"},
 "se_r_bosses":  {"en":"Most bosses","pl":"Najwięcej bossów","de":"Meiste Bosse","tr":"En çok boss"},
 "se_r_refines": {"en":"Most +7 and up","pl":"Najwięcej +7 i wyżej","de":"Meiste +7 und mehr","tr":"En çok +7 ve üzeri"},
 "se_r_horse":   {"en":"Best horse","pl":"Najlepszy koń","de":"Bestes Pferd","tr":"En iyi at"},
 "se_r_gold":    {"en":"Most yang","pl":"Najwięcej yang","de":"Meiste Yang","tr":"En çok yang"},
})


# What used to be written where it stands - a flash in English on every page,
# "← Back", "Map", "dni" - and the season's marks for people (28 September).
T.update({
 "pl_map": {"pl": "Mapa", "en": "Map"},
 "se_days": {"pl": "{n} dni", "en": "{n} days"},
 "unit_min": {"pl": "min", "en": "min"},
 "ai_items_example": {"pl": "30048\tstall\t# Kawalek Lodu\ntype:19\tstall\t# marmury polimorfii\n50703\tdrop\t# Kwiat Kaki", "en": "30048\tstall\t# Piece of Ice\ntype:19\tstall\t# polymorph marbles\n50703\tdrop\t# Kaki Blossom"},
 "login_locked": {"pl": "Zbyt wiele błędnych prób. Dla bezpieczeństwa odczekaj 15 minut. ⏳", "en": "Too many wrong attempts. Please wait 15 minutes for security. ⏳"},
 "login_wrong": {"pl": "Błędne hasło panelu, spróbuj jeszcze raz. 🙂", "en": "Wrong passphrase, try again. 🙂"},
 "too_many_tries": {"pl": "Zbyt wiele prób. Odczekaj chwilę. ⏳", "en": "Too many attempts. Please wait a while. ⏳"},
 "dl_not_ready": {"pl": "Pobieranie gry nie jest jeszcze gotowe.", "en": "The game download is not ready yet."},
 "play_not_set": {"pl": "Gra w przeglądarce nie jest ustawiona na tym serwerze.", "en": "Playing in the browser is not set up on this server."},
 "reg_too_many": {"pl": "Z tego połączenia założono już zbyt wiele kont. Spróbuj ponownie później. ⏳", "en": "Too many accounts were created from this connection. Please try again later. ⏳"},
 "reg_bad_user": {"pl": "Nazwa użytkownika musi mieć 4-16 liter lub cyfr, bez spacji. 🙂", "en": "The username must be 4-16 letters/numbers, no spaces. 🙂"},
 "reg_bad_pw": {"pl": "Hasło musi mieć co najmniej 6 znaków. 🙂", "en": "The password must be at least 6 characters. 🙂"},
 "reg_pw_mismatch": {"pl": "Hasła się różnią — spróbuj jeszcze raz. 🙂", "en": "The two passwords don't match — try again. 🙂"},
 "reg_bad_social": {"pl": "Kod usuwania musi mieć dokładnie 7 cyfr (np. 1234567). 🙂", "en": "The delete code must be exactly 7 digits (e.g. 1234567). 🙂"},
 "reg_name_taken": {"pl": "Ta nazwa użytkownika jest już zajęta — wybierz inną. 🙂", "en": "That username is already taken — pick another one. 🙂"},
 "reg_failed": {"pl": "Nie udało się teraz założyć konta. Spróbuj ponownie za chwilę. 🙏", "en": "The account could not be created right now. Please try again in a bit. 🙏"},
 "acc_wrong": {"pl": "Błędna nazwa użytkownika lub hasło. 🙂", "en": "Wrong username or password. 🙂"},
 "acc_new_short": {"pl": "Nowe hasło musi mieć co najmniej 6 znaków. 🙂", "en": "The new password must be at least 6 characters. 🙂"},
 "acc_new_mismatch": {"pl": "Nowe hasła się różnią. 🙂", "en": "The two new passwords don't match. 🙂"},
 "acc_pw_changed": {"pl": "🔒 Hasło zostało zmienione! Przy następnym logowaniu do gry użyj nowego.", "en": "🔒 Your password was changed! Use the new one next time you log into the game."},
 "acc_pw_wrong": {"pl": "Obecne hasło jest błędne. 🙂", "en": "The current password is wrong. 🙂"},
 "gold_1m": {"pl": "💰 1 milion", "en": "💰 1 Million"},
 "gold_10m": {"pl": "💰 10 milionów", "en": "💰 10 Million"},
 "gold_100m": {"pl": "💰 100 milionów", "en": "💰 100 Million"},
 "gold_1b": {"pl": "👑 1 miliard", "en": "👑 1 Billion"},
 "win_inventory": {"pl": "ekwipunek", "en": "inventory"},
 "win_equipment": {"pl": "założone", "en": "equipped"},
 "win_safebox": {"pl": "magazyn", "en": "storage"},
 "win_mall": {"pl": "magazyn ItemShopu", "en": "item-shop storage"},
 "win_dragon_soul": {"pl": "kamienie smoka", "en": "dragon soul"},
 "win_belt": {"pl": "pas", "en": "belt"},
 "win_offlineshop": {"pl": "sklep offline", "en": "offline shop"},
 "se_char": {"pl": "Postać", "en": "Character"},
 "se_people_note": {"pl": "👤 to postać gracza. Postaci GM-ów (common.gmlist) nie są liczone.", "en": "👤 marks a player's character. Game masters' characters (common.gmlist) are not counted."},
 "se_people_more": {"pl": "Gracze spoza pierwszej pięćdziesiątki", "en": "Players outside the top fifty"},
})


# The bots' decisions (28 September): the world page of what the core explains
# - why a line went on a counter at its price, why a piece was put on - and the
# AI page's switch for how long it keeps those rows.
T.update({
 "dc_nav": {"pl": "🔎 Decyzje botów", "en": "🔎 Bot decisions",
       "de": "🔎 Entscheidungen der Bots", "tr": "🔎 Bot kararları"},
 "dc_open": {"pl": "🔎 Otwórz decyzje botów", "en": "🔎 Open bot decisions",
       "de": "🔎 Entscheidungen der Bots öffnen", "tr": "🔎 Bot kararlarını aç"},
 "dc_dash_hint": {"pl": "Nietypowe decyzje botów z całego świata: co wystawiły i po ile, co założyły i dlaczego — żeby dziwne zachowanie dało się dokładnie zgłosić.",
       "en": "The bots' unusual decisions across the world: what they listed and at what price, what they put on and why — so odd behaviour can be reported precisely.",
       "de": "Ungewöhnliche Entscheidungen der Bots in der ganzen Welt: was sie eingestellt haben und zu welchem Preis, was sie angelegt haben und warum — damit seltsames Verhalten genau gemeldet werden kann.",
       "tr": "Botların dünya genelindeki olağandışı kararları: neyi hangi fiyata tezgâha koydular, neyi neden kuşandılar — tuhaf davranışlar tam olarak bildirilebilsin diye."},
 "dc_intro": {"pl": "Wiersze z ostatnich godzin, które rdzeń oznaczył flagą. „?” rozwija wyjaśnienie, „Kopiuj” daje je jako tekst do zgłoszenia, a nick bota otwiera jego kartę na mapie.",
       "en": "Rows of the last hours the core flagged. “?” opens an explanation, “Copy” gives it as text for a report, and a bot's name opens its card on the map.",
       "de": "Zeilen der letzten Stunden, die der Kern markiert hat. „?“ klappt eine Erklärung auf, „Kopieren“ gibt sie als Text für eine Meldung, und der Name eines Bots öffnet seine Karte auf der Karte.",
       "tr": "Çekirdeğin son saatlerde işaretlediği satırlar. “?” bir açıklamayı açar, “Kopyala” onu bildirim için metin olarak verir, botun adı ise haritadaki kartını açar."},
 "dc_state_on": {"pl": "Zapisywanie wyjaśnień: włączone, wiersze trzymane {n} dni.", "en": "Recording explanations: on, rows kept {n} days.",
       "de": "Erklärungen aufzeichnen: an, Zeilen werden {n} Tage aufbewahrt.", "tr": "Açıklamaları kaydetme: açık, satırlar {n} gün saklanır."},
 "dc_state_default": {"pl": " (domyślnie — w pliku wag nie ma klucza EXPLAIN)", "en": " (the default — the weights file has no EXPLAIN key)",
       "de": " (Standard — die Gewichtsdatei hat keinen EXPLAIN-Schlüssel)", "tr": " (varsayılan — ağırlık dosyasında EXPLAIN anahtarı yok)"},
 "dc_state_off": {"pl": "Zapisywanie wyjaśnień: wyłączone (EXPLAIN 0) — rdzeń nic nie zapisuje.",
       "en": "Recording explanations: off (EXPLAIN 0) — the core records nothing.",
       "de": "Erklärungen aufzeichnen: aus (EXPLAIN 0) — der Kern zeichnet nichts auf.",
       "tr": "Açıklamaları kaydetme: kapalı (EXPLAIN 0) — çekirdek hiçbir şey kaydetmiyor."},
 "dc_state_engine": {"pl": "Ten silnik (r40250) nie zapisuje wyjaśnień — robi to tylko linia 2.x.",
       "en": "This engine (r40250) records no explanations — only the 2.x line does.",
       "de": "Diese Engine (r40250) zeichnet keine Erklärungen auf — nur die 2.x-Linie tut das.",
       "tr": "Bu motor (r40250) açıklama kaydetmez — bunu yalnızca 2.x hattı yapar."},
 "dc_change": {"pl": "Zmień w zachowaniu botów", "en": "Change it in bot behaviour",
       "de": "Im Bot-Verhalten ändern", "tr": "Bot davranışında değiştir"},
 "dc_hours": {"pl": "Ostatnie", "en": "The last", "de": "Die letzten", "tr": "Son"},
 "dc_hours_n": {"pl": "{n} godz.", "en": "{n} h", "de": "{n} Std.", "tr": "{n} sa"},
 "dc_kind": {"pl": "Rodzaj", "en": "Kind", "de": "Art", "tr": "Tür"},
 "dc_kind_all": {"pl": "Wszystko", "en": "Everything", "de": "Alles", "tr": "Hepsi"},
 "dc_kind_equip": {"pl": "Ekwipunek", "en": "Equipment", "de": "Ausrüstung", "tr": "Ekipman"},
 "dc_kind_listing": {"pl": "Lada", "en": "Counter", "de": "Ladentisch", "tr": "Tezgâh"},
 "dc_flag": {"pl": "Flaga", "en": "Flag", "de": "Markierung", "tr": "İşaret"},
 "dc_flag_unusual": {"pl": "Każda nietypowa", "en": "Any unusual", "de": "Jede ungewöhnliche", "tr": "Her olağandışı"},
 "dc_flag_any": {"pl": "Każda flaga", "en": "Any flag", "de": "Jede Markierung", "tr": "Her işaret"},
 "dc_show": {"pl": "Pokaż", "en": "Show", "de": "Anzeigen", "tr": "Göster"},
 "dc_col_time": {"pl": "Czas", "en": "Time", "de": "Zeit", "tr": "Zaman"},
 "dc_col_bot": {"pl": "Bot", "en": "Bot", "de": "Bot", "tr": "Bot"},
 "dc_col_what": {"pl": "Co", "en": "What", "de": "Was", "tr": "Ne"},
 "dc_col_flags": {"pl": "Flagi", "en": "Flags", "de": "Markierungen", "tr": "İşaretler"},
 "dc_none": {"pl": "Brak oznaczonych decyzji w tym oknie czasu.", "en": "No flagged decisions in this window.",
       "de": "Keine markierten Entscheidungen in diesem Zeitraum.", "tr": "Bu zaman aralığında işaretli karar yok."},
 "dc_missing": {"pl": "W bazie nie ma jeszcze tabel wyjaśnień (log.playerbot_equip, log.playerbot_listing): starsza wersja świata albo rdzeń ich jeszcze nie utworzył.",
       "en": "The database has no explanation tables yet (log.playerbot_equip, log.playerbot_listing): an older world, or the core has not created them yet.",
       "de": "Die Datenbank hat noch keine Erklärungstabellen (log.playerbot_equip, log.playerbot_listing): eine ältere Welt, oder der Kern hat sie noch nicht angelegt.",
       "tr": "Veritabanında henüz açıklama tabloları yok (log.playerbot_equip, log.playerbot_listing): daha eski bir dünya ya da çekirdek onları henüz oluşturmadı."},
 "dc_count": {"pl": "Wierszy: {n}", "en": "Rows: {n}", "de": "Zeilen: {n}", "tr": "Satır: {n}"},
 "dc_card": {"pl": "Karta bota na mapie", "en": "The bot's card on the map", "de": "Die Karte des Bots auf der Karte", "tr": "Botun haritadaki kartı"},
 "dc_limit_note": {"pl": "pokazano najnowsze {n}; zawęź okno albo flagę, by zobaczyć starsze.",
       "en": "the newest {n} are shown; narrow the window or the flag to see older ones.",
       "de": "die neuesten {n} werden gezeigt; grenze Zeitraum oder Markierung ein, um ältere zu sehen.",
       "tr": "en yeni {n} gösteriliyor; eskileri görmek için aralığı ya da işareti daralt."},
 "ai_explain": {"pl": "Wyjaśnienia decyzji botów", "en": "Explanations of the bots' decisions",
       "de": "Erklärungen der Bot-Entscheidungen", "tr": "Bot kararlarının açıklamaları"},
 "ai_explain_help": {"pl": "Rdzeń zapisuje, dlaczego bot wystawił przedmiot i jak wyliczył cenę, oraz dlaczego zmienił założony przedmiot — do dymków w sklepie bota, historii ekwipunku i strony decyzji. 0 wyłącza zapis (nic nie jest liczone), 1–30 to liczba dni, przez które wiersze zostają w bazie; bez wpisu w pliku: 7 dni. Linii, które wciąż stoją na ladzie, nic nie kasuje. Działa w ciągu pięciu sekund.",
       "en": "The core records why a bot listed an item and how it reached the price, and why it changed a worn piece — for the tooltips in a bot's shop, the equipment history and the decisions page. 0 switches recording off (nothing is computed), 1–30 is how many days the rows stay in the database; with no line in the file: 7 days. Lines still standing on a counter are never deleted. Applies within five seconds.",
       "de": "Der Kern zeichnet auf, warum ein Bot einen Gegenstand eingestellt und wie er den Preis ermittelt hat, und warum er ein getragenes Stück gewechselt hat — für die Tooltips im Laden eines Bots, den Ausrüstungsverlauf und die Entscheidungsseite. 0 schaltet die Aufzeichnung aus (nichts wird berechnet), 1–30 ist die Zahl der Tage, die die Zeilen in der Datenbank bleiben; ohne Zeile in der Datei: 7 Tage. Zeilen, die noch auf einem Ladentisch stehen, werden nie gelöscht. Wirkt binnen fünf Sekunden.",
       "tr": "Çekirdek, botun bir eşyayı neden tezgâha koyduğunu ve fiyata nasıl ulaştığını, giyili bir parçayı neden değiştirdiğini kaydeder — botun dükkânındaki ipuçları, ekipman geçmişi ve kararlar sayfası için. 0 kaydı kapatır (hiçbir şey hesaplanmaz), 1–30 satırların veritabanında kaç gün kalacağıdır; dosyada satır yoksa: 7 gün. Hâlâ tezgâhta duran satırlar asla silinmez. Beş saniye içinde etkili olur."},
 "ai_explain_days": {"pl": "Dni przechowywania (0 = wyłączone):", "en": "Days kept (0 = off):",
       "de": "Aufbewahrungstage (0 = aus):", "tr": "Saklama günü (0 = kapalı):"},
 # The export for an analyst or a language model, and the page's two new
 # filters: the anomalies the panel reads out of a row, and one bot.
 "dc_flag_anomaly": {"pl": "Anomalie wykryte przez panel", "en": "Anomalies the panel found",
       "de": "Vom Panel gefundene Auffälligkeiten", "tr": "Panelin bulduğu anormallikler"},
 "dc_bot": {"pl": "Bot (pid albo nick)", "en": "Bot (pid or name)", "de": "Bot (PID oder Name)", "tr": "Bot (pid ya da ad)"},
 "dc_bot_unknown": {"pl": "Nie ma takiego bota: {name}", "en": "No such bot: {name}", "de": "Kein solcher Bot: {name}",
       "tr": "Böyle bir bot yok: {name}"},
 "dc_copy_ai_all": {"pl": "📋 Kopiuj wszystkie dla AI ({n})", "en": "📋 Copy all for an AI ({n})",
       "de": "📋 Alle für eine KI kopieren ({n})", "tr": "📋 Hepsini yapay zekâ için kopyala ({n})"},
 "dc_ai_hint": {"pl": "„Kopiuj dla AI” daje decyzję albo wszystkie pokazane jako Markdown: każda liczba, nazwy kodów z gry, wartości odniesienia i krótki przewodnik — do wklejenia modelowi językowemu, który ma wskazać, gdzie bot się pogubił.",
       "en": "“Copy for an AI” gives a decision, or all those shown, as Markdown: every number, the game's code names, the reference values and a short guide — to paste to a language model that is to find where a bot went wrong.",
       "de": "„Für eine KI kopieren“ gibt eine Entscheidung oder alle gezeigten als Markdown: jede Zahl, die Code-Namen des Spiels, die Referenzwerte und eine kurze Anleitung — zum Einfügen bei einem Sprachmodell, das finden soll, wo sich ein Bot verrannt hat.",
       "tr": "“Yapay zekâ için kopyala” bir kararı ya da gösterilenlerin hepsini Markdown olarak verir: her sayı, oyunun kod adları, referans değerleri ve kısa bir kılavuz — bir botun nerede yanıldığını bulacak bir dil modeline yapıştırmak için."},
})


# MT2009_PLUS_BOT_SESSIONS_V1: a bot's sessions (30 September): the "Sesje
# gry" card on a bot's page and its state in the list of characters, from
# log.playerbot_session (playerbot_session.h).
T.update({
 "ss_title": {"pl": "🕒 Sesje gry", "en": "🕒 Play sessions", "de": "🕒 Spielsitzungen", "tr": "🕒 Oyun oturumları"},
 "ss_state_in": {"pl": "🟢 W grze od {since}, kanał {channel}", "en": "🟢 In the game since {since}, channel {channel}",
       "de": "🟢 Im Spiel seit {since}, Kanal {channel}", "tr": "🟢 {since} zamanından beri oyunda, kanal {channel}"},
 "ss_state_in_now": {"pl": "🟢 W grze, kanał {channel}", "en": "🟢 In the game, channel {channel}",
       "de": "🟢 Im Spiel, Kanal {channel}", "tr": "🟢 Oyunda, kanal {channel}"},
 "ss_state_rest": {"pl": "💤 Odpoczywa do {until} (wylogował się {since})", "en": "💤 Resting until {until} (logged out {since})",
       "de": "💤 Ruht bis {until} (abgemeldet {since})", "tr": "💤 {until} zamanına kadar dinleniyor ({since} çıkış yaptı)"},
 "ss_state_out": {"pl": "⚪ Poza grą od {since}: {why}", "en": "⚪ Out of the game since {since}: {why}",
       "de": "⚪ Nicht im Spiel seit {since}: {why}", "tr": "⚪ {since} zamanından beri oyunda değil: {why}"},
 "ss_state_none": {"pl": "⚪ Poza grą — w ostatnich 7 dniach nie zapisano żadnej sesji.",
       "en": "⚪ Out of the game — no session recorded in the last 7 days.",
       "de": "⚪ Nicht im Spiel — in den letzten 7 Tagen wurde keine Sitzung erfasst.",
       "tr": "⚪ Oyunda değil — son 7 günde hiçbir oturum kaydedilmedi."},
 "ss_week": {"pl": "Ostatnie 7 dni: {played} w grze", "en": "Last 7 days: {played} in the game",
       "de": "Letzte 7 Tage: {played} im Spiel", "tr": "Son 7 gün: {played} oyunda"},
 "ss_total": {"pl": "łączny czas gry postaci: {played}", "en": "the character's total play time: {played}",
       "de": "Gesamtspielzeit des Charakters: {played}", "tr": "karakterin toplam oyun süresi: {played}"},
 "ss_strip_hint": {"pl": "Każdy pasek to doba od północy do północy, zielone odcinki to czas w grze, złoty to sesja, która trwa. Najedź na odcinek, żeby zobaczyć godziny.",
       "en": "Each strip is a day from midnight to midnight; the green stretches are time in the game, the gold one a session still going on. Point at a stretch to see its hours.",
       "de": "Jeder Streifen ist ein Tag von Mitternacht bis Mitternacht; grüne Abschnitte sind Zeit im Spiel, der goldene eine noch laufende Sitzung. Zeige auf einen Abschnitt, um seine Uhrzeiten zu sehen.",
       "tr": "Her şerit gece yarısından gece yarısına bir gündür; yeşil bölümler oyunda geçen zaman, altın rengi olan hâlâ süren oturumdur. Saatlerini görmek için bir bölümün üzerine gel."},
 "ss_col_in": {"pl": "Wejście", "en": "Logged in", "de": "Angemeldet", "tr": "Giriş"},
 "ss_col_out": {"pl": "Wyjście", "en": "Logged out", "de": "Abgemeldet", "tr": "Çıkış"},
 "ss_col_len": {"pl": "Czas", "en": "Length", "de": "Dauer", "tr": "Süre"},
 "ss_col_ch": {"pl": "Kanał", "en": "Channel", "de": "Kanal", "tr": "Kanal"},
 "ss_col_why": {"pl": "Dlaczego", "en": "Why", "de": "Warum", "tr": "Neden"},
 "ss_ongoing": {"pl": "trwa", "en": "going on", "de": "läuft", "tr": "sürüyor"},
 "ss_rest_until": {"pl": "odpoczynek do {until}", "en": "a rest until {until}", "de": "Pause bis {until}", "tr": "{until} zamanına kadar dinlenme"},
 "ss_missing": {"pl": "Serwer nie zapisuje jeszcze sesji botów: rdzeń gry robi to od tej aktualizacji, od swojego pierwszego startu.",
       "en": "The server does not record the bots' sessions yet: the game core does from this update on, from its first start.",
       "de": "Der Server erfasst die Sitzungen der Bots noch nicht: der Spielkern tut es ab diesem Update, ab seinem ersten Start.",
       "tr": "Sunucu botların oturumlarını henüz kaydetmiyor: oyun çekirdeği bunu bu güncellemeden itibaren, ilk başlatılışından sonra yapar."},
 "ss_error": {"pl": "Nie udało się odczytać sesji z bazy.", "en": "The sessions could not be read from the database.",
       "de": "Die Sitzungen konnten nicht aus der Datenbank gelesen werden.", "tr": "Oturumlar veritabanından okunamadı."},
 "ss_today": {"pl": "dziś", "en": "today", "de": "heute", "tr": "bugün"},
 "ss_yesterday": {"pl": "wczoraj", "en": "yesterday", "de": "gestern", "tr": "dün"},
 "ss_tomorrow": {"pl": "jutro", "en": "tomorrow", "de": "morgen", "tr": "yarın"},
 "ss_h_min": {"pl": "{h} h {m} min", "en": "{h} h {m} min", "de": "{h} Std. {m} Min.", "tr": "{h} sa {m} dk"},
 "ss_min": {"pl": "{m} min", "en": "{m} min", "de": "{m} Min.", "tr": "{m} dk"},
 "ss_in_0": {"pl": "wejście nieznane", "en": "an unknown login", "de": "unbekannte Anmeldung", "tr": "bilinmeyen giriş"},
 "ss_in_1": {"pl": "start serwera lub uzupełnienie", "en": "server start or top-up", "de": "Serverstart oder Auffüllen", "tr": "sunucu başlangıcı veya tamamlama"},
 "ss_in_2": {"pl": "po odpoczynku", "en": "after a rest", "de": "nach einer Pause", "tr": "dinlenmeden sonra"},
 "ss_in_3": {"pl": "z innego kanału", "en": "from another channel", "de": "von einem anderen Kanal", "tr": "başka bir kanaldan"},
 "ss_in_4": {"pl": "z właścicielem (Towarzysz)", "en": "with its owner (companion)", "de": "mit seinem Besitzer (Begleiter)", "tr": "sahibiyle (yoldaş)"},
 "ss_out_0": {"pl": "nieznane (serwer nie zamknął sesji)", "en": "unknown (the server did not close the session)",
       "de": "unbekannt (der Server hat die Sitzung nicht geschlossen)", "tr": "bilinmiyor (sunucu oturumu kapatmadı)"},
 "ss_out_1": {"pl": "odpoczynek", "en": "a rest", "de": "Pause", "tr": "dinlenme"},
 "ss_out_2": {"pl": "ban", "en": "a ban", "de": "Sperre", "tr": "yasak"},
 "ss_out_3": {"pl": "przejście na inny kanał", "en": "moved to another channel", "de": "auf einen anderen Kanal gewechselt", "tr": "başka kanala geçti"},
 "ss_out_4": {"pl": "wylogowany przez GM-a lub panel", "en": "logged out by a GM or the panel", "de": "von einem GM oder dem Panel abgemeldet", "tr": "GM veya panel tarafından çıkarıldı"},
 "ss_out_5": {"pl": "z właścicielem (Towarzysz)", "en": "with its owner (companion)", "de": "mit seinem Besitzer (Begleiter)", "tr": "sahibiyle (yoldaş)"},
 "ss_out_6": {"pl": "rozłączenie", "en": "disconnected", "de": "Verbindung getrennt", "tr": "bağlantı koptu"},
 "ss_out_7": {"pl": "zatrzymanie serwera", "en": "the server stopped", "de": "Server gestoppt", "tr": "sunucu durduruldu"},
 # "Koniec gry" (playerbot_retirement.h): this project's own reason.
 "ss_out_8": {"pl": "koniec gry (sprzedał wszystko, nowa postać)", "en": "end of play (sold everything, a new character)",
       "de": "Spielende (alles verkauft, neuer Charakter)", "tr": "oyunun sonu (her şeyi sattı, yeni karakter)"},
 "ss_col_state": {"pl": "Stan", "en": "State", "de": "Status", "tr": "Durum"},
 "ss_list_in": {"pl": "🟢 w grze, kanał {channel}", "en": "🟢 in the game, channel {channel}", "de": "🟢 im Spiel, Kanal {channel}", "tr": "🟢 oyunda, kanal {channel}"},
 "ss_list_rest": {"pl": "💤 odpoczywa do {until}", "en": "💤 resting until {until}", "de": "💤 ruht bis {until}", "tr": "💤 {until} zamanına kadar dinleniyor"},
 "ss_list_out": {"pl": "⚪ poza grą", "en": "⚪ out of the game", "de": "⚪ nicht im Spiel", "tr": "⚪ oyunda değil"},
 "ss_only_bots": {"pl": "🤖 Tylko boty, z ich stanem", "en": "🤖 Bots only, with their state", "de": "🤖 Nur Bots, mit ihrem Status", "tr": "🤖 Yalnızca botlar, durumlarıyla"},
 "ss_all_chars": {"pl": "👥 Wszystkie postacie", "en": "👥 All characters", "de": "👥 Alle Charaktere", "tr": "👥 Tüm karakterler"},
 "ss_bots_count": {"pl": "Botów: {total} — w grze {in_game}, odpoczywa {resting}, poza grą {out}.",
       "en": "Bots: {total} — {in_game} in the game, {resting} resting, {out} out of the game.",
       "de": "Bots: {total} — {in_game} im Spiel, {resting} ruhen, {out} nicht im Spiel.",
       "tr": "Botlar: {total} — {in_game} oyunda, {resting} dinleniyor, {out} oyunda değil."},
})


POINT_TO_APPLY = {6: 1, 8: 2, 13: 3, 15: 4, 12: 5, 14: 6, 17: 7, 19: 8, 21: 9, 32: 10, 33: 11,
 37: 12, 38: 13, 39: 14, 40: 15, 41: 16, 43: 17, 44: 18, 45: 19, 46: 20, 47: 21,
 48: 22, 63: 23, 64: 24, 65: 25, 66: 26, 67: 27, 68: 28, 69: 29, 70: 30, 71: 31,
 72: 32, 73: 33, 74: 34, 75: 35, 76: 36, 77: 37, 78: 38, 79: 39, 81: 41, 82: 42,
 83: 43, 84: 44, 85: 45, 86: 46, 87: 47, 88: 48, 89: 49, 90: 50, 28: 51, 34: 52,
 95: 53, 96: 54, 22: 55, 23: 56, 42: 57, 10: 58, 54: 59, 55: 60, 56: 61, 57: 62,
 53: 63, 114: 64, 115: 65, 116: 66, 117: 67, 118: 68, 119: 69, 120: 70, 121: 71,
 122: 72, 123: 73, 124: 74, 125: 75, 126: 76, 59: 78, 60: 79, 61: 80, 62: 81,
 128: 82, 16: 83, 130: 84, 131: 85, 132: 86, 133: 87, 134: 88, 135: 89, 136: 90,
 137: 91,
 # mt2009 points with no APPLY id at all (length.h 138..168 - the engine
 # applies them straight from the item). A pseudo key of 1000 + point, so
 # the label tables can name them; without it the panel wrote "Bonus #139".
 138: 1138, 139: 1139, 140: 1140, 141: 1141, 142: 1142, 143: 1143, 144: 1144, 145: 1145, 146: 1146, 147: 1147, 148: 1148, 149: 1149, 150: 1150, 151: 1151, 152: 1152, 153: 1153, 154: 1154, 155: 1155, 156: 1156, 157: 1157, 158: 1158, 159: 1159, 160: 1160, 161: 1161, 162: 1162, 163: 1163, 164: 1164, 165: 1165, 166: 1166, 167: 1167, 168: 1168}


def apply_key(attr_type):
    """The APPLY_META key for an attribute type, on either engine."""
    t = int(attr_type or 0)
    return POINT_TO_APPLY.get(t, t) if ENGINE_MT2009 else t


def t(key):
    return T.get(key, {}).get(lang(), T.get(key, {}).get("en", key))


MAP_I18N = {
 "pl": {
  "title":"Mapa świata na żywo — Chunjo","live":"NA ŻYWO (1,5 s)","subtitle":"Interaktywny podgląd pozycji i rozwoju botów w czasie rzeczywistym",
  "player_panel":"Panel graczy","play_browser":"Graj w przeglądarce","show_bots":"Pokaż boty","names_levels":"Nicki i poziomy","pt_only":"Tylko w grupie (PT)",
  "level":"Poziom","all":"Wszystkie","map":"Mapa","m1":"M1 — Joan (Chunjo)","m2":"M2 — Bokjung (Chunjo)","m3":"Ziemia Klanu Chunjo","s1":"M1 — Yongan (Shinsoo)","s2":"M2 — Jayang (Shinsoo)","s3":"Ziemia Klanu Shinsoo","smonkey":"Loch Małp Shinsoo","j1":"M1 — Pyongmoo (Jinno)","j2":"M2 — Bakra (Jinno)","j3":"Ziemia Klanu Jinno","jmonkey":"Loch Małp Jinno","monkey":"Łatwy Loch Małp","monkey_medium":"Średni Loch Małp","monkey_hard":"Trudny Loch Małp","orc":"Dolina Orków","desert":"Pustynia Yongbi","sohan":"Góra Sohan","spider":"Loch Pająków V1","spider_v2":"Loch Pająków V2","hwang":"Świątynia Hwang","forest":"Las","red_forest":"Czerwony Las","demon_tower":"Wieża Demonów","fire_land":"Doyyumhwaji (Ognista Ziemia)","grotto":"Grota Wygnańców V1","grotto2":"Grota Wygnańców V2","heat":"Mapa cieplna","heat_deaths":"Zgony botów","heat_metins":"Rozbite metiny","heat_skills":"Awanse umiejętności","search":"🔍 Szukaj bota (np. botarek)...",
  "solo_bot":"Bot solo","party_bot":"W grupie (PT)","metin_fight":"Walka z Metinem","loading":"Ładowanie...","world_stats":"Statystyki świata","active_bots":"Aktywne boty",
  "in_parties":"W grupach (PT)","avg_level":"Średni poziom","max_level":"Maks. poziom","rankings":"Rankingi botów","rank_level":"Poziom","rank_weapon":"Broń","rank_armor":"Zbroja",
  "rank_weapon30":"Bronie 30 Lv","rank_items":"Przedmioty","rank_horse":"Koń","rank_biologist":"Biolog","rank_hunting":"Polowanie","rank_shops":"Otwarte sklepy","rank_skills":"Umiejętności","rank_plus9":"Przedmiot +9","rank_stall_open":"Stragan otwarty","rank_empty":"Brak danych rankingu.","rank_show":"Pokaż","rank_search":"Szukaj w rankingu...","none":"Brak","items_short":"przedm.",
  "horse_lv":"Koń Lv","visible":"Widocznych","characters":"postaci","in_group":"W grupie [PT]","solo":"Solo","player":"GRACZ","bot":"Bot","class":"Klasa","action":"Akcja","status":"Status","personality":"Osobowość","mood":"Nastrój","charakter":"Charakter","ambition":"Ambicja","current_goal":"Aktualny cel",
  "coordinates":"Koordynaty","open_inventory":"Kliknij, aby otworzyć ekwipunek i EQ","loading_character":"Ładowanie ekwipunku i statystyk postaci","error":"Błąd","not_found":"Nie znaleziono danych",
  "teleport_me":"Teleportuj moją postać w grze (1 klik)","position":"Pozycja","horse":"Koń","biologist":"Biolog","bio_stage":"Etap Biologa","hunting":"Polowanie","no_data":"Brak danych",
  "stats":"Statystyki","unspent_stats":"Nierozdane: {n} pkt statystyk","skills":"Umiejętności","profession_none":"Nie wybrano","profession_pending":"Profesja nie została jeszcze wybrana.","depot":"Magazyn","depot_empty":"Magazyn jest pusty.","shop":"Sklep","shop_none":"Ten bot nie ma otwartego sklepu.","shop_empty":"Lada jest pusta.","shop_price":"Cena","shop_premium":"premium","shop_expired":"wygasły — nikt z niego nie kupi, dopóki bot go nie odnowi",
  "unspent_skills":"Nierozdane: {n} pkt umiejętności","equipped":"Założony ekwipunek (EQ)","weapon":"Broń","armor":"Zbroja","helmet":"Hełm","shield":"Tarcza","bracelet":"Bransoleta",
  "boots":"Buty","necklace":"Naszyjnik","earrings":"Kolczyki","empty":"Puste","inventory":"Zawartość ekwipunku","items_count":"przedmiotów","inventory_empty":"Ekwipunek jest pusty.","quantity":"Ilość",
  "gear_history":"Historia ekwipunku","gear_history_hint":"Handel, bonusy i ulepszanie w osobnych zakładkach — z log.log i dziennika sklepów offline","gear_history_loading":"Ładowanie historii...","gear_history_empty":"Brak wpisów w tej zakładce.","gear_history_more":"Pokaż starsze","gear_tab_trade":"Handel","gear_tab_bonus":"Bonusy","gear_tab_refine":"Ulepszanie","gear_tab_other":"Inne","gear_tab_all":"Wszystko",
  "event_log":"Dziennik zdarzeń bota (logi na żywo)","track_live":"Śledź na żywo","copy_logs":"Kopiuj logi","loading_logs":"Ładowanie logów postaci","no_logs":"Brak najświeższych wpisów w logach dla tej postaci.",
  "log_error":"Błąd odczytu logów","network_error":"Błąd sieci","teleporting":"Teleportowanie Twojej postaci w grze...","teleported":"Przeteleportowano {name} do bota w grze!","you":"Cię","failure":"Niepowodzenie",
  "copied":"Skopiowano","paste":"wklej w grze [Enter] → Ctrl+V → [Enter]","solo_exp":"Solo — zdobywanie doświadczenia","party_exp":"[PT] Zdobywanie doświadczenia w grupie","metin_hunt":"Polowanie na Metiny",
  "character_missing":"Postać nie znaleziona","bio_next":"Następna misja od Lv {level}: {name}","bio_key":"{name}{sep}{have}/{need}, czeka na: {key}",
  "bio_all":"Wszystkie podstawowe misje ukończone","bio_complete":"komplet","bio_done":"ukończone","bio_skipped":"za niskie dla bota, pominięte: {n}","bio_dropper":"nie dotyczy — dropper nie robi Biologa",
  "bio_rank_now":"{done}/{total} ukończone • teraz: {stage}","bio_rank_next":"{done}/{total} ukończone • {stage}",
  "rankings_all":"Rankingi","people_only":"Tylko gracze","people_only_hint":"Same postacie ludzi, bez botów, ponumerowane między sobą.",
  "person_mark":"Postać gracza","ranking_people_note":"👤 postać gracza · postaci GM-ów nie są liczone",
  "rank_avg_short":"ŚR","rank_skill_short":"UM","bag":"Plecak","weapon30_one":"Broń 30 Lv",
  "tt_req_level":"Wymagany Poziom","tt_attack":"Wartość Ataku","tt_magic_attack":"Wartość Magicznego Ataku",
  "tt_attack_speed":"Szybkość Ataku","tt_defense":"Obrona","tt_move_speed":"Szybkość Ruchu",
  "tt_broken_stone":"Pęknięty Kamień","tt_empty_socket":"Czysty Slot","tt_soul_stone":"Kamień Duszy",
  "item_n":"Przedmiot #{n}","bot_badge":"BOT","exp_hold":"blokada expa na {n} lvl",
  "hunt_done_through":"Ukończone do Lv {n}","hunt_not_started":"Jeszcze nierozpoczęte",
  "gh_sold_for":"{count} za {price} yang","gh_instead":"zamiast {item}","gh_paid":"za {price} yang","gh_from":"od {seller}",
  "warp_player_offline":"Żadna postać gracza nie jest teraz w grze.","warp_no_human":"Nie ma postaci gracza, którą można przenieść.",
  "warp_timeout":"Gra nie odpowiedziała na czas.","saddlebags":"Juki"
 },
 "en": {
  "title":"Live world map — Chunjo","live":"LIVE (1.5 s)","subtitle":"Interactive real-time view of bot positions and progression",
  "player_panel":"Player panel","play_browser":"Play in browser","show_bots":"Show bots","names_levels":"Names and levels","pt_only":"Party only (PT)",
  "level":"Level","all":"All","map":"Map","m1":"M1 — Joan (Chunjo)","m2":"M2 — Bokjung (Chunjo)","m3":"Chunjo guild map","s1":"M1 — Yongan (Shinsoo)","s2":"M2 — Jayang (Shinsoo)","s3":"Shinsoo guild map","smonkey":"Shinsoo Monkey Dungeon","j1":"M1 — Pyongmoo (Jinno)","j2":"M2 — Bakra (Jinno)","j3":"Jinno guild map","jmonkey":"Jinno Monkey Dungeon","monkey":"Easy Monkey Dungeon","monkey_medium":"Medium Monkey Dungeon","monkey_hard":"Hard Monkey Dungeon","orc":"Orc Valley","desert":"Yongbi Desert","sohan":"Mount Sohan","spider":"Spider Dungeon V1","spider_v2":"Spider Dungeon V2","hwang":"Hwang Temple","forest":"Forest","red_forest":"Red Forest","demon_tower":"Demon Tower","fire_land":"Doyyumhwaji (Fireland)","grotto":"Grotto of Exile V1","grotto2":"Grotto of Exile V2","heat":"Heatmap","heat_deaths":"Bot deaths","heat_metins":"Metins broken","heat_skills":"Skill-ups","search":"🔍 Find a bot (e.g. botarek)...",
  "solo_bot":"Solo bot","party_bot":"In party (PT)","metin_fight":"Fighting a Metin","loading":"Loading...","world_stats":"World statistics","active_bots":"Active bots",
  "in_parties":"In parties (PT)","avg_level":"Average level","max_level":"Max level","rankings":"Bot rankings","rank_level":"Level","rank_weapon":"Weapon","rank_armor":"Armour",
  "rank_weapon30":"Lv 30 Weapons","rank_items":"Items","rank_horse":"Horse","rank_biologist":"Biologist","rank_hunting":"Hunting","rank_shops":"Open shops","rank_skills":"Skills","rank_plus9":"Item +9","rank_stall_open":"Stall open","rank_empty":"No ranking data.","rank_show":"Show","rank_search":"Search ranking...","none":"None","items_short":"items",
  "horse_lv":"Horse Lv","visible":"Visible","characters":"characters","in_group":"In party [PT]","solo":"Solo","player":"PLAYER","bot":"Bot","class":"Class","action":"Action","status":"Status","personality":"Personality","mood":"Mood","charakter":"Character","ambition":"Ambition","current_goal":"Current goal",
  "coordinates":"Coordinates","open_inventory":"Click to open inventory and equipment","loading_character":"Loading character equipment and statistics","error":"Error","not_found":"No data found",
  "teleport_me":"Teleport my in-game character (one click)","position":"Position","horse":"Horse","biologist":"Biologist","bio_stage":"Biologist stage","hunting":"Hunting","no_data":"No data",
  "stats":"Statistics","unspent_stats":"Unspent: {n} stat points","skills":"Skills","profession_none":"Not selected","profession_pending":"The profession has not been selected yet.","depot":"Depot","depot_empty":"The depot is empty.","shop":"Shop","shop_none":"This bot has no stall open.","shop_empty":"The counter is empty.","shop_price":"Price","shop_premium":"premium","shop_expired":"expired — nobody can buy from it until the bot renews it",
  "unspent_skills":"Unspent: {n} skill points","equipped":"Equipped items","weapon":"Weapon","armor":"Armour","helmet":"Helmet","shield":"Shield","bracelet":"Bracelet",
  "boots":"Boots","necklace":"Necklace","earrings":"Earrings","empty":"Empty","inventory":"Inventory contents","items_count":"items","inventory_empty":"The inventory is empty.","quantity":"Quantity",
  "gear_history":"Equipment history","gear_history_hint":"Trade, bonuses and refining on separate tabs — from log.log and the offline shop log","gear_history_loading":"Loading history...","gear_history_empty":"No entries on this tab.","gear_history_more":"Show older","gear_tab_trade":"Trade","gear_tab_bonus":"Bonuses","gear_tab_refine":"Refining","gear_tab_other":"Other","gear_tab_all":"All",
  "event_log":"Bot event log (live)","track_live":"Track live","copy_logs":"Copy logs","loading_logs":"Loading logs for","no_logs":"No recent log entries for this character.",
  "log_error":"Log read error","network_error":"Network error","teleporting":"Teleporting your in-game character...","teleported":"Teleported {name} to the bot in game!","you":"you","failure":"Failure",
  "copied":"Copied","paste":"paste in game [Enter] → Ctrl+V → [Enter]","solo_exp":"Solo levelling","party_exp":"[PT] Party levelling","metin_hunt":"Hunting Metins",
  "character_missing":"Character not found","bio_next":"Next mission at Lv {level}: {name}","bio_key":"{name}{sep}{have}/{need}, waiting for: {key}",
  "bio_all":"All basic missions completed","bio_complete":"complete","bio_done":"done","bio_skipped":"outgrown, skipped: {n}","bio_dropper":"does not apply — a dropper does not do the Biologist",
  "bio_rank_now":"{done}/{total} done • now: {stage}","bio_rank_next":"{done}/{total} done • {stage}",
  "rankings_all":"Rankings","people_only":"Players only","people_only_hint":"People's characters only, without the bots, numbered among themselves.",
  "person_mark":"A player's character","ranking_people_note":"👤 a player's character · game masters' characters are not counted",
  "rank_avg_short":"AVG","rank_skill_short":"SKILL","bag":"Bag","weapon30_one":"Lv 30 weapon",
  "tt_req_level":"Required Level","tt_attack":"Attack Value","tt_magic_attack":"Magic Attack Value",
  "tt_attack_speed":"Attack Speed","tt_defense":"Defence","tt_move_speed":"Movement Speed",
  "tt_broken_stone":"Broken Stone","tt_empty_socket":"Empty Socket","tt_soul_stone":"Spirit Stone",
  "item_n":"Item #{n}","bot_badge":"BOT","exp_hold":"exp held at level {n}",
  "hunt_done_through":"Completed through Lv {n}","hunt_not_started":"Not started yet",
  "gh_sold_for":"{count} for {price} yang","gh_instead":"instead of {item}","gh_paid":"for {price} yang","gh_from":"from {seller}",
  "warp_player_offline":"None of the players' characters is in the game right now.","warp_no_human":"There is no player's character to move.",
  "warp_timeout":"The game did not answer in time.","saddlebags":"Saddlebags"
 }
}


# The words around the bots' explanations - the shop's tooltip and its pinned
# pane, the gear history's "?" and its decisions tab, the /decisions page - in
# all four languages. The codes' own words are the DECISION_* tables beside
# the gear history, and the server says an explanation whole; these are what
# frames it.
for _lang, _texts in {
 "pl": {
  "ex_why_counter": "Dlaczego na ladzie", "ex_how_price": "Jak powstała cena",
  "ex_pin_hint": "Kliknij, aby przypiąć wyjaśnienie.", "ex_copy": "Kopiuj", "ex_copied": "Skopiowano wyjaśnienie",
  "ex_close": "Zamknij", "ex_none": "Brak zapisanego wyjaśnienia tej linii.",
  "ex_state_missing": "Ten świat nie ma jeszcze tabel wyjaśnień (starsza wersja albo rdzeń ich jeszcze nie utworzył).",
  "ex_state_off": "Ten silnik nie zapisuje wyjaśnień decyzji botów.",
  "ex_unusual": "nietypowe", "ex_unusual_only": "Tylko nietypowe",
  "ex_col_step": "Krok", "ex_col_effect": "Zmiana", "ex_col_value": "Cena", "ex_col_term": "Składnik",
  "ex_steps_cut": "… dalsze kroki ucięte (lista była za długa)", "ex_last_change": "Ostatnia zmiana",
  "ex_changes": "zmian od wystawienia: {n}", "ex_was_now": "było {was} → jest {now}",
  "ex_codes": "Kody", "ex_flags": "Flagi", "ex_list_price": "cena {price}", "ex_price": "{price} yang",
  "ex_stand": "Sklep otwarty, bo: {reason}", "ex_pick": "Kandydat {rank}. z {n}",
  "ex_pick_refused": ", lada odrzuciła {k} wyżej od niego", "ex_score": "wynik {score}",
  "ex_cut": "{shape}: ze stosu {whole} szt. na ladę {take}, zapas trzymany w plecaku: {keep}",
  "ex_held": "w plecaku łącznie {held}",
  "ex_price_differs": "Cena na ladzie ({now}) różni się od wyjaśnionej ({explained}) — tej zmiany nic nie zapisało.",
  "ex_end_sold": "Sprzedane za {price} yang", "ex_end_off": "Zdjęte z lady: {reason}",
  "ex_context": "Okoliczności", "ex_origin": "Skąd nowy", "ex_roles": "Role", "ex_lines": "Bonusy",
  "ex_stones": "Kamienie", "ex_old": "Stary", "ex_new": "Nowy", "ex_delta": "Różnica",
  "ex_terms": "Składniki wyniku", "ex_blow": "Cios broni rozłożony na części", "ex_gain": "Zmiana wyniku",
  "ex_score_word": "Wynik", "ex_bot": "Bot", "ex_item": "Przedmiot", "ex_time": "Czas", "ex_more": "Wyjaśnienie",
  "ex_title_equip": "Dlaczego to założył", "ex_title_unequip": "Dlaczego to zdjął",
  "ex_level_slot": "poziom {level}, slot: {slot}", "ex_suppressed": "limit pominął przedtem {n} wpisów tego bota",
  "ex_wear_unique": "unikat", "ex_wear_slot": "slot {n}", "ex_mob_n": "potwór #{n}",
  "ex_skill_n": "umiejętność #{n}", "ex_code_n": "kod {n}", "ex_nothing": "nic",
  "gear_tab_decisions": "Decyzje", "gh_unequipped": "Zdjęte",
 },
 "en": {
  "ex_why_counter": "Why on the counter", "ex_how_price": "How the price was reached",
  "ex_pin_hint": "Click to pin the explanation.", "ex_copy": "Copy", "ex_copied": "Explanation copied",
  "ex_close": "Close", "ex_none": "No explanation recorded for this line.",
  "ex_state_missing": "This world has no explanation tables yet (an older version, or the core has not created them yet).",
  "ex_state_off": "This engine does not record the bots' explanations.",
  "ex_unusual": "unusual", "ex_unusual_only": "Unusual only",
  "ex_col_step": "Step", "ex_col_effect": "Effect", "ex_col_value": "Price", "ex_col_term": "Term",
  "ex_steps_cut": "… further steps cut (the list was too long)", "ex_last_change": "Last change",
  "ex_changes": "changes since listing: {n}", "ex_was_now": "was {was} → now {now}",
  "ex_codes": "Codes", "ex_flags": "Flags", "ex_list_price": "price {price}", "ex_price": "{price} yang",
  "ex_stand": "Shop open because: {reason}", "ex_pick": "Candidate {rank} of {n}",
  "ex_pick_refused": ", the counter refused {k} ranked above it", "ex_score": "score {score}",
  "ex_cut": "{shape}: of a stack of {whole}, {take} to the counter; the reserve kept in the bag: {keep}",
  "ex_held": "{held} in the bag in all",
  "ex_price_differs": "The counter's price ({now}) differs from the explained one ({explained}) — nothing recorded that change.",
  "ex_end_sold": "Sold for {price} yang", "ex_end_off": "Taken off the counter: {reason}",
  "ex_context": "Context", "ex_origin": "Where the new piece came from", "ex_roles": "Roles", "ex_lines": "Bonus lines",
  "ex_stones": "Stones", "ex_old": "Old", "ex_new": "New", "ex_delta": "Delta",
  "ex_terms": "Score terms", "ex_blow": "The weapon's blow, broken down", "ex_gain": "Score change",
  "ex_score_word": "Score", "ex_bot": "Bot", "ex_item": "Item", "ex_time": "Time", "ex_more": "Explanation",
  "ex_title_equip": "Why it put this on", "ex_title_unequip": "Why it took this off",
  "ex_level_slot": "level {level}, slot: {slot}", "ex_suppressed": "the budget dropped {n} of this bot's rows before this one",
  "ex_wear_unique": "unique", "ex_wear_slot": "slot {n}", "ex_mob_n": "monster #{n}",
  "ex_skill_n": "skill #{n}", "ex_code_n": "code {n}", "ex_nothing": "nothing",
  "gear_tab_decisions": "Decisions", "gh_unequipped": "Taken off",
 },
 "de": {
  "ex_why_counter": "Warum auf dem Ladentisch", "ex_how_price": "Wie der Preis zustande kam",
  "ex_pin_hint": "Klicken, um die Erklärung anzuheften.", "ex_copy": "Kopieren", "ex_copied": "Erklärung kopiert",
  "ex_close": "Schließen", "ex_none": "Für diese Zeile ist keine Erklärung aufgezeichnet.",
  "ex_state_missing": "Diese Welt hat noch keine Erklärungstabellen (eine ältere Version, oder der Kern hat sie noch nicht angelegt).",
  "ex_state_off": "Diese Engine zeichnet keine Erklärungen der Bots auf.",
  "ex_unusual": "ungewöhnlich", "ex_unusual_only": "Nur ungewöhnliche",
  "ex_col_step": "Schritt", "ex_col_effect": "Wirkung", "ex_col_value": "Preis", "ex_col_term": "Bestandteil",
  "ex_steps_cut": "… weitere Schritte abgeschnitten (die Liste war zu lang)", "ex_last_change": "Letzte Änderung",
  "ex_changes": "Änderungen seit dem Einstellen: {n}", "ex_was_now": "war {was} → jetzt {now}",
  "ex_codes": "Codes", "ex_flags": "Markierungen", "ex_list_price": "Preis {price}", "ex_price": "{price} Yang",
  "ex_stand": "Laden offen, weil: {reason}", "ex_pick": "Kandidat {rank} von {n}",
  "ex_pick_refused": ", der Ladentisch lehnte {k} davor ab", "ex_score": "Wert {score}",
  "ex_cut": "{shape}: von einem Stapel zu {whole} Stück {take} auf den Ladentisch; im Beutel gehaltene Reserve: {keep}",
  "ex_held": "insgesamt {held} im Beutel",
  "ex_price_differs": "Der Preis auf dem Ladentisch ({now}) weicht vom erklärten ({explained}) ab — diese Änderung wurde nicht aufgezeichnet.",
  "ex_end_sold": "Verkauft für {price} Yang", "ex_end_off": "Vom Ladentisch genommen: {reason}",
  "ex_context": "Umstände", "ex_origin": "Herkunft des neuen Stücks", "ex_roles": "Rollen", "ex_lines": "Bonuszeilen",
  "ex_stones": "Steine", "ex_old": "Alt", "ex_new": "Neu", "ex_delta": "Differenz",
  "ex_terms": "Bestandteile des Werts", "ex_blow": "Der Schlag der Waffe, aufgeschlüsselt", "ex_gain": "Wertänderung",
  "ex_score_word": "Wert", "ex_bot": "Bot", "ex_item": "Gegenstand", "ex_time": "Zeit", "ex_more": "Erklärung",
  "ex_title_equip": "Warum er das angelegt hat", "ex_title_unequip": "Warum er das abgelegt hat",
  "ex_level_slot": "Level {level}, Platz: {slot}", "ex_suppressed": "das Budget ließ davor {n} Zeilen dieses Bots weg",
  "ex_wear_unique": "Unikat", "ex_wear_slot": "Platz {n}", "ex_mob_n": "Monster #{n}",
  "ex_skill_n": "Fertigkeit #{n}", "ex_code_n": "Code {n}", "ex_nothing": "nichts",
  "gear_tab_decisions": "Entscheidungen", "gh_unequipped": "Abgelegt",
 },
 "tr": {
  "ex_why_counter": "Neden tezgâhta", "ex_how_price": "Fiyat nasıl oluştu",
  "ex_pin_hint": "Açıklamayı sabitlemek için tıkla.", "ex_copy": "Kopyala", "ex_copied": "Açıklama kopyalandı",
  "ex_close": "Kapat", "ex_none": "Bu satır için kayıtlı açıklama yok.",
  "ex_state_missing": "Bu dünyada henüz açıklama tabloları yok (eski bir sürüm ya da çekirdek onları henüz oluşturmadı).",
  "ex_state_off": "Bu motor botların açıklamalarını kaydetmez.",
  "ex_unusual": "olağandışı", "ex_unusual_only": "Yalnızca olağandışı",
  "ex_col_step": "Adım", "ex_col_effect": "Etki", "ex_col_value": "Fiyat", "ex_col_term": "Bileşen",
  "ex_steps_cut": "… sonraki adımlar kesildi (liste çok uzundu)", "ex_last_change": "Son değişiklik",
  "ex_changes": "tezgâha konduktan beri değişiklik: {n}", "ex_was_now": "önce {was} → şimdi {now}",
  "ex_codes": "Kodlar", "ex_flags": "İşaretler", "ex_list_price": "fiyat {price}", "ex_price": "{price} yang",
  "ex_stand": "Dükkân açık, çünkü: {reason}", "ex_pick": "{n} aday içinde {rank}.",
  "ex_pick_refused": ", tezgâh üstündeki {k} adayı reddetti", "ex_score": "puan {score}",
  "ex_cut": "{shape}: {whole} adetlik yığından {take} tezgâha; çantada tutulan yedek: {keep}",
  "ex_held": "çantada toplam {held}",
  "ex_price_differs": "Tezgâhtaki fiyat ({now}) açıklanandan ({explained}) farklı — bu değişikliği hiçbir şey kaydetmedi.",
  "ex_end_sold": "{price} yang karşılığında satıldı", "ex_end_off": "Tezgâhtan indirildi: {reason}",
  "ex_context": "Bağlam", "ex_origin": "Yeni parçanın kaynağı", "ex_roles": "Roller", "ex_lines": "Bonus satırları",
  "ex_stones": "Taşlar", "ex_old": "Eski", "ex_new": "Yeni", "ex_delta": "Fark",
  "ex_terms": "Puan bileşenleri", "ex_blow": "Silahın darbesi, parçalarına ayrılmış", "ex_gain": "Puan değişimi",
  "ex_score_word": "Puan", "ex_bot": "Bot", "ex_item": "Eşya", "ex_time": "Zaman", "ex_more": "Açıklama",
  "ex_title_equip": "Bunu neden kuşandı", "ex_title_unequip": "Bunu neden çıkardı",
  "ex_level_slot": "seviye {level}, yuva: {slot}", "ex_suppressed": "bütçe bundan önce bu botun {n} satırını atladı",
  "ex_wear_unique": "eşsiz", "ex_wear_slot": "yuva {n}", "ex_mob_n": "canavar #{n}",
  "ex_skill_n": "beceri #{n}", "ex_code_n": "kod {n}", "ex_nothing": "hiçbir şey",
  "gear_tab_decisions": "Kararlar", "gh_unequipped": "Çıkarıldı",
 },
}.items():
    # Only the languages this panel speaks (Polish and English): the German
    # and Turkish words stay beside them for a panel that adds those.
    if _lang in MAP_I18N:
        MAP_I18N[_lang].update(_texts)


# The export for an analyst or a language model (29 September, the operator:
# "zebysmy mogli latwo widziec, gdzie sa bledy, i dawac je AI do analizy"): one
# decision, a bot's decisions or a page of them as Markdown, with every number
# the page shows, the header's code names beside the words, the values the rules
# compared against and a short guide to reading it (exMarkdown, EXPLAIN_JS).
for _lang, _texts in {
 "pl": {
  "ex_copy_ai": "Kopiuj dla AI", "ex_copied_ai": "Skopiowano jako Markdown dla AI",
  "ex_anomalies": "Anomalie", "ex_ai_title": "Decyzje botów Metin2 — do analizy",
  "ex_ai_guide": "Każda decyzja to wiersz zapisany przez rdzeń gry w chwili jej podjęcia (log.playerbot_equip, "
                 "log.playerbot_listing); nazwy w `…` to kody z playerbot_explain_rules.h i stałe nakładki. Zmiana "
                 "ekwipunku: przegląd zakłada rzecz, której wynik (TERM_TOTAL) przewyższa noszoną o SWAP_MARGIN_PERMILLE "
                 "promili, a rzecz zdjętą w ciągu godziny dopiero o SWAP_BACK_MARGIN_PERCENT procent. Wynik broni to jej "
                 "cios ×1000 (TERM_BLOW_X1000, model GetPlayerBotWeaponHitDamageAt: stopień ataku na piechotę, rzut broni, "
                 "atak z plusa, obrona potwora na poziomie bota, średnie obrażenia i obrażenia umiejętności, ukryty bonus "
                 "poziomu broni mt2009) ze stopniem Iwakury tylko na ciosie, i obok preferencja klasy i stałe bonusy; "
                 "reszty ekwipunku — obrona, bonusy i kamienie razy stopień rodziny. Linia lady: kroki ceny w kolejności "
                 "wyceny — arkusz Iwakury ze stawką yang i inflacją, bonusy, pamięć sprzedaży, rejestr rynku, ogranicznik "
                 "kroku, rozrzut sprzedawcy, progi, zaokrąglenie. Flagi zapisał rdzeń, anomalie wyliczył panel.",
  "ex_ai_ask": "Zadanie: wskaż decyzje sprzeczne z ich własnymi liczbami albo z celem bota, podaj regułę (nazwę kodu), "
               "która je podjęła, i co by je naprawiło.",
  "ex_ai_refs": "Wartości odniesienia (stałe z kodu)", "ex_ai_equip": "Zmiana ekwipunku", "ex_ai_listing": "Linia lady",
  "ex_ai_now": "teraz", "ex_ai_decision": "Decyzja", "ex_ai_level_then": "poziom w chwili decyzji: {level}",
  "ex_sheet_ratio": "cena za sztukę wobec arkusza: {unit} / {sheet} = {ratio}", "ex_ai_code": "kod",
  "ex_ai_n": "Decyzji: {n}",
 },
 "en": {
  "ex_copy_ai": "Copy for an AI", "ex_copied_ai": "Copied as Markdown for an AI",
  "ex_anomalies": "Anomalies", "ex_ai_title": "Metin2 bot decisions — for analysis",
  "ex_ai_guide": "Each decision is a row the game core wrote the moment it was taken (log.playerbot_equip, "
                 "log.playerbot_listing); the names in `…` are the codes of playerbot_explain_rules.h and the overlay's "
                 "constants. An equipment change: the pass puts on a piece whose score (TERM_TOTAL) beats the worn one's "
                 "by SWAP_MARGIN_PERMILLE per mille, and a piece it took off within the hour only by "
                 "SWAP_BACK_MARGIN_PERCENT percent. A weapon's score is its blow x1000 (TERM_BLOW_X1000, the model of "
                 "GetPlayerBotWeaponHitDamageAt: the attack grade on foot, the weapon's roll, the plus's attack, a monster's "
                 "defence at the bot's level, average and skill damage, mt2009's hidden bonus by the weapon's level) with "
                 "Iwakura's tier on the blow alone, and beside it the class's preference and the fixed lines; the rest of "
                 "the gear's is defence, lines and stones times the family's tier. A counter line: the price's steps in the "
                 "order the pricing takes them - Iwakura's sheet at the yang rate and the inflation, the lines, the sale "
                 "memory, the market ledger, the step limit, the keeper's spread, the floors, the rounding. The flags are "
                 "the core's, the anomalies the panel's.",
  "ex_ai_ask": "Task: point out the decisions that contradict their own numbers or the bot's goal, name the rule "
               "(the code name) that took each, and say what would fix it.",
  "ex_ai_refs": "Reference values (constants of the code)", "ex_ai_equip": "Equipment change", "ex_ai_listing": "Counter line",
  "ex_ai_now": "now", "ex_ai_decision": "Decision", "ex_ai_level_then": "level at the decision: {level}",
  "ex_sheet_ratio": "unit price against the sheet: {unit} / {sheet} = {ratio}", "ex_ai_code": "code",
  "ex_ai_n": "Decisions: {n}",
 },
 "de": {
  "ex_copy_ai": "Für eine KI kopieren", "ex_copied_ai": "Als Markdown für eine KI kopiert",
  "ex_anomalies": "Auffälligkeiten", "ex_ai_title": "Entscheidungen der Metin2-Bots — zur Analyse",
  "ex_ai_guide": "Jede Entscheidung ist eine Zeile, die der Spielkern im Moment der Entscheidung geschrieben hat "
                 "(log.playerbot_equip, log.playerbot_listing); die Namen in `…` sind die Codes aus "
                 "playerbot_explain_rules.h und Konstanten des Overlays. Ein Ausrüstungswechsel: der Durchlauf legt ein "
                 "Stück an, dessen Wert (TERM_TOTAL) den des getragenen um SWAP_MARGIN_PERMILLE Promille übertrifft, ein "
                 "innerhalb der Stunde abgelegtes Stück erst um SWAP_BACK_MARGIN_PERCENT Prozent. Der Wert einer Waffe ist "
                 "ihr Schlag x1000 (TERM_BLOW_X1000, das Modell von GetPlayerBotWeaponHitDamageAt: Angriffsstufe zu Fuß, "
                 "Wurf der Waffe, Angriff aus dem Plus, Verteidigung eines Monsters auf dem Level des Bots, "
                 "Durchschnitts- und Fertigkeitsschaden, der versteckte Levelbonus der Waffe auf mt2009) mit Iwakuras Stufe "
                 "nur auf dem Schlag, daneben die Klassenvorliebe und die festen Zeilen; der Rest der Ausrüstung: "
                 "Verteidigung, Zeilen und Steine mal der Stufe der Familie. Eine Ladentischzeile: die Schritte des Preises "
                 "in der Reihenfolge der Bepreisung - Iwakuras Liste zur Yang-Rate und Inflation, die Zeilen, das "
                 "Verkaufsgedächtnis, das Marktbuch, die Schrittbegrenzung, die Spanne des Verkäufers, die Untergrenzen, "
                 "die Rundung. Die Markierungen stammen vom Kern, die Auffälligkeiten vom Panel.",
  "ex_ai_ask": "Aufgabe: zeige die Entscheidungen, die ihren eigenen Zahlen oder dem Ziel des Bots widersprechen, nenne "
               "die Regel (den Code-Namen), die jede getroffen hat, und was sie beheben würde.",
  "ex_ai_refs": "Referenzwerte (Konstanten des Codes)", "ex_ai_equip": "Ausrüstungswechsel", "ex_ai_listing": "Ladentischzeile",
  "ex_ai_now": "jetzt", "ex_ai_decision": "Entscheidung", "ex_ai_level_then": "Level bei der Entscheidung: {level}",
  "ex_sheet_ratio": "Stückpreis gegenüber der Liste: {unit} / {sheet} = {ratio}", "ex_ai_code": "Code",
  "ex_ai_n": "Entscheidungen: {n}",
 },
 "tr": {
  "ex_copy_ai": "Yapay zekâ için kopyala", "ex_copied_ai": "Yapay zekâ için Markdown olarak kopyalandı",
  "ex_anomalies": "Anormallikler", "ex_ai_title": "Metin2 bot kararları — analiz için",
  "ex_ai_guide": "Her karar, oyun çekirdeğinin karar anında yazdığı bir satırdır (log.playerbot_equip, "
                 "log.playerbot_listing); `…` içindeki adlar playerbot_explain_rules.h kodları ve katmanın sabitleridir. "
                 "Ekipman değişikliği: tarama, puanı (TERM_TOTAL) giyilenden SWAP_MARGIN_PERMILLE binde fazla olan parçayı "
                 "kuşanır; son bir saatte çıkardığı parçayı ise ancak SWAP_BACK_MARGIN_PERCENT yüzde fazlaysa. Bir silahın "
                 "puanı darbesi x1000'dir (TERM_BLOW_X1000, GetPlayerBotWeaponHitDamageAt modeli: yayan saldırı derecesi, "
                 "silahın zarı, artıdan gelen saldırı, botun seviyesindeki bir canavarın savunması, ortalama ve beceri "
                 "hasarı, mt2009'un silah seviyesine bağlı gizli bonusu), Iwakura kademesi yalnızca darbeye uygulanır, "
                 "yanında sınıf tercihi ve sabit satırlar; ekipmanın geri kalanı: savunma, satırlar ve taşlar çarpı ailenin "
                 "kademesi. Tezgâh satırı: fiyat adımları fiyatlama sırasıyla - yang oranı ve enflasyonla Iwakura'nın "
                 "listesi, satırlar, satış hafızası, pazar defteri, adım sınırı, satıcı marjı, tabanlar, yuvarlama. "
                 "İşaretler çekirdeğin, anormallikler panelin.",
  "ex_ai_ask": "Görev: kendi sayılarıyla ya da botun hedefiyle çelişen kararları göster, her birini alan kuralı (kod "
               "adını) belirt ve neyin düzelteceğini söyle.",
  "ex_ai_refs": "Referans değerleri (kodun sabitleri)", "ex_ai_equip": "Ekipman değişikliği", "ex_ai_listing": "Tezgâh satırı",
  "ex_ai_now": "şimdi", "ex_ai_decision": "Karar", "ex_ai_level_then": "karar anındaki seviye: {level}",
  "ex_sheet_ratio": "adet fiyatı listeye göre: {unit} / {sheet} = {ratio}", "ex_ai_code": "kod",
  "ex_ai_n": "Karar: {n}",
 },
}.items():
    # Only the languages this panel speaks (Polish and English): the German
    # and Turkish words stay beside them for a panel that adds those.
    if _lang in MAP_I18N:
        MAP_I18N[_lang].update(_texts)


# Every bonus line the engine can roll, in the wording the game itself uses.
# Polish and English were generated from three sources that have to agree:
# the server's EApplyTypes enum fixes the numbers, the client's AFFECT_DICT
# maps each number to a locale key, and the client's locale_game.txt holds the
# text - the table that replaced a hand-made one with 71 and 72 the wrong way
# round, the two post-kill recovery chances as plain numbers and no entry for
# 87-91 ("Bonus #89").
#
# The text keeps the client's own placeholder, and that is what says how to
# print the value: %d%% is a percentage, %d a plain number, %.1f a multiplier,
# and no placeholder at all means the client shows the line with no number
# after it. Ids the client has no text for are absent on purpose. The mt2009
# engine numbers these lines as POINT_*; the live map puts a type through
# POINT_TO_APPLY before it looks here. Only the page's own language goes into
# the page (apply_meta_for), so an English page carries no Polish.
APPLY_TEXTS = {
 1: ("flat", {"pl": "Max PŻ: +%d", "en": "Max. HP +%d"}),
 2: ("flat", {"pl": "Max PE: +%d", "en": "Max. SP +%d"}),
 3: ("flat", {"pl": "Witalność +%d", "en": "Vitality +%d"}),
 4: ("flat", {"pl": "Inteligencja +%d", "en": "Intelligence +%d"}),
 5: ("flat", {"pl": "Siła: +%d", "en": "Strength +%d"}),
 6: ("flat", {"pl": "Zręczność +%d", "en": "Dexterity +%d"}),
 7: ("percent", {"pl": "Szybkość Ataku +%d%%", "en": "Attack Speed +%d%%"}),
 8: ("percent", {"pl": "Szybkość Ruchu %d%%", "en": "Moving Speed %d%%"}),
 9: ("percent", {"pl": "Szybkość Zaklęcia +%d%%", "en": "Casting Speed +%d%%"}),
 10: ("percent", {"pl": "Regeneracja PŻ: +%d%%", "en": "HP Regeneration +%d%%"}),
 11: ("percent", {"pl": "Regeneracja PE: +%d%%", "en": "SP Regeneration +%d%%"}),
 12: ("percent", {"pl": "Szansa na Otrucie %d%%", "en": "Poisoning chance %d%%"}),
 13: ("percent", {"pl": "Szansa na Omdlenie %d%%", "en": "Blackout chance %d%%"}),
 14: ("percent", {"pl": "Szansa na Spowolnienie %d%%", "en": "Slowing chance %d%%"}),
 15: ("percent", {"pl": "Szansa na cios krytyczny +%d%%", "en": "Chance of critical hit +%d%% "}),
 16: ("percent", {"pl": "Szansa na przeszywające Uderzenie: %d%% ", "en": "%d%% Chance for piercing Hits"}),
 17: ("percent", {"pl": "Silny przeciwko Ludziom +%d%%", "en": "Strong against Half Humans +%d%%"}),
 18: ("percent", {"pl": "Silny przeciwko Zwierzętom +%d%%", "en": "Strong against Animals +%d%%"}),
 19: ("percent", {"pl": "Silny przeciwko Orkom +%d%%", "en": "Strong against Orcs +%d%%"}),
 20: ("percent", {"pl": "Silny przeciwko Mistykom +%d%%", "en": "Strong against Mystics +%d%%"}),
 21: ("percent", {"pl": "Silny przeciwko Nieumarłym +%d%%", "en": "Strong against Undead +%d%%"}),
 22: ("percent", {"pl": "Silny przeciwko Diabłom +%d%%", "en": "Strong against Devils +%d%%"}),
 23: ("percent", {"pl": "%d%% obrażeń będzie dodanych do PŻ", "en": "%d%% damage  will be absorbed by HP"}),
 24: ("percent", {"pl": "%d%% obrażeń będzie dodanych do PE", "en": "%d%% damage will be absorbed by SP"}),
 25: ("percent", {"pl": "%d%% Szansa na kradzież PE", "en": "%d%% chance to rob SP"}),
 26: ("percent", {"pl": "Szansa na odzyskanie PE: %d%%", "en": "%d%% Chance to get back SP when hit"}),
 27: ("percent", {"pl": "Szansa na blok ciosów %d%%", "en": "Chance to block a close-combat attack %d%% "}),
 28: ("percent", {"pl": "Szansa na uniknięcie Strzały: %d%%", "en": "Chance to avoid Arrows %d%%"}),
 29: ("percent", {"pl": "Odporność na Miecze: %d%%", "en": "Sword Defence %d%%"}),
 30: ("percent", {"pl": "Odporność na Broń Dwuręczną: %d%%", "en": "Two-Handed Defence %d%%"}),
 31: ("percent", {"pl": "Odporność na Sztylety: %d%%", "en": "Dagger Defence %d%%"}),
 32: ("percent", {"pl": "Odporność na Dzwony: %d%%", "en": "Bell Defence %d%%"}),
 33: ("percent", {"pl": "Odporność na Wachlarze: %d%%", "en": "Fan Defence %d%%"}),
 34: ("percent", {"pl": "Odporność na Strzały: %d%%", "en": "Arrow Resistance %d%%"}),
 35: ("percent", {"pl": "Odporność na Ogień: %d%%", "en": "Fire Resistance %d%%"}),
 36: ("percent", {"pl": "Odporność na Błyskawice: %d%%", "en": "Lightning Resistance %d%%"}),
 37: ("percent", {"pl": "Odporność na Magię: %d%%", "en": "Magic Resistance %d%%"}),
 38: ("percent", {"pl": "Odporność na Wiatr: %d%%", "en": "Wind Resistance %d%%"}),
 39: ("percent", {"pl": "%d%% Szansa na odbicie Ciosu", "en": "%d%% Chance to reflect close combat hits  "}),
 40: ("percent", {"pl": "Szansa na odbicie Klątwy: %d%%", "en": "Chance to reflect Curse: %d%%"}),
 41: ("percent", {"pl": "Odporność na Trucizny: %d%%", "en": "Poison Resistance %d%%"}),
 42: ("percent", {"pl": "Szansa na odzyskanie PE: %d%%", "en": "%d%% Chance to restore SP"}),
 43: ("percent", {"pl": "Szansa na Bonus DOŚ: %d%%", "en": "%d%% Chance for EXP Bonus"}),
 44: ("percent", {"pl": "Szansa na podwójną ilość Yang: %d%%", "en": "%d%% Chance to drop double Yang"}),
 45: ("percent", {"pl": "Szansa na podwójną ilość Przedmiotów: %d%%", "en": "%d%% Chance to drop double the Items"}),
 46: ("percent", {"pl": "Mikstury %d%% efekt podniesiony", "en": "Potion %d%% effect raise"}),
 47: ("percent", {"pl": "Szansa na odzyskanie PŻ: %d%%", "en": "%d%% Chance, to restore HP"}),
 48: ("boolean", {"pl": "Odporność na Omdlenia", "en": "Defence against blackouts"}),
 49: ("boolean", {"pl": "Odporność na Spowolnienia", "en": "Defence against slowing"}),
 50: ("boolean", {"pl": "Niewrażliwy na Upadek", "en": "Immune against falling down"}),
 52: ("flat", {"pl": "Zasięg Łuku +%dm", "en": "Arc Range +%dm"}),
 53: ("flat", {"pl": "Wartość Ataku +%d", "en": "Attack Value +%d"}),
 54: ("flat", {"pl": "Obrona +%d", "en": "Defence +%d"}),
 55: ("flat", {"pl": "Wartość Magicznego Ataku: +%d", "en": "Magical Attack Value +%d"}),
 56: ("flat", {"pl": "Magiczna Obrona: +%d", "en": "Magical Defence +%d"}),
 58: ("flat", {"pl": "Max Wytrzymałość: +%d", "en": "Max. Endurance +%d"}),
 59: ("percent", {"pl": "Silny przeciwko Wojownikom +%d%%", "en": "Strong against Warriorr +%d%%"}),
 60: ("percent", {"pl": "Silny przeciwko Ninja +%d%%", "en": "Strong against Ninjas +%d%%"}),
 61: ("percent", {"pl": "Silny przeciwko Sura +%d%%", "en": "Strong against Sura +%d%%"}),
 62: ("percent", {"pl": "Silny przeciwko Szamanom +%d%%", "en": "Strong against Shamans +%d%%"}),
 63: ("percent", {"pl": "Silny przeciwko Potworom +%d%%", "en": "Strength against monsters +%d%%"}),
 64: ("percent", {"pl": "Wartość Ataku: +%d%%", "en": "Attack Value +%d%%"}),
 65: ("percent", {"pl": "Obrona: +%d%%", "en": "Defence +%d%%"}),
 66: ("percent", {"pl": "Punkty Doświadczenia: +%d%%", "en": "EXP +%d%%"}),
 67: ("multiplier", {"pl": "Szansa na zdobycie Przedmiotów pomnożona o %.1f", "en": "Chance of capturing Items multiplied with %.1f"}),
 68: ("multiplier", {"pl": "Szansa na zdobycie Yang pomnożona o %.1f", "en": "Chance of capturing Yang multiplied with %.1f"}),
 69: ("percent", {"pl": "Maks. PŻ +%d%%", "en": "Max. HP +%d%%"}),
 70: ("percent", {"pl": "Maks. PE +%d%% ", "en": "Max. SP +%d%% "}),
 71: ("percent", {"pl": "Obrażenie Umiejętności: %d%%", "en": "Skill Damage %d%%"}),
 72: ("percent", {"pl": "Średnie Obrażenia: %d%%", "en": "Average Damage %d%%"}),
 73: ("percent", {"pl": "Odporność na Obrażenia Umiejętności %d%%", "en": "Resistance against Skill Damage %d%%"}),
 74: ("percent", {"pl": "Odporność na Obrażenia: %d%%", "en": "Average Damage Resistance %d%%"}),
 75: ("percent", {"pl": "iCafe DOŚ Bonus +%d%%", "en": "iCafe EXP Bonus +%d%%"}),
 76: ("percent_decimal", {"pl": "iCafe Szansa na zdobycie Przedmiotów plus %.1f%%", "en": "iCafe Chance of capturing Items plus %.1f%%"}),
 78: ("percent", {"pl": "Odporność na Wojowników: %d%%", "en": "Defence chance against warrior attacks: %d%%"}),
 79: ("percent", {"pl": "Odporność na Ninja: %d%%", "en": "Defence chance against ninja attacks: %d%%"}),
 80: ("percent", {"pl": "Odporność na Sura: %d%%", "en": "Defence chance against sura attacks: %d%%"}),
 81: ("percent", {"pl": "Odporność na Szamanów: %d%%", "en": "Defence chance against shaman attacks: %d%%"}),
 82: ("flat", {"pl": "Energia %d", "en": "Energy %d "}),
 84: ("percent", {"pl": "Bonus kostiumu %d%% ", "en": "Costume bonus %d%% "}),
 85: ("percent", {"pl": "Magiczny atak +%d%%", "en": "Magic attack +%d%%"}),
 86: ("percent", {"pl": "Magiczny/krótkodystansowy atak +%d%%", "en": "Magic/melee attack +%d%%"}),
 87: ("percent", {"pl": "Odporność na lód +%d%%", "en": "Ice resistance +%d%%"}),
 88: ("percent", {"pl": "Odporność na ziemię +%d%%", "en": "Earth resistance +%d%%"}),
 89: ("percent", {"pl": "Odporność na mrok +%d%%", "en": "Resistance against darkness +%d%%"}),
 90: ("percent", {"pl": "Odporność na cios krytyczny +%d%%", "en": "Resistance against critical hits +%d%%"}),
 91: ("percent", {"pl": "Odporność na przeszywający cios +%d%%", "en": "Resistance against piercing hits +%d%%"}),
 1138: ("percent", {"pl": "Terror +%d%%", "en": "Terror +%d%%"}),
 1139: ("percent", {"pl": "Regeneracja wytrzymałości +%d%%", "en": "Stamina regeneration +%d%%"}),
 1140: ("flat", {"pl": "Atak sztyletem przeciw potworom +%d", "en": "Dagger attack against monsters +%d"}),
 1141: ("flat", {"pl": "Wartość ataku przeciw potworom +%d", "en": "Attack value against monsters +%d"}),
 1142: ("flat", {"pl": "Odporność na potwory +%d‰", "en": "Resistance against monsters +%d‰"}),
 1143: ("percent", {"pl": "Pochłanianie obrażeń +%d%%", "en": "Damage absorption +%d%%"}),
 1144: ("percent", {"pl": "Pochłanianie obrażeń od potworów +%d%%", "en": "Damage absorption from monsters +%d%%"}),
 1145: ("boolean", {"pl": "Przełamanie odporności na ogłuszenie", "en": "Breaks stun immunity"}),
 1146: ("boolean", {"pl": "Przełamanie klątwy świątyni", "en": "Breaks the temple curse"}),
 1147: ("percent", {"pl": "Czas trwania umiejętności +%d%%", "en": "Skill duration +%d%%"}),
 1148: ("percent", {"pl": "Silny przeciw potworom z Doliny Orków +%d%%", "en": "Strong against Orc Valley monsters +%d%%"}),
 1149: ("percent", {"pl": "Silny przeciw Metinom +%d%%", "en": "Strong against Metin stones +%d%%"}),
 1150: ("percent", {"pl": "Silny przeciw bossom +%d%%", "en": "Strong against bosses +%d%%"}),
 1151: ("percent", {"pl": "Magiczny atak przeciw potworom +%d%%", "en": "Magic attack against monsters +%d%%"}),
 1152: ("percent", {"pl": "Przełamanie odporności na miecz +%d%%", "en": "Breaks sword resistance +%d%%"}),
 1153: ("percent", {"pl": "Przełamanie odporności na broń dwuręczną +%d%%", "en": "Breaks two-handed resistance +%d%%"}),
 1154: ("percent", {"pl": "Przełamanie odporności na sztylet +%d%%", "en": "Breaks dagger resistance +%d%%"}),
 1155: ("percent", {"pl": "Przełamanie odporności na dzwonek +%d%%", "en": "Breaks bell resistance +%d%%"}),
 1156: ("percent", {"pl": "Przełamanie odporności na wachlarz +%d%%", "en": "Breaks fan resistance +%d%%"}),
 1157: ("percent", {"pl": "Przełamanie odporności na łuk +%d%%", "en": "Breaks bow resistance +%d%%"}),
 1158: ("percent", {"pl": "Szansa na zbieranie +%d%%", "en": "Collecting chance +%d%%"}),
 1159: ("percent", {"pl": "Szansa na naukę +%d%%", "en": "Learning chance +%d%%"}),
 1160: ("percent", {"pl": "Odporność na ludzi +%d%%", "en": "Resistance against humans +%d%%"}),
 1161: ("flat", {"pl": "Magiczny atak +%d", "en": "Magic attack +%d"}),
 1162: ("percent", {"pl": "Szansa na podpalenie +%d%%", "en": "Chance of burning +%d%%"}),
 1163: ("percent", {"pl": "Zamiana obrażeń na PE +%d%%", "en": "Damage converted to SP +%d%%"}),
 1164: ("percent", {"pl": "Szansa na rzadki łup +%d%%", "en": "Rare drop chance +%d%%"}),
 1165: ("flat", {"pl": "Magiczna wartość ataku przeciw potworom +%d", "en": "Magic attack value against monsters +%d"}),
 1166: ("percent", {"pl": "Szansa na unieruchomienie +%d%%", "en": "Chance of rooting +%d%%"}),
 1167: ("flat", {"pl": "Atak specjalny +%d", "en": "Special attack +%d"}),
 1168: ("percent", {"pl": "Kara za śmierć +%d%%", "en": "Death penalty +%d%%"}),
}


def map_i18n(language=None):
    language = language or (lang() if has_request_context() else "en")
    return MAP_I18N.get(language, MAP_I18N["en"])


JOB_NAMES_MAP = {
 "pl": {0:"Wojownik (M)",4:"Wojowniczka (K)",1:"Ninja (M)",5:"Ninja (K)",
        2:"Sura (M)",6:"Sura (K)",3:"Szaman (M)",7:"Szamanka (K)"},
 "en": {0:"Warrior (M)",4:"Warrior (F)",1:"Ninja (M)",5:"Ninja (F)",
        2:"Sura (M)",6:"Sura (F)",3:"Shaman (M)",7:"Shaman (F)"},
}


def localized_job_name(job, language=None):
    language = language or (lang() if has_request_context() else "en")
    names = JOB_NAMES_MAP.get(language, JOB_NAMES_MAP["en"])
    return names.get(int(job or 0), names[0])


# The bots' explanations laid out: the server says each one whole in the page's
# language (explain_listing, explain_equip), and this is the one place that
# draws it - for the live map's shop tooltip and pane and its gear history, and
# for the /decisions page - and turns it into plain text for a report. It reads
# the page's own I18N (map_i18n). Put into both templates as text, so it must
# carry nothing Jinja would read: no double braces and no brace-hash.
EXPLAIN_CSS = """
.m2ex{font-size:11px;line-height:1.4;color:#d4d4d8;text-align:left}
.m2ex-h{margin:6px 0 2px;color:#fbbf24;font-weight:700}
.m2ex-muted{color:#a1a1aa}
.m2ex-warn{color:#fca5a5;margin-top:3px}
.m2ex-flags{margin:3px 0}
.m2ex-flag{display:inline-block;margin:1px 3px 1px 0;padding:0 5px;border-radius:3px;border:1px solid #3f3f46;
  background:#18181b;color:#d4d4d8;font-size:10px;font-weight:600;line-height:15px}
.m2ex-flag.unusual{border-color:#ef4444;background:#3f1111;color:#fecaca}
table.m2ex-t{border-collapse:collapse;width:100%;margin:3px 0}
table.m2ex-t th,table.m2ex-t td{padding:1px 4px;border-bottom:1px solid #27272a;font-size:10px;line-height:1.35;
  text-align:left;vertical-align:top;background:none;color:#d4d4d8}
table.m2ex-t th{color:#a1a1aa;text-transform:none;letter-spacing:0}
table.m2ex-t .n{text-align:right;white-space:nowrap}
table.m2ex-t tr.m2ex-top td{background:rgba(251,191,36,.14);color:#fde68a;font-weight:700}
.m2ex-cols{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin:4px 0}
.m2ex-piece{background:#101012;border:1px solid #27272a;border-radius:4px;padding:4px 5px}
.m2ex-lines{color:#4ade80}
.m2ex-codes{margin-top:5px;font-family:monospace;font-size:9px;color:#71717a;word-break:break-all}
.m2ex-box{margin:3px 0 6px;padding:6px 8px;background:#0c0c0e;border:1px solid #3f3f46;border-radius:6px}
button.m2ex-btn{padding:1px 8px;font-size:10px;background:#334155;color:#fff;border:none;border-radius:4px;
  cursor:pointer;margin:0 0 0 4px;box-shadow:none;font-weight:600}
button.m2ex-q{padding:0 6px;font-size:10px;line-height:15px;background:#1f2937;color:#fbbf24;border:1px solid #fbbf24;
  border-radius:8px;cursor:pointer;margin:0 0 0 4px;box-shadow:none;font-weight:700}
.m2ex-flag.anomaly{border-color:#f59e0b;background:#3a2606;color:#fde68a}
"""


# The numbers the equipment pass and the counter compare against, for the AI
# export's reference block (exMarkdown): each a constant of the overlay under
# the name it has there, with the header it is in. The test reads each out of
# its header, so a change there not made here fails it.
DECISION_REFERENCE = (
    ("SWAP_MARGIN_PERMILLE", 10, "playerbot_swap_rules.h"),
    ("SWAP_BACK_MARGIN_PERCENT", 5, "playerbot_swap_rules.h"),
    ("WEAPON_TIER_STEP_PERCENT", 2, "playerbot_swap_rules.h"),
    ("PLAYERBOT_TIER_SCORE_PERCENT", 8, "playerbot_types.h"),
    ("PLAYERBOT_MONSTER_DEFENCE_OVER_LEVEL", 15, "playerbot_types.h"),
    ("PLAYERBOT_WEAPON_OWN_LINE_PERCENT", 100, "playerbot_types.h"),
    ("PLAYERBOT_WEAPON_OTHER_LINE_PERCENT", 35, "playerbot_types.h"),
    ("LOW_WEAPON_BAN_FROM_LEVEL", 30, "playerbot_refine_rules.h"),
    ("LOW_WEAPON_MAX_LEVEL", 10, "playerbot_refine_rules.h"),
    ("ANVIL_LEVEL_BAND", 15, "playerbot_refine_rules.h"),
    ("FLIP_FLOP_WINDOW_SECONDS", 3600, "playerbot_explain_rules.h"),
)


EXPLAIN_JS = """
var EX_NL = String.fromCharCode(10);
var EX_REFS = """ + json.dumps([list(r) for r in DECISION_REFERENCE]) + """;

function exEsc(value) {
  return String(value === undefined || value === null ? '' : value)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#039;');
}

function exFlagsHtml(flags) {
  if (!flags || !flags.length) return '';
  var html = '<div class="m2ex-flags">';
  flags.forEach(function(f) {
    html += '<span class="m2ex-flag' + (f.unusual ? ' unusual' : '') + '">' + exEsc(f.label) + '</span>';
  });
  return html + '</div>';
}

function exUnusualBadge(ex) {
  return ex && ex.unusual ? ' <span class="m2ex-flag unusual">' + exEsc(I18N.ex_unusual) + '</span>' : '';
}

function exStepsHtml(steps, cut) {
  if (!steps || !steps.length) return '';
  var html = '<table class="m2ex-t"><tr><th>' + exEsc(I18N.ex_col_step) + '</th><th class="n">' +
             exEsc(I18N.ex_col_effect) + '</th><th class="n">' + exEsc(I18N.ex_col_value) + '</th></tr>';
  steps.forEach(function(s) {
    html += '<tr><td><b>' + exEsc(s.label) + '</b>' +
            (s.detail ? ' <span class="m2ex-muted">' + exEsc(s.detail) + '</span>' : '') +
            '</td><td class="n">' + exEsc(s.effect) + '</td><td class="n">' + exEsc(s.value) + '</td></tr>';
  });
  html += '</table>';
  if (cut) html += '<div class="m2ex-muted">' + exEsc(I18N.ex_steps_cut) + '</div>';
  return html;
}

// Why a line is on its counter and how its price came about. `full` adds the
// last change's own steps and the codes: the pane has room, a tooltip not.
function exListingHtml(ex, full) {
  if (!ex) return '<div class="m2ex m2ex-muted">' + exEsc(I18N.ex_none) + '</div>';
  var html = '<div class="m2ex"><div class="m2ex-h">' + exEsc(I18N.ex_why_counter) + exUnusualBadge(ex) + '</div>';
  html += '<div>' + exEsc(ex.goods) + '</div>';
  ['stand', 'pick', 'cut', 'listed'].forEach(function(key) {
    if (ex[key]) html += '<div class="m2ex-muted">' + exEsc(ex[key]) + '</div>';
  });
  html += exFlagsHtml(ex.flags) + exAnomaliesHtml(ex.anomalies);
  html += '<div class="m2ex-h">' + exEsc(I18N.ex_how_price) + '</div>' + exStepsHtml(ex.steps, ex.steps_cut);
  if (ex.sheet) html += '<div class="m2ex-muted">' + exEsc(ex.sheet) + '</div>';
  if (ex.last) {
    html += '<div class="m2ex-h">' + exEsc(I18N.ex_last_change) + ': ' + exEsc(ex.last.title) + '</div>' +
            '<div class="m2ex-muted">' + exEsc(ex.last.line) + '</div>';
    if (full) html += exStepsHtml(ex.last.steps, ex.last.steps_cut);
  }
  if (ex.end) html += '<div class="m2ex-warn">' + exEsc(ex.end) + '</div>';
  if (ex.price_note) html += '<div class="m2ex-warn">' + exEsc(ex.price_note) + '</div>';
  if (full && ex.codes) html += '<div class="m2ex-codes">' + exEsc(I18N.ex_codes) + ': ' + exEsc(ex.codes) + '</div>';
  return html + '</div>';
}

function exPieceHtml(title, piece) {
  var html = '<div class="m2ex-piece"><div class="m2ex-muted">' + exEsc(title) + '</div>';
  if (!piece) return html + '<div>' + exEsc(I18N.ex_nothing) + '</div></div>';
  html += '<div><b>' + exEsc(piece.name) + '</b></div>' +
          '<div class="m2ex-muted">' + exEsc(I18N.ex_score_word) + ': ' + exEsc(piece.score) + '</div>';
  if (piece.roles && piece.roles.length) {
    html += '<div>' + piece.roles.map(function(r) { return '<span class="m2ex-flag">' + exEsc(r) + '</span>'; }).join('') + '</div>';
  }
  if (piece.lines && piece.lines.length) html += '<div class="m2ex-lines">' + piece.lines.map(exEsc).join('<br>') + '</div>';
  if (piece.stones && piece.stones.length) {
    html += '<div class="m2ex-muted">' + exEsc(I18N.ex_stones) + ': ' + piece.stones.map(exEsc).join(', ') + '</div>';
  }
  return html + '</div>';
}

function exTermsHtml(title, rows) {
  if (!rows || !rows.length) return '';
  var html = '<div class="m2ex-h">' + exEsc(title) + '</div><table class="m2ex-t"><tr><th>' + exEsc(I18N.ex_col_term) +
             '</th><th class="n">' + exEsc(I18N.ex_old) + '</th><th class="n">' + exEsc(I18N.ex_new) +
             '</th><th class="n">' + exEsc(I18N.ex_delta) + '</th></tr>';
  rows.forEach(function(r) {
    // m2ex-top and never "top", which is BASE's header bar (display:flex).
    html += '<tr' + (r.top ? ' class="m2ex-top"' : '') + '><td>' + exEsc(r.label) + '</td><td class="n">' + exEsc(r.old) +
            '</td><td class="n">' + exEsc(r.new) + '</td><td class="n">' + exEsc(r.delta) + '</td></tr>';
  });
  return html + '</table>';
}

// Why a piece went on or came off: the pass and rule, the two pieces side by
// side, the score's terms old against new (the three that moved it most lit),
// and a weapon's blow the same way.
function exEquipHtml(ex, full) {
  if (!ex) return '';
  var html = '<div class="m2ex"><div class="m2ex-h">' + exEsc(ex.title) + exUnusualBadge(ex) + '</div>';
  html += '<div><b>' + exEsc(ex.summary) + '</b></div><div class="m2ex-muted">' + exEsc(ex.slot) +
          (ex.time ? ' · ' + exEsc(ex.time) : '') + '</div>';
  if (ex.context) html += '<div class="m2ex-muted">' + exEsc(I18N.ex_context) + ': ' + exEsc(ex.context) + '</div>';
  if (ex.origin) html += '<div class="m2ex-muted">' + exEsc(I18N.ex_origin) + ': ' + exEsc(ex.origin) + '</div>';
  if (ex.gain) html += '<div>' + exEsc(I18N.ex_gain) + ': <b>' + exEsc(ex.gain) + '</b></div>';
  if (ex.suppressed) html += '<div class="m2ex-warn">' + exEsc(ex.suppressed) + '</div>';
  html += exFlagsHtml(ex.flags) + exAnomaliesHtml(ex.anomalies);
  html += '<div class="m2ex-cols">' + exPieceHtml(I18N.ex_new, ex['new']) + exPieceHtml(I18N.ex_old, ex.old) + '</div>';
  html += exTermsHtml(I18N.ex_terms, ex.terms);
  html += exTermsHtml(I18N.ex_blow, ex.blow);
  if (full && ex.codes) html += '<div class="m2ex-codes">' + exEsc(I18N.ex_codes) + ': ' + exEsc(ex.codes) + '</div>';
  return html + '</div>';
}

function exHtml(ex, full) {
  return ex && ex.kind === 'equip' ? exEquipHtml(ex, full) : exListingHtml(ex, full);
}

function exStepsText(steps, cut) {
  var out = [];
  (steps || []).forEach(function(s) {
    out.push('  ' + s.label + (s.detail ? ' (' + s.detail + ')' : '') + (s.effect ? ' ' + s.effect : '') +
             (s.value ? ' = ' + s.value : ''));
  });
  if (cut) out.push('  ' + I18N.ex_steps_cut);
  return out;
}

function exTermsText(title, rows) {
  if (!rows || !rows.length) return [];
  var out = [title + ':'];
  rows.forEach(function(r) {
    out.push('  ' + (r.top ? '* ' : '') + r.label + ': ' + r.old + ' -> ' + r['new'] + (r.delta ? ' (' + r.delta + ')' : ''));
  });
  return out;
}

// An explanation as plain text for a report: the bot, the item, the time,
// the words as the page shows them, and the codes the numbers came from.
function exText(ex, botName, pid, itemName) {
  var lines = [I18N.ex_bot + ': ' + (botName || '') + ' (pid ' + pid + ')'];
  if (!ex) return lines.join(EX_NL);
  if (ex.kind === 'equip') {
    lines.push(I18N.ex_time + ': ' + ex.time);
    lines.push(ex.title + ': ' + ex.summary + ' · ' + ex.slot);
    if (ex.context) lines.push(I18N.ex_context + ': ' + ex.context);
    if (ex.origin) lines.push(I18N.ex_origin + ': ' + ex.origin);
    [[I18N.ex_new, ex['new']], [I18N.ex_old, ex.old]].forEach(function(pair) {
      var p = pair[1];
      lines.push(pair[0] + ': ' + (!p ? I18N.ex_nothing : p.name + ' · ' + I18N.ex_score_word + ' ' + p.score +
                 (p.roles.length ? ' · ' + I18N.ex_roles + ': ' + p.roles.join(', ') : '') +
                 (p.lines.length ? ' · ' + I18N.ex_lines + ': ' + p.lines.join('; ') : '') +
                 (p.stones.length ? ' · ' + I18N.ex_stones + ': ' + p.stones.join(', ') : '')));
    });
    if (ex.gain) lines.push(I18N.ex_gain + ': ' + ex.gain);
    if (ex.suppressed) lines.push(ex.suppressed);
    lines = lines.concat(exTermsText(I18N.ex_terms, ex.terms), exTermsText(I18N.ex_blow, ex.blow));
  } else {
    lines.push(I18N.ex_item + ': ' + (itemName || '') + ' (id ' + ex.item_id + ')');
    lines.push(I18N.ex_time + ': ' + ex.listed_time + (ex.last_time && ex.last_time !== ex.listed_time ? ' / ' + ex.last_time : ''));
    lines.push(I18N.ex_why_counter + ': ' + ex.goods);
    ['stand', 'pick', 'cut', 'listed'].forEach(function(key) { if (ex[key]) lines.push('  ' + ex[key]); });
    lines.push(I18N.ex_how_price + ':');
    lines = lines.concat(exStepsText(ex.steps, ex.steps_cut));
    if (ex.sheet) lines.push('  ' + ex.sheet);
    if (ex.last) {
      lines.push(I18N.ex_last_change + ': ' + ex.last.title + ' · ' + ex.last.line);
      lines = lines.concat(exStepsText(ex.last.steps, ex.last.steps_cut));
    }
    if (ex.end) lines.push(ex.end);
    if (ex.price_note) lines.push(ex.price_note);
  }
  if (ex.flags && ex.flags.length) {
    lines.push(I18N.ex_flags + ': ' + ex.flags.map(function(f) {
      return f.label + (f.unusual ? ' [' + I18N.ex_unusual + ']' : '');
    }).join(', '));
  }
  if (ex.anomalies && ex.anomalies.length) {
    lines.push(I18N.ex_anomalies + ': ' + ex.anomalies.map(function(a) { return a.label; }).join('; '));
  }
  lines.push(I18N.ex_codes + ': ' + ex.codes);
  return lines.join(EX_NL);
}

// The anomalies the panel read out of a row, beside the core's flags.
function exAnomaliesHtml(anomalies) {
  if (!anomalies || !anomalies.length) return '';
  var html = '<div class="m2ex-flags">';
  anomalies.forEach(function(a) {
    html += '<span class="m2ex-flag anomaly">⚑ ' + exEsc(a.label) + '</span>';
  });
  return html + '</div>';
}

// ---- the export for an analyst or a language model -----------------------
// Markdown with every number the page shows, the rules header's code names
// beside the words, and a guide and the reference values once at the top.
function exMdCell(value) {
  return String(value === undefined || value === null ? '' : value).split('|').join('/').split(EX_NL).join(' ');
}

function exMdCode(name) {
  return name ? ' (`' + name + '`)' : '';
}

function exMdBot(bot, ex) {
  if (!bot) return '';
  var parts = [];
  if (bot.job) parts.push(bot.job + (bot.path ? ', ' + bot.path : ''));
  if (ex && ex.kind === 'equip' && ex.level) parts.push(I18N.ex_ai_level_then.replace('{level}', ex.level));
  var now = [];
  if (bot.level) now.push(String(bot.level));
  ['persona', 'mood', 'goal', 'map'].forEach(function(k) { if (bot[k]) now.push(bot[k]); });
  if (now.length) parts.push(I18N.ex_ai_now + ': ' + now.join(', '));
  if (bot.stats) parts.push(bot.stats);
  return parts.length ? '- **' + I18N.ex_bot + ':** ' + exMdCell(parts.join(' · ')) : '';
}

function exMdFlags(ex) {
  var out = [];
  if (ex.flags && ex.flags.length) {
    out.push('- **' + I18N.ex_flags + ':** ' + ex.flags.map(function(f) {
      return exMdCell(f.label) + exMdCode(f.name) + (f.unusual ? ' [' + I18N.ex_unusual + ']' : '');
    }).join(', '));
  }
  if (ex.anomalies && ex.anomalies.length) {
    out.push('- **' + I18N.ex_anomalies + ':** ' + ex.anomalies.map(function(a) { return exMdCell(a.label); }).join('; '));
  }
  return out;
}

function exMdTerms(title, rows) {
  if (!rows || !rows.length) return [];
  var out = ['', '**' + title + '**', '',
             '| ' + I18N.ex_col_term + ' | ' + I18N.ex_ai_code + ' | ' + I18N.ex_old + ' | ' + I18N.ex_new + ' | ' +
             I18N.ex_delta + ' |', '|---|---|---:|---:|---:|'];
  rows.forEach(function(r) {
    var label = exMdCell(r.label);
    out.push('| ' + (r.top ? '**' + label + '**' : label) + ' | `' + (r.name || r.code) + '` | ' + exMdCell(r.old) +
             ' | ' + exMdCell(r['new']) + ' | ' + exMdCell(r.delta) + ' |');
  });
  return out;
}

function exMdSteps(steps, cut) {
  if (!steps || !steps.length) return [];
  var out = ['', '| # | ' + I18N.ex_col_step + ' | ' + I18N.ex_ai_code + ' | | ' + I18N.ex_col_effect + ' | ' +
             I18N.ex_col_value + ' |', '|---:|---|---|---|---:|---:|'];
  steps.forEach(function(s, i) {
    out.push('| ' + (i + 1) + ' | ' + exMdCell(s.label) + ' | `' + (s.name || s.code) + '` | ' + exMdCell(s.detail) +
             ' | ' + exMdCell(s.effect) + ' | ' + exMdCell(s.value) + ' |');
  });
  if (cut) out.push('', I18N.ex_steps_cut);
  return out;
}

function exMdPiece(piece, field) {
  if (!piece) return field === 'name' ? I18N.ex_nothing : '';
  if (field === 'name') return piece.name + ' (vnum ' + piece.vnum + ', +' + piece.plus + ', id ' + piece.id + ')';
  if (field === 'roles') {
    return (piece.roles || []).map(function(r, i) {
      return r + (piece.role_names && piece.role_names[i] ? ' `' + piece.role_names[i] + '`' : '');
    }).join(', ');
  }
  var value = piece[field];
  if (value && value.join) return value.join(', ');
  return value === undefined || value === null ? '' : String(value);
}

// One decision as Markdown. bot: {name, job, path, level, persona, mood, goal,
// map, stats} - what the page knows of the bot now; itemName for a counter line.
function exMarkdownOne(ex, bot, itemName) {
  if (!ex) return '';
  var name = (bot && bot.name) || '';
  var out = [];
  if (ex.kind === 'equip') {
    out.push('### ' + I18N.ex_ai_equip + ' E' + ex.id + ' · ' + ex.time + ' · ' + name + ' (pid ' + ex.pid + ')');
    var botLine = exMdBot(bot, ex);
    if (botLine) out.push(botLine);
    var names = ex.names || {};
    out.push('- **' + I18N.ex_ai_decision + ':** ' + exMdCell(ex.title + ': ' + ex.summary) + ' (`' + (names.path || '') +
             '` · `' + (names.rule || '') + '`) · ' + exMdCell(ex.slot));
    if (ex.context) out.push('- **' + I18N.ex_context + ':** ' + exMdCell(ex.context) + exMdCode(names.context));
    if (ex.origin) out.push('- **' + I18N.ex_origin + ':** ' + exMdCell(ex.origin) + exMdCode(names.origin));
    if (ex.gain) out.push('- **' + I18N.ex_gain + ':** ' + ex.gain);
    out = out.concat(exMdFlags(ex));
    if (ex.suppressed) out.push('- ' + exMdCell(ex.suppressed));
    out.push('', '| | ' + I18N.ex_new + ' | ' + I18N.ex_old + ' |', '|---|---|---|');
    [['ex_item', 'name'], ['ex_score_word', 'score'], ['ex_roles', 'roles'], ['ex_lines', 'lines'],
     ['ex_stones', 'stones']].forEach(function(row) {
      var a = exMdPiece(ex['new'], row[1]), b = exMdPiece(ex.old, row[1]);
      if (row[1] === 'name' || a || b) out.push('| ' + I18N[row[0]] + ' | ' + exMdCell(a) + ' | ' + exMdCell(b) + ' |');
    });
    out = out.concat(exMdTerms(I18N.ex_terms, ex.terms), exMdTerms(I18N.ex_blow, ex.blow));
  } else {
    out.push('### ' + I18N.ex_ai_listing + ' L' + ex.item_id + ' · ' + ex.listed_time + ' · ' + name + ' (pid ' + ex.pid + ')');
    var botLine2 = exMdBot(bot, ex);
    if (botLine2) out.push(botLine2);
    var ln = ex.names || {};
    out.push('- **' + I18N.ex_item + ':** ' + exMdCell(itemName || '') + ' (vnum ' + ex.vnum + ', × ' + ex.count + ')');
    out.push('- **' + I18N.ex_why_counter + ':** ' + exMdCell(ex.goods) + exMdCode(ln.goods));
    ['stand', 'pick', 'cut', 'listed', 'sheet'].forEach(function(key) {
      if (ex[key]) out.push('- ' + exMdCell(ex[key]) + (key === 'listed' ? exMdCode(ln.list_event) : ''));
    });
    out = out.concat(exMdFlags(ex));
    out.push('', '**' + I18N.ex_how_price + '**');
    out = out.concat(exMdSteps(ex.steps, ex.steps_cut));
    if (ex.last) {
      out.push('', '**' + I18N.ex_last_change + ':** ' + exMdCell(ex.last.title + ' · ' + ex.last.line) + exMdCode(ln.last_event));
      out = out.concat(exMdSteps(ex.last.steps, ex.last.steps_cut));
    }
    if (ex.end) out.push('', '- ' + exMdCell(ex.end) + exMdCode(ln.off || ln.last_event));
    if (ex.price_note) out.push('- ' + exMdCell(ex.price_note));
  }
  out.push('', I18N.ex_codes + ': `' + ex.codes + '`');
  return out.join(EX_NL);
}

// A whole export: the title, the guide, the task, the reference values, and
// every decision given as {ex, bot, item}.
function exMarkdown(items) {
  items = (items || []).filter(function(it) { return it && it.ex; });
  var out = ['# ' + I18N.ex_ai_title, '', I18N.ex_ai_n.replace('{n}', items.length) + ' · ' + new Date().toISOString(),
             '', I18N.ex_ai_guide, '', '**' + I18N.ex_ai_ask + '**', '', '## ' + I18N.ex_ai_refs, '',
             '| | | |', '|---|---:|---|'];
  EX_REFS.forEach(function(r) { out.push('| `' + r[0] + '` | ' + r[1] + ' | ' + r[2] + ' |'); });
  items.forEach(function(it) {
    out.push('', '---', '', exMarkdownOne(it.ex, it.bot, it.item));
  });
  return out.join(EX_NL) + EX_NL;
}

function exCopyAi(items) {
  exCopy(exMarkdown(items), I18N.ex_copied_ai);
}

function exCopyFallback(text) {
  var area = document.createElement('textarea');
  area.value = text;
  area.style.position = 'fixed';
  area.style.opacity = '0';
  document.body.appendChild(area);
  area.select();
  try { document.execCommand('copy'); } catch (e) {}
  document.body.removeChild(area);
}

// The clipboard API only exists on a secure page (https, or the loopback);
// a panel opened over the network by its address falls back on the old way.
function exCopy(text, message) {
  var done = function() {
    var toast = document.getElementById('mapToast');
    if (!toast) return;
    toast.innerText = '📋 ' + (message || I18N.ex_copied);
    toast.style.display = 'block';
    setTimeout(function() { toast.style.display = 'none'; }, 2500);
  };
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(text).then(done, function() { exCopyFallback(text); done(); });
    return;
  }
  exCopyFallback(text);
  done();
}
"""


# What log.log says about one bot's equipment. The engine writes the refines
# (REFINE SUCCESS / REFINE FAIL, and REMOVE (REFINE FAIL) for a piece that
# burned), the stall purchases (SHOP_BUY), the merchant sales the core makes
# through RemoveItem (PLAYERBOT_SHOP_SELL), the stone a bonus reroll consumes
# (PLAYERBOT_BONUS) and the Moonlight chest; the core adds the swap into a
# wear slot (PLAYERBOT_EQUIP), both ends of a gift, the keeper's side of a
# stall sale and the storekeeper deposit. Asked for by a player who wanted to
# know why his top Sura "suddenly flies without her +8".
def log_text(value):
    """A text column of log.log as a str. On mt2009 `type`, `how`, `hint` and
    `ip` are VARBINARY and the driver hands them over as bytes; the game writes
    CP1250 into them. r40250's big5 columns come back as str already."""
    if isinstance(value, (bytes, bytearray)):
        for encoding in ("cp1250", "utf-8", "latin1"):
            try:
                return bytes(value).decode(encoding)
            except UnicodeDecodeError:
                pass
        return bytes(value).decode("cp1250", "replace")
    return value or ""


GEAR_HISTORY_HOWS = {
    "REFINE SUCCESS":        ("refine_ok",   {"pl": "Ulepszenie udane",   "en": "Refine succeeded"}),
    "REFINE FAIL":           ("refine_fail", {"pl": "Ulepszenie nieudane", "en": "Refine failed"}),
    "REMOVE (REFINE FAIL)":  ("burned",      {"pl": "Spalone przy ulepszaniu", "en": "Burned in refine"}),
    "REFINE FISH_ROD SUCCESS": ("refine_ok", {"pl": "Wędka ulepszona",    "en": "Rod refined"}),
    "REFINE FISH_ROD FAIL":  ("refine_fail", {"pl": "Wędka nieulepszona", "en": "Rod refine failed"}),
    "PLAYERBOT_EQUIP":       ("equip",       {"pl": "Założone",           "en": "Equipped"}),
    "PLAYERBOT_GIFT_OUT":    ("gift_out",    {"pl": "Podarowane",         "en": "Given away"}),
    "PLAYERBOT_GIFT_IN":     ("gift_in",     {"pl": "Dostane w prezencie", "en": "Received as gift"}),
    "PLAYERBOT_STALL_SOLD":  ("stall_sold",  {"pl": "Sprzedane na straganie", "en": "Sold at the stall"}),
    "SHOP_BUY":              ("bought",      {"pl": "Kupione na straganie", "en": "Bought at a stall"}),
    "PLAYERBOT_SHOP_SELL":   ("vendor",      {"pl": "Sprzedane handlarzowi", "en": "Sold to merchant"}),
    "PLAYERBOT_BONUS":       ("bonus",       {"pl": "Zużyte na przemianę bonusów", "en": "Used for a bonus reroll"}),
    "PLAYERBOT_BONUS_ADD":   ("bonus",       {"pl": "Dodano bonus (Wzmocnienie)", "en": "Bonus line added"}),
    "PLAYERBOT_BONUS_CHANGE":("bonus",       {"pl": "Zmieniono bonusy (Zmiana)",  "en": "Bonus lines rerolled"}),
    "PLAYERBOT_BONUS_MARBLE":("bonus",       {"pl": "Dodano 5. bonus (Marmur)",   "en": "Fifth line added (marble)"}),
    "SAFEBOX PUT":           ("safebox",     {"pl": "Do magazynu",        "en": "Into the safebox"}),
    "SAFEBOX GET":           ("safebox",     {"pl": "Z magazynu",         "en": "Out of the safebox"}),
    "MOONLIGHT_GET":         ("get",         {"pl": "Ze Szkatułki Blasku", "en": "From a Moonlight chest"}),
    # A bot's purchase from a village merchant and the marble a hundred Magic
    # Dust make (Iwakura's Patch 4, point 11): neither had a row the card read
    # (B23 of Iwakura's audit of 26 September).
    "PLAYERBOT_NPC_BUY":     ("bought",      {"pl": "Kupione u handlarza", "en": "Bought from a merchant",
                             "de": "Beim Händler gekauft", "tr": "Satıcıdan satın alındı"}),
    "PLAYERBOT_DUST_MARBLE": ("bonus",       {"pl": "Marmur z Magicznego Pyłu", "en": "Marble from Magic Dust",
                             "de": "Marmor aus Magischem Staub", "tr": "Sihirli Tozdan Mermer"}),
    # Iwakura's exchange of surplus refine materials (29 September): the stone
    # or the marble it gave, the reward a failed roll did not give, and the
    # materials it took.
    "PLAYERBOT_MATERIAL_EXCHANGE": ("bonus", {"pl": "Z wymiany ulepszaczy", "en": "From a material exchange",
                             "de": "Aus einem Materialtausch", "tr": "Malzeme takasından"}),
    "PLAYERBOT_MATERIAL_EXCHANGE_FAIL": ("refine_fail", {"pl": "Wymiana ulepszaczy nieudana",
                             "en": "Material exchange failed", "de": "Materialtausch fehlgeschlagen",
                             "tr": "Malzeme takası başarısız"}),
    "PLAYERBOT_MATERIAL_EXCHANGE_IN": ("vendor", {"pl": "Oddane na wymianę ulepszaczy",
                             "en": "Given in a material exchange", "de": "Für einen Materialtausch abgegeben",
                             "tr": "Malzeme takasına verildi"}),
    "EXCHANGE_TAKE":         ("gift_in",     {"pl": "Z wymiany",          "en": "From a trade",
                             "de": "Aus einem Handel", "tr": "Ticaretten"}),
    "EXCHANGE_GIVE":         ("gift_out",    {"pl": "Oddane w wymianie",  "en": "Given in a trade",
                             "de": "Im Handel abgegeben", "tr": "Ticarette verildi"}),
}


# ---- The bots' explanations ----------------------------------------------------
# Why a bot put a line on its counter and how it reached the price, and why it
# changed a worn piece ("why did it swap Miecz Pelni Ksiezyca +6 for Krwawy Miecz
# +4" - the question the operator wants to be able to answer and report). The
# mt2009 core writes two InnoDB tables in `log` beside the decisions themselves:
# playerbot_listing, one row per counter line keyed by the line's item id, and
# playerbot_equip, one row per equipment decision. It writes numbers only. Every
# code below is a fixed number of playerbot_explain_rules.h (one enum entry per
# line; a number never changes once shipped, a new one is appended), and this is
# where they are said in the page's language - tests/admin_panel_decisions_test.py
# reads that header and checks that each one has its four texts here, with the
# parameters the design gives it.
#
# The two text encodings are the header's too: steps and terms are
# "code=value[:a[:b[:c]]]" joined by ";" with trailing zero parameters left out
# and ";~" when the column ran out of room; lines are "type:value" joined by ","
# then "|" and the vnums of the seated stones. A line's type is the item's own
# attrtype - a POINT number on mt2009 - so it goes through apply_key like every
# other bonus line in this panel ("The panels' 71 and 72").
#
# What is explained is what a bot lists and what it wears, and nothing else:
# the panel shows what the two tables hold and adds nothing to it.
#
# A template's placeholders are {a} {b} {c} and {value}; the kind of each says
# how the number is written (_dx_param, beside the route that reads them).
EXPLAIN_DEFAULT_DAYS = 7       # the core's own default while the key is absent


# The LIFE_HOURS slider: 0 the key unset, 1..24 the hours of play a day
# (playerbot_life_rules.h; blipu, 30 September).
LIFE_HOURS_MAX = 24


EXPLAIN_MAX_DAYS = 30


# The flags that make a row "unusual" (the rules header's masks).
DECISION_LISTING_UNUSUAL = 1 | 4 | 8 | 16 | 512 | 2048


DECISION_EQUIP_UNUSUAL = 1 | 2 | 4 | 8 | 16 | 64 | 128


# How a line went up, changed or came down (EVENT_*).
DECISION_EVENTS = {
 0: {"pl": "Nieznane zdarzenie", "en": "Unknown event", "de": "Unbekanntes Ereignis", "tr": "Bilinmeyen olay"},
 1: {"pl": "Wystawione przy otwarciu sklepu", "en": "Listed when the shop opened",
     "de": "Beim Öffnen des Ladens eingestellt", "tr": "Dükkân açılırken tezgâha kondu"},
 2: {"pl": "Dołożone przy obsłudze sklepu", "en": "Added on a shop visit",
     "de": "Bei einem Ladenbesuch hinzugefügt", "tr": "Dükkân ziyaretinde eklendi"},
 3: {"pl": "Wystawione na straganie", "en": "Listed on a stall", "de": "Am Stand eingestellt", "tr": "Tezgâha kondu"},
 4: {"pl": "Przecenione", "en": "Repriced", "de": "Neu bepreist", "tr": "Yeniden fiyatlandı"},
 5: {"pl": "Pomyłka w cenie poprawiona przez właściciela", "en": "Price slip put right by the keeper",
     "de": "Preisfehler vom Besitzer korrigiert", "tr": "Fiyat hatası sahibi tarafından düzeltildi"},
 6: {"pl": "Pomyłka w cenie poprawiona przez rdzeń", "en": "Price slip put right by the core",
     "de": "Preisfehler vom Kern korrigiert", "tr": "Fiyat hatası çekirdek tarafından düzeltildi"},
 7: {"pl": "Podniesione do progu", "en": "Raised to the floor", "de": "Auf die Untergrenze angehoben", "tr": "Tabana yükseltildi"},
 8: {"pl": "Zdjęte z lady", "en": "Taken off the counter", "de": "Vom Ladentisch genommen", "tr": "Tezgâhtan indirildi"},
 9: {"pl": "Zabrane z lady do założenia", "en": "Taken back to wear", "de": "Zum Anlegen zurückgenommen", "tr": "Giymek için geri alındı"},
 10: {"pl": "Sprzedane", "en": "Sold", "de": "Verkauft", "tr": "Satıldı"},
}


# Why the stand is open: EPlayerBotShopReason, playerbot_types.h, 0..9 as it is.
DECISION_STAND = {
 0: {"pl": "powód nieznany", "en": "reason unknown", "de": "Grund unbekannt", "tr": "neden bilinmiyor"},
 1: {"pl": "osobowość Handlarz", "en": "the Merchant personality", "de": "die Händler-Persönlichkeit", "tr": "Tüccar kişiliği"},
 2: {"pl": "brak yang na mikstury", "en": "no yang for potions", "de": "kein Yang für Tränke", "tr": "iksir için yang yok"},
 3: {"pl": "pełny plecak", "en": "a full bag", "de": "ein voller Beutel", "tr": "dolu çanta"},
 4: {"pl": "dropper z pełnym plecakiem", "en": "a dropper with a full bag", "de": "ein Dropper mit vollem Beutel", "tr": "çantası dolu bir dropper"},
 5: {"pl": "nadmiar ksiąg", "en": "too many books", "de": "zu viele Bücher", "tr": "fazla kitap"},
 6: {"pl": "los droppera", "en": "a dropper's roll", "de": "der Wurf eines Droppers", "tr": "bir dropper'ın zarı"},
 7: {"pl": "los", "en": "chance", "de": "Zufall", "tr": "şans"},
 8: {"pl": "zbędny duplikat", "en": "a spare duplicate", "de": "ein überzähliges Duplikat", "tr": "fazla bir kopya"},
 9: {"pl": "nadmiar towaru", "en": "surplus goods", "de": "Warenüberschuss", "tr": "mal fazlası"},
 # MT2009 Plus: a medal dropper standing with its stock (PLAYERBOT_SHOP_REASON_MEDALS).
 10: {"pl": "medale konne droppera", "en": "a medal dropper's medals", "de": "die Medaillen eines Medaillen-Droppers",
      "tr": "bir madalya dropper'ının madalyaları"},
}


# What shape the line was cut to (SHAPE_*).
DECISION_SHAPES = {
 0: {"pl": "cały stos", "en": "the whole stack", "de": "der ganze Stapel", "tr": "yığının tamamı"},
 1: {"pl": "naturalna porcja", "en": "a natural lot", "de": "eine natürliche Menge", "tr": "doğal bir parti"},
 2: {"pl": "paczka mikstur", "en": "a potion pack", "de": "ein Trankpaket", "tr": "iksir paketi"},
 3: {"pl": "paczka pyłu", "en": "a dust pack", "de": "ein Staubpaket", "tr": "toz paketi"},
 4: {"pl": "para medali", "en": "a pair of medals", "de": "ein Medaillenpaar", "tr": "madalya çifti"},
 5: {"pl": "sztuka ponad zapas", "en": "a single over the keep", "de": "ein Einzelstück über dem Vorrat", "tr": "stok üstü tek parça"},
 6: {"pl": "pojedynczy klucz", "en": "a single key", "de": "ein einzelner Schlüssel", "tr": "tek anahtar"},
 7: {"pl": "paczka szkatułek", "en": "a chest pack", "de": "ein Truhenpaket", "tr": "sandık paketi"},
 8: {"pl": "sztuka oddzielona na straganie", "en": "a single split off for a stall",
     "de": "ein für den Stand abgeteiltes Einzelstück", "tr": "tezgâh için ayrılan tek parça"},
}


# Why a line came home (OFF_*): BotOfflineUnwantedLine's words, in its order.
DECISION_OFF = {
 0: {"pl": "bez podanego powodu", "en": "no reason given", "de": "ohne angegebenen Grund", "tr": "neden belirtilmedi"},
 1: {"pl": "Lista Przydatnych Przedmiotów zatrzymuje go dla magazyniera", "en": "the Useful Items List keeps it for the storekeeper",
     "de": "die Liste nützlicher Gegenstände behält es für den Lagerverwalter", "tr": "Faydalı Eşyalar Listesi onu depocu için tutuyor"},
 2: {"pl": "kamień duszy dla Alchemika (na pył)", "en": "a spirit stone for the Alchemist's dust",
     "de": "ein Geiststein für den Staub des Alchemisten", "tr": "Simyacının tozu için bir ruh taşı"},
 3: {"pl": "za dużo marmurów albo drugi tego samego potwora", "en": "too many marbles, or a second of one monster",
     "de": "zu viele Verwandlungskugeln oder eine zweite desselben Monsters", "tr": "fazla dönüşüm küresi ya da aynı canavarın ikincisi"},
 4: {"pl": "ponad limit linii tego samego przedmiotu", "en": "over the line cap of one item",
     "de": "über der Zeilengrenze eines Gegenstands", "tr": "aynı eşyanın satır sınırının üstünde"},
 5: {"pl": "za dużo ksiąg misji w wiosce", "en": "too many mission books in the village",
     "de": "zu viele Missionsbücher im Dorf", "tr": "köyde fazla görev kitabı"},
 6: {"pl": "za dużo słabych broni na ladach", "en": "too many junk weapons on the counters",
     "de": "zu viele Schrottwaffen auf den Ladentischen", "tr": "tezgâhlarda fazla hurda silah"},
 7: {"pl": "niepełna paczka mikstur, do zlania", "en": "not a whole potion pack, to be poured together",
     "de": "kein ganzes Trankpaket, wird zusammengeschüttet", "tr": "tam olmayan iksir paketi, birleştirilecek"},
 8: {"pl": "za dużo słabych zbroi tej rodziny", "en": "too many low armours of this family",
     "de": "zu viele schwache Rüstungen dieser Familie", "tr": "bu aileden fazla düşük zırh"},
 9: {"pl": "za dużo słabej biżuterii tej rodziny", "en": "too many low jewels of this family",
     "de": "zu viel schwacher Schmuck dieser Familie", "tr": "bu aileden fazla düşük takı"},
 10: {"pl": "farba z wody, do wyrzucenia", "en": "a dye from the water, to be thrown away",
      "de": "eine Haarfarbe aus dem Wasser, wird weggeworfen", "tr": "sudan çıkan boya, atılacak"},
 11: {"pl": "broń 30 lv wraca pod kowadło", "en": "a level-30 weapon goes back to the anvil",
      "de": "eine Waffe Lv 30 geht zurück zum Amboss", "tr": "30. seviye silah örse geri dönüyor"},
 12: {"pl": "szkatułki: bot ich nie sprzedaje albo linia za długa", "en": "chests: the bot does not sell them, or the line is too long",
      "de": "Truhen: der Bot verkauft sie nicht, oder die Zeile ist zu lang", "tr": "sandıklar: bot onları satmıyor ya da satır çok uzun"},
 13: {"pl": "za długa linia Kamieni Duchowych", "en": "too long a line of Soul Stones",
      "de": "zu lange Zeile Seelensteine", "tr": "fazla uzun bir Ruh Taşı satırı"},
 14: {"pl": "linia zwojów nie po 1, 2 lub 5", "en": "a scroll line not of 1, 2 or 5",
      "de": "eine Schriftrollenzeile nicht zu 1, 2 oder 5", "tr": "1, 2 ya da 5 olmayan parşömen satırı"},
 15: {"pl": "za długa linia medali konnych", "en": "too long a line of horse medals",
      "de": "zu lange Zeile Pferdemedaillen", "tr": "fazla uzun bir at madalyası satırı"},
 16: {"pl": "sterta nie w naturalnej porcji", "en": "a heap not in a natural lot",
      "de": "ein Haufen nicht in natürlicher Menge", "tr": "doğal partide olmayan bir yığın"},
 17: {"pl": "materiał nie w naturalnej porcji", "en": "a material not in a natural lot",
      "de": "ein Material nicht in natürlicher Menge", "tr": "doğal partide olmayan bir malzeme"},
 18: {"pl": "ekwipunek poniżej 30 lv za słaby albo ponad limit", "en": "gear under level 30 too low, or over its cap",
      "de": "Ausrüstung unter Lv 30 zu schwach oder über der Grenze", "tr": "30. seviye altı ekipman çok zayıf ya da sınırın üstünde"},
 19: {"pl": "wędka lub kilof wraca do właściciela", "en": "a rod or a pickaxe goes back to its owner",
      "de": "eine Angel oder Spitzhacke geht an ihren Besitzer zurück", "tr": "olta ya da kazma sahibine dönüyor"},
 20: {"pl": "zabrane do założenia", "en": "taken back to wear", "de": "zum Anlegen zurückgenommen", "tr": "giymek için geri alındı"},
 # MT2009 Plus's own reasons (from 200, so an appended upstream code never clashes).
 200: {"pl": "własna mikstura bojowa bota (zielona lub fioletowa)", "en": "the bot's own combat potion (green or purple)",
       "de": "der eigene Kampftrank des Bots (grün oder lila)", "tr": "botun kendi savaş iksiri (yeşil ya da mor)"},
 201: {"pl": "zwykły lub błyszczący kamień smoka to teraz materiał", "en": "an ordinary or brilliant Dragon Stone is material now",
       "de": "ein gewöhnlicher oder glänzender Drachenstein ist jetzt Material", "tr": "sıradan ya da parlak Ejderha Taşı artık malzeme"},
 202: {"pl": "szarfa, którą bot chce dla siebie", "en": "a sash the bot wants for itself",
       "de": "eine Schärpe, die der Bot selbst will", "tr": "botun kendisi için istediği bir kuşak"},
 203: {"pl": "towar rzemieślniczy się nie sprzedał — do wymiany u Dozorcy", "en": "crafting goods left unsold — for the Dozorca's exchange",
       "de": "Handwerksware blieb unverkauft — für den Tausch beim Dozorca", "tr": "zanaat malı satılmadı — Dozorca takası için"},
 204: {"pl": "Cor Draconis lub szarfa nie sprzedała się po pełnej przecenie — do handlarza",
       "en": "a Cor Draconis or a sash unsold through the whole markdown — for the merchant",
       "de": "ein Cor Draconis oder eine Schärpe blieb trotz vollem Preisnachlass unverkauft — für den Händler",
       "tr": "tam indirime rağmen satılmayan Cor Draconis ya da kuşak — satıcıya"},
}


# The small vocabularies a parameter can be one of ("enum:NAME").
DECISION_ENUMS = {
 # PLAYERBOT_ITEM_POLICY_*, the words of the item policy file.
 "POLICY": {
  1: {"pl": "zostaw", "en": "keep", "de": "behalten", "tr": "sakla"},
  2: {"pl": "stragan", "en": "stall", "de": "Stand", "tr": "tezgâh"},
  3: {"pl": "handlarz", "en": "merchant", "de": "Händler", "tr": "satıcı"},
  4: {"pl": "wyrzuć", "en": "drop", "de": "wegwerfen", "tr": "yere at"},
 },
 "STALKI_WHY": {
  1: {"pl": "innej klasy", "en": "another class's", "de": "einer anderen Klasse", "tr": "başka bir sınıfın"},
  2: {"pl": "za daleko w przód", "en": "too far ahead", "de": "zu weit voraus", "tr": "çok ileride"},
  3: {"pl": "druga sztuka", "en": "a second copy", "de": "ein zweites Exemplar", "tr": "ikinci bir kopya"},
 },
 # PlayerBotKeepsLevel30ForAnvil's why: 1, 2 and 6 keep the weapon, 3 to 5 let it go.
 "L30_WHY": {
  1: {"pl": "projekt", "en": "a project", "de": "ein Projekt", "tr": "bir proje"},
  2: {"pl": "broń własnej klasy", "en": "the class's own", "de": "die der eigenen Klasse", "tr": "kendi sınıfının"},
  3: {"pl": "innej klasy", "en": "another class's", "de": "einer anderen Klasse", "tr": "başka bir sınıfın"},
  4: {"pl": "losowanie wskazało ladę", "en": "the draw said counter", "de": "die Auslosung sagte Ladentisch", "tr": "kura tezgâhı gösterdi"},
  5: {"pl": "plecak trzyma już maksimum", "en": "the bag already keeps the maximum",
      "de": "der Beutel hält schon das Maximum", "tr": "çanta zaten azami sayıyı tutuyor"},
  6: {"pl": "zatrzymana losowaniem", "en": "kept by the draw", "de": "durch die Auslosung behalten", "tr": "kurayla tutuldu"},
 },
 "SCROLL_KEEP": {
  0: {"pl": "nic nie czeka na zwój", "en": "nothing waits for a scroll", "de": "nichts wartet auf eine Schriftrolle", "tr": "parşömen bekleyen bir şey yok"},
  1: {"pl": "noszony ekwipunek do ulepszenia", "en": "worn gear to refine", "de": "getragene Ausrüstung zum Verbessern", "tr": "yükseltilecek giyili ekipman"},
  2: {"pl": "handlarz zasobami", "en": "a resource trader", "de": "ein Rohstoffhändler", "tr": "bir kaynak tüccarı"},
  3: {"pl": "reguła zwojów", "en": "the scroll rule", "de": "die Schriftrollen-Regel", "tr": "parşömen kuralı"},
 },
 "MEDAL_WHY": {
  1: {"pl": "Handlarz", "en": "a Merchant", "de": "ein Händler", "tr": "Tüccar"},
  2: {"pl": "dropper medali", "en": "a medal dropper", "de": "ein Medaillen-Dropper", "tr": "madalya dropper'ı"},
  3: {"pl": "ponad zapas", "en": "over the keep", "de": "über dem Vorrat", "tr": "stok üstü"},
  4: {"pl": "koń na maksimum dla poziomu", "en": "the horse is maxed for the level",
      "de": "das Pferd ist für das Level ausgereizt", "tr": "at, seviyesi için en üstte"},
 },
 "MAT_WHY": {
  1: {"pl": "próba rynku", "en": "a market probe", "de": "ein Markttest", "tr": "piyasa denemesi"},
  2: {"pl": "brak popytu", "en": "no demand", "de": "keine Nachfrage", "tr": "talep yok"},
  3: {"pl": "nadmiar", "en": "overstock", "de": "Überbestand", "tr": "fazla stok"},
 },
 "VALUABLE": {
  0: {"pl": "", "en": "", "de": "", "tr": ""},
  1: {"pl": " (z cennym bonusem)", "en": " (with a valuable bonus)", "de": " (mit wertvollem Bonus)", "tr": " (değerli bonuslu)"},
 },
 "BOOK_USEFUL": {
  0: {"pl": " (bot jej nie przeczyta)", "en": " (of no use to the bot)", "de": " (für den Bot nutzlos)", "tr": " (bota faydası yok)"},
  1: {"pl": " (przydatna, ale ponad zapas)", "en": " (useful, but over the keep)",
      "de": " (nützlich, aber über dem Vorrat)", "tr": " (faydalı, ama stok üstü)"},
 },
 "AT_TOP": {
  0: {"pl": "", "en": "", "de": "", "tr": ""},
  1: {"pl": " (na maksimum)", "en": " (at the top)", "de": " (am Maximum)", "tr": " (en üstte)"},
 },
 "PREVIEW": {
  0: {"pl": "", "en": "", "de": "", "tr": ""},
  1: {"pl": " (podgląd bez właściciela, więc bez wahania)", "en": " (a preview without its keeper, so no swing)",
      "de": " (Vorschau ohne Besitzer, also keine Schwankung)", "tr": " (sahipsiz önizleme, bu yüzden dalgalanma yok)"},
 },
 "MARBLE_SRC": {
  0: {"pl": "losowanie z przedziału", "en": "drawn from the band", "de": "aus dem Bereich gezogen", "tr": "aralıktan çekildi"},
  1: {"pl": "wyjątek wymieniony z nazwy", "en": "a named exception", "de": "eine namentliche Ausnahme", "tr": "adıyla belirtilmiş istisna"},
 },
 "FIXED": {
  1: {"pl": "biała perła", "en": "white pearl", "de": "weiße Perle", "tr": "beyaz inci"},
  2: {"pl": "niebieska perła", "en": "blue pearl", "de": "blaue Perle", "tr": "mavi inci"},
  3: {"pl": "czerwona perła", "en": "red pearl", "de": "rote Perle", "tr": "kırmızı inci"},
  4: {"pl": "małża", "en": "shellfish", "de": "Muschel", "tr": "istiridye"},
  5: {"pl": "medal konny", "en": "horse medal", "de": "Pferdemedaille", "tr": "at madalyası"},
  6: {"pl": "Suszona Głowa", "en": "Dried Head", "de": "getrockneter Kopf", "tr": "Kurutulmuş Kafa"},
  7: {"pl": "fryzura", "en": "hairstyle", "de": "Frisur", "tr": "saç modeli"},
  8: {"pl": "Magiczny Pył", "en": "Magic Dust", "de": "Magischer Staub", "tr": "Sihirli Toz"},
 },
 "GRADE_TABLE": {
  0: {"pl": "", "en": "", "de": "", "tr": ""},
  1: {"pl": " (z tabeli stopni)", "en": " (from the grade table)", "de": " (aus der Stufentabelle)", "tr": " (derece tablosundan)"},
 },
 "CLOCK_UNIT": {
  0: {"pl": "wystawień", "en": "stands", "de": "Standzeiten", "tr": "stant dönemi"},
  1: {"pl": "min", "en": "min", "de": "Min.", "tr": "dk"},
 },
 "CLOCK_RESTART": {
  0: {"pl": "", "en": "", "de": "", "tr": ""},
  1: {"pl": " (zegar ruszył po restarcie)", "en": " (the clock began after a restart)",
      "de": " (die Uhr begann nach einem Neustart)", "tr": " (saat yeniden başlatmadan sonra başladı)"},
 },
 "GENERATION": {
  0: {"pl": "bieżące pokolenie", "en": "the current generation", "de": "die aktuelle Generation", "tr": "güncel kuşak"},
  1: {"pl": "wycenione w innym pokoleniu (nadrabia tempo)", "en": "priced under another generation (catching up)",
      "de": "unter einer anderen Generation bepreist (holt auf)", "tr": "başka bir kuşakta fiyatlanmış (yetişiyor)"},
 },
 "SLIP_BY": {
  0: {"pl": "poprawił właściciel", "en": "the keeper put it right", "de": "der Besitzer hat ihn korrigiert", "tr": "sahibi düzeltti"},
  1: {"pl": "poprawił rdzeń, bo właściciel był daleko", "en": "the core put it right, the keeper being away",
      "de": "der Kern hat ihn korrigiert, weil der Besitzer weg war", "tr": "sahibi uzakta olduğu için çekirdek düzeltti"},
  2: {"pl": "poprawił rdzeń, bo pomyłka była wstrzymana", "en": "the core put it right, the slip being held",
      "de": "der Kern hat ihn korrigiert, weil der Fehler zurückgehalten war", "tr": "hata bekletildiği için çekirdek düzeltti"},
  3: {"pl": "poprawił rdzeń, po czasie", "en": "the core put it right, late",
      "de": "der Kern hat ihn korrigiert, verspätet", "tr": "çekirdek geç de olsa düzeltti"},
 },
 "LADDER_CAT": {
  1: {"pl": "broń", "en": "weapon", "de": "Waffe", "tr": "silah"},
  2: {"pl": "zbroja", "en": "armour", "de": "Rüstung", "tr": "zırh"},
  3: {"pl": "tarcza", "en": "shield", "de": "Schild", "tr": "kalkan"},
  4: {"pl": "hełm", "en": "helmet", "de": "Helm", "tr": "kask"},
  5: {"pl": "buty", "en": "boots", "de": "Schuhe", "tr": "ayakkabı"},
  6: {"pl": "bransoleta", "en": "bracelet", "de": "Armband", "tr": "bilezik"},
  7: {"pl": "naszyjnik", "en": "necklace", "de": "Halskette", "tr": "kolye"},
  8: {"pl": "kolczyki", "en": "earrings", "de": "Ohrringe", "tr": "küpe"},
  9: {"pl": "broń zapasowa", "en": "backup weapon", "de": "Ersatzwaffe", "tr": "yedek silah"},
  10: {"pl": "zbroja zapasowa", "en": "backup armour", "de": "Ersatzrüstung", "tr": "yedek zırh"},
 },
 "SAFEBOX_WHY": {
  1: {"pl": "Stalka", "en": "a Stalki", "de": "eine Stalki", "tr": "bir Stalki"},
  2: {"pl": "zwolnione z Listy Przydatnych Przedmiotów", "en": "released from the Useful Items List",
      "de": "aus der Liste nützlicher Gegenstände freigegeben", "tr": "Faydalı Eşyalar Listesinden çıkarıldı"},
  3: {"pl": "hazard", "en": "a gamble", "de": "ein Glücksspiel", "tr": "bir kumar"},
  4: {"pl": "klucz", "en": "a key", "de": "ein Schlüssel", "tr": "bir anahtar"},
  9: {"pl": "inne", "en": "other", "de": "anderes", "tr": "diğer"},
 },
}


# Why an item is goods at all (GOODS_*): the branches of ScorePlayerBotShopStock,
# code: (the kinds of a, b, c; the four texts).
DECISION_GOODS = {
 0: ({}, {"pl": "Wystawione, zanim zaczęto zapisywać wyjaśnienia", "en": "Listed before explanations were recorded",
          "de": "Eingestellt, bevor Erklärungen aufgezeichnet wurden", "tr": "Açıklamalar kaydedilmeye başlanmadan önce tezgâha kondu"}),
 1: ({"a": "enum:POLICY"},
     {"pl": "Operator przypisał go ladzie (reguła „{a}”)", "en": "The operator sent it to the counter (rule “{a}”)",
      "de": "Der Betreiber hat ihn dem Ladentisch zugewiesen (Regel „{a}“)", "tr": "Operatör onu tezgâha gönderdi (kural “{a}”)"}),
 2: ({"a": "plus"},
     {"pl": "Towar hazardzisty z sesji przy kowadle ({a})", "en": "A gambler's goods from its session at the anvil ({a})",
      "de": "Ware eines Glücksspielers aus seiner Sitzung am Amboss ({a})", "tr": "Kumarbazın örs başındaki oturumundan mal ({a})"}),
 3: ({"a": "int", "b": "int"},
     {"pl": "Kamienie Duchowe ponad zapas: w plecaku {a}, zapas {b}", "en": "Soul Stones over the keep: {a} in the bag, keeps {b}",
      "de": "Seelensteine über dem Vorrat: {a} im Beutel, behält {b}", "tr": "Stok üstü Ruh Taşları: çantada {a}, saklanan {b}"}),
 4: ({"a": "mob"},
     {"pl": "Marmur polimorfii ({a}) — towar, nie złom", "en": "A polymorph marble ({a}) — goods, not scrap",
      "de": "Eine Verwandlungskugel ({a}) — Ware, kein Schrott", "tr": "Bir dönüşüm küresi ({a}) — mal, hurda değil"}),
 5: ({}, {"pl": "Wykrywacz metinów — bot teraz nie poluje na kamienie", "en": "A Metin detector — the bot is not hunting stones now",
          "de": "Ein Metin-Detektor — der Bot jagt gerade keine Steine", "tr": "Bir Metin dedektörü — bot şu an taş avlamıyor"}),
 6: ({}, {"pl": "Suszona Głowa — dla graczy idących do Katakumb", "en": "A Dried Head — for a player's run of the Catacomb",
          "de": "Ein getrockneter Kopf — für den Katakomben-Lauf eines Spielers", "tr": "Bir Kurutulmuş Kafa — bir oyuncunun Yeraltı Mezarı koşusu için"}),
 7: ({"a": "int", "b": "int"},
     {"pl": "Kamienie bonusów ponad zapas: w plecaku {a}, zapas {b}", "en": "Bonus stones over the keep: {a} in the bag, keeps {b}",
      "de": "Bonussteine über dem Vorrat: {a} im Beutel, behält {b}", "tr": "Stok üstü bonus taşları: çantada {a}, saklanan {b}"}),
 8: ({"a": "skill"},
     {"pl": "Zwój Zapomnienia ({a}) z listy „do handlarki” — wystawia go tylko Handlarz",
      "en": "A Forgetting Scroll ({a}) of the merchant-only list — only a Merchant lists it",
      "de": "Eine Schriftrolle des Vergessens ({a}) von der Nur-Händler-Liste — nur ein Händler stellt sie ein",
      "tr": "Yalnız-satıcı listesinden bir Unutma Parşömeni ({a}) — onu yalnızca Tüccar satar"}),
 9: ({"a": "plus", "b": "level", "c": "enum:STALKI_WHY"},
     {"pl": "Stalka, której bot nie zatrzymuje: {a}, poziom {b} — {c}", "en": "A Stalki the bot does not keep: {a}, level {b} — {c}",
      "de": "Eine Stalki, die der Bot nicht behält: {a}, Level {b} — {c}", "tr": "Botun tutmadığı bir Stalki: {a}, seviye {b} — {c}"}),
 10: ({"a": "plus", "b": "pct", "c": "plus"},
      {"pl": "Broń 30 lv innej klasy, gotowa na sprzedaż: {a}, średnie obrażenia {b}, cel {c}",
       "en": "A level-30 weapon of another class, ready for sale: {a}, average damage {b}, target {c}",
       "de": "Eine Waffe Lv 30 einer anderen Klasse, verkaufsbereit: {a}, Durchschnittsschaden {b}, Ziel {c}",
       "tr": "Başka bir sınıfın 30. seviye silahı, satışa hazır: {a}, ortalama zarar {b}, hedef {c}"}),
 11: ({"a": "plus", "b": "pct", "c": "enum:L30_WHY"},
      {"pl": "Broń 30 lv niezatrzymana do kowadła: {a}, średnie obrażenia {b} — {c}",
       "en": "A level-30 weapon not kept for the anvil: {a}, average damage {b} — {c}",
       "de": "Eine Waffe Lv 30, nicht für den Amboss behalten: {a}, Durchschnittsschaden {b} — {c}",
       "tr": "Örs için tutulmayan 30. seviye silah: {a}, ortalama zarar {b} — {c}"}),
 12: ({"a": "plus", "b": "level"},
      {"pl": "Zbroja lub biżuteria {a} poniżej 30 lv (poziom {b}) — towar każdego bota",
       "en": "Armour or a jewel at {a} under level 30 (level {b}) — every bot's goods",
       "de": "Rüstung oder Schmuck {a} unter Lv 30 (Level {b}) — Ware jedes Bots",
       "tr": "30. seviye altında {a} zırh ya da takı (seviye {b}) — her botun malı"}),
 13: ({"a": "plus", "b": "plus", "c": "level"},
      {"pl": "Ekwipunek poniżej 30 lv na minimalnym ulepszeniu: {a} (próg {b}), poziom {c}",
       "en": "Gear under level 30 at its minimum refine: {a} (threshold {b}), level {c}",
       "de": "Ausrüstung unter Lv 30 auf Mindestverbesserung: {a} (Schwelle {b}), Level {c}",
       "tr": "Asgari yükseltmede 30. seviye altı ekipman: {a} (eşik {b}), seviye {c}"}),
 14: ({"a": "plus", "b": "enum:VALUABLE"},
      {"pl": "Nadwyżka z Listy Przydatnych Przedmiotów: {a}{b}", "en": "Surplus of the Useful Items List: {a}{b}",
       "de": "Überschuss der Liste nützlicher Gegenstände: {a}{b}", "tr": "Faydalı Eşyalar Listesi fazlası: {a}{b}"}),
 15: ({"a": "apply", "b": "applyval:a"},
      {"pl": "Cenny bonus: {a} {b}", "en": "A valuable bonus: {a} {b}", "de": "Ein wertvoller Bonus: {a} {b}", "tr": "Değerli bir bonus: {a} {b}"}),
 16: ({"a": "plus"},
      {"pl": "Ekwipunek z listy „do handlarza” ({a}) — wystawia go tylko Handlarz",
       "en": "Gear of the merchant-only list ({a}) — only a Merchant lists it",
       "de": "Ausrüstung von der Nur-Händler-Liste ({a}) — nur ein Händler stellt sie ein",
       "tr": "Yalnız-satıcı listesinden ekipman ({a}) — onu yalnızca Tüccar satar"}),
 17: ({"a": "plus", "b": "item", "c": "plus"},
      {"pl": "Cenny zapas {a}; bot nosi {b} ({c})", "en": "A precious spare at {a}; the bot wears {b} ({c})",
       "de": "Ein wertvolles Ersatzstück {a}; der Bot trägt {b} ({c})", "tr": "Değerli bir yedek {a}; bot {b} ({c}) giyiyor"}),
 18: ({"a": "int", "b": "int", "c": "enum:SCROLL_KEEP"},
      {"pl": "Zwoje ponad zapas: {a} w plecaku, zapas {b} ({c})", "en": "Scrolls over the keep: {a} in the bag, keeps {b} ({c})",
       "de": "Schriftrollen über dem Vorrat: {a} im Beutel, behält {b} ({c})", "tr": "Stok üstü parşömenler: çantada {a}, saklanan {b} ({c})"}),
 19: ({"a": "int", "b": "int", "c": "enum:MEDAL_WHY"},
      {"pl": "Medale konne: {a} w plecaku, zapas {b} ({c})", "en": "Horse medals: {a} in the bag, keeps {b} ({c})",
       "de": "Pferdemedaillen: {a} im Beutel, behält {b} ({c})", "tr": "At madalyaları: çantada {a}, saklanan {b} ({c})"}),
 20: ({"a": "int", "b": "int", "c": "int"},
      {"pl": "Materiał, którego wiosce brakuje dla graczy: {a} w plecaku, rezerwa kowadła {b}, podaż w wiosce {c}",
       "en": "A material the village is short of for the players: {a} in the bag, anvil reserve {b}, village supply {c}",
       "de": "Ein Material, das dem Dorf für die Spieler fehlt: {a} im Beutel, Amboss-Reserve {b}, Angebot im Dorf {c}",
       "tr": "Köyün oyuncular için eksik olduğu malzeme: çantada {a}, örs rezervi {b}, köydeki arz {c}"}),
 21: ({"a": "int", "b": "int", "c": "int"},
      {"pl": "Materiał, którego brakuje botom: {a} w plecaku, rezerwa {b}, brakuje {c} botom",
       "en": "A material the bots are short of: {a} in the bag, reserve {b}, {c} bots short",
       "de": "Ein Material, das den Bots fehlt: {a} im Beutel, Reserve {b}, {c} Bots fehlt es",
       "tr": "Botların eksik olduğu malzeme: çantada {a}, rezerv {b}, {c} botta eksik"}),
 22: ({"a": "int", "b": "int", "c": "enum:MAT_WHY"},
      {"pl": "Materiał ponad rezerwę: {a} w plecaku, rezerwa {b} ({c})", "en": "A material over the reserve: {a} in the bag, reserve {b} ({c})",
       "de": "Material über der Reserve: {a} im Beutel, Reserve {b} ({c})", "tr": "Rezerv üstü malzeme: çantada {a}, rezerv {b} ({c})"}),
 23: ({"a": "plus"},
      {"pl": "Towar zebrany z ziemi (siano, marchew, księgi misji), {a}", "en": "Pickup goods (hay, carrots, mission books), {a}",
       "de": "Aufgesammelte Ware (Heu, Karotten, Missionsbücher), {a}", "tr": "Yerden toplanan mal (saman, havuç, görev kitapları), {a}"}),
 24: ({}, {"pl": "Mikstura z tablicy Baek-Go ponad zapas", "en": "A potion from Baek-Go's board, over the keep",
           "de": "Ein Trank von Baek-Gos Tafel, über dem Vorrat", "tr": "Baek-Go'nun tahtasından stok üstü bir iksir"}),
 25: ({}, {"pl": "Receptura przeczytana do sufitu", "en": "A recipe already read to its ceiling",
           "de": "Ein Rezept, schon bis zur Obergrenze gelesen", "tr": "Tavanına kadar okunmuş bir tarif"}),
 26: ({}, {"pl": "Farba do włosów z ItemShopu", "en": "An item-shop hair dye", "de": "Ein Haarfärbemittel aus dem ItemShop", "tr": "ItemShop saç boyası"}),
 27: ({}, {"pl": "Farba z wody zatrzymana na sprzedaż", "en": "A dye from the water kept for sale",
           "de": "Eine Haarfarbe aus dem Wasser, für den Verkauf behalten", "tr": "Satış için tutulan sudan çıkma boya"}),
 28: ({}, {"pl": "Fryzura z ItemShopu, której bot nie założy", "en": "An item-shop hairstyle the bot cannot wear",
           "de": "Eine ItemShop-Frisur, die der Bot nicht tragen kann", "tr": "Botun takamayacağı bir ItemShop saç modeli"}),
 29: ({"a": "skill"},
      {"pl": "Zwój Zapomnienia — żadna umiejętność bota na niego nie czeka ({a})",
       "en": "A Forgetting Scroll — none of the bot's skills waits for it ({a})",
       "de": "Eine Schriftrolle des Vergessens — keine Fertigkeit des Bots wartet darauf ({a})",
       "tr": "Bir Unutma Parşömeni — botun hiçbir becerisi onu beklemiyor ({a})"}),
 30: ({"a": "int", "b": "int"},
      {"pl": "Magiczny Pył ponad zapas: {a} w plecaku, zapas {b}", "en": "Magic Dust over the keep: {a} in the bag, keeps {b}",
       "de": "Magischer Staub über dem Vorrat: {a} im Beutel, behält {b}", "tr": "Stok üstü Sihirli Toz: çantada {a}, saklanan {b}"}),
 31: ({"a": "plus", "b": "int"},
      {"pl": "Kamień duszy {a} (rodzaj {b}) — jeden z 15 na 100 zostawionych dla rynku",
       "en": "A spirit stone {a} (kind {b}) — one of the 15 in 100 kept for the market",
       "de": "Ein Geiststein {a} (Art {b}) — einer der 15 von 100, die für den Markt bleiben",
       "tr": "Bir ruh taşı {a} (tür {b}) — pazar için ayrılan 100'de 15'ten biri"}),
 32: ({"a": "plus", "b": "int"},
      {"pl": "Kamień duszy {a} (rodzaj {b}), którego bot nie osadzi", "en": "A spirit stone {a} (kind {b}) the bot cannot seat",
       "de": "Ein Geiststein {a} (Art {b}), den der Bot nicht einsetzen kann", "tr": "Botun takamayacağı bir ruh taşı {a} (tür {b})"}),
 33: ({"a": "int", "b": "int", "c": "enum:BOOK_USEFUL"},
      {"pl": "Księga ogólna: {a} przed nią w plecaku, zapas {b}{c}", "en": "A general book: {a} ahead of it in the bag, keeps {b}{c}",
       "de": "Ein allgemeines Buch: {a} davor im Beutel, behält {b}{c}", "tr": "Genel bir kitap: çantada önünde {a}, saklanan {b}{c}"}),
 34: ({}, {"pl": "Wyceniony z nazwy w arkuszu Iwakury", "en": "Priced by name on Iwakura's sheet",
           "de": "Auf Iwakuras Liste namentlich bepreist", "tr": "Iwakura'nın listesinde adıyla fiyatlanmış"}),
 35: ({"a": "skill", "b": "int"},
      {"pl": "Księga ({a}) na ladzie droppera metinów, zapas {b}", "en": "A book ({a}) on a Metin dropper's counter, keeps {b}",
       "de": "Ein Buch ({a}) auf dem Ladentisch eines Metin-Droppers, behält {b}", "tr": "Metin dropper'ının tezgâhında bir kitap ({a}), saklanan {b}"}),
 36: ({"a": "skill", "b": "int"},
      {"pl": "Własna księga ({a}) ponad zapas {b}", "en": "Its own book ({a}) over the keep of {b}",
       "de": "Ein eigenes Buch ({a}) über dem Vorrat von {b}", "tr": "Kendi kitabı ({a}), {b} stoğunun üstünde"}),
 37: ({"a": "skill"},
      {"pl": "Księga innej ścieżki ({a})", "en": "Another build's book ({a})", "de": "Das Buch eines anderen Wegs ({a})", "tr": "Başka bir yolun kitabı ({a})"}),
 38: ({"a": "plus"},
      {"pl": "Zbroja lub biżuteria {a} — towar każdego bota", "en": "Armour or a jewel at {a} — every bot's goods",
       "de": "Rüstung oder Schmuck {a} — Ware jedes Bots", "tr": "{a} zırh ya da takı — her botun malı"}),
 39: ({"a": "plus"},
      {"pl": "Słaby ekwipunek {a} na ladzie zbieracza złomu", "en": "Low gear {a} on a scrap keeper's counter",
       "de": "Schwache Ausrüstung {a} auf dem Ladentisch eines Schrotthändlers", "tr": "Hurdacının tezgâhında zayıf ekipman {a}"}),
 40: ({"a": "int", "b": "int"},
      {"pl": "Nieotwarta szkatułka ponad zapas: {a} w plecaku, zapas {b}", "en": "An unopened chest over the hold: {a} in the bag, holds {b}",
       "de": "Eine ungeöffnete Truhe über dem Vorrat: {a} im Beutel, behält {b}", "tr": "Stok üstü açılmamış sandık: çantada {a}, saklanan {b}"}),
 41: ({"a": "int", "b": "int"},
      {"pl": "Klucz bez skrzyni ponad zapas: {a} w plecaku, zapas {b}", "en": "A key with no chest, over the keep: {a} in the bag, keeps {b}",
       "de": "Ein Schlüssel ohne Truhe, über dem Vorrat: {a} im Beutel, behält {b}", "tr": "Sandıksız anahtar, stok üstü: çantada {a}, saklanan {b}"}),
 # MT2009 Plus's own goods (from 200).
 200: ({}, {"pl": "Cor Draconis albo szarfa — towar dla graczy", "en": "A Cor Draconis or a sash — goods for the players",
            "de": "Ein Cor Draconis oder eine Schärpe — Ware für die Spieler", "tr": "Bir Cor Draconis ya da kuşak — oyuncular için mal"}),
 201: ({}, {"pl": "Materiał budowlany gildii, którego gildia bota nie trzyma", "en": "A guild building material the bot's guild does not keep",
            "de": "Ein Gildenbaumaterial, das die Gilde des Bots nicht behält", "tr": "Botun loncasının tutmadığı bir lonca yapı malzemesi"}),
 202: ({}, {"pl": "Kamień smoka, którego bot nie użyje", "en": "A Dragon Stone the bot has no use for",
            "de": "Ein Drachenstein, den der Bot nicht braucht", "tr": "Botun kullanmadığı bir Ejderha Taşı"}),
 # MT2009_PLUS_BOTLIFE_V1: a refine stone of the jewellery (Diament, Ebonit...).
 203: ({"a": "int", "b": "int"},
      {"pl": "Przetop ponad to, co przyjmą gniazda biżuterii bota: {a} w plecaku, zapas {b}",
       "en": "A refine stone over what the bot's jewellery sockets take: {a} in the bag, keeps {b}",
       "de": "Ein Veredelungsstein über dem, was die Schmuckfassungen des Bots aufnehmen: {a} im Beutel, behält {b}",
       "tr": "Botun takı yuvalarının alacağından fazla arıtma taşı: çantada {a}, saklanan {b}"}),
}


# One step of the price (STEP_*): code: (the kind of `value` when it is not the
# price after the step - None when it is, and the value column shows it -, the
# kinds of a, b, c, the step's name, the step's detail).
DECISION_STEPS = {
 0: (None, {}, {"pl": "Nieznany krok", "en": "Unknown step", "de": "Unbekannter Schritt", "tr": "Bilinmeyen adım"},
     {"pl": "", "en": "", "de": "", "tr": ""}),
 1: ("pct", {"a": "pct", "b": "x10000", "c": "int"},
     {"pl": "Kontekst", "en": "Context", "de": "Kontext", "tr": "Bağlam"},
     {"pl": "krzywa {value}, stawka yang {a}, inflacja {b} (kroków: {c})", "en": "curve {value}, yang rate {a}, inflation {b} ({c} steps)",
      "de": "Kurve {value}, Yang-Rate {a}, Inflation {b} ({c} Schritte)", "tr": "eğri {value}, yang oranı {a}, enflasyon {b} ({c} adım)"}),
 2: ("x100", {"a": "apply", "b": "applyval:a", "c": "enum:AT_TOP"},
     {"pl": "Linia bonusu", "en": "Bonus line", "de": "Bonuszeile", "tr": "Bonus satırı"},
     {"pl": "{a} {b}{c}: {value}", "en": "{a} {b}{c}: {value}", "de": "{a} {b}{c}: {value}", "tr": "{a} {b}{c}: {value}"}),
 3: ("x100", {"a": "int"},
     {"pl": "Linie na maksimum", "en": "Lines at the top", "de": "Zeilen am Maximum", "tr": "En üstteki satırlar"},
     {"pl": "linii na maksimum: {a}, mnożnik {value}", "en": "{a} lines at the top, multiplier {value}",
      "de": "{a} Zeilen am Maximum, Faktor {value}", "tr": "en üstte {a} satır, çarpan {value}"}),
 4: ("pct", {"a": "x100"},
     {"pl": "Dopłata za bonusy", "en": "Bonus premium", "de": "Bonus-Aufschlag", "tr": "Bonus primi"},
     {"pl": "dopłata {value} (iloczyn z limitem {a})", "en": "premium {value} (capped product {a})",
      "de": "Aufschlag {value} (begrenztes Produkt {a})", "tr": "prim {value} (sınırlı çarpım {a})"}),
 5: ("yang", {"a": "plus"},
     {"pl": "Wkład u kowala", "en": "The blacksmith's bill", "de": "Rechnung des Schmieds", "tr": "Demirci faturası"},
     {"pl": "kowal dostał {value} za drogę do {a}", "en": "the blacksmith was paid {value} on the way to {a}",
      "de": "der Schmied bekam {value} auf dem Weg zu {a}", "tr": "demirciye {a} yolunda {value} ödendi"}),
 6: (None, {"a": "yang", "b": "item", "c": "plus"},
     {"pl": "Arkusz Iwakury (ekwipunek)", "en": "Iwakura's sheet (gear)", "de": "Iwakuras Liste (Ausrüstung)", "tr": "Iwakura'nın listesi (ekipman)"},
     {"pl": "{b}, {c}: z arkusza {a}", "en": "{b}, {c}: sheet price {a}", "de": "{b}, {c}: Listenpreis {a}", "tr": "{b}, {c}: liste fiyatı {a}"}),
 7: ("pct", {"a": "int"},
     {"pl": "Osadzone kamienie", "en": "Seated stones", "de": "Eingesetzte Steine", "tr": "Takılı taşlar"},
     {"pl": "kamieni: {a}, dopłata {value}", "en": "{a} stones, premium {value}", "de": "{a} Steine, Aufschlag {value}", "tr": "{a} taş, prim {value}"}),
 8: (None, {"a": "plus"},
     {"pl": "Cena stała za plus", "en": "Flat price for the plus", "de": "Festpreis für das Plus", "tr": "Artı için sabit fiyat"},
     {"pl": "cena stała dla {a}", "en": "flat price for {a}", "de": "Festpreis für {a}", "tr": "{a} için sabit fiyat"}),
 9: (None, {"a": "yang", "b": "x100"},
     {"pl": "Złom", "en": "Scrap", "de": "Schrott", "tr": "Hurda"},
     {"pl": "cena handlarza {a}, mnożnik {b}", "en": "merchant price {a}, multiplier {b}",
      "de": "Händlerpreis {a}, Faktor {b}", "tr": "satıcı fiyatı {a}, çarpan {b}"}),
 10: (None, {"a": "yang"},
      {"pl": "Próg wkładu", "en": "Investment floor", "de": "Investitionsuntergrenze", "tr": "Yatırım tabanı"},
      {"pl": "podniesiona do rachunku kowala {a}", "en": "raised to the blacksmith's bill {a}",
       "de": "auf die Rechnung des Schmieds {a} angehoben", "tr": "demirci faturasına ({a}) yükseltildi"}),
 11: (None, {"a": "pct"},
      {"pl": "Premia za bonusy", "en": "Bonus premium applied", "de": "Bonusprämie", "tr": "Uygulanan bonus primi"},
      {"pl": "premia {a}", "en": "premium {a}", "de": "Prämie {a}", "tr": "prim {a}"}),
 12: (None, {"a": "pctsigned", "b": "enum:PREVIEW"},
      {"pl": "Konkurencja", "en": "Competition", "de": "Konkurrenz", "tr": "Rekabet"},
      {"pl": "odchylenie {a}{b}", "en": "swing {a}{b}", "de": "Schwankung {a}{b}", "tr": "sapma {a}{b}"}),
 13: (None, {"a": "mob", "b": "enum:MARBLE_SRC"},
      {"pl": "Arkusz Iwakury (marmur)", "en": "Iwakura's sheet (marble)", "de": "Iwakuras Liste (Kugel)", "tr": "Iwakura'nın listesi (küre)"},
      {"pl": "{a}: {b}", "en": "{a}: {b}", "de": "{a}: {b}", "tr": "{a}: {b}"}),
 14: (None, {"a": "int"},
      {"pl": "Ilość", "en": "Count", "de": "Anzahl", "tr": "Adet"},
      {"pl": "× {a} szt.", "en": "× {a}", "de": "× {a} Stück", "tr": "× {a} adet"}),
 15: (None, {"a": "yang", "b": "pct"},
      {"pl": "Punkt wyjścia: handlarz", "en": "Starting point: the merchant", "de": "Ausgangspunkt: der Händler", "tr": "Başlangıç: satıcı"},
      {"pl": "cena handlarza {a}, narzut {b}", "en": "merchant price {a}, markup {b}", "de": "Händlerpreis {a}, Aufschlag {b}", "tr": "satıcı fiyatı {a}, kâr payı {b}"}),
 16: (None, {},
      {"pl": "Punkt wyjścia: broń 30 lv", "en": "Starting point: a level-30 weapon", "de": "Ausgangspunkt: eine Waffe Lv 30", "tr": "Başlangıç: 30. seviye silah"},
      {"pl": "", "en": "", "de": "", "tr": ""}),
 17: (None, {"a": "skill", "b": "yang"},
      {"pl": "Arkusz Iwakury (księga)", "en": "Iwakura's sheet (book)", "de": "Iwakuras Liste (Buch)", "tr": "Iwakura'nın listesi (kitap)"},
      {"pl": "{a}: z arkusza {b}", "en": "{a}: sheet price {b}", "de": "{a}: Listenpreis {b}", "tr": "{a}: liste fiyatı {b}"}),
 18: (None, {"a": "item"},
      {"pl": "Arkusz Iwakury (księga ogólna)", "en": "Iwakura's sheet (general book)",
       "de": "Iwakuras Liste (allgemeines Buch)", "tr": "Iwakura'nın listesi (genel kitap)"},
      {"pl": "{a}", "en": "{a}", "de": "{a}", "tr": "{a}"}),
 19: (None, {"a": "yang"},
      {"pl": "Arkusz Iwakury (materiał)", "en": "Iwakura's sheet (material)", "de": "Iwakuras Liste (Material)", "tr": "Iwakura'nın listesi (malzeme)"},
      {"pl": "z arkusza {a}", "en": "sheet price {a}", "de": "Listenpreis {a}", "tr": "liste fiyatı {a}"}),
 20: (None, {"a": "yang", "b": "yang"},
      {"pl": "Wartość szkatułki", "en": "Chest worth", "de": "Truhenwert", "tr": "Sandık değeri"},
      {"pl": "z arkusza {a}, wartość zawartości {b}", "en": "sheet {a}, worth by its contents {b}",
       "de": "Liste {a}, Wert nach Inhalt {b}", "tr": "liste {a}, içeriğe göre değer {b}"}),
 21: (None, {"a": "skill"},
      {"pl": "Arkusz Iwakury (Zwój Zapomnienia)", "en": "Iwakura's sheet (Forgetting Scroll)",
       "de": "Iwakuras Liste (Schriftrolle des Vergessens)", "tr": "Iwakura'nın listesi (Unutma Parşömeni)"},
      {"pl": "{a}", "en": "{a}", "de": "{a}", "tr": "{a}"}),
 22: (None, {"a": "enum:FIXED"},
      {"pl": "Stała cena wyjściowa", "en": "Fixed starting price", "de": "Fester Ausgangspreis", "tr": "Sabit başlangıç fiyatı"},
      {"pl": "{a}", "en": "{a}", "de": "{a}", "tr": "{a}"}),
 23: (None, {"a": "plus", "b": "int", "c": "enum:GRADE_TABLE"},
      {"pl": "Arkusz Iwakury (kamień duszy)", "en": "Iwakura's sheet (spirit stone)", "de": "Iwakuras Liste (Geiststein)", "tr": "Iwakura'nın listesi (ruh taşı)"},
      {"pl": "{a}, rodzaj {b}{c}", "en": "{a}, kind {b}{c}", "de": "{a}, Art {b}{c}", "tr": "{a}, tür {b}{c}"}),
 24: (None, {},
      {"pl": "Brak ceny handlarza", "en": "No merchant price", "de": "Kein Händlerpreis", "tr": "Satıcı fiyatı yok"},
      {"pl": "", "en": "", "de": "", "tr": ""}),
 25: (None, {"a": "yang", "b": "permille", "c": "pct"},
      {"pl": "Portfele", "en": "Wallets", "de": "Geldbörsen", "tr": "Cüzdanlar"},
      {"pl": "mediana portfela {a}, udział {b}, wartość {c}", "en": "median wallet {a}, share {b}, worth {c}",
       "de": "mittlere Geldbörse {a}, Anteil {b}, Wert {c}", "tr": "ortanca cüzdan {a}, pay {b}, değer {c}"}),
 26: ("yang", {"a": "pct"},
      {"pl": "Limit portfela na stos", "en": "Wallet cap for the stack", "de": "Geldbörsen-Grenze für den Stapel", "tr": "Yığın için cüzdan sınırı"},
      {"pl": "limit {value} za sztukę, stos {a}", "en": "cap {value} a unit, stack {a}",
       "de": "Grenze {value} pro Stück, Stapel {a}", "tr": "adet başına {value} sınır, yığın {a}"}),
 27: (None, {"a": "yang", "b": "int"},
      {"pl": "Pamięć sprzedaży", "en": "Sale memory", "de": "Verkaufsgedächtnis", "tr": "Satış hafızası"},
      {"pl": "mediana zapłaconej ceny {a}, sprzedaży: {b}", "en": "median paid {a}, {b} sales",
       "de": "Median bezahlt {a}, {b} Verkäufe", "tr": "ödenen ortanca {a}, {b} satış"}),
 28: (None, {"a": "int", "b": "int", "c": "x100"},
      {"pl": "Rejestr rynku", "en": "Market ledger", "de": "Marktbuch", "tr": "Pazar defteri"},
      {"pl": "brakuje {a} botom, na ladach {b} szt., mnożnik {c}", "en": "{a} bots short, {b} units on counters, multiplier {c}",
       "de": "{a} Bots fehlt es, {b} Stück auf Ladentischen, Faktor {c}", "tr": "{a} botta eksik, tezgâhlarda {b} adet, çarpan {c}"}),
 29: (None, {"a": "yang"},
      {"pl": "Ogranicznik kroku", "en": "Step limit", "de": "Schrittbegrenzung", "tr": "Adım sınırı"},
      {"pl": "chciał {a}", "en": "wanted {a}", "de": "wollte {a}", "tr": "istenen {a}"}),
 30: (None, {"a": "pct"},
      {"pl": "Rozrzut sprzedawcy", "en": "The keeper's spread", "de": "Spanne des Verkäufers", "tr": "Satıcı marjı"},
      {"pl": "{a}", "en": "{a}", "de": "{a}", "tr": "{a}"}),
 31: (None, {"a": "yang"},
      {"pl": "Próg dla towaru z bonusem", "en": "Floor for bonus goods", "de": "Untergrenze für Bonusware", "tr": "Bonuslu mal tabanı"},
      {"pl": "próg {a} za sztukę", "en": "floor {a} a unit", "de": "Untergrenze {a} pro Stück", "tr": "adet başına taban {a}"}),
 32: (None, {},
      {"pl": "Zaokrąglenie", "en": "Rounded", "de": "Gerundet", "tr": "Yuvarlandı"},
      {"pl": "", "en": "", "de": "", "tr": ""}),
 33: (None, {"a": "pct"},
      {"pl": "Zniżka biedaka", "en": "A poor bot's discount", "de": "Rabatt eines armen Bots", "tr": "Yoksul bot indirimi"},
      {"pl": "zniżka {a}", "en": "discount {a}", "de": "Rabatt {a}", "tr": "indirim {a}"}),
 34: (None, {"a": "int", "b": "enum:CLOCK_UNIT", "c": "enum:CLOCK_RESTART"},
      {"pl": "Zegar przeceny", "en": "Markdown clock", "de": "Preisnachlass-Uhr", "tr": "İndirim saati"},
      {"pl": "{a} {b}{c}", "en": "{a} {b}{c}", "de": "{a} {b}{c}", "tr": "{a} {b}{c}"}),
 35: (None, {"a": "pct"},
      {"pl": "Przecena", "en": "Markdown", "de": "Preisnachlass", "tr": "İndirim"},
      {"pl": "obniżka {a}", "en": "down {a}", "de": "minus {a}", "tr": "{a} düşüş"}),
 36: (None, {"a": "pct", "b": "pct", "c": "int"},
      {"pl": "Podwyżka", "en": "Markup", "de": "Preisaufschlag", "tr": "Zam"},
      {"pl": "podwyżka {a}: brakowało w {b} przeglądów, sprzedanych linii {c}", "en": "up {a}: missing in {b} of looks, {c} lines sold",
       "de": "plus {a}: fehlte bei {b} der Blicke, {c} Zeilen verkauft", "tr": "{a} artış: bakışların {b} kadarında yoktu, {c} satır satıldı"}),
 37: (None, {"a": "pct"},
      {"pl": "Podwyżka odrzucona", "en": "Markup refused", "de": "Aufschlag abgelehnt", "tr": "Zam reddedildi"},
      {"pl": "{a} przekroczyłoby limit yang", "en": "{a} would pass the yang limit",
       "de": "{a} würde die Yang-Grenze überschreiten", "tr": "{a} yang sınırını aşardı"}),
 38: (None, {"a": "yang"},
      {"pl": "Próg wystawienia", "en": "Listing floor", "de": "Einstell-Untergrenze", "tr": "Listeleme tabanı"},
      {"pl": "podniesiona do progu {a}", "en": "raised to the floor {a}", "de": "auf die Untergrenze {a} angehoben", "tr": "{a} tabanına yükseltildi"}),
 39: (None, {"a": "yang"},
      {"pl": "Pomyłka w cenie", "en": "Price slip", "de": "Preisfehler", "tr": "Fiyat hatası"},
      {"pl": "miało być {a}", "en": "meant {a}", "de": "gemeint war {a}", "tr": "olması gereken {a}"}),
 40: (None, {"a": "enum:GENERATION"},
      {"pl": "Pokolenie cen", "en": "Price generation", "de": "Preisgeneration", "tr": "Fiyat kuşağı"},
      {"pl": "{a}", "en": "{a}", "de": "{a}", "tr": "{a}"}),
 41: (None, {"a": "yang", "b": "minutes", "c": "enum:SLIP_BY"},
      {"pl": "Pomyłka poprawiona", "en": "Slip put right", "de": "Preisfehler korrigiert", "tr": "Fiyat hatası düzeltildi"},
      {"pl": "było {a} przez {b}; {c}", "en": "was {a} for {b}; {c}", "de": "war {a} für {b}; {c}", "tr": "{b} boyunca {a} idi; {c}"}),
 42: (None, {"a": "yang"},
      {"pl": "Podniesione do progu", "en": "Raised to the floor", "de": "Auf die Untergrenze angehoben", "tr": "Tabana yükseltildi"},
      {"pl": "było {a}", "en": "was {a}", "de": "war {a}", "tr": "önceden {a}"}),
 # MT2009 Plus: Cor Draconis, a Dragon Stone, a sash, the crafting goods.
 200: (None, {"a": "yang"},
       {"pl": "Cena operatora", "en": "The operator's price", "de": "Preis des Betreibers", "tr": "Operatörün fiyatı"},
       {"pl": "{a} za sztukę", "en": "{a} a unit", "de": "{a} pro Stück", "tr": "adet başına {a}"}),
}


# The listing flags (LFLAG_*), bit: texts.
DECISION_LFLAGS = {
 1: {"pl": "taniej niż u handlarza", "en": "under the merchant's price", "de": "unter dem Händlerpreis", "tr": "satıcı fiyatının altında"},
 2: {"pl": "cenę podniósł próg", "en": "a floor lifted the price", "de": "eine Untergrenze hob den Preis", "tr": "fiyatı bir taban yükseltti"},
 4: {"pl": "poniżej połowy arkusza", "en": "under half of the sheet", "de": "unter der Hälfte der Liste", "tr": "listenin yarısının altında"},
 8: {"pl": "ponad 3× arkusz", "en": "over 3× the sheet", "de": "über dem 3-Fachen der Liste", "tr": "listenin 3 katının üstünde"},
 16: {"pl": "pamięć sprzedaży mocno przesunęła cenę", "en": "the sale memory pulled the price hard",
      "de": "das Verkaufsgedächtnis zog den Preis stark", "tr": "satış hafızası fiyatı sertçe çekti"},
 32: {"pl": "rejestr na granicy mnożnika", "en": "the ledger at its multiplier's edge",
      "de": "das Marktbuch am Rand seines Faktors", "tr": "defter çarpanının sınırında"},
 64: {"pl": "przecena na maksimum", "en": "markdown at its maximum", "de": "Preisnachlass am Maximum", "tr": "indirim en üst düzeyde"},
 128: {"pl": "podwyżka na maksimum", "en": "markup at its maximum", "de": "Aufschlag am Maximum", "tr": "zam en üst düzeyde"},
 256: {"pl": "pomyłka w cenie", "en": "price slip", "de": "Preisfehler", "tr": "fiyat hatası"},
 512: {"pl": "lepsze od noszonego i wkrótce do założenia", "en": "better than the worn piece and wearable soon",
       "de": "besser als das getragene Stück und bald tragbar", "tr": "giyilenden iyi ve yakında giyilebilir"},
 1024: {"pl": "lada odrzuciła wyższego kandydata", "en": "a higher-ranked candidate was refused",
        "de": "ein höher eingestufter Kandidat wurde abgelehnt", "tr": "daha üst sıradaki bir aday reddedildi"},
 2048: {"pl": "przecena o ponad 50%", "en": "a reprice moved it over 50%", "de": "eine Neubepreisung um über 50%", "tr": "yeniden fiyatlama %50'den fazla oynattı"},
}


# The equipment flags (EFLAG_*), bit: texts.
DECISION_EFLAGS = {
 1: {"pl": "plus spadł o 2 lub więcej", "en": "the plus fell by 2 or more", "de": "das Plus fiel um 2 oder mehr", "tr": "artı 2 ya da daha fazla düştü"},
 2: {"pl": "niższy wymagany poziom", "en": "a lower level limit", "de": "eine niedrigere Levelgrenze", "tr": "daha düşük seviye sınırı"},
 4: {"pl": "gorsze bonusy niż stary", "en": "worse lines than the old piece", "de": "schlechtere Zeilen als das alte Stück", "tr": "eskisinden kötü satırlar"},
 8: {"pl": "zysk poniżej 2%", "en": "a gain under 2%", "de": "ein Gewinn unter 2%", "tr": "%2'nin altında kazanç"},
 16: {"pl": "zamiana tam i z powrotem w godzinę", "en": "swapped back within the hour",
      "de": "innerhalb einer Stunde zurückgetauscht", "tr": "bir saat içinde geri değiştirildi"},
 32: {"pl": "stary wyzerowany regułą", "en": "the old piece scored 0 by a rule", "de": "das alte Stück per Regel mit 0 bewertet", "tr": "eski parça bir kuralla 0 puan aldı"},
 64: {"pl": "zastąpiony projekt lub nagroda", "en": "a project or a prize replaced", "de": "ein Projekt oder ein Preis ersetzt", "tr": "bir proje ya da ödül değiştirildi"},
 128: {"pl": "limit pominął wcześniejsze wpisy", "en": "the budget dropped rows before this one",
       "de": "das Budget ließ davor Zeilen weg", "tr": "bütçe bundan önce satırları atladı"},
 256: {"pl": "po spaleniu przy ulepszaniu", "en": "after a burn at the anvil", "de": "nach einem Verbrennen am Amboss", "tr": "örste yanmadan sonra"},
}


# Which pass decided it (PATH_*), the rule it followed (RULE_*) and the moment it
# came at (CONTEXT_*).
DECISION_PATHS = {
 0: {"pl": "Nieznana ścieżka", "en": "Unknown path", "de": "Unbekannter Pfad", "tr": "Bilinmeyen yol"},
 1: {"pl": "Przegląd ekwipunku", "en": "The equipment pass", "de": "Der Ausrüstungsdurchlauf", "tr": "Ekipman taraması"},
 2: {"pl": "Pusta ręka", "en": "An empty hand", "de": "Eine leere Hand", "tr": "Boş el"},
 3: {"pl": "Broń awaryjna od handlarza", "en": "An emergency weapon from the merchant", "de": "Eine Notfallwaffe vom Händler", "tr": "Satıcıdan acil silah"},
 4: {"pl": "Zdjęcie broni niepasującej do profesji", "en": "Taking off a weapon wrong for the profession",
     "de": "Ablegen einer Waffe, die nicht zur Lehre passt", "tr": "Beceri grubuna uymayan silahın çıkarılması"},
 5: {"pl": "Zdjęcie przedmiotu unikalnego", "en": "Taking off a unique item", "de": "Ablegen eines Unikats", "tr": "Eşsiz bir eşyanın çıkarılması"},
}


DECISION_RULES = {
 0: {"pl": "nieznana reguła", "en": "unknown rule", "de": "unbekannte Regel", "tr": "bilinmeyen kural"},
 1: {"pl": "lepszy wynik", "en": "a better score", "de": "ein besserer Wert", "tr": "daha iyi puan"},
 2: {"pl": "pusty slot", "en": "an empty slot", "de": "ein leerer Platz", "tr": "boş yuva"},
 3: {"pl": "przypięte przez właściciela towarzysza", "en": "pinned by the companion's owner",
     "de": "vom Besitzer des Begleiters festgelegt", "tr": "yoldaşın sahibi tarafından sabitlendi"},
 4: {"pl": "stara broń zakazana (słaba broń od 30 lv)", "en": "the old weapon banned (a low weapon from level 30)",
     "de": "die alte Waffe verboten (eine schwache Waffe ab Lv 30)", "tr": "eski silah yasaklı (30. seviyeden itibaren zayıf silah)"},
 5: {"pl": "łucznik zmienia broń na kamień", "en": "an archer's switch for a stone", "de": "Wechsel eines Bogenschützen für einen Stein", "tr": "okçunun taş için silah değişimi"},
 6: {"pl": "najlepsza broń do pustej ręki", "en": "the best weapon for an empty hand", "de": "die beste Waffe für eine leere Hand", "tr": "boş el için en iyi silah"},
 7: {"pl": "słaba broń jako ostatnie wyjście", "en": "a low weapon as the last resort", "de": "eine schwache Waffe als letzter Ausweg", "tr": "son çare olarak zayıf silah"},
 8: {"pl": "broń awaryjna", "en": "an emergency weapon", "de": "eine Notfallwaffe", "tr": "acil silah"},
 9: {"pl": "broń nie pasuje do profesji", "en": "the weapon does not fit the profession", "de": "die Waffe passt nicht zur Lehre", "tr": "silah beceri grubuna uymuyor"},
 10: {"pl": "unikat, którego bot nigdy nie nosi", "en": "a unique the bot never wears", "de": "ein Unikat, das der Bot nie trägt", "tr": "botun asla takmadığı eşsiz eşya"},
}


DECISION_CONTEXTS = {
 0: {"pl": "bez szczególnych okoliczności", "en": "no particular moment", "de": "kein besonderer Moment", "tr": "özel bir an değil"},
 1: {"pl": "po wizycie u kowala", "en": "after the blacksmith", "de": "nach dem Schmied", "tr": "demirciden sonra"},
 2: {"pl": "po spaleniu noszonego przedmiotu", "en": "after the worn piece burned", "de": "nachdem das getragene Stück verbrannte", "tr": "giyili parça yandıktan sonra"},
 3: {"pl": "tuż po wejściu do gry (do 120 s)", "en": "just after spawning (120 s or less)", "de": "direkt nach dem Erscheinen (bis 120 s)", "tr": "oyuna girdikten hemen sonra (120 sn'ye kadar)"},
 4: {"pl": "po zabraniu z lady", "en": "back from the counter", "de": "vom Ladentisch zurück", "tr": "tezgâhtan dönünce"},
 5: {"pl": "po wystawieniu noszonego przedmiotu na własną ladę", "en": "after the worn piece went on the bot's own counter",
     "de": "nachdem das getragene Stück auf den eigenen Ladentisch kam", "tr": "giyili parça botun kendi tezgâhına konduktan sonra"},
}


# Where the new piece came from (ORIGIN_*): code: (the kind of its ref, written
# {a}; the four texts). Anything the core took no note of is "unknown".
DECISION_ORIGINS = {
 0: ({}, {"pl": "nieznane (łup, skrzynia, wymiana)", "en": "unknown (a drop, a chest, a trade)",
          "de": "unbekannt (Beute, Truhe, Handel)", "tr": "bilinmiyor (ganimet, sandık, takas)"}),
 1: ({"a": "enum:LADDER_CAT"},
     {"pl": "kupione u handlarza z drabinki ({a})", "en": "bought from the merchant, off the ladder ({a})",
      "de": "beim Händler gekauft, nach der Leiter ({a})", "tr": "satıcıdan merdivene göre alındı ({a})"}),
 2: ({"a": "yang"},
     {"pl": "kupione awaryjnie u handlarza za {a}", "en": "bought in an emergency from the merchant for {a}",
      "de": "im Notfall beim Händler gekauft für {a}", "tr": "satıcıdan acil olarak {a} karşılığında alındı"}),
 3: ({"a": "yang"},
     {"pl": "kupione u handlarza jako właściwa broń za {a}", "en": "bought from the merchant as a proper weapon for {a}",
      "de": "beim Händler als richtige Waffe gekauft für {a}", "tr": "satıcıdan uygun silah olarak {a} karşılığında alındı"}),
 4: ({"a": "int"},
     {"pl": "ulepszone u kowala z przedmiotu #{a}", "en": "refined at the anvil from item #{a}",
      "de": "am Amboss verbessert aus Gegenstand #{a}", "tr": "örste #{a} eşyasından yükseltildi"}),
 5: ({"a": "int"},
     {"pl": "zabrane z lady do założenia (zysk {a})", "en": "taken back from the counter to wear (gain {a})",
      "de": "vom Ladentisch zum Anlegen zurückgenommen (Gewinn {a})", "tr": "giymek için tezgâhtan geri alındı (kazanç {a})"}),
 6: ({"a": "enum:SAFEBOX_WHY"},
     {"pl": "wyjęte z magazynu ({a})", "en": "taken out of the safebox ({a})", "de": "aus dem Lager genommen ({a})", "tr": "depodan alındı ({a})"}),
 7: ({"a": "int"},
     {"pl": "zrobione przez hazardzistę (rodzaj {a})", "en": "made by a gambler (kind {a})",
      "de": "von einem Glücksspieler gemacht (Art {a})", "tr": "bir kumarbaz yaptı (tür {a})"}),
}


# What a piece is to its bot (ROLE_* bits).
DECISION_ROLES = {
 1: {"pl": "rodzina broni 30 lv", "en": "a level-30 weapon family", "de": "Waffenfamilie Lv 30", "tr": "30. seviye silah ailesi"},
 2: {"pl": "broń 30 lv tej klasy", "en": "the class's level-30 weapon", "de": "die Waffe Lv 30 der Klasse", "tr": "sınıfın 30. seviye silahı"},
 4: {"pl": "projekt broni 30 lv", "en": "a level-30 weapon project", "de": "Projekt Waffe Lv 30", "tr": "30. seviye silah projesi"},
 8: {"pl": "projekt bonusów", "en": "a lines project", "de": "Bonus-Projekt", "tr": "bonus projesi"},
 16: {"pl": "Stalka", "en": "Stalki", "de": "Stalki", "tr": "Stalki"},
 32: {"pl": "Stalka zatrzymana na poziom", "en": "Stalki kept for the level", "de": "Stalki für das Level behalten", "tr": "seviye için tutulan Stalki"},
 64: {"pl": "zapas na wyższy stopień", "en": "a higher-tier spare", "de": "Ersatz für eine höhere Stufe", "tr": "üst kademe yedeği"},
 128: {"pl": "zapas", "en": "a backup", "de": "Ersatz", "tr": "yedek"},
 256: {"pl": "przedmiot reguły zwojów", "en": "a scroll-rule piece", "de": "Stück der Schriftrollen-Regel", "tr": "parşömen kuralı parçası"},
 512: {"pl": "nagroda", "en": "a prize", "de": "ein Preis", "tr": "ödül"},
 1024: {"pl": "biżuteria z listy", "en": "a listed jewel", "de": "gelisteter Schmuck", "tr": "listelenmiş takı"},
 2048: {"pl": "z kamieniami duszy", "en": "with spirit stones", "de": "mit Geiststeinen", "tr": "ruh taşlı"},
 4096: {"pl": "zestaw hazardzisty", "en": "a gamble set", "de": "Glücksspiel-Satz", "tr": "kumar seti"},
 8192: {"pl": "przypięte przez właściciela", "en": "pinned by the owner", "de": "vom Besitzer festgelegt", "tr": "sahibi sabitledi"},
 16384: {"pl": "zakazana słaba broń", "en": "a banned low weapon", "de": "verbotene schwache Waffe", "tr": "yasaklı zayıf silah"},
 32768: {"pl": "broń tylko na zwoje", "en": "a scroll-only weapon", "de": "Waffe nur mit Schriftrollen", "tr": "yalnız parşömenli silah"},
}


# The score's terms and the weapon blow's (TERM_*): code: (the divisor the value
# is written with - BLOW and DEFENCE are x1000 -, the four names). 20..38 are
# the blow model's own (TPlayerBotBlowParts), shown beside each other.
DECISION_TERMS = {
 0: (1, {"pl": "nieznany składnik", "en": "unknown term", "de": "unbekannter Bestandteil", "tr": "bilinmeyen bileşen"}),
 1: (1, {"pl": "Wynik", "en": "Score", "de": "Wert", "tr": "Puan"}),
 2: (1000, {"pl": "Cios", "en": "Blow", "de": "Schlag", "tr": "Darbe"}),
 3: (1, {"pl": "Preferencja klasy", "en": "Class preference", "de": "Klassenvorliebe", "tr": "Sınıf tercihi"}),
 4: (1000, {"pl": "Obrona", "en": "Defence", "de": "Verteidigung", "tr": "Savunma"}),
 5: (1, {"pl": "Rozstrzygnięcie poziomem", "en": "Level tie-break", "de": "Entscheidung nach Level", "tr": "Seviye eşitlik bozucu"}),
 6: (1, {"pl": "Stałe bonusy przedmiotu", "en": "The item's fixed bonuses", "de": "Feste Boni des Gegenstands", "tr": "Eşyanın sabit bonusları"}),
 7: (1, {"pl": "Linie bonusów", "en": "Bonus lines", "de": "Bonuszeilen", "tr": "Bonus satırları"}),
 8: (1, {"pl": "Kamienie duszy", "en": "Spirit stones", "de": "Geiststeine", "tr": "Ruh taşları"}),
 9: (1, {"pl": "Odporności", "en": "Immunities", "de": "Immunitäten", "tr": "Bağışıklıklar"}),
 10: (1, {"pl": "Linie na rasy", "en": "Race lines", "de": "Rassenzeilen", "tr": "Irk satırları"}),
 11: (1, {"pl": "Stopień z listy Iwakury (%)", "en": "Iwakura's tier (%)", "de": "Iwakuras Stufe (%)", "tr": "Iwakura'nın kademesi (%)"}),
 12: (1, {"pl": "Biżuteria z listy (%)", "en": "Listed jewel (%)", "de": "Gelisteter Schmuck (%)", "tr": "Listelenmiş takı (%)"}),
 13: (1, {"pl": "Rodzina butów", "en": "Boots family", "de": "Schuhfamilie", "tr": "Ayakkabı ailesi"}),
 14: (1, {"pl": "Wymagany poziom", "en": "Level limit", "de": "Levelgrenze", "tr": "Seviye sınırı"}),
 15: (1, {"pl": "Plus", "en": "Plus", "de": "Plus", "tr": "Artı"}),
 20: (1, {"pl": "Cios (razem)", "en": "Blow (total)", "de": "Schlag (gesamt)", "tr": "Darbe (toplam)"}),
 21: (1, {"pl": "Rzut ataku broni", "en": "Weapon attack roll", "de": "Angriffswurf der Waffe", "tr": "Silah saldırı zarı"}),
 22: (1, {"pl": "Rzut ataku magicznego", "en": "Magic attack roll", "de": "Magischer Angriffswurf", "tr": "Büyü saldırı zarı"}),
 23: (1, {"pl": "Atak z plusa", "en": "Attack from the plus", "de": "Angriff durch das Plus", "tr": "Artıdan gelen saldırı"}),
 24: (1, {"pl": "Stopień", "en": "Grade", "de": "Stufe", "tr": "Derece"}),
 25: (1, {"pl": "Wartość ataku %", "en": "Attack value %", "de": "Angriffswert %", "tr": "Saldırı değeri %"}),
 26: (1, {"pl": "Silny przeciw rasie %", "en": "Strong against the race %", "de": "Stark gegen die Rasse %", "tr": "Irka karşı güçlü %"}),
 27: (1, {"pl": "Średnie obrażenia %", "en": "Average damage %", "de": "Durchschnittsschaden %", "tr": "Ortalama zarar %"}),
 28: (1, {"pl": "Obrażenia umiejętności %", "en": "Skill damage %", "de": "Fertigkeitsschaden %", "tr": "Beceri hasarı %"}),
 29: (1, {"pl": "Cios krytyczny %", "en": "Critical hit %", "de": "Kritischer Treffer %", "tr": "Kritik vuruş %"}),
 30: (1, {"pl": "Przeszywające uderzenie %", "en": "Piercing hit %", "de": "Durchbohrender Treffer %", "tr": "Delici vuruş %"}),
 31: (1, {"pl": "Ukryty bonus poziomu %", "en": "Hidden level bonus %", "de": "Versteckter Levelbonus %", "tr": "Gizli seviye bonusu %"}),
 32: (1, {"pl": "Atak", "en": "Attack", "de": "Angriff", "tr": "Saldırı"}),
 33: (1, {"pl": "Obrona potwora", "en": "Monster defence", "de": "Monsterverteidigung", "tr": "Canavar savunması"}),
 34: (1, {"pl": "Zwykłe uderzenie", "en": "Plain hit", "de": "Normaler Treffer", "tr": "Normal vuruş"}),
 35: (1, {"pl": "Uderzenie umiejętnością", "en": "Skill hit", "de": "Fertigkeitstreffer", "tr": "Beceri vuruşu"}),
 36: (1, {"pl": "Styl walki", "en": "Fighting style", "de": "Kampfstil", "tr": "Dövüş tarzı"}),
 37: (1, {"pl": "Udział zwykłych uderzeń", "en": "Plain-hit share", "de": "Anteil normaler Treffer", "tr": "Normal vuruş payı"}),
 38: (1, {"pl": "Udział umiejętności", "en": "Skill share", "de": "Fertigkeitsanteil", "tr": "Beceri payı"}),
 40: (1, {"pl": "Linie wg stopni (ocena)", "en": "Tiered lines score", "de": "Zeilenwert nach Stufen", "tr": "Kademeli satır puanı"}),
}


DECISION_BLOW_TERMS = frozenset(range(20, 39))


# The terms that add up to a score, and of the blow the ones that move it: the
# three with the largest change are the ones a report should start from.
DECISION_TERM_RANKED = frozenset((2, 3, 4, 5, 6, 7, 8, 9, 10, 13))


DECISION_BLOW_RANKED = frozenset(range(21, 36))


# What the code calls each number, for the export an analyst or a language
# model reads ("Kopiuj dla AI", exMarkdown): the words above are the page's
# language, and these are the rules header's own names, so a report that says
# STEP_LEDGER or RULE_SCORE_UPGRADE can be looked up in
# playerbot_explain_rules.h and in the pass that writes it.
# tests/admin_panel_decisions_test.py checks every one against the header.
DECISION_CODE_NAMES = {
 "EVENT": {
  1: "EVENT_LIST_CREATE", 2: "EVENT_LIST_ADD", 3: "EVENT_LIST_CLASSIC", 4: "EVENT_REPRICE", 5: "EVENT_SLIP_FIX",
  6: "EVENT_SLIP_FIX_CORE", 7: "EVENT_RAISE_TO_FLOOR", 8: "EVENT_TAKE_OFF", 9: "EVENT_RECLAIM", 10: "EVENT_SOLD",
 },
 "GOODS": {
  0: "GOODS_UNKNOWN", 1: "GOODS_POLICY_STALL", 2: "GOODS_GAMBLE_GOODS", 3: "GOODS_GM_STONE_SPARE", 4: "GOODS_MARBLE",
  5: "GOODS_METIN_DETECTOR", 6: "GOODS_CATACOMB_HEAD", 7: "GOODS_BONUS_STONE_SPARE",
  8: "GOODS_FORGET_SCROLL_MERCHANT_ONLY", 9: "GOODS_STALKI_OTHER", 10: "GOODS_LEVEL30_SALE_READY",
  11: "GOODS_LEVEL30_NOT_KEPT", 12: "GOODS_LOW_LEVEL_LOW_PLUS", 13: "GOODS_LOW_LEVEL_REFINED",
  14: "GOODS_LPP_SURPLUS", 15: "GOODS_VALUABLE_BONUS", 16: "GOODS_MERCHANT_ONLY_GEAR", 17: "GOODS_PRECIOUS_SPARE",
  18: "GOODS_SAFE_SCROLL_OVER_KEEP", 19: "GOODS_HORSE_MEDAL", 20: "GOODS_MATERIAL_FLOOR", 21: "GOODS_MATERIAL_SHORT",
  22: "GOODS_MATERIAL_OTHER", 23: "GOODS_PICKUP_GOODS", 24: "GOODS_SURPLUS_POTION", 25: "GOODS_SURPLUS_RECIPE",
  26: "GOODS_HAIR_DYE_SHOP", 27: "GOODS_HAIR_DYE_FISHED_KEPT", 28: "GOODS_ISHOP_HAIRSTYLE",
  29: "GOODS_FORGET_SCROLL", 30: "GOODS_MAGIC_DUST", 31: "GOODS_SOUL_STONE_LOW_MARKET", 32: "GOODS_SOUL_STONE",
  33: "GOODS_GENERAL_BOOK", 34: "GOODS_SHEET_GOODS", 35: "GOODS_SKILL_BOOK_DROPPER",
  36: "GOODS_SKILL_BOOK_OWN_SPARE", 37: "GOODS_SKILL_BOOK_OTHER_CLASS", 38: "GOODS_LOW_PLUS_GEAR",
  39: "GOODS_SCRAP_KEEPER_LOW", 40: "GOODS_SURPLUS_CHEST", 41: "GOODS_SURPLUS_KEY",
  200: "GOODS_RARE_GOODS", 201: "GOODS_GUILD_MATERIAL", 202: "GOODS_DRAGON_STONE_SPARE",
  203: "GOODS_ACCESSORY_STONE_SPARE",
 },
 "SHAPE": {
  0: "SHAPE_WHOLE_STACK", 1: "SHAPE_NATURAL_LINE", 2: "SHAPE_POTION_PACK", 3: "SHAPE_DUST_PACK",
  4: "SHAPE_MEDAL_PAIR", 5: "SHAPE_COUNTED_SINGLE", 6: "SHAPE_KEY_SINGLE", 7: "SHAPE_CHEST_PACK",
  8: "SHAPE_CLASSIC_SPLIT_SINGLE",
 },
 "OFF": {
  1: "OFF_LPP", 2: "OFF_SOUL_STONE_DUST", 3: "OFF_MARBLE", 4: "OFF_SAME_VNUM", 5: "OFF_MISSION_BOOKS",
  6: "OFF_JUNK_WEAPON", 7: "OFF_POTION_PACK", 8: "OFF_LOW_ARMOUR", 9: "OFF_LOW_JEWEL", 10: "OFF_HAIR_DYE",
  11: "OFF_LEVEL30_ANVIL", 12: "OFF_CHEST_PACK", 13: "OFF_GM_STONE", 14: "OFF_SCROLL_PACK", 15: "OFF_MEDAL_PACK",
  16: "OFF_HEAP_PACK", 17: "OFF_MATERIAL_PACK", 18: "OFF_LOW_GEAR", 19: "OFF_TACKLE", 20: "OFF_RECLAIM_TO_WEAR",
  200: "OFF_BUFF_POTION", 201: "OFF_DS_LOW_GRADE", 202: "OFF_SASH_KEEPER", 203: "OFF_CRAFT_EXCHANGE", 204: "OFF_RARE_UNSOLD",
 },
 "STEP": {
  1: "STEP_CONTEXT", 2: "STEP_BONUS_LINE", 3: "STEP_BONUS_MAX_LINES", 4: "STEP_BONUS_PERCENT", 5: "STEP_INVESTMENT",
  6: "STEP_SHEET_GEAR", 7: "STEP_SOCKET_STONES", 8: "STEP_FLAT_PLUS", 9: "STEP_SCRAP", 10: "STEP_INVESTMENT_FLOOR",
  11: "STEP_BONUS_PREMIUM", 12: "STEP_COMPETITION", 13: "STEP_SHEET_MARBLE", 14: "STEP_COUNT",
  15: "STEP_PRIOR_MERCHANT", 16: "STEP_PRIOR_LEVEL30", 17: "STEP_SHEET_BOOK", 18: "STEP_SHEET_GENERAL_BOOK",
  19: "STEP_SHEET_MATERIAL", 20: "STEP_CHEST_WORTH", 21: "STEP_SHEET_FORGET_SCROLL", 22: "STEP_FIXED_PRIOR",
  23: "STEP_SHEET_SOUL_STONE", 24: "STEP_NO_MERCHANT_PRIOR", 25: "STEP_WALLET", 26: "STEP_WALLET_STACK_CAP",
  27: "STEP_SALE_MEMORY", 28: "STEP_LEDGER", 29: "STEP_STEP_LIMIT", 30: "STEP_SPREAD", 31: "STEP_BONUS_GOODS_FLOOR",
  32: "STEP_ROUND", 33: "STEP_POOR_DISCOUNT", 34: "STEP_MARKDOWN_CLOCK", 35: "STEP_MARKDOWN", 36: "STEP_MARKUP",
  37: "STEP_MARKUP_REFUSED", 38: "STEP_LISTING_FLOOR", 39: "STEP_SLIP", 40: "STEP_GENERATION",
  41: "STEP_SLIP_PUT_RIGHT", 42: "STEP_RAISED_TO_FLOOR", 200: "STEP_OPERATOR_PRICE",
 },
 "LFLAG": {
  1: "LFLAG_UNDER_MERCHANT", 2: "LFLAG_FLOOR_BOUND", 4: "LFLAG_UNDER_SHEET_HALF", 8: "LFLAG_OVER_SHEET_3X",
  16: "LFLAG_MEMORY_PULL", 32: "LFLAG_REGULATOR_EDGE", 64: "LFLAG_MARKDOWN_MAX", 128: "LFLAG_MARKUP_MAX",
  256: "LFLAG_SLIP", 512: "LFLAG_BETTER_THAN_WORN_SOON", 1024: "LFLAG_LOW_RANK", 2048: "LFLAG_PRICE_JUMP",
 },
 "PATH": {
  1: "PATH_EQUIP_PASS", 2: "PATH_EMPTY_HAND", 3: "PATH_EMERGENCY_BUY", 4: "PATH_PROFESSION_OFF", 5: "PATH_UNIQUE",
 },
 "RULE": {
  1: "RULE_SCORE_UPGRADE", 2: "RULE_EMPTY_SLOT", 3: "RULE_OWNER_PIN", 4: "RULE_OLD_LOW_WEAPON_BANNED",
  5: "RULE_ARCHER_STONE_SWITCH", 6: "RULE_EMPTY_HAND_BEST", 7: "RULE_EMPTY_HAND_LOW_FALLBACK",
  8: "RULE_EMERGENCY_WEAPON", 9: "RULE_PROFESSION_MISMATCH", 10: "RULE_UNIQUE_NEVER_WORN",
 },
 "CONTEXT": {
  0: "CONTEXT_NONE", 1: "CONTEXT_AFTER_BLACKSMITH", 2: "CONTEXT_AFTER_BURN", 3: "CONTEXT_AFTER_SPAWN",
  4: "CONTEXT_RETURNED_FROM_COUNTER", 5: "CONTEXT_WENT_TO_COUNTER",
 },
 "ORIGIN": {
  0: "ORIGIN_UNKNOWN", 1: "ORIGIN_NPC_LADDER", 2: "ORIGIN_NPC_EMERGENCY", 3: "ORIGIN_NPC_PROPER_WEAPON",
  4: "ORIGIN_REFINED", 5: "ORIGIN_RECLAIMED", 6: "ORIGIN_SAFEBOX", 7: "ORIGIN_GAMBLER",
 },
 "ROLE": {
  1: "ROLE_LEVEL30_FAMILY", 2: "ROLE_CLASS_LEVEL30", 4: "ROLE_LEVEL30_PROJECT", 8: "ROLE_LINES_PROJECT",
  16: "ROLE_STALKI", 32: "ROLE_STALKI_KEPT", 64: "ROLE_HIGHER_TIER_SPARE", 128: "ROLE_BACKUP",
  256: "ROLE_SCROLL_RULE_PIECE", 512: "ROLE_PRIZE", 1024: "ROLE_LISTED_JEWEL", 2048: "ROLE_SOUL_STONES",
  4096: "ROLE_GAMBLE_SET", 8192: "ROLE_OWNER_PINNED", 16384: "ROLE_BANNED_LOW_WEAPON",
  32768: "ROLE_SCROLL_ONLY_WEAPON",
 },
 "TERM": {
  1: "TERM_TOTAL", 2: "TERM_BLOW_X1000", 3: "TERM_CLASS_PREF", 4: "TERM_DEFENCE_X1000", 5: "TERM_LEVEL_TIE",
  6: "TERM_PROTO_APPLIES", 7: "TERM_LINES", 8: "TERM_SOUL_STONES", 9: "TERM_IMMUNE", 10: "TERM_RACE_LINES",
  11: "TERM_TIER_PCT", 12: "TERM_LISTED_JEWEL_PCT", 13: "TERM_BOOTS_FAMILY", 14: "TERM_LEVEL_LIMIT", 15: "TERM_PLUS",
  20: "TERM_BLOW", 21: "TERM_ROLL", 22: "TERM_MAGIC_ROLL", 23: "TERM_PLUS_ATTACK", 24: "TERM_GRADE",
  25: "TERM_ATT_PCT", 26: "TERM_RACE_PCT", 27: "TERM_AVG_PCT", 28: "TERM_SKILL_PCT", 29: "TERM_CRIT_PCT",
  30: "TERM_PEN_PCT", 31: "TERM_LEVEL_BONUS_PCT", 32: "TERM_ATTACK", 33: "TERM_MOB_DEFENCE", 34: "TERM_HIT",
  35: "TERM_SKILL_HIT", 36: "TERM_STYLE", 37: "TERM_HIT_SHARE", 38: "TERM_SKILL_SHARE", 40: "TERM_LINES_TIERED",
 },
 "EFLAG": {
  1: "EFLAG_PLUS_DOWN", 2: "EFLAG_LEVEL_DOWN", 4: "EFLAG_LINES_WORSE", 8: "EFLAG_SMALL_GAIN", 16: "EFLAG_FLIP_FLOP",
  32: "EFLAG_OLD_ZEROED", 64: "EFLAG_PROJECT_REPLACED", 128: "EFLAG_SUPPRESSED_BEFORE", 256: "EFLAG_AFTER_BURN",
 },
}


def decision_code_name(prefix, code):
    """The header's name of a code ("STEP_LEDGER"), or PREFIX_<n> for one a
    newer core wrote and this panel does not know yet."""
    code = int(code or 0)
    return DECISION_CODE_NAMES.get(prefix, {}).get(code) or "%s_%d" % (prefix, code)


# What the panel reads out of a row beyond the core's own flags: the mistakes
# the explanations found on 29 September, so the next of their kind shows at a
# glance and can be filtered for on /decisions. A weapon put on over one whose
# modelled blow was higher (the tier nudge that overruled the blow), a level-30
# weapon taken off for another weapon (the operator's question: is it the model
# or a rule?), and a weapon listed as a spare while a rod or a pickaxe was in
# the hand (the bot's own weapon on its counter after fishing). {a} is the
# numbers that make it one.
DECISION_ANOMALIES = {
 "blow_down": {"pl": "słabszy cios niż zdjęta broń: {a}", "en": "a weaker blow than the weapon taken off: {a}",
               "de": "ein schwächerer Schlag als die abgelegte Waffe: {a}", "tr": "çıkarılan silahtan zayıf darbe: {a}"},
 "level30_off": {"pl": "broń 30 lv zastąpiona inną bronią", "en": "a level-30 weapon replaced by another weapon",
                 "de": "eine Waffe Lv 30 durch eine andere Waffe ersetzt", "tr": "30. seviye silah başka bir silahla değiştirildi"},
 "tool_spare": {"pl": "wystawione jako zapas, gdy w ręce było narzędzie ({a})",
                "en": "listed as a spare while a tool was in the hand ({a})",
                "de": "als Ersatz eingestellt, während ein Werkzeug in der Hand war ({a})",
                "tr": "elde bir alet varken yedek olarak tezgâha kondu ({a})"},
 "worn_listed": {"pl": "noszony przedmiot trafił na własną ladę bota", "en": "the worn piece went on the bot's own counter",
                 "de": "das getragene Stück kam auf den eigenen Ladentisch des Bots",
                 "tr": "giyili parça botun kendi tezgâhına kondu"},
}


# A rod (27400..27599 on mt2009) or a pickaxe (29101..29199) in the weapon slot.
DECISION_TOOL_RANGES = ((27400, 27599), (29101, 29199))


def decision_is_tool(vnum):
    vnum = int(vnum or 0)
    return any(low <= vnum <= high for low, high in DECISION_TOOL_RANGES)


def _dx_anomaly(code, a, language):
    texts = DECISION_ANOMALIES[code]
    return {"code": code, "label": _dx_pick(texts, language).replace("{a}", a)}


def decision_equip_anomalies(row, new_terms, old_terms, language):
    """The anomalies of an equipment row: a change after the worn piece went on
    the bot's own counter (CONTEXT_WENT_TO_COUNTER), a weapon swap by score
    whose new blow (TERM_BLOW) is under the old one's, and a level-30 weapon
    taken off for a weapon of another family."""
    out = []
    if int(row.get("context") or 0) == 5:
        out.append(_dx_anomaly("worn_listed", "", language))
    if int(row.get("wear") or 0) != 4 or not int(row.get("new_vnum") or 0) or not int(row.get("old_vnum") or 0):
        return out
    new_blow, old_blow = new_terms.get(20), old_terms.get(20)
    if int(row.get("path") or 0) == 1 and int(row.get("rule") or 0) == 1 and new_blow is not None \
            and old_blow and new_blow < old_blow:
        change = (new_blow - old_blow) * 100.0 / old_blow
        tier_new, tier_old = new_terms.get(11), old_terms.get(11)
        detail = "%d → %d (%s%%)" % (old_blow, new_blow, _dx_dec(change, 1, language))
        if tier_new is not None and tier_old is not None and tier_new != tier_old:
            detail += "; %s %+d%% / %+d%%" % (decision_code_name("TERM", 11), tier_old, tier_new)
        out.append(_dx_anomaly("blow_down", detail, language))
    if int(row.get("old_roles") or 0) & 1 and not int(row.get("new_roles") or 0) & 1:
        out.append(_dx_anomaly("level30_off", "", language))
    return out


def decision_listing_anomalies(row, language):
    """The anomalies of a counter line: a spare whose "worn piece" (why_b of
    GOODS_PRECIOUS_SPARE) was a rod or a pickaxe - the weapon the hand takes
    back after the session, put on the counter."""
    out = []
    if int(row.get("why") or 0) == 17 and decision_is_tool(row.get("why_b")):
        out.append(_dx_anomaly("tool_spare", localized_item_name(int(row.get("why_b") or 0), language), language))
    return out


# The steps that give a line its sheet price for one unit (their value is the
# price after them), and the one that puts the lines' premium on it.
DECISION_SHEET_STEPS = frozenset((6, 13, 17, 18, 19, 20, 21, 23))


def decision_sheet_ratio(row, steps):
    """What one unit asks against the sheet's price for it, from the steps: the
    first sheet step's value, with STEP_BONUS_PREMIUM's after it for gear, and
    the unit the line meant (a slip's price meant, STEP_SLIP's a). None when
    the price came from no sheet."""
    sheet = None
    meant = None
    for code, value, a, _b, _c in steps:
        if code in DECISION_SHEET_STEPS and sheet is None:
            sheet = value
        elif code == 11 and sheet:
            sheet = value
        elif code == 39:
            meant = a
    count = max(1, int(row.get("count") or 0))
    price = meant or int(row.get("list_price") or 0)
    if not sheet or not price:
        return None
    return {"unit": price // count, "sheet": sheet, "ratio": float(price) / count / sheet}


# ---- reading and saying the explanations -------------------------------------
# The wear slots the card knows, by the engine's wear number, and the word the
# live map uses for each. One map for the inventory card and the explanations.
WEAR_SLOT_KEYS = {
    0: "body",       # Zbroja
    1: "head",       # Hełm
    2: "foots",      # Buty
    3: "wrist",      # Bransoleta
    4: "weapon",     # Broń
    5: "neck",       # Naszyjnik
    6: "ear",        # Kolczyki
    10: "shield",    # Tarcza (WEAR_SHIELD; slot 7 is WEAR_UNIQUE1)
}


WEAR_SLOT_WORDS = {"body": "armor", "head": "helmet", "foots": "boots", "wrist": "bracelet",
                   "weapon": "weapon", "neck": "necklace", "ear": "earrings", "shield": "shield"}


WEAR_UNIQUE_SLOTS = frozenset((7, 8))


def decode_explain_pairs(text):
    """A steps or terms column as [(code, value, a, b, c)], and whether it ends
    in the "~" that says the core ran out of room. A part it cannot read is
    left out rather than taking the rest with it."""
    out, cut = [], False
    for part in str(text or "").split(";"):
        part = part.strip()
        if not part:
            continue
        if part == "~":
            cut = True
            continue
        code, sep, rest = part.partition("=")
        if not sep:
            continue
        try:
            numbers = [int(x) for x in rest.split(":")][:4]
            code = int(code)
        except ValueError:
            continue
        numbers += [0] * (4 - len(numbers))
        out.append((code, numbers[0], numbers[1], numbers[2], numbers[3]))
    return out, cut


def decode_explain_lines(text):
    """A lines column as ([(attrtype, value)], [seated stone vnums])."""
    lines_part, _, stones_part = str(text or "").partition("|")
    lines = []
    for part in lines_part.split(","):
        kind, sep, value = part.strip().partition(":")
        if not sep:
            continue
        try:
            lines.append((int(kind), int(value)))
        except ValueError:
            continue
    stones = [int(p) for p in (s.strip() for s in stones_part.split(",")) if p.isdigit()]
    return lines, stones


def _dx_lang(language):
    return language if language in LANGS else "en"


def _dx_pick(texts, language):
    """A text of a {pl, en, de, tr} dict, English when the language has none."""
    return texts.get(language) or texts.get("en") or ""


def _dx_int(value):
    """A number the way this panel writes yang: thousands apart with a space."""
    try:
        n = int(value)
    except (TypeError, ValueError):
        return str(value)
    return ("-" if n < 0 else "") + "{:,}".format(abs(n)).replace(",", " ")


def _dx_dec(value, digits, language):
    text = ("%." + str(int(digits)) + "f") % float(value)
    return text if language == "en" else text.replace(".", ",")


# A mob's name for the marbles, read lazily from the world's own mob_proto and
# kept: a counter carries three marbles at most, so this is a handful of reads.
_MOB_NAMES = {}


_MOB_NAME_RETRY = 600


def localized_mob_name(vnum, language=None):
    language = _dx_lang(language or (lang() if has_request_context() else "en"))
    vnum = int(vnum or 0)
    if vnum <= 0:
        return "—"
    known = _MOB_NAMES.get(vnum)
    if ENGINE_MT2009 and (known is None or (not known[0] and time.time() - known[1] > _MOB_NAME_RETRY)):
        name = ""
        try:
            with db() as c, c.cursor() as cur:
                # cp1250 like every name column, and latin1 on the way out: the
                # bytes, decoded here (see _load_item_proto).
                cur.execute("SELECT CAST(locale_name AS BINARY) AS name FROM player.mob_proto WHERE vnum = %s",
                            (vnum,))
                row = cur.fetchone()
            name = log_text(row.get("name")).strip() if row else ""
        except Exception:
            name = ""
        if len(_MOB_NAMES) < 8192:
            _MOB_NAMES[vnum] = (name, time.time())
        known = (name, time.time())
    return (known[0] if known else "") or map_i18n(language)["ex_mob_n"].format(n=vnum)


def _dx_skill(skill_id, language):
    skill_id = int(skill_id or 0)
    if skill_id <= 0:
        return "—"
    name = SKILL_NAMES_BY_LANG.get(language, {}).get(skill_id) or SKILL_ID_NAMES.get(skill_id)
    return name or map_i18n(language)["ex_skill_n"].format(n=skill_id)


# The value slot of a bonus line's text, with what is glued to it: "+%d%%",
# ": %d", "%.1f", "+%dm", "+%d‰".
_DX_APPLY_SLOT = re.compile(r"\s*:?\s*\+?%(?:\.1f|d|s)(?:%%|‰|m\b)?")


def apply_name(attr_type, language):
    """A bonus line's name without its value ("Średnie Obrażenia")."""
    entry = APPLY_TEXTS.get(apply_key(attr_type))
    if not entry:
        return "Bonus #%d" % int(attr_type or 0)
    text = _DX_APPLY_SLOT.sub("", _dx_pick(entry[1], language)).replace("%%", "%")
    return re.sub(r"\s{2,}", " ", text).strip(" :")


def apply_value_text(attr_type, value, language):
    """A bonus line's value in its own unit, for beside apply_name."""
    entry = APPLY_TEXTS.get(apply_key(attr_type))
    fmt = entry[0] if entry else "flat"
    value = int(value or 0)
    if fmt == "boolean":
        return ""
    if fmt == "multiplier":
        return "×" + _dx_dec(value, 1, language)
    if fmt in ("percent", "percent_decimal"):
        return "%s%d%%" % ("+" if value > 0 else "", value)
    return "%s%d" % ("+" if value > 0 else "", value)


def format_apply(attr_type, value, language):
    """A whole bonus line as the tooltip's formatApply writes it."""
    entry = APPLY_TEXTS.get(apply_key(attr_type))
    value = int(value or 0)
    if not entry:
        return "Bonus #%d: %s%d" % (int(attr_type or 0), "+" if value > 0 else "", value)
    fmt, texts = entry
    text = _dx_pick(texts, language)
    if fmt == "boolean":
        return text
    shown = _dx_dec(value, 1, "en") if fmt in ("multiplier", "percent_decimal") else str(value)

    def slot(match):
        if not match.group(0).startswith("+") or value < 0:
            return shown
        return "+" + shown
    return re.sub(r"\+?%(?:\.1f|d|s)", slot, text, count=1).replace("%%", "%").strip()


def _dx_param(kind, raw, params, language):
    """One number of a code, written by its kind:
    int, yang - a count or a price, thousands apart;
    plus - "+7"; level - a bare number; pct, pctsigned, permille;
    x100, x10000 - a multiplier stored as an integer ("×1,04");
    minutes; item, skill, mob - a name; apply - a bonus line's name, and
    applyval:<param> - its value in that line's unit;
    enum:<NAME> - a word of DECISION_ENUMS."""
    try:
        raw = int(raw or 0)
    except (TypeError, ValueError):
        return str(raw)
    if kind in ("int", "yang"):
        return _dx_int(raw)
    if kind == "plus":
        return "+%d" % raw
    if kind == "level":
        return str(raw)
    if kind == "pct":
        return "%d%%" % raw
    if kind == "pctsigned":
        return ("+%d%%" % raw) if raw > 0 else ("%d%%" % raw)
    if kind == "permille":
        return "%d‰" % raw
    if kind == "x100":
        return "×" + _dx_dec(raw / 100.0, 2, language)
    if kind == "x10000":
        return "×" + _dx_dec(raw / 10000.0, 4, language)
    if kind == "minutes":
        return "%d %s" % (raw, _dx_pick(T["unit_min"], language))
    if kind == "item":
        return localized_item_name(raw, language) if raw > 0 else "—"
    if kind == "skill":
        return _dx_skill(raw, language)
    if kind == "mob":
        return localized_mob_name(raw, language)
    if kind == "apply":
        return apply_name(raw, language) if raw else "—"
    if kind.startswith("applyval:"):
        return apply_value_text(params.get(kind[len("applyval:"):], 0), raw, language)
    if kind.startswith("enum:"):
        entry = DECISION_ENUMS.get(kind[len("enum:"):], {}).get(raw)
        return _dx_pick(entry, language) if entry else str(raw)
    return str(raw)


_DX_SLOT = re.compile(r"\{(a|b|c|value)\}")


def _dx_fill(template, kinds, raw, language):
    """A template with {a} {b} {c} {value} written from raw by their kinds."""
    def one(match):
        name = match.group(1)
        return _dx_param(kinds.get(name, "int"), raw.get(name, 0), raw, language)
    return _DX_SLOT.sub(one, template)


def _dx_unknown(code, raw, language):
    """A code this panel has no words for (a newer core's): said as a number
    with its parameters, so a report still carries it."""
    params = ", ".join("%s=%s" % (k, raw[k]) for k in ("value", "a", "b", "c") if raw.get(k))
    return map_i18n(language)["ex_code_n"].format(n=code) + (" (" + params + ")" if params else "")


def decision_label(table, code, language):
    """The words of a code of one of the plain tables."""
    entry = table.get(int(code or 0))
    return _dx_pick(entry, language) if entry else _dx_unknown(code, {}, language)


def decision_goods_text(code, a, b, c, language):
    entry = DECISION_GOODS.get(int(code or 0))
    raw = {"a": a or 0, "b": b or 0, "c": c or 0}
    if not entry:
        return _dx_unknown(code, raw, language)
    kinds, texts = entry
    return _dx_fill(_dx_pick(texts, language), kinds, raw, language)


def decision_origin_text(code, ref, language):
    entry = DECISION_ORIGINS.get(int(code or 0))
    raw = {"a": ref or 0}
    if not entry:
        return _dx_unknown(code, raw, language)
    kinds, texts = entry
    return _dx_fill(_dx_pick(texts, language), kinds, raw, language)


def _dx_change(before, after, language):
    """How a step moved the price: a percentage, or a multiple when it is large."""
    if not before or before == after:
        return "=" if before else ""
    ratio = float(after) / float(before)
    if ratio >= 2.0 or ratio <= 0.5:
        return "×" + _dx_dec(ratio, 2, language)
    change = (ratio - 1.0) * 100.0
    return ("+" if change > 0 else "") + _dx_dec(change, 1, language) + "%"


def decision_step_rows(text, language):
    """A steps column as rows of name, detail, effect on the price, and the
    price after the step; and whether the column was cut."""
    steps, cut = decode_explain_pairs(text)
    rows, running = [], None
    for code, value, a, b, c in steps:
        raw = {"value": value, "a": a, "b": b, "c": c}
        entry = DECISION_STEPS.get(code)
        if entry:
            value_kind, kinds, names, details = entry
            kinds = dict(kinds)
            if value_kind:
                kinds["value"] = value_kind
            label = _dx_pick(names, language)
            detail = _dx_fill(_dx_pick(details, language), kinds, raw, language)
        else:
            # A step of a newer core: its value is most likely the price.
            value_kind, label, detail = None, _dx_unknown(code, {}, language), _dx_unknown(code, raw, language)
        effect = shown = ""
        if value_kind is None:
            shown = _dx_int(value)
            effect = _dx_change(running, value, language) if running is not None else ""
            running = value
        rows.append({"code": code, "name": decision_code_name("STEP", code), "label": label, "detail": detail,
                     "effect": effect, "value": shown})
    return rows, cut


def decision_flags(flags, table, unusual_mask, language, prefix=None):
    """The flag badges of a row, each marked when it is one of the unusual,
    with the header's name of the flag when the table's prefix is given."""
    flags = int(flags or 0)
    out = []
    bit = 1
    while bit <= flags:
        if flags & bit:
            entry = table.get(bit)
            badge = {"code": bit, "label": _dx_pick(entry, language) if entry else "#%d" % bit,
                     "unusual": bool(bit & unusual_mask)}
            if prefix:
                badge["name"] = decision_code_name(prefix, bit)
            out.append(badge)
        bit <<= 1
    return out


def decision_roles(roles, language):
    roles = int(roles or 0)
    out = []
    bit = 1
    while bit <= roles:
        if roles & bit:
            entry = DECISION_ROLES.get(bit)
            out.append(_dx_pick(entry, language) if entry else "#%d" % bit)
        bit <<= 1
    return out


def _dx_time(value, seconds=True):
    if hasattr(value, "strftime"):
        return value.strftime("%d.%m %H:%M:%S" if seconds else "%d.%m %H:%M")
    return str(value or "")


def listing_codes(row):
    """A counter line's row as one line of numbers, for a report."""
    g = lambda k: row.get(k) if row.get(k) is not None else 0
    return ("L1 id=%s pid=%s v=%s n=%s ev=%s sr=%s why=%s:%s:%s:%s sc=%s rk=%s/%s rf=%s cut=%s:%s:%s held=%s "
            "lp=%s ls=%s p=%s was=%s le=%s lst=%s ch=%s off=%s sold=%s f=%s at=%s/%s") % (
        g("item_id"), g("pid"), g("vnum"), g("count"), g("list_event"), g("stand_reason"),
        g("why"), g("why_a"), g("why_b"), g("why_c"), g("score"), g("pick_rank"), g("candidates"),
        g("refused_above"), g("cut_shape"), g("cut_from"), g("cut_keep"), g("held"), g("list_price"),
        row.get("list_steps") or "-", g("price"), g("was"), g("last_event"), row.get("last_steps") or "-",
        g("changes"), g("off_reason"), g("sold_price"), g("flags"),
        _dx_time(row.get("listed_at")).replace(" ", "_"), _dx_time(row.get("last_at")).replace(" ", "_"))


def equip_codes(row):
    """An equipment decision's row as one line of numbers, for a report."""
    g = lambda k: row.get(k) if row.get(k) is not None else 0
    return ("E1 id=%s t=%s pid=%s lv=%s w=%s path=%s rule=%s ctx=%s new=%s:%s:+%s:%s:%s:%s:%s nl=%s nt=%s "
            "old=%s:%s:+%s:%s:%s ol=%s ot=%s sup=%s f=%s") % (
        g("id"), _dx_time(row.get("time")).replace(" ", "_"), g("pid"), g("level"), g("wear"), g("path"),
        g("rule"), g("context"), g("new_id"), g("new_vnum"), g("new_plus"), g("new_score"), g("new_roles"),
        g("new_origin"), g("new_origin_ref"), row.get("new_lines") or "-", row.get("new_terms") or "-",
        g("old_id"), g("old_vnum"), g("old_plus"), g("old_score"), g("old_roles"),
        row.get("old_lines") or "-", row.get("old_terms") or "-", g("suppressed"), g("flags"))


LISTING_CHANGE_EVENTS = frozenset((4, 5, 6, 7))


LISTING_END_EVENTS = frozenset((8, 9, 10))


def explain_listing(row, language, counter_price=None):
    """One counter line's row of log.playerbot_listing, said in the page's
    language: why it is goods and on this counter, how it was picked and cut,
    the price step by step, what happened to it since, its flags, and the raw
    codes for a report. counter_price is what the line asks now, when known."""
    language = _dx_lang(language)
    m = map_i18n(language)
    count = int(row.get("count") or 0)
    result = {"kind": "listing", "item_id": int(row.get("item_id") or 0), "pid": int(row.get("pid") or 0),
              "vnum": int(row.get("vnum") or 0), "count": count}
    goods = decision_goods_text(row.get("why"), row.get("why_a"), row.get("why_b"), row.get("why_c"), language)
    if int(row.get("score") or 0):
        goods += " · " + m["ex_score"].format(score=_dx_int(row.get("score")))
    result["goods"] = goods
    stand = int(row.get("stand_reason") or 0)
    result["stand"] = m["ex_stand"].format(reason=decision_label(DECISION_STAND, stand, language)) if stand else ""
    candidates = int(row.get("candidates") or 0)
    pick = ""
    if candidates:
        # pick_rank is the index among the scored candidates, 0 the first.
        pick = m["ex_pick"].format(rank=int(row.get("pick_rank") or 0) + 1, n=candidates)
        if int(row.get("refused_above") or 0):
            pick += m["ex_pick_refused"].format(k=int(row.get("refused_above") or 0))
    result["pick"] = pick
    cut = ""
    if int(row.get("cut_from") or 0):
        cut = m["ex_cut"].format(shape=decision_label(DECISION_SHAPES, row.get("cut_shape"), language),
                                 whole=_dx_int(row.get("cut_from")), take=_dx_int(count),
                                 keep=_dx_int(row.get("cut_keep")))
    if int(row.get("held") or 0):
        cut += ("; " if cut else "") + m["ex_held"].format(held=_dx_int(row.get("held")))
    result["cut"] = cut
    list_event = int(row.get("list_event") or 0)
    list_price = int(row.get("list_price") or 0)
    result["listed_time"] = _dx_time(row.get("listed_at"))
    result["listed"] = " · ".join(p for p in (
        decision_label(DECISION_EVENTS, list_event, language) if list_event else "",
        result["listed_time"],
        m["ex_list_price"].format(price=_dx_int(list_price)) if list_price else "") if p)
    result["steps"], result["steps_cut"] = decision_step_rows(row.get("list_steps"), language)
    last_event = int(row.get("last_event") or 0)
    result["last_time"] = _dx_time(row.get("last_at"))
    result["last"] = None
    if last_event in LISTING_CHANGE_EVENTS:
        steps, steps_cut = decision_step_rows(row.get("last_steps"), language)
        line = m["ex_was_now"].format(was=_dx_int(row.get("was")), now=_dx_int(row.get("price")))
        if int(row.get("changes") or 0):
            line += " · " + m["ex_changes"].format(n=int(row.get("changes") or 0))
        result["last"] = {"title": decision_label(DECISION_EVENTS, last_event, language) + " · " + result["last_time"],
                          "line": line, "steps": steps, "steps_cut": steps_cut}
    result["end"] = ""
    if last_event in LISTING_END_EVENTS:
        if last_event == 10:
            end = m["ex_end_sold"].format(price=_dx_int(row.get("sold_price")))
        else:
            end = m["ex_end_off"].format(reason=decision_label(DECISION_OFF, row.get("off_reason"), language))
        result["end"] = end + " · " + result["last_time"]
    explained = int(row.get("price") or 0) or list_price
    result["price"] = _dx_int(explained) if explained else ""
    result["price_note"] = ""
    if counter_price is not None and explained and int(counter_price or 0) != explained \
            and last_event not in LISTING_END_EVENTS:
        result["price_note"] = m["ex_price_differs"].format(now=_dx_int(counter_price), explained=_dx_int(explained))
    flags = int(row.get("flags") or 0)
    result["flags"] = decision_flags(flags, DECISION_LFLAGS, DECISION_LISTING_UNUSUAL, language, "LFLAG")
    result["unusual"] = bool(flags & DECISION_LISTING_UNUSUAL)
    result["codes"] = listing_codes(row)
    # For the export: the header's names of what the row says, the anomalies
    # the panel reads out of it, and the unit against the sheet.
    result["names"] = {"goods": decision_code_name("GOODS", row.get("why")),
                       "list_event": decision_code_name("EVENT", list_event) if list_event else "",
                       "last_event": decision_code_name("EVENT", last_event) if last_event else "",
                       "off": decision_code_name("OFF", row.get("off_reason"))
                       if last_event in LISTING_END_EVENTS and last_event != 10 else ""}
    result["anomalies"] = decision_listing_anomalies(row, language)
    ratio = decision_sheet_ratio(row, decode_explain_pairs(row.get("list_steps"))[0])
    result["sheet"] = m["ex_sheet_ratio"].format(unit=_dx_int(ratio["unit"]), sheet=_dx_int(ratio["sheet"]),
                                                 ratio="×" + _dx_dec(ratio["ratio"], 2, language)) if ratio else ""
    return result


def _dx_wear_label(wear, language):
    m = map_i18n(language)
    key = WEAR_SLOT_KEYS.get(int(wear or 0))
    if key:
        return m[WEAR_SLOT_WORDS[key]]
    if int(wear or 0) in WEAR_UNIQUE_SLOTS:
        return m["ex_wear_unique"]
    return m["ex_wear_slot"].format(n=int(wear or 0))


def _dx_piece(row, side, language):
    """The new or the old piece of an equipment row."""
    item_id = int(row.get(side + "_id") or 0)
    vnum = int(row.get(side + "_vnum") or 0)
    if not item_id and not vnum:
        return None
    lines, stones = decode_explain_lines(row.get(side + "_lines"))
    return {"id": item_id, "vnum": vnum,
            "name": localized_item_name(vnum, language) if vnum else "#%d" % item_id,
            "plus": int(row.get(side + "_plus") or 0),
            "score": _dx_int(row.get(side + "_score") or 0),
            "roles": decision_roles(row.get(side + "_roles"), language),
            "lines": [format_apply(kind, value, language) for kind, value in lines],
            "stones": [localized_item_name(v, language) for v in stones]}


def _dx_term_value(value, scale, language):
    if value is None:
        return "—"
    if scale == 1:
        return _dx_int(value)
    return _dx_dec(float(value) / scale, 1, language)


def _dx_term_rows(codes, new_terms, old_terms, ranked, has_old, language):
    """The old/new/delta rows of some terms, the three largest changes among
    the ranked ones marked `top`."""
    rows = []
    for code in codes:
        entry = DECISION_TERMS.get(code)
        scale = entry[0] if entry else 1
        new, old = new_terms.get(code), old_terms.get(code) if has_old else None
        delta = None
        if has_old and (new is not None or old is not None):
            delta = (new or 0) - (old or 0)
        rows.append({"code": code, "name": decision_code_name("TERM", code),
                     "label": _dx_pick(entry[1], language) if entry else _dx_unknown(code, {}, language),
                     "old": _dx_term_value(old, scale, language), "new": _dx_term_value(new, scale, language),
                     "delta": "" if not delta else (("+" if delta > 0 else "") + _dx_term_value(delta, scale, language)),
                     "top": False, "_delta": abs(delta or 0) if code in ranked else 0})
    for row in sorted((r for r in rows if r["_delta"]), key=lambda r: -r["_delta"])[:3]:
        row["top"] = True
    for row in rows:
        del row["_delta"]
    return rows


def explain_equip(row, language):
    """One row of log.playerbot_equip, said in the page's language: which pass
    and rule, when, the two pieces with their roles and lines, where the new
    one came from, the score's terms old against new with the three that moved
    it most, the weapon's blow beside each other, the flags, the raw codes."""
    language = _dx_lang(language)
    m = map_i18n(language)
    new, old = _dx_piece(row, "new", language), _dx_piece(row, "old", language)
    path = decision_label(DECISION_PATHS, row.get("path"), language)
    rule = decision_label(DECISION_RULES, row.get("rule"), language)
    context = int(row.get("context") or 0)
    result = {"kind": "equip", "id": int(row.get("id") or 0), "pid": int(row.get("pid") or 0),
              "time": _dx_time(row.get("time")),
              "title": m["ex_title_equip"] if new else m["ex_title_unequip"],
              "path": path, "rule": rule, "summary": path + " · " + rule,
              "context": decision_label(DECISION_CONTEXTS, context, language) if context else "",
              "slot": m["ex_level_slot"].format(level=int(row.get("level") or 0),
                                                slot=_dx_wear_label(row.get("wear"), language)),
              "origin": decision_origin_text(row.get("new_origin"), row.get("new_origin_ref"), language) if new else "",
              "new": new, "old": old}
    new_score, old_score = int(row.get("new_score") or 0), int(row.get("old_score") or 0)
    result["gain"] = ""
    if new and old and old_score > 0:
        change = (new_score - old_score) * 100.0 / old_score
        result["gain"] = ("+" if change > 0 else "") + _dx_dec(change, 1, language) + "%"
    new_terms = {code: value for code, value, _a, _b, _c in decode_explain_pairs(row.get("new_terms"))[0]}
    old_terms = {code: value for code, value, _a, _b, _c in decode_explain_pairs(row.get("old_terms"))[0]}
    codes = sorted(set(new_terms) | set(old_terms))
    result["terms"] = _dx_term_rows([c for c in codes if c not in DECISION_BLOW_TERMS], new_terms, old_terms,
                                    DECISION_TERM_RANKED, bool(old), language)
    result["blow"] = _dx_term_rows([c for c in codes if c in DECISION_BLOW_TERMS], new_terms, old_terms,
                                   DECISION_BLOW_RANKED, bool(old), language)
    flags = int(row.get("flags") or 0)
    result["flags"] = decision_flags(flags, DECISION_EFLAGS, DECISION_EQUIP_UNUSUAL, language, "EFLAG")
    result["unusual"] = bool(flags & DECISION_EQUIP_UNUSUAL)
    suppressed = int(row.get("suppressed") or 0)
    result["suppressed"] = m["ex_suppressed"].format(n=suppressed) if suppressed else ""
    result["codes"] = equip_codes(row)
    # For the export: the header's names, the level the bot had then, and
    # what the panel reads out of the row beyond the core's flags.
    result["names"] = {"path": decision_code_name("PATH", row.get("path")),
                       "rule": decision_code_name("RULE", row.get("rule")),
                       "context": decision_code_name("CONTEXT", context),
                       "origin": decision_code_name("ORIGIN", row.get("new_origin")) if new else ""}
    for side, piece in (("new", new), ("old", old)):
        if piece:
            roles = int(row.get(side + "_roles") or 0)
            piece["role_names"] = [decision_code_name("ROLE", bit) for bit in (1 << k for k in range(16)) if roles & bit]
    result["level"] = int(row.get("level") or 0)
    result["anomalies"] = decision_equip_anomalies(row, new_terms, old_terms, language)
    return result


def _explain_rows(cur, sql, params):
    """The rows of one of the two explanation tables and whether the table is
    there: "ok", "missing" (MySQL's 1146: a world from before them, or a core
    that has not created them yet) or "error". Never raises - an explanation
    that cannot be read is no explanation, not a page that fails."""
    try:
        cur.execute(sql, params)
        return list(cur.fetchall()), "ok"
    except Exception as e:
        code = e.args[0] if getattr(e, "args", None) else None
        if code == 1146:
            return [], "missing"
        app.logger.warning("explanations: %s", e)
        return [], "error"


def _explain_state(*states):
    """One state for several reads: missing only when every table is."""
    states = [s for s in states if s]
    if not states:
        return "ok"
    if all(s == "missing" for s in states):
        return "missing"
    if "ok" in states:
        return "ok"
    return states[0]


def gear_history_equip_entry(row, language, messages, lang_key):
    """An equipment decision as a line of the gear history, with its why."""
    why = explain_equip(row, language)
    t = row.get("time")
    if why["new"]:
        labels = GEAR_HISTORY_HOWS["PLAYERBOT_EQUIP"][1]
        entry = {"kind": "equip", "label": labels.get(lang_key, labels["en"]), "item": why["new"]["name"],
                 "detail": messages["gh_instead"].format(item=why["old"]["name"]) if why["old"] else ""}
    else:
        entry = {"kind": "unequip", "label": messages["gh_unequipped"],
                 "item": why["old"]["name"] if why["old"] else "", "detail": ""}
    entry.update({"time": _dx_time(t, seconds=False), "why": why, "unusual": why["unusual"]})
    return (t, entry)


def gear_history_listing_entry(row, language, messages):
    """A counter line's last event as a line of the gear history, with its why."""
    why = explain_listing(row, language)
    event = int(row.get("last_event") or 0) or int(row.get("list_event") or 0)
    count = int(row.get("count") or 0)
    vnum = int(row.get("vnum") or 0)
    if event == 10:
        # The label already says "Sold": the line carries what it sold for.
        detail = messages["ex_price"].format(price=_dx_int(row.get("sold_price")))
    elif event in LISTING_END_EVENTS:
        detail = decision_label(DECISION_OFF, row.get("off_reason"), language)
    else:
        detail = messages["ex_price"].format(price=why["price"]) if why["price"] else ""
    t = row.get("last_at") or row.get("listed_at")
    return (t, {"time": _dx_time(t, seconds=False), "kind": "listing",
                "label": decision_label(DECISION_EVENTS, event, language),
                "item": (localized_item_name(vnum, language) if vnum else "#%d" % why["item_id"]) +
                        (" x%d" % count if count > 1 else ""),
                "detail": detail, "why": why, "unusual": why["unusual"]})


def gear_history_decisions(cur, pid, limit, unusual, language, messages, lang_key):
    """The card's decisions tab: the bot's equipment decisions and its counter
    lines by their last event, newest first. Unusual only filters in SQL, so the
    limit counts the rows that are shown."""
    equip_sql = "SELECT * FROM log.playerbot_equip WHERE pid = %s"
    listing_sql = "SELECT * FROM log.playerbot_listing WHERE pid = %s"
    equip_args, listing_args = [pid], [pid]
    if unusual:
        equip_sql += " AND (flags & %s) <> 0"
        listing_sql += " AND (flags & %s) <> 0"
        equip_args.append(DECISION_EQUIP_UNUSUAL)
        listing_args.append(DECISION_LISTING_UNUSUAL)
    # pid_time_idx and pid_last_idx carry these orders; LIMIT keeps it cheap.
    equip_rows, equip_state = _explain_rows(cur, equip_sql + " ORDER BY `time` DESC, id DESC LIMIT %s",
                                            tuple(equip_args + [limit]))
    listing_rows, listing_state = _explain_rows(cur, listing_sql + " ORDER BY last_at DESC LIMIT %s",
                                                tuple(listing_args + [limit]))
    entries = [gear_history_equip_entry(r, language, messages, lang_key) for r in equip_rows]
    entries += [gear_history_listing_entry(r, language, messages) for r in listing_rows]
    entries.sort(key=lambda e: e[0] if hasattr(e[0], "strftime") else datetime.datetime.min, reverse=True)
    return entries[:limit], _explain_state(equip_state, listing_state)


# ---- the bots' decisions, world-wide ------------------------------------------
# Every row the core flagged in the last hours, from both explanation tables
# (log.playerbot_equip by `time`, log.playerbot_listing by `last_at`, each on
# its own index and LIMITed), with the bot's name and a link to its card on
# the live map, each explanation a "?" away and a click from being copied into
# a report. mt2009 alone: r40250's core records nothing, and the page says so.
DECISION_PAGE_HOURS = (1, 6, 24, 72, 168)


DECISION_PAGE_LIMIT = 300


def explain_state_text():
    """What the weights file says of the explanations, in the page's words."""
    if not ENGINE_MT2009:
        return t("dc_state_engine")
    days = read_ai_weights().get("EXPLAIN")
    if days is None:
        # "... 7 days (the default - no EXPLAIN key)." - the note before the stop.
        return t("dc_state_on").format(n=EXPLAIN_DEFAULT_DAYS).rstrip(".") + t("dc_state_default") + "."
    if days == 0:
        return t("dc_state_off")
    return t("dc_state_on").format(n=days)


def decision_page_masks(flag):
    """The page's flag filter as (equipment mask, listing mask): every unusual
    one, any flag at all, or one flag of one table ("e1", "l2048"). The
    anomalies are not flags of the core's and have their own queries."""
    if flag in ("any", "anomaly"):
        return 0xFFFFFFFF, 0xFFFFFFFF
    if flag and flag[0] in ("e", "l") and flag[1:].isdigit():
        bit = int(flag[1:])
        if flag[0] == "e" and bit in DECISION_EFLAGS:
            return bit, 0
        if flag[0] == "l" and bit in DECISION_LFLAGS:
            return 0, bit
    return DECISION_EQUIP_UNUSUAL, DECISION_LISTING_UNUSUAL


# The anomalies are the panel's own reading of a row, so their rows are looked
# for in a wider net and read one by one: every weapon change with two pieces
# in the window, and every spare listed with a tool in the hand.
DECISION_PAGE_SCAN = 3000


def decision_page_bot(cur, text):
    """The page's bot filter - a pid, or a character's exact name - as (pid,
    found); (0, True) when there is none."""
    text = (text or "").strip()
    if not text:
        return 0, True
    if text.isdigit():
        return int(text), True
    try:
        cur.execute("SELECT id FROM player.player WHERE name = %s LIMIT 1", (text,))
        row = cur.fetchone()
    except Exception:
        row = None
    return (int(row["id"]), True) if row and row.get("id") else (0, False)


def decision_page_bots(rows_by_pid, language):
    """What the AI export says of each bot on the page now: its class and path
    and level from player.player, and from the core's status file the
    personality, mood and goal it plays by."""
    live_all = read_playerbot_live_status()
    profession_names = SKILL_GROUP_NAMES_BY_LANG.get(language, SKILL_GROUP_NAMES_EN)
    bots = {}
    for pid, r in rows_by_pid.items():
        bot = {"pid": pid, "name": log_text(r.get("pname")).strip() or "#%d" % pid}
        job = r.get("pjob")
        if job is not None:
            bot["job"] = localized_job_name(job, language)
            bot["path"] = profession_names.get((int(job or 0) % 4, int(r.get("psg") or 0)), "")
        if r.get("plevel") is not None:
            bot["level"] = int(r.get("plevel") or 0)
        live = live_all.get(pid)
        if live:
            labels = playerbot_live_labels(live, language)
            bot.update({"persona": labels["personality"], "mood": labels["mood"], "goal": labels["goal"]})
        bots[str(pid)] = bot
    return bots


