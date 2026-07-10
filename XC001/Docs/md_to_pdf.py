# -*- coding: utf-8 -*-
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph, Preformatted, SimpleDocTemplate, Spacer, Table, TableStyle


ROOT = Path(__file__).resolve().parents[1]
MD_PATH = ROOT / "Docs" / "XC001_SCPI指令说明.md"
PDF_PATH = ROOT / "Docs" / "XC001_SCPI指令说明.pdf"
FONT_PATH = Path(r"C:\Windows\Fonts\simhei.ttf")


def para(text, style):
    return Paragraph(escape(text).replace("`", ""), style)


def split_table_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in line.split("|")]


def is_separator_row(line):
    cells = split_table_row(line)
    return cells and all(set(c.replace(":", "").strip()) <= {"-"} and "-" in c for c in cells)


def column_widths(col_count):
    page_width = 182 * mm
    if col_count == 2:
        return [44 * mm, page_width - 44 * mm]
    if col_count == 3:
        return [44 * mm, 38 * mm, page_width - 82 * mm]
    if col_count == 4:
        return [34 * mm, 34 * mm, 72 * mm, page_width - 140 * mm]
    return [page_width / col_count] * col_count


def add_table(story, rows, styles):
    if not rows:
        return
    data = [[para(cell, styles["Cell"]) for cell in row] for row in rows]
    table = Table(data, colWidths=column_widths(len(rows[0])), repeatRows=1)
    table.setStyle(TableStyle([
        ("FONTNAME", (0, 0), (-1, -1), "CN"),
        ("FONTSIZE", (0, 0), (-1, -1), 8.3),
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#142536")),
        ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#BFC8D2")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]))
    story.append(table)
    story.append(Spacer(1, 5))


def build_story(md_text, styles):
    story = []
    lines = md_text.splitlines()
    i = 0
    in_code = False
    code_lines = []
    table_rows = []

    def flush_table():
        nonlocal table_rows
        if table_rows:
            add_table(story, table_rows, styles)
            table_rows = []

    while i < len(lines):
        line = lines[i].rstrip()

        if line.startswith("```"):
            flush_table()
            if not in_code:
                in_code = True
                code_lines = []
            else:
                in_code = False
                story.append(Preformatted("\n".join(code_lines), styles["Code"]))
                story.append(Spacer(1, 5))
            i += 1
            continue

        if in_code:
            code_lines.append(line)
            i += 1
            continue

        if line.strip().startswith("|") and i + 1 < len(lines) and is_separator_row(lines[i + 1]):
            flush_table()
            table_rows.append(split_table_row(line))
            i += 2
            while i < len(lines) and lines[i].strip().startswith("|"):
                table_rows.append(split_table_row(lines[i]))
                i += 1
            flush_table()
            continue

        if not line.strip():
            flush_table()
            story.append(Spacer(1, 4))
            i += 1
            continue

        flush_table()
        stripped = line.strip()
        if stripped == "---":
            story.append(Spacer(1, 8))
        elif stripped.startswith("## "):
            story.append(para(stripped[3:], styles["H1"]))
        elif stripped.startswith("### "):
            story.append(para(stripped[4:], styles["H2"]))
        elif stripped.startswith("> "):
            story.append(para(stripped[2:], styles["Quote"]))
        elif stripped[0:2].isdigit() and stripped[2:4] == ". ":
            story.append(para(stripped, styles["Body"]))
        else:
            story.append(para(stripped, styles["Body"]))
        i += 1

    flush_table()
    return story


def header_footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("CN", 8)
    canvas.setFillColor(colors.HexColor("#6B7682"))
    canvas.drawString(14 * mm, 9 * mm, "XC001 控制板 SCPI 指令说明")
    canvas.drawRightString(196 * mm, 9 * mm, f"第 {doc.page} 页")
    canvas.restoreState()


def main():
    pdfmetrics.registerFont(TTFont("CN", str(FONT_PATH)))
    base = getSampleStyleSheet()
    styles = {
        "H1": ParagraphStyle("H1", parent=base["Heading1"], fontName="CN", fontSize=16, leading=22, spaceBefore=10, spaceAfter=6, textColor=colors.HexColor("#142536")),
        "H2": ParagraphStyle("H2", parent=base["Heading2"], fontName="CN", fontSize=12.5, leading=17, spaceBefore=8, spaceAfter=4, textColor=colors.HexColor("#1F3A54")),
        "Body": ParagraphStyle("Body", parent=base["BodyText"], fontName="CN", fontSize=9.2, leading=14),
        "Cell": ParagraphStyle("Cell", parent=base["BodyText"], fontName="CN", fontSize=8.3, leading=12),
        "Quote": ParagraphStyle("Quote", parent=base["BodyText"], fontName="CN", fontSize=9, leading=13, leftIndent=8, textColor=colors.HexColor("#4B5563")),
        "Code": ParagraphStyle("Code", parent=base["Code"], fontName="CN", fontSize=8.2, leading=11, backColor=colors.HexColor("#F3F5F7")),
    }

    story = build_story(MD_PATH.read_text(encoding="utf-8"), styles)
    doc = SimpleDocTemplate(
        str(PDF_PATH),
        pagesize=A4,
        rightMargin=12 * mm,
        leftMargin=12 * mm,
        topMargin=14 * mm,
        bottomMargin=16 * mm,
    )
    doc.build(story, onFirstPage=header_footer, onLaterPages=header_footer)
    print(PDF_PATH)


if __name__ == "__main__":
    main()
