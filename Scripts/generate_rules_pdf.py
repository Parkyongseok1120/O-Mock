from __future__ import annotations

import os
from pathlib import Path
from typing import Iterable, Sequence

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
    Spacer,
    Table,
    TableStyle,
)
from reportlab.lib.utils import ImageReader


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "output" / "pdf"
TMP_DIR = ROOT / "tmp" / "pdfs"
OUT_PATH = OUT_DIR / "O-Mock_게임_및_미니게임_규칙서_KO.pdf"

SCREEN_MENU = ROOT / "Saved" / "Screenshots" / "MainMenu_BotSetting_AlignedFinal.png"
SCREEN_LOBBY = ROOT / "Saved" / "Screenshots" / "LockedClient_CorrectRetry.png"
SCREEN_INVENTORY = ROOT / "Saved" / "Screenshots" / "InventoryReplacement_Final2.png"

PAGE_W, PAGE_H = A4
MARGIN_X = 17 * mm
MARGIN_TOP = 18 * mm
MARGIN_BOTTOM = 16 * mm
CONTENT_W = PAGE_W - 2 * MARGIN_X

INK = colors.HexColor("#17251F")
MUTED = colors.HexColor("#65746D")
FOREST = colors.HexColor("#0D5E43")
FOREST_DARK = colors.HexColor("#083B2D")
MINT = colors.HexColor("#DDF2E7")
CREAM = colors.HexColor("#F8F4E8")
GOLD = colors.HexColor("#C9972E")
ORANGE = colors.HexColor("#C86132")
RED = colors.HexColor("#B33A3A")
CYAN = colors.HexColor("#38B7C8")
MAGENTA = colors.HexColor("#C33F9D")
GREEN = colors.HexColor("#2E9E61")
LIGHT_LINE = colors.HexColor("#D7DED9")
WHITE = colors.white


def register_fonts() -> tuple[str, str]:
    candidates = [
        (Path(r"C:\Windows\Fonts\malgun.ttf"), Path(r"C:\Windows\Fonts\malgunbd.ttf")),
        (Path(r"C:\Windows\Fonts\NanumGothic.ttf"), Path(r"C:\Windows\Fonts\NanumGothicBold.ttf")),
    ]
    for regular, bold in candidates:
        if regular.exists() and bold.exists():
            pdfmetrics.registerFont(TTFont("OMockKR", str(regular)))
            pdfmetrics.registerFont(TTFont("OMockKRBold", str(bold)))
            pdfmetrics.registerFontFamily("OMockKR", normal="OMockKR", bold="OMockKRBold")
            return "OMockKR", "OMockKRBold"
    raise FileNotFoundError("A Korean TrueType font was not found in C:\\Windows\\Fonts")


FONT, FONT_BOLD = register_fonts()


styles = getSampleStyleSheet()
BODY = ParagraphStyle(
    "BodyKR",
    parent=styles["BodyText"],
    fontName=FONT,
    fontSize=9.15,
    leading=14.0,
    textColor=INK,
    spaceAfter=2.6 * mm,
    wordWrap="CJK",
)
BODY_SMALL = ParagraphStyle(
    "BodySmallKR",
    parent=BODY,
    fontSize=8.15,
    leading=12.0,
    spaceAfter=1.7 * mm,
)
BODY_TINY = ParagraphStyle(
    "BodyTinyKR",
    parent=BODY,
    fontSize=7.2,
    leading=9.7,
    spaceAfter=1.0 * mm,
)
TITLE = ParagraphStyle(
    "TitleKR",
    parent=BODY,
    fontName=FONT_BOLD,
    fontSize=26,
    leading=33,
    textColor=WHITE,
    alignment=TA_LEFT,
    spaceAfter=4 * mm,
)
SUBTITLE = ParagraphStyle(
    "SubtitleKR",
    parent=BODY,
    fontSize=11.3,
    leading=17,
    textColor=colors.HexColor("#E8F5EF"),
    spaceAfter=2 * mm,
)
H1 = ParagraphStyle(
    "H1KR",
    parent=BODY,
    fontName=FONT_BOLD,
    fontSize=17.5,
    leading=23,
    textColor=FOREST_DARK,
    spaceBefore=0,
    spaceAfter=4.2 * mm,
)
H2 = ParagraphStyle(
    "H2KR",
    parent=BODY,
    fontName=FONT_BOLD,
    fontSize=11.4,
    leading=16,
    textColor=FOREST,
    spaceBefore=2 * mm,
    spaceAfter=2.2 * mm,
)
H3 = ParagraphStyle(
    "H3KR",
    parent=BODY,
    fontName=FONT_BOLD,
    fontSize=9.8,
    leading=14,
    textColor=INK,
    spaceBefore=1.2 * mm,
    spaceAfter=1.3 * mm,
)
CALLOUT = ParagraphStyle(
    "CalloutKR",
    parent=BODY,
    fontName=FONT_BOLD,
    fontSize=9.4,
    leading=14.2,
    textColor=FOREST_DARK,
    leftIndent=3 * mm,
    rightIndent=3 * mm,
    spaceAfter=0,
)
CAPTION = ParagraphStyle(
    "CaptionKR",
    parent=BODY,
    fontSize=7.4,
    leading=10.2,
    textColor=MUTED,
    alignment=TA_CENTER,
    spaceAfter=2.2 * mm,
)
CELL = ParagraphStyle(
    "CellKR",
    parent=BODY_SMALL,
    fontSize=7.7,
    leading=10.6,
    spaceAfter=0,
)
CELL_BOLD = ParagraphStyle(
    "CellBoldKR",
    parent=CELL,
    fontName=FONT_BOLD,
    textColor=FOREST_DARK,
)
HEADER_CELL = ParagraphStyle(
    "HeaderCellKR",
    parent=CELL,
    fontName=FONT_BOLD,
    textColor=WHITE,
    alignment=TA_CENTER,
)
NUMBER_BADGE = ParagraphStyle(
    "NumberBadgeKR",
    parent=BODY,
    fontName=FONT_BOLD,
    fontSize=8,
    leading=9,
    textColor=WHITE,
    alignment=TA_CENTER,
    spaceAfter=0,
)


def para(text: str, style: ParagraphStyle = BODY) -> Paragraph:
    return Paragraph(text, style)


def bullet(text: str, level: int = 0, style: ParagraphStyle = BODY) -> Paragraph:
    s = ParagraphStyle(
        f"bullet-{level}-{style.name}",
        parent=style,
        leftIndent=(4 + level * 4) * mm,
        firstLineIndent=-2.8 * mm,
        spaceAfter=1.2 * mm,
    )
    return Paragraph(f"•&nbsp;&nbsp;{text}", s)


def callout(text: str, bg=MINT, stroke=colors.HexColor("#B9D8C8")) -> Table:
    t = Table([[para(text, CALLOUT)]], colWidths=[CONTENT_W], hAlign="LEFT")
    t.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), bg),
                ("BOX", (0, 0), (-1, -1), 0.7, stroke),
                ("LEFTPADDING", (0, 0), (-1, -1), 3.2 * mm),
                ("RIGHTPADDING", (0, 0), (-1, -1), 3.2 * mm),
                ("TOPPADDING", (0, 0), (-1, -1), 2.8 * mm),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 2.8 * mm),
            ]
        )
    )
    return t


