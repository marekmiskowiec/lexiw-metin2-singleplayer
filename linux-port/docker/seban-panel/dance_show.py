"""Lexiw: "Show tańca" - the operator's own dance troupe, to film.

The roster is player.playerbot_dancer: a name, a class (with its sex), a level,
an order, a look and a place of its own in the "manual" formation. The game
core (playerbot_dancer.h) gives every row a seeded character that never
played, renames it and from then on spawns it only for the show. The show
itself is /opt/m2spool/playerbot_dance.tsv, rewritten for every button: gather
the troupe beside one of the operator's characters (or on a spot of a map) in
a formation, start it - a countdown, a marker gesture to cut the film to, then
the choreography script, step by step - and stop. The core writes back what
it sees in /opt/m2spool/playerbot_dance.status, and where the owner stands in
playerbot_dance.status.owner.

What the form held last is kept in playerbot_dance.panel.json (the script as
it was typed); a scene (player.playerbot_dance_scene) is that and the roster
together, loaded in one click.

install_dance_show(module) is called from app.py.
"""
import json
import os
import re
import time
import uuid

from flask import flash, redirect, render_template, request, session, url_for

# player.job: the class and the sex of the character the engine picks.
JOBS = ((0, "Wojownik"), (4, "Wojowniczka"), (5, "Ninja (mężczyzna)"), (1, "Ninja (kobieta)"),
        (2, "Sura"), (6, "Sura (kobieta)"), (7, "Szaman (mężczyzna)"), (3, "Szamanka"))
# (engine key, label, the word the script uses)
FORMATIONS = (("row", "Jeden rząd", "rząd"), ("rows", "Dwa rzędy (mijankowo)", "dwa-rzędy"),
              ("grid", "Trzy rzędy (mijankowo)", "trzy-rzędy"), ("column", "Kolumna (jeden za drugim)", "kolumna"),
              ("wedge", "Klin (ostrzem do widza)", "klin"), ("vee", "V (otwarte do widza)", "v"),
              ("circle", "Koło (twarzą do środka)", "koło"), ("halfcircle", "Półkole", "półkole"),
              ("manual", "Własne miejsca (z tabeli zespołu)", "własne"))
# (client command, label, the word the script uses): the emotions the client
# plays for anybody (cmd_emotion.cpp's table without the two-person ones).
GESTURES = (("clap", "Klaskanie", "klask"), ("cheer1", "Wiwat", "wiwat"), ("cheer2", "Hurra", "hurra"),
            ("congratulation", "Gratulacje", "gratulacje"), ("forgive", "Przeprosiny", "przepraszam"),
            ("angry", "Złość", "złość"), ("attractive", "Uwodzenie", "uwodzenie"), ("sad", "Smutek", "smutek"),
            ("shy", "Wstyd", "wstyd"), ("cheerup", "Doping", "doping"), ("banter", "Zazdrość", "zazdrość"),
            ("joy", "Radość", "radość"),
            # Custom, in the client patched by tools/kaia-dances (LEXIW_KAIA_DANCES_V1).
            ("backflip", "Salto (custom)", "salto"), ("pushup", "Pompka (custom)", "pompka"))
WHO_WORDS = {"wszyscy": "all", "parzyści": "even", "parzysci": "even", "nieparzyści": "odd", "nieparzysci": "odd"}
NAME_RE = re.compile(r"^[A-Za-z0-9\[\]]{2,24}$")
MAX_DANCERS = 40
MAX_STEPS = 200
# Dances 7-24: the "Dances by KaiaProduction" pack in the patched client (LEXIW_KAIA_DANCES_V1).
MAX_DANCE = 24
DDL = ("CREATE TABLE IF NOT EXISTS player.playerbot_dancer ("
       "id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, slot SMALLINT NOT NULL DEFAULT 0, "
       "name VARCHAR(24) NOT NULL, job TINYINT UNSIGNED NOT NULL DEFAULT 0, level TINYINT UNSIGNED NOT NULL DEFAULT 1, "
       "pid INT UNSIGNED NULL, status VARCHAR(64) NOT NULL DEFAULT '', "
       "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, UNIQUE KEY pid (pid)) ENGINE=InnoDB")
