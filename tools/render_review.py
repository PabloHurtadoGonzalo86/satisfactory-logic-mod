#!/usr/bin/env python3
"""Render docs/review/theory-review-20261013.md from its JSON (generated, do not edit by hand).
Use --check to fail when the Markdown is out of date."""
import json, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "docs" / "review" / "theory-review-20261013.json"
DST = ROOT / "docs" / "review" / "theory-review-20261013.md"
ORDER = ["error", "misleading", "gap", "nit"]

ADDENDUM = """## Qué se hizo con este informe

Los 22 problemas se trataron en una ronda de correcciones (13-oct-2026), cada uno re-verificado contra el texto
antes de aplicarlo, y después los revisó un verificador adversarial independiente: **21 resueltos y 1 parcial**,
más 6 regresiones menores que se corrigieron en una segunda ronda. Los scripts de la teoría salen con código 0.

Tres puntos donde lo aplicado difiere de lo propuesto aquí, y por qué:

- **R06, la cifra del revisor era un artefacto.** El informe propone «semiperiodo 5, periodo >= 10» para el flip-flop T
  de puertas. Al medirlo de nuevo con el simulador de la serie resultó que ese número salía de juzgar el último flanco
  antes de que Q se asentara. Medido bien: con T fijo a 1, semiperiodo mínimo 3 (periodo 6); con T cambiando al azar
  fuera de la ventana prohibida, semiperiodo 4 (periodo 8). Ambas cifras las genera el script.
- **R09, el nombre de las señales del sumador.** Renombrar `p` a `q` chocaba con otra `q` del mismo apartado; se adoptó la
  nomenclatura de la literatura de sumadores (`g` generación, `p` propagación).
- **R19, «Paso» frente a «paso».** No se renombraron los apartados porque todos los documentos y los comprobadores los
  referencian como «Paso N»; se declaró la convención (sección con mayúscula, unidad de tiempo en minúscula) y se unificó.
"""

def build() -> str:
    r = json.loads(SRC.read_text(encoding="utf-8"))
    o = ["# Revisión conceptual independiente de docs/theory (13-oct-2026)\n",
         "_Generado desde `theory-review-20261013.json` por `tools/render_review.py`; no editar a mano. "
         "Las tablas y cifras de la teoría las comprueban scripts; esta revisión mira la prosa._\n",
         "## Veredicto global\n", " ".join(r["overall"].split()) + "\n"]
    counts = {s: sum(1 for i in r["issues"] if i["severity"] == s) for s in ORDER}
    o.append("## Resumen\n")
    o.append("| error | misleading | gap | nit |\n| ---: | ---: | ---: | ---: |\n"
             f"| {counts['error']} | {counts['misleading']} | {counts['gap']} | {counts['nit']} |\n")
    n = 0
    for sev in ORDER:
        o.append(f"## {sev}\n")
        for i in [x for x in r["issues"] if x["severity"] == sev]:
            n += 1
            o.append(f"### R{n:02d}. `{i['file']}` — {' '.join(i['where'].split())}\n")
            o.append("**Problema.** " + " ".join(i["problem"].split()) + "\n")
            o.append("**Arreglo propuesto.** " + " ".join(i["proposedFix"].split()) + "\n")
            if i.get("evidence"):
                o.append("**Evidencia.** " + " ".join(i["evidence"].split()) + "\n")
    o.append(ADDENDUM)
    return "\n".join(o)

if __name__ == "__main__":
    text = build() + "\n"
    if "--check" in sys.argv:
        ok = DST.exists() and DST.read_text(encoding="utf-8") == text
        print("ok" if ok else "out of date"); sys.exit(0 if ok else 1)
    DST.write_text(text, encoding="utf-8"); print("wrote", DST.name)
