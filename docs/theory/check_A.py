#!/usr/bin/env python3
"""check_A.py - verificacion de los documentos 01, 02 y 03 (clave A).

Que hace:
  1. Regenera los tres documentos en memoria (build_A.py) y comprueba que los
     .md escritos son identicos (sin ediciones a mano) y que cada linea de
     tabla, bloque, afirmacion numerica y dato del enunciado aparece
     LITERALMENTE en su .md.
  2. Comprueba la estructura exigida (secciones, 2 lineas de Objetivo,
     2 a 5 lineas en "En Redstone / en Satisfactory", 5 vinetas de Resumen,
     separador antes de la solucion, diagramas solo ASCII).
  3. Verifica la semantica con codigo INDEPENDIENTE de lib_A.py: lee las
     tablas, netlists, mapas de Karnaugh y ecuaciones del propio .md y las
     recalcula (evaluador de formulas propio, busqueda DFS propia, cierre
     propio, Quine-McCluskey por fuerza bruta).
  4. Exige que todo numero 0b.. / 0x.. de la prosa sea una ecuacion verificada
     o un dato del enunciado registrado.

Uso:  python3 -W error check_A.py     (exit 0 = todo correcto)
"""
from __future__ import annotations

import pathlib
import re
import sys
import warnings
from itertools import combinations, product

sys.dont_write_bytecode = True  # no dejar __pycache__ junto a los documentos
warnings.simplefilter("error")  # cualquier aviso (SyntaxWarning, DeprecationWarning...) es un fallo

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import build_A  # noqa: E402  (solo para regenerar los documentos)

N_CHECKS = 0


def ok(cond: bool, msg: str = "") -> None:
    global N_CHECKS
    N_CHECKS += 1
    if not cond:
        raise AssertionError(msg)


def section(name: str) -> None:
    print(f"[ok] {name} (acumuladas: {N_CHECKS})")


# ==========================================================================
# Evaluador de formulas booleanas (independiente de lib_A)
# ==========================================================================
GATE2 = {
    "AND": lambda a, b: a & b, "OR": lambda a, b: a | b, "XOR": lambda a, b: a ^ b,
    "NAND": lambda a, b: 1 - (a & b), "NOR": lambda a, b: 1 - (a | b), "XNOR": lambda a, b: 1 - (a ^ b),
}
F4_ONS = (0, 1, 2, 5, 8, 9, 10)
CALLS = dict(GATE2)
CALLS["NOT"] = lambda a: 1 - a
CALLS["MAJ"] = lambda a, b, c: int(a + b + c >= 2)
CALLS["F4"] = lambda a, b, c, d: int(((a << 3) | (b << 2) | (c << 1) | d) in F4_ONS)
CALLS["LETRA"] = lambda a, b, c, d: int(((a << 3) | (b << 2) | (c << 1) | d) >= 10)
KNOWN_FN = {  # funcion con nombre -> (numero de variables, conjunto de minterms)
    "MAJ": (3, {3, 5, 6, 7}), "F4": (4, set(F4_ONS)), "LETRA": (4, set(range(10, 16))),
    "OR": (2, {1, 2, 3}), "XOR": (2, {1, 2}),
    "FIN": (3, {5}),  # contador de 0 a 5: vale 1 en el ultimo valor
}
KNOWN_DC = {"FIN": {6, 7}}  # casillas «no importa»: los valores 6 y 7 que el contador nunca alcanza

TOKEN_RE = re.compile(r"¬|·|\+|⊕|\(|\)|,|[A-Za-z_][A-Za-z0-9_]*|[01]")


