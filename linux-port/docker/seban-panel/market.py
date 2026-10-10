"""Lexiw: Giełda - every offer on the offline shops' counters as one list.

What a bot or a person has put up for sale in an IkarusShop stand, with the
categories, filters and sorting of a market tracker: by name, class, level,
refine plus, a bonus and its value, a price per piece. Each offer is compared
with the median price of the same item across the whole market (a "deal" is
one clearly under it) and has an age: when the collector first saw it at its
current price.

The page reads player.web_seban_offer_flat, a flat indexed copy of the offers
that the collector rebuilds every five minutes (collector.py). It never joins
the game's own tables, so it answers in milliseconds - and shows the market as
of the last snapshot, like the Rynek page does.

install_market(module) is called from app.py the way install_wiki is.
"""
import re
from datetime import datetime

import pymysql
from flask import render_template, request

PER_PAGE = 50

# (key, label, SQL over the flat table's f). "skillbook" and "other" are filled
# in install_market, which knows the book vnums.
GROUPS = [
    ("Broń", [
        ("sword", "Miecze", "f.ptype=1 AND f.psubtype=0"),
        ("dagger", "Sztylety", "f.ptype=1 AND f.psubtype=1"),
        ("bow", "Łuki", "f.ptype=1 AND f.psubtype=2"),
        ("twohand", "Broń dwuręczna", "f.ptype=1 AND f.psubtype=3"),
        ("bell", "Dzwony", "f.ptype=1 AND f.psubtype=4"),
        ("fan", "Wachlarze", "f.ptype=1 AND f.psubtype=5"),
        ("arrow", "Strzały", "f.ptype=1 AND f.psubtype=6"),
    ]),
    ("Pancerze i biżuteria", [
        ("body", "Zbroje", "f.ptype=2 AND f.psubtype=0"),
        ("helmet", "Hełmy", "f.ptype=2 AND f.psubtype=1"),
        ("shield", "Tarcze", "f.ptype=2 AND f.psubtype=2"),
        ("bracelet", "Bransolety", "f.ptype=2 AND f.psubtype=3"),
        ("shoes", "Buty", "f.ptype=2 AND f.psubtype=4"),
        ("necklace", "Naszyjniki", "f.ptype=2 AND f.psubtype=5"),
        ("earrings", "Kolczyki", "f.ptype=2 AND f.psubtype=6"),
        ("ring", "Pierścienie i pasy", "f.ptype IN (33,34)"),
    ]),
    ("Pozostałe", [
        ("stone", "Kamienie duchowe", "f.ptype=10"),
        ("skillbook", "Księgi umiejętności", None),
        ("use", "Przedmioty użytkowe", "f.ptype IN (3,4)"),
        ("material", "Materiały i surowce", "f.ptype IN (5,14)"),
        ("costume", "Kostiumy", "f.ptype=28"),
        ("chest", "Skrzynie i szkatułki", "f.ptype IN (11,20,23)"),
        ("fish", "Ryby i wędki", "f.ptype IN (12,13)"),
        ("other", "Inne", None),
    ]),
]

CLASSES = {"woj": (1 << 2, "Wojownik"), "ninja": (1 << 3, "Ninja"), "sura": (1 << 4, "Sura"), "szaman": (1 << 5, "Szaman")}

SORTS = {
    "cheap": ("Cena za sztukę ↑", "f.unit ASC"),
    "dear": ("Cena za sztukę ↓", "f.unit DESC"),
    "stack": ("Cena stosu ↓", "f.price DESC"),
    "new": ("Najnowsze", "f.first_seen DESC"),
    "old": ("Najdłużej wystawione", "f.first_seen ASC"),
}


def median(values):
    values = sorted(values)
    if not values:
        return 0
    middle = len(values) // 2
    return values[middle] if len(values) % 2 else (values[middle - 1] + values[middle]) / 2


# A bonus's colour by what it does, from the words of its label. First match wins.
BONUS_KINDS = (
    ("defence", ("Odporność", "Odbicie", "blok", "unik", "Pochłanianie", "Wartość obrony", "obrażenia niższe")),
    ("attack", ("Silny przeciw", "Obrażenia umiejętności", "Średnie obrażenia", "Wartość ataku", "Magiczny atak",
                "cios krytyczny", "przeszywający", "Szansa na otrucie", "omdlenie", "spowolnienie", "Zasięg")),
    ("vital", ("Maks.", "Regeneracja", "Kradzież", "Witalność", "Siła", "Zręczność", "Inteligencja", "PŻ", "PM", "PE")),
    ("utility", ("Szybkość", "Bonus", "Czas trwania", "Szansa na zdobycie", "Energia")),
)


def bonus_kind(text):
    for kind, words in BONUS_KINDS:
        if any(word in text for word in words):
            return kind
    return "other"


def age_text(first_seen, now):
    if not first_seen:
        return "—"
    seconds = max(0, int((now - first_seen).total_seconds()))
    if seconds < 3600:
        return f"{max(1, seconds // 60)} min"
    if seconds < 86400:
        return f"{seconds // 3600} godz."
    days = seconds // 86400
    return f"{days} {'dzień' if days == 1 else 'dni'}"


