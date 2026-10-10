"""Lexiw: a smoke test of the panel - opens every page that can be opened
without changing anything and fails on any server error.

Run it inside the panel's container, where the database and the game's files
are:

    docker compose exec seban-panel python smoke_test.py

It logs in through the app's own test client (the session flag the login
sets), asks every GET route in turn - the ones with a number or a name in
the address get a real one from the database - and reports status and time.
500s (and pages that fail to render) are failures; a 404 on a page that was
given a made-up id is only noted. Routes that act when merely opened
(logout, restarts, downloads, the update and reset pages' actions) are
skipped by name and by method - only GET routes are ever called.
Exit code 1 if anything failed, so it fits a script or a CI line.
"""
import re
import sys
import time

import app as panel

# Endpoints (or fragments of their address) that do something when opened.
SKIP = re.compile(r"logout|restart|reset|delete|download|export|backup|teleport|update|updater|stop|start|"
                  r"shutdown|apply|grant|takeover|/api/admin|events/|static")


def first(sql, default=None):
    try:
        row = panel.one(sql)
        return next(iter(row.values())) if row else default
    except Exception:
        return default


def sample_values():
    """Real ids for the routes that take one."""
    pid = first("SELECT id FROM player.player WHERE name NOT LIKE '[%%' ORDER BY id LIMIT 1", 1)
    return {
        "pid": pid,
        "account_id": first("SELECT id FROM account.account ORDER BY id LIMIT 1", 1),
        "guild_id": first("SELECT id FROM player.guild ORDER BY id LIMIT 1", 1),
        "vnum": 16,
        "mob": 182,
        "number": 1,
        "key": "m1",
        "id": 1,
        "gid": 1,
    }


def arguments(rule, samples):
    values = {}
    for name in rule.arguments:
        values[name] = samples.get(name, samples["id"])
    # Wiki and market pages take a monster's vnum in some routes and an item's in others.
    if rule.endpoint == "wiki_mob":
        values["vnum"] = samples["mob"]
    return values


def main():
    samples = sample_values()
    app = panel.app
    app.config["TESTING"] = False  # keep real error pages, so a failure shows as 500
    client = app.test_client()
    with client.session_transaction() as session:
        session["seban_admin"] = True
    failures, notes, count = [], [], 0
    started = time.time()
    for rule in sorted(app.url_map.iter_rules(), key=lambda r: r.rule):
        if "GET" not in rule.methods or SKIP.search(rule.rule) or SKIP.search(rule.endpoint):
            continue
        try:
            path = panel.url_for_rule(rule, arguments(rule, samples)) if hasattr(panel, "url_for_rule") else None
        except Exception:
            path = None
        if path is None:
            with app.test_request_context():
                try:
                    from flask import url_for
                    path = url_for(rule.endpoint, **arguments(rule, samples))
                except Exception as error:  # a route whose arguments we cannot guess
                    notes.append(f"? {rule.rule}: no address ({error.__class__.__name__})")
                    continue
        t0 = time.time()
        try:
            response = client.get(path, follow_redirects=False)
            status = response.status_code
        except Exception as error:
            status, response = 599, None
            failures.append(f"✗ 599 {path}: {error.__class__.__name__}: {error}")
            print(f"599 {path}")
            count += 1
            continue
        took = int((time.time() - t0) * 1000)
        count += 1
        if status >= 500:
            failures.append(f"✗ {status} {path}")
        elif status == 404 and rule.arguments:
            notes.append(f"? 404 {path} (made-up id)")
        elif status in (301, 302) and "/login" in (response.headers.get("Location") or ""):
            failures.append(f"✗ {status} {path} -> login (session not accepted)")
        print(f"{status} {took:5d} ms  {path}")
    print(f"\n{count} pages in {time.time() - started:.1f} s")
    for note in notes:
        print(note)
    if failures:
        print("\nFAILED:")
        for failure in failures:
            print(" ", failure)
        return 1
    print("OK: no server errors")
    return 0


if __name__ == "__main__":
    sys.exit(main())
