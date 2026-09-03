from __future__ import annotations

import html
import re
import sys
import textwrap
from pathlib import Path

from PIL import Image as PILImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Flowable,
    Frame,
    Image,
    PageBreak,
    PageTemplate,
    Paragraph,
    Preformatted,
    Spacer,
    Table,
    TableStyle,
)


BLUE = colors.HexColor("#173B57")
CYAN = colors.HexColor("#1C8AA6")
PALE = colors.HexColor("#EAF4F7")
INK = colors.HexColor("#18242D")
MUTED = colors.HexColor("#60717D")
PAPER = colors.HexColor("#FAFCFD")


class SourcePageBadge(Flowable):
    def __init__(self, number: int, width: float):
        super().__init__()
        self.number = number
        self.width = width
        self.height = 12 * mm

    def draw(self):
        canvas = self.canv
        canvas.setFillColor(PALE)
        canvas.roundRect(0, 0, self.width, self.height, 3 * mm, fill=1, stroke=0)
        canvas.setFillColor(BLUE)
        canvas.setFont("YaHeiBold", 10)
        canvas.drawString(4 * mm, 4.1 * mm, f"原稿第 {self.number} 页 · 中文精翻")
        canvas.setFillColor(MUTED)
        canvas.setFont("YaHei", 7.5)
        canvas.drawRightString(self.width - 4 * mm, 4.2 * mm, "左侧缩略图用于核对公式、图示与版面")


class TranslationDocTemplate(BaseDocTemplate):
    def __init__(self, filename: str, **kwargs):
        super().__init__(filename, **kwargs)
        frame = Frame(
            self.leftMargin,
            self.bottomMargin,
            self.width,
            self.height,
            id="main",
            leftPadding=0,
            rightPadding=0,
            topPadding=0,
            bottomPadding=0,
        )
        self.addPageTemplates(PageTemplate(id="translation", frames=[frame], onPage=draw_page))
        self._bookmark_index = 0

    def afterFlowable(self, flowable):
        if not isinstance(flowable, Paragraph):
            return
        if flowable.style.name not in {"H1", "H2"}:
            return
        text = flowable.getPlainText()
        key = f"heading-{self._bookmark_index}"
        self._bookmark_index += 1
        self.canv.bookmarkPage(key)
        self.canv.addOutlineEntry(text, key, 0 if flowable.style.name == "H1" else 1, False)


def draw_page(canvas, doc):
    canvas.saveState()
    canvas.setFillColor(PAPER)
    canvas.rect(0, 0, A4[0], A4[1], fill=1, stroke=0)
    if doc.page > 1:
        canvas.setStrokeColor(colors.HexColor("#D8E4EA"))
        canvas.line(doc.leftMargin, 17 * mm, A4[0] - doc.rightMargin, 17 * mm)
        canvas.setFont("YaHei", 7.8)
        canvas.setFillColor(MUTED)
        canvas.drawString(doc.leftMargin, 11 * mm, "A Gentle Introduction to ReSTIR · 中文精翻版")
        canvas.drawRightString(A4[0] - doc.rightMargin, 11 * mm, str(doc.page))
    canvas.restoreState()


def inline_markup(text: str) -> str:
    escaped = html.escape(text, quote=False)
    escaped = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", escaped)
    escaped = re.sub(
        r"(https?://[^\s<]+)",
        r'<link href="\1" color="#1C7188">\1</link>',
        escaped,
    )
    return escaped


def parse_blocks(raw: str):
    lines = raw.splitlines()
    blocks = []
    paragraph = []
    code = []
    in_code = False

    def flush_paragraph():
        nonlocal paragraph
        if paragraph:
            blocks.append(("p", " ".join(line.strip() for line in paragraph)))
            paragraph = []

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("```"):
            if in_code:
                blocks.append(("code", "\n".join(code)))
                code = []
                in_code = False
            else:
                flush_paragraph()
                in_code = True
            continue
        if in_code:
            code.append(line.rstrip())
            continue
        if not stripped:
            flush_paragraph()
        elif stripped.startswith("### "):
            flush_paragraph(); blocks.append(("h3", stripped[4:]))
        elif stripped.startswith("## "):
            flush_paragraph(); blocks.append(("h2", stripped[3:]))
        elif stripped.startswith("# "):
            flush_paragraph(); blocks.append(("h1", stripped[2:]))
        elif stripped.startswith("> "):
            flush_paragraph(); blocks.append(("quote", stripped[2:]))
        elif re.match(r"^[-*] ", stripped):
            flush_paragraph(); blocks.append(("bullet", stripped[2:]))
        elif re.match(r"^\d+\. ", stripped):
            flush_paragraph(); blocks.append(("number", stripped))
        else:
            paragraph.append(stripped)
    flush_paragraph()
    if code:
        blocks.append(("code", "\n".join(code)))
    return blocks


