#!/usr/bin/env python3
"""Fail when production core code uses a construct that docs/spec/03-pruebas.md section 9 forbids.

Standard library only. Comments, string literals and character literals are blanked before
matching, so a forbidden word inside them never fires. Run it as `python3 -I scripts/check_forbidden.py`.

Exit status: 0 clean, 1 violations, 2 usage or I/O problem (including "nothing to scan").
"""

from __future__ import annotations

import argparse
import bisect
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ROOTS = ("core/include", "core/src", "core/host")
EXTENSIONS = {".hpp", ".h", ".hh", ".hxx", ".cpp", ".cc", ".cxx", ".inl", ".ipp", ".tpp"}

# Only rules whose violation is a judgement call can be waived; determinism rules cannot.
WAIVABLE = {"sort", "mutable-static"}

RAW_PREFIXES = {"R", "u8R", "uR", "UR", "LR"}


@dataclass(frozen=True)
class Rule:
    ident: str
    pattern: "re.Pattern[str]"
    message: str
    severity: str = "error"


def _rule(ident: str, regex: str, message: str, severity: str = "error") -> Rule:
    return Rule(ident, re.compile(regex), message, severity)


RULES = (
    _rule("float", r"\bfloat\b", "floating-point type `float` (integers only, 01 section 7)"),
    _rule("double", r"\bdouble\b", "floating-point type `double` (integers only, 01 section 7)"),
    _rule("random", r"<random>|\bstd::shuffle\b|\bs?rand\s*\(",
          "non-deterministic or library RNG (use the project SplitMix64; 03 section 6)"),
    _rule("unordered", r"\bunordered_\w*",
          "unordered container (iteration order is unspecified; iterate by ascending id)"),
    _rule("std-hash", r"\bstd::hash\b", "std::hash (implementation-defined values; 01 section 7)"),
    _rule("chrono",
          r"<chrono>|<ctime>|<time\.h>|<sys/time\.h>|\bstd::chrono\b"
          r"|\b(?:steady|system|high_resolution|utc|file)_clock\b|\bclock_gettime\b"
          r"|\bgettimeofday\b|\bstd::time\s*\(",
          "clock or time source (the core has no clock; 02 section 1)"),
    _rule("throw", r"\bthrow\b", "`throw` (the core is built with -fno-exceptions)"),
    _rule("try", r"\btry\b", "`try` (the core is built with -fno-exceptions)"),
    _rule("catch", r"\bcatch\b", "`catch` (the core is built with -fno-exceptions)"),
    _rule("long", r"\blong\b", "bare `long` (use a fixed-width <cstdint> type)"),
    _rule("memcpy", r"\b(?:memcpy|memmove|bcopy)\b",
          "memcpy-style copy (serialize byte by byte, little-endian; 02 section 6)"),
    _rule("sort", r"\bstd::(?:ranges::)?sort\b|(?<![\w.>:])sort\s*\(",
          "sort: ties make the order unspecified; use unique keys and waive with "
          "`slg-allow(sort): <why keys are unique>`, or std::stable_sort"),
    _rule("mutable-static", r"\bthread_local\b",
          "thread_local state (hidden global; 02 section 1)", "warning"),
)

FLOAT_LITERAL = "float-literal"
WAIVER_RE = re.compile(r"slg-allow\(([^)]*)\)\s*:\s*(\S.*)")
WAIVER_MARK_RE = re.compile(r"slg-allow\(")
STATIC_RE = re.compile(r"\bstatic\b")
CONST_RE = re.compile(r"\b(?:const|constexpr|consteval|constinit|operator)\b")


@dataclass(frozen=True)
class Violation:
    path: str
    line: int
    rule: str
    message: str
    severity: str
    source: str

    def render(self) -> str:
        head = f"{self.path}:{self.line}: {self.severity}: [{self.rule}] {self.message}"
        return f"{head}\n    {self.source.strip()}"


def _is_digit(ch: str) -> bool:
    return "0" <= ch <= "9"


def _is_ident_start(ch: str) -> bool:
    return ch.isalpha() or ch == "_"


def _is_ident_char(ch: str) -> bool:
    return ch.isalnum() or ch == "_"


def _number_is_float(token: str) -> bool:
    low = token.replace("'", "").lower()
    if low.startswith("0x"):
        return "." in low or "p" in low
    if low.startswith("0b"):
        return False
    return "." in low or "e" in low


def blank_source(text: str) -> tuple[str, list[tuple[int, str]]]:
    """Return (text with comments/strings/chars/numbers blanked, float literals as (offset, token)).

    Newlines are preserved so offsets keep mapping to the original line numbers.
    """
    out: list[str] = []
    floats: list[tuple[int, str]] = []
    n = len(text)
    i = 0

    def blank(segment: str) -> str:
        return "".join(c if c == "\n" else " " for c in segment)

    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if ch == "/" and nxt == "/":
            j = i
            while j < n:
                if text[j] == "\n":
                    k = j - 1
                    if k > i and text[k] == "\r":
                        k -= 1
                    if k >= i and text[k] == "\\":
                        j += 1
                        continue
                    break
                j += 1
            out.append(blank(text[i:j]))
            i = j
        elif ch == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(blank(text[i:j]))
            i = j
        elif ch == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(blank(text[i:j]))
            i = j
        elif ch == "'":
            j = i + 1
            while j < n and text[j] != "'" and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(blank(text[i:j]))
            i = j
        elif _is_ident_start(ch):
            j = i + 1
            while j < n and _is_ident_char(text[j]):
                j += 1
            word = text[i:j]
            if word in RAW_PREFIXES and j < n and text[j] == '"':
                open_paren = text.find("(", j)
                delimiter = text[j + 1:open_paren] if open_paren >= 0 else ""
                close = text.find(")" + delimiter + '"', open_paren) if open_paren >= 0 else -1
                end = n if close < 0 else close + len(delimiter) + 2
                out.append(blank(text[i:end]))
                i = end
            else:
                out.append(word)
                i = j
        elif _is_digit(ch) or (ch == "." and _is_digit(nxt)):
            j = i + 1
            while j < n:
                c = text[j]
                if c in "+-" and text[j - 1] in "eEpP":
                    j += 1
                elif _is_ident_char(c) or c == ".":
                    j += 1
                elif c == "'" and j + 1 < n and _is_ident_char(text[j + 1]):
                    j += 1
                else:
                    break
            token = text[i:j]
            if _number_is_float(token):
                floats.append((i, token))
            out.append(" " * (j - i))
            i = j
        else:
            out.append(ch)
            i += 1
    return "".join(out), floats