# The look of a dancer (item vnums, 0 = keep what it wears) and its own place
# in the "manual" formation (cm, to the right and back); the core adds the
# same columns, whichever comes first.
LOOK_DDL = ("ALTER TABLE player.playerbot_dancer ADD COLUMN IF NOT EXISTS weapon INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS armor INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS costume INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS hair INT UNSIGNED NOT NULL DEFAULT 0, "
            "ADD COLUMN IF NOT EXISTS skin INT UNSIGNED NOT NULL DEFAULT 0")
POS_DDL = ("ALTER TABLE player.playerbot_dancer ADD COLUMN IF NOT EXISTS pos_x INT NOT NULL DEFAULT 0, "
           "ADD COLUMN IF NOT EXISTS pos_y INT NOT NULL DEFAULT 0")
SCENE_DDL = ("CREATE TABLE IF NOT EXISTS player.playerbot_dance_scene ("
             "id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(48) NOT NULL, "
             "settings MEDIUMTEXT NOT NULL, roster MEDIUMTEXT NOT NULL, "
             "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, "
             "UNIQUE KEY name (name)) ENGINE=InnoDB")
# (column, label, what fits: item_proto (type, subtype) - None = any subtype but arrows)
LOOK_SLOTS = (("weapon", "Broń", ((1, None),)), ("armor", "Zbroja", ((2, 0),)),
              ("costume", "Kostium", ((28, 0),)), ("hair", "Fryzura", ((28, 1), (25, None))),
              ("skin", "Skórka broni", ((28, 4),)))
# item_proto.antiflag bits: who may NOT use it.
ANTI_FEMALE, ANTI_MALE = 1 << 0, 1 << 1
ANTI_CLASS = (1 << 2, 1 << 3, 1 << 4, 1 << 5)   # Wojownik, Ninja, Sura, Szaman = job % 4
FEMALE_JOBS = {1, 3, 4, 6}
# What the control form holds, with its defaults.
SETTINGS = {"owner": "", "count": "10", "formation": "row", "dance": "1", "repeat": "0", "quiet": "0",
            "anchor": "owner", "map": "", "spot_x": "", "spot_y": "", "facing": "owner", "facing_deg": "180",
            "transition": "walk", "countdown": "3", "notice": "1", "marker": "-", "daymode": "keep",
            "snow": "keep", "loop": "0", "script": ""}
PHASES = {"gather": "zbiórka", "countdown": "odliczanie", "marker": "gest startowy", "dancing": "taniec",
          "done": "skrypt skończony", "-": "—"}


