#!/usr/bin/env python3
"""Render docs/research/*.md from docs/research/data/*.json.

The JSON files are the original structured output of the research run of
2026-10-13 (researchers + skeptic verdicts + critic). Nothing is edited by hand
in the Markdown: run `python3 tools/render_research.py` to regenerate it and
`python3 tools/render_research.py --check` to fail if it is out of date.
"""
from __future__ import annotations

import json
import pathlib
import sys
from collections import Counter

ROOT = pathlib.Path(__file__).resolve().parent.parent
DATA = ROOT / "docs" / "research" / "data"
OUT = ROOT / "docs" / "research"

TITLES = {
    "versions": "Versions and toolchain",
    "prior-art": "Prior art and name collisions",
    "buildables-cpp-vs-editor": "Buildables: C++ versus editor",
    "wires-and-connections": "Wires and connections",
    "runtime-api": "Sensors, actuators and runtime",
    "redstone-and-logic-theory": "Redstone semantics and logic theory",
    "publishing-and-policy": "Publishing and policy",
}
ORDER = list(TITLES)


def esc_cell(text: str) -> str:
    return " ".join(str(text).split()).replace("|", "\\|")


def quote_block(text: str) -> str:
    """Verbatim quote in a fenced block: Markdown never interprets brackets, asterisks or HTML inside it."""
    body = str(text).strip("\n")
    fence = "```"
    while fence in body:
        fence += "`"
    return f"{fence}text\n{body}\n{fence}"


def verdict_index(verdict_sets: list[dict]) -> dict[str, list[tuple[str, dict]]]:
    idx: dict[str, list[tuple[str, dict]]] = {}
    for vs in verdict_sets:
        lens = (vs.get("lens") or "skeptic").strip()
        for v in vs.get("verdicts", []):
            idx.setdefault(v["id"], []).append((lens, v))
    return idx


def render_topic(key: str, blob: dict) -> tuple[str, Counter]:
    r = blob["research"]
    idx = verdict_index(blob.get("verdictSets", []))
    tally: Counter = Counter()
    o: list[str] = []
    o.append(f"# {TITLES.get(key, key)}\n")
    o.append(
        f"_Generated from `data/{key}.json` by `tools/render_research.py`; do not edit by hand. "
        "Research run of 2026-10-13. Claims are the researcher's; verdicts are from independent skeptics "
        "told to refute them. Wording is kept verbatim (English)._\n"
    )
    o.append("## Summary\n")
    o.append(r["summary"].strip() + "\n")

    o.append("## Claims at a glance\n")
    o.append("| Id | Confidence | Decision-driving | Skeptic verdicts |")
    o.append("| --- | --- | --- | --- |")
    for c in r["claims"]:
        vs = idx.get(c["id"], [])
        for _, v in vs:
            tally[v["verdict"]] += 1
        verdicts = ", ".join(v["verdict"] for _, v in vs) or "not verified (not decision-driving)"
        o.append(
            f"| {esc_cell(c['id'])} | {esc_cell(c['confidence'])} | "
            f"{'yes' if c['decisionDriving'] else 'no'} | {esc_cell(verdicts)} |"
        )
    o.append("")

    o.append("## Claims in detail\n")
    for c in r["claims"]:
        o.append(f"### {c['id']} ({c['confidence']}{', decision-driving' if c['decisionDriving'] else ''})\n")
        o.append(c["statement"].strip() + "\n")
        if c.get("evidence"):
            o.append("Evidence:\n")
            for e in c["evidence"]:
                o.append(f"- <{e['url']}>")
                o.append("")
                o.append("\n".join("  " + ln for ln in quote_block(e["quote"]).splitlines()))
            o.append("")
        for lens, v in idx.get(c["id"], []):
            o.append(f"**Skeptic ({lens}): {v['verdict']}**\n")
            if v.get("corrected"):
                o.append("Corrected statement: " + v["corrected"].strip() + "\n")
            o.append("Note: " + v["note"].strip() + "\n")
            if v.get("evidenceUrl"):
                o.append(f"Evidence: <{v['evidenceUrl']}>\n")

    if r.get("catalog"):
        o.append("## Catalog\n")
        o.append("| Name | Reference | Licence | Last updated | Relevance |")
        o.append("| --- | --- | --- | --- | --- |")
        for m in r["catalog"]:
            link = f"[{esc_cell(m['name'])}]({m['url']})"
            o.append(
                f"| {link} | {esc_cell(m.get('reference', ''))} | {esc_cell(m.get('license', ''))} | "
                f"{esc_cell(m.get('lastUpdated', ''))} | {esc_cell(m.get('relevance', m.get('description', '')))} |"
            )
        o.append("")
    if r.get("nameChecks"):
        o.append("## Name checks (SMR references, 2026-10-13)\n")
        o.append("| Candidate | Exists in SMR | Similar existing | Note |")
        o.append("| --- | --- | --- | --- |")
        for n in r["nameChecks"]:
            o.append(
                f"| {esc_cell(n['candidate'])} | {'yes' if n['existsInSMR'] else 'no'} | "
                f"{esc_cell(n.get('similarExisting', ''))} | {esc_cell(n.get('note', ''))} |"
            )
        o.append("")

    o.append("## Open questions\n")
    for q in r["openQuestions"]:
        o.append("- " + " ".join(q.split()))
    o.append("")
    o.append("## Recommendations\n")
    for q in r["recommendations"]:
        o.append("- " + " ".join(q.split()))
    o.append("")
    return "\n".join(o), tally