def make_styles():
    base = getSampleStyleSheet()
    return {
        "title": ParagraphStyle("TitleZH", parent=base["Title"], fontName="YaHeiBold", fontSize=28, leading=38, textColor=colors.white, alignment=TA_LEFT, wordWrap="CJK"),
        "subtitle": ParagraphStyle("SubtitleZH", parent=base["Normal"], fontName="YaHei", fontSize=12, leading=20, textColor=colors.HexColor("#D8F0F5"), wordWrap="CJK"),
        "H1": ParagraphStyle("H1", parent=base["Heading1"], fontName="YaHeiBold", fontSize=18, leading=26, textColor=BLUE, spaceBefore=8, spaceAfter=8, wordWrap="CJK", keepWithNext=True),
        "H2": ParagraphStyle("H2", parent=base["Heading2"], fontName="YaHeiBold", fontSize=13, leading=20, textColor=CYAN, spaceBefore=7, spaceAfter=5, wordWrap="CJK", keepWithNext=True),
        "H3": ParagraphStyle("H3", parent=base["Heading3"], fontName="YaHeiBold", fontSize=11, leading=17, textColor=BLUE, spaceBefore=5, spaceAfter=3, wordWrap="CJK", keepWithNext=True),
        "body": ParagraphStyle("BodyZH", parent=base["BodyText"], fontName="YaHei", fontSize=9.4, leading=15.2, textColor=INK, alignment=TA_LEFT, wordWrap="CJK", spaceAfter=4),
        "bullet": ParagraphStyle("BulletZH", parent=base["BodyText"], fontName="YaHei", fontSize=9.2, leading=14.8, textColor=INK, leftIndent=5 * mm, firstLineIndent=-3 * mm, wordWrap="CJK", spaceAfter=2),
        "quote": ParagraphStyle("QuoteZH", parent=base["BodyText"], fontName="YaHei", fontSize=8.7, leading=14, textColor=BLUE, backColor=PALE, borderColor=CYAN, borderWidth=0.7, borderPadding=7, leftIndent=3 * mm, rightIndent=3 * mm, wordWrap="CJK", spaceBefore=3, spaceAfter=6),
        "code": ParagraphStyle("CodeZH", parent=base["Code"], fontName="YaHei", fontSize=7.7, leading=11.3, textColor=colors.HexColor("#24343E"), backColor=colors.HexColor("#EEF3F5"), borderPadding=7, leftIndent=2 * mm, rightIndent=2 * mm, spaceBefore=3, spaceAfter=6),
        "small": ParagraphStyle("SmallZH", parent=base["BodyText"], fontName="YaHei", fontSize=8.2, leading=13, textColor=MUTED, wordWrap="CJK"),
    }