def section_title(number: str, title: str, kicker: str) -> list[Flowable]:
    badge = Table([[para(number, NUMBER_BADGE)]], colWidths=[9 * mm], rowHeights=[9 * mm])
    badge.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, -1), FOREST),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("BOX", (0, 0), (-1, -1), 0, FOREST),
            ]
        )
    )
    title_block = Table(
        [[badge, para(title, H1)]],
        colWidths=[12 * mm, CONTENT_W - 12 * mm],
        hAlign="LEFT",
    )
    title_block.setStyle(
        TableStyle(
            [
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 0),
                ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                ("TOPPADDING", (0, 0), (-1, -1), 0),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 0),
            ]
        )
    )
    return [title_block, para(kicker, ParagraphStyle("Kicker", parent=BODY, textColor=MUTED, fontSize=8.5, leading=12.5)), Spacer(1, 1.5 * mm)]


def kr_table(
    rows: Sequence[Sequence[str | Paragraph]],
    widths: Sequence[float],
    header: bool = True,
    font_size: float = 7.7,
) -> Table:
    converted = []
    for ridx, row in enumerate(rows):
        converted.append(
            [
                value
                if isinstance(value, Paragraph)
                else para(str(value), HEADER_CELL if header and ridx == 0 else CELL)
                for value in row
            ]
        )
    t = Table(converted, colWidths=list(widths), repeatRows=1 if header else 0, hAlign="LEFT")
    commands = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("GRID", (0, 0), (-1, -1), 0.35, LIGHT_LINE),
        ("LEFTPADDING", (0, 0), (-1, -1), 2.2 * mm),
        ("RIGHTPADDING", (0, 0), (-1, -1), 2.2 * mm),
        ("TOPPADDING", (0, 0), (-1, -1), 2.0 * mm),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 2.0 * mm),
        ("BACKGROUND", (0, 1 if header else 0), (-1, -1), colors.white),
    ]
    if header:
        commands.extend(
            [
                ("BACKGROUND", (0, 0), (-1, 0), FOREST_DARK),
                ("VALIGN", (0, 0), (-1, 0), "MIDDLE"),
            ]
        )
    for row in range(1 if header else 0, len(rows)):
        if (row - (1 if header else 0)) % 2 == 1:
            commands.append(("BACKGROUND", (0, row), (-1, row), colors.HexColor("#F4F8F5")))
    t.setStyle(TableStyle(commands))
    return t


def screenshot(path: Path, width: float, caption: str) -> list[Flowable]:
    if not path.exists():
        return [callout(f"스크린샷 파일을 찾을 수 없음: {path.name}", bg=colors.HexColor("#FDE9E2"), stroke=ORANGE)]
    iw, ih = ImageReader(str(path)).getSize()
    img = Image(str(path), width=width, height=width * ih / iw, lazy=0)
    img.hAlign = "CENTER"
    return [img, Spacer(1, 1.2 * mm), para(caption, CAPTION)]


class FiveDirectionsDiagram(Flowable):
    def __init__(self, width: float, height: float = 61 * mm):
        super().__init__()
        self.width = width
        self.height = height

    def draw(self):
        c = self.canv
        c.saveState()
        c.setFillColor(colors.HexColor("#FBF2D7"))
        c.roundRect(0, 0, self.width, self.height, 4 * mm, stroke=0, fill=1)
        c.setStrokeColor(colors.HexColor("#CAA95B"))
        c.setLineWidth(0.45)
        grid_w = 56 * mm
        cell = 7 * mm
        ox = 7 * mm
        oy = 5 * mm
        for i in range(9):
            c.line(ox + i * cell, oy, ox + i * cell, oy + 8 * cell)
            c.line(ox, oy + i * cell, ox + 8 * cell, oy + i * cell)
        lines = [
            [(1, 6), (2, 6), (3, 6), (4, 6), (5, 6)],
            [(7, 1), (7, 2), (7, 3), (7, 4), (7, 5)],
            [(1, 1), (2, 2), (3, 3), (4, 4), (5, 5)],
            [(3, 5), (4, 4), (5, 3), (6, 2), (7, 1)],
        ]
        line_colors = [FOREST, GOLD, RED, CYAN]
        for points, color in zip(lines, line_colors):
            c.setFillColor(color)
            for x, y in points:
                c.circle(ox + x * cell, oy + y * cell, 2.3 * mm, stroke=0, fill=1)
        tx = 73 * mm
        c.setFont(FONT_BOLD, 10)
        c.setFillColor(FOREST_DARK)
        c.drawString(tx, self.height - 12 * mm, "승리하는 네 방향")
        c.setFont(FONT, 8.1)
        c.setFillColor(INK)
        labels = [
            (FOREST, "가로: 좌우가 이어진 5개 이상"),
            (GOLD, "세로: 상하가 이어진 5개 이상"),
            (RED, "대각선 ↘: 같은 색 5개 이상"),
            (CYAN, "대각선 ↗: 같은 색 5개 이상"),
        ]
        y = self.height - 23 * mm
        for color, label in labels:
            c.setFillColor(color)
            c.circle(tx + 2 * mm, y + 1.2 * mm, 1.6 * mm, stroke=0, fill=1)
            c.setFillColor(INK)
            c.drawString(tx + 7 * mm, y - 1.2 * mm, label)
            y -= 9 * mm
        c.restoreState()


class FlowStrip(Flowable):
    def __init__(self, labels: Sequence[str], width: float, accent=FOREST):
        super().__init__()
        self.labels = labels
        self.width = width
        self.height = 27 * mm
        self.accent = accent

    def draw(self):
        c = self.canv
        c.saveState()
        gap = 4 * mm
        arrow = 6 * mm
        box_w = (self.width - (len(self.labels) - 1) * (gap + arrow)) / len(self.labels)
        x = 0
        for idx, label in enumerate(self.labels):
            c.setFillColor(colors.HexColor("#F1F7F3"))
            c.setStrokeColor(self.accent)
            c.setLineWidth(0.8)
            c.roundRect(x, 3 * mm, box_w, 20 * mm, 2.5 * mm, stroke=1, fill=1)
            c.setFillColor(self.accent)
            c.setFont(FONT_BOLD, 7.2)
            lines = label.split("\n")
            y = 15.2 * mm + (len(lines) - 1) * 2.2 * mm
            for line in lines:
                c.drawCentredString(x + box_w / 2, y, line)
                y -= 4.8 * mm
            x += box_w
            if idx < len(self.labels) - 1:
                c.setStrokeColor(self.accent)
                c.setFillColor(self.accent)
                start_x = x + 1.2 * mm
                end_x = x + gap + arrow - 1.2 * mm
                mid_y = 13 * mm
                c.line(start_x, mid_y, end_x, mid_y)
                c.line(end_x, mid_y, end_x - 2 * mm, mid_y + 1.5 * mm)
                c.line(end_x, mid_y, end_x - 2 * mm, mid_y - 1.5 * mm)
                x += gap + arrow
        c.restoreState()


