"""Lexiw: "Moduły świata" - the world switches only the classic panel had,
brought over so it can go (LEXIW_WORLD_MODULES_V1): the Arezzo module,
Seon-Hae's 6th/7th bonus with its drops, the Dragon Stone shards, the health
of monsters, the Flower Event and the Easter event's two switches.

Each is what the classic panel wrote: an event flag in player.quest (what the
db core loads at a start) plus a row of web_admin_queue that web_admin.quest
applies live within seconds; the Flower Event and Seon-Hae's drops are files
in /opt/m2spool that the cores re-read when they change.

install_world_modules(module) is called from app.py.
"""
from flask import flash, redirect, render_template, request, url_for

FLOWER_KEYS = (
    # key, default, lowest, highest, label
    ("seed_chance", 100, 0, 10000, "Szansa na nasiono z potwora (setne części procenta, 100 = 1%)"),
    ("min_level", 1, 1, 250, "Najniższy poziom gracza"),
    ("counter_max", 99999, 1, 999999, "Najwięcej nasion / latorośli jednego rodzaju"),
    ("seeds_per_shoot", 1, 1, 1000, "Nasion za 1 latorośl"),
    ("shoots_per_reward", 10, 1, 1000, "Latorośli jednego kwiatu za 1 nagrodę"),
    ("add_rate", 50, 0, 100, "Szansa – pierwszy kwiat (%)"),
    ("change_rate", 30, 0, 100, "Szansa – zamiana na inny kwiat (%)"),
    ("upgrade_rate", 15, 0, 100, "Szansa – ten sam kwiat, poziom wyżej (%)"),
    ("max_level", 5, 1, 20, "Najwyższy poziom bonusu kwiatu"),
)
FLOWER_NAMES = ("Chryzantema", "Konwalia", "Narcyz", "Lilia", "Słonecznik")
FLOWER_DEFAULT_REWARDS = {1: (83023, 1), 2: (83024, 1), 3: (83025, 1), 4: (83026, 1), 5: (83027, 1)}
SEONHAE_WAIT_MAX = 10080
RARE_LEVEL_DEFAULT = (35, 30, 20, 10, 5)
SEONHAE_SHARDS = set(range(39070, 39078)) | {39081}
SEONHAE_ADDITIVES = {72064, 72065, 72066, 72067}
SEONHAE_DROPS_DEFAULT = """# Seon-Hae drops. Real players only, only while Seon-Hae is on.
# ADDITIVES - Metins and bosses (rank boss/king) of these maps, each kill rolls additive_chance:
additive_chance 60
# additive_map <map> <additive vnum> <count>: Grotto of Exile V1 + V2 - 1 Medium, Enchanted Forest - 1 Large
additive_map 72 72065 1
additive_map 73 72065 1
additive_map 362 72066 1
# mob <mob vnum> <additive vnum> <count>: one monster (any rank, any map), instead of its map's rule
mob 2493 72066 2
mob 9714 72066 2
# SHARDS - ordinary monsters (not Metins, not bosses) of these maps, 1 shard per hit of shard_chance %:
shard_chance 0.5
shard_map 73
shard_map 209
shard_map 362
# shard <vnum> <weight>: the shard's colour
shard 39070 1
shard 39071 1
shard 39072 1
shard 39073 1
shard 39074 1
shard 39075 1
shard 39076 1
shard 39077 1
shard 39081 1
"""
MOB_HP_MIN, MOB_HP_MAX = 10, 300


