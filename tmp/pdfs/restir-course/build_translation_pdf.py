from __future__ import annotations

import html
import hashlib
import math
import re
import sys
import textwrap
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
from matplotlib.backends.backend_agg import FigureCanvasAgg
from matplotlib.figure import Figure
from PIL import Image as PILImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT
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
    KeepTogether,
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


INLINE_RESERVED = {
    "A", "An", "And", "As", "At", "Chapter", "Chris", "Cem", "Daqi",
    "For", "From", "Generalized", "GPU", "CPU", "HDR", "RGB", "BSDF", "NEE",
    "MIS", "RIS", "ReSTIR", "DI", "GI", "PT", "GRIS", "PDF", "PSS", "SPP",
    "WRS", "NVIDIA", "Path", "Guiding", "Reservoir", "Sample", "Samples",
    "Temporal", "Spatial", "Reuse", "Shift", "Mapping", "Jacobian", "Hybrid",
    "Lobe", "Technique", "Tag", "Primary", "Space", "Woodcock", "Photon", "Mapping",
    "PhotonMapping", "Lambertian", "Pawel", "Markus", "Benedikt", "Wojciech",
    "Giovanni", "Wyman", "Kettunen", "Lin", "Bitterli", "Yuksel", "Jarosz",
    "Kozlowski", "De", "Francesco", "Equation", "Hz", "O", "M", "I",
}

INLINE_VARIABLES = set("AIJLMNPTUVWXYabcdefhijklmnpqrstuvwxyzΩεδσαβγλμω")


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


