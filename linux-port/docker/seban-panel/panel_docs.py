"""Lexiw: "Instrukcja panelu" - what is where in this panel, what each page
lets you set, what the classic panel still has that this one does not, and
what is missing (LEXIW_PANEL_DOCS_V1).

The text is panel_docs.md, Markdown; [[ep:endpoint|label]] (or
[[ep:endpoint#anchor|label]]) links a page of this panel through url_for, so
the links follow the panel's address.

install_panel_docs(module) is called from app.py.
"""
import html
import re
from pathlib import Path

import markdown
from flask import render_template, url_for
from werkzeug.routing import BuildError

DOC = Path(__file__).with_name("panel_docs.md")
LINK_RE = re.compile(r"\[\[ep:([a-z_]+)(?:\?([a-z_]+)=([a-z0-9_-]+))?(#[a-z0-9-]+)?\|([^\]]+)\]\]")
CODE_RE = re.compile(r"(```.*?```|`[^`\n]*`)", re.S)


def install_panel_docs(m):
    app = m.app

    def expand(match):
        endpoint, arg, value, anchor, label = match.groups()
        try:
            url = url_for(endpoint, **({arg: value} if arg else {})) + (anchor or "")
        except BuildError:
            return f'<span class="guide-broken">{html.escape(label)}</span>'
        return f'<a href="{url}">{html.escape(label)}</a>'

    @app.route("/docs")
    @m.login_required
    def panel_docs():
        text = DOC.read_text(encoding="utf-8")
        parts = CODE_RE.split(text)
        source = "".join(part if CODE_RE.fullmatch(part) else LINK_RE.sub(expand, part) for part in parts)
        engine = markdown.Markdown(extensions=["tables", "fenced_code", "sane_lists", "toc"],
                                   extension_configs={"toc": {"toc_depth": "2-2", "permalink": False}})
        body = engine.convert(source)
        return render_template("panel_docs.html", body=body, toc=engine.toc_tokens)
