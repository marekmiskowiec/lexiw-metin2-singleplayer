"""Lexiw: "Decyzje botów" - the bots' flagged decisions of the last hours, as
the classic panel's /decisions page had them (LEXIW_ONE_PANEL_V1): equipment
swaps and shop listings, by the core's flags or by the anomalies read out of
a row, for every bot or one, each with its full explanation.

The data and the explanations are the classic panel's own code, carried over
in decisions_classic.py; this module is the page around them.

install_decisions_page(module) is called from app.py.
"""
import datetime

from flask import flash, render_template, request

import decisions_classic as dc


def install_decisions_page(m):
    app = m.app
    dc.db = m.db

    @app.route("/players/decisions")
    @m.login_required
    def decisions_page():
        language = "pl"
        lang_key = dc._dx_lang(language)
        messages = dc.map_i18n(language)
        try:
            hours = int(request.args.get("hours", 24))
        except (TypeError, ValueError):
            hours = 24
        if hours not in dc.DECISION_PAGE_HOURS:
            hours = 24
        kind = request.args.get("kind", "all")
        if kind not in ("all", "equip", "listing"):
            kind = "all"
        flag = request.args.get("flag", "unusual")
        anomaly = flag == "anomaly"
        equip_mask, listing_mask = dc.decision_page_masks(flag)
        if not anomaly and (equip_mask, listing_mask) == (dc.DECISION_EQUIP_UNUSUAL, dc.DECISION_LISTING_UNUSUAL):
            flag = "unusual"
        bot_text = (request.args.get("bot") or "").strip()[:32]
        rows, states, capped, meta = [], [], False, {}
        try:
            with m.db() as c, c.cursor() as cur:
                pid, found_bot = dc.decision_page_bot(cur, bot_text)
                if not found_bot:
                    flash(f"Nie ma bota „{bot_text}”.", "error")
                pid_sql = " AND e.pid = %s" if pid else ""
                pid_args = (pid,) if pid else ()
                select_equip = ("SELECT e.*, CAST(p.name AS BINARY) AS pname, p.level AS plevel, p.job AS pjob, "
                                "p.skill_group AS psg FROM log.playerbot_equip e LEFT JOIN player.player p ON p.id = e.pid ")
                if found_bot and kind in ("all", "equip") and equip_mask:
                    if anomaly:
                        sql = (select_equip + "WHERE e.`time` >= NOW() - INTERVAL %s HOUR AND ((e.wear = 4 AND e.old_vnum <> 0 "
                               "AND e.new_vnum <> 0) OR e.context = 5)" + pid_sql + " ORDER BY e.`time` DESC LIMIT %s")
                        args, cap = (hours,) + pid_args + (dc.DECISION_PAGE_SCAN,), dc.DECISION_PAGE_SCAN
                    else:
                        sql = (select_equip + "WHERE e.`time` >= NOW() - INTERVAL %s HOUR AND (e.flags & %s) <> 0" + pid_sql +
                               " ORDER BY e.`time` DESC LIMIT %s")
                        args, cap = (hours, equip_mask) + pid_args + (dc.DECISION_PAGE_LIMIT,), dc.DECISION_PAGE_LIMIT
                    found, state = dc._explain_rows(cur, sql, args)
                    states.append(state)
                    capped = capped or len(found) >= cap
                    for r in found:
                        why = dc.explain_equip(r, language)
                        if anomaly and not why["anomalies"]:
                            continue
                        piece = why["new"] or why["old"] or {}
                        what = piece.get("name", "")
                        if why["new"] and why["old"]:
                            what += " " + messages["gh_instead"].format(item=why["old"]["name"])
                        meta.setdefault(why["pid"], r)
                        rows.append({"_t": r.get("time"), "time": dc._dx_time(r.get("time")),
                                     "pid": why["pid"], "name": dc.log_text(r.get("pname")).strip() or "#%d" % why["pid"],
                                     "kind_label": "Wyposażenie", "item": piece.get("name", ""),
                                     "what": what + " · " + why["summary"], "unusual": why["unusual"], "why": why})
                pid_sql = " AND l.pid = %s" if pid else ""
                select_listing = ("SELECT l.*, CAST(p.name AS BINARY) AS pname, p.level AS plevel, p.job AS pjob, "
                                  "p.skill_group AS psg FROM log.playerbot_listing l LEFT JOIN player.player p ON p.id = l.pid ")
                if found_bot and kind in ("all", "listing") and listing_mask:
                    if anomaly:
                        tools = " OR ".join("l.why_b BETWEEN %d AND %d" % pair for pair in dc.DECISION_TOOL_RANGES)
                        sql = (select_listing + "WHERE l.last_at >= NOW() - INTERVAL %s HOUR AND l.why = 17 AND (" + tools +
                               ")" + pid_sql + " ORDER BY l.last_at DESC LIMIT %s")
                        args = (hours,) + pid_args + (dc.DECISION_PAGE_LIMIT,)
                    else:
                        sql = (select_listing + "WHERE l.last_at >= NOW() - INTERVAL %s HOUR AND (l.flags & %s) <> 0" + pid_sql +
                               " ORDER BY l.last_at DESC LIMIT %s")
                        args = (hours, listing_mask) + pid_args + (dc.DECISION_PAGE_LIMIT,)
                    found, state = dc._explain_rows(cur, sql, args)
                    states.append(state)
                    capped = capped or len(found) >= dc.DECISION_PAGE_LIMIT
                    for r in found:
                        entry = dc.gear_history_listing_entry(r, language, messages)[1]
                        why = entry["why"]
                        if anomaly and not why["anomalies"]:
                            continue
                        meta.setdefault(why["pid"], r)
                        rows.append({"_t": r.get("last_at") or r.get("listed_at"),
                                     "time": dc._dx_time(r.get("last_at") or r.get("listed_at")),
                                     "pid": why["pid"], "name": dc.log_text(r.get("pname")).strip() or "#%d" % why["pid"],
                                     "kind_label": "Sklep", "item": entry["item"],
                                     "what": " · ".join(p for p in (entry["item"], entry["label"], entry["detail"]) if p),
                                     "unusual": why["unusual"], "why": why})
        except Exception:
            app.logger.exception("decisions page")
            flash("Baza danych nie odpowiada.", "error")
        rows.sort(key=lambda e: e["_t"] if hasattr(e["_t"], "strftime") else datetime.datetime.min, reverse=True)
        capped = capped or len(rows) > dc.DECISION_PAGE_LIMIT
        rows = rows[:dc.DECISION_PAGE_LIMIT]
        for row in rows:
            del row["_t"]
        shown = {row["pid"] for row in rows}
        bots = dc.decision_page_bots({p: r for p, r in meta.items() if p in shown}, language) if rows else {}
        flag_choices = [("unusual", "nietypowe"), ("any", "wszystkie oznaczone"), ("anomaly", "anomalie (⚑)")]
        flag_choices += [("e%d" % bit, "Wyposażenie: " + dc._dx_pick(texts, lang_key))
                         for bit, texts in sorted(dc.DECISION_EFLAGS.items())]
        flag_choices += [("l%d" % bit, "Sklep: " + dc._dx_pick(texts, lang_key))
                         for bit, texts in sorted(dc.DECISION_LFLAGS.items())]
        return render_template(
            "decisions.html", rows=rows, messages=messages, bots=bots, bot_text=bot_text,
            missing=bool(states) and all(s == "missing" for s in states), capped=capped,
            state_text=dc.explain_state_text(), hours=hours, kind=kind, flag=flag,
            hour_choices=dc.DECISION_PAGE_HOURS,
            kind_choices=[("all", "wszystkie"), ("equip", "wyposażenie"), ("listing", "sklep")],
            flag_choices=flag_choices, explain_css=dc.EXPLAIN_CSS, explain_js=dc.EXPLAIN_JS)
