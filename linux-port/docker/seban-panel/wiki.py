"""Lexiw: the server wiki - pages built from this server's own data.

Stage 1: the search over items and monsters, and a page for every item -
what it is, what each refine level gives, where it comes from (which
monsters drop it and how often, which chests hold it, the common and etc
drop tables) and what it costs.

The drop and chest files are the ones the panel's own editors read (the
image's file and the operator's changes in the spool), read the way the
core reads them (drop_parse / chest_parse in app.py) and turned around:
item -> sources. That index is built once and again only when one of the
files changes.

install_wiki(module) is called at the end of app.py with app.py itself, the
way item_grants.py is: the helpers stay where they are, in one place.
"""
import re
from collections import defaultdict

from flask import abort, jsonify, render_template, request

# item_proto.antiflag: which classes / sexes may NOT use an item.
ANTIFLAG_CLASSES = ((1 << 2, "Wojownik"), (1 << 3, "Ninja"), (1 << 4, "Sura"), (1 << 5, "Szaman"))
ANTIFLAG_FEMALE, ANTIFLAG_MALE = 1 << 0, 1 << 1
ITEM_ANTIFLAG_SELL = 1 << 8

TYPE_NAMES = {
    1: "Broń", 2: "Zbroja i biżuteria", 3: "Przedmiot użytkowy", 4: "Przedmiot użytkowy", 5: "Materiał",
    6: "Przedmiot specjalny", 7: "Narzędzie", 8: "Los", 10: "Kamień duchowy", 11: "Pojemnik", 12: "Ryba",
    13: "Wędka", 14: "Surowiec", 16: "Przedmiot unikatowy", 17: "Księga umiejętności", 18: "Przedmiot questowy",
    19: "Polimorfia", 20: "Skrzynia", 21: "Klucz", 23: "Szkatułka", 24: "Kilof", 25: "Fryzura", 28: "Kostium",
    29: "Kamień smoka", 33: "Pierścień", 34: "Pas", 35: "Zwierzak",
}
WEAPON_SUBTYPE_NAMES = {0: "Miecz", 1: "Sztylet", 2: "Łuk", 3: "Broń dwuręczna", 4: "Dzwon", 5: "Wachlarz", 6: "Strzały"}
ARMOR_SUBTYPE_NAMES = {0: "Zbroja", 1: "Hełm", 2: "Tarcza", 3: "Bransoleta", 4: "Buty", 5: "Naszyjnik", 6: "Kolczyki"}
COMMON_RANKS = {0: "zwykłe potwory", 1: "silniejsze potwory", 2: "rycerze", 3: "elitarne potwory"}