def parse_script(text, formation_words, gesture_words):
    """The choreography as the operator types it, one step a line:
        0 taniec 5            (everybody, once)
        0 taniec 5 co 1       (again every second, until their next step)
        8 taniec 3 parzyści   (wszyscy, parzyści, nieparzyści, rząd 2, 4, 1-5)
        12 gest klask
        16 szyk koło          (the troupe walks into the new formation)
        20 pauza nieparzyści  (their repeating dance stops)
    -> ([(ms, action, arg, who, every_ms)], [errors])."""
    steps, errors = [], []
    for no, raw in enumerate(text.splitlines(), 1):
        line = raw.split("#", 1)[0].strip().lower()
        if not line:
            continue
        words = line.split()
        try:
            at = float(words[0].replace(",", "."))
        except ValueError:
            errors.append(f"Linia {no}: na początku czas w sekundach (np. 0, 8, 12.5).")
            continue
        if not 0 <= at <= 3600 or len(words) < 2:
            errors.append(f"Linia {no}: czas 0–3600 s, a po nim co ma się stać (taniec, gest, szyk, pauza).")
            continue
        action, rest = words[1], words[2:]
        every = 0
        if "co" in rest:
            i = rest.index("co")
            try:
                every = float(rest[i + 1].replace(",", "."))
            except (IndexError, ValueError):
                errors.append(f"Linia {no}: po „co” podaj sekundy, np. „co 1”.")
                continue
            if not 0.5 <= every <= 600:
                errors.append(f"Linia {no}: „co” od 0,5 do 600 sekund.")
                continue
            rest = rest[:i] + rest[i + 2:]
        arg, who_words = None, rest
        if action == "taniec":
            if not rest or not rest[0].isdigit() or not 1 <= int(rest[0]) <= MAX_DANCE:
                errors.append(f"Linia {no}: „taniec” i numer 1–6 (7–24: tańce custom, wymagają klienta z paczką Kaia).")
                continue
            arg, who_words, kind = f"dance{int(rest[0])}", rest[1:], "emote"
        elif action == "gest":
            arg = gesture_words.get(rest[0]) if rest else None
            if not arg:
                errors.append(f"Linia {no}: nieznany gest. Dostępne: {', '.join(sorted(gesture_words))}.")
                continue
            who_words, kind = rest[1:], "emote"
        elif action == "szyk":
            arg = formation_words.get(rest[0]) if rest else None
            if not arg:
                errors.append(f"Linia {no}: nieznany szyk. Dostępne: {', '.join(formation_words)}.")
                continue
            who_words, kind, every = [], "formation", 0
        elif action == "pauza":
            arg, kind, every = "-", "pause", 0
        else:
            errors.append(f"Linia {no}: „{action}” — znam: taniec, gest, szyk, pauza.")
            continue
        who = "all"
        if who_words:
            joined = " ".join(who_words)
            if joined in WHO_WORDS:
                who = WHO_WORDS[joined]
            elif len(who_words) == 2 and who_words[0] in ("rząd", "rzad") and who_words[1].isdigit():
                who = f"row{int(who_words[1])}"
            elif re.fullmatch(r"\d{1,2}-\d{1,2}", joined) or re.fullmatch(r"\d{1,2}", joined):
                who = joined
            else:
                errors.append(f"Linia {no}: kto? wszyscy, parzyści, nieparzyści, rząd 2, numer (4) albo zakres (1-5).")
                continue
        steps.append((int(round(at * 1000)), kind, arg, who, int(round(every * 1000))))
    if len(steps) > MAX_STEPS:
        errors.append(f"Najwyżej {MAX_STEPS} kroków.")
    steps.sort(key=lambda s: s[0])
    return steps, errors