class MiniGameDiagram(Flowable):
    def __init__(self, width: float):
        super().__init__()
        self.width = width
        self.height = 71 * mm

    def draw(self):
        c = self.canv
        c.saveState()
        c.setFillColor(colors.HexColor("#F7F0DD"))
        c.roundRect(0, 0, self.width, self.height, 4 * mm, fill=1, stroke=0)
        cell = 7.2 * mm
        ox = 9 * mm
        oy = 10 * mm
        c.setStrokeColor(colors.HexColor("#B89551"))
        c.setLineWidth(0.5)
        for i in range(8):
            c.line(ox + i * cell, oy, ox + i * cell, oy + 7 * cell)
            c.line(ox, oy + i * cell, ox + 7 * cell, oy + i * cell)
        # An illustrative puzzle, deliberately not the shipped answer layout.
        stones = [(1, 3), (2, 3), (3, 3), (4, 3), (2, 1), (2, 2), (4, 5)]
        for index, (x, y) in enumerate(stones):
            c.setFillColor(FOREST_DARK if index < 4 else colors.HexColor("#EDEDE9"))
            c.circle(ox + (x + 0.5) * cell, oy + (y + 0.5) * cell, 2.3 * mm, fill=1, stroke=0)
        c.setFillColor(colors.HexColor("#E4A743"))
        c.setStrokeColor(GOLD)
        c.setLineWidth(1.2)
        c.circle(ox + 5.5 * cell, oy + 3.5 * cell, 2.7 * mm, fill=0, stroke=1)
        tx = 75 * mm
        c.setFillColor(FOREST_DARK)
        c.setFont(FONT_BOLD, 10)
        c.drawString(tx, self.height - 13 * mm, "승리 수 찾기 · 7×7")
        c.setFont(FONT, 8.2)
        c.setFillColor(INK)
        lines = [
            "1. 빈칸 중 오목을 완성하는 한 칸을 찾는다.",
            "2. 제한 시간은 8초, 제출 기회는 1회다.",
            "3. 서버가 제출 순서와 정답을 확정한다.",
            "4. 모든 제출 또는 시간 종료 후 3초간 결과 표시.",
        ]
        y = self.height - 25 * mm
        for line in lines:
            c.drawString(tx, y, line)
            y -= 8.4 * mm
        c.setFont(FONT, 6.8)
        c.setFillColor(MUTED)
        c.drawString(9 * mm, 4.3 * mm, "※ 위 그림은 규칙 설명용 예시이며 실제 퍼즐 정답 배치가 아니다.")
        c.restoreState()


class NetworkDiagram(Flowable):
    def __init__(self, width: float):
        super().__init__()
        self.width = width
        self.height = 69 * mm

    def draw_box(self, c, x, y, w, h, title, lines, fill):
        c.setFillColor(fill)
        c.setStrokeColor(colors.HexColor("#9BB9AA"))
        c.setLineWidth(0.7)
        c.roundRect(x, y, w, h, 3 * mm, fill=1, stroke=1)
        c.setFillColor(FOREST_DARK)
        c.setFont(FONT_BOLD, 8.5)
        c.drawCentredString(x + w / 2, y + h - 7 * mm, title)
        c.setFont(FONT, 7.2)
        c.setFillColor(INK)
        ty = y + h - 14 * mm
        for line in lines:
            c.drawCentredString(x + w / 2, ty, line)
            ty -= 5.2 * mm

    def draw(self):
        c = self.canv
        c.saveState()
        client_w = 47 * mm
        server_w = 61 * mm
        h = 47 * mm
        y = 11 * mm
        self.draw_box(c, 0, y, client_w, h, "클라이언트 PC", ["입력 요청", "공개 보드 표시", "본인 인벤토리만 표시"], colors.HexColor("#EDF5F9"))
        self.draw_box(c, self.width - client_w, y, client_w, h, "클라이언트 PC", ["입력 요청", "복제 상태 수신", "예측 UI 계산"], colors.HexColor("#EDF5F9"))
        sx = (self.width - server_w) / 2
        self.draw_box(c, sx, y, server_w, h, "호스트 · 리슨 서버", ["턴·시간·대상 검증", "아이템 비용·효과 확정", "승리·미니게임·통계 기록"], colors.HexColor("#DDF2E7"))
        c.setStrokeColor(GOLD)
        c.setFillColor(GOLD)
        c.setLineWidth(1.1)
        for x1, x2 in [(client_w, sx), (sx + server_w, self.width - client_w)]:
            mid = (x1 + x2) / 2
            c.line(x1 + 1 * mm, y + h / 2, x2 - 1 * mm, y + h / 2)
            c.line(mid, y + h / 2, mid - 2 * mm, y + h / 2 + 1.5 * mm)
            c.line(mid, y + h / 2, mid - 2 * mm, y + h / 2 - 1.5 * mm)
        c.setFont(FONT_BOLD, 7.2)
        c.setFillColor(ORANGE)
        c.drawCentredString(self.width / 2, 4 * mm, "최종 판정은 항상 호스트 서버가 수행한다")
        c.restoreState()


class RulesDocTemplate(BaseDocTemplate):
    def __init__(self, filename: str):
        super().__init__(
            filename,
            pagesize=A4,
            leftMargin=MARGIN_X,
            rightMargin=MARGIN_X,
            topMargin=MARGIN_TOP,
            bottomMargin=MARGIN_BOTTOM,
            title="O-Mock 게임 및 미니게임 상세 규칙서",
            author="O-Mock Project",
            subject="오목, 아이템, LAN 로비, 미니게임 및 밸런스 통계 규칙",
        )
        frame = Frame(
            MARGIN_X,
            MARGIN_BOTTOM,
            CONTENT_W,
            PAGE_H - MARGIN_TOP - MARGIN_BOTTOM,
            id="body",
            leftPadding=0,
            rightPadding=0,
            topPadding=0,
            bottomPadding=0,
        )
        self.addPageTemplates([PageTemplate(id="rules", frames=[frame], onPage=self.draw_page)])

    @staticmethod
    def draw_page(canvas, doc):
        if doc.page == 1:
            draw_cover(canvas, doc)
            return
        canvas.saveState()
        canvas.setStrokeColor(LIGHT_LINE)
        canvas.setLineWidth(0.45)
        canvas.line(MARGIN_X, PAGE_H - 12 * mm, PAGE_W - MARGIN_X, PAGE_H - 12 * mm)
        canvas.setFont(FONT_BOLD, 7.4)
        canvas.setFillColor(FOREST_DARK)
        canvas.drawString(MARGIN_X, PAGE_H - 9.1 * mm, "O-MOCK · 3D LAN GOMOK")
        canvas.setFont(FONT, 7.2)
        canvas.setFillColor(MUTED)
        canvas.drawRightString(PAGE_W - MARGIN_X, PAGE_H - 9.1 * mm, "게임 및 미니게임 상세 규칙서")
        canvas.line(MARGIN_X, 11 * mm, PAGE_W - MARGIN_X, 11 * mm)
        canvas.setFont(FONT, 7.0)
        canvas.setFillColor(MUTED)
        canvas.drawString(MARGIN_X, 6.8 * mm, "구현 기준: 2026-08-23 · 프로토타입 규칙 v1.0")
        canvas.drawRightString(PAGE_W - MARGIN_X, 6.8 * mm, f"{doc.page}")
        canvas.restoreState()


