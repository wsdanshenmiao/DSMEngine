from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main() -> None:
    source = Path(sys.argv[1])
    destination = Path(sys.argv[2])
    destination.mkdir(parents=True, exist_ok=True)
    pages = sorted(source.glob("page-*.png"))
    font = ImageFont.truetype(r"C:\Windows\Fonts\msyh.ttc", 18)
    columns, rows = 4, 4
    cell_w, cell_h = 260, 380
    for batch_index in range(0, len(pages), columns * rows):
        batch = pages[batch_index:batch_index + columns * rows]
        sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), "#DDE6EA")
        draw = ImageDraw.Draw(sheet)
        for index, page_path in enumerate(batch):
            with Image.open(page_path).convert("RGB") as page:
                page.thumbnail((cell_w - 18, cell_h - 38), Image.Resampling.LANCZOS)
                x = (index % columns) * cell_w + (cell_w - page.width) // 2
                y = (index // columns) * cell_h + 28
                sheet.paste(page, (x, y))
            label = f"PDF {batch_index + index + 1}"
            draw.text(((index % columns) * cell_w + 8, (index // columns) * cell_h + 4), label, fill="#173B57", font=font)
        sheet.save(destination / f"contact-{batch_index // (columns * rows) + 1:02d}.jpg", quality=88, optimize=True)


if __name__ == "__main__":
    main()
