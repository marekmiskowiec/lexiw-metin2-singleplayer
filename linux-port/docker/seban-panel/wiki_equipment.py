"""Lexiw: the wiki's equipment lists - Broń, Biżuteria, Zbroje.

One row per item (its +0), filtered in the page by name, kind and level; a
row opens into every level +0..+9 with its attack or defence, bonuses and
what the blacksmith takes for the next step (/api/wiki/equipment/<vnum>).
Everything is read off item_proto and refine_proto, once per ten minutes.

install_wiki_equipment(module) is called from app.py, after install_wiki.
"""
import re
import time

from flask import abort, jsonify, render_template, url_for

WEAPON_KINDS = {0: "Miecz", 1: "Sztylet", 2: "Łuk", 3: "Dwuręczna", 4: "Dzwon", 5: "Wachlarz"}
JEWEL_KINDS = {3: "Bransoleta", 4: "Buty", 5: "Naszyjnik", 6: "Kolczyki"}
ARMOR_KINDS = {0: "Zbroja", 1: "Hełm", 2: "Tarcza"}
# (route key, title, intro, item_proto type, {subtype: label})
CATEGORIES = {
    "bron": ("Broń", "Wszystkie bronie: miecze, sztylety, łuki, bronie dwuręczne, dzwony i wachlarze. "
             "Tabela pozwala porównać poziom, atak i wbudowane bonusy.", 1, WEAPON_KINDS),
    "bizuteria": ("Biżuteria", "Bransolety, buty, naszyjniki i kolczyki. Tabela służy głównie do porównania bonusów: "
                  "HP, odporności, szybkości i innych.", 2, JEWEL_KINDS),
    "zbroje": ("Zbroje", "Zbroje, hełmy i tarcze. Tabela pozwala porównać poziom, obronę i wbudowane bonusy.", 2, ARMOR_KINDS),
}
# The flat defence the client shows (as item_base_stats): value1 + refine
# bonus x this, only for these kinds.
DEFENCE_MULTIPLIER = {0: 2, 1: 2, 2: 2, 4: 1}
CACHE_SECONDS = 600


