# -*- coding: utf-8 -*-
from pathlib import Path
from subprocess import run
from xml.sax.saxutils import escape


ROOT = Path(__file__).resolve().parents[1]
MD_PATH = ROOT / "Docs" / "XC001_SCPI指令说明.md"
HTML_PATH = ROOT / "Docs" / "XC001_SCPI指令说明.html"
PDF_PATH = ROOT / "Docs" / "XC001_SCPI指令说明.pdf"
CHROME = Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe")


def inline(text):
    out = ""
    parts = text.split("`")
    for i, part in enumerate(parts):
        if i % 2:
            out += f"<code>{escape(part)}</code>"
        else:
            out += escape(part)
    return out


def split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in line.split("|")]


def is_sep(line):
    cells = split_row(line)
    return bool(cells) and all(set(c.replace(":", "").strip()) <= {"-"} and "-" in c for c in cells)


def md_to_html(md):
    lines = md.splitlines()
    html = []
    i = 0
    in_code = False
    code = []
    while i < len(lines):
        line = lines[i].rstrip()
        stripped = line.strip()

        if stripped.startswith("```"):
            if not in_code:
                in_code = True
                code = []
            else:
                in_code = False
                html.append("<pre><code>" + escape("\n".join(code)) + "</code></pre>")
            i += 1
            continue

        if in_code:
            code.append(line)
            i += 1
            continue

        if stripped.startswith("|") and i + 1 < len(lines) and is_sep(lines[i + 1]):
            rows = [split_row(line)]
            i += 2
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append(split_row(lines[i]))
                i += 1
            html.append("<table><thead><tr>" + "".join(f"<th>{inline(c)}</th>" for c in rows[0]) + "</tr></thead><tbody>")
            for row in rows[1:]:
                html.append("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in row) + "</tr>")
            html.append("</tbody></table>")
            continue

        if not stripped:
            i += 1
            continue
        if stripped == "---":
            html.append("<hr>")
        elif stripped.startswith("![") and "](" in stripped and stripped.endswith(")"):
            alt = stripped[2:stripped.index("](")]
            src = stripped[stripped.index("](") + 2:-1]
            img_path = (MD_PATH.parent / src).resolve()
            html.append(f'<p class="logo-line"><img class="doc-logo" src="{img_path.as_uri()}" alt="{escape(alt)}"></p>')
        elif stripped.startswith("## "):
            html.append(f"<h2>{inline(stripped[3:])}</h2>")
        elif stripped.startswith("### "):
            html.append(f"<h3>{inline(stripped[4:])}</h3>")
        elif stripped.startswith("> "):
            html.append(f"<blockquote>{inline(stripped[2:])}</blockquote>")
        elif stripped[0:2].isdigit() and stripped[2:4] == ". ":
            items = []
            while i < len(lines):
                s = lines[i].strip()
                if len(s) >= 4 and s[0:2].isdigit() and s[2:4] == ". ":
                    items.append(f"<li>{inline(s[4:])}</li>")
                    i += 1
                else:
                    break
            html.append("<ol>" + "".join(items) + "</ol>")
            continue
        else:
            html.append(f"<p>{inline(stripped)}</p>")
        i += 1
    return "\n".join(html)


def main():
    body = md_to_html(MD_PATH.read_text(encoding="utf-8"))
    html = f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>XC001 SCPI 指令说明</title>
<style>
@page {{ size: A4; margin: 14mm 12mm 16mm; }}
body {{ font-family: "Microsoft YaHei", "Noto Sans SC", Arial, sans-serif; color:#1b2630; line-height:1.55; font-size:13px; }}
h2 {{ color:#142536; font-size:22px; margin:18px 0 10px; border-bottom:2px solid #d9e1ea; padding-bottom:5px; break-after:avoid; }}
h3 {{ color:#1f3a54; font-size:16px; margin:14px 0 8px; break-after:avoid; }}
p {{ margin: 7px 0; }}
.logo-line {{ margin: 4px 0 12px; }}
.doc-logo {{ width: 150px; height: auto; display: block; }}
hr {{ border:0; border-top:1px solid #d9e1ea; margin:12px 0; }}
table {{ width:100%; border-collapse:collapse; margin:8px 0 14px; break-inside:auto; }}
tr {{ break-inside:avoid; break-after:auto; }}
th {{ background:#142536; color:white; font-weight:600; }}
th,td {{ border:1px solid #bfc8d2; padding:6px 7px; vertical-align:top; word-break:break-word; }}
code {{ font-family: Consolas, "Microsoft YaHei", monospace; background:#eef2f5; padding:1px 4px; border-radius:3px; }}
pre {{ background:#101820; color:#dff3ff; padding:10px 12px; border-radius:6px; white-space:pre-wrap; break-inside:avoid; }}
pre code {{ background:transparent; color:inherit; padding:0; }}
blockquote {{ margin:8px 0; padding:7px 10px; border-left:4px solid #0f62a8; background:#f2f6fa; color:#4b5563; }}
ol {{ padding-left:24px; }}
</style>
</head>
<body>
{body}
</body>
</html>"""
    HTML_PATH.write_text(html, encoding="utf-8")
    run([
        str(CHROME),
        "--headless=new",
        "--disable-gpu",
        "--no-pdf-header-footer",
        f"--print-to-pdf={PDF_PATH}",
        HTML_PATH.as_uri(),
    ], check=True)
    print(PDF_PATH)


if __name__ == "__main__":
    main()
