"""Lexiw: "Show tańca" - the operator's own dance troupe, to film.

The roster is player.playerbot_dancer: a name, a class (with its sex), a level
and an order. The game core (playerbot_dancer.h) gives every row a seeded
character that never played, renames it and from then on spawns it only for
the show. The show itself is /opt/m2spool/playerbot_dance.tsv, rewritten for
every button: gather the troupe beside one of the operator's characters in a
formation, start one dance for all of them in the same server tick (repeated
every few seconds), stop. The core writes back what it sees in
/opt/m2spool/playerbot_dance.status.

install_dance_show(module) is called from app.py.
"""
import os
import re
import time
import uuid

import pymysql
from flask import flash, redirect, render_template, request, session, url_for

# player.job: the class and the sex of the character the engine picks.
JOBS = ((0, "Wojownik"), (4, "Wojowniczka"), (5, "Ninja (mężczyzna)"), (1, "Ninja (kobieta)"),
        (2, "Sura"), (6, "Sura (kobieta)"), (7, "Szaman (mężczyzna)"), (3, "Szamanka"))
FORMATIONS = (("row", "Jeden rząd"), ("rows", "Dwa rzędy"), ("circle", "Koło wokół postaci"))
NAME_RE = re.compile(r"^[A-Za-z0-9\[\]]{2,24}$")
MAX_DANCERS = 40
DDL = ("CREATE TABLE IF NOT EXISTS player.playerbot_dancer ("
       "id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, slot SMALLINT NOT NULL DEFAULT 0, "
       "name VARCHAR(24) NOT NULL, job TINYINT UNSIGNED NOT NULL DEFAULT 0, level TINYINT UNSIGNED NOT NULL DEFAULT 1, "
       "pid INT UNSIGNED NULL, status VARCHAR(64) NOT NULL DEFAULT '', "
       "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, UNIQUE KEY pid (pid)) ENGINE=InnoDB")
# The look of a dancer (item vnums, 0 = keep what it wears); the core adds the
# same columns, whichever comes first.
LOOK_DDL = ("ALTER TABLE player.playerbot_dancer ADD COLUMN IF NOT EXISTS weapon INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS armor INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS costume INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS hair INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS skin INT UNSIGNED NOT NULL DEFAULT 0")
# (column, label, what fits: item_proto (type, subtype) - None = any subtype but arrows)
LOOK_SLOTS = (("weapon", "Broń", ((1, None),)), ("armor", "Zbroja", ((2, 0),)),
              ("costume", "Kostium", ((28, 0),)), ("hair", "Fryzura", ((28, 1), (25, None))),
              ("skin", "Skórka broni", ((28, 4),)))
# item_proto.antiflag bits: who may NOT use it.
ANTI_FEMALE, ANTI_MALE = 1 << 0, 1 << 1
ANTI_CLASS = (1 << 2, 1 << 3, 1 << 4, 1 << 5)   # Wojownik, Ninja, Sura, Szaman = job % 4
FEMALE_JOBS = {1, 3, 4, 6}


