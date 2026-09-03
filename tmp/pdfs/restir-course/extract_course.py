from __future__ import annotations

import json
import sys
from pathlib import Path

from pypdf import PdfReader


def main() -> None:
    source = Path(sys.argv[1])
    destination = Path(sys.argv[2])
    reader = PdfReader(source)
    pages = []
    for index, page in enumerate(reader.pages, start=1):
        pages.append({"page": index, "text": page.extract_text() or ""})
    destination.write_text(
        json.dumps({"source": str(source), "pages": pages}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