def install_dance_show(m):
    app = m.app
    show_file = m.RATES_SPOOL / "playerbot_dance.tsv"
    status_file = m.RATES_SPOOL / "playerbot_dance.status"
    owner_file = m.RATES_SPOOL / "playerbot_dance.status.owner"
    panel_file = m.RATES_SPOOL / "playerbot_dance.panel.json"
    job_names = dict(JOBS)
    formation_keys = {key for key, _l, _w in FORMATIONS}
    formation_words = {word: key for key, _l, word in FORMATIONS}
    formation_words.update({key: key for key in formation_keys})
    formation_words.update({"rzad": "row", "kolo": "circle", "polkole": "halfcircle", "wlasne": "manual"})
    gesture_words = {word: key for key, _l, word in GESTURES}
    gesture_words.update({key: key for key, _l, _w in GESTURES})
    gesture_words.update({"zlosc": "angry", "zazdrosc": "banter", "radosc": "joy"})
    gesture_keys = {key for key, _l, _w in GESTURES}

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

    def write_atomic(path, text):
        m.RATES_SPOOL.mkdir(parents=True, exist_ok=True)
        temporary = path.with_name(f"{path.name}.{uuid.uuid4().hex}")
        try:
            temporary.write_text(text, encoding="utf-8")
            temporary.chmod(0o664)
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)

    def read_settings():
        settings = dict(SETTINGS)
        try:
            saved = json.loads(panel_file.read_text(encoding="utf-8"))
            settings.update({k: str(v) for k, v in saved.items() if k in SETTINGS})
        except (OSError, ValueError):
            # Before the panel kept its own file: what the show file says.
            show = read_kv(show_file)
            settings.update({k: v for k, v in show.items() if k in SETTINGS and k != "script"})
        return settings

    def owners():
        """The operator's own characters the troupe can stand beside: the GMs'."""
        return m.rows("""SELECT p.id, p.name, p.level FROM common.gmlist g
                          JOIN player.player p ON BINARY p.name = BINARY g.mName ORDER BY p.name""")

    def check():
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
        m.rows(POS_DDL)
        m.rows(SCENE_DDL)

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

    def read_pos():
        """A dancer's own place (metres in the form, cm in the table)."""
        pos = {}
        for column in ("pos_x", "pos_y"):
            try:
                pos[column] = int(round(max(-50.0, min(50.0, float((request.form.get(column) or "0").replace(",", ".")))) * 100))
            except ValueError:
                pos[column] = 0
        return pos

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

    @app.get("/api/dance/owner")
    @m.login_required
    def dance_owner_spot():
        """Where the owner stands now (written by its core every 2 s), in the
        coordinates the game shows (local, metres)."""
        spot = read_kv(owner_file)
        try:
            age = int(time.time() - int(spot.get("time", "0")))
            return {"ok": age < 30, "owner": int(spot.get("owner", 0)), "name": spot.get("name", ""),
                    "map": int(spot.get("map", 0)), "map_name": m.map_name(int(spot.get("map", 0))),
                    "x": round(int(spot.get("x", 0)) / 100), "y": round(int(spot.get("y", 0)) / 100),
                    "rot": int(float(spot.get("rot", 0))), "age": age}
        except ValueError:
            return {"ok": False}

    @app.route("/dance")
    @m.login_required
    def dance_show():
        ensure_table()
        roster = list(m.rows("""SELECT d.id, d.slot, d.name, d.job, d.level, d.pid, d.status,
                                       d.weapon, d.armor, d.costume, d.hair, d.skin, d.pos_x, d.pos_y,
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
            row["pos_x_m"] = f"{int(row['pos_x'] or 0) / 100:g}"
            row["pos_y_m"] = f"{int(row['pos_y'] or 0) / 100:g}"
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
        scenes = m.rows("SELECT id, name, roster, updated_at FROM player.playerbot_dance_scene ORDER BY name")
        for scene in scenes:
            try:
                scene["dancers"] = len(json.loads(scene["roster"]))
            except ValueError:
                scene["dancers"] = 0
        maps = sorted(((i, n) for i, n in m.MAP_NAMES.items()), key=lambda x: x[1])
        return render_template("dance_show.html", roster=roster, jobs=JOBS, formations=FORMATIONS, gestures=GESTURES,
                               owners=owners(), look_slots=LOOK_SLOTS, settings=read_settings(), maps=maps,
                               show=show, status=status, status_age=age, last_beat=last_beat, scenes=scenes,
                               phases=PHASES, formation_names={k: lbl for k, lbl, _w in FORMATIONS},
                               manual=[{"x": int(r["pos_x"] or 0), "y": int(r["pos_y"] or 0)} for r in roster],
                               dance_csrf=m.update_csrf_token(), max_dancers=MAX_DANCERS)

    @app.post("/dance/roster")
    @m.login_required
    def dance_roster():
        if not check():
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
        pos = read_pos()
        if action == "add":
            count = int((m.one("SELECT COUNT(*) AS n FROM player.playerbot_dancer") or {}).get("n") or 0)
            if count >= MAX_DANCERS:
                flash(f"Najwyżej {MAX_DANCERS} tancerzy.", "error")
            elif (problem := name_problem(name) or read_look(job)[1]):
                flash(problem, "error")
            else:
                look = read_look(job)[0]
                if not slot:
                    slot = int((m.one("SELECT COALESCE(MAX(slot),0)+1 AS s FROM player.playerbot_dancer WHERE slot < 900")
                                or {}).get("s") or 1)
                m.rows("INSERT INTO player.playerbot_dancer (slot, name, job, level, weapon, armor, costume, hair, skin, "
                       "pos_x, pos_y, status) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,'czeka na Zbierz')",
                       (slot, name, job, level, look["weapon"], look["armor"], look["costume"], look["hair"], look["skin"],
                        pos["pos_x"], pos["pos_y"]))
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
                    m.rows("UPDATE player.playerbot_dancer SET name=%s, job=%s, level=%s, slot=%s, pos_x=%s, pos_y=%s "
                           "WHERE id=%s", (name, job, level, slot, pos["pos_x"], pos["pos_y"], row_id))
                else:
                    m.rows("UPDATE player.playerbot_dancer SET name=%s, job=%s, level=%s, slot=%s, "
                           "weapon=%s, armor=%s, costume=%s, hair=%s, skin=%s, pos_x=%s, pos_y=%s WHERE id=%s",
                           (name, job, level, slot, look["weapon"], look["armor"], look["costume"], look["hair"],
                            look["skin"], pos["pos_x"], pos["pos_y"], row_id))
                flash(f"Zapisano tancerza {name}. Nowy nick wchodzi, gdy postać jest poza grą (do ~11 minut po wylogowaniu); "
                      "wygląd i miejsce wchodzą przy najbliższym „Zbierz”.")
        elif action == "delete" and row_id:
            m.rows("DELETE FROM player.playerbot_dancer WHERE id=%s", (row_id,))
            flash("Usunięto tancerza. Jeśli był w grze, wyloguje się przy najbliższym odczycie.")
        return redirect(url_for("dance_show"))

    def read_form_settings():
        """The control form as settings (strings, as kept), and the first problem."""
        f = request.form
        s = dict(SETTINGS)
        for key in SETTINGS:
            if key in ("quiet", "notice"):
                s[key] = "1" if f.get(key) else "0"
            elif key in f:
                s[key] = f.get(key, "").strip() if key != "script" else f.get(key, "").replace("\r\n", "\n")[:20000]
        problem = None
        try:
            s["count"] = str(max(1, min(MAX_DANCERS, int(s["count"] or 10))))
            s["dance"] = str(max(1, min(MAX_DANCE, int(s["dance"] or 1))))
            s["repeat"] = str(max(0, min(600, int(s["repeat"] or 0))))
            s["countdown"] = str(max(1, min(30, int(s["countdown"] or 3))))
            s["loop"] = str(max(0, min(3600, int(s["loop"] or 0))))
            s["facing_deg"] = str(int(s["facing_deg"] or 0) % 360)
        except ValueError:
            problem = "Liczba tancerzy, taniec, powtórka, odliczanie, pętla i kierunek muszą być liczbami."
        if s["formation"] not in formation_keys:
            s["formation"] = "row"
        if s["transition"] not in ("walk", "run", "jump"):
            s["transition"] = "walk"
        if s["facing"] not in ("owner", "same", "deg"):
            s["facing"] = "owner"
        if s["marker"] not in gesture_keys and s["marker"] != "-":
            s["marker"] = "-"
        if s["daymode"] not in ("keep", "dark", "light"):
            s["daymode"] = "keep"
        if s["snow"] not in ("keep", "on", "off"):
            s["snow"] = "keep"
        if s["anchor"] != "spot":
            s["anchor"] = "owner"
        elif not problem:
            try:
                index, x, y = int(s["map"]), float(s["spot_x"].replace(",", ".")), float(s["spot_y"].replace(",", "."))
            except ValueError:
                problem = "Punkt na mapie: wybierz mapę i wpisz X i Y (liczby, jak w grze)."
            else:
                bounds = m.MAP_BOUNDS.get(index)
                if bounds and not (0 <= x * 100 < bounds[2] and 0 <= y * 100 < bounds[3]):
                    problem = (f"Punkt na mapie: {m.map_name(index)} ma X 0–{bounds[2] // 100} i "
                               f"Y 0–{bounds[3] // 100}.")
        return s, problem

    def show_values(s, command, steps):
        values = {"command": command, "owner": s["owner"], "count": s["count"], "formation": s["formation"],
                  "dance": s["dance"], "repeat": s["repeat"], "quiet": s["quiet"], "anchor": s["anchor"],
                  "facing": s["facing_deg"] if s["facing"] == "deg" else s["facing"], "transition": s["transition"],
                  "countdown": s["countdown"], "notice": s["notice"], "marker": s["marker"], "daymode": s["daymode"],
                  "snow": s["snow"], "loop": s["loop"] if steps else "0"}
        if s["anchor"] == "spot":
            values.update({"map": s["map"], "spot_x": int(round(float(s["spot_x"].replace(",", ".")) * 100)),
                           "spot_y": int(round(float(s["spot_y"].replace(",", ".")) * 100))})
        values["stamp"] = uuid.uuid4().hex[:12]
        text = "".join(f"{k} {v}\n" for k, v in values.items())
        text += "".join(f"step {at} {kind} {arg} {who} {every}\n" for at, kind, arg, who, every in steps)
        return text

    @app.post("/dance/show")
    @m.login_required
    def dance_command():
        if not check():
            return redirect(url_for("dance_show"))
        ensure_table()
        command = request.form.get("command", "")
        if command not in ("gather", "start", "stop", "save_scene", "save"):
            return redirect(url_for("dance_show"))
        s, problem = read_form_settings()
        # What was typed is kept even when it cannot go to the game yet.
        write_atomic(panel_file, json.dumps(s, ensure_ascii=False))
        if problem:
            flash(problem, "error")
            return redirect(url_for("dance_show"))
        steps, errors = parse_script(s["script"], formation_words, gesture_words)
        if errors:
            flash("Skrypt: " + " ".join(errors[:5]) + (" …" if len(errors) > 5 else ""), "error")
            return redirect(url_for("dance_show"))
        if command == "save":
            flash("Zapisano ustawienia (bez zmiany w grze).")
            return redirect(url_for("dance_show"))
        if command == "save_scene":
            name = request.form.get("scene_name", "").strip()[:48]
            if not name:
                flash("Wpisz nazwę sceny.", "error")
                return redirect(url_for("dance_show"))
            roster = m.rows("""SELECT id, slot, name, job, level, weapon, armor, costume, hair, skin, pos_x, pos_y
                               FROM player.playerbot_dancer WHERE slot < 900 ORDER BY slot, id LIMIT %s""",
                            (int(s["count"]),))
            m.rows("""INSERT INTO player.playerbot_dance_scene (name, settings, roster) VALUES (%s,%s,%s)
                      ON DUPLICATE KEY UPDATE settings=VALUES(settings), roster=VALUES(roster)""",
                   (name, json.dumps(s, ensure_ascii=False), json.dumps([dict(r) for r in roster], ensure_ascii=False)))
            flash(f"Zapisano scenę „{name}”: {len(roster)} tancerzy z wyglądem, szyk, miejsce i skrypt.")
            return redirect(url_for("dance_show"))
        if not s["owner"]:
            flash("Wybierz postać, przy której ma stanąć zespół (albo która ogląda pokaz).", "error")
            return redirect(url_for("dance_show"))
        write_atomic(show_file, show_values(s, command, steps))
        quiet = s["quiet"] == "1"
        scripted = bool(s["script"].strip())
        flash({"gather": "Zbieram zespół: tancerze logują się i stają w szyku (kilka–kilkanaście sekund).",
               "start": f"Start! Odliczanie {s['countdown']} s" + (", gest startowy" if s["marker"] != "-" else "")
                        + (f", potem skrypt ({len(steps)} kroków)." if scripted else ", potem taniec u wszystkich naraz."),
               "stop": "Stop: tancerze kończą i wylogowują się."}[command]
              + (" Świat bez botów: grające boty wylogowują się (ok. 20 s) i nie wchodzą, dopóki to pole jest zaznaczone."
                 if quiet else ""))
        return redirect(url_for("dance_show"))

    @app.post("/dance/scene")
    @m.login_required
    def dance_scene():
        if not check():
            return redirect(url_for("dance_show"))
        ensure_table()
        scene = m.one("SELECT id, name, settings, roster FROM player.playerbot_dance_scene WHERE id=%s",
                      (int(request.form.get("scene", "0") or 0),))
        if not scene:
            flash("Nie ma takiej sceny.", "error")
            return redirect(url_for("dance_show"))
        if request.form.get("action") == "delete":
            m.rows("DELETE FROM player.playerbot_dance_scene WHERE id=%s", (scene["id"],))
            flash(f"Usunięto scenę „{scene['name']}”.")
            return redirect(url_for("dance_show"))
        try:
            settings = {k: str(v) for k, v in json.loads(scene["settings"]).items() if k in SETTINGS}
            dancers = json.loads(scene["roster"])
        except ValueError:
            flash("Scena jest uszkodzona.", "error")
            return redirect(url_for("dance_show"))
        # The scene's dancers first, in its order; the rest of the roster
        # stays, at the back (900+), out of the count.
        kept, notes = [], []
        for place, d in enumerate(dancers, 1):
            row = m.one("SELECT id, job FROM player.playerbot_dancer WHERE id=%s", (int(d.get("id", 0)),))
            values = (place, int(d.get("level", 1)), int(d.get("weapon", 0)), int(d.get("armor", 0)),
                      int(d.get("costume", 0)), int(d.get("hair", 0)), int(d.get("skin", 0)),
                      int(d.get("pos_x", 0)), int(d.get("pos_y", 0)))
            if row and int(row["job"]) == int(d.get("job", 0)):
                m.rows("UPDATE player.playerbot_dancer SET slot=%s, level=%s, weapon=%s, armor=%s, costume=%s, hair=%s, "
                       "skin=%s, pos_x=%s, pos_y=%s WHERE id=%s", values + (row["id"],))
                if not name_problem(str(d.get("name", "")), row["id"]):
                    m.rows("UPDATE player.playerbot_dancer SET name=%s WHERE id=%s", (d["name"], row["id"]))
                kept.append(int(row["id"]))
            elif not name_problem(str(d.get("name", ""))) and int(d.get("job", 0)) in job_names:
                m.rows("INSERT INTO player.playerbot_dancer (slot, level, weapon, armor, costume, hair, skin, pos_x, pos_y, "
                       "name, job, status) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,'czeka na Zbierz')",
                       values + (d["name"], int(d["job"])))
                kept.append(int(m.one("SELECT id FROM player.playerbot_dancer WHERE name=%s", (d["name"],))["id"]))
                notes.append(f"{d['name']} dodany na nowo")
            else:
                notes.append(f"{d.get('name', '?')} pominięty (nick zajęty albo inna klasa)")
        others = m.rows("SELECT id FROM player.playerbot_dancer WHERE id NOT IN (" +
                        ",".join(["%s"] * len(kept)) + ") ORDER BY slot, id", kept) if kept else \
            m.rows("SELECT id FROM player.playerbot_dancer ORDER BY slot, id")
        for i, r in enumerate(others):
            m.rows("UPDATE player.playerbot_dancer SET slot=%s WHERE id=%s", (min(999, 900 + i), r["id"]))
        settings["count"] = str(max(1, len(kept)))
        merged = dict(SETTINGS)
        merged.update(settings)
        write_atomic(panel_file, json.dumps(merged, ensure_ascii=False))
        flash(f"Wczytano scenę „{scene['name']}”: {len(kept)} tancerzy na początku zespołu, ustawienia w sterowaniu. "
              "Kliknij „Zbierz”, żeby stanęli." + (" " + "; ".join(notes) + "." if notes else ""))
        return redirect(url_for("dance_show"))
