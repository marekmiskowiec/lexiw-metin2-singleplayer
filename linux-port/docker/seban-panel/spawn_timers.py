"""Lexiw: "Timery spawnów" - where the Metins, bosses, ore veins and herbs
stand now, and when the empty points will be filled (LEXIW_SPAWN_TIMERS_V1).

Every core writes /opt/m2spool/world_spawns.<channel>.<port>.tsv every ten
seconds
(playerbot_spawn_export.h): its respawn lines that hold one of these, each
with the seconds to its next tick, and every such monster standing now with
its position. The page lists them per map, with the coordinates the game
shows and a /warp command to go there.

install_spawn_timers(module) is called from app.py.
"""
import time

from flask import jsonify, render_template

KINDS = (("metin", "Metiny"), ("boss", "Bossowie"), ("miniboss", "Minibossy"), ("ore", "Rudy"), ("flower", "Kwiaty"))
RANK = {"boss": 6, "metin": 5, "miniboss": 4, "ore": 3, "flower": 3}
STALE_SECONDS = 60


def install_spawn_timers(m):
    app = m.app

    def kind_of(mob):
        name = (mob.get("name") or "").strip().lower()
        if mob["kind"] == "metin":
            return "metin"
        if mob["kind"] == "boss":
            return "boss" if int(mob.get("rank") or 0) >= 5 else "miniboss"
        if name.startswith("żyła"):
            return "ore"
        if "kwiat" in name or "grzyb" in name:
            return "flower"
        return None

    def read_files():
        files = []
        for path in sorted(m.RATES_SPOOL.glob("world_spawns.*.tsv")):
            # A core that is gone (or the one-file-per-channel format before it)
            # leaves a file that no longer changes: not the world any more.
            try:
                if time.time() - path.stat().st_mtime > STALE_SECONDS * 2:
                    continue
            except OSError:
                continue
            try:
                channel = int(path.name.split(".")[1])
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except (ValueError, OSError):
                continue
            files.append((channel, lines))
        return files

    def state():
        mobs = m.drop_mob_rows()
        now = time.time()
        points, written = [], {}
        for channel, lines in read_files():
            stamp = 0
            regens, alive = [], {}
            for line in lines:
                t = line.split("\t")
                if t[0] == "T" and len(t) > 1:
                    stamp = int(t[1])
                elif t[0] == "R" and len(t) >= 12:
                    regens.append(t)
                elif t[0] == "A" and len(t) >= 6:
                    alive.setdefault((int(t[1]), int(t[2])), []).append((int(t[3]), int(t[4]), t[5]))
            written[channel] = stamp
            # Lines may overlap (two Metin lines on one spot): each monster
            # counts once, for the tightest line holding it, and a line takes
            # no more than its own count says stands - none when it is 0.
            regens.sort(key=lambda t: (int(t[4]) - int(t[2])) * (int(t[5]) - int(t[3])))
            taken = set()
            for t in regens:
                number = int(t[1])
                sx, sy, ex, ey = (int(v) for v in t[2:6])
                members = [int(v) for v in t[11].split(",") if v.isdigit()]
                known = [(v, mobs[v], kind_of(mobs[v])) for v in members if v in mobs]
                known = [k for k in known if k[2]]
                if not known:
                    continue
                vnum, lead, kind = max(known, key=lambda k: (RANK[k[2]], k[1]["level"]))
                here = []
                room = int(t[8]) * (1 if t[6] == "0" else len(known))
                for v, mob, _k in known:
                    for x, y, vid in alive.get((number, v), []):
                        if len(here) < room and vid not in taken and sx - 10 <= x <= ex + 10 and sy - 10 <= y <= ey + 10:
                            taken.add(vid)
                            here.append({"x": x, "y": y, "name": mob["name"], "vnum": v})
                if here:  # name the row after what stands there, not the best it may roll
                    vnum = here[0]["vnum"]
                    lead = mobs[vnum]
                bounds = m.MAP_BOUNDS.get(number)
                for spot in here:
                    if bounds:
                        spot["warp"] = f"/warp {bounds[0] // 100 + spot['x']} {bounds[1] // 100 + spot['y']}"
                next_s = int(t[10])
                points.append({
                    "channel": channel, "map": number, "map_name": m.map_name(number), "kind": kind,
                    "name": lead["name"], "level": lead["level"], "vnum": vnum,
                    "names": sorted({mob["name"] for _v, mob, _k in known}),
                    "area": f"X {sx}–{ex}, Y {sy}–{ey}", "center": ((sx + ex) // 2, (sy + ey) // 2),
                    "center_warp": f"/warp {bounds[0] // 100 + (sx + ex) // 2} {bounds[1] // 100 + (sy + ey) // 2}" if bounds else "",
                    "alive": here, "count": int(t[8]), "max": int(t[9]),
                    # the tick as an epoch, so the page counts down by itself
                    "next_at": int(stamp + next_s) if next_s >= 0 and stamp else None,
                })
        kind_order = {k: i for i, (k, _l) in enumerate(KINDS)}
        points.sort(key=lambda p: (p["map_name"], p["channel"], kind_order[p["kind"]], p["name"]))
        fresh = bool(written) and all(now - stamp < STALE_SECONDS for stamp in written.values())
        return {"points": points, "written": written, "fresh": fresh, "now": int(now)}

    @app.route("/timers")
    @m.login_required
    def spawn_timers():
        data = state()
        maps = sorted({(p["map"], p["map_name"]) for p in data["points"]}, key=lambda x: x[1])
        counts = {k: sum(1 for p in data["points"] if p["kind"] == k) for k, _l in KINDS}
        return render_template("spawn_timers.html", data=data, kinds=KINDS, maps=maps, counts=counts)

    @app.get("/api/timers")
    @m.login_required
    def api_spawn_timers():
        return jsonify(state())