def install_wiki_equipment(m):
    app = m.app
    cache = {"at": 0, "rows": {}}

    def load():
        if time.time() - cache["at"] < CACHE_SECONDS and cache["rows"]:
            return cache["rows"]
        rows = m.rows(
            "SELECT i.vnum, i.name, i.locale_name, i.type, i.subtype, i.antiflag, i.limittype0, i.limitvalue0,"
            " i.limittype1, i.limitvalue1, i.value1, i.value2, i.value3, i.value4, i.value5,"
            " i.applytype0, i.applyvalue0, i.applytype1, i.applyvalue1, i.applytype2, i.applyvalue2,"
            " i.refined_vnum, i.refine_set, r.cost, r.prob,"
            " r.vnum0, r.count0, r.vnum1, r.count1, r.vnum2, r.count2, r.vnum3, r.count3, r.vnum4, r.count4"
            " FROM player.item_proto i LEFT JOIN world.refine_proto r ON r.id = i.refine_set"
            " WHERE (i.type = 1 AND i.subtype IN (0,1,2,3,4,5)) OR (i.type = 2 AND i.subtype IN (0,1,2,3,4,5,6))")
        cache["rows"] = {int(r["vnum"]): r for r in rows}
        cache["at"] = time.time()
        return cache["rows"]

    def name_of(row):
        return m.game_text(row.get("locale_name") or row.get("name")) or ""

    def usable_name(name):
        """Names the package never filled ('??', Korean left-overs) are not items of this world."""
        return bool(name) and not name.startswith("?") and re.search(r"[A-Za-zĄĆĘŁŃÓŚŹŻąćęłńóśźż]", name)

    def level_of(row):
        for i in (0, 1):
            if int(row.get(f"limittype{i}") or 0) == 1:
                return int(row.get(f"limitvalue{i}") or 0)
        return 0

    def values(row):
        """Attack, magic attack and defence as the in-game tooltip shows them."""
        v = lambda i: int(row.get(f"value{i}") or 0)
        bonus = v(5)
        out = {"attack": None, "magic": None, "defence": None}
        if int(row["type"]) == 1:
            if v(3) or v(4):
                out["attack"] = (v(3) + bonus, v(4) + bonus)
            if v(1) or v(2):
                out["magic"] = (v(1) + bonus, v(2) + bonus)
        else:
            multiplier = DEFENCE_MULTIPLIER.get(int(row["subtype"]))
            if multiplier and (v(1) + bonus * multiplier):
                out["defence"] = v(1) + bonus * multiplier
        return out

    def bonuses(row):
        out = []
        for i in range(3):
            apply_type, value = row.get(f"applytype{i}"), int(row.get(f"applyvalue{i}") or 0)
            if not apply_type or not value:
                continue
            shown = m.POINT_TO_APPLY.get(int(apply_type), int(apply_type)) if m.ENGINE_MT2009 else int(apply_type)
            if int(row["type"]) == 1 and int(row["subtype"]) == 3 and shown == 7:
                value -= 10  # a two-handed weapon's attack speed, as the client shows it
            out.append(m.apply_text(apply_type, value))
        return out

    def range_text(pair):
        if not pair:
            return None
        return f"{pair[0]} – {pair[1]}" if pair[0] != pair[1] else str(pair[1])

    def chains(item_type, kinds):
        """Every item of the category as a chain of its levels, from the one
        nothing refines into."""
        rows = load()
        mine = {v: r for v, r in rows.items() if int(r["type"]) == item_type and int(r["subtype"]) in kinds}
        # A series ends where the name changes: a +9 that refines into the
        # next item (Bezduszne Noże+9 -> Kolec Pięciu Elementów+0) starts
        # that item's own series.
        def same_series(a, b):
            return base_name(name_of(mine[a])) == base_name(name_of(mine[b]))
        targets = {int(r["refined_vnum"]) for v, r in mine.items()
                   if int(r.get("refined_vnum") or 0) in mine and same_series(v, int(r["refined_vnum"]))}
        out = []
        for vnum in sorted(mine):
            if vnum in targets:
                continue
            chain, seen, current = [], set(), vnum
            while current in mine and current not in seen and len(chain) < 16:
                if chain and not same_series(chain[-1], current):
                    break
                chain.append(current)
                seen.add(current)
                current = int(mine[current].get("refined_vnum") or 0)
            out.append(chain)
        return mine, out

    CLASS_BITS = (("w", "Wojownik", 1 << 2), ("n", "Ninja", 1 << 3), ("s", "Sura", 1 << 4), ("sz", "Szaman", 1 << 5))

    def classes_of(row):
        """The classes that may wear it (antiflag: who may not), as short keys."""
        anti = int(row.get("antiflag") or 0)
        return " ".join(key for key, _label, bit in CLASS_BITS if not anti & bit)

    def base_name(name):
        return re.sub(r"\s*\+\d+$", "", name)

    items_cache = {}

    def list_items(category):
        """The category's items, one per series, as the page lists them."""
        cached = items_cache.get(category)
        if cached and time.time() - cached[0] < CACHE_SECONDS:
            return cached[1]
        title, intro, item_type, kinds = CATEGORIES[category]
        mine, all_chains = chains(item_type, kinds)
        items = []
        # A lone copy of one level of a series (an event "Miecz+5" on its own
        # vnum) is the series' item, already in its row.
        in_series = {name_of(mine[v]) for chain in all_chains if len(chain) > 1 for v in chain}
        for chain in all_chains:
            row = mine[chain[0]]
            name = name_of(row)
            if not usable_name(name) or (len(chain) == 1 and name in in_series):
                continue
            stats = values(row)
            # The package's empty skeletons (no attack, no defence, no bonus).
            if not ((stats["attack"] or stats["magic"]) if item_type == 1 else (stats["defence"] or bonuses(row))):
                continue
            items.append({
                "vnum": chain[0], "name": base_name(name), "full": name, "icon": m.item_icon_url(chain[0]),
                "kind": kinds[int(row["subtype"])], "subtype": int(row["subtype"]), "level": level_of(row),
                "attack": range_text(stats["attack"]), "magic": range_text(stats["magic"]),
                "defence": stats["defence"], "bonuses": bonuses(row), "levels": len(chain), "classes": classes_of(row),
            })
        items.sort(key=lambda i: (i["level"], i["kind"], i["name"]))
        items_cache[category] = (time.time(), items)
        return items

    @app.route("/wiki/<any(bron, bizuteria, zbroje):category>")
    @m.login_required
    def wiki_equipment(category):
        title, intro, item_type, kinds = CATEGORIES[category]
        items = list_items(category)
        return render_template("wiki_equipment.html", category=category, title=title, intro=intro, items=items,
                               kinds=sorted(set((i["subtype"], i["kind"]) for i in items)), classes=CLASS_BITS,
                               weapons=item_type == 1, jewels=category == "bizuteria",
                               categories=[(k, c[0]) for k, c in CATEGORIES.items()])

    # The wiki's side menu (templates/wiki_base.html): categories with how
    # many pages each holds, as the MT2009 wiki has them.
    @app.template_global()
    def wiki_nav():
        from wiki import MAP_FAMILIES
        from wiki_guides import guide_list
        guides = guide_list()
        maps = [("Wszystkie mapy", url_for("wiki_maps"))]
        maps += [(family["name"], url_for("wiki_region", key=key)) for key, family in MAP_FAMILIES.items()]
        family_maps = {n for family in MAP_FAMILIES.values() for n in family["numbers"]}
        try:
            dirs = m.wiki_map_dirs()
        except Exception:
            dirs = {}
        maps += sorted(((name, url_for("wiki_map", number=number)) for number, name in m.MAP_NAMES.items()
                        if number in dirs and number not in family_maps), key=lambda x: x[0])
        return [
            {"icon": "📖", "title": "Poradniki", "items": [(g["title"], url_for("wiki_guide", slug=g["slug"])) for g in guides]},
            {"icon": "🛡️", "title": "Ekwipunek", "items": [
                (f"{CATEGORIES[key][0]} ({len(list_items(key))})", url_for("wiki_equipment", category=key)) for key in CATEGORIES]},
            {"icon": "🗺️", "title": "Mapy", "items": maps},
            {"icon": "🤖", "title": "Boty", "items": [("Osobowości botów", url_for("wiki_bots"))]},
        ]

    @app.get("/api/wiki/equipment/<int:vnum>")
    @m.login_required
    def api_wiki_equipment(vnum):
        """The levels of one item: +0..+9, each with what the next step takes."""
        rows = load()
        if vnum not in rows:
            abort(404)
        start = rows[vnum]
        kinds = WEAPON_KINDS if int(start["type"]) == 1 else {**JEWEL_KINDS, **ARMOR_KINDS}
        mine, all_chains = chains(int(start["type"]), kinds)
        chain = next((c for c in all_chains if vnum in c), [vnum])
        material_vnums = {int(mine[v][f"vnum{i}"]) for v in chain for i in range(5) if mine[v].get(f"vnum{i}")}
        names = {}
        if material_vnums:
            for r in m.rows("SELECT vnum, name, locale_name FROM player.item_proto WHERE vnum IN ("
                            + ",".join(["%s"] * len(material_vnums)) + ")", sorted(material_vnums)):
                names[int(r["vnum"])] = m.game_text(r["locale_name"] or r["name"])
        levels = []
        for v in chain:
            row = mine[v]
            name = name_of(row)
            plus = re.search(r"\+(\d+)$", name)
            stats = values(row)
            step = None
            if int(row.get("refine_set") or 0) and row.get("prob") is not None and int(row.get("refined_vnum") or 0):
                step = {"chance": int(row["prob"]), "cost": int(row["cost"] or 0),
                        "materials": [{"vnum": int(row[f"vnum{i}"]), "count": int(row[f"count{i}"]),
                                       "name": names.get(int(row[f"vnum{i}"]), f"VNUM {row[f'vnum{i}']}"),
                                       "icon": m.item_icon_url(row[f"vnum{i}"]),
                                       "url": url_for("wiki_item", vnum=int(row[f"vnum{i}"]))}
                                      for i in range(5) if row.get(f"vnum{i}")]}
            levels.append({"vnum": v, "plus": int(plus.group(1)) if plus else None, "name": name,
                           "url": url_for("wiki_item", vnum=v), "level": level_of(row),
                           "attack": range_text(stats["attack"]), "magic": range_text(stats["magic"]),
                           "defence": stats["defence"], "bonuses": bonuses(row), "step": step})
        # The last level refines into another item (the next tier): where it
        # leads and what that step takes.
        upgrade = None
        last = mine[chain[-1]]
        nxt = int(last.get("refined_vnum") or 0)
        if nxt and nxt not in chain and nxt in mine and levels and levels[-1]["step"]:
            upgrade = {"name": name_of(mine[nxt]), "url": url_for("wiki_item", vnum=nxt), "icon": m.item_icon_url(nxt),
                       "step": levels[-1]["step"]}
        return jsonify(ok=True, levels=levels, upgrade=upgrade)
