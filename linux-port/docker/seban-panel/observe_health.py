"""Lexiw: "Zdrowie botów" - one page that answers "is this change to the bots working?".

How every personality and persona does (levels, deaths, wealth, trade), where
the bots spend their time and why they walk. A snapshot saved before a change
to the game core can be compared with the numbers after it.

install_health(module) is called from app.py after install_observe, which
hands over its cached progress measurements as module.observe_progress_cached.
"""
import json
import re
import time
from datetime import datetime

import pymysql
from flask import flash, redirect, render_template, request, session, url_for

ACTION_GROUPS = (
    ("Walka", {2}), ("Podróż", {1}), ("Łup", {3}),
    ("Miasto: sprawunki i ulepszanie", {5, 6, 7, 8, 9, 12}),
    ("Stragan i rynek", {13, 15}), ("Odpoczynek", {4, 17}), ("Drużyna", {10, 16}),
    ("Zajęcia (ryby, ruda, zioła, Biolog)", {11, 14, 18, 19}), ("Planowanie i inne", {0}),
)
# Why a travelling bot walks, from the status text the core writes.
TRAVEL_REASONS = (
    ("Przenosiny między polami", re.compile(r"lepszego miejsca", re.I)),
    ("Sprawunki w mieście", re.compile(r"handlarz|handlark|kowal|magazyn|do miasta|zapasy", re.I)),
    ("Zioła dla Biologa", re.compile(r"biolog", re.I)),
    ("Loch Małp i medale", re.compile(r"lochu? ma|medal", re.I)),
    ("Wyprawa po broń 30 poz.", re.compile(r"broni na", re.I)),
    ("Za drużyną i exp", re.compile(r"\[PT\]|expie|wracam na exp", re.I)),
)
# key, header, +1 higher is better / -1 lower is better / 0 neutral
COLUMNS = (("bots", "Botów", 0), ("level", "Śr. poziom", 1), ("per_day", "Awans (poz./dobę)", 1),
           ("deaths", "Zgonów/bota/dobę", -1), ("gold", "Mediana yang", 0), ("shop", "% ze straganem", 1),
           ("sales", "Sprzedaży/bota/dobę", 1), ("fight", "% walki", 1), ("travel", "% podróży", -1))


def median(values):
    values = sorted(values)
    if not values:
        return 0
    middle = len(values) // 2
    return values[middle] if len(values) % 2 else (values[middle - 1] + values[middle]) / 2