def install_dance_show(m):
    app = m.app
    show_file = m.RATES_SPOOL / "playerbot_dance.tsv"
    status_file = m.RATES_SPOOL / "playerbot_dance.status"
    job_names = dict(JOBS)

    def read_kv(path):
        values = {}
        try:
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
                key, _, value = line.strip().partition(" ")
                if key and not key.startswith("#"):
                    values[key] = value.strip()
        except OSError:
            pass
        return values

    def write_show(values):
        m.RATES_SPOOL.mkdir(parents=True, exist_ok=True)
        temporary = m.RATES_SPOOL / f"playerbot_dance.tsv.{uuid.uuid4().hex}"
        try:
            temporary.write_text("".join(f"{k} {v}\n" for k, v in values.items()), encoding="utf-8")
            temporary.chmod(0o664)
            os.replace(temporary, show_file)
        finally:
            temporary.unlink(missing_ok=True)

    def owners():
        """The operator's own characters the troupe can stand beside: the GMs'."""
        return m.rows("""SELECT p.id, p.name, p.level FROM common.gmlist g
                          JOIN player.player p ON BINARY p.name = BINARY g.mName ORDER BY p.name""")

    def check(form_name):
        if request.form.get("dance_csrf", "") != session.get("seban_update_csrf", ""):
            flash("Sesja wygasła, spróbuj jeszcze raz.", "error")
            return False
        return True

    def name_problem(name, row_id=None):
        if not NAME_RE.match(name):
            return "Nick: 2–24 znaki, litery, cyfry i nawiasy [ ]."
        clash = m.one("SELECT id FROM player.playerbot_dancer WHERE name=%s AND id<>%s LIMIT 1", (name, row_id or 0))
        if clash:
            return f"Nick „{name}” ma już inny tancerz."
        own = m.one("SELECT pid FROM player.playerbot_dancer WHERE id=%s", (row_id or 0,)) if row_id else None
        taken = m.one("SELECT id FROM player.player WHERE BINARY name=BINARY %s LIMIT 1", (name,))
        if taken and not (own and own.get("pid") and int(own["pid"]) == int(taken["id"])):
            return f"Nick „{name}” nosi już ktoś w świecie."
        return None

    def ensure_table():
        m.rows(DDL)
        m.rows(LOOK_DDL)

    def look_problem(column, vnum, job):
        """None when the item fits that slot and that class and sex, else why not."""
        if not vnum:
            return None
        label, fits = next((lbl, f) for col, lbl, f in LOOK_SLOTS if col == column)
        row = m.one("SELECT type, subtype, antiflag, COALESCE(locale_name, name) AS name FROM player.item_proto WHERE vnum=%s",
                    (vnum,))
        if not row:
            return f"{label}: nie ma przedmiotu o VNUM {vnum}."
        kind, sub = int(row["type"]), int(row["subtype"])
        if not any(kind == t and (s is None and not (t == 1 and sub == 6) or s == sub) for t, s in fits):
            return f"{label}: „{m.game_text(row['name'])}” (VNUM {vnum}) nie jest tym rodzajem przedmiotu."
        anti = int(row["antiflag"] or 0)
        if anti & ANTI_CLASS[job % 4] or anti & (ANTI_FEMALE if job in FEMALE_JOBS else ANTI_MALE):
            return f"{label}: „{m.game_text(row['name'])}” nie pasuje do klasy albo płci tego tancerza."
        return None

    def skin_problem(skin, weapon):
        """A weapon skin is worn over a weapon of its kind (item value3 = the
        weapon's subtype), as the engine asks when a player puts one on."""
        if not skin:
            return None
        if not weapon:
            return "Skórka broni: wpisz też broń tego rodzaju w polu „Broń” (skórka zakłada się na broń)."
        s = m.one("SELECT value3 FROM player.item_proto WHERE vnum=%s", (skin,))
        w = m.one("SELECT subtype FROM player.item_proto WHERE vnum=%s", (weapon,))
        if s and w and int(s["value3"]) != int(w["subtype"]):
            return "Skórka broni: pasuje do innego rodzaju broni niż ta w polu „Broń”."
        return None

    def read_look(job):
        """The look vnums of the form, and the first problem with them."""
        look, problem = {}, None
        for column, _label, _fits in LOOK_SLOTS:
            try:
                look[column] = max(0, int(request.form.get(column, "0") or 0))
            except ValueError:
                look[column] = 0
            problem = problem or look_problem(column, look[column], job)
        problem = problem or skin_problem(look.get("skin", 0), look.get("weapon", 0))
        return look, problem

    @app.get("/api/dance/items")
    @m.login_required
    def dance_items():
        """The picker beside a look field: items of that slot's kind a dancer of
        that class and sex can wear, by name or VNUM."""
        column = request.args.get("slot", "")
        fits = next((f for c, _l, f in LOOK_SLOTS if c == column), None)
        query = request.args.get("q", "").strip()
        try:
            job = int(request.args.get("job", "0") or 0)
            weapon = int(request.args.get("weapon", "0") or 0)
        except ValueError:
            job, weapon = 0, 0
        if fits is None or (len(query) < 2 and not query.isdigit()):
            return {"items": []}
        kinds = " OR ".join(f"(p.type={t}" + (f" AND p.subtype={s})" if s is not None else
                                              (" AND p.subtype<>6)" if t == 1 else ")")) for t, s in fits)
        anti = ANTI_CLASS[job % 4] | (ANTI_FEMALE if job in FEMALE_JOBS else ANTI_MALE)
        clause, params = m.item_name_search(query)
        extra = ""
        if column == "skin" and weapon:
            w = m.one("SELECT subtype FROM player.item_proto WHERE vnum=%s", (weapon,))
            if w:
                extra = f" AND p.value3={int(w['subtype'])}"
        found = m.rows(f"""SELECT p.vnum, COALESCE(p.locale_name, p.name) AS name,
                CASE WHEN p.limittype0=1 THEN p.limitvalue0 WHEN p.limittype1=1 THEN p.limitvalue1 ELSE 0 END AS lvl
              FROM player.item_proto p WHERE ({kinds}) AND (p.antiflag & {anti}) = 0{extra} AND ({clause})
              ORDER BY (p.vnum=%s) DESC, CHAR_LENGTH(p.locale_name), p.vnum LIMIT 25""",
                       params + [int(query) if query.isdigit() else -1])
        return {"items": [{"vnum": int(r["vnum"]), "name": m.game_text(r["name"]), "level": int(r["lvl"] or 0),
                           "icon": m.item_icon_url(int(r["vnum"]))} for r in found]}

    @app.route("/dance")
    @m.login_required
    def dance_show():
        ensure_table()
        roster = list(m.rows("""SELECT d.id, d.slot, d.name, d.job, d.level, d.pid, d.status,
                                       d.weapon, d.armor, d.costume, d.hair, d.skin,
                                       p.name AS char_name, p.level AS char_level
                                FROM player.playerbot_dancer d LEFT JOIN player.player p ON p.id = d.pid
                                ORDER BY d.slot, d.id"""))
        wanted = sorted({int(r[c]) for r in roster for c, _l, _f in LOOK_SLOTS if r[c]})
        names = {}
        if wanted:
            for r in m.rows("SELECT vnum, COALESCE(locale_name, name) AS name FROM player.item_proto WHERE vnum IN ("
                            + ",".join(["%s"] * len(wanted)) + ")", wanted):
                names[int(r["vnum"])] = m.game_text(r["name"])
        for row in roster:
            row["job_name"] = job_names.get(int(row["job"]), "?")
            row["name_pending"] = bool(row["pid"]) and row["char_name"] and row["char_name"] != row["name"]
            row["look"] = [{"column": c, "label": lbl, "vnum": int(row[c] or 0), "name": names.get(int(row[c] or 0))}
                           for c, lbl, _f in LOOK_SLOTS]
        show = read_kv(show_file)
        status = read_kv(status_file)
        age = None
        try:
            age = int(time.time() - int(status.get("time", "0")))
        except ValueError:
            pass
        last_beat = None
        try:
            if int(status.get("last_beat", "0") or 0):
                last_beat = time.strftime("%H:%M:%S", time.localtime(int(status["last_beat"])))
        except ValueError:
            pass
        return render_template("dance_show.html", roster=roster, jobs=JOBS, formations=FORMATIONS, owners=owners(),
                               look_slots=LOOK_SLOTS,
                               show=show, status=status, status_age=age, last_beat=last_beat,
                               dance_csrf=m.update_csrf_token(), max_dancers=MAX_DANCERS)

    @app.post("/dance/roster")
    @m.login_required
    def dance_roster():
        if not check("roster"):
            return redirect(url_for("dance_show"))
        ensure_table()
        action = request.form.get("action", "")
        row_id = int(request.form.get("id", "0") or 0)
        name = request.form.get("name", "").strip()
        try:
            level = max(1, min(99, int(request.form.get("level", "1") or 1)))
            job = int(request.form.get("job", "0") or 0)
            slot = max(0, min(999, int(request.form.get("slot", "0") or 0)))
        except ValueError:
            flash("Poziom, klasa i kolejność muszą być liczbami.", "error")
            return redirect(url_for("dance_show"))
        if job not in job_names:
            job = 0
        if action == "add":
            count = int((m.one("SELECT COUNT(*) AS n FROM player.playerbot_dancer") or {}).get("n") or 0)
            if count >= MAX_DANCERS:
                flash(f"Najwyżej {MAX_DANCERS} tancerzy.", "error")
            elif (problem := name_problem(name) or read_look(job)[1]):
                flash(problem, "error")
            else:
                look = read_look(job)[0]
                if not slot:
                    slot = int((m.one("SELECT COALESCE(MAX(slot),0)+1 AS s FROM player.playerbot_dancer") or {}).get("s") or 1)
                m.rows("INSERT INTO player.playerbot_dancer (slot, name, job, level, weapon, armor, costume, hair, skin, status) "
                       "VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,'czeka na Zbierz')",
                       (slot, name, job, level, look["weapon"], look["armor"], look["costume"], look["hair"], look["skin"]))
                flash(f"Dodano tancerza {name}. Postać dostanie przy pierwszym „Zbierz”.")
        elif action == "save" and row_id:
            current = m.one("SELECT pid, job FROM player.playerbot_dancer WHERE id=%s", (row_id,))
            if not current:
                flash("Nie ma takiego tancerza.", "error")
            elif (problem := name_problem(name, row_id)):
                flash(problem, "error")
            else:
                if current.get("pid") and int(current["job"]) != job:
                    job = int(current["job"])
                    flash("Klasy przydzielonej postaci nie da się zmienić: usuń tancerza i dodaj nowego.", "error")
                look, look_error = read_look(job)
                if look_error:
                    flash(look_error + " Wygląd nie został zapisany.", "error")
                    m.rows("UPDATE player.playerbot_dancer SET name=%s, job=%s, level=%s, slot=%s WHERE id=%s",
                           (name, job, level, slot, row_id))
                else:
                    m.rows("UPDATE player.playerbot_dancer SET name=%s, job=%s, level=%s, slot=%s, "
                           "weapon=%s, armor=%s, costume=%s, hair=%s, skin=%s WHERE id=%s",
                           (name, job, level, slot, look["weapon"], look["armor"], look["costume"], look["hair"], look["skin"], row_id))
                flash(f"Zapisano tancerza {name}. Nowy nick wchodzi, gdy postać jest poza grą (do ~11 minut po wylogowaniu); "
                      "wygląd zakłada się przy najbliższym „Zbierz”.")
        elif action == "delete" and row_id:
            m.rows("DELETE FROM player.playerbot_dancer WHERE id=%s", (row_id,))
            flash("Usunięto tancerza. Jeśli był w grze, wyloguje się przy najbliższym odczycie.")
        return redirect(url_for("dance_show"))

    @app.post("/dance/show")
    @m.login_required
    def dance_command():
        if not check("show"):
            return redirect(url_for("dance_show"))
        command = request.form.get("command", "")
        if command not in ("gather", "start", "stop"):
            return redirect(url_for("dance_show"))
        try:
            owner = int(request.form.get("owner", "0") or 0)
            count = max(1, min(MAX_DANCERS, int(request.form.get("count", "10") or 10)))
            dance = max(1, min(6, int(request.form.get("dance", "1") or 1)))
            repeat = max(0, min(600, int(request.form.get("repeat", "0") or 0)))
        except ValueError:
            flash("Liczba tancerzy, taniec i powtórka muszą być liczbami.", "error")
            return redirect(url_for("dance_show"))
        formation = request.form.get("formation", "row")
        if formation not in dict(FORMATIONS):
            formation = "row"
        if not owner:
            flash("Wybierz postać, przy której ma stanąć zespół.", "error")
            return redirect(url_for("dance_show"))
        # "Świat bez botów": the cores log the population out and hold the door
        # while it is on; unticked, the bots come back by themselves.
        quiet = 1 if request.form.get("quiet") else 0
        write_show({"command": command, "owner": owner, "count": count, "formation": formation, "dance": dance,
                    "repeat": repeat, "quiet": quiet, "stamp": uuid.uuid4().hex[:12]})
        flash({"gather": "Zbieram zespół: tancerze logują się i stają w szyku (kilka–kilkanaście sekund).",
               "start": "Start! Taniec rusza za 2 sekundy, u wszystkich naraz.",
               "stop": "Stop: tancerze kończą i wylogowują się."}[command]
              + (" Świat bez botów: grające boty wylogowują się (ok. 20 s) i nie wchodzą, dopóki to pole jest zaznaczone."
                 if quiet else ""))
        return redirect(url_for("dance_show"))