def install_wiki(m):
    """m: app.py itself (its app, helpers and file paths)."""
    app = m.app
    index_cache = {"key": None, "index": None}

    # ------------------------------------------------------------ the index

    def _mtime(path):
        try:
            return path.stat().st_mtime
        except OSError:
            return 0

    def _source_key():
        return tuple(_mtime(path) for path in (m.DROP_BASE, m.DROP_CUSTOM, m.DROP_COMMON_BASE, m.DROP_ETC_BASE,
                                               m.CHEST_BASE, m.CHEST_CUSTOM, m.CHEST_SNAPSHOT))

    def drop_index():
        """{vnum: {"mobs": [...], "chests": [...], "common": [...], "etc": [...]}}"""
        key = _source_key()
        if index_cache["key"] == key and index_cache["index"] is not None:
            return index_cache["index"]
        index = defaultdict(lambda: {"mobs": [], "chests": [], "common": [], "etc": []})

        # mob_drop_item.txt: what the engine uses for each monster - the
        # operator's group of a kind, or the image's (drop_mob_groups).
        state = m.drop_state()
        mobs = {grp["mob"] for grp in state["base"]} | {mob for mob, _kind in state["custom"]}
        for mob in mobs:
            for kind, info in m.drop_mob_groups(state, mob).items():
                for group in info["groups"]:
                    weights = sum(max(0.0, m.drop_float(e["prob"])) for e in group["items"]) if kind == "kill" else 0
                    for entry in group["items"]:
                        item = str(entry["item"])
                        if not item.isdigit():
                            continue  # "s<vnum>": a draw from a chest group, not an item
                        index[int(item)]["mobs"].append({
                            "mob": mob, "kind": kind, "count": entry["count"],
                            "chance": m.drop_chance(kind, entry, group, weights),
                            "kill_drop": group.get("kill_drop"), "level_limit": group.get("level_limit")})

        # special_item_group.txt: the chests (and other groups) holding it.
        chests = m.chest_effective(m.chest_state())
        for chest_vnum, group in chests.items():
            weights = sum(max(0, int(e["prob"])) for e in group["items"] if str(e["prob"]).lstrip("-").isdigit())
            for entry in group["items"]:
                item = str(entry["item"])
                if not item.isdigit():
                    continue
                prob = int(entry["prob"]) if str(entry["prob"]).lstrip("-").isdigit() else 0
                if group["type"] == "pct":
                    chance = min(100.0, max(0, prob))
                elif group["type"] == "":
                    chance = (100.0 * prob / weights) if weights and prob > 0 else 0.0
                else:
                    chance = None  # quest / special / attr groups: no plain chance
                index[int(item)]["chests"].append({"chest": chest_vnum, "type": group["type"],
                                                   "count": entry["count"], "chance": chance})

        # common_drop_item.txt: by monster rank and level range, six cells a rank.
        text = m.drop_read(m.DROP_COMMON_BASE) or ""
        for line in text.replace("\r", "").split("\n")[1:]:
            cells = line.split("\t")
            for rank in COMMON_RANKS:
                part = cells[rank * 6: rank * 6 + 6]
                if len(part) < 5 or not part[4].strip().isdigit() or int(part[4]) <= 1:
                    continue
                if not part[1].strip().isdigit() or part[1].strip() == "0":
                    continue
                index[int(part[4])]["common"].append({"rank": rank, "levels": f"{part[1].strip()}–{part[2].strip()}",
                                                      "chance": m.drop_float(part[3]) / 4})

        # etc_drop_item.txt: an item, and the chance of the monsters whose
        # mob_proto.drop_item names it.
        text = m.drop_read(m.DROP_ETC_BASE) or ""
        etc = {}
        for line in text.replace("\r", "").split("\n"):
            parts = line.split("\t")
            if len(parts) >= 2 and parts[0].strip().isdigit():
                etc[int(parts[0].strip())] = m.drop_float(parts[-1]) / 4
        if etc:
            for mob in m.drop_mob_rows().values():
                if mob["drop_item"] in etc:
                    index[mob["drop_item"]]["etc"].append({"mob": mob["vnum"], "chance": etc[mob["drop_item"]]})

        index = dict(index)
        index_cache.update(key=key, index=index)
        return index

    # -------------------------------------------------------------- the item

    def proto_row(vnum):
        return m.one("SELECT * FROM player.item_proto WHERE vnum=%s", (vnum,))

    def type_label(row):
        item_type, subtype = int(row.get("type") or 0), int(row.get("subtype") or 0)
        if item_type == 1:
            return "Broń · " + WEAPON_SUBTYPE_NAMES.get(subtype, f"rodzaj {subtype}")
        if item_type == 2:
            return ARMOR_SUBTYPE_NAMES.get(subtype, "Zbroja i biżuteria")
        return TYPE_NAMES.get(item_type, f"Typ {item_type}")

    def classes_of(row):
        anti = int(row.get("antiflag") or 0)
        allowed = [name for bit, name in ANTIFLAG_CLASSES if not anti & bit]
        sex = "tylko kobiety" if anti & ANTIFLAG_MALE else "tylko mężczyźni" if anti & ANTIFLAG_FEMALE else ""
        return allowed, sex

    def native_bonuses(row):
        """The item's own bonuses, as the player page's tooltip shows them."""
        item_type, subtype = int(row.get("type") or 0), int(row.get("subtype") or 0)
        out = []
        for i in range(3):
            apply_type, value = row.get(f"applytype{i}"), int(row.get(f"applyvalue{i}") or 0)
            if not apply_type or not value:
                continue
            shown = m.POINT_TO_APPLY.get(int(apply_type), int(apply_type)) if m.ENGINE_MT2009 else int(apply_type)
            if item_type == 1 and subtype == 3 and shown == 7:
                value -= 10  # a two-handed weapon's attack speed, as the client shows it
            out.append(m.apply_text(apply_type, value))
        return out

    def refine_chain(row):
        """Every level of the item: back through the items that refine into
        it, forward through refined_vnum - each level its own vnum."""
        vnum = int(row["vnum"])
        chain, seen, current = [vnum], {vnum}, vnum
        while True:
            back = m.one("SELECT vnum FROM player.item_proto WHERE refined_vnum=%s ORDER BY vnum LIMIT 1", (current,))
            if not back or int(back["vnum"]) in seen:
                break
            current = int(back["vnum"])
            chain.insert(0, current)
            seen.add(current)
        current = int(row.get("refined_vnum") or 0)
        while current and current not in seen and len(chain) < 30:
            chain.append(current)
            seen.add(current)
            nxt = m.one("SELECT refined_vnum FROM player.item_proto WHERE vnum=%s", (current,))
            current = int((nxt or {}).get("refined_vnum") or 0)
        if len(chain) < 2:
            return []
        marks = ",".join(["%s"] * len(chain))
        names = {int(r["vnum"]): m.game_text(r["locale_name"] or r["name"])
                 for r in m.rows(f"SELECT vnum,name,locale_name FROM player.item_proto WHERE vnum IN ({marks})", chain)}
        return [{"vnum": v, "name": names.get(v, f"VNUM {v}"), "icon": m.item_icon_url(v), "current": v == vnum,
                 "stats": [s for s in m.item_base_stats(v) if not s.startswith("Wymagany poziom:")]} for v in chain]

    def npc_sale(row):
        """What an NPC pays for one, as CShopManager::Sell counts it: the
        shop_buy_price column, divided by 5 (ENABLE_NO_SELL_PRICE_DIVIDED_BY_5
        is off in this build) but for gold bars, less a tax of 3-20% by price,
        more for items of a high level. None: the item cannot be sold."""
        if int(row.get("antiflag") or 0) & ITEM_ANTIFLAG_SELL:
            return None
        price, vnum = int(row.get("shop_buy_price") or 0), int(row["vnum"])
        gold_bar = 80003 <= vnum <= 80008
        if not gold_bar:
            price //= 5
        tax = 20 if price >= 100000 else 10 if price >= 30000 else 8 if price >= 10000 else 5 if price >= 3000 else 3
        if gold_bar:
            tax = 3
        level = max([int(row.get(f"limitvalue{i}") or 0) for i in range(2) if int(row.get(f"limittype{i}") or 0) == 1] or [0])
        tax += 8 if level > 60 else 5 if level > 50 else 3 if level > 40 else 0
        return {"price": price - price * tax // 100, "tax": tax}

    def market_price(vnum):
        """The offline shops now, as the market page counts it (the
        collector's latest snapshot)."""
        try:
            latest = m.one("SELECT MAX(captured_at) AS at FROM player.web_seban_shop_item_snapshot").get("at")
            return m.shop_item_market_row(vnum, 0, latest)
        except m.pymysql.MySQLError:
            return {}

    def sources_of(vnum):
        found = drop_index().get(vnum) or {"mobs": [], "chests": [], "common": [], "etc": []}
        mob_rows = m.drop_mob_rows()
        mobs = []
        for entry in found["mobs"]:
            mob = mob_rows.get(entry["mob"])
            if not mob:
                continue
            mobs.append(dict(entry, name=mob["name"], level=mob["level"], kind_name=mob["kind_name"], mob_kind=mob["kind"],
                             kind_label=m.DROP_TYPES.get(entry["kind"], entry["kind"])))
        mobs.sort(key=lambda e: (-(e["chance"] or 0), e["level"]))
        etc = []
        for entry in found["etc"]:
            mob = mob_rows.get(entry["mob"])
            if mob:
                etc.append(dict(entry, name=mob["name"], level=mob["level"], kind_name=mob["kind_name"]))
        etc.sort(key=lambda e: e["level"])
        chest_names = m.chest_item_names([e["chest"] for e in found["chests"]])
        chests = [dict(e, name=(chest_names.get(e["chest"]) or {}).get("name") or f"Grupa {e['chest']}",
                       icon=m.item_icon_url(e["chest"]), type_label=m.CHEST_TYPES.get(e["type"], e["type"]))
                  for e in found["chests"]]
        chests.sort(key=lambda e: -(e["chance"] or 0))
        common = [dict(e, rank_label=COMMON_RANKS[e["rank"]]) for e in found["common"]]
        return {"mobs": mobs, "chests": chests, "common": common, "etc": etc}

    # ----------------------------------------------------------------- pages

    @app.route("/wiki")
    @m.login_required
    def wiki():
        index = drop_index()
        return render_template("wiki.html", query=request.args.get("q", "").strip(),
                               item_count=m.one("SELECT COUNT(*) AS n FROM player.item_proto").get("n", 0),
                               mob_count=len(m.drop_mob_rows()), dropped_items=len(index))

    @app.get("/api/wiki/search")
    @m.login_required
    def api_wiki_search():
        query = request.args.get("q", "").strip()
        if len(query) < 2 and not query.isdigit():
            return jsonify(ok=True, items=[], mobs=[])
        clause, params = m.item_name_search(query)
        items = m.rows("SELECT p.vnum,p.name,p.locale_name,p.type,p.subtype FROM player.item_proto p WHERE "
                       + clause + " ORDER BY (p.vnum=%s) DESC, CHAR_LENGTH(p.locale_name), p.vnum LIMIT 30",
                       params + [int(query) if query.isdigit() else -1])
        index = drop_index()
        found_items = [{"vnum": int(r["vnum"]), "name": m.game_text(r["locale_name"] or r["name"]),
                        "type": type_label(r), "icon": m.item_icon_url(r["vnum"]),
                        "sources": bool(index.get(int(r["vnum"])))} for r in items]
        # What drops or sits in a chest first - what is looked up most;
        # sorted() keeps the query's order (an exact VNUM, then the
        # shortest names) inside each half.
        found_items = sorted(found_items, key=lambda item: not item["sources"])
        folded = query.lower()
        mobs = [mob for mob in m.drop_mob_rows().values()
                if mob["kind"] in ("mob", "boss", "metin") and (folded in mob["name"].lower() or str(mob["vnum"]) == query)]
        mobs.sort(key=lambda mob: (len(mob["name"]), mob["level"]))
        found_mobs = [{"vnum": mob["vnum"], "name": mob["name"], "level": mob["level"], "kind": mob["kind_name"]}
                      for mob in mobs[:12]]
        return jsonify(ok=True, items=found_items, mobs=found_mobs)

    @app.route("/wiki/item/<int:vnum>")
    @m.login_required
    def wiki_item(vnum):
        row = proto_row(vnum)
        if not row:
            abort(404)
        name = m.game_text(row.get("locale_name") or row.get("name"))
        allowed, sex = classes_of(row)
        level = re.search(r"\+(\d+)$", name)
        return render_template(
            "wiki_item.html", item=row, name=name, icon=m.item_icon_url(vnum), type_label=type_label(row),
            stats=m.item_base_stats(vnum), bonuses=native_bonuses(row),
            classes=allowed, sex=sex, refine_level=int(level.group(1)) if level else None,
            chain=refine_chain(row), sources=sources_of(vnum), market=market_price(vnum), npc_sale=npc_sale(row),
            DROP_TYPES=m.DROP_TYPES)