def install_health(m):
    app = m.app
    cache = {"at": 0.0, "data": None}
    log_cache = {"at": 0.0, "data": None}

    def log_counts():
        """Deaths and stall sales of the last 24 hours per character: one pass
        over the log (no index on time), kept ten minutes."""
        if log_cache["data"] is None or time.time() - log_cache["at"] > 600:
            found = m.rows("SELECT l.who AS pid, SUM(l.how='DEAD_BY_NPC') AS deaths, SUM(l.how='PLAYERBOT_STALL_SOLD') AS sales "
                           "FROM log.log l WHERE l.how IN ('DEAD_BY_NPC','PLAYERBOT_STALL_SOLD') "
                           "AND l.time >= NOW() - INTERVAL 24 HOUR GROUP BY l.who")
            log_cache.update(at=time.time(), data={int(r["pid"]): (int(r["deaths"] or 0), int(r["sales"] or 0)) for r in found})
        return log_cache["data"]

    def action_group(action):
        for name, ids in ACTION_GROUPS:
            if action in ids:
                return name
        return "Planowanie i inne"

    def travel_reason(status):
        for name, pattern in TRAVEL_REASONS:
            if pattern.search(status or ""):
                return name
        return "Inne"

    def group_row(label, members, ctx):
        n = len(members)
        pids = [b["id"] for b in members]
        rates = [24 * ctx["rate"][p] for p in pids if p in ctx["rate"]]
        actions = [int(b.get("action") or 0) for b in members]
        return {"label": label, "bots": n, "level": round(sum(int(b["level"]) for b in members) / n, 1),
                "per_day": round(median(rates), 1) if rates else None,
                "deaths": round(sum(ctx["log"].get(p, (0, 0))[0] for p in pids) / n, 1),
                "gold": int(median([ctx["gold"].get(p, 0) for p in pids])),
                "shop": round(100 * sum(1 for p in pids if p in ctx["offers"]) / n),
                "sales": round(sum(ctx["log"].get(p, (0, 0))[1] for p in pids) / n, 2),
                "fight": round(100 * actions.count(2) / n), "travel": round(100 * actions.count(1) / n)}

    def health_data():
        bots = [b for b in m.live_bots() if b.get("personality") is not None]
        _by_bot, _stamps, measured = m.observe_progress_cached()
        ctx = {"rate": {item["pid"]: item["rate"] for item in measured}, "log": log_counts()}
        ctx["gold"] = {int(r["id"]): int(r["gold"] or 0) for r in m.rows(
            "SELECT p.id, p.gold FROM player.player p JOIN account.account a ON a.id=p.account_id WHERE LEFT(a.login,10)='playerbot_'")}
        try:
            ctx["offers"] = {int(r["owner_id"]) for r in m.rows("SELECT DISTINCT owner_id FROM player.web_seban_offer_flat")}
        except pymysql.MySQLError:
            ctx["offers"] = set()
        characters, personas = {}, {}
        for bot in bots:
            characters.setdefault(int(bot["personality"]), []).append(bot)
            if bot.get("persona") is not None:
                personas.setdefault(int(bot["persona"]), []).append(bot)
        total = len(bots)
        shares = {}
        for bot in bots:
            name = action_group(int(bot.get("action") or 0))
            shares[name] = shares.get(name, 0) + 1
        travelling = [b for b in bots if int(b.get("action") or 0) == 1]
        reasons = {}
        for bot in travelling:
            name = travel_reason(bot.get("status", ""))
            reasons[name] = reasons.get(name, 0) + 1
        by_size = lambda kv: -len(kv[1])  # noqa: E731
        return {
            "at": datetime.now().strftime("%d.%m %H:%M"), "total": total,
            "overall": group_row("Wszystkie boty", bots, ctx) if bots else None,
            "characters": [group_row(m.BOT_PERSONALITIES.get(k, f"#{k}"), v, ctx) for k, v in sorted(characters.items(), key=by_size)],
            "personas": [group_row(m.BOT_PERSONAS.get(k, f"#{k}"), v, ctx) for k, v in sorted(personas.items(), key=by_size)],
            "actions": [{"label": name, "bots": shares.get(name, 0), "pct": round(100 * shares.get(name, 0) / total, 1) if total else 0}
                        for name, _ids in ACTION_GROUPS],
            "reasons": [{"label": name, "bots": n, "pct": round(100 * n / total, 1) if total else 0,
                         "of_travel": round(100 * n / len(travelling)) if travelling else 0}
                        for name, n in sorted(reasons.items(), key=lambda kv: -kv[1])],
            "travelling": len(travelling)}

    def ensure_table():
        m.rows("CREATE TABLE IF NOT EXISTS player.web_seban_health_snapshot (id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
               "name VARCHAR(80) NOT NULL, created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, data MEDIUMTEXT NOT NULL) ENGINE=InnoDB")

    def with_deltas(rows, saved_rows):
        """Each row gets, per column, the change against the saved row of the same name."""
        saved = {r["label"]: r for r in saved_rows or []}
        for row in rows:
            old = saved.get(row["label"])
            row["delta"] = {}
            if not old:
                continue
            for key, _header, direction in COLUMNS:
                a, b = row.get(key), old.get(key)
                if a is None or b is None or a == b:
                    continue
                change = round(a - b, 2)
                tone = "" if direction == 0 else ("good" if change * direction > 0 else "bad")
                shown = str(int(change)) if float(change).is_integer() else str(change).replace(".", ",")
                row["delta"][key] = {"text": ("+" if change > 0 else "") + shown, "tone": tone}

    @app.route("/players/health", methods=["GET", "POST"])
    @m.login_required
    def bot_health():
        ensure_table()
        if request.method == "POST":
            if request.form.get("health_csrf", "") != session.get("seban_update_csrf", ""):
                flash("Sesja wygasła, spróbuj jeszcze raz.")
                return redirect(url_for("bot_health"))
            if request.form.get("action") == "delete":
                m.rows("DELETE FROM player.web_seban_health_snapshot WHERE id=%s", (int(request.form.get("snap", "0") or 0),))
            else:
                name = (request.form.get("name", "").strip() or "Migawka")[:80]
                m.rows("INSERT INTO player.web_seban_health_snapshot (name, data) VALUES (%s, %s)",
                       (name, json.dumps(health_data(), ensure_ascii=False)))
                flash(f"Zapisano migawkę „{name}”.")
            return redirect(url_for("bot_health"))
        if cache["data"] is None or time.time() - cache["at"] > 120:
            cache.update(at=time.time(), data=health_data())
        data = json.loads(json.dumps(cache["data"]))
        snapshots = m.rows("SELECT id, name, DATE_FORMAT(created_at, '%%d.%%m %%H:%%i') AS at "
                           "FROM player.web_seban_health_snapshot ORDER BY id DESC LIMIT 30")
        picked = None
        try:
            wanted = int(request.args.get("vs", "0"))
        except ValueError:
            wanted = 0
        if wanted:
            row = m.one("SELECT name, data FROM player.web_seban_health_snapshot WHERE id=%s", (wanted,))
            if row:
                picked = {"id": wanted, "name": row["name"], "data": json.loads(row["data"])}
        old = picked["data"] if picked else {}
        for section in ("characters", "personas"):
            with_deltas(data[section], old.get(section))
        if data["overall"]:
            with_deltas([data["overall"]], [old["overall"]] if old.get("overall") else [])
        for section in ("actions", "reasons"):
            saved = {r["label"]: r for r in old.get(section, [])}
            for row in data[section]:
                before = saved.get(row["label"])
                row["delta"] = round(row["pct"] - before["pct"], 1) if before else None
        return render_template("bot_health.html", d=data, columns=COLUMNS, snapshots=snapshots, picked=picked,
                               health_csrf=m.update_csrf_token())