def build_story(source_md: Path, thumbs: Path, styles, page_width: float):
    raw = source_md.read_text(encoding="utf-8")
    sections = re.split(r"(?m)^@@PAGE\s+(\d+)\s*$", raw)
    page_sections = [(int(sections[i]), sections[i + 1].strip()) for i in range(1, len(sections), 2)]
    if [number for number, _ in page_sections] != list(range(1, 62)):
        raise ValueError("译文页码必须完整覆盖 1..61")

    story = []
    cover = Table(
        [[Paragraph("ReSTIR 温和导论", styles["title"])],
         [Paragraph("实时路径复用 · SIGGRAPH 2023 Course Notes 中文精翻版", styles["subtitle"])],
         [Spacer(1, 118 * mm)],
         [Paragraph("逐页对应原稿 · 保留公式编号、算法、图注、术语与参考文献", styles["subtitle"])]],
        colWidths=[page_width], rowHeights=[30 * mm, 20 * mm, 118 * mm, 20 * mm],
        style=TableStyle([("BACKGROUND", (0, 0), (-1, -1), BLUE), ("BOX", (0, 0), (-1, -1), 0, BLUE), ("LEFTPADDING", (0, 0), (-1, -1), 13 * mm), ("RIGHTPADDING", (0, 0), (-1, -1), 13 * mm), ("TOPPADDING", (0, 0), (-1, 1), 10 * mm), ("BOTTOMPADDING", (0, -1), (-1, -1), 7 * mm)]),
    )
    story.extend([cover, PageBreak()])

    for idx, (number, page_text) in enumerate(page_sections):
        if idx:
            story.append(PageBreak())
        story.append(SourcePageBadge(number, page_width))
        story.append(Spacer(1, 3 * mm))
        thumb_path = thumbs / f"page-{number:02d}.jpg"
        with PILImage.open(thumb_path) as image:
            iw, ih = image.size
        tw = 43 * mm
        th = tw * ih / iw
        thumb = Image(str(thumb_path), width=tw, height=th)
        note = Paragraph(
            "<b>对照说明</b><br/>本节译文对应左侧原稿整页。公式采用原编号；复杂数学排版、图形与边栏提示可直接放大左侧缩略图，或在随附英文原稿中查看。",
            styles["small"],
        )
        compare = Table([[thumb, note]], colWidths=[48 * mm, page_width - 48 * mm], style=TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (1, 0), (1, 0), colors.HexColor("#F2F7F9")), ("BOX", (1, 0), (1, 0), 0.5, colors.HexColor("#D4E5EB")), ("LEFTPADDING", (1, 0), (1, 0), 5 * mm), ("RIGHTPADDING", (1, 0), (1, 0), 5 * mm), ("TOPPADDING", (1, 0), (1, 0), 5 * mm), ("BOTTOMPADDING", (1, 0), (1, 0), 5 * mm)]))
        story.extend([compare, Spacer(1, 4 * mm)])

        for kind, text in parse_blocks(page_text):
            if kind == "code":
                wrapped = []
                for line in text.splitlines():
                    wrapped.extend(textwrap.wrap(line, width=92, subsequent_indent="    ", replace_whitespace=False, drop_whitespace=False) or [""])
                story.append(Preformatted("\n".join(wrapped), styles["code"], maxLineLength=100))
            elif kind == "h1":
                story.append(Paragraph(inline_markup(text), styles["H1"]))
            elif kind == "h2":
                story.append(Paragraph(inline_markup(text), styles["H2"]))
            elif kind == "h3":
                story.append(Paragraph(inline_markup(text), styles["H3"]))
            elif kind == "quote":
                story.append(Paragraph(inline_markup(text), styles["quote"]))
            elif kind == "bullet":
                story.append(Paragraph("• " + inline_markup(text), styles["bullet"]))
            elif kind == "number":
                story.append(Paragraph(inline_markup(text), styles["bullet"]))
            else:
                story.append(Paragraph(inline_markup(text), styles["body"]))
    return story


def main():
    source_md = Path(sys.argv[1])
    thumbs = Path(sys.argv[2])
    output = Path(sys.argv[3])
    output.parent.mkdir(parents=True, exist_ok=True)

    pdfmetrics.registerFont(TTFont("YaHei", r"C:\Windows\Fonts\msyh.ttc", subfontIndex=0))
    pdfmetrics.registerFont(TTFont("YaHeiBold", r"C:\Windows\Fonts\msyhbd.ttc", subfontIndex=0))
    styles = make_styles()
    doc = TranslationDocTemplate(
        str(output),
        pagesize=A4,
        leftMargin=18 * mm,
        rightMargin=18 * mm,
        topMargin=16 * mm,
        bottomMargin=23 * mm,
        title="A Gentle Introduction to ReSTIR - 中文精翻版",
        author="Chris Wyman 等；中文翻译排版版",
        subject="SIGGRAPH 2023 ReSTIR Course Notes 中文精翻",
    )
    story = build_story(source_md, thumbs, styles, doc.width)
    doc.build(story)


if __name__ == "__main__":
    main()
