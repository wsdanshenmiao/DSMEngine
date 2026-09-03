from __future__ import annotations

import json
import re
import sys
from pathlib import Path

from PIL import Image
from pypdf import PdfReader


def main() -> None:
    pdf_path = Path(sys.argv[1])
    markdown_path = Path(sys.argv[2])
    render_dir = Path(sys.argv[3])

    reader = PdfReader(pdf_path)
    extracted = "\n".join(page.extract_text() or "" for page in reader.pages)
    markdown = markdown_path.read_text(encoding="utf-8")
    source_markers = [int(value) for value in re.findall(r"(?m)^@@PAGE\s+(\d+)\s*$", markdown)]
    missing_badges = [number for number in range(1, 62) if f"原稿第 {number} 页" not in extracted]
    required_terms = [
        "无偏贡献权重",
        "加权 Reservoir 采样",
        "广义 Balance Heuristic",
        "Hybrid Shift",
        "Pairwise MIS",
        "Sample Tiling",
        "游戏集成经验",
        "缩略语表",
        "符号表",
    ]
    missing_terms = [term for term in required_terms if term not in extracted]

    rendered = sorted(render_dir.glob("page-*.png"))
    sparse_pages = []
    for image_path in rendered:
        with Image.open(image_path).convert("L") as image:
            histogram = image.histogram()
            nonwhite = sum(histogram[:245])
            if nonwhite < 1500:
                sparse_pages.append(image_path.name)

    checks = {
        "pdf_exists": pdf_path.is_file(),
        "pdf_size_bytes": pdf_path.stat().st_size,
        "pdf_pages": len(reader.pages),
        "source_markers_are_1_to_61": source_markers == list(range(1, 62)),
        "missing_source_page_badges": missing_badges,
        "missing_required_terms": missing_terms,
        "rendered_page_count": len(rendered),
        "sparse_rendered_pages": sparse_pages,
        "contains_internal_marker": "@@PAGE" in extracted,
    }
    checks["passed"] = (
        checks["pdf_exists"]
        and checks["pdf_size_bytes"] > 1_000_000
        and checks["pdf_pages"] >= 62
        and checks["source_markers_are_1_to_61"]
        and not missing_badges
        and not missing_terms
        and len(rendered) == len(reader.pages)
        and not sparse_pages
        and not checks["contains_internal_marker"]
    )
    print(json.dumps(checks, ensure_ascii=False, indent=2))
    raise SystemExit(0 if checks["passed"] else 1)


if __name__ == "__main__":
    main()
