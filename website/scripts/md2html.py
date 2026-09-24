#!/usr/bin/env python3
"""Minimal markdown to HTML for myc docs. No CDN. Supports headings,
paragraphs, fenced code, inline code, tables, lists, bold/italic, links."""

import html
import re
import sys


def escape(s: str) -> str:
    return html.escape(s, quote=False)


def inline(text: str) -> str:
    """Process inline markdown: code, links, bold, italic."""
    # Protect code spans first
    parts = []
    i = 0
    pattern = re.compile(
        r'`([^`]+)`'
        r'|\[([^\]]+)\]\(([^)]+)\)'
        r'|\*\*([^*]+)\*\*'
        r'|__([^_]+)__'
        r'|\*([^*]+)\*'
        r'|_([^_]+)_'
    )
    for m in pattern.finditer(text):
        if m.start() > i:
            parts.append(escape(text[i:m.start()]))
        if m.group(1) is not None:
            parts.append(f"<code>{escape(m.group(1))}</code>")
        elif m.group(2) is not None:
            label = inline(m.group(2))
            href = escape(m.group(3))
            # rewrite .md links to .html counterparts when local
            href = rewrite_md_href(href)
            parts.append(f'<a href="{href}">{label}</a>')
        elif m.group(4) is not None:
            parts.append(f"<strong>{escape(m.group(4))}</strong>")
        elif m.group(5) is not None:
            parts.append(f"<strong>{escape(m.group(5))}</strong>")
        elif m.group(6) is not None:
            parts.append(f"<em>{escape(m.group(6))}</em>")
        elif m.group(7) is not None:
            parts.append(f"<em>{escape(m.group(7))}</em>")
        i = m.end()
    if i < len(text):
        parts.append(escape(text[i:]))
    return "".join(parts)


def rewrite_md_href(href: str) -> str:
    """Map known markdown doc links to website/docs HTML pages."""
    mapping = {
        "language.md": "language.html",
        "stdlib.md": "stdlib.html",
        "errors.md": "errors.html",
        "vm.md": "vm.html",
        "graphics.md": "graphics.html",
        "README.md": "overview.html",
        "../README.md": "readme.html",
        "../website/": "../index.html",
        "../website/docs/": "index.html",
        "../website/docs/index.html": "index.html",
    }
    # strip anchors for rewrite key
    base = href
    frag = ""
    if "#" in href:
        base, frag = href.split("#", 1)
        frag = "#" + frag
    if base in mapping:
        return mapping[base] + frag
    if base.endswith(".md"):
        name = base.rsplit("/", 1)[-1][:-3]
        return name + ".html" + frag
    return href


def is_table_sep(line: str) -> bool:
    s = line.strip()
    if not s.startswith("|"):
        return False
    cells = [c.strip() for c in s.strip("|").split("|")]
    if not cells:
        return False
    return all(re.fullmatch(r":?-{3,}:?", c) for c in cells)


def parse_table_row(line: str) -> list:
    return [c.strip() for c in line.strip().strip("|").split("|")]


def convert(md: str) -> str:
    lines = md.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    out = []
    i = 0
    n = len(lines)

    def flush_para(buf):
        if not buf:
            return
        text = " ".join(s.strip() for s in buf)
        out.append(f"<p>{inline(text)}</p>")
        buf.clear()

    para = []

    while i < n:
        line = lines[i]

        # fenced code
        if line.startswith("```"):
            flush_para(para)
            lang = line[3:].strip()
            i += 1
            code_lines = []
            while i < n and not lines[i].startswith("```"):
                code_lines.append(lines[i])
                i += 1
            if i < n:
                i += 1  # closing fence
            code = escape("\n".join(code_lines))
            cls = f' class="language-{escape(lang)}"' if lang else ""
            out.append(f"<pre><code{cls}>{code}</code></pre>")
            continue

        # blank
        if not line.strip():
            flush_para(para)
            i += 1
            continue

        # headings
        hm = re.match(r"^(#{1,6})\s+(.*)$", line)
        if hm:
            flush_para(para)
            level = len(hm.group(1))
            out.append(f"<h{level}>{inline(hm.group(2).strip())}</h{level}>")
            i += 1
            continue

        # unordered list
        if re.match(r"^[-*+]\s+", line):
            flush_para(para)
            out.append("<ul>")
            while i < n and re.match(r"^[-*+]\s+", lines[i]):
                item = re.sub(r"^[-*+]\s+", "", lines[i])
                # continuation lines (indented)
                i += 1
                while i < n and lines[i] and not re.match(r"^[-*+#|`]", lines[i]) and not re.match(r"^\d+\.\s+", lines[i]) and lines[i].startswith("  "):
                    item += " " + lines[i].strip()
                    i += 1
                out.append(f"<li>{inline(item)}</li>")
            out.append("</ul>")
            continue

        # ordered list
        if re.match(r"^\d+\.\s+", line):
            flush_para(para)
            out.append("<ol>")
            while i < n and re.match(r"^\d+\.\s+", lines[i]):
                item = re.sub(r"^\d+\.\s+", "", lines[i])
                i += 1
                while i < n and lines[i] and not re.match(r"^[-*+#|`]", lines[i]) and not re.match(r"^\d+\.\s+", lines[i]) and lines[i].startswith("  "):
                    item += " " + lines[i].strip()
                    i += 1
                out.append(f"<li>{inline(item)}</li>")
            out.append("</ol>")
            continue

        # table: header + separator + rows
        if (
            "|" in line
            and i + 1 < n
            and is_table_sep(lines[i + 1])
        ):
            flush_para(para)
            headers = parse_table_row(line)
            i += 2  # skip header + sep
            out.append("<table>")
            out.append("<thead><tr>")
            for h in headers:
                out.append(f"<th>{inline(h)}</th>")
            out.append("</tr></thead>")
            out.append("<tbody>")
            while i < n and lines[i].strip().startswith("|"):
                cells = parse_table_row(lines[i])
                # pad/truncate to header count
                while len(cells) < len(headers):
                    cells.append("")
                cells = cells[: len(headers)]
                out.append("<tr>")
                for c in cells:
                    out.append(f"<td>{inline(c)}</td>")
                out.append("</tr>")
                i += 1
            out.append("</tbody></table>")
            continue

        # paragraph accumulation
        para.append(line)
        i += 1

    flush_para(para)
    return "\n".join(out)


def main():
    if len(sys.argv) != 2:
        print("usage: md2html.py <file.md>", file=sys.stderr)
        sys.exit(1)
    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        md = f.read()
    # strip a single leading H1? Keep it; template may wrap with article.
    print(convert(md))


if __name__ == "__main__":
    main()