class AlgorithmBlock(Flowable):
    """按原讲义的算法框样式绘制伪代码：标题、行号、缩进导引线和绿色注释。"""

    def __init__(self, source: str, width: float, title: str = ""):
        super().__init__()
        self.source = source
        self.width = width
        self.title = title
        self.lines = source.splitlines()
        self.font_size = 7.25
        self.leading = 9.7
        self.title_size = 8.1
        self.top = 5.8 * mm
        self.bottom = 4.5 * mm
        self.height = self._measure_height()

    def _measure_height(self) -> float:
        line_count = max(1, len(self.lines))
        return self.top + self.title_size + 2.5 * mm + line_count * self.leading + self.bottom

    def wrap(self, availWidth, availHeight):
        self.width = min(self.width, availWidth)
        self.height = self._measure_height()
        return self.width, self.height

    @staticmethod
    def _split_comment(line: str):
        if "//" not in line:
            return line, ""
        code, comment = line.split("//", 1)
        return code.rstrip(), "//" + comment

    @staticmethod
    def _indent_level(line: str) -> int:
        leading = len(line) - len(line.lstrip(" "))
        return max(0, leading // 4)

    @staticmethod
    def _code_markup(value: str) -> str:
        escaped = html.escape(value, quote=False)
        escaped = re.sub(r"\b(class|struct|function|if|else|for|return|update|Resample)\b", r"<b>\1</b>", escaped)
        escaped = re.sub(r"(?<![A-Za-z])([A-Za-z])_([A-Za-z0-9]+)", r"<i>\1</i><sub>\2</sub>", escaped)
        return escaped

    def draw(self):
        canvas = self.canv
        canvas.saveState()
        canvas.setStrokeColor(colors.HexColor("#1F2A30"))
        canvas.setLineWidth(0.55)
        canvas.line(0, self.height - 1.1 * mm, self.width, self.height - 1.1 * mm)

        y = self.height - self.top
        if self.title:
            canvas.setFillColor(INK)
            canvas.setFont("YaHeiBold" if any("\u4e00" <= ch <= "\u9fff" for ch in self.title) else "Times-Bold", self.title_size)
            canvas.drawString(0, y, self.title)
            y -= self.title_size + 2.5 * mm
        else:
            y -= 1.5 * mm

        number_width = 14 * mm
        code_origin = number_width + 2.5 * mm
        indent_step = 7.0 * mm
        canvas.setFont("Times-Roman", self.font_size)

        for line_number, line in enumerate(self.lines, 1):
            if y < self.bottom:
                break
            indent = self._indent_level(line)
            base_y = y

            # 细竖线模拟原算法排版中的括号/控制流导引线。
            if line.strip():
                canvas.setStrokeColor(colors.HexColor("#263238"))
                for level in range(1, indent + 1):
                    x = code_origin + (level - 1) * indent_step - 2.0 * mm
                    canvas.line(x, base_y - self.leading + 1.5, x, base_y + 1.5)

            canvas.setFillColor(colors.HexColor("#4F5B61"))
            canvas.setFont("Times-Roman", 5.8)
            canvas.drawRightString(number_width - 1.5 * mm, base_y, str(line_number))

            code, comment = self._split_comment(line)
            code_x = code_origin + indent * indent_step
            code_style = ParagraphStyle(
                "AlgorithmCodeLine",
                fontName="Times-Roman",
                fontSize=self.font_size,
                leading=self.leading,
                textColor=INK,
                wordWrap=None,
                spaceAfter=0,
                spaceBefore=0,
            )
            code_para = Paragraph(self._code_markup(code.strip()), code_style)
            code_para.wrapOn(canvas, max(1, self.width - code_x - 2 * mm), self.leading)
            code_para.drawOn(canvas, code_x, base_y - self.leading * 0.72)
            if comment:
                code_width = pdfmetrics.stringWidth(code.strip(), "Times-Roman", self.font_size)
                comment_style = ParagraphStyle(
                    "AlgorithmCommentLine",
                    fontName="Times-Italic",
                    fontSize=self.font_size,
                    leading=self.leading,
                    textColor=colors.HexColor("#5F7D55"),
                    wordWrap=None,
                )
                comment_para = Paragraph(html.escape(comment, quote=False), comment_style)
                comment_para.wrapOn(canvas, max(1, self.width - code_x - code_width - 2 * mm), self.leading)
                comment_para.drawOn(canvas, code_x + code_width + 2.5 * mm, base_y - self.leading * 0.72)
            y -= self.leading

        canvas.setStrokeColor(colors.HexColor("#1F2A30"))
        canvas.setLineWidth(0.55)
        canvas.line(0, self.bottom - 1.0 * mm, self.width, self.bottom - 1.0 * mm)
        canvas.restoreState()


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


def render_inline_math(latex: str, cache_dir: Path):
    """生成可嵌入 Paragraph 的透明数学小图，并缓存避免重复渲染。"""
    cache_dir = cache_dir / "inline-math"
    cache_dir.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(latex.encode("utf-8")).hexdigest()[:20]
    output = cache_dir / f"inline-{digest}.png"
    if not output.is_file():
        # 根据公式长度给出足够的画布，bbox_inches=tight 会去掉多余留白。
        width = max(0.45, min(8.0, 0.16 * len(latex) + 0.35))
        figure = Figure(figsize=(width, 0.34), dpi=300)
        FigureCanvasAgg(figure)
        figure.patch.set_alpha(0)
        figure.text(0.01, 0.04, f"${latex}$", fontsize=9.8, color="#18242D")
        figure.savefig(output, dpi=300, transparent=True, bbox_inches="tight", pad_inches=0.012)
    with PILImage.open(output) as rendered:
        pixel_width, pixel_height = rendered.size
    return output, pixel_width * 72 / 300, pixel_height * 72 / 300


def normalize_inline_latex(value: str) -> str:
    s = value.strip()
    s = s.replace("…", r"\ldots ").replace("...", r"\ldots ")
    s = re.sub(r"(?<=\d)\s*[x×]\s*(?=\d)", r" \\times ", s)
    s = s.replace("Ω", r"\Omega").replace("ε", r"\epsilon")
    s = re.sub(r"\bpHat(?:_([A-Za-z0-9]+))?\b", lambda m: r"\hat{p}" + (f"_{{{m.group(1)}}}" if m.group(1) else ""), s)
    s = re.sub(r"\bpBar\b", lambda _m: r"\bar{p}", s)
    s = re.sub(r"\bf_lBar\b", lambda _m: r"\bar{f}_l", s)
    s = re.sub(r"\bW_Xi\b", lambda _m: r"W_{X_i}", s)
    s = re.sub(r"\bW_xi\b", lambda _m: r"W_{x_i}", s)
    s = re.sub(r"\bp_Xi\b", lambda _m: r"p_{X_i}", s)
    s = re.sub(r"\bp_Yi\b", lambda _m: r"p_{Y_i}", s)
    s = re.sub(r"\bJforward\b", lambda _m: r"J_{\mathrm{forward}}", s)
    s = re.sub(r"\bcCap\b", lambda _m: r"c_{\mathrm{cap}}", s)
    s = re.sub(r"\babsJacobian\b", lambda _m: r"|J|", s)
    s = re.sub(r"\bLe\b", lambda _m: r"L_e", s)
    s = re.sub(r"\bfs\b", lambda _m: r"f_s", s)
    s = re.sub(r"\bdx\b", lambda _m: r"\mathrm{d}x", s)
    s = re.sub(r"\bintegral\(([^)]+)\)", lambda m: r"\int " + m.group(1), s)
    s = re.sub(r"\buBar\b", lambda _m: r"\bar{u}", s)
    s = re.sub(r"\bpHatFromOptimized\b", lambda _m: r"\mathrm{pHatFromOptimized}", s)
    s = re.sub(r"\bpHatFrom\b", lambda _m: r"\mathrm{pHatFrom}", s)
    s = re.sub(r"\b(alpha|beta|lambda|omega|epsilon)\b", lambda m: "\\" + m.group(1), s)
    s = re.sub(r"\bsupp\s*\(", lambda _m: r"\operatorname{supp}(", s)
    s = re.sub(r"\b([xXyY])([0-9]+)\b", lambda m: f"{m.group(1)}_{{{m.group(2)}}}", s)
    s = re.sub(r"\b([A-Za-zΩ])_([A-Za-z0-9]+)\b", lambda m: f"{m.group(1)}_{{{m.group(2)}}}", s)
    s = re.sub(r"\b([A-Za-z])\^(-?[A-Za-z0-9]+)\b", lambda m: f"{m.group(1)}^{{{m.group(2)}}}", s)
    s = s.replace("<I>", r"\langle I\rangle")
    s = s.replace("<l>", r"\langle l\rangle")
    s = s.replace("->", r"\to ").replace("↔", r"\leftrightarrow ")
    s = s.replace("∪", r"\cup ").replace("∩", r"\cap ").replace("∈", r"\in ")
    s = s.replace("!=", r"\neq ").replace(">=", r"\geq ").replace("<=", r"\leq ")
    s = s.replace("∝", r"\propto ")
    return s


def inline_markup(text: str, math_cache: Path | None = None) -> str:
    """将句内数学表达式转成透明图片，避免正文中的下标/希腊字母退化为普通文本。"""
    if math_cache is None:
        math_cache = Path(".")
    images = {}
    protected = {}

    def hold(value: str, prefix: str = "\ue000") -> str:
        key = f"{prefix}{len(protected)}\ue001"
        protected[key] = value
        return key

    # 链接和显式 $...$/\(...\) 先保护，避免自动识别破坏 URL 或标记。
    text = re.sub(r"https?://[^\s]+", lambda m: hold(m.group(0), "\ue010"), text)
    explicit = re.compile(r"\$(.+?)\$|\\\((.+?)\\\)")
    text = explicit.sub(lambda m: hold(render_inline_math(normalize_inline_latex(m.group(1) or m.group(2)), math_cache)[0].as_posix(), "\ue020"), text)

    def add_math(match):
        raw = match.group(0)
        latex = normalize_inline_latex(raw)
        path, width, height = render_inline_math(latex, math_cache)
        # 正文字号约 9.4 pt；将极长表达式限制在一行可用宽度内。
        if width > 105:
            scale = 105 / width
            width *= scale
            height *= scale
        tag = f'<img src="{html.escape(path.as_posix(), quote=True)}" width="{width:.2f}" height="{height:.2f}" valign="-{max(1.5, height * 0.22):.2f}"/>'
        return hold(tag)

    # 先处理带运算符的完整短表达式，再处理函数调用和下标变量。
    expression = re.compile(
        r"(?<![A-Za-z0-9_])(?:[A-Za-zΩ][A-Za-z0-9_]*(?:\([^()\n]{0,55}\))?|\|[^|\n]{1,45}\|)"
        r"(?:\s*(?:=|==|!=|>=|<=|>|<|≤|≥|∝|/|\+|-|×|x|∪|∩|∈)\s*"
        r"(?:[A-Za-zΩ][A-Za-z0-9_]*(?:\([^()\n]{0,55}\))?|\d+(?:\.\d+)?|\|[^|\n]{1,45}\|))+"
        r"(?![A-Za-z0-9_])"
    )
    text = expression.sub(add_math, text)

    # 常见的向量/期望记号，即使没有显式运算符也应使用数学字体。
    special = re.compile(
        r"(?<![A-Za-z0-9_])(?:\|<I>-I\||<I>|<l>|"
        r"pHat|pBar|f_lBar|fs|Le|dx|W_X|p_Xi|p_Yi|Jforward|cCap|"
        r"alpha|beta|lambda|omega|epsilon|"
        r"[A-Za-zΩ][A-Za-z0-9_]*\.\.\.[A-Za-zΩ][A-Za-z0-9_]*|"
        r"[A-Za-zΩ][A-Za-z0-9_]*\([^()\n]{1,55}\)|"
        r"[A-Za-zΩ][A-Za-z0-9_]*_[A-Za-z0-9]+|"
        r"[A-Za-zΩ][A-Za-z0-9]*\^[A-Za-z0-9-]+|"
        r"\d+\s*[x×]\s*\d+(?:\s*[x×]\s*\d+)*|"
        r"O\([^()\n]+\)|\d+\s*/\s*\d+)"
        r"(?![A-Za-z0-9_])"
    )

    def add_special(match):
        raw = match.group(0)
        bare = raw.strip()
        if bare in INLINE_RESERVED:
            return raw
        return add_math(match)

    text = special.sub(add_special, text)

    # 单独出现的变量（“p 与 f”“M 个样本”“X 表示随机变量”等）使用斜体数学字体。
    single = re.compile(
        r"(?<![A-Za-z0-9_])([AIJLMNPTUVWXYabcdfhijklmnpqrstuvwxyzΩ])"
        r"(?=(?:\s*(?:[与和为是表示值个中在从对让使若当被由向随趋覆盖接近非零有效相同不同输入输出样本候选变量分布函数积分概率密度权重支撑支持处时后前上下一起即可会将的、，。；：:,.!?()])))"
    )

    def add_single(match):
        return add_math(match)

    text = single.sub(add_single, text)

    escaped = html.escape(text, quote=False)
    escaped = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", escaped)
    escaped = re.sub(r"(\ue010\d+\ue001)", lambda m: m.group(0), escaped)

    # 恢复图片和链接占位符。显式公式占位符存的是路径，统一重新读取尺寸。
    for key, value in protected.items():
        if key.startswith("\ue010"):
            safe = html.escape(value, quote=True)
            replacement = f'<link href="{safe}" color="#1C7188">{html.escape(value, quote=False)}</link>'
        elif key.startswith("\ue020"):
            with PILImage.open(value) as rendered:
                pw, ph = rendered.size
            replacement = f'<img src="{html.escape(value, quote=True)}" width="{pw * 72 / 300:.2f}" height="{ph * 72 / 300:.2f}" valign="-2.2"/>'
        else:
            replacement = value
        escaped = escaped.replace(html.escape(key, quote=False), replacement)
    return escaped


def make_paragraph(markup: str, style: ParagraphStyle) -> Paragraph:
    # ReportLab 的 CJK 分词器在一个段落中混排多个 inline <img> 时会产生空字符异常；
    # LTR 分词器仍能按译文中的空格换行，并可稳定处理数学图片。
    if "<img " in markup and style.wordWrap == "CJK":
        style = ParagraphStyle(f"{style.name}Inline", parent=style, wordWrap="LTR")
    return Paragraph(markup, style)


def parse_blocks(raw: str):
    lines = raw.splitlines()
    blocks = []
    paragraph = []
    code = []
    in_code = False
    code_language = ""

    def flush_paragraph():
        nonlocal paragraph
        if paragraph:
            blocks.append(("p", " ".join(line.strip() for line in paragraph)))
            paragraph = []

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("```"):
            if in_code:
                blocks.append(("math" if code_language == "math" else "code", "\n".join(code)))
                code = []
                in_code = False
                code_language = ""
            else:
                flush_paragraph()
                in_code = True
                code_language = stripped[3:].strip().lower()
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
        "body": ParagraphStyle("BodyZH", parent=base["BodyText"], fontName="YaHei", fontSize=9.4, leading=14.2, textColor=INK, alignment=TA_LEFT, wordWrap="CJK", spaceAfter=3.5),
        "bullet": ParagraphStyle("BulletZH", parent=base["BodyText"], fontName="YaHei", fontSize=9.2, leading=14.2, textColor=INK, leftIndent=5 * mm, firstLineIndent=-3 * mm, wordWrap="CJK", spaceAfter=2),
        "quote": ParagraphStyle("QuoteZH", parent=base["BodyText"], fontName="YaHei", fontSize=8.7, leading=14, textColor=BLUE, backColor=PALE, borderColor=CYAN, borderWidth=0.7, borderPadding=7, leftIndent=3 * mm, rightIndent=3 * mm, wordWrap="CJK", spaceBefore=3, spaceAfter=6),
        "code": ParagraphStyle("CodeZH", parent=base["Code"], fontName="YaHei", fontSize=7.5, leading=10.5, textColor=colors.HexColor("#24343E"), backColor=colors.HexColor("#EEF3F5"), borderPadding=5, leftIndent=2 * mm, rightIndent=2 * mm, spaceBefore=3, spaceAfter=5),
        "equation_number": ParagraphStyle("EquationNumber", parent=base["BodyText"], fontName="YaHei", fontSize=9.5, leading=13, textColor=INK, alignment=TA_RIGHT),
        "algorithm_title": ParagraphStyle("AlgorithmTitle", parent=base["BodyText"], fontName="YaHeiBold", fontSize=9.5, leading=13, textColor=INK, spaceBefore=2, spaceAfter=4, wordWrap="CJK"),
        "small": ParagraphStyle("SmallZH", parent=base["BodyText"], fontName="YaHei", fontSize=8.2, leading=13, textColor=MUTED, wordWrap="CJK"),
    }


