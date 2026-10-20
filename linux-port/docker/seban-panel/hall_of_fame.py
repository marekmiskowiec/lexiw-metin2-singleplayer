"""Lexiw: Sala chwały - the world's record holders on one page.

Ten best of every board, all-time:
  * level-30 weapons by average damage and by skill damage - the same items
    and order as /rankings?type=weapon30 (bot_ranking in app.py);
  * the engine's own per-character counters, player.player_special_flag
    (CHARACTER::AddPlayerStat - what the game's "Statystyki" window shows):
    monsters, Metins, bosses, mini-bosses, fish, ore, herbs, refines, the
    biggest hits, yang earned and duels won.

Who counts is the rankings' rule (ranking_scope_sql): everybody but game
masters. install_hall_of_fame(module) is called from app.py.
"""
import time

import pymysql
from flask import render_template

# (title, icon, [(flag, card title, unit, /rankings kind with the same numbers or None)])
BOARDS = (
    ("Pogromcy", "⚔️", (
        ("stat_monster", "Zabite potwory", "potworów", "monsters_killed"),
        ("stat_stone", "Zniszczone Metiny", "Metinów", None),
        ("stat_boss", "Pokonani bossowie", "bossów", None),
        ("stat_miniboss", "Pokonani minibossowie", "minibossów", "minibosses"),
    )),
    ("Rzemiosło", "🛠️", (
        ("stat_fishing", "Złowione ryby", "ryb", None),
        ("stat_mining", "Wykopana ruda", "rud", "mining"),
        ("stat_herbalism", "Zebrane zioła", "ziół", None),
        ("stat_refine_success", "Udane ulepszenia", "ulepszeń", None),
    )),
    ("Rekordy", "🏆", (
        ("stat_damage", "Najmocniejszy cios", "obrażeń", "damage_max"),
        ("stat_damage_skill", "Najmocniejsza umiejętność", "obrażeń", "damage_max_skill"),
        ("stat_damage_horse", "Najmocniejszy cios konno", "obrażeń", "damage_max_horse"),
        ("stat_gold", "Najwięcej zdobytego yang", "yang", "yang_earned"),
        ("stat_duel", "Wygrane pojedynki", "pojedynków", "duel_wins"),
    )),
)
TOP = 10


def install_hall_of_fame(m):
    app = m.app
    cache = {"at": 0.0, "data": None}

    def item_tips(ids):
        """{item id: item} with what the shared tooltip macro shows - the same
        columns and _enrich_items pass the Giełda uses for a stall's offers."""
        if not ids:
            return {}
        marks = ",".join(["%s"] * len(ids))
        items = list(m.rows(f"""SELECT i.id, i.vnum, i.count, i.socket0, i.socket1, i.socket2,
              i.attrtype0,i.attrvalue0,i.attrtype1,i.attrvalue1,i.attrtype2,i.attrvalue2,i.attrtype3,i.attrvalue3,
              i.attrtype4,i.attrvalue4,i.attrtype5,i.attrvalue5,i.attrtype6,i.attrvalue6,
              COALESCE(p.locale_name, p.name) AS item_name,
              p.applytype0,p.applyvalue0,p.applytype1,p.applyvalue1,p.applytype2,p.applyvalue2,p.size AS item_size,p.type AS item_type
            FROM player.item i LEFT JOIN player.item_proto p ON p.vnum = i.vnum WHERE i.id IN ({marks})""", list(ids)))
        m._enrich_items(items)
        return {int(item["id"]): item for item in items}

    def weapon_board(sort_by, key):
        board = []
        top = m.bot_ranking("weapon30", sort_by)[:TOP]
        tips = item_tips([int(r["item_id"]) for r in top if r.get("item_id")])
        for row in top:
            vnum = int(row["vnum"])
            board.append({"id": int(row["id"]), "name": row["name"], "level": int(row["level"]),
                          "item": m.game_text(row["item_name"]), "vnum": vnum, "plus": vnum % 10,
                          "icon": m.item_icon_url(vnum), "shop": bool(int(row.get("is_shop") or 0)),
                          "tip": tips.get(int(row.get("item_id") or 0)),
                          "value": int(row.get(key) or 0),
                          "other": int(row.get("skill_damage" if key == "avg_damage" else "avg_damage") or 0)})
        return board

    def flag_boards():
        flags = [flag for _title, _icon, cards in BOARDS for flag, *_rest in cards]
        marks = ",".join(["%s"] * len(flags))
        base = m.ranking_scope_sql("p", False)
        found = m.rows(f"""SELECT t.flag, t.id, t.name, t.level, t.value FROM (
              SELECT f.flag, p.id, p.name, p.level, f.value,
                     ROW_NUMBER() OVER (PARTITION BY f.flag ORDER BY f.value DESC, p.level DESC, p.name) AS rn
              FROM player.player_special_flag f JOIN player.player p ON p.id=f.pid
              WHERE {base} AND f.flag IN ({marks}) AND f.value > 0) t
            WHERE t.rn <= {TOP} ORDER BY t.flag, t.rn""", flags)
        totals = {r["flag"]: (int(r["n"] or 0), int(r["total"] or 0)) for r in m.rows(
            f"""SELECT f.flag, COUNT(*) AS n, SUM(f.value) AS total FROM player.player_special_flag f
                JOIN player.player p ON p.id=f.pid WHERE {base} AND f.flag IN ({marks}) AND f.value > 0
                GROUP BY f.flag""", flags)}
        by_flag = {}
        for r in found:
            by_flag.setdefault(r["flag"], []).append({"id": int(r["id"]), "name": r["name"], "level": int(r["level"]),
                                                      "value": int(r["value"])})
        boards = []
        for title, icon, cards in BOARDS:
            boards.append({"title": title, "icon": icon, "cards": [
                {"title": card, "unit": unit, "kind": kind, "rows": by_flag.get(flag, []),
                 "people": totals.get(flag, (0, 0))[0], "total": totals.get(flag, (0, 0))[1],
                 # a record ("the biggest hit") is not summed: the world's total means nothing there
                 "record": flag.startswith("stat_damage")}
                for flag, card, unit, kind in cards]})
        return boards

    def hall_data():
        return {"weapons": [
                    {"title": "Broń 30 poziomu: średnie obrażenia", "key": "avg", "label": "Średnie", "other": "Umiejętności",
                     "rows": weapon_board("avg", "avg_damage")},
                    {"title": "Broń 30 poziomu: moc umiejętności", "key": "skill", "label": "Umiejętności", "other": "Średnie",
                     "rows": weapon_board("skill", "skill_damage")}],
                "boards": flag_boards(), "at": time.strftime("%d.%m %H:%M")}

    @app.route("/hall-of-fame")
    @m.login_required
    def hall_of_fame():
        if cache["data"] is None or time.time() - cache["at"] > 600:
            try:
                cache.update(at=time.time(), data=hall_data())
            except pymysql.MySQLError:
                app.logger.exception("Sala chwały: no data")
                return render_template("hall_of_fame.html", d=None)
        return render_template("hall_of_fame.html", d=cache["data"])
