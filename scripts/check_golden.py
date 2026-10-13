#!/usr/bin/env python3
"""Compare golden traces with what the Python oracle and the C++ runner produce, byte for byte.

For every `<name>.ltrace` under the golden directory the inputs are the siblings `<name>.lnet` and
`<name>.lscn`. Each engine is a command template where `{lnet}`, `{lscn}` and (optional) `{out}` are
replaced by paths; without `{out}` the trace is read from stdout. Standard library only.

Exit status: 0 every comparison matches, 1 a mismatch or error, 2 usage problem or nothing to compare.
"""

from __future__ import annotations

import argparse
import difflib
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIFF_LINES = 12


def run_engine(template: str, lnet: Path, lscn: Path, timeout: int) -> tuple[bytes | None, str]:
    """Return (trace bytes, "") or (None, reason)."""
    with tempfile.TemporaryDirectory(prefix="slg-golden-") as tmp:
        out = Path(tmp) / "out.ltrace"
        words = [w.replace("{lnet}", str(lnet)).replace("{lscn}", str(lscn)).replace("{out}", str(out))
                 for w in shlex.split(template)]
        try:
            done = subprocess.run(words, cwd=ROOT, capture_output=True, timeout=timeout, check=False)
        except FileNotFoundError as exc:
            return None, f"cannot start {words[0]!r}: {exc.strerror}"
        except subprocess.TimeoutExpired:
            return None, f"timed out after {timeout} s"
        if done.returncode != 0:
            tail = done.stderr.decode("utf-8", "replace").strip().splitlines()[-3:]
            return None, f"exit status {done.returncode}: {' | '.join(tail)}"
        if "{out}" in template:
            if not out.exists():
                return None, "command did not write {out}"
            return out.read_bytes(), ""
        return done.stdout, ""


def describe_difference(expected: bytes, actual: bytes) -> str:
    want = expected.decode("utf-8", "replace").splitlines(keepends=True)
    got = actual.decode("utf-8", "replace").splitlines(keepends=True)
    diff = list(difflib.unified_diff(want, got, "golden", "actual", n=1))
    if not diff:
        return f"bytes differ but lines match (lengths {len(expected)} vs {len(actual)}; line endings or trailing bytes)"
    shown = [line.rstrip("\n") for line in diff[:DIFF_LINES]]
    if len(diff) > DIFF_LINES:
        shown.append(f"... {len(diff) - DIFF_LINES} more diff lines")
    return "\n".join("      " + line for line in shown)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--golden-dir", default="core/golden")
    parser.add_argument("--oracle-cmd", required=True, help="command template for the Python oracle")
    parser.add_argument("--runner-cmd", help="command template for a standalone C++ runner (optional)")
    parser.add_argument("--timeout", type=int, default=120, help="seconds per engine run")
    args = parser.parse_args(argv)

    golden_dir = Path(args.golden_dir)
    if not golden_dir.is_absolute():
        golden_dir = ROOT / golden_dir
    traces = sorted(golden_dir.rglob("*.ltrace")) if golden_dir.is_dir() else []
    if not traces:
        print(f"check_golden: no *.ltrace under {golden_dir}; refusing to pass vacuously", file=sys.stderr)
        return 2

    engines = [("oracle", args.oracle_cmd)]
    if args.runner_cmd:
        engines.append(("c++", args.runner_cmd))

    failures = 0
    comparisons = 0
    for trace in traces:
        name = trace.relative_to(golden_dir).with_suffix("").as_posix()
        lnet, lscn = trace.with_suffix(".lnet"), trace.with_suffix(".lscn")
        missing = [p.name for p in (lnet, lscn) if not p.exists()]
        if missing:
            print(f"FAIL  {name}: missing input {', '.join(missing)}")
            failures += 1
            continue
        expected = trace.read_bytes()
        for label, template in engines:
            comparisons += 1
            actual, problem = run_engine(template, lnet, lscn, args.timeout)
            if actual is None:
                print(f"FAIL  {name} [{label}]: {problem}")
                failures += 1
            elif actual != expected:
                print(f"FAIL  {name} [{label}]: output differs from the golden trace")
                print(describe_difference(expected, actual))
                failures += 1
            else:
                print(f"ok    {name} [{label}] ({len(expected)} bytes)")

    print(f"check_golden: {len(traces)} golden traces, {comparisons} comparisons, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