def render_math_line(latex: str, cache_dir: Path) -> Path:
    cache_dir.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256(latex.encode("utf-8")).hexdigest()[:20]
    output = cache_dir / f"formula-{digest}.png"
    if output.is_file():
        return output

    figure = Figure(figsize=(12, 1.2), dpi=300)
    FigureCanvasAgg(figure)
    figure.patch.set_alpha(0)
    figure.text(0.01, 0.12, f"${latex}$", fontsize=13.5, color="#18242D")
    figure.savefig(
        output,
        dpi=300,
        transparent=True,
        bbox_inches="tight",
        pad_inches=0.025,
    )
    return output


def make_math_block(text: str, styles, page_width: float, cache_dir: Path):
    tag_match = re.search(r"(?m)^\\tag\{([^}]+)\}\s*$", text)
    tag = tag_match.group(1) if tag_match else ""
    if tag_match:
        text = text[:tag_match.start()] + text[tag_match.end():]
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    if not lines:
        raise ValueError("数学公式块不能为空")

    number_width = 18 * mm if tag else 0
    formula_width = page_width - number_width
    formula_rows = []
    for line in lines:
        image_path = render_math_line(line, cache_dir)
        with PILImage.open(image_path) as rendered:
            pixel_width, pixel_height = rendered.size
        width = pixel_width * 72 / 300
        height = pixel_height * 72 / 300
        maximum_width = formula_width - 5 * mm
        if width > maximum_width:
            scale = maximum_width / width
            width *= scale
            height *= scale
        formula = Image(str(image_path), width=width, height=height)
        formula.hAlign = "CENTER"
        formula_rows.append([formula])

    formula_table = Table(
        formula_rows,
        colWidths=[formula_width],
        style=TableStyle([
            ("ALIGN", (0, 0), (-1, -1), "CENTER"),
            ("LEFTPADDING", (0, 0), (-1, -1), 0),
            ("RIGHTPADDING", (0, 0), (-1, -1), 0),
            ("TOPPADDING", (0, 0), (-1, -1), 0.5 * mm),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 0.5 * mm),
        ]),
    )
    row = [formula_table]
    widths = [formula_width]
    if tag:
        row.append(Paragraph(f"({html.escape(tag)})", styles["equation_number"]))
        widths.append(number_width)
    table = Table(
        [row],
        colWidths=widths,
        style=TableStyle([
            ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
            ("ALIGN", (-1, 0), (-1, 0), "RIGHT"),
            ("LEFTPADDING", (0, 0), (-1, -1), 0),
            ("RIGHTPADDING", (0, 0), (-1, -1), 0),
            ("TOPPADDING", (0, 0), (-1, -1), 0.8 * mm),
            ("BOTTOMPADDING", (0, 0), (-1, -1), 0.8 * mm),
        ]),
    )
    return KeepTogether([Spacer(1, 0.5 * mm), table, Spacer(1, 0.5 * mm)])