class Parser:
    def __init__(self, s: str):
        self.t = TOKEN_RE.findall(s)
        ok("".join(self.t) == re.sub(r"\s+", "", s), f"formula no tokenizable: {s!r}")
        self.i = 0

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def eat(self, want=None):
        tok = self.peek()
        ok(tok is not None and (want is None or tok == want), f"se esperaba {want!r}, hay {tok!r}")
        self.i += 1
        return tok

    def parse(self):
        node = self.orx()
        ok(self.i == len(self.t), f"sobran tokens: {self.t[self.i:]}")
        return node

    def orx(self):
        kids = [self.xorx()]
        while self.peek() in ("+", "OR"):
            self.eat()
            kids.append(self.xorx())
        return ("or", kids) if len(kids) > 1 else kids[0]

    def xorx(self):
        kids = [self.andx()]
        while self.peek() in ("⊕", "XOR"):
            self.eat()
            kids.append(self.andx())
        return ("xor", kids) if len(kids) > 1 else kids[0]

    def andx(self):
        kids = [self.notx()]
        while self.peek() in ("·", "AND"):
            self.eat()
            kids.append(self.notx())
        return ("and", kids) if len(kids) > 1 else kids[0]

    def notx(self):
        if self.peek() in ("¬", "NOT"):
            self.eat()
            return ("not", self.notx())
        return self.atom()

    def atom(self):
        tok = self.eat()
        if tok == "(":
            e = self.orx()
            self.eat(")")
            return e
        if tok in ("0", "1"):
            return ("const", int(tok))
        if tok in CALLS and self.peek() == "(":
            self.eat("(")
            args = [self.orx()]
            while self.peek() == ",":
                self.eat(",")
                args.append(self.orx())
            self.eat(")")
            return ("call", tok, args)
        ok(re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", tok) is not None, f"token inesperado {tok!r}")
        return ("var", tok)


def parse_formula(s: str):
    return Parser(s).parse()


def ev(node, env: dict) -> int:
    k = node[0]
    if k == "const":
        return node[1]
    if k == "var":
        return env[node[1]]
    if k == "not":
        return 1 - ev(node[1], env)
    if k in ("and", "or", "xor"):
        vals = [ev(c, env) for c in node[1]]
        r = vals[0]
        for v in vals[1:]:
            r = (r & v) if k == "and" else (r | v) if k == "or" else (r ^ v)
        return r
    return CALLS[node[1]](*[ev(a, env) for a in node[2]])


def variables(node) -> set:
    k = node[0]
    if k == "var":
        return {node[1]}
    if k == "const":
        return set()
    if k == "not":
        return variables(node[1])
    if k == "call":
        return set().union(*[variables(a) for a in node[2]])
    return set().union(*[variables(c) for c in node[1]])


def cost(node) -> dict:
    """Puertas de 2 entradas; los NOT de una misma variable se comparten."""
    negvars: set = set()
    c = {"AND": 0, "OR": 0, "XOR": 0, "NOT": 0}

    def walk(n):
        k = n[0]
        if k == "not":
            if n[1][0] == "var":
                negvars.add(n[1][1])
            else:
                c["NOT"] += 1
                walk(n[1])
        elif k in ("and", "or", "xor"):
            c[k.upper()] += len(n[1]) - 1
            for ch in n[1]:
                walk(ch)

    walk(node)
    c["NOT"] += len(negvars)
    c["total"] = sum(c.values())
    return c


def code_of(node, names: list[str]) -> int:
    """Codigo de una formula: el bit k es la salida en la fila k (la primera variable es el bit alto)."""
    code = 0
    for k, row in enumerate(product((0, 1), repeat=len(names))):
        code |= ev(node, dict(zip(names, row))) << k
    return code


# ==========================================================================
# Lectura de los .md
# ==========================================================================
def parse_tables(md: str) -> dict:
    """id -> (caption, headers, rows). Una tabla = titulo en negrita + cabecera + separador + filas."""
    lines = md.split("\n")
    out = {}
    i = 0
    while i < len(lines):
        m = re.fullmatch(r"\*\*Tabla (\d+\.\d+)\. (.+)\*\*", lines[i])
        if m:
            tid, cap = m.group(1), m.group(2)
            ok(lines[i + 1] == "" and lines[i + 2].startswith("|"), f"formato de tabla {tid}")
            hdr = [h.strip() for h in lines[i + 2][1:-1].split("|")]
            ok(re.fullmatch(r"\|( --- \|)+", lines[i + 3]) is not None, f"separador de {tid}")
            rows = []
            j = i + 4
            while j < len(lines) and lines[j].startswith("|"):
                rows.append([x.strip() for x in lines[j][1:-1].split("|")])
                j += 1
            for r in rows:
                ok(len(r) == len(hdr), f"fila ancha en {tid}: {r}")
            ok(tid not in out, f"tabla repetida {tid}")
            out[tid] = (cap, hdr, rows)
            i = j
        else:
            i += 1
    return out


def fenced_blocks(md: str) -> list[str]:
    return re.findall(r"```text\n(.*?)\n```", md, flags=re.S)


def sections(md: str) -> dict:
    """Titulo de seccion de nivel 2 -> lineas de su cuerpo."""
    out: dict = {}
    cur = None
    for ln in md.split("\n"):
        if ln.startswith("## "):
            cur = ln[3:]
            out[cur] = []
        elif cur is not None:
            out[cur].append(ln)
    return out


# ==========================================================================
# Busqueda exhaustiva propia (DFS) y cierres propios
# ==========================================================================
def var_codes_n(n: int) -> list[int]:
    return [sum(row[i] << k for k, row in enumerate(product((0, 1), repeat=n))) for i in range(n)]


def make_gate_fns(names: list[str], n: int):
    mask = (1 << (1 << n)) - 1
    fns2 = {
        "AND": lambda x, y: x & y, "OR": lambda x, y: x | y, "XOR": lambda x, y: x ^ y,
        "NAND": lambda x, y: ~(x & y) & mask, "NOR": lambda x, y: ~(x | y) & mask,
    }
    two = [fns2[g] for g in names if g in fns2]
    one = [(lambda x: ~x & mask)] if "NOT" in names else []
    return two, one, mask


def dfs_min(n: int, names: list[str], max_gates: int) -> dict:
    """Minimo de puertas de cada funcion, probando TODAS las secuencias de hasta max_gates puertas."""
    two, one, mask = make_gate_fns(names, n)
    start = var_codes_n(n)
    best = {x: 0 for x in start}

    def rec(sig: list[int], depth: int) -> None:
        have = set(sig)
        cands = set()
        for f in two:
            for i in range(len(sig)):
                for j in range(i, len(sig)):
                    cands.add(f(sig[i], sig[j]))
        for f in one:
            for x in sig:
                cands.add(f(x))
        for z in cands - have:
            if z not in best or best[z] > depth + 1:
                best[z] = depth + 1
            if depth + 1 < max_gates:
                rec(sig + [z], depth + 1)

    rec(list(start), 0)
    return best


def closure_n(names: list[str], n: int, consts: tuple = ()) -> int:
    two, one, mask = make_gate_fns(names, n)
    s = set(var_codes_n(n)) | {mask if k else 0 for k in consts}
    while True:
        new = set()
        for f in two:
            for x in s:
                for y in s:
                    new.add(f(x, y))
        for f in one:
            for x in s:
                new.add(f(x))
        if new <= s:
            return len(s)
        s |= new


def brute_min_sop(ons: set, n: int, dc: set = frozenset()) -> tuple[int, int, list]:
    """(terminos, literales) minimos de una suma de productos, por fuerza bruta sobre implicantes primos.

    Los implicantes pueden cubrir unos y casillas «no importa» (dc); solo hay que cubrir los unos."""
    cubes = list(product((0, 1, None), repeat=n))

    def covers(cube):
        return {sum(b << (n - 1 - i) for i, b in enumerate(row)) for row in product((0, 1), repeat=n)
                if all(c is None or c == b for c, b in zip(cube, row))}

    imps = [c for c in cubes if covers(c) <= (ons | set(dc))]

    def contains(big, small):
        return all(b is None or b == s for b, s in zip(big, small))

    primes = [c for c in imps if not any(o != c and contains(o, c) for o in imps)]
    best = None
    for k in range(1, len(primes) + 1):
        for sub in combinations(primes, k):
            cov = set()
            for cb in sub:
                cov |= covers(cb)
            if cov >= ons:
                lits = sum(x is not None for cb in sub for x in cb)
                if best is None or lits < best[1]:
                    best = (k, lits, sub)
        if best:
            break
    ok(best is not None, "sin cobertura")
    return best[0], best[1], primes


# ==========================================================================
# Verificacion de netlists
# ==========================================================================
NET_TARGET = {
    "NOT": (1, lambda a: 1 - a), "AND": (2, GATE2["AND"]), "OR": (2, GATE2["OR"]), "XOR": (2, GATE2["XOR"]),
    "NAND": (2, GATE2["NAND"]), "NOR": (2, GATE2["NOR"]), "XNOR": (2, GATE2["XNOR"]), "MAJ": (3, CALLS["MAJ"]),
}
NETS: dict = {}  # (objetivo, base, puertas) -> dict


def verify_netlist(text: str) -> None:
    lines = text.split("\n")
    m = re.fullmatch(r"# objetivo: (\w+) \| base: ([\w/]+) \| puertas: (\d+) \| entradas: ([A-D, ]+)", lines[0])
    ok(m is not None, f"cabecera de netlist: {lines[0]!r}")
    tgt, base, ng, ins = m.group(1), m.group(2), int(m.group(3)), [x.strip() for x in m.group(4).split(",")]
    allowed = set(base.split("/"))
    ok(len(lines) - 1 == ng, f"numero de puertas declarado {ng} != lineas {len(lines) - 1}")
    gates = []
    defined = set(ins)
    for ln in lines[1:]:
        g = re.fullmatch(r"(\w+) = (\w+)\(([\w, ]+)\)", ln)
        ok(g is not None, f"linea de netlist: {ln!r}")
        name, gate, args = g.group(1), g.group(2), [a.strip() for a in g.group(3).split(",")]
        ok(name not in defined, f"senal redefinida {name}")
        ok(all(a in defined for a in args), f"argumento no definido en {ln!r}")
        ok(gate in allowed, f"puerta {gate} fuera de la base {base}")
        ok(len(args) == (1 if gate == "NOT" else 2), f"aridad de {ln!r}")
        gates.append((name, gate, args))
        defined.add(name)
    k, fn = NET_TARGET[tgt]
    ok(k == len(ins), "numero de entradas del objetivo")
    depth = {i: 0 for i in ins}
    code = 0
    for r, row in enumerate(product((0, 1), repeat=k)):
        env = dict(zip(ins, row))
        for name, gate, args in gates:
            env[name] = CALLS[gate](*[env[a] for a in args])
        ok(env[gates[-1][0]] == fn(*row), f"netlist {tgt}/{base} falla en la fila {row}")
        code |= env[gates[-1][0]] << r
    for name, gate, args in gates:
        depth[name] = 1 + max(depth[a] for a in args)
    NETS[(tgt, base, ng)] = {"inputs": ins, "gates": gates, "depth": depth[gates[-1][0]], "code": code}


# ==========================================================================
# Verificadores de mapas de Karnaugh y bloques
# ==========================================================================
def gray_codes(k: int) -> list[str]:
    if k == 1:
        return ["0", "1"]
    prev = gray_codes(k - 1)
    return ["0" + x for x in prev] + ["1" + x for x in reversed(prev)]


def verify_kmap(text: str, index: bool) -> None:
    lines = text.split("\n")
    if index:
        m = re.fullmatch(r"# Numero de minterm de cada casilla \((\d) variables\)", lines[0])
        ok(m is not None, lines[0])
        n = int(m.group(1))
        ons = None
        dc: set = set()
    else:
        m = re.fullmatch(r"# K-mapa de (\w+)\(([A-D,]+)\), unos en los minterms ([\d,]+)(?: y X en los minterms ([\d,]+))?", lines[0])
        ok(m is not None, lines[0])
        fname, names, ons = m.group(1), m.group(2).split(","), {int(x) for x in m.group(3).split(",")}
        dc = {int(x) for x in m.group(4).split(",")} if m.group(4) else set()
        n = len(names)
        ok(KNOWN_FN[fname][0] == n and KNOWN_FN[fname][1] == ons, f"minterms de {fname} no coinciden con su definicion")
        ok(KNOWN_DC.get(fname, set()) == dc and not (ons & dc), f"casillas X de {fname} no coinciden con su definicion")
    rb, cb = {2: (1, 1), 3: (1, 2), 4: (2, 2)}[n]
    colvars = lines[1].strip()
    ok(len(colvars) == cb, "variables de columna")
    colcodes = lines[2].split()
    ok(colcodes == gray_codes(cb), f"columnas no estan en orden Gray: {colcodes}")
    rowlines = lines[3:]
    ok(len(rowlines) == 1 << rb, "numero de filas del mapa")
    seen = {}
    rowcodes = []
    for ln in rowlines:
        r = re.fullmatch(r"([A-D]+)=([01]+) \| (.*)", ln)
        ok(r is not None, ln)
        ok(len(r.group(1)) == rb and len(r.group(2)) == rb, "etiqueta de fila")
        rowcodes.append(r.group(2))
        cells = r.group(3).split()
        ok(len(cells) == 1 << cb, "celdas por fila")
        for cc, cell in zip(colcodes, cells):
            minterm = (int(r.group(2), 2) << cb) | int(cc, 2)
            seen[minterm] = cell
    ok(rowcodes == gray_codes(rb), f"filas no estan en orden Gray: {rowcodes}")
    ok(sorted(seen) == list(range(1 << n)), "faltan casillas")
    for mt, cell in seen.items():
        if index:
            ok(int(cell) == mt, f"casilla {mt} dice {cell}")
        else:
            ok(cell == ("X" if mt in dc else str(int(mt in ons))), f"casilla {mt} dice {cell}")


def verify_formulas_block(text: str) -> None:
    for ln in text.split("\n"):
        m = re.fullmatch(r"(\w+)\(([A-D,]+)\) = (.+?)    # (.+)", ln)
        ok(m is not None, f"linea de formula: {ln!r}")
        name, names, formula, note = m.group(1), m.group(2).split(","), m.group(3), m.group(4)
        n, ons = KNOWN_FN[name]
        ok(n == len(names), f"variables de {name}")
        node = parse_formula(formula)
        ok(variables(node) <= set(names), f"variables ajenas en {formula}")
        dc = KNOWN_DC.get(name, set())
        if dc:  # solo las funciones con casillas X distinguen las dos lecturas
            ok(("sin usar las X" in note) != ("usando las X" in note), f"nota de {name} sin lectura de las X: {note!r}")
        free = dc if "usando las X" in note else set()  # sin usar las X, las casillas X valen 0
        for k, row in enumerate(product((0, 1), repeat=n)):
            if k in free:
                continue  # «no importa»: aqui vale lo que convenga
            ok(ev(node, dict(zip(names, row))) == int(k in ons), f"{name} = {formula} falla en la fila {k}")


def verify_calc_block(text: str) -> None:
    lines = text.split("\n")
    ok(len(lines) == 4, "bloque de calculo")
    a = lines[0].strip()
    m = re.fullmatch(r"(AND| OR|XOR) (0b[01]+)", lines[1])
    ok(m is not None, lines[1])
    op, b = m.group(1).strip(), m.group(2)
    ok(re.fullmatch(r"-+", lines[2].strip()) is not None and len(lines[2].strip()) == len(a), "raya")
    r = lines[3].strip()
    w = len(a) - 2
    ok(len(b) - 2 == w and len(r) - 2 == w and w % 4 == 0, "anchos")
    ok(int(r, 2) == GATE2[op](int(a, 2), int(b, 2)), f"{a} {op} {b} != {r}")


def verify_group_block(text: str) -> None:
    l1, l2 = text.split("\n")
    m = re.fullmatch(r"([01]{4}) ([01]{4})   \(grupos de 4 bits\)", l1)
    ok(m is not None, l1)
    d = re.fullmatch(r"\s+([0-9A-F])\s+([0-9A-F])   \(un digito hex por grupo\)", l2)
    ok(d is not None, l2)
    ok(f"{int(m.group(1), 2):X}" == d.group(1) and f"{int(m.group(2), 2):X}" == d.group(2), "digitos hex")


# ==========================================================================
# Verificacion generica de tablas de verdad
# ==========================================================================
def verify_truth_table(tid: str, hdr: list[str], rows: list[list[str]]) -> None:
    i = 1 if hdr[0] == "m" else 0
    names = []
    while i < len(hdr) and re.fullmatch(r"[A-D]", hdr[i]):
        names.append(hdr[i])
        i += 1
    n = len(names)
    ok(n >= 1, f"tabla {tid} sin entradas")
    first_out = i
    expected_inputs = list(product((0, 1), repeat=n))
    ok(len(rows) == len(expected_inputs), f"tabla {tid}: faltan filas")
    for r_idx, (row, inp) in enumerate(zip(rows, expected_inputs)):
        off = 1 if hdr[0] == "m" else 0
        ok(row[off:off + n] == [str(x) for x in inp], f"tabla {tid}: entradas de la fila {r_idx}")
        if off:
            ok(row[0] == str(r_idx), f"tabla {tid}: indice m de la fila {r_idx}")
        env = dict(zip(names, inp))
        for j in range(first_out, len(hdr)):
            h = hdr[j]
            bm = re.fullmatch(r"(NOT|AND|OR|XOR|NAND|NOR|XNOR) \((\d+) (NAND|NOR)\)", h)
            if bm:
                net = NETS[(bm.group(1), bm.group(3), int(bm.group(2)))]
                local = {k_: env[k_] for k_ in net["inputs"]}
                val = None
                e2 = dict(local)
                for name, gate, args in net["gates"]:
                    e2[name] = CALLS[gate](*[e2[a] for a in args])
                    val = e2[name]
                # y ademas coincide con la puerta nativa del mismo nombre
                nat = NET_TARGET[bm.group(1)][1](*[env[k_] for k_ in net["inputs"]])
                ok(val == nat, f"tabla {tid}: {h} no coincide con la puerta nativa")
                name_out = None
            else:
                nm = re.fullmatch(r"(\w+) = (.+)", h)
                name_out, formula = (nm.group(1), nm.group(2)) if nm else (None, h)
                val = ev(parse_formula(formula), env)
            ok(row[j] == str(val), f"tabla {tid}, fila {r_idx}, columna {h!r}: {row[j]} != {val}")
            if name_out:
                env[name_out] = val


# ==========================================================================
# Verificadores de tablas con formato propio
# ==========================================================================
def chk_group_table(rows, ons: set, names: list[str], total_min: tuple[int, int]) -> None:
    n = len(names)
    union: set = set()
    lits = 0
    for idx, r in enumerate(rows, 1):
        ok(int(r[0]) == idx, "numero de grupo")
        ms = [int(x) for x in r[2].split(", ")]
        fixed = {}
        for part in r[3].split(", "):
            nm_, val = part.split("=")
            fixed[names.index(nm_)] = int(val)
        full = {m for m in range(1 << n) if all(((m >> (n - 1 - j)) & 1) == v for j, v in fixed.items())}
        ok(set(ms) == full and ms == sorted(ms), f"grupo {idx}: no es el subcubo de {fixed}")
        ok(int(r[1]) == len(ms) == 2 ** (n - len(fixed)), f"grupo {idx}: tamano")
        ok(set(ms) <= ons, f"grupo {idx}: incluye ceros")
        term = "·".join(names[j] if v else "¬" + names[j] for j, v in sorted(fixed.items()))
        ok(r[4] == term, f"grupo {idx}: termino {r[4]} != {term}")
        for j in list(fixed):  # primo: no se puede agrandar
            bigger = {m for m in range(1 << n) if all(((m >> (n - 1 - jj)) & 1) == v for jj, v in fixed.items() if jj != j)}
            ok(not bigger <= ons, f"grupo {idx}: se puede agrandar quitando {names[j]}")
        union |= set(ms)
        lits += len(fixed)
    ok(union == ons, "los grupos no cubren exactamente los unos")
    ok((len(rows), lits) == total_min, f"cobertura no minima: {(len(rows), lits)} vs {total_min}")


def chk_cost_table(rows, fname: str, names: list[str]) -> None:
    n, ons = KNOWN_FN[fname]
    for r in rows:
        node = parse_formula(r[1])
        for k, row in enumerate(product((0, 1), repeat=n)):
            ok(ev(node, dict(zip(names, row))) == int(k in ons), f"{fname}: {r[0]} falla en la fila {k}")
        c = cost(node)
        ok([int(x) for x in r[2:]] == [c["NOT"], c["AND"], c["OR"], c["total"]], f"coste de {r[0]}: {r[2:]} vs {c}")
        ok(c["XOR"] == 0, "sin XOR en estas formas")


def width_of(tok: str) -> int:
    return len(tok) - 2 if tok.startswith("0b") else 4 * (len(tok) - 2)


# ==========================================================================
# Ecuaciones de la prosa (0b/0x): verificacion de cadenas y recuento de restos
# ==========================================================================
TOK = r"(?:0b[01]+|0x[0-9A-F]+)"
ITEM = (
    rf"(?:NOT\s+{TOK}|{TOK}\s(?:AND|OR|XOR)\s{TOK}|{TOK}\s(?:<<|>>)\s\d+|{TOK}|\d+(?:\s[×+\-]\s\d+)*)"
)
CHAIN_RE = re.compile(rf"(?<![\w^.]){ITEM}(?: = {ITEM})+(?![\w^])(?!\.\d)")
ANY_TOKEN_RE = re.compile(TOK)


def tok_val(t: str) -> int:
    return int(t, 2) if t.startswith("0b") else int(t, 16)


def eval_item(item: str):
    """(valor, ancho_de_operacion_o_None, [tokens])."""
    toks = ANY_TOKEN_RE.findall(item)
    m = re.fullmatch(rf"NOT\s+({TOK})", item)
    if m:
        w = width_of(m.group(1))
        return tok_val(m.group(1)) ^ (2**w - 1), w, toks
    m = re.fullmatch(rf"({TOK})\s(AND|OR|XOR)\s({TOK})", item)
    if m:
        w = max(width_of(m.group(1)), width_of(m.group(3)))
        return GATE2[m.group(2)](tok_val(m.group(1)), tok_val(m.group(3))), w, toks
    m = re.fullmatch(rf"({TOK})\s(<<|>>)\s(\d+)", item)
    if m:
        w = width_of(m.group(1))
        v = tok_val(m.group(1))
        n = int(m.group(3))
        return (((v << n) & (2**w - 1)) if m.group(2) == "<<" else (v >> n)), w, toks
    if re.fullmatch(TOK, item):
        return tok_val(item), None, toks
    expr = item.replace("×", "*")
    ok(re.fullmatch(r"[0-9 +\-*]+", expr) is not None, f"expresion no valida {item!r}")
    return eval(expr, {"__builtins__": {}}, {}), None, []  # solo digitos y + - *  (validado arriba)


def prose_of(md: str) -> str:
    """El texto sin bloques de codigo ni lineas de tabla (esas ya se verifican aparte)."""
    no_blocks = re.sub(r"```text\n.*?\n```", "", md, flags=re.S)
    return "\n".join(ln for ln in no_blocks.split("\n") if not ln.startswith("|") and not ln.startswith("**Tabla "))


def verify_chains(md_prose: str, given: set) -> int:
    """Verifica cada cadena de igualdades numericas; devuelve cuantas hay. Comprueba tokens sueltos."""
    n_chains = 0
    for line in md_prose.split("\n"):
        covered = []
        for m in CHAIN_RE.finditer(line):
            items = m.group(0).split(" = ")
            vals, widths, toks = [], set(), []
            for it in items:
                v, w, ts = eval_item(it)
                vals.append(v)
                toks += ts
                if w is not None:
                    widths.add(w)
            ok(len(set(vals)) == 1, f"ecuacion falsa: {m.group(0)!r} -> {vals}")
            ok(len(widths) <= 1, f"anchos mezclados en {m.group(0)!r}")
            for t in toks:
                if widths:
                    ok(width_of(t) == next(iter(widths)), f"token {t} con ancho distinto en {m.group(0)!r}")
                elif t.startswith("0b"):
                    ok(width_of(t) % 4 == 0, f"binario sin completar nibbles: {t}")
            covered.append(m.span())
            n_chains += 1
        rest = line
        for a, b in reversed(covered):
            rest = rest[:a] + " " * (b - a) + rest[b:]
        for t in ANY_TOKEN_RE.findall(rest):
            ok(t in given, f"numero {t} sin verificar (no esta en una ecuacion ni es dato del enunciado): {line!r}")
    return n_chains


# ==========================================================================
# MAIN
# ==========================================================================
def main() -> None:
    fresh = {name: fn() for name, fn in build_A.DOCS.items()}
    mds = {}
    for name, (md_new, ctx) in fresh.items():
        path = HERE / name
        ok(path.exists(), f"falta {name}")
        mds[name] = path.read_text(encoding="utf-8")
        ok(mds[name] == md_new, f"{name} difiere de lo que genera build_A.py (editado a mano o desactualizado)")
    section("regeneracion: los tres .md son identicos a la salida de build_A.py")

    # ---------------- 1. presencia literal de cada linea registrada ----------------
    for name, (_, ctx) in fresh.items():
        md = mds[name]
        mdlines = md.split("\n")
        mdset = set(mdlines)
        for s in ctx.lines:
            ok(s in mdset, f"{name}: linea de tabla ausente: {s!r}")
        for s in ctx.claims:
            ok(s in md, f"{name}: afirmacion ausente: {s!r}")
        for s in ctx.given:
            ok(s in md, f"{name}: dato del enunciado ausente: {s!r}")
        for s in ctx.free:  # literales de ancho libre: solo los de una senal de un bit
            ok(s in md and s in ("0b0", "0b1"), f"{name}: literal de ancho libre no admitido o ausente: {s!r}")
        reg_blocks = sorted(t for _, t in ctx.blocks)
        ok(sorted(fenced_blocks(md)) == reg_blocks, f"{name}: los bloques de codigo no coinciden con los registrados")
        for kind, t in ctx.blocks:
            ok(f"```text\n{t}\n```" in md, f"{name}: bloque ausente")
        table_like = [ln for ln in mdlines if ln.startswith("|") or ln.startswith("**Tabla ")]
        ok(set(table_like) <= set(ctx.lines), f"{name}: hay lineas de tabla no generadas por el script")
    section("cada linea de tabla, bloque, afirmacion y dato aparece literalmente en su .md")

    # ---------------- 2. estructura ----------------
    heads = ["Objetivo", "Idea intuitiva", "Explicación paso a paso", "Ejemplo resuelto completo", "Tablas",
             "Mini-ejercicio", "En Redstone / en Satisfactory", "Resumen"]
    for name, md in mds.items():
        secs = sections(md)
        keys = list(secs)
        ok(len(keys) == len(heads), f"{name}: secciones {keys}")
        for k, h in zip(keys, heads):
            ok(k.startswith(h), f"{name}: seccion {k!r} no es {h!r}")
        ok(md.startswith("# 0"), f"{name}: titulo")
        obj = [x for x in secs[keys[0]] if x.strip()]
        ok(len(obj) == 2, f"{name}: Objetivo tiene {len(obj)} lineas")
        red = [x for x in secs[keys[6]] if x.strip()]
        ok(2 <= len(red) <= 5, f"{name}: 'En Redstone / en Satisfactory' tiene {len(red)} lineas")
        res = [x for x in secs[keys[7]] if x.strip()]
        ok(len(res) == 5 and all(x.startswith("- ") for x in res), f"{name}: Resumen debe tener 5 vinetas")
        ej = secs[keys[5]]
        ok("---" in ej, f"{name}: falta el separador antes de la solucion")
        ok("**Solución**" in ej[ej.index("---"):], f"{name}: la solucion no esta despues del separador")
        ok("**Solución**" not in ej[: ej.index("---")], f"{name}: la solucion aparece antes del separador")
        for bad in ("{C(", "{c.", "{nets[", "{blk_", "URL_", "{ROW5", "{t_", "{f_", "{k_"):
            ok(bad not in md, f"{name}: marcador sin sustituir {bad!r}")
        ok(re.search(r"\bNone\b|\bnan\b", md) is None, f"{name}: aparece None/nan")
    section("estructura: secciones en orden, Objetivo 2 lineas, Redstone/Satisfactory 2-5, Resumen 5, separador")

    for name, (_, ctx) in fresh.items():
        for kind, t in ctx.blocks:
            if kind != "formulas":
                ok(t.isascii(), f"{name}: bloque '{kind}' no es ASCII puro")
    section("diagramas (netlist, mapas, calculos, ascii) en ASCII puro")

    # ---------------- 3. semantica independiente ----------------
    all_tables = {n: parse_tables(md) for n, md in mds.items()}
    d1, d2, d3 = "01-numeracion-binaria-hex.md", "02-algebra-de-boole-y-puertas.md", "03-universalidad-nand.md"

    for name, (_, ctx) in fresh.items():
        for kind, t in ctx.blocks:
            if kind == "netlist":
                verify_netlist(t)
            elif kind == "kmap":
                verify_kmap(t, False)
            elif kind == "kmap-index":
                verify_kmap(t, True)
            elif kind == "formulas":
                verify_formulas_block(t)
            elif kind == "calc":
                verify_calc_block(t)
            elif kind == "group":
                verify_group_block(t)
            elif kind == "ascii":
                ok(ANY_TOKEN_RE.search(t) is None, "bloque ascii con numeros")
            else:
                ok(False, f"tipo de bloque desconocido {kind}")
    section(f"bloques: {len(NETS)} netlists evaluadas en todas las filas, mapas de Karnaugh (orden Gray), formulas, calculos")

    truth_ids = {
        d1: {"1.6"}, d2: {"2.1", "2.2", "2.3", "2.4", "2.5", "2.6", "2.7", "2.8", "2.9", "2.12", "2.15"},
        d3: {"3.1", "3.2", "3.3", "3.4", "3.5", "3.10"},
    }
    custom_ids = {
        d1: {"1.1", "1.2", "1.3", "1.4", "1.5", "1.7"},
        d2: {"2.10", "2.11", "2.13", "2.14", "2.16", "2.17", "2.18"},
        d3: {"3.6", "3.7", "3.8", "3.9"},
    }
    for name in (d1, d2, d3):
        ok(set(all_tables[name]) == truth_ids[name] | custom_ids[name], f"{name}: tablas sin verificador: {set(all_tables[name]) ^ (truth_ids[name] | custom_ids[name])}")
        for tid in truth_ids[name]:
            _, hdr, rows = all_tables[name][tid]
            verify_truth_table(tid, hdr, rows)
    section("tablas de verdad: cada columna se recalcula desde la formula de su cabecera")

    # ---- documento 01: tablas con formato propio ----
    T1 = all_tables[d1]
    cap, hdr, rows = T1["1.2"]
    seq = int(rows[0][1])
    rem = []
    for i, r in enumerate(rows, 1):
        ok([int(x) for x in r] == [i, seq, seq // 2, seq % 2], f"1.2 fila {i}")
        rem.append(seq % 2)
        seq //= 2
    ok(seq == 0, "1.2 no llega a 0")
    rem_str = "".join(str(x) for x in reversed(rem))
    ok(int(rem_str, 2) == int(rows[0][1]), "1.2 restos")
    ok(f"de {rows[0][1]}" in cap, "1.2 caption")
    ok(f"Restos de abajo arriba: {rem_str}," in mds[d1], "1.2: texto con los restos")
    cap, hdr, rows = T1["1.1"]
    col = re.search(r"#([0-9a-f]{6})", cap).group(1)
    ok([r[0] for r in rows] == ["Rojo (R)", "Verde (G)", "Azul (B)"], "1.1 canales")
    got = ""
    for r in rows:
        v = int(r[1], 16)
        ok(re.fullmatch(r"0x[0-9A-F]{2}", r[1]) and re.fullmatch(r"0b[01]{8}", r[3]), "1.1 formato")
        ok(int(r[2]) == v == int(r[3], 2), f"1.1 {r}")
        got += f"{v:02x}"
    ok(got == col, "1.1: los canales no forman el color del titulo")
    ok(f"color {'#' + col}" in mds[d1] and mds[d1].count("#" + col) >= 3, "color citado")
    cap, hdr, rows = T1["1.3"]
    for r in rows:
        k = int(r[0])
        ok([int(r[1]), int(r[2])] == [2**k, 2**k - 1], f"1.3 {r}")
        ok(r[3] == {1: "bit", 4: "nibble", 8: "byte"}.get(k, ""), f"1.3 nombre {r}")
    ok([int(r[0]) for r in rows] == list(range(1, 9)), "1.3 filas")
    cap, hdr, rows = T1["1.4"]
    ok(hdr == ["Posición"] + [str(k) for k in range(7, -1, -1)], "1.4 cabecera")
    ok(rows[0] == ["Peso"] + [str(2**k) for k in range(7, -1, -1)], "1.4 pesos")
    m = re.fullmatch(r"Bits de (0b[01]{8})", rows[1][0])
    ok(m is not None, "1.4 etiqueta")
    ok(rows[1][1:] == list(m.group(1)[2:]), "1.4 bits del ejemplo")
    cap, hdr, rows = T1["1.5"]
    ok(len(rows) == 16, "1.5 filas")
    for v, r in enumerate(rows):
        ok(r == [f"0x{v:X}", str(v), f"0b{v:04b}"], f"1.5 fila {v}")
    cap, hdr, rows = T1["1.7"]
    x = re.search(r"x = (0b[01]{4}) \(valor (\d+)\)", cap)
    xv = int(x.group(1), 2)
    ok(xv == int(x.group(2)), "1.7 caption")
    ok([int(r[0]) for r in rows] == list(range(5)), "1.7 n")
    for r in rows:
        k = int(r[0])
        left = (xv << k) & 15
        ok(r[1:] == [f"0b{left:04b}", str(left), f"0b{xv >> k:04b}", str(xv >> k)], f"1.7 fila {k}")
    section("documento 01: tablas de potencias, pesos, hexadecimal, divisiones, canales y desplazamientos")

    given1 = set(fresh[d1][1].given)
    for t in given1:
        ok(re.fullmatch(r"(?:0b(?:[01]{4})+|0x[0-9A-F]+)", t) is not None, f"dato no canonico {t}")
    n1 = verify_chains(prose_of(mds[d1]), given1 | set(fresh[d1][1].free))
    # los datos de las tablas con numeros (captions y filas) se ignoran aqui: ya verificados arriba
    for ln in mds[d1].split("\n"):
        m = re.search(r"~(\d+) vale (-?\d+)", ln)
        if m:
            ok(~int(m.group(1)) == int(m.group(2)), ln)
        m = re.search(r"~(\d+) & (\d+) vale (\d+)", ln)
        if m:
            ok((~int(m.group(1))) & int(m.group(2)) == int(m.group(3)), ln)
    ok(eval("0b0110") == 6 == eval("0x6"), "literales de Python")
    ok(n1 > 30, "pocas ecuaciones verificadas en el documento 01")
    section(f"documento 01: {n1} ecuaciones con 0b/0x verificadas y ningun numero sin respaldo")

    # ---- documentos 02 y 03: no deben contener 0b/0x salvo ecuaciones y datos ----
    for name in (d2, d3):
        n_ = verify_chains(prose_of(mds[name]), set(fresh[name][1].given) | set(fresh[name][1].free))
        section(f"{name}: {n_} ecuaciones numericas verificadas")

    # ---- documento 02 ----
    T2 = all_tables[d2]
    ids = ["A", "B", "C", "D"]
    for tid, (fname, names) in (("2.10", ("MAJ", ids[:3])), ("2.13", ("F4", ids)), ):
        _, hdr, rows = T2[tid]
        ok(hdr == ["Grupo", "Casillas", "Minterms", "Variables que no cambian", "Término"], f"{tid} cabecera")
        n_, ons = KNOWN_FN[fname]
        tm, tl, _ = brute_min_sop(ons, n_)
        chk_group_table(rows, ons, names, (tm, tl))
    for tid, fname, names in (("2.11", "MAJ", ids[:3]), ("2.14", "F4", ids), ("2.18", "LETRA", ids)):
        _, hdr, rows = T2[tid]
        chk_cost_table(rows, fname, names)
    # minimalidad de las formulas de los bloques 'formulas' frente a la fuerza bruta
    for t in [t for k, t in fresh[d2][1].blocks if k == "formulas"]:
        for ln in t.split("\n"):
            m = re.fullmatch(r"(\w+)\(([A-D,]+)\) = (.+?)    # (.+)", ln)
            if "minima" in m.group(4):
                n_, ons = KNOWN_FN[m.group(1)]
                dc_ = KNOWN_DC.get(m.group(1), set()) if "usando las X" in m.group(4) else set()
                node = parse_formula(m.group(3))
                terms = node[1] if node[0] == "or" else [node]
                lits = sum(len(tn[1]) if tn[0] == "and" else 1 for tn in terms)
                tm, tl, _ = brute_min_sop(ons, n_, dc_)
                ok((len(terms), lits) == (tm, tl), f"{m.group(1)}: la SOP minima declarada no es minima")
    # SOP/POS canonicas = suma de minterms / producto de maxterms exactos
    maj_blocks = [t for k, t in fresh[d2][1].blocks if k == "formulas" and t.startswith("MAJ")]
    ok(any("canonica" in t for t in maj_blocks), "formulas canonicas de MAJ")
    # tabla 2.16: identidades
    _, hdr, rows = T2["2.16"]
    for r in rows:
        l, rr = parse_formula(r[1]), parse_formula(r[2])
        names = sorted(variables(l) | variables(rr))
        ok(int(r[3]) == len(names), f"2.16 variables {r}")
        ok(int(r[4]) == 2 ** len(names), f"2.16 combinaciones {r}")
        for row in product((0, 1), repeat=len(names)):
            env = dict(zip(names, row))
            ok(ev(l, env) == ev(rr, env), f"2.16: {r[1]} != {r[2]} en {env}")
        ok(r[5] == "sí", "2.16 veredicto")
    # tabla 2.17: puertas como nibble
    _, hdr, rows = T2["2.17"]
    for r in rows:
        node = parse_formula(r[1])
        outs = [int(x) for x in r[2:6]]  # filas 3, 2, 1, 0
        for out, (a, b) in zip(outs, ((1, 1), (1, 0), (0, 1), (0, 0))):
            ok(ev(node, {"A": a, "B": b}) == out, f"2.17 {r[0]}")
            ok(GATE2[r[0]](a, b) == out, f"2.17 puerta nativa {r[0]}")
        cm = re.fullmatch(r"(0b[01]{4}) = (0x[0-9A-F])", r[6])
        ok(cm is not None, "2.17 codigo")
        ok(int(cm.group(1), 2) == int(cm.group(2), 16) == sum(o << k for o, k in zip(outs, (3, 2, 1, 0))), f"2.17 codigo {r}")
        ok(code_of(node, ["A", "B"]) == int(cm.group(1), 2), "2.17 codigo por formula")
    # MAJ: el codigo 0b11101000 = 232 = 0xE8 de la prosa
    mcode = code_of(parse_formula("MAJ(A,B,C)"), ["A", "B", "C"])
    ok(f"0b{mcode:08b} = {mcode} = 0x{mcode:02X}" in mds[d2], "codigo de MAJ")
    # busqueda exhaustiva propia: MAJ con AND/OR/NOT y con XOR
    for gs in (["AND", "OR", "NOT"], ["AND", "OR", "XOR", "NOT"]):
        res3 = dfs_min(3, gs, 3)
        ok(mcode not in res3, f"MAJ se construye con 3 puertas o menos con {gs}")
        res4 = dfs_min(3, gs, 4)
        ok(res4.get(mcode) == 4, f"MAJ no se construye con 4 puertas con {gs}")
    m = re.search(r"con (\d+) puertas o menos: ninguno calcula MAJ\. Con (\d+) sí", mds[d2])
    ok(m is not None and (int(m.group(1)), int(m.group(2))) == (3, 4), "texto del minimo de MAJ")
    ok("Añadiendo XOR a las puertas permitidas el mínimo sigue siendo 4." in mds[d2], "texto del minimo con XOR")
    section("documento 02: grupos primos y minimos (fuerza bruta), costes por arbol de formula, identidades, codigos y minimo de MAJ por DFS")

    # ---- documento 03 ----
    T3 = all_tables[d3]
    best_nand = dfs_min(2, ["NAND"], 5)
    best_nor = dfs_min(2, ["NOR"], 5)
    ok(len(best_nand) == 16 and len(best_nor) == 16, "no se alcanzan las 16 funciones")
    maxsearch = max(max(best_nand.values()), max(best_nor.values()))
    ok(f"con hasta {maxsearch} puertas" in mds[d3], "texto del tope de la busqueda")
    A2, B2 = var_codes_n(2)
    notA = ~A2 & 15
    _, hdr, rows = T3["3.6"]
    ok(len(rows) == 12 and "en los 12 casos" in mds[d3], "doce construcciones")
    for r in rows:
        base, tgt, ng, depth, nrows, verdict, mn = r[0], r[1], int(r[2]), int(r[3]), int(r[4]), r[5], int(r[6])
        net = NETS[(tgt, base, ng)]
        ok(depth == net["depth"], f"3.6 capas de {tgt}/{base}")
        ok(nrows == 2 ** len(net["inputs"]) and verdict == "sí", "3.6 filas")
        tcode = notA if tgt == "NOT" else sum(NET_TARGET[tgt][1](a, b) << k for k, (a, b) in enumerate(product((0, 1), repeat=2)))
        k_in = len(net["inputs"])
        exp_code = sum(NET_TARGET[tgt][1](*row) << i for i, row in enumerate(product((0, 1), repeat=k_in)))
        ok(net["code"] == exp_code, "3.6 codigo de la netlist")
        best = (best_nand if base == "NAND" else best_nor)[tcode]
        ok(mn == best == ng, f"3.6: {tgt}/{base} usa {ng}, minimo {best}, tabla {mn}")
    _, hdr, rows = T3["3.7"]
    for r in rows:
        k = int(r[0])
        ok([int(r[1]), int(r[2])] == [2**k, 2 ** (2**k)], f"3.7 {r}")
    _, hdr, rows = T3["3.8"]
    ok(len(rows) == 16, "3.8 filas")
    for code, r in enumerate(rows):
        cm = re.fullmatch(r"(0b[01]{4}) = (0x[0-9A-F])", r[0])
        ok(cm is not None and int(cm.group(1), 2) == int(cm.group(2), 16) == code, f"3.8 codigo {r}")
        ok(code_of(parse_formula(r[2]), ["A", "B"]) == code, f"3.8: la formula {r[2]} no tiene el codigo {code}")
        if r[1] in GATE2:
            ok(sum(GATE2[r[1]](a, b) << k for k, (a, b) in enumerate(product((0, 1), repeat=2))) == code, f"3.8 nombre {r[1]}")
        ok([int(r[3]), int(r[4])] == [best_nand[code], best_nor[code]], f"3.8 minimos de {code}")
    _, hdr, rows = T3["3.9"]
    for r in rows:
        cell = r[0]
        consts = ()
        if cell.endswith(" y la constante 1"):
            consts = (1,)
            cell = cell[: -len(" y la constante 1")]
        gs = cell.split(", ")
        c2, c3 = closure_n(gs, 2, consts), closure_n(gs, 3, consts)
        ok([int(r[1]), int(r[2]), int(r[3]), int(r[4])] == [c2, 16, c3, 256], f"3.9 {r}")
        ok(r[5] == ("sí" if (c2, c3) == (16, 256) else "no"), f"3.9 veredicto {r}")
    section("documento 03: costes minimos NAND/NOR por DFS exhaustivo, 16 funciones por codigo, cierres de conjuntos de puertas")

    # filas del XOR con NAND en la prosa
    xor_net = NETS[("XOR", "NAND", 4)]
    cnt = 0
    for ln in mds[d3].split("\n"):
        m = re.fullmatch(r"- A=(\d), B=(\d): t=(\d), u=(\d), v=(\d), Y=(\d)", ln)
        if m:
            a, b, t_, u_, v_, y_ = map(int, m.groups())
            env = {"A": a, "B": b}
            for name, gate, args in xor_net["gates"]:
                env[name] = CALLS[gate](*[env[x] for x in args])
            ok((env["t"], env["u"], env["v"], env["Y"]) == (t_, u_, v_, y_) and y_ == a ^ b, f"fila de XOR {ln}")
            cnt += 1
    ok(cnt == 4, "las 4 filas de la traza de XOR")
    # conteos de la prosa del documento 03 contra las netlists verificadas
    resumen = [x for x in sections(mds[d3])["Resumen"] if x.strip()][0]
    exp = ("- Con NAND se construyen NOT ({}), AND ({}), OR ({}), NOR ({}) y XOR ({}); con NOR, NOT ({}), OR ({}), AND ({}), NAND ({}) y XOR ({}).")
    cnt_of = lambda tgt, base: [k[2] for k in NETS if k[0] == tgt and k[1] == base][0]  # noqa: E731
    ok(resumen == exp.format(*[f"{cnt_of(t, 'NAND')} puerta" if t == "NOT" else cnt_of(t, "NAND") for t in ("NOT", "AND", "OR", "NOR", "XOR")],
                             *[cnt_of(t, "NOR") for t in ("NOT", "OR", "AND", "NAND", "XOR")]), "resumen del documento 03")
    ok(NETS[("XNOR", "NAND", 5)]["depth"] == 4 and "Total: 5 puertas y 4 capas" in mds[d3], "ejercicio XNOR")
    ok("con 4 NAND o menos no existe" in mds[d3] and best_nand[0b1001] == 5, "XNOR no cabe en 4 NAND")
    section("documento 03: traza de XOR, resumen y ejercicio coherentes con las netlists")

    # ---- revision conceptual del 13-oct-2026: cada afirmacion nueva de la prosa se recalcula aqui ----
    m1, m2, m3 = mds[d1], mds[d2], mds[d3]
    gate_code = lambda tgt: sum(NET_TARGET[tgt][1](a, b) << k for k, (a, b) in enumerate(product((0, 1), repeat=2)))  # noqa: E731
    c_nand = {t: best_nand[gate_code(t)] for t in ("AND", "OR", "NOR", "XOR", "XNOR")} | {"NOT": best_nand[notA]}
    c_nor = {t: best_nor[gate_code(t)] for t in ("AND", "OR", "NAND", "XOR", "XNOR")} | {"NOT": best_nor[notA]}

    def unique_max(d: dict) -> str:
        top = max(d.values())
        names_ = [k for k, v in d.items() if v == top]
        ok(len(names_) == 1, f"el maximo no es unico: {d}")
        return names_[0]

    # 03, R01: XOR no es la puerta mas cara con NAND (lo es XNOR) y con NOR se invierte
    mm = re.search(r"XOR cuesta (\d+) NAND, tanto como el NOR construido con NAND en el Paso 4, .*?La más cara con NAND es XNOR: con un NOT más sobre el XOR salen (\d+) puertas", m3, re.S)
    ok(mm is not None, "texto del coste de XOR y XNOR con NAND")
    ok(int(mm.group(1)) == c_nand["XOR"] == c_nand["NOR"], "XOR cuesta lo mismo que NOR con NAND")
    ok(unique_max(c_nand) == "XNOR" and int(mm.group(2)) == c_nand["XNOR"] == c_nand["XOR"] + 1, "XNOR es la mas cara con NAND")
    ok("Con NOR la situación se invierte (Paso 6)." in m3 and unique_max(c_nor) == "XOR" and c_nor["XNOR"] == c_nor["NAND"], "con NOR se invierte")
    # y lo mismo entre las 16 funciones de dos variables (Tabla 3.8): solo XNOR con NAND y solo XOR con NOR llegan al maximo
    ok(unique_max({k: best_nand[k] for k in range(16)}) == 0b1001 and unique_max({k: best_nor[k] for k in range(16)}) == 0b0110, "la mas cara de las 16 funciones")
    ok("XOR es la puerta más cara" not in m3, "frase falsa de R01")
    # 03, R03: precios con cada base
    nand_depths = {t: v["depth"] for (t, b, _), v in NETS.items() if b == "NAND"}
    mm = re.search(r"Con solo NAND, OR cuesta (\d+) puertas en vez de 1, NOR (\d+), XOR (\d+) y XNOR (\d+), y esta última llega a (\d+) capas \(Tabla 3\.6\)", m3)
    ok(mm is not None, "texto del coste con solo NAND")
    ok([int(x) for x in mm.groups()[:4]] == [c_nand["OR"], c_nand["NOR"], c_nand["XOR"], c_nand["XNOR"]], "precios con solo NAND")
    ok(unique_max(nand_depths) == "XNOR" and int(mm.group(5)) == nand_depths["XNOR"], "XNOR es la unica con las capas maximas")
    mm = re.search(r"con solo NAND, XOR cuesta (\d+) piezas y XNOR (\d+); con solo NOR \(la puerta nativa de Redstone: la antorcha con dos entradas\), XOR cuesta (\d+), XNOR (\d+), OR (\d+) y AND (\d+)\.", m3)
    ok(mm is not None, "texto del precio con cada base")
    ok([int(x) for x in mm.groups()] == [c_nand["XOR"], c_nand["XNOR"], c_nor["XOR"], c_nor["XNOR"], c_nor["OR"], c_nor["AND"]], "precios con NAND y con NOR")
    # 03, R10: las constantes
    mm = re.search(r"La constante 0 no tiene ningún minterm, así que su suma de productos queda vacía y se escribe aparte como (\S+); la constante 1 sí es la suma de todos los minterms, pero (\S+) es mucho más corta\. Con NAND cuestan (\d+) y (\d+) puertas, Tabla 3\.8\.", m3)
    ok(mm is not None, "texto de las constantes")
    ok(code_of(parse_formula(mm.group(1)), ["A", "B"]) == 0b0000 and code_of(parse_formula(mm.group(2)), ["A", "B"]) == 0b1111, "A·¬A y A+¬A son las constantes")
    ok([k for k in range(4) if (0b0000 >> k) & 1] == [] and len([k for k in range(4) if (0b1111 >> k) & 1]) == 4, "minterms de las constantes")
    ok([int(mm.group(3)), int(mm.group(4))] == [best_nand[0b0000], best_nand[0b1111]], "coste de las constantes con NAND")
    # 03, R22 y R19: referencias a tablas y a pasos
    mm = re.search(r"\(Tablas 2\.2 y 2\.17 del documento 02; el código de la 2\.17 es (0b[01]{4}) = (0x[0-9A-F])\)", m3)
    ok(mm is not None, "referencia a las tablas 2.2 y 2.17")
    nand_row = [r for r in T2["2.17"][2] if r[0] == "NAND"][0]
    ok(nand_row[6] == f"{mm.group(1)} = {mm.group(2)}" and "2.2" in T2, "el codigo de NAND esta en la Tabla 2.17")
    ok("como dice el paso 2" not in m3 and "como dice el punto 2 de este ejemplo" in m3, "paso 2 vs punto 2 del ejemplo")
    ok("Coincide con el paso 3" not in m1 and "Coincide con el punto 3 de este ejemplo" in m1, "paso 3 vs punto 3 del ejemplo")
    # un apartado se cita como «Paso N» (con mayuscula): «paso» en minuscula es la unidad de tiempo (README, «Paso N» y «paso»)
    for nm_, mx_ in (("01", m1), ("02", m2), ("03", m3)):
        ok(re.search(r"(?<![A-Za-z])paso \d", mx_) is None, f"documento {nm_}: ningun apartado citado como «paso N» en minuscula")

    # todas las referencias «Tabla N.M» resuelven, y las de otro documento dicen de cual
    ref_re = re.compile(r"Tablas? (\d+\.\d+(?:(?:, | y )\d+\.\d+)*)")
    n_refs = 0
    for name, md in mds.items():
        own = str(int(name[:2]))
        text = prose_of(md)
        for mm in ref_re.finditer(text):
            for tid in re.findall(r"\d+\.\d+", mm.group(1)):
                pre = tid.split(".")[0]
                ok(pre in ("1", "2", "3") and tid in all_tables[{"1": d1, "2": d2, "3": d3}[pre]], f"{name}: la Tabla {tid} no existe")
                if pre != own:
                    ok(re.match(rf"\s*del documento 0{pre}", text[mm.end():mm.end() + 40]) is not None, f"{name}: la Tabla {tid} es de otro documento y no lo dice")
                n_refs += 1
    ok(n_refs > 30, "pocas referencias a tablas")

    # 02, R22: la fuerza bruta solo vale para n pequeno; el documento es combinacional como el 03
    mm = re.search(r"con (\d+) variables ya son (\d+) filas", m2)
    ok(mm is not None and 2 ** int(mm.group(1)) == int(mm.group(2)) and int(mm.group(1)) >= 20, "filas con n grande")
    ok("Mientras n sea pequeño, es tan poco que se puede **probar todo**" in m2 and "mientras n sea pequeño, cualquier igualdad" in m2, "fuerza bruta solo con n pequeno")
    ok("En este documento y en el 03 todo es combinacional" in m2 and "En estos tres documentos" not in m2, "frase combinacional")
    # 02 y 03, R15: las netlists de estos documentos no tienen tiempo
    ok("una netlist no tiene tiempo" in m2 and "que no tiene tiempo" in m3, "netlist sin tiempo")
    ok("esta es una suma aritmética de números, no un OR" in m2, "suma aritmetica en el documento 02")
    # 02, R11: casillas «no importa» con un ejemplo resuelto, todo recalculado
    ok("casillas «no importa»" in m2 and "(o X, ver más abajo las casillas «no importa»)" in m2 and "casillas X que valen 0 o 1 según convenga" in m2, "texto de las casillas no importa")
    ons_fin, dc_fin = KNOWN_FN["FIN"][1], KNOWN_DC["FIN"]
    mm = re.search(r"Un contador de (\d+) bits \(A es el bit más alto\) cuenta de 0 a (\d+) y vuelve a 0, así que sus valores (\d+) y (\d+) no aparecen nunca\. FIN\(A,B,C\) vale 1 cuando el contador está en su último valor, (\d+); en (\d+) y (\d+) da igual:", m2)
    ok(mm is not None, "texto del contador del ejemplo FIN")
    nb_, last, v1, v2, last2, v3, v4 = (int(x) for x in mm.groups())
    ok(nb_ == 3 and {last} == {last2} == ons_fin and {v1, v2} == {v3, v4} == dc_fin == set(range(last + 1, 2 ** nb_)), "el contador y sus valores inalcanzables")
    mm = re.search(
        r"la forma mínima es (\S+) \((\d+) puertas: (\d+) NOT y (\d+) AND\)\. Contando la casilla (\d+) como 1 se forma el grupo \{(\d+), (\d+)\} \(([^)]+)\): "
        r"dentro de él cambia ([A-C]), que desaparece, y queda (\S+) \(una sola puerta, un AND\)\. La casilla (\d+) se cuenta como 0: "
        r"para unirla al (\d+) habría que tomar toda la fila A=1, que incluye la casilla (\d+), un 0\.", m2)
    ok(mm is not None, "texto de la minimizacion con X")
    f_sin, tot0, nots0, ands0, as1, g_a, g_b, fixed_txt, changing, f_con, as0, with5, zero_cell = mm.groups()
    n_sin, n_con = parse_formula(f_sin), parse_formula(f_con)
    row_of = lambda k: dict(zip("ABC", ((k >> 2) & 1, (k >> 1) & 1, k & 1)))  # noqa: E731
    ok(all(ev(n_sin, row_of(k)) == int(k in ons_fin) for k in range(8)), "sin usar las X, la formula es exactamente la funcion (X = 0)")
    ok(all(ev(n_con, row_of(k)) == int(k in ons_fin) for k in range(8) if k not in dc_fin), "usando las X, la formula acierta en las casillas que importan")
    c_sin, c_con = cost(n_sin), cost(n_con)
    ok((c_sin["total"], c_sin["NOT"], c_sin["AND"]) == (int(tot0), int(nots0), int(ands0)), "coste sin usar las X")
    ok((c_con["total"], c_con["AND"], c_con["NOT"], c_con["OR"]) == (1, 1, 0, 0) and c_con["total"] < c_sin["total"], "con las X es una sola puerta y sale mas barato")
    used = sorted(k for k in dc_fin if ev(n_con, row_of(k)) == 1)
    unused = sorted(k for k in dc_fin if ev(n_con, row_of(k)) == 0)
    ok(used == [int(as1)] and unused == [int(as0)] and int(as1) in dc_fin and int(as0) in dc_fin, "una X se usa como 1 y la otra como 0")
    ok(sorted(ons_fin | set(used)) == [int(g_a), int(g_b)], "el grupo es el 5 mas la X usada")
    lits = n_con[1] if n_con[0] == "and" else [n_con]
    pol = {(l[1] if l[0] == "var" else l[1][1]): int(l[0] == "var") for l in lits}
    ok(fixed_txt == ", ".join(f"{v}={pol[v]}" for v in sorted(pol)), "variables fijas del grupo")
    ok({changing} == set("ABC") - variables(n_con), "la variable que cambia dentro del grupo")
    fila_a1 = [k for k in range(8) if row_of(k)["A"] == 1]
    ok([k for k in fila_a1 if k not in ons_fin | dc_fin] == [int(zero_cell)] and int(with5) in ons_fin and int(as0) in fila_a1, "la fila A=1 contiene un 0")
    mm = re.search(r"el circuito (\S+) da (\d) en la casilla (\d+) y (\d) en la (\d+)\.", m2)
    ok(mm is not None and mm.group(1) == f_con, "frase final del ejemplo FIN")
    ok(ev(n_con, row_of(int(mm.group(3)))) == int(mm.group(2)) == 1 and ev(n_con, row_of(int(mm.group(5)))) == int(mm.group(4)) == 0, "que da el circuito en las casillas X")
    ok("Volverás a usar estas casillas en el documento 07." in m2, "enlace al documento 07")

    # 01: notacion (ancho libre y potencia), 'el script', dashboard y Redstone
    ok("siempre con ceros a la izquierda hasta completar nibbles" not in m1 and "tantos dígitos como bits tenga lo que representa" in m1, "regla de ancho relajada")
    ok("una señal de un solo bit es simplemente 0b0 o 0b1" in m1, "senal de un bit")
    mm = re.search(r"`2 \^ (\d+)` vale (\d+) y no (\d+); la potencia se escribe `2 \*\* (\d+)`", m1)
    ok(mm is not None and int(mm.group(1)) == int(mm.group(4)) and (2 ^ int(mm.group(1))) == int(mm.group(2)) and 2 ** int(mm.group(1)) == int(mm.group(3)) and int(mm.group(2)) != int(mm.group(3)), "^ no es la potencia")
    ok("2^n es la potencia «2 elevado a n»" in m1 and "a ** n   (ojo: a ^ n es XOR, no una potencia)" in m1, "aviso de potencia")
    ok("«el script»" in m1 and "`check_*.py`" in m1, "definicion de «el script»")
    for x in (m2, m3):
        ok(re.search(r"[Ee]l script", x) is not None and "script de la serie (documento 01)" in x, "primera mencion de «el script»")
    ok(all("tu dashboard" not in x for x in (m1, m2, m3)), "tu dashboard")
    mm = re.search(r"el nivel (0b[01]{4}) = (\d+) = (0x[0-9A-F]) cuenta como «hay señal» aunque su bit más bajo valga 0", m1)
    ok(mm is not None and int(mm.group(1), 2) == int(mm.group(2)) == int(mm.group(3), 16) > 0 and int(mm.group(2)) & 1 == 0, "nivel verdadero con el bit bajo a 0")
    ok(all(bool(x & 15) == (x > 0) for x in range(16)) and "eso es «x > 0», el OR de los 4 bits" in m1, "x > 0 es el OR de los cuatro bits")
    ok("la equivalencia es de cantidad de información, no de cableado" in m1 and "no cuatro hilos con un bit cada uno" in m1, "equivalencia de informacion")
    section("revision conceptual: costes de XOR/XNOR/constantes, referencias a tablas, casillas «no importa», notacion y «el script»")

    # ---- fuentes externas citadas ----
    for name, urls in (
        (d1, ["https://minecraft.wiki/w/Redstone_mechanics", "https://minecraft.wiki/w/Redstone_circuits/Transmission", "https://minecraft.wiki/w/Redstone_circuit",
              "https://www.php.net/manual/en/language.types.integer.php", "https://www.php.net/manual/en/language.operators.bitwise.php"]),
        (d2, ["https://minecraft.wiki/w/Redstone_Torch", "https://minecraft.wiki/w/Redstone_circuit"]),
        (d3, ["https://minecraft.wiki/w/Redstone_circuits/Logic"]),
    ):
        for u in urls:
            ok(u in mds[name], f"{name}: falta la fuente {u}")
    section("fuentes externas citadas con su URL")

    print(f"TODO CORRECTO: {N_CHECKS} comprobaciones, 0 fallos, 0 avisos")


if __name__ == "__main__":
    main()
