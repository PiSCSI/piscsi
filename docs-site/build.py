#!/usr/bin/env python3
"""Build the PiSCSI man-page documentation site."""

from __future__ import annotations

import argparse
import html
import json
import re
import shutil
import subprocess
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parent
SOURCE_DIR = REPO_ROOT / "doc"
DIST_DIR = ROOT / "dist"
TEMPLATE = ROOT / "templates" / "page.html.tmpl"
ASSETS_DIR = ROOT / "assets"


class TextExtractor(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.parts: list[str] = []

    def handle_data(self, data: str) -> None:
        self.parts.append(data)

    def text(self) -> str:
        return re.sub(r"\\s+", " ", " ".join(self.parts)).strip()


def description_from_source(source: Path) -> str:
    for line in source.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^\\.Nd\\s+(.*)$", line)
        if match:
            return re.sub(r"\\s+", " ", match.group(1)).strip()
    return "PiSCSI command reference"


def render_fragment(source: Path) -> str:
    try:
        result = subprocess.run(
            ["mandoc", "-T", "html", "-O", "fragment", str(source)],
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as error:
        raise SystemExit("mandoc is required to build the documentation site") from error
    except subprocess.CalledProcessError as error:
        raise SystemExit(f"mandoc failed for {source.name}: {error.stderr}") from error
    return result.stdout.strip()


def navigation(pages: list[dict[str, str]], current: str = "") -> str:
    links = []
    for page in pages:
        active = ' aria-current="page"' if page["url"] == current else ""
        links.append(
            f'<a class="nav-link{ " is-active" if active else ""}" href="{page["url"]}"{active}>'
            f'<span>{page["title"]}</span><small>{page["section"]}</small></a>'
        )
    return "\\n".join(links)


def apply_template(template: str, *, title: str, description: str, nav: str, content: str, current: str = "") -> str:
    values = {
        "{{TITLE}}": html.escape(title),
        "{{DESCRIPTION}}": html.escape(description),
        "{{NAV}}": nav,
        "{{CONTENT}}": content,
        "{{CURRENT}}": current,
    }
    for marker, value in values.items():
        template = template.replace(marker, value)
    return template


def build() -> None:
    sources = sorted(SOURCE_DIR.glob("*.1")) + sorted(SOURCE_DIR.glob("*.8"))
    if not sources:
        raise SystemExit("No man pages found in doc/")

    pages = []
    rendered: dict[str, str] = {}
    for source in sources:
        section = source.suffix[1:]
        title = source.stem
        url = f"{source.stem}.html"
        description = description_from_source(source)
        fragment = render_fragment(source)
        rendered[url] = fragment
        pages.append({"title": title, "section": section, "url": url, "description": description})

    template = TEMPLATE.read_text(encoding="utf-8")
    if DIST_DIR.exists():
        shutil.rmtree(DIST_DIR)
    DIST_DIR.mkdir(parents=True)
    shutil.copytree(ASSETS_DIR, DIST_DIR / "assets")

    for page in pages:
        content = (
            f'<div class="doc-heading"><span class="eyebrow">man {page["section"]}</span>'
            f'<h1>{html.escape(page["title"])}</h1><p>{html.escape(page["description"])}</p></div>'
            f'<article class="man-page">{rendered[page["url"]]}</article>'
        )
        output = apply_template(
            template,
            title=f'{page["title"]} | PiSCSI Documentation',
            description=page["description"],
            nav=navigation(pages, page["url"]),
            content=content,
            current=page["url"],
        )
        (DIST_DIR / page["url"]).write_text(output, encoding="utf-8")

    cards = "\\n".join(
        f'<a class="page-card" href="{page["url"]}"><span class="card-kicker">man {page["section"]}</span>'
        f'<strong>{html.escape(page["title"])}</strong><span>{html.escape(page["description"])}</span></a>'
        for page in pages
    )
    index_content = (
        '<div class="hero"><span class="eyebrow">PiSCSI reference</span>'
        '<h1>Command documentation,<br><em>kept close to the metal.</em></h1>'
        '<p class="hero-copy">The official man pages for PiSCSI and its companion utilities, generated directly from the project sources.</p>'
        '<div class="hero-meta"><span>Source: <code>doc/</code></span><span>Updated automatically</span></div></div>'
        f'<section class="page-list"><div class="section-label"><span>Reference library</span><span>{len(pages):02d} pages</span></div><div class="page-grid">{cards}</div></section>'
    )
    (DIST_DIR / "index.html").write_text(
        apply_template(
            template,
            title="PiSCSI Documentation",
            description="PiSCSI command and utility reference documentation.",
            nav=navigation(pages),
            content=index_content,
        ),
        encoding="utf-8",
    )

    search_index = []
    for page in pages:
        extractor = TextExtractor()
        extractor.feed(rendered[page["url"]])
        search_index.append({**page, "text": extractor.text()})
    (DIST_DIR / "search-index.json").write_text(json.dumps(search_index, indent=2) + "\n", encoding="utf-8")
    print(f"Built {len(pages)} man pages in {DIST_DIR}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    build()