def build_story(source_md: Path, thumbs: Path, styles, page_width: float, math_cache: Path):
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
         [Paragraph("逐页对应原稿 · 数学公式按原式重排 · 保留编号、算法、图注、术语与参考文献", styles["subtitle"])]],
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
        tw = 39 * mm
        th = tw * ih / iw
        thumb = Image(str(thumb_path), width=tw, height=th)
        note = Paragraph(
            "<b>对照说明</b><br/>本节译文对应左侧原稿整页。数学公式已按原式重排并保留原编号；图形与边栏提示可放大左侧缩略图，或在随附英文原稿中核对。",
            styles["small"],
        )
        compare = Table([[thumb, note]], colWidths=[44 * mm, page_width - 44 * mm], style=TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (1, 0), (1, 0), colors.HexColor("#F2F7F9")), ("BOX", (1, 0), (1, 0), 0.5, colors.HexColor("#D4E5EB")), ("LEFTPADDING", (1, 0), (1, 0), 5 * mm), ("RIGHTPADDING", (1, 0), (1, 0), 5 * mm), ("TOPPADDING", (1, 0), (1, 0), 5 * mm), ("BOTTOMPADDING", (1, 0), (1, 0), 5 * mm)]))
        story.extend([compare, Spacer(1, 3 * mm)])

        blocks = parse_blocks(page_text)
        pending_algorithm_title = ""
        for block_index, (kind, text) in enumerate(blocks):
            if kind == "math":
                story.append(make_math_block(text, styles, page_width, math_cache))
            elif kind == "code":
                story.append(AlgorithmBlock(text, page_width, pending_algorithm_title))
                pending_algorithm_title = ""
            elif kind == "h1":
                story.append(make_paragraph(inline_markup(text, math_cache), styles["H1"]))
            elif kind == "h2":
                next_kind = blocks[block_index + 1][0] if block_index + 1 < len(blocks) else ""
                if text.startswith("算法 ") and next_kind == "code":
                    pending_algorithm_title = text
                elif text.startswith("算法 "):
                    story.append(make_paragraph(inline_markup(text, math_cache), styles["algorithm_title"]))
                else:
                    story.append(make_paragraph(inline_markup(text, math_cache), styles["H2"]))
            elif kind == "h3":
                story.append(make_paragraph(inline_markup(text, math_cache), styles["H3"]))
            elif kind == "quote":
                story.append(make_paragraph(inline_markup(text, math_cache), styles["quote"]))
            elif kind == "bullet":
                story.append(make_paragraph("• " + inline_markup(text, math_cache), styles["bullet"]))
            elif kind == "number":
                story.append(make_paragraph(inline_markup(text, math_cache), styles["bullet"]))
            else:
                make = inline_markup(text, math_cache)
                story.append(make_paragraph(make, styles["body"]))
    return story


def main():
    source_md = Path(sys.argv[1])
    thumbs = Path(sys.argv[2])
    output = Path(sys.argv[3])
    output.parent.mkdir(parents=True, exist_ok=True)
    math_cache = source_md.parent / "math-cache"
    math_cache.mkdir(parents=True, exist_ok=True)

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
    story = build_story(source_md, thumbs, styles, doc.width, math_cache)
    doc.build(story)


if __name__ == "__main__":
    main()