def render_critic(c: dict) -> str:
    o = ["# Completeness critic\n"]
    o.append(
        "_Generated from `data/critic.json` by `tools/render_research.py`; do not edit by hand. "
        "The critic read all research results and skeptic verdicts of 2026-10-13._\n"
    )
    o.append("## Biggest risk\n")
    o.append(" ".join(c["biggestRisk"].split()) + "\n")
    for title, key in (
        ("Contradictions between topics", "contradictions"),
        ("Decision-driving claims still unverified", "unverifiedDecisionClaims"),
        ("Questions for the owner", "ownerQuestions"),
        ("Gaps nobody researched", "gaps"),
    ):
        o.append(f"## {title}\n")
        for x in c[key]:
            o.append("- " + " ".join(x.split()))
        o.append("")
    return "\n".join(o)


def render_index(tallies: dict[str, Counter]) -> str:
    o = ["# Research of 13 October 2026\n"]
    o.append(
        "Primary-source research for the logic mod, with adversarial verification. Every page is generated from "
        "the original structured output in `data/` by `tools/render_research.py`.\n"
    )
    o.append("| Topic | confirmed | partially-correct | refuted | unverifiable |")
    o.append("| --- | ---: | ---: | ---: | ---: |")
    total: Counter = Counter()
    for key in ORDER:
        t = tallies[key]
        total.update(t)
        o.append(
            f"| [{TITLES[key]}]({key}.md) | {t['confirmed']} | {t['partially-correct']} | "
            f"{t['refuted']} | {t['unverifiable']} |"
        )
    o.append(
        f"| **Total** | **{total['confirmed']}** | **{total['partially-correct']}** | "
        f"**{total['refuted']}** | **{total['unverifiable']}** |"
    )
    o.append("")
    o.append("Counts are verdicts: a claim checked by two skeptics counts twice.\n")
    o.append("See also the [completeness critic](critic.md).\n")
    return "\n".join(o)


def build() -> dict[pathlib.Path, str]:
    files: dict[pathlib.Path, str] = {}
    tallies: dict[str, Counter] = {}
    for key in ORDER:
        blob = json.loads((DATA / f"{key}.json").read_text(encoding="utf-8"))
        text, tally = render_topic(key, blob)
        files[OUT / f"{key}.md"] = text
        tallies[key] = tally
    files[OUT / "critic.md"] = render_critic(json.loads((DATA / "critic.json").read_text(encoding="utf-8")))
    files[OUT / "README.md"] = render_index(tallies)
    return files


def main(argv: list[str]) -> int:
    files = build()
    if "--check" in argv:
        stale = [p.name for p, t in files.items() if not p.exists() or p.read_text(encoding="utf-8") != t + "\n"]
        if stale:
            print("out of date:", ", ".join(sorted(stale)))
            return 1
        print(f"ok: {len(files)} pages are up to date")
        return 0
    for p, t in files.items():
        p.write_text(t + "\n", encoding="utf-8")
    print(f"wrote {len(files)} pages")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
