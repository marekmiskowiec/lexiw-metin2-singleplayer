"""Lexiw: pages for watching the bots' world.

  /system/cores    Rdzenie gry - every game process: CPU, memory, bots, the
                   age of its status file (collector.py's core snapshots)
  /players/progress Rozwój botów - levels now and over time, how fast bots
                   level, who is fastest and who stands still (the hourly
                   web_seban_bot_progress snapshots and the metric history)
  /players/deaths  Śmierci botów - what kills them, where, at which levels

install_observe(module) is called from app.py the way install_wiki is.
"""
import time
from datetime import datetime

import pymysql
from flask import render_template, request, url_for


def median(values):
    values = sorted(values)
    if not values:
        return 0
    middle = len(values) // 2
    return values[middle] if len(values) % 2 else (values[middle - 1] + values[middle]) / 2


def install_observe(m):
    app = m.app
    exp_need = list(m.EXP_LEVELS)

    def level_fraction(level, exp):
        """level + how far through it: 20.5 is half way from 20 to 21."""
        need = exp_need[min(level, len(exp_need) - 1)] if level < len(exp_need) else 0
        return level + (min(exp, need) / need if need else 0)

    # ------------------------------------------------------------------ cores

    @app.route("/system/cores")
    @m.login_required
    def system_cores():
        try:
            latest = m.one("SELECT MAX(captured_at) AS at FROM player.web_seban_core_snapshot").get("at")
            current = list(m.rows("SELECT core, cpu_percent, rss_mb, bots, status_age FROM player.web_seban_core_snapshot "
                                  "WHERE captured_at=%s ORDER BY (core LIKE 'ch%%') DESC, core", (latest,))) if latest else []
            history = m.rows("SELECT DATE_FORMAT(captured_at, '%%d.%%m %%H:%%i') AS label, core, cpu_percent, bots "
                             "FROM player.web_seban_core_snapshot WHERE captured_at >= NOW() - INTERVAL 24 HOUR "
                             "ORDER BY captured_at") if latest else []
        except pymysql.MySQLError:
            latest, current, history = None, [], []
        labels, cpu, bots = [], {}, {}
        for row in history:
            if row["label"] not in labels:
                labels.append(row["label"])
        index = {label: n for n, label in enumerate(labels)}
        for row in history:
            if row["cpu_percent"] is None:
                continue
            cpu.setdefault(row["core"], [None] * len(labels))[index[row["label"]]] = float(row["cpu_percent"])
            bots.setdefault(row["core"], [None] * len(labels))[index[row["label"]]] = int(row["bots"])
        # Only the processes that did anything: a core with no bots sits at 0.2%.
        busy = [core for core, values in cpu.items() if max((v for v in values if v is not None), default=0) >= 5]
        for row in current:
            cpu_now = float(row["cpu_percent"]) if row["cpu_percent"] is not None else None
            age = row["status_age"]
            row.update(cpu=cpu_now, rss=int(row["rss_mb"]), bots=int(row["bots"]), age=age,
                       state="stale" if age is not None and age > 25 else "limit" if cpu_now is not None and cpu_now >= 90
                       else "busy" if cpu_now is not None and cpu_now >= 60 else "ok",
                       is_core=row["core"].startswith("ch"))
        return render_template("system_cores.html", latest=latest, cores=current, labels=labels,
                               cpu_series=[{"core": core, "data": cpu[core]} for core in sorted(busy)],
                               bot_series=[{"core": core, "data": bots[core]} for core in sorted(busy) if core in bots])

    # --------------------------------------------------------------- progress

    def progress_data():
        rows = m.rows("SELECT UNIX_TIMESTAMP(captured_at) AS ts, pid, level, exp FROM player.web_seban_bot_progress "
                      "WHERE captured_at >= NOW() - INTERVAL 48 HOUR ORDER BY captured_at")
        by_bot = {}
        for r in rows:
            by_bot.setdefault(int(r["pid"]), []).append((int(r["ts"]), int(r["level"]), int(r["exp"])))
        stamps = sorted({p[0] for points in by_bot.values() for p in points})
        measured = []
        for pid, points in by_bot.items():
            if len(points) < 2 or points[-1][0] - points[0][0] < 3000:
                continue
            first, last = points[0], points[-1]
            hours = (last[0] - first[0]) / 3600
            gain = level_fraction(last[1], last[2]) - level_fraction(first[1], first[2])
            measured.append({"pid": pid, "level": last[1], "frac": level_fraction(last[1], last[2]) - last[1],
                             "gain": gain, "rate": gain / hours, "hours": hours})
        return by_bot, stamps, measured

    _progress_cache = {"at": 0.0, "data": None}

    @app.route("/players/progress")
    @m.login_required
    def bot_progress():
        if _progress_cache["data"] is None or time.time() - _progress_cache["at"] > 300:
            try:
                by_bot, stamps, measured = progress_data()
            except pymysql.MySQLError:
                by_bot, stamps, measured = {}, [], []
            _progress_cache.update(at=time.time(), data=(by_bot, stamps, measured))
        by_bot, stamps, measured = _progress_cache["data"]
        # Levels right now, from the latest snapshot.
        histogram = {}
        for points in by_bot.values():
            if points[-1][0] == (stamps[-1] if stamps else 0):
                histogram[points[-1][1]] = histogram.get(points[-1][1], 0) + 1
        levels_now = [{"level": level, "count": count} for level, count in sorted(histogram.items())]
        # How fast, by level: the median gain per hour of the bots that now stand there.
        by_level = {}
        for item in measured:
            by_level.setdefault(item["level"], []).append(item)
        speed = []
        for level, items in sorted(by_level.items()):
            if len(items) < 3:
                continue
            rate = median([i["rate"] for i in items])
            frac = median([i["frac"] for i in items])
            speed.append({"level": level, "bots": len(items), "rate": rate, "per_day": rate * 24,
                          "eta": (1 - frac) / rate if rate > 0.0005 else None,
                          "still": sum(1 for i in items if i["gain"] < 0.01)})
        names = {}
        picks = sorted(measured, key=lambda i: -i["rate"])[:10] + sorted(measured, key=lambda i: i["rate"])[:10]
        if picks:
            ids = sorted({p["pid"] for p in picks})
            for r in m.rows("SELECT id, name FROM player.player WHERE id IN (" + ",".join(["%s"] * len(ids)) + ")", ids):
                names[int(r["id"])] = r["name"]
        fast = [dict(i, name=names.get(i["pid"], f"#{i['pid']}")) for i in sorted(measured, key=lambda i: -i["rate"])[:10]]
        slow = [dict(i, name=names.get(i["pid"], f"#{i['pid']}")) for i in sorted(measured, key=lambda i: i["rate"])[:10]]
        # The level bands since the metric history began (lvl_<from>_<to>, every 5 minutes).
        bands = m.rows("SELECT DATE_FORMAT(MIN(captured_at), '%%d.%%m %%H:00') AS label, metric, ROUND(AVG(value)) AS value "
                       "FROM player.web_seban_metric_snapshot WHERE metric LIKE 'lvl\\_%%' AND captured_at >= NOW() - INTERVAL 7 DAY "
                       "GROUP BY DATE_FORMAT(captured_at, '%%Y-%%m-%%d %%H'), metric ORDER BY MIN(captured_at)")
        labels = []
        series = {}
        for r in bands:
            if r["label"] not in labels:
                labels.append(r["label"])
        position = {label: n for n, label in enumerate(labels)}
        for r in bands:
            key = r["metric"][4:].replace("_", "–")
            series.setdefault(key, [0] * len(labels))[position[r["label"]]] = int(r["value"])
        band_series = sorted(series.items(), key=lambda kv: int(kv[0].split("–")[0]))
        return render_template("bot_progress.html", levels_now=levels_now, total=sum(histogram.values()),
                               snapshots=len(stamps), measured=len(measured), speed=speed, fast=fast, slow=slow,
                               band_labels=labels, band_series=band_series,
                               since=datetime.fromtimestamp(stamps[0]) if stamps else None)

    @app.route("/api/player/<int:pid>/progress")
    @m.login_required
    def api_player_progress(pid):
        """One bot's level and experience, an hourly point, for its card."""
        found = m.rows("SELECT DATE_FORMAT(captured_at, '%%d.%%m %%H:00') AS label, level, exp FROM player.web_seban_bot_progress "
                       "WHERE pid=%s AND captured_at >= NOW() - INTERVAL 14 DAY ORDER BY captured_at", (pid,))
        points = [{"label": r["label"], "level": int(r["level"]), "value": round(level_fraction(int(r["level"]), int(r["exp"])), 2)} for r in found]
        return {"ok": True, "points": points}

    # ----------------------------------------------------------------- deaths

    def map_case():
        """SQL that names the map a (x, y) lies on, from the map bounds."""
        parts = []
        for index, (base_x, base_y, width, height) in m.MAP_BOUNDS.items():
            parts.append(f"WHEN l.x >= {int(base_x)} AND l.x < {int(base_x + width)} AND l.y >= {int(base_y)} "
                         f"AND l.y < {int(base_y + height)} THEN {int(index)}")
        return "CASE " + " ".join(parts) + " ELSE 0 END"

    _death_cache = {"at": 0.0, "data": None}

    def deaths_data(hours):
        base = "FROM log.log l WHERE l.type='CHARACTER' AND l.how IN ('DEAD_BY_NPC','DEAD_BY_PC') AND l.time >= NOW() - INTERVAL %s HOUR"
        total = m.one(f"SELECT SUM(l.how='DEAD_BY_NPC') AS npc, SUM(l.how='DEAD_BY_PC') AS pc, COUNT(DISTINCT l.who) AS bots {base}", (hours,))
        killers = m.rows(f"SELECT l.what AS vnum, COUNT(*) AS n, COUNT(DISTINCT l.who) AS victims, AVG(p.level) AS avg_level, "
                         f"MIN(p.level) AS lo, MAX(p.level) AS hi FROM log.log l JOIN player.player p ON p.id=l.who "
                         f"WHERE l.type='CHARACTER' AND l.how='DEAD_BY_NPC' AND l.time >= NOW() - INTERVAL %s HOUR "
                         f"GROUP BY l.what ORDER BY n DESC LIMIT 15", (hours,))
        where = m.rows(f"SELECT {map_case()} AS map_index, COUNT(*) AS n, COUNT(DISTINCT l.who) AS victims {base} AND l.how='DEAD_BY_NPC' "
                       f"GROUP BY map_index ORDER BY n DESC LIMIT 15", (hours,))
        levels = m.rows(f"SELECT FLOOR(p.level / 5) * 5 AS band, COUNT(*) AS n, COUNT(DISTINCT l.who) AS victims "
                        f"FROM log.log l JOIN player.player p ON p.id=l.who WHERE l.type='CHARACTER' AND l.how='DEAD_BY_NPC' "
                        f"AND l.time >= NOW() - INTERVAL %s HOUR GROUP BY band ORDER BY band", (hours,))
        hourly = m.rows("SELECT DATE_FORMAT(l.time, '%%d.%%m %%H:00') AS label, SUM(l.how='DEAD_BY_NPC') AS npc, SUM(l.how='DEAD_BY_PC') AS pc "
                        "FROM log.log l WHERE l.type='CHARACTER' AND l.how IN ('DEAD_BY_NPC','DEAD_BY_PC') AND l.time >= NOW() - INTERVAL %s HOUR "
                        "GROUP BY DATE_FORMAT(l.time, '%%Y-%%m-%%d %%H') ORDER BY MIN(l.time)", (hours,))
        victims = m.rows(f"SELECT l.who AS pid, p.name, p.level, COUNT(*) AS n {base.replace('FROM log.log l', 'FROM log.log l JOIN player.player p ON p.id=l.who')} "
                         f"AND l.how='DEAD_BY_NPC' GROUP BY l.who, p.name, p.level ORDER BY n DESC LIMIT 10", (hours,))
        mob_names = {}
        wanted = sorted({int(r["vnum"]) for r in killers})
        if wanted:
            for r in m.rows("SELECT vnum, name, locale_name, level FROM player.mob_proto WHERE vnum IN (" + ",".join(["%s"] * len(wanted)) + ")", wanted):
                mob_names[int(r["vnum"])] = (m.game_text(r["locale_name"] or r["name"]), int(r["level"]))
        for r in killers:
            name, level = mob_names.get(int(r["vnum"]), (f"VNUM {r['vnum']}", None))
            r.update(name=name, mob_level=level, avg_level=round(float(r["avg_level"]), 1), deaths_each=round(int(r["n"]) / max(1, int(r["victims"])), 1))
        for r in levels:
            r["label"] = f"{int(r['band'])}–{int(r['band']) + 4}"
        for r in where:
            r.update(name=m.map_name(r["map_index"]) if int(r["map_index"]) else "gdzie indziej", n=int(r["n"]))
        return {"total": total, "killers": killers, "where": where, "levels": levels, "hourly": hourly, "victims": victims}

    @app.route("/players/deaths")
    @m.login_required
    def bot_deaths():
        hours = {"6": 6, "24": 24, "168": 168}.get(request.args.get("h", "24"), 24)
        cache = _death_cache.setdefault(hours, {"at": 0.0, "data": None})
        if cache["data"] is None or time.time() - cache["at"] > 300:
            cache.update(at=time.time(), data=deaths_data(hours))
        return render_template("bot_deaths.html", hours=hours, **cache["data"])

    @app.route("/api/player/<int:pid>/deaths")
    @m.login_required
    def api_player_deaths(pid):
        """How often one bot died to monsters and players, and to which monsters."""
        counts = m.one("SELECT SUM(time >= NOW() - INTERVAL 24 HOUR AND how='DEAD_BY_NPC') AS npc24, "
                       "SUM(time >= NOW() - INTERVAL 24 HOUR AND how='DEAD_BY_PC') AS pc24, SUM(how='DEAD_BY_NPC') AS npc7, "
                       "SUM(how='DEAD_BY_PC') AS pc7 FROM log.log WHERE who=%s AND type='CHARACTER' "
                       "AND how IN ('DEAD_BY_NPC','DEAD_BY_PC') AND time >= NOW() - INTERVAL 7 DAY", (pid,))
        killers = m.rows("SELECT what AS vnum, COUNT(*) AS n FROM log.log WHERE who=%s AND type='CHARACTER' AND how='DEAD_BY_NPC' "
                         "AND time >= NOW() - INTERVAL 7 DAY GROUP BY what ORDER BY n DESC LIMIT 5", (pid,))
        names = {}
        wanted = sorted({int(r["vnum"]) for r in killers})
        if wanted:
            for r in m.rows("SELECT vnum, name, locale_name FROM player.mob_proto WHERE vnum IN (" + ",".join(["%s"] * len(wanted)) + ")", wanted):
                names[int(r["vnum"])] = m.game_text(r["locale_name"] or r["name"])
        return {"ok": True, "npc24": int(counts.get("npc24") or 0), "pc24": int(counts.get("pc24") or 0),
                "npc7": int(counts.get("npc7") or 0), "pc7": int(counts.get("pc7") or 0),
                "killers": [{"vnum": int(r["vnum"]), "name": names.get(int(r["vnum"]), f"VNUM {r['vnum']}"), "n": int(r["n"]),
                             "url": url_for("wiki_mob", vnum=int(r["vnum"]))} for r in killers]}
