#!/usr/bin/env python3
"""Build website/docs/*.html from project markdown sources."""

import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WEBSITE = os.path.join(ROOT, "website")
DOCS_OUT = os.path.join(WEBSITE, "docs")
MD2HTML = os.path.join(WEBSITE, "scripts", "md2html.py")

# (out_name, md_path_rel_to_root, title, nav_label, short_desc for index)
PAGES = [
    ("readme.html", "README.md", "Project README", "README",
     "Build, run, tests, layout, and overview of myc."),
    ("overview.html", "docs/README.md", "Docs overview", "Overview",
     "Index of markdown docs: language, stdlib, errors, VM, graphics."),
    ("language.html", "docs/language.md", "Language", "Language",
     "Syntax, keywords, types, operators, control flow, structs, imports."),
    ("stdlib.html", "docs/stdlib.md", "Stdlib", "Stdlib",
     "VM builtins and packages (math, fs, json, csv, os, net, http, graphics)."),
    ("errors.html", "docs/errors.md", "Errors", "Errors",
     "Error format, exit codes, and example failing programs."),
    ("vm.html", "docs/vm.md", "VM", "VM",
     "Pipeline, GC, and bytecode overview."),
    ("graphics.html", "docs/graphics.md", "Graphics", "Graphics",
     "macOS AppKit window API and stub behavior elsewhere."),
]


def md_to_html(md_path: str) -> str:
    r = subprocess.run(
        [sys.executable, MD2HTML, md_path],
        capture_output=True,
        text=True,
        check=True,
    )
    return r.stdout


def doc_subnav(current: str) -> str:
    items = ['      <li><a href="index.html">All docs</a></li>']
    for out_name, _, _, label, _ in PAGES:
        cls = ' class="current"' if out_name == current else ""
        items.append(f'      <li><a href="{out_name}"{cls}>{label}</a></li>')
    return "    <ul class=\"doc-nav\">\n" + "\n".join(items) + "\n    </ul>"


def page_shell(title: str, body: str, current: str, css_href: str, js_href: str,
               home_href: str, lang_href: str, stdlib_href: str, docs_href: str) -> str:
    sub = doc_subnav(current)
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} - myc</title>
  <link rel="stylesheet" href="{css_href}">
</head>
<body>
  <header>
    <div class="inner">
      <a class="logo" href="{home_href}">myc</a>
      <button type="button" class="nav-toggle" aria-label="Menu">Menu</button>
      <nav>
        <a href="{home_href}">Home</a>
        <a href="{lang_href}">Language</a>
        <a href="{stdlib_href}">Stdlib</a>
        <a href="{docs_href}">Docs</a>
      </nav>
    </div>
  </header>
  <main class="doc">
{sub}
{body}
  </main>
  <footer>
    <div class="inner">myc language documentation</div>
  </footer>
  <script src="{js_href}"></script>
</body>
</html>
"""


def build_article(out_name: str, md_rel: str, title: str) -> None:
    md_path = os.path.join(ROOT, md_rel)
    body = md_to_html(md_path)
    # Wrap body; h1 already comes from markdown
    html = page_shell(
        title=title,
        body=body,
        current=out_name,
        css_href="../styles.css",
        js_href="../main.js",
        home_href="../index.html",
        lang_href="../language.html",
        stdlib_href="../stdlib.html",
        docs_href="index.html",
    )
    # Fix accidental em/en dashes if any snuck in from sources: leave sources alone;
    # we only ensure we don't introduce them.
    out_path = os.path.join(DOCS_OUT, out_name)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html)
    print("wrote", out_path)


def build_index() -> None:
    items = []
    for out_name, _, title, _, desc in PAGES:
        items.append(
            f'      <li><a href="{out_name}"><strong>{title}</strong></a>'
            f" - {desc}</li>"
        )
    body = (
        "    <h1>Documentation</h1>\n"
        "    <p>Rendered from the project markdown sources. Plain HTML, no CDN.</p>\n"
        "    <ol class=\"doc-list\">\n"
        + "\n".join(items)
        + "\n    </ol>\n"
        '    <p>Source tree: <code>docs/</code> and root <code>README.md</code>.</p>'
    )
    html = page_shell(
        title="Docs",
        body=body,
        current="index.html",
        css_href="../styles.css",
        js_href="../main.js",
        home_href="../index.html",
        lang_href="../language.html",
        stdlib_href="../stdlib.html",
        docs_href="index.html",
    )
    out_path = os.path.join(DOCS_OUT, "index.html")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html)
    print("wrote", out_path)


def main():
    os.makedirs(DOCS_OUT, exist_ok=True)
    for out_name, md_rel, title, _, _ in PAGES:
        build_article(out_name, md_rel, title)
    build_index()


if __name__ == "__main__":
    main()