def draw_cover(canvas, doc):
    canvas.saveState()
    if SCREEN_MENU.exists():
        reader = ImageReader(str(SCREEN_MENU))
        iw, ih = reader.getSize()
        scale = max(PAGE_W / iw, PAGE_H / ih)
        dw, dh = iw * scale, ih * scale
        canvas.drawImage(reader, (PAGE_W - dw) / 2, (PAGE_H - dh) / 2, dw, dh, mask="auto")
    else:
        canvas.setFillColor(colors.HexColor("#84B97A"))
        canvas.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)
    canvas.setFillColor(colors.Color(0.02, 0.10, 0.075, alpha=0.82))
    canvas.setFillAlpha(0.86)
    canvas.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)
    canvas.setFillAlpha(1)
    canvas.setFillColor(GOLD)
    canvas.rect(18 * mm, 49 * mm, 3 * mm, 168 * mm, fill=1, stroke=0)
    canvas.setFillColor(WHITE)
    canvas.setFont(FONT_BOLD, 10)
    canvas.drawString(28 * mm, 217 * mm, "O-MOCK · 3D LAN GOMOK")
    canvas.setFont(FONT_BOLD, 25)
    canvas.drawString(28 * mm, 193 * mm, "게임 및 미니게임")
    canvas.drawString(28 * mm, 178 * mm, "상세 규칙서")
    canvas.setFillColor(colors.HexColor("#DDEBE4"))
    canvas.setFont(FONT, 10.5)
    canvas.drawString(28 * mm, 161 * mm, "2~4인 오목 · 아이템 5종 · LAN 로비 · 승리 수 찾기")
    canvas.setStrokeColor(colors.HexColor("#83B9A1"))
    canvas.line(28 * mm, 151 * mm, 169 * mm, 151 * mm)
    canvas.setFont(FONT, 9)
    canvas.setFillColor(colors.HexColor("#C9DCD3"))
    cover_lines = [
        "이 문서는 현재 Shipping 빌드의 실제 구현을 기준으로 작성되었습니다.",
        "기본 규칙, 예외 처리, 조작법, 서버 권한, 밸런스 통계를 포함합니다.",
    ]
    y = 140 * mm
    for line in cover_lines:
        canvas.drawString(28 * mm, y, line)
        y -= 7 * mm
    canvas.setFillColor(colors.HexColor("#E9F4EE"))
    canvas.roundRect(28 * mm, 66 * mm, 141 * mm, 42 * mm, 4 * mm, fill=1, stroke=0)
    canvas.setFillColor(FOREST_DARK)
    canvas.setFont(FONT_BOLD, 9)
    canvas.drawString(36 * mm, 96 * mm, "한 문장 요약")
    canvas.setFont(FONT, 9)
    canvas.drawString(36 * mm, 86 * mm, "같은 색 돌 5개 이상을 먼저 잇되, 아이템과 5라운드마다")
    canvas.drawString(36 * mm, 79 * mm, "열리는 미니게임 보상을 이용해 흐름을 뒤집는 서버 권위 전략 게임")
    canvas.setFillColor(colors.HexColor("#B7CFC3"))
    canvas.setFont(FONT, 7.8)
    canvas.drawString(28 * mm, 43 * mm, "문서 버전 1.0 · 2026-08-23")
    canvas.restoreState()


