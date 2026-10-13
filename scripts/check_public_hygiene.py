#!/usr/bin/env python3
"""Fail if a tracked file contains local paths, private-key blocks or well-known token prefixes.

Generic by design: it names nothing private. Patterns are assembled from pieces so this file does not match itself.
"""
import re
import subprocess
import sys

PATTERNS = {
    "local home path": re.compile("/ho" + r"me/[A-Za-z0-9_.-]+/"),
    "local file URL": re.compile("file" + r":///"),
    "windows user path": re.compile(r"[A-Za-z]:\\\\Users\\\\"),
    "server directory": re.compile("/s" + r"rv/[A-Za-z0-9_.-]+"),
    "private key block": re.compile("-----BEGIN [A-Z ]*PRIV" + "ATE KEY"),
    "github token": re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})\b"),
    "aws key": re.compile(r"\bAKIA[0-9A-Z]{16}\b"),
    "slack token": re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{10,}"),
}


def main() -> int:
    files = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True).stdout.split("\n")
    bad = 0
    for name in filter(None, files):
        try:
            text = open(name, encoding="utf-8").read()
        except (UnicodeDecodeError, FileNotFoundError):
            continue
        for label, pat in PATTERNS.items():
            for m in pat.finditer(text):
                line = text.count("\n", 0, m.start()) + 1
                print(f"{name}:{line}: {label}")
                bad += 1
    print("ok: no private paths or tokens in tracked files" if not bad else f"{bad} problem(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
