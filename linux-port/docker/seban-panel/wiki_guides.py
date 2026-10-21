"""Lexiw: Wiki, stage 5 - guides written as plain Markdown files.

Every file in wiki_guides/ is one guide, shown at /wiki/guide/<file name>.
A guide starts with a small header between two "---" lines:

    ---
    title: Bossowie i Metiny
    summary: One sentence for the list and the search.
    order: 20
    ---

and goes on in Markdown (tables, code blocks, headings). Links to the wiki
itself are written in double brackets and are checked when the page is drawn:

    [[item:50011]]   an item          [[item:50011|text to show]]
    [[mob:101]]      a monster        [[map:63]]  a map
    [[region:m1]]    a map family     [[guide:jak-uzywac-wiki]]  another guide
    [[page:mapy]]    the maps list, [[page:osobowosci-botow]], [[page:gielda]], [[page:aktywnosc-botow]], [[page:wiki]], [[page:respawny]]

An unknown number is drawn as red text, so a typo does not go unnoticed.
New or changed files are picked up on the next request after the panel image is
rebuilt (the folder is part of the image).

install_guides(module) is called from app.py the way install_wiki is.
"""
import html
import re
from pathlib import Path

import markdown
from flask import abort, render_template

GUIDE_DIR = Path(__file__).parent / "wiki_guides"
LINK_RE = re.compile(r"\[\[(item|mob|map|region|guide|page):([\w-]+)(?:\|([^\]]+))?\]\]")
CODE_RE = re.compile(r"(```.*?```|`[^`\n]+`)", re.S)
SLUG_RE =re.compile(r"^[a-z0-9][a-z0-9-]*$")
# [[page:x]]: panel pages a guide may point to (the wiki's own and the ones about the bots and the market).
PAGES = {"wiki": "wiki", "mapy": "wiki_maps", "osobowosci-botow": "wiki_bots", "gielda": "market", "aktywnosc-botow": "bot_activity", "wiadomosci": "world_feed", "respawny": "respawns"}

_cache = {"key": None, "guides": {}}


def _parse(path):
    text = path.read_text(encoding="utf-8").lstrip("﻿").replace("\r\n", "\n")
    meta, body = {}, text
    if text.startswith("---\n"):
        end = text.find("\n---", 4)
        if end != -1:
            for line in text[4:end].split("\n"):
                key, _, value = line.partition(":")
                if value:
                    meta[key.strip().lower()] = value.strip()
            body = text[end + 4:].lstrip("\n")
    try:
        order = int(meta.get("order", 100))
    except ValueError:
        order = 100
    return {"slug": path.stem, "title": meta.get("title") or path.stem.replace("-", " ").capitalize(),
            "summary": meta.get("summary", ""), "order": order, "body": body}


def load_guides():
    """{slug: guide}, re-read when a file is added, changed or removed."""
    try:
        files = sorted(p for p in GUIDE_DIR.glob("*.md") if SLUG_RE.match(p.stem))
    except OSError:
        files = []
    key = tuple((p.name, p.stat().st_mtime_ns) for p in files)
    if key != _cache["key"]:
        guides = {}
        for path in files:
            try:
                guides[path.stem] = _parse(path)
            except (OSError, UnicodeDecodeError):
                continue
        _cache.update(key=key, guides=guides)
    return _cache["guides"]


def guide_list():
    return sorted(load_guides().values(), key=lambda g: (g["order"], g["title"].lower()))


def search_guides(query, limit=8):
    """Guides whose title, summary or text has the query in it (title first)."""
    folded = query.strip().lower()
    if len(folded) < 2:
        return []
    found = []
    for guide in guide_list():
        in_title = folded in guide["title"].lower()
        if in_title or folded in guide["summary"].lower() or folded in guide["body"].lower():
            found.append((not in_title, guide))
    found.sort(key=lambda pair: pair[0])
    return [{"slug": g["slug"], "title": g["title"], "summary": g["summary"]} for _, g in found[:limit]]


def install_guides(m):
    """m: app.py itself."""
    from flask import url_for
    app = m.app

    def resolve(kind, ref, label):
        """(url, shown text) for one [[kind:ref]], or None when there is no such thing."""
        if kind == "item" and ref.isdigit():
            row = m.one("SELECT name, locale_name FROM player.item_proto WHERE vnum=%s", (int(ref),))
            if row:
                return url_for("wiki_item", vnum=int(ref)), label or m.game_text(row.get("locale_name") or row.get("name"))
        elif kind == "mob" and ref.isdigit():
            mob = m.drop_mob_rows().get(int(ref))
            if mob:
                return url_for("wiki_mob", vnum=int(ref)), label or mob["name"]
        elif kind == "map" and ref.isdigit() and int(ref) in m.MAP_NAMES:
            return url_for("wiki_map", number=int(ref)), label or m.MAP_NAMES[int(ref)]
        elif kind == "region":
            import wiki
            family = wiki.MAP_FAMILIES.get(ref)
            if family:
                return url_for("wiki_region", key=ref), label or family["name"]
        elif kind == "page":
            endpoint = PAGES.get(ref)
            if endpoint:
                return url_for(endpoint), label or ref
        elif kind == "guide":
            guide = load_guides().get(ref)
            if guide:
                return url_for("wiki_guide", slug=ref), label or guide["title"]
        return None

    def expand(match):
        kind, ref, label = match.group(1), match.group(2), match.group(3)
        found = resolve(kind, ref, label)
        if found:
            return f'<a href="{found[0]}">{html.escape(found[1])}</a>'
        return f'<span class="guide-broken" title="Nie ma takiego odnośnika w wiki">[[{html.escape(kind)}:{html.escape(ref)}]]</span>'

    @app.route("/wiki/guides")
    @m.login_required
    def wiki_guides():
        return render_template("wiki_guides.html", guides=guide_list())

    @app.route("/wiki/guide/<slug>")
    @m.login_required
    def wiki_guide(slug):
        guides = load_guides()
        guide = guides.get(slug)
        if not guide:
            abort(404)
        engine = markdown.Markdown(extensions=["tables", "fenced_code", "sane_lists", "toc"],
                                   extension_configs={"toc": {"toc_depth": "2-3", "permalink": False}})
        # Links are expanded outside code only, so a guide can show the syntax itself.
        parts = CODE_RE.split(guide["body"])
        source = "".join(part if CODE_RE.fullmatch(part) else LINK_RE.sub(expand, part) for part in parts)
        body = engine.convert(source)
        ordered = guide_list()
        position = next((i for i, g in enumerate(ordered) if g["slug"] == slug), 0)
        return render_template("wiki_guide.html", guide=guide, body=body, toc=engine.toc_tokens,
                               previous=ordered[position - 1] if position > 0 else None,
                               following=ordered[position + 1] if position + 1 < len(ordered) else None)