def make_story() -> list[Flowable]:
    story: list[Flowable] = []

    # Cover occupies the first page via a zero-height flowable and explicit callback.
    story.append(Spacer(1, PAGE_H - MARGIN_TOP - MARGIN_BOTTOM - 1))
    story.append(PageBreak())

    story += section_title("01", "빠른 시작", "처음 실행한 사람이 2분 안에 한 판을 시작할 수 있도록 핵심만 먼저 정리합니다.")
    story.append(callout("<b>승리 조건:</b> 자신의 돌을 가로·세로·두 대각선 중 한 방향으로 <b>연속 5개 이상</b> 만들면 즉시 승리합니다. 돌을 놓은 직후와 견인·강탈로 보드가 바뀐 직후 서버가 다시 판정합니다."))
    story.append(Spacer(1, 3 * mm))
    story.append(FiveDirectionsDiagram(CONTENT_W))
    story.append(Spacer(1, 4 * mm))
    story.append(para("<b>한 판 시작 순서</b>", H2))
    story.append(FlowStrip(["메인 메뉴\n방 만들기/검색", "로비\n준비", "호스트\n경기 시작", "돌·아이템\n턴 진행", "5목/무승부\n결과"], CONTENT_W))
    story.append(Spacer(1, 2 * mm))
    quick_rows = [
        ["해야 할 일", "규칙"],
        ["자기 턴", "빈 교차점을 왼쪽 클릭해 돌을 놓습니다. 아이템은 카드 또는 숫자키로 선택합니다."],
        ["시간", "개인 시간과 턴 시간이 동시에 줄어듭니다. 둘 중 하나라도 0이 되면 서버가 자동 착수합니다."],
        ["미니게임", "5·10·15…라운드 종료 때 7×7 승리 수 찾기가 열립니다. 정답 순서대로 에너지를 받습니다."],
        ["경기 종료", "5목, 판이 가득 찬 무승부, 또는 기권으로 한 명만 남은 경우 종료됩니다."],
    ]
    story.append(kr_table(quick_rows, [35 * mm, CONTENT_W - 35 * mm]))
    story.append(PageBreak())

    story += section_title("02", "오목판·플레이어·승리", "기본 오목의 판정과 2~4인 확장 규칙입니다.")
    story.append(para("보드와 착수", H2))
    for text in [
        "지원 보드 템플릿은 <b>15×15, 17×17, 19×19, 21×21, 23×23</b> 정사각형입니다.",
        "4인 방은 공간 부족을 막기 위해 선택값이 작더라도 최소 <b>21×21</b>로 자동 보정됩니다.",
        "판 밖, 이미 돌이 있는 칸, 봉인된 칸에는 놓을 수 없습니다. 승리 또는 무승부가 확정된 뒤에는 추가 착수가 거부됩니다.",
        "플레이어는 좌석 1~4에 배정되며 각 좌석은 고유 돌 색을 사용합니다. 네트워크에서는 참가 순서대로 빈 좌석을 받습니다.",
    ]:
        story.append(bullet(text))
    story.append(para("5목 판정", H2))
    story.append(para("새로 놓이거나 아이템으로 바뀐 돌을 중심으로 네 방향의 같은 색 돌을 양쪽으로 셉니다. 합계가 5 이상이면 승리입니다. 정확히 5개로 제한하지 않으므로 6목 이상도 승리이며, 현재 프로토타입에는 금수·삼삼·장목 금지 같은 렌주 제한이 없습니다."))
    story.append(callout("<b>무승부:</b> 빈칸이 하나도 남지 않았고 누구도 5목을 만들지 못했을 때만 성립합니다. 봉인 칸도 보드가 찼는지 계산할 때 비어 있는 착수 가능 칸으로 취급되지 않습니다.", bg=CREAM, stroke=colors.HexColor("#D9C58D")))
    story.append(Spacer(1, 4 * mm))
    board_rows = [
        ["방 인원", "허용 보드", "자동 보정", "권장 용도"],
        ["2인", "15/17/19/21/23", "가장 가까운 지원 크기", "빠른 표준전"],
        ["3인", "15/17/19/21/23", "가장 가까운 지원 크기", "중형 이상 권장"],
        ["4인", "21/23", "15~19 선택 시 21", "다인 기본전"],
    ]
    story.append(kr_table(board_rows, [25 * mm, 43 * mm, 42 * mm, CONTENT_W - 110 * mm]))
    story.append(Spacer(1, 3 * mm))
    story += screenshot(SCREEN_INVENTORY, 133 * mm, "실제 21×21 네트워크 경기 화면. 공개 보드와 개인 HUD가 동시에 표시됩니다.")
    story.append(PageBreak())

    story += section_title("03", "턴·라운드·시간", "아이템과 기권이 섞여도 순서가 깨지지 않도록 활성 플레이어 기준으로 계산합니다.")
    story.append(para("턴과 라운드", H2))
    story.append(para("<b>턴</b>은 한 플레이어가 행동하는 구간이고, <b>라운드</b>는 현재 활성 플레이어가 모두 한 번씩 행동을 마친 구간입니다. 기권한 플레이어는 활성 목록에서 제외됩니다. 턴 건너뛰기 대상은 행동한 것으로 처리되어 라운드가 무한히 늘어지지 않습니다."))
    story.append(FlowStrip(["턴 시작\n잠금 해제·지급", "아이템 선택\n최대 1회", "돌 착수\n또는 시간 초과", "다음 활성\n플레이어", "전원 완료\n라운드 +1"], CONTENT_W, accent=GOLD))
    story.append(para("시간 두 종류", H2))
    time_rows = [
        ["시간", "기본값", "커스텀 범위", "작동 방식"],
        ["개인 총 시간", "120초", "30~600초", "자기 턴에 실제 흐른 만큼 감소합니다."],
        ["턴 제한 시간", "25초", "5~120초", "매 턴 0부터 시작하며 최대치에 닿으면 자동 착수합니다."],
        ["라운드 회복", "부족분의 35%", "고정", "라운드 종료 시 (최대−현재)×0.35만큼 회복합니다."],
    ]
    story.append(kr_table(time_rows, [31 * mm, 25 * mm, 30 * mm, CONTENT_W - 86 * mm]))
    story.append(para("시간 초과 자동 처리", H2))
    for idx, text in enumerate([
        "마지막으로 커서를 올린 칸이 여전히 유효한 빈칸이면 그곳에 둡니다.",
        "그렇지 않으면 중앙에서 맨해튼 거리가 가까운 빈칸을 찾고, 좌표 순서로 안정적으로 선택합니다.",
        "인벤토리 교체 제안이 남아 있으면 첫 번째 슬롯을 자동 교체한 뒤 자동 착수합니다. 방치된 클라이언트가 경기를 영구 정지시키지 못하게 하는 예외입니다.",
        "유효한 빈칸이 전혀 없으면 돌 없이 턴을 끝냅니다.",
    ], 1):
        story.append(para(f"<b>{idx}.</b> {text}"))
    story.append(callout("미니게임 진행 8초와 결과 표시 3초 동안에는 본판 개인 시간과 턴 시간이 정지합니다.", bg=colors.HexColor("#EAF1FA"), stroke=colors.HexColor("#AAC0D6")))
    story.append(PageBreak())

    story += section_title("04", "메인 메뉴·LAN 방·로비", "같은 공유기/로컬 네트워크에서 리슨 서버 방식으로 연결합니다.")
    story += screenshot(SCREEN_MENU, 158 * mm, "메인 메뉴: 방 설정, 검색 결과 수, 방별 인원·규칙·잠금 여부, 직접 IP 연결을 한 화면에서 확인합니다.")
    room_rows = [
        ["설정", "범위/선택", "적용 규칙"],
        ["최대 인원", "2~4명", "방 광고와 좌석 수에 적용"],
        ["봇", "0~3명", "빈 좌석에 투입하며 사람 참가자가 우선"],
        ["보드", "15/17/19/21/23", "4인은 최소 21"],
        ["개인/턴 시간", "30~600초 / 5~120초", "서버가 최종 적용"],
        ["아이템/미니게임", "각각 켜기·끄기", "클라이언트 요청보다 서버 설정 우선"],
        ["비밀번호", "최대 32자", "잠금 방은 방 목록에서 참가"],
    ]
    story.append(kr_table(room_rows, [31 * mm, 43 * mm, CONTENT_W - 74 * mm]))
    story.append(Spacer(1, 3 * mm))
    story.append(para("방 검색과 비밀번호", H2))
    story.append(para("방 목록은 현재/최대 인원, 예정 봇 수, 잠금 여부, 보드 크기, 개인·턴 시간, 아이템·미니게임 설정, 핑을 표시합니다. 잠금 방은 공개된 방별 salt와 입력한 비밀번호로 BLAKE3 증명을 만들고 서버의 PreLogin 단계에서 비교합니다. 평문 비밀번호는 세션 광고에 넣지 않습니다."))
    story.append(callout("<b>보안 범위:</b> 이 기능은 신뢰 가능한 LAN용 접근 제어입니다. 암호화된 인터넷 인증이나 계정 보안 시스템이 아니며, 경기 시작 후 재접속·호스트 이전은 지원하지 않습니다. 잠금 방은 직접 IP가 아니라 LAN 방 목록에서 참가해야 합니다.", bg=colors.HexColor("#FDECE5"), stroke=ORANGE))
    story.append(PageBreak())

    story += section_title("05", "로비 준비·네트워크 권한", "호스트만 경기를 시작하고, 경기 판정은 호스트 서버가 확정합니다.")
    story += screenshot(SCREEN_LOBBY, 130 * mm, "잠금 방에 잘못된 비밀번호로 거부된 뒤 올바른 비밀번호로 재시도해 입장한 2/4 로비.")
    story.append(para("로비 규칙", H2))
    for text in [
        "사람만 플레이할 때는 최소 2명이 필요합니다. 봇을 1명 이상 설정하면 호스트 1명도 시작할 수 있으며, 모든 사람 참가자는 READY 상태여야 합니다.",
        "경기 시작 시 빈 좌석에 예정 봇이 들어옵니다. 사람 참가자로 좌석이 채워지면 실제 생성되는 봇 수는 자동으로 줄어듭니다.",
        "호스트는 좌석 1이며 START MATCH를 실행할 수 있습니다. 일반 참가자는 준비/취소만 할 수 있습니다.",
        "경기 시작과 함께 방 광고와 진행 중 참가가 닫힙니다. 연결이 끊긴 플레이어는 즉시 기권 처리됩니다.",
        "2인전에서 한 명이 기권하면 남은 한 명이 즉시 승리합니다. 3~4인전은 활성 플레이어가 한 명 남을 때까지 계속됩니다.",
    ]:
        story.append(bullet(text))
    story.append(NetworkDiagram(CONTENT_W))
    privacy_rows = [
        ["모든 플레이어에게 공개", "소유자에게만 공개"],
        ["보드, 현재 턴, 라운드, 시간, 승자, 보호 칸, 공개 상태, 기권/준비", "보유 아이템 ID, 이번 턴 획득 잠금, 한 턴 사용 여부, 교체 대기 아이템"],
    ]
    story.append(KeepTogether([
        para("복제되는 정보와 숨겨지는 정보", H2),
        kr_table(privacy_rows, [CONTENT_W / 2, CONTENT_W / 2]),
    ]))
    story.append(PageBreak())

    story += section_title("06", "에너지·인벤토리·교체", "매 턴 자원이 들어오지만, 획득 직후 사용과 가득 찬 인벤토리 우회를 막습니다.")
    inventory_rows = [
        ["항목", "규칙"],
        ["에너지", "매 자기 턴 시작 +1, 최대 5. 아이템 비용과 미니게임 보상도 이 상한을 따릅니다."],
        ["아이템 지급", "아이템 활성 방에서 매 턴 시작 시 등록된 1~5번 중 보유하지 않은 아이템을 찾습니다."],
        ["인벤토리", "최대 2칸. 같은 아이템을 동시에 두 장 보유할 수 없습니다."],
        ["획득 잠금", "그 턴에 새로 받은 아이템은 NEW로 표시되며 다음 자기 턴 시작부터 사용 가능합니다."],
        ["사용 횟수", "한 턴에 최대 1개. 아이템 사용만으로 턴이 끝나지 않으며 이어서 돌을 둡니다."],
    ]
    story.append(kr_table(inventory_rows, [34 * mm, CONTENT_W - 34 * mm]))
    story.append(para("인벤토리가 가득 찼을 때", H2))
    story.append(FlowStrip(["새 아이템\n제안 표시", "기존 2개와\n효과 비교", "버릴 슬롯\n직접 클릭", "선택 아이템\n즉시 제거", "새 아이템\n잠금 상태 저장"], CONTENT_W, accent=ORANGE))
    story.append(Spacer(1, 2 * mm))
    for text in [
        "교체 대기 중에는 보드 착수와 아이템 사용이 서버에서 모두 거부됩니다. 보드 클릭·숫자키·변조 RPC로 건너뛸 수 없습니다.",
        "선택하지 않은 기존 아이템은 그대로 남습니다. 선택된 한 장만 제거되고 새 제안이 원자적으로 들어옵니다.",
        "교체된 새 아이템도 이번 턴 획득품이므로 다음 자기 턴까지 잠겨 있습니다.",
        "시간이 끝날 때까지 선택하지 않으면 서버가 첫 슬롯을 교체해 자동 착수합니다.",
    ]:
        story.append(bullet(text))
    story += screenshot(SCREEN_INVENTORY, 151 * mm, "실제 교체 UI: 새 제안의 이름·설명과 기존 두 카드를 비교한 뒤 버릴 카드를 누릅니다.")
    story.append(PageBreak())

    story += section_title("07", "아이템 1~2 · 봉인석과 견인", "대상 선택 → 서버 검증 → 효과 → 비용 차감과 로그 기록 순으로 처리됩니다.")
    item12_rows = [
        ["ID / 이름", "비용", "대상", "정확한 효과", "실패 조건"],
        ["1 · 봉인석", "1", "빈칸", "대상 칸을 현재 라운드 다음 경계까지 Blocked로 바꿉니다. 즉 1라운드 동안 착수할 수 없습니다.", "판 밖, 돌/봉인 존재, 에너지 부족, 이미 이번 턴 아이템 사용"],
        ["2 · 견인", "1", "빈칸", "대상 빈칸의 상·우·하·좌 인접 칸에서 자신의 돌을 찾아 그 빈칸으로 이동합니다. 이동 후 5목을 재판정합니다.", "인접 자기 돌 없음, 출발 돌 수호막, 대상이 빈칸 아님"],
    ]
    story.append(kr_table(item12_rows, [24 * mm, 14 * mm, 20 * mm, 67 * mm, CONTENT_W - 125 * mm]))
    story.append(para("봉인석 지속 시간 예시", H2))
    story.append(para("3라운드 도중 봉인석을 사용하면 만료 라운드는 4로 기록됩니다. 3라운드의 남은 플레이어와 4라운드가 끝날 때까지 막힌 상태가 유지되고, 4라운드 종료 효과 정리에서 다시 빈칸으로 돌아옵니다."))
    story.append(para("견인 출발 돌 결정 우선순위", H2))
    story.append(para("플레이어는 <b>도착할 빈칸</b>을 고릅니다. 도착 칸 주변에 자기 돌이 여러 개라면 구현상 다음 순서로 첫 유효 돌이 선택됩니다: <b>아래(0,+1) → 오른쪽(+1,0) → 위(0,−1) → 왼쪽(−1,0)</b>. 수호막이 있는 출발 돌은 후보에서 제외됩니다."))
    story.append(callout("견인 고급 예측: 카드를 선택하면 가능한 도착 칸이 <b>청록색</b>으로 나타납니다. 그 칸에 마우스를 올리면 실제 이동될 출발 돌이 <b>초록색</b>으로 표시됩니다.", bg=colors.HexColor("#E5F8FA"), stroke=CYAN))
    story.append(Spacer(1, 4 * mm))
    example_rows = [
        ["상황", "판정"],
        ["견인으로 자신의 4목 끝을 채움", "즉시 5목 승리 판정"],
        ["견인 출발 돌이 수호막 상태", "그 돌은 이동 후보가 아니므로 다른 인접 돌 탐색"],
        ["아이템 실행 도중 상태가 바뀌어 효과 실패", "에너지와 아이템을 복구하고 사용 횟수도 원상 복귀"],
    ]
    story.append(kr_table(example_rows, [57 * mm, CONTENT_W - 57 * mm]))
    story.append(PageBreak())

    story += section_title("08", "아이템 3~5 · 강탈·스킵·수호막", "공격·턴 제어·방어의 상호작용을 정확히 이해해야 합니다.")
    item345_rows = [
        ["ID / 이름", "비용", "대상", "효과"],
        ["3 · 강탈", "3", "상대 돌", "고립되고 수호막이 없는 상대 돌을 자신의 색으로 바꿉니다. 변환 직후 5목을 재판정합니다."],
        ["4 · 턴 건너뛰기", "3", "다음 활성 플레이어", "현재 진행 방향에서 바로 다음 플레이어의 다음 턴을 한 번 생략합니다. 카드 클릭 즉시 대상이 자동 결정됩니다."],
        ["5 · 수호막", "2", "자기 돌", "선택한 자기 돌을 2라운드 동안 견인·강탈 등 아이템 변화로부터 보호합니다."],
    ]
    story.append(kr_table(item345_rows, [30 * mm, 15 * mm, 35 * mm, CONTENT_W - 80 * mm]))
    story.append(para("‘고립된 돌’의 정확한 뜻", H2))
    story.append(para("대상 돌 주위의 <b>8칸(상하좌우와 네 대각선)</b>을 모두 봅니다. 그중 같은 색 돌이 하나라도 있으면 고립으로 보지 않습니다. 다른 색 돌, 빈칸, 봉인 칸은 같은 편 연결로 계산하지 않습니다. 판 가장자리 밖 좌표도 같은 색 돌이 아니므로 고립 판정에 방해되지 않습니다."))
    story.append(para("턴 건너뛰기 예외", H2))
    for text in [
        "대상은 임의로 고를 수 없고 현재 턴 방향의 다음 활성 플레이어로 고정됩니다.",
        "이미 스킵 예약이 걸린 같은 플레이어에게 다시 중첩할 수 없습니다.",
        "기권자는 활성 목록에서 빠지므로 자동으로 건너뜁니다.",
        "스킵이 소비되면 그 플레이어의 ‘다음 턴’만 사라지고 이후에는 정상 복귀합니다.",
    ]:
        story.append(bullet(text))
    story.append(para("수호막 지속과 보호 범위", H2))
    story.append(para("수호막은 사용 라운드 +2를 만료 시점으로 기록하며 해당 라운드 종료 정리까지 유지됩니다. 보호된 돌은 강탈 대상이 될 수 없고 견인의 출발 돌로도 사용할 수 없습니다. 빈칸이나 이미 보호된 돌에는 다시 걸 수 없습니다."))
    story.append(callout("강탈 고급 예측: 사용 가능한 고립 상대 돌은 <b>자홍색</b>으로 표시됩니다. 수호막 돌과 같은 색 돌에 연결된 돌은 미리보기 단계에서 제외됩니다.", bg=colors.HexColor("#F9E8F4"), stroke=MAGENTA))
    story.append(PageBreak())

    story += section_title("09", "고급 예측 UI·3D 카메라", "판정 결과를 사용 전에 읽을 수 있도록 공개 상태만으로 미리 계산합니다.")
    prediction_rows = [
        ["색", "표시 대상", "언제 보이는가"],
        ["금색", "현재 플레이어가 지금 두면 승리하는 빈칸", "경기 중 항상"],
        ["빨간색", "활성 상대가 다음 착수로 승리할 수 있는 위협 칸", "경기 중 항상; 기권 상대는 제외"],
        ["청록색", "견인 가능한 도착 빈칸", "견인 카드 선택 중"],
        ["초록색", "현재 마우스 도착 칸으로 실제 끌려갈 출발 돌", "견인 도착 후보에 호버"],
        ["자홍색", "강탈 가능한 고립·비보호 상대 돌", "강탈 카드 선택 중"],
    ]
    story.append(kr_table(prediction_rows, [24 * mm, 72 * mm, CONTENT_W - 96 * mm]))
    story.append(Spacer(1, 3 * mm))
    story.append(callout("예측은 서버의 비밀 인벤토리를 보지 않고, 모든 참가자에게 복제된 공개 보드·수호막·기권 상태만 사용합니다. 표시가 있어도 실제 클릭 순간 서버 상태가 바뀌었다면 서버 검증이 최종 우선입니다."))
    story.append(para("카메라 조작", H2))
    camera_rows = [
        ["입력", "동작", "세부"],
        ["오른쪽 버튼 드래그", "자유 회전", "Yaw 감도 0.85, Pitch 감도 0.70. 드래그 중 보드 입력은 하지 않습니다."],
        ["마우스 휠", "줌 인/아웃", "한 단계당 거리 배율을 약 0.86배로 조정. 기본 대비 0.32~2.15배 범위."],
        ["F", "카메라 초기화", "보드 크기에 맞춘 기본 회전·거리로 복귀"],
        ["왼쪽 클릭", "보드/아이템/미니게임 입력", "현재 단계에 따라 서버 요청 종류가 달라짐"],
    ]
    story.append(kr_table(camera_rows, [35 * mm, 31 * mm, CONTENT_W - 66 * mm]))
    story.append(para("메인 메뉴 연출", H2))
    story.append(para("메인 메뉴에서도 실제 3D 보드·언덕·나무·하늘을 보여주며 카메라가 낮은 속도로 자동 회전합니다. 게임에 들어가면 자동 회전 대신 사용자의 자유 회전과 줌이 우선합니다."))
    story.append(PageBreak())

    story += section_title("10", "미니게임 · 승리 수 찾기", "본판 규칙을 재사용한 7×7 퍼즐이며 5라운드마다 모든 활성 플레이어가 참여합니다.")
    story.append(MiniGameDiagram(CONTENT_W))
    story.append(para("시작 조건", H2))
    for text in [
        "미니게임 설정이 ON이고, 5·10·15…번째 라운드가 정상 종료되었으며, 아직 본판 승자가 없을 때 시작합니다.",
        "시작 즉시 본판의 돌 놓기와 아이템 사용이 잠기고 개인/턴 시간이 정지합니다.",
        "모든 플레이어에게 같은 7×7 퍼즐이 공개됩니다. 목표는 빈칸 하나를 골라 제시된 돌의 5목을 완성하는 것입니다.",
    ]:
        story.append(bullet(text))
    story.append(para("제출과 종료", H2))
    minigame_rows = [
        ["항목", "규칙"],
        ["제한 시간", "8초"],
        ["제출 기회", "활성 플레이어마다 1회. 틀린 답도 제출 완료로 기록됩니다."],
        ["네트워크", "각 PC가 독립적으로 클릭하며 서버가 도착 순서와 정답을 확정합니다."],
        ["로컬 핫시트", "하나의 마우스를 좌석 순서대로 넘겨 순차 제출합니다."],
        ["조기 종료", "모든 활성 플레이어가 제출하면 8초 전이라도 결과 단계로 이동합니다."],
        ["시간 종료", "미제출 플레이어가 있어도 결과 단계로 이동합니다."],
        ["결과 표시", "3초 동안 결과를 보여준 뒤 본판의 다음 턴을 시작합니다."],
    ]
    story.append(kr_table(minigame_rows, [33 * mm, CONTENT_W - 33 * mm]))
    story.append(para("보상", H2))
    reward_rows = [
        ["정답 순위", "에너지", "비고"],
        ["1번째 정답", "+3", "통계의 미니게임 승자 좌석으로 기록"],
        ["2번째 정답", "+2", "현재 에너지 5 상한 적용"],
        ["3번째 이후", "+1", "4인전 4번째 정답도 +1"],
        ["오답/미제출", "+0", "추가 벌점 없음"],
    ]
    story.append(kr_table(reward_rows, [40 * mm, 30 * mm, CONTENT_W - 70 * mm]))
    story.append(PageBreak())

    story += section_title("11", "봇·자동 테스트·밸런스 통계", "사람을 모으지 않아도 규칙 안정성과 장기 밸런스를 확인하기 위한 기능입니다.")
    story.append(para("봇 의사결정", H2))
    bot_rows = [
        ["우선순위", "행동"],
        ["1", "자신이 즉시 5목을 만들 수 있는 칸"],
        ["2", "상대의 즉시 승리 위협을 막는 칸"],
        ["3", "전술 점수와 중앙 거리를 고려한 유효 칸"],
        ["아이템", "설정 확률을 통과하면 강탈→견인→스킵→수호막→봉인석 순으로 사용 가능한 효과를 탐색"],
    ]
    story.append(kr_table(bot_rows, [28 * mm, CONTENT_W - 28 * mm]))
    story.append(para("봇 안전 규칙", H2))
    story.append(para("봇은 호스트 서버에서만 판단합니다. 현재 봇 턴이 아니면 에너지·아이템·보드를 변경하지 않으며, 인벤토리 교체 제안이 있으면 낮은 우선순위 아이템을 먼저 버려 모달을 해소합니다. 아이템은 서버의 정상 대상 검증·이벤트·통계 경로로 사용한 뒤, 같은 턴에 전술 수를 놓습니다."))
    story.append(para("단계 14 영속 통계", H2))
    stats_rows = [
        ["파일", "내용"],
        ["Saved/Balance/O_Mock_Matches.csv", "경기별 원시 기록: ID, UTC 시작, 시간, 라운드, 인원, 보드, 승자 좌석, 옵션, 시간 초과, 기권, 스킵, 미니게임, 아이템 획득/사용"],
        ["Saved/Balance/O_Mock_BalanceSummary.json", "전체 경기 집계: 평균 게임 시간·라운드, 무승부율, 좌석별 승리/승률, 아이템별 획득·사용률, 판 크기별 경기 수"],
    ]
    story.append(kr_table(stats_rows, [54 * mm, CONTENT_W - 54 * mm]))
    story.append(para("저장 안정성과 개인정보", H2))
    for text in [
        "호스트 권한에서 종료된 경기만 저장합니다. 같은 MatchId를 다시 저장해도 중복 행을 만들지 않습니다.",
        "CSV와 JSON은 임시 파일에 완성한 뒤 각각 원자적으로 교체합니다. 다음 저장 시 JSON을 CSV에서 다시 만들므로 중간 실패를 복구합니다.",
        "플레이어 이름, IP, 비밀번호, 비밀번호 증명 등 개인 식별·인증 정보는 기록하지 않습니다.",
    ]:
        story.append(bullet(text))
    story.append(callout("현재 자동화 묶음은 규칙·시간·아이템·네트워크 권한·로비·미니게임·예측·실전 봇·통계를 포함한 62개 테스트이며 최종 검증에서 62/62 통과했습니다.", bg=CREAM, stroke=GOLD))
    story.append(PageBreak())

    story += section_title("12", "조작법·상태별 입력", "같은 클릭도 현재 화면과 선택 상태에 따라 의미가 달라집니다.")
    controls_rows = [
        ["입력", "메인 메뉴/로비", "본판", "미니게임"],
        ["왼쪽 클릭", "버튼·방 선택·준비", "빈칸 착수 / 선택 아이템 대상 지정 / 교체 카드 선택", "정답 칸 1회 제출"],
        ["오른쪽 버튼 드래그", "3D 화면 회전 가능", "카메라 자유 회전", "카메라 자유 회전"],
        ["마우스 휠", "줌", "줌 인/아웃", "줌 인/아웃"],
        ["1~5", "—", "해당 ID 아이템 선택", "본판 입력 잠금"],
        ["Esc", "—", "아이템 대상 선택 취소", "—"],
        ["F", "카메라 초기화", "카메라 초기화", "카메라 초기화"],
        ["T", "READY", "—", "—"],
        ["Enter", "호스트 경기 시작", "—", "—"],
        ["R", "—", "호스트/로컬 재시작 요청", "미니게임 중에는 본판 단계 우선"],
    ]
    story.append(kr_table(controls_rows, [20 * mm, 43 * mm, 68 * mm, CONTENT_W - 131 * mm]))
    story.append(para("아이템 사용 절차", H2))
    story.append(FlowStrip(["카드/숫자키\n선택", "에너지·잠금\n1회 제한 확인", "대상 칸\n또는 자동 대상", "서버 검증\n효과 적용", "돌 착수로\n턴 종료"], CONTENT_W))
    story.append(para("재시작과 기권", H2))
    for text in [
        "재시작은 현재 판·효과·인벤토리·시간·미니게임 상태를 초기화하고 같은 규칙 설정으로 새 경기를 시작합니다.",
        "경기 중 연결 종료는 재접속 대기 상태가 아니라 기권입니다. 잠깐의 Wi‑Fi 단절도 같은 결과가 될 수 있으므로 안정된 LAN을 권장합니다.",
        "호스트 PC가 종료되면 리슨 서버도 종료됩니다. 자동 호스트 이전은 없습니다.",
    ]:
        story.append(bullet(text))
    story.append(PageBreak())

    story += section_title("13", "자주 묻는 판정", "실전에서 혼동하기 쉬운 경계 사례를 짧게 정리합니다.")
    faq_rows = [
        ["질문", "판정"],
        ["아이템을 쓰면 턴이 끝나나?", "아니요. 성공 후에도 돌을 하나 둬야 턴이 끝납니다. 단, 아이템으로 5목이 되면 즉시 종료됩니다."],
        ["이번 턴에 받은 아이템을 바로 쓸 수 있나?", "아니요. 다음 자기 턴 시작에 잠금이 풀립니다."],
        ["인벤토리 교체를 무시하고 돌을 둘 수 있나?", "아니요. 서버가 교체 전 착수와 아이템 RPC를 모두 거부합니다."],
        ["봉인 칸에 견인할 수 있나?", "아니요. 견인 목적지는 유효한 Empty 칸이어야 합니다."],
        ["수호막 돌을 견인하거나 강탈할 수 있나?", "둘 다 불가합니다. 견인 출발 후보와 강탈 후보에서 제외됩니다."],
        ["강탈 대상 옆에 다른 색 돌만 있으면 고립인가?", "예. 같은 색 돌이 8방향 이웃에 없으면 고립입니다."],
        ["스킵 대상을 직접 고를 수 있나?", "아니요. 현재 턴 방향의 다음 활성 플레이어로 자동 결정됩니다."],
        ["6목도 이기나?", "예. 5개 이상 연속이면 승리합니다."],
        ["미니게임 오답 후 다시 고를 수 있나?", "아니요. 플레이어마다 제출은 1회입니다."],
        ["아이템 OFF인데 미니게임 ON이 가능한가?", "가능합니다. 미니게임 에너지 보상은 기록되지만 사용할 아이템은 지급되지 않습니다."],
        ["다른 지역의 PC와 인터넷으로 연결할 수 있나?", "기본 구현은 Null OnlineSubsystem 기반 LAN 검색/직접 IP 프로토타입입니다. 공유기 밖 인터넷 접속, NAT traversal, 계정 매치메이킹은 범위 밖입니다."],
        ["잘못된 비밀번호 뒤 다시 시도할 수 있나?", "예. 실패한 클라이언트 세션을 정리하므로 방을 새로 고르고 올바른 비밀번호로 재시도할 수 있습니다."],
    ]
    story.append(kr_table(faq_rows, [58 * mm, CONTENT_W - 58 * mm]))
    story.append(Spacer(1, 4 * mm))
    story.append(callout("규칙 충돌 시 우선순위는 <b>서버의 현재 상태 → 방 설정 → 아이템 대상 규칙 → 클라이언트 예측 UI</b> 순입니다. 화면의 미리보기는 도움말이고, 서버 판정이 최종 결과입니다.", bg=colors.HexColor("#EAF1FA"), stroke=colors.HexColor("#9AB4CE")))
    story.append(PageBreak())

    story += section_title("14", "구현 범위·검증 체크리스트", "현재 프로토타입에 들어간 기능과 의도적으로 제외된 범위를 마지막으로 확인합니다.")
    included_rows = [
        ["구분", "완료된 범위"],
        ["기본 게임", "2~4인, 15~23 정사각 보드, 5목/무승부, 턴·라운드, 시간·자동 착수, 기권·재시작"],
        ["아이템", "5종, 에너지, 2칸 인벤토리, 획득 잠금, 한 턴 1회, 직접 교체 모달, 서버 검증"],
        ["네트워크", "LAN 방 검색, 직접 IP(무잠금), 잠금 방, 준비 로비, 2클라이언트 실제 접속·복제"],
        ["봇", "방별 0~3명, 사람 좌석 우선, 승리·방어 착수, 확률 아이템 사용, 교체 모달 자동 해소"],
        ["미니게임", "5라운드 주기, 7×7 승리 수 찾기, 8초, 순위 보상, 3초 결과"],
        ["UX", "3D 풍경·하늘·나무·언덕, 자유 회전·확대, 메뉴 자동 회전, 승리/아이템 예측"],
        ["품질", "62개 자동화, Blueprint 전체 컴파일 0 오류/경고, GUI 해상도·잠금 재시도 검증, 밸런스 파일"],
    ]
    story.append(kr_table(included_rows, [30 * mm, CONTENT_W - 30 * mm]))
    story.append(para("의도적으로 포함하지 않은 기능", H2))
    excluded = [
        "인터넷 매치메이킹, 랭킹/등급전, 계정 시스템",
        "경기 중 재접속, 호스트 마이그레이션, 관전자",
        "Steam/EOS NAT traversal과 암호화 인증",
        "상점·재화·20종 이상 아이템, 복잡한 특수 지형",
        "운영 서버 수준의 안티치트와 리플레이 복구",
    ]
    for text in excluded:
        story.append(bullet(text))
    story.append(para("최종 플레이 전 점검", H2))
    checks = [
        "호스트와 참가 PC가 같은 LAN에 있고 Windows 방화벽에서 게임 실행 파일을 허용했는가?",
        "방 목록의 인원·봇·보드·시간·아이템·미니게임·잠금 설정이 의도와 같은가?",
        "모든 참가자가 READY이고 호스트가 START MATCH를 눌렀는가?",
        "아이템 교체 패널이 뜨면 보드보다 먼저 버릴 슬롯을 골랐는가?",
        "5라운드 직전에는 8초 미니게임을 대비해 화면 중앙을 확인하고 있는가?",
        "장시간 밸런스 테스트 뒤 Saved/Balance의 CSV와 JSON을 백업했는가?",
    ]
    for idx, text in enumerate(checks, 1):
        story.append(para(f"<b>□ {idx}.</b> {text}"))
    story.append(Spacer(1, 5 * mm))
    story.append(callout("이 규칙서는 설계 초안이 아니라 2026-08-23 현재 프로젝트 소스·에셋·자동화·2클라이언트 GUI 검증 결과를 기준으로 작성되었습니다.", bg=CREAM, stroke=GOLD))
    story.append(Spacer(1, 9 * mm))
    closing = ParagraphStyle("Closing", parent=H1, alignment=TA_CENTER, textColor=FOREST)
    story.append(para("좋은 수는 판 위에서, 좋은 경기는 규칙의 신뢰에서 시작됩니다.", closing))
    story.append(para("O-MOCK · END OF RULEBOOK", ParagraphStyle("End", parent=CAPTION, fontName=FONT_BOLD, textColor=GOLD)))

    return story


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    TMP_DIR.mkdir(parents=True, exist_ok=True)
    doc = RulesDocTemplate(str(OUT_PATH))
    doc.build(make_story())
    print(OUT_PATH)


if __name__ == "__main__":
    main()