def _waivers(raw_lines: list[str], line: int) -> tuple[set[str], list[str]]:
    """Waived rule ids that apply to `line` (same line or the one above) and malformed waivers."""
    granted: set[str] = set()
    malformed: list[str] = []
    for idx in (line - 1, line - 2):
        if idx < 0 or idx >= len(raw_lines):
            continue
        text = raw_lines[idx]
        if not WAIVER_MARK_RE.search(text):
            continue
        match = WAIVER_RE.search(text)
        if not match:
            malformed.append("slg-allow without a non-empty reason")
            continue
        for name in (part.strip() for part in match.group(1).split(",")):
            if name in WAIVABLE:
                granted.add(name)
            else:
                malformed.append(f"rule `{name}` cannot be waived")
    return granted, malformed


def _static_warnings(clean: str) -> list[tuple[int, str]]:
    """Heuristic: a `static` variable that is not const is hidden global state (warning only)."""
    hits: list[tuple[int, str]] = []
    for match in STATIC_RE.finditer(clean):
        stop = len(clean)
        for k in range(match.end(), len(clean)):
            if clean[k] in "(={;":
                stop = k
                break
        if stop < len(clean) and clean[stop] == "(":
            continue
        statement = clean[match.start():stop + 1]
        if CONST_RE.search(statement):
            continue
        hits.append((match.start(), "static variable that is not const (hidden global state?)"))
    return hits


def scan_text(text: str, path: str = "<memory>") -> list[Violation]:
    clean, floats = blank_source(text)
    raw_lines = text.splitlines()
    starts = [0] + [m.end() for m in re.finditer("\n", text)]

    def line_of(offset: int) -> int:
        return bisect.bisect_right(starts, offset)

    def source_of(line: int) -> str:
        return raw_lines[line - 1] if 0 < line <= len(raw_lines) else ""

    found: list[tuple[int, str, str, str]] = []
    for rule in RULES:
        for match in rule.pattern.finditer(clean):
            found.append((match.start(), rule.ident, rule.message, rule.severity))
    for offset, token in floats:
        found.append((offset, FLOAT_LITERAL, f"floating-point literal `{token}` (integers only)", "error"))
    for offset, message in _static_warnings(clean):
        found.append((offset, "mutable-static", message, "warning"))

    violations: list[Violation] = []
    reported_waiver_problems: set[tuple[int, str]] = set()
    for offset, ident, message, severity in sorted(found):
        line = line_of(offset)
        granted, malformed = _waivers(raw_lines, line)
        for problem in malformed:
            if (line, problem) not in reported_waiver_problems:
                reported_waiver_problems.add((line, problem))
                violations.append(Violation(path, line, "bad-waiver", problem, "error", source_of(line)))
        if ident in granted:
            continue
        violations.append(Violation(path, line, ident, message, severity, source_of(line)))
    return violations


def iter_sources(paths: list[Path]) -> list[Path]:
    files: set[Path] = set()
    for base in paths:
        if base.is_file():
            files.add(base)
        elif base.is_dir():
            files.update(p for p in base.rglob("*") if p.is_file() and p.suffix in EXTENSIONS)
    return sorted(files)


def _display(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", help="files or directories (default: core/include core/src, core/host if present)")
    parser.add_argument("--allow-empty", action="store_true", help="do not fail when no source file is found")
    parser.add_argument("--strict-warnings", action="store_true", help="treat warnings as errors")
    parser.add_argument("--list-rules", action="store_true")
    args = parser.parse_args(argv)

    if args.list_rules:
        for rule in RULES:
            print(f"{rule.ident:15} {rule.severity:8} {rule.message}")
        print(f"{FLOAT_LITERAL:15} {'error':8} floating-point literal (integers only)")
        return 0

    if args.paths:
        targets = [Path(p) for p in args.paths]
    else:
        targets = [ROOT / p for p in DEFAULT_ROOTS if (ROOT / p).exists()]
    missing = [t for t in targets if not t.exists()]
    if missing:
        for t in missing:
            print(f"check_forbidden: no such path: {t}", file=sys.stderr)
        return 2

    files = iter_sources(targets)
    if not files and not args.allow_empty:
        print("check_forbidden: no C++ sources found to scan; refusing to pass vacuously", file=sys.stderr)
        return 2

    errors = warnings = 0
    for file in files:
        text = file.read_text(encoding="utf-8", errors="replace")
        for violation in scan_text(text, _display(file)):
            print(violation.render())
            if violation.severity == "error" or args.strict_warnings:
                errors += 1
            else:
                warnings += 1
    print(f"check_forbidden: {len(files)} files scanned, {errors} errors, {warnings} warnings")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
