#!/usr/bin/env python3
"""Offline landing-page checks; see docs/contributing/README_POLICY.md.

Usage: python3 scripts/readme_lint.py [README.md README.kr.md]
No measured figures are allowed in landing pages, even with source comments:
evidence belongs in BENCHMARKS.md. Numeric configuration/code is not a claim.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from html import unescape
from html.parser import HTMLParser
from pathlib import Path
import re
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]
LIMIT = 300
FIRST_SCREEN = 20
REPO_URL = "https://github.com/kcenon/pacs_system"
QUALIFIER = re.compile(
    r"\b(?:production[- ](?:ready|grade)|enterprise[- ]grade|battle[- ]tested|"
    r"blazing(?:ly)?|world[- ]class|comprehensive|robust|seamless(?:ly)?|"
    r"guarantee(?:d|s)?|compliant|HIPAA|RAII\s+Grade\s+A|"
    r"zero[- ](?:copy|overhead|leaks?|data races?|buffer overflows?|manual memory management)|"
    r"same performance as direct API|(?:ThreadSanitizer|AddressSanitizer|TSAN|ASAN|UBSAN)\s+clean)\b"
    r"|(?:프로덕션|엔터프라이즈)[ -]?(?:준비|급)|포괄적|견고한|원활한|세계[ -]?최고|"
    r"완벽한|완전한\s*(?:준수|호환)|성능\s*동일|동일한\s*성능|무결점|"
    r"(?:누수|데이터\s*경쟁|수동\s*메모리\s*관리)\s*(?:없음|제로)",
    re.I,
)
NUMBER = r"\d[\d,.]*(?:\s*[kmg])?"
FIGURE = re.compile(
    rf"(?<![\w/]){NUMBER}\s*(?:%|percent\b|퍼센트|[x×]\s*(?:faster|speedup)|배\s*(?:빠|성능)|"
    r"(?:ops|msg|messages|req|requests|[kmgt]?i?b)\s*/\s*(?:s(?:ec(?:ond)?s?)?)\b|"
    r"(?:ns|us|µs|μs|ms|[mg]hz)\b|(?:나노초|마이크로초|밀리초))"
    rf"|초당\s*{NUMBER}\s*(?:건|개|메시지|요청)",
    re.I,
)
DEFINITION = re.compile(r'^ {0,3}\[([^\]]+)\]:\s*(?:<([^>]+)>|(\S+))')
IMAGE = re.compile(
    r'!\[([^\]]*)\](?:\(\s*(<[^>]+>|[^\s)]+)(?:\s+["\'][^\n]*?["\'])?\s*\)|\[([^\]]*)\])?'
)
RELEASE = re.compile(
    re.escape(REPO_URL) + r"/releases/tag/v(\d+\.\d+\.\d+(?:[-+][\w.-]+)?)\b"
)


@dataclass
class Finding:
    line: int
    message: str


@dataclass
class Report:
    lines: int
    claim_lines: int
    findings: list[Finding]

    @property
    def density(self):
        return 100 * self.claim_lines / max(self.lines, 1)


def prose_lines(source: str):
    """Preserve source line numbers; ignore comments and fenced/indented code."""
    source = re.sub(r"<!--.*?(?:-->|\Z)", lambda m: "\n" * m[0].count("\n"), source, flags=re.S)
    fence = None
    for number, line in enumerate(source.splitlines(), 1):
        # Allow fences inside block quotes as well as ordinary Markdown fences.
        candidate = re.sub(r"^(?: {0,3}>[ \t])+", "", line)
        marker = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", candidate)
        if fence:
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence) and not marker[2].strip():
                fence = None
            yield number, ""
        elif marker:
            fence = marker[1]
            yield number, ""
        elif candidate.startswith(("    ", "\t")):
            yield number, ""
        else:
            yield number, candidate


def ref_key(value):
    return " ".join(value.split()).casefold()


class Images(HTMLParser):
    def __init__(self):
        super().__init__()
        self.sources = []
        self.text = []

    def handle_starttag(self, tag, attrs):
        if tag.lower() == "img":
            self.sources.append((self.getpos()[0], dict(attrs).get("src", "")))

    def handle_data(self, data):
        self.text.append(data)


def workflow_badge(url: str, root: Path):
    parsed = urlsplit(url)
    match = re.fullmatch(r"/kcenon/pacs_system/actions/workflows/([\w.-]+\.ya?ml)/badge\.svg", parsed.path)
    return bool(
        parsed.scheme == "https" and parsed.netloc == "github.com" and match
        and (root / ".github/workflows" / match[1]).is_file()
    )


def lint(source: str, root: Path = ROOT):
    lines = source.splitlines()
    visible = list(prose_lines(source))
    definitions = {}
    for _, line in visible:
        match = DEFINITION.match(line)
        if match:
            definitions[ref_key(match[1])] = match[2] or match[3]
    findings = []
    if len(lines) > LIMIT:
        findings.append(Finding(LIMIT + 1, f"line limit exceeded ({len(lines)} > {LIMIT}); move detail to docs/"))
    opening = "\n".join(line for number, line in visible if number <= FIRST_SCREEN)
    opening_text = re.sub(r"[*_`]", "", opening)
    if not re.search(r"\bStatus:\s*(?:active|maintenance|experimental)\b", opening_text):
        findings.append(Finding(1, "show Status: active|maintenance|experimental in the first 20 lines"))
    # Resolve reference-style release links as well as inline links.
    expanded = re.sub(
        r"\[([^\]]+)\]\[([^\]]*)\]",
        lambda m: f"[{m[1]}]({definitions.get(ref_key(m[2] or m[1]), '')})",
        opening,
    )
    release_links = re.findall(r"\[(v?\d+\.\d+\.\d+(?:[-+][\w.-]+)?)\]\(([^)]+)\)", expanded)
    if not any((match := RELEASE.fullmatch(url)) and label.lstrip("v") == match[1]
               for label, url in release_links):
        findings.append(Finding(1, "link the published release version in the first 20 lines"))
    # Parse the complete visible stream so multiline HTML images cannot evade checks.
    html = Images()
    html.feed("\n".join(line for _, line in visible))
    for number, url in html.sources:
        if not workflow_badge(url, root):
            findings.append(Finding(number, f"image must use a badge for an existing repository workflow: {url!r}"))
    claim_lines = set()
    for number, line in visible:
        if not line or DEFINITION.match(line):
            continue
        for match in IMAGE.finditer(line):
            url = match[2]
            if url:
                url = url.strip("<>")
            else:
                url = definitions.get(ref_key(match[3] or match[1]), "")
            if not workflow_badge(url, root):
                findings.append(Finding(number, f"image must use a badge for an existing repository workflow: {url!r}"))
        # Scan rendered prose, including inline-code claims, but not link destinations.
        text = IMAGE.sub("", line)
        text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
        text = re.sub(r"\[([^\]]+)\]\[[^\]]*\]", r"\1", text)
        text = re.sub(r"https?://[^\s<>]+", "", text)
        html = Images()
        html.feed(text)
        text = unescape("".join(html.text))
        text = re.sub(r"[*_`]", "", text)
        if QUALIFIER.search(text):
            findings.append(Finding(number, "unsupported qualifier; describe verifiable behavior instead"))
            claim_lines.add(number)
        if FIGURE.search(text):
            findings.append(Finding(number, "measured figure; move the claim and its evidence to docs/BENCHMARKS.md"))
            claim_lines.add(number)
    report = Report(len(lines), len(claim_lines), findings)
    if report.density > 1.0:
        findings.append(Finding(1, f"qualifier/figure density {report.density:.2f} exceeds 1.0 per 100 lines"))
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", default=["README.md", "README.kr.md"])
    args = parser.parse_args(argv)
    failed = False
    for name in args.files:
        path = Path(name)
        try:
            report = lint(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError) as error:
            print(f"{path}:1: cannot read README: {error}")
            failed = True
            continue
        for finding in report.findings:
            print(f"{path}:{finding.line}: {finding.message}")
        print(f"{path}: {report.lines} lines; {report.claim_lines} qualifier/figure lines; "
              f"density {report.density:.2f}/100; {'FAIL' if report.findings else 'PASS'}")
        failed |= bool(report.findings)
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