def install_market(m):
    app = m.app
    book_marks = ",".join(str(int(v)) for v in sorted(m.SKILLBOOK_VNUMS))
    categories = {}
    for _title, items in GROUPS:
        for key, label, sql in items:
            categories[key] = [label, sql]
    categories["skillbook"][1] = f"f.vnum IN ({book_marks})"
    named = [f"({sql})" for key, (_label, sql) in categories.items() if key != "other"]
    categories["other"][1] = "NOT (" + " OR ".join(named) + ")"
    FLAT = "player.web_seban_offer_flat f"

    def number(name, low=0, high=2_000_000_000_000):
        try:
            return max(low, min(high, int(float(request.args.get(name, "").replace(" ", "").replace(",", ".")))))
        except ValueError:
            return None

    def filters():
        """The where clause and parameters of the current filters, and what
        was asked, as the template shows it."""
        where, params, asked = ["1=1"], [], {}
        category = request.args.get("cat", "")
        if category in categories:
            where.append("(" + categories[category][1] + ")")
        asked["cat"] = category if category in categories else ""
        query = request.args.get("q", "").strip()
        asked["q"] = query
        if query:
            where.append("(f.name LIKE %s OR f.pname LIKE %s" + (" OR f.vnum=%s" if query.isdigit() else "") + ")")
            params += [f"%{query}%", f"%{query}%"] + ([int(query)] if query.isdigit() else [])
        job = request.args.get("job", "")
        asked["job"] = job if job in CLASSES else ""
        if job in CLASSES:
            where.append("(f.antiflag & %s) = 0")
            params.append(CLASSES[job][0])
        for field, op, column in (("lmin", ">=", "f.level"), ("lmax", "<=", "f.level"),
                                  ("pmin", ">=", "f.plus"), ("pmax", "<=", "f.plus"),
                                  ("cmin", ">=", "f.unit"), ("cmax", "<=", "f.unit")):
            value = number(field, 0, 255 if field[0] in "lp" else 2_000_000_000_000)
            asked[field] = value
            if value is not None:
                where.append(f"{column} {op} %s")
                params.append(value)
        bonus = number("bonus", 0, 1000)
        asked["bonus"] = bonus
        bonus_min = number("bmin", -1000, 1000)
        asked["bmin"] = bonus_min
        if bonus:
            clauses = []
            for slot in range(7):
                clause = f"f.attrtype{slot} = %s"
                params.append(bonus)
                if bonus_min is not None:
                    clause += f" AND f.attrvalue{slot} >= %s"
                    params.append(bonus_min)
                clauses.append("(" + clause + ")")
            where.append("(" + " OR ".join(clauses) + ")")
        sort = request.args.get("sort", "cheap")
        asked["sort"] = sort if sort in SORTS else "cheap"
        return " AND ".join(where), params, asked

    def bonus_choices():
        """Bonuses worth filtering by: the ones that offers carry, named as
        the tooltip names them."""
        seen = set()
        for slot in range(7):
            for r in m.rows(f"SELECT DISTINCT attrtype{slot} AS t FROM {FLAT} WHERE attrtype{slot} > 0"):
                seen.add(int(r["t"]))
        names = []
        for t in sorted(seen):
            key = m.POINT_TO_APPLY.get(t, t) if m.ENGINE_MT2009 else t
            names.append({"id": t, "label": m.APPLY_LABELS.get(key, (f"Bonus #{t}", ""))[0]})
        return sorted(names, key=lambda n: n["label"])

    def category_counts():
        """Offers per category, for the numbers beside the menu: one pass,
        a SUM for every category."""
        keys = list(categories)
        sums = ", ".join(f"COALESCE(SUM({categories[key][1]}), 0) AS c{n}" for n, key in enumerate(keys))
        row = m.one(f"SELECT {sums} FROM {FLAT}")
        return {key: int(row.get(f"c{n}") or 0) for n, key in enumerate(keys)}

    def overview():
        """The whole market in numbers."""
        total = m.one(f"SELECT COUNT(*) AS offers, COUNT(DISTINCT f.owner_id) AS shops, COALESCE(SUM(f.price),0) AS value, "
                      f"COALESCE(SUM(f.cnt),0) AS pieces, MAX(f.first_seen) AS newest FROM {FLAT}")
        common = m.rows(f"SELECT f.vnum, f.name, COUNT(*) AS offers, MIN(f.unit) AS lowest FROM {FLAT} "
                        f"WHERE f.ptype IN (1,2) GROUP BY f.vnum, f.name ORDER BY offers DESC LIMIT 8")
        dearest = m.rows(f"SELECT f.vnum, f.name, f.price, f.seller, f.owner_id FROM {FLAT} ORDER BY f.price DESC LIMIT 8")
        for row in list(common) + list(dearest):
            row["name"] = m.game_text(row["name"])
        for row in list(common) + list(dearest):
            row["name"] = m.game_text(row["name"])
        snapshot = m.one("SELECT MAX(captured_at) AS at FROM player.web_seban_shop_snapshot").get("at")
        return {"offers": int(total["offers"] or 0), "shops": int(total["shops"] or 0), "value": int(total["value"] or 0),
                "pieces": int(total["pieces"] or 0), "common": common, "dearest": dearest, "snapshot": snapshot}

    @app.route("/market")
    @m.login_required
    def market():
        try:
            return render_market()
        except pymysql.MySQLError:
            # The collector has not made its first snapshot yet.
            app.logger.exception("Giełda: the offer table is not ready")
            return render_template("market.html", ready=False)

    def render_market():
        where, params, asked = filters()
        page = max(1, number("page", 1, 100000) or 1)
        total = int(m.one(f"SELECT COUNT(*) AS n FROM {FLAT} WHERE {where}", params).get("n") or 0)
        total_pages = max(1, -(-total // PER_PAGE))
        page = min(page, total_pages)
        order = SORTS[asked["sort"]][1]
        offers = list(m.rows(f"""SELECT f.item_id AS id, f.vnum, f.cnt AS count, f.price, f.unit, f.owner_id, f.seller,
            f.shop_name, f.shop_map, f.first_seen, f.name AS item_name, f.socket0, f.socket1, f.socket2,
            f.attrtype0,f.attrvalue0,f.attrtype1,f.attrvalue1,f.attrtype2,f.attrvalue2,f.attrtype3,f.attrvalue3,
            f.attrtype4,f.attrvalue4,f.attrtype5,f.attrvalue5,f.attrtype6,f.attrvalue6,
            p.applytype0,p.applyvalue0,p.applytype1,p.applyvalue1,p.applytype2,p.applyvalue2,p.size AS item_size,p.type AS item_type
          FROM {FLAT} LEFT JOIN player.item_proto p ON p.vnum = f.vnum
          WHERE {where} ORDER BY {order}, f.item_id LIMIT %s OFFSET %s""", params + [PER_PAGE, (page - 1) * PER_PAGE]))
        m._enrich_items(offers)
        for offer in offers:
            for bonus in offer["bonuses"]:
                bonus["kind"] = bonus_kind(bonus["text"])
                bonus["negative"] = bool(re.search(r"[−-]\d", bonus["text"]))
        # The market's median per piece of every item on this page: what
        # "cheap" and "dear" mean for it.
        medians = {}
        vnums = sorted({int(o["vnum"]) for o in offers})
        if vnums:
            marks = ",".join(["%s"] * len(vnums))
            by_key = {}
            for r in m.rows(f"SELECT f.vnum, IF(f.vnum IN ({book_marks}), f.socket0, 0) AS variant, f.unit FROM {FLAT} "
                            f"WHERE f.vnum IN ({marks})", vnums):
                by_key.setdefault((int(r["vnum"]), int(r["variant"])), []).append(float(r["unit"]))
            medians = {key: (median(values), len(values)) for key, values in by_key.items()}
        now = datetime.now()
        for offer in offers:
            unit = float(offer["unit"])
            variant = int(offer["socket0"] or 0) if int(offer["vnum"]) in m.SKILLBOOK_VNUMS else 0
            mid, seen = medians.get((int(offer["vnum"]), variant), (0, 0))
            offer.update(unit=unit, count=int(offer["count"]), price=int(offer["price"]),
                         icon_url=m.item_icon_url(offer["vnum"]), age=age_text(offer["first_seen"], now),
                         median=mid, same_offers=seen, map_name=m.map_name(offer["shop_map"]),
                         shop_name=m.game_text(offer["shop_name"]) or "",
                         # Only a comparison among several offers means something.
                         diff=round((unit - mid) * 100 / mid) if mid and seen >= 3 else None)
        info = shared()
        args = {k: v for k, v in request.args.items() if k != "page" and v not in ("", None)}
        window = [n for n in range(1, total_pages + 1) if n in (1, total_pages) or abs(n - page) <= 2]
        numbers, last = [], 0
        for n in window:
            if last and n - last > 1:
                numbers.append(None)
            numbers.append(n)
            last = n
        return render_template("market.html", ready=True, offers=offers, total=total, page=page, total_pages=total_pages,
                               page_numbers=numbers, first=(page - 1) * PER_PAGE + 1 if total else 0, last=min(total, page * PER_PAGE),
                               asked=asked, args=args, groups=GROUPS, counts=info["counts"], overview=info["overview"],
                               classes=CLASSES, sorts=SORTS, bonuses=info["bonuses"], per_page=PER_PAGE)

    # The menu's counts, the market's totals and the bonuses to filter by only
    # change when the collector rebuilds the table, so they are kept for a
    # minute rather than worked out for every page.
    _shared = {"overview": None, "counts": None, "bonuses": None, "at": 0.0}

    def shared():
        import time
        if _shared["overview"] is None or time.time() - _shared["at"] > 60:
            _shared.update(overview=overview(), counts=category_counts(), bonuses=bonus_choices(), at=time.time())
        return _shared