def install_world_modules(m):
    app = m.app
    flower_file = m.RATES_SPOOL / "flower_event.tsv"
    seonhae_file = m.RATES_SPOOL / "seonhae_drops.tsv"

    def flags(names):
        marks = ",".join(["%s"] * len(names))
        found = {}
        for row in m.rows(f"SELECT szName, lValue FROM player.quest WHERE dwPID=0 AND szName IN ({marks})", tuple(names)):
            name = row["szName"].decode("ascii", "replace") if isinstance(row["szName"], bytes) else row["szName"]
            try:
                found[name] = int(row["lValue"])
            except (TypeError, ValueError):
                pass
        return found

    def set_flags(values):
        for name, value in values.items():
            m.rows("REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES (0, %s, '', %s)", (name, int(value)))

    def go_live(cmd, arg1, arg2="", what="Zapisano"):
        """The row web_admin.quest turns into the live flags; the flags above stay for a restart."""
        try:
            status, _qid = m.queue_player_admin_command("", cmd, arg1, arg2, wait=12.0)
        except Exception:
            status = "failed"
        if status == "done":
            flash(f"{what} i włączono od razu w grze.")
        else:
            flash(f"{what} w bazie. Gra nie potwierdziła zmiany na żywo (serwer wyłączony albo dopiero startuje) — zadziała po najbliższym starcie.")

    def read_flower():
        cur = {k: d for k, d, _lo, _hi, _l in FLOWER_KEYS}
        cur["use_after_event"] = 1
        cur["rewards"] = dict(FLOWER_DEFAULT_REWARDS)
        try:
            lines = flower_file.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return cur
        bounds = {k: (lo, hi) for k, _d, lo, hi, _l in FLOWER_KEYS}
        for line in lines:
            f = line.split("#", 1)[0].split()
            if len(f) < 2:
                continue
            try:
                nums = [int(x) for x in f[1:4]]
            except ValueError:
                continue
            if f[0] in bounds:
                lo, hi = bounds[f[0]]
                cur[f[0]] = max(lo, min(hi, nums[0]))
            elif f[0] == "use_after_event":
                cur["use_after_event"] = 1 if nums[0] else 0
            elif f[0] == "reward" and len(nums) == 3 and 1 <= nums[0] <= 5 and nums[1] > 0 and 1 <= nums[2] <= 1000:
                cur["rewards"][nums[0]] = (nums[1], nums[2])
        return cur

    def write_text(path, text):
        tmp = path.with_suffix(path.suffix + ".tmp")
        tmp.write_text(text, encoding="utf-8", newline="\n")
        tmp.replace(path)

    def check_seonhae_drops(text):
        """The line numbers the core would skip (its parser's rules)."""
        bad = []
        for no, raw in enumerate(text.splitlines(), 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            key, args = line.split()[0], line.split()[1:]
            ok = False
            try:
                if key in ("additive_chance", "shard_chance"):
                    ok = len(args) == 1 and 0.0 <= float(args[0]) <= 100.0
                else:
                    nums = [int(x) for x in args]
                    if key in ("additive_map", "mob"):
                        ok = len(nums) == 3 and nums[0] > 0 and nums[1] in SEONHAE_ADDITIVES and 1 <= nums[2] <= 50
                    elif key == "shard_map":
                        ok = len(nums) == 1 and nums[0] > 0
                    elif key == "shard":
                        ok = len(nums) == 2 and nums[0] in SEONHAE_SHARDS and nums[1] > 0
            except ValueError:
                ok = False
            if not ok:
                bad.append(no)
        return bad

    def number(name, default, lo, hi):
        try:
            return max(lo, min(hi, int(request.form.get(name, default))))
        except (TypeError, ValueError):
            return default

    @app.route("/manage/world")
    @m.login_required
    def world_modules():
        f = flags(["mt2009_arezzo_closed", "m2_seonhae_on", "m2_seonhae_wait_min", "m2_alchemy_off", "m2_sash_off",
                   "ds_drop", "ds_cor_day", "m2_mob_hp", "easter_drop", "easter_rabbit"]
                  + ["m2_rare_lv%d" % i for i in range(1, 6)])
        lv = [max(0, min(100, f.get("m2_rare_lv%d" % i, 0))) for i in range(1, 6)]
        hp = f.get("m2_mob_hp", 0)
        state = {
            # no row: Arezzo off (the module is voluntary), Seon-Hae off, alchemy and sashes on
            "arezzo": f.get("mt2009_arezzo_closed", 1) <= 0,
            "seonhae": f.get("m2_seonhae_on", 0) == 1,
            "seonhae_wait": max(0, min(SEONHAE_WAIT_MAX, f.get("m2_seonhae_wait_min", 0))),
            "seonhae_lv": lv if sum(lv) > 0 else list(RARE_LEVEL_DEFAULT),
            "alchemy": f.get("m2_alchemy_off", 0) <= 0,
            "sashes": f.get("m2_sash_off", 0) <= 0,
            "ds_drop": f["ds_drop"] if 1 <= f.get("ds_drop", 0) <= 100 else 10,
            "ds_cor_day": f["ds_cor_day"] if 1 <= f.get("ds_cor_day", 0) <= 20 else 5,
            "mob_hp": 100 if hp <= 0 else max(MOB_HP_MIN, min(MOB_HP_MAX, hp)),
            "easter_drop": f.get("easter_drop", 0) > 0,
            "easter_rabbit": f.get("easter_rabbit", 0) > 0,
        }
        try:
            drops = seonhae_file.read_text(encoding="utf-8", errors="replace")
        except OSError:
            drops = SEONHAE_DROPS_DEFAULT
        return render_template("world_modules.html", s=state, flower=read_flower(), flower_keys=FLOWER_KEYS,
                               flower_names=FLOWER_NAMES, seonhae_drops=drops)

    def back(anchor):
        return redirect(url_for("world_modules") + "#" + anchor)

    @app.post("/manage/world/arezzo")
    @m.login_required
    def world_arezzo():
        on = request.form.get("on") == "1"
        set_flags({"mt2009_arezzo_closed": 0 if on else 1})
        go_live("AREZZO", "1" if on else "0", what="Arezzo " + ("włączone" if on else "wyłączone") + ": zapisano")
        return back("arezzo")

    @app.post("/manage/world/seonhae")
    @m.login_required
    def world_seonhae():
        on = request.form.get("on") == "1"
        wait = number("wait", 0, 0, SEONHAE_WAIT_MAX)
        lv = [number("lv%d" % i, 0, 0, 100) for i in range(1, 6)]
        if sum(lv) <= 0:
            lv = list(RARE_LEVEL_DEFAULT)
        drops = request.form.get("drops")
        if drops is not None:
            text = drops.replace("\r\n", "\n").strip()
            bad = check_seonhae_drops(text)
            if bad:
                flash("Nie zapisano reguł dropu — błędne wiersze: " + ", ".join(str(n) for n in bad[:20]), "error")
            elif text:
                write_text(seonhae_file, text + "\n")
            else:
                seonhae_file.unlink(missing_ok=True)
        values = {"m2_seonhae_on": 1 if on else 0, "m2_seonhae_wait_min": wait}
        values.update({"m2_rare_lv%d" % (i + 1): v for i, v in enumerate(lv)})
        set_flags(values)
        go_live("SEONHAE", "%d,%d" % (1 if on else 0, wait), ",".join(str(v) for v in lv), what="Seon-Hae: zapisano")
        return back("seonhae")

    @app.post("/manage/world/dragon-stone")
    @m.login_required
    def world_dragon_stone():
        alchemy = request.form.get("alchemy") == "1"
        sashes = request.form.get("sashes") == "1"
        ds_drop = number("ds_drop", 10, 1, 100)
        ds_cor_day = number("ds_cor_day", 5, 1, 20)
        set_flags({"m2_alchemy_off": 0 if alchemy else 1, "m2_sash_off": 0 if sashes else 1,
                   "ds_drop": ds_drop, "ds_cor_day": ds_cor_day})
        # the same pair the Gra i serwer page keeps for the updater's rules
        m.write_settings({"allow_alchemy": "1" if alchemy else "0", "allow_sashes": "1" if sashes else "0"})
        go_live("RARE", "%d,%d" % (alchemy, sashes), "%d,%d" % (ds_drop, ds_cor_day), what="Alchemia i szarfy: zapisano")
        return back("smocze-kamienie")

    @app.post("/manage/world/mob-hp")
    @m.login_required
    def world_mob_hp():
        value = number("pct", 100, MOB_HP_MIN, MOB_HP_MAX)
        set_flags({"m2_mob_hp": value})
        go_live("MOB_HP", str(value), what=f"Życie potworów {value}%: zapisano")
        return back("zycie-potworow")

    @app.post("/manage/world/easter")
    @m.login_required
    def world_easter():
        drop = request.form.get("drop") == "1"
        rabbit = request.form.get("rabbit") == "1"
        set_flags({"easter_drop": int(drop), "easter_rabbit": int(rabbit)})
        go_live("EASTER", "%d,%d" % (drop, rabbit), what="Wielkanoc: zapisano")
        return back("wielkanoc")

    @app.post("/manage/world/flower")
    @m.login_required
    def world_flower():
        cur = read_flower()
        for k, d, lo, hi, _l in FLOWER_KEYS:
            cur[k] = number(k, cur[k], lo, hi)
        cur["use_after_event"] = 1 if request.form.get("use_after_event") == "1" else 0
        for i in range(1, 6):
            try:
                vnum = int(request.form.get("reward_vnum_%d" % i, cur["rewards"][i][0]))
                count = max(1, min(1000, int(request.form.get("reward_count_%d" % i, cur["rewards"][i][1]))))
            except (TypeError, ValueError):
                continue
            # an item number the world lacks would only be skipped by the core
            if vnum > 0 and m.one("SELECT vnum FROM player.item_proto WHERE vnum=%s", (vnum,)):
                cur["rewards"][i] = (vnum, count)
            elif vnum > 0:
                flash(f"Nie ma przedmiotu o numerze {vnum} — nagroda za {FLOWER_NAMES[i - 1]} bez zmian.", "error")
        body = ["# MT2009_PLUS_FLOWER_V1 - Dzieci Kwiaty, written by the panel (playerbot_flower.h reads it)"]
        body += ["%s %d" % (k, cur[k]) for k, _d, _lo, _hi, _l in FLOWER_KEYS]
        body.append("use_after_event %d" % cur["use_after_event"])
        body += ["reward %d %d %d" % (i, *cur["rewards"][i]) for i in range(1, 6)]
        try:
            write_text(flower_file, "\n".join(body) + "\n")
            flash("Dzieci Kwiaty: zapisano — serwer użyje nowych ustawień w ciągu 5 sekund.")
        except OSError:
            flash("Nie udało się zapisać pliku ustawień Dzieci Kwiatów.", "error")
        return back("dzieci-kwiaty")
