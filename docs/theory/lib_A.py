"""Biblioteca de calculo y formato de la clave A (documentos 01, 02 y 03).

Todo numero, tabla, ecuacion o mapa de los documentos sale de aqui. Los
documentos se ensamblan en build_A.py y se verifican en check_A.py.
"""
from __future__ import annotations

from itertools import product

# --------------------------------------------------------------------------
# Formato de numeros: una sola notacion en toda la serie
# --------------------------------------------------------------------------


def bits(x: int, w: int) -> str:
    """Binario con prefijo 0b y ancho fijo de w bits (w multiplo de 4)."""
    assert w % 4 == 0 and 0 <= x < 2**w, (x, w)
    return f"0b{x:0{w}b}"


def hexs(x: int, w: int) -> str:
    """Hexadecimal con prefijo 0x, mayusculas, w/4 digitos."""
    assert w % 4 == 0 and 0 <= x < 2**w, (x, w)
    return f"0x{x:0{w // 4}X}"


def chain(x: int, w: int) -> str:
    """Cadena de igualdades binario = decimal = hex."""
    return f"{bits(x, w)} = {x} = {hexs(x, w)}"


def weights_sum(x: int, w: int) -> str:
    """Suma de pesos de los bits a 1, de mayor a menor."""
    ws = [2**k for k in range(w - 1, -1, -1) if (x >> k) & 1]
    return " + ".join(str(v) for v in ws)


OPS = {"AND": lambda a, b: a & b, "OR": lambda a, b: a | b, "XOR": lambda a, b: a ^ b}


def op_eq(a: int, op: str, b: int, w: int, as_hex: bool = False) -> str:
    """Una operacion bit a bit en una linea: 'a OP b = r' (ancho w)."""
    f = hexs if as_hex else bits
    return f"{f(a, w)} {op} {f(b, w)} = {f(OPS[op](a, b), w)}"


def not_eq(a: int, w: int, as_hex: bool = False) -> str:
    f = hexs if as_hex else bits
    return f"NOT {f(a, w)} = {f(a ^ (2**w - 1), w)}"


def shift_eq(a: int, op: str, n: int, w: int, as_hex: bool = False) -> str:
    f = hexs if as_hex else bits
    r = ((a << n) & (2**w - 1)) if op == "<<" else (a >> n)
    return f"{f(a, w)} {op} {n} = {f(r, w)}"


def calc_block_text(a: int, op: str, b: int, w: int) -> str:
    """Operacion en vertical (diagrama ASCII)."""
    r = OPS[op](a, b)
    sep = "-" * len(bits(a, w))
    return "\n".join(
        [f"    {bits(a, w)}", f"{op:>3} {bits(b, w)}", f"    {sep}", f"    {bits(r, w)}"]
    )


# --------------------------------------------------------------------------
# Contexto de registro: todo lo que se escribe queda anotado para el check
# --------------------------------------------------------------------------


class Ctx:
    def __init__(self) -> None:
        self.lines: list[str] = []  # lineas exactas (tablas y sus titulos)
        self.blocks: list[tuple[str, str]] = []  # (tipo, texto) bloques con cercas
        self.claims: list[str] = []  # cadenas literales que deben aparecer
        self.given: list[str] = []  # numeros dados como dato (enunciados)
        self.free: list[str] = []  # literales binarios de ancho libre (una senal de 1 bit, 0b0 y 0b1)
        self.tables: list[tuple[str, list[str], list[list[str]]]] = []

    def claim(self, s: str) -> str:
        self.claims.append(s)
        return s

    def given_tok(self, s: str) -> str:
        self.given.append(s)
        return s

    def free_tok(self, s: str) -> str:
        self.free.append(s)
        return s

    def table(self, tid: str, caption: str, headers: list[str], rows: list[list]) -> str:
        out = [
            f"**Tabla {tid}. {caption}**",
            "",
            "| " + " | ".join(headers) + " |",
            "|" + "|".join(" --- " for _ in headers) + "|",
        ]
        srows = [[str(x) for x in r] for r in rows]
        for r in srows:
            assert len(r) == len(headers), (tid, r)
            out.append("| " + " | ".join(r) + " |")
        self.tables.append((tid, list(headers), srows))
        self.lines.extend(line for line in out if line)
        return "\n".join(out)

    def block(self, kind: str, text: str) -> str:
        self.blocks.append((kind, text))
        return f"```text\n{text}\n```"


# --------------------------------------------------------------------------
# Booleanos
# --------------------------------------------------------------------------


def rows_of(n: int) -> list[tuple[int, ...]]:
    """Filas de una tabla de verdad: la primera variable es el bit mas alto."""
    return list(product((0, 1), repeat=n))


def func_code(fn, n: int) -> int:
    """Codigo de una funcion: el bit k vale la salida en la fila k."""
    return sum(int(fn(*r)) << k for k, r in enumerate(rows_of(n)))


def b_not(a): return 1 - a
def b_and(a, b): return a & b
def b_or(a, b): return a | b
def b_xor(a, b): return a ^ b
def b_nand(a, b): return 1 - (a & b)
def b_nor(a, b): return 1 - (a | b)
def b_xnor(a, b): return 1 - (a ^ b)


GATE_FN = {
    "NOT": b_not, "AND": b_and, "OR": b_or, "XOR": b_xor,
    "NAND": b_nand, "NOR": b_nor, "XNOR": b_xnor,
}


def maj3(a, b, c): return int(a + b + c >= 2)


def truth_table(c: Ctx, tid: str, caption: str, names: list[str], cols: list[tuple[str, object]]) -> str:
    """Tabla de verdad: columnas de entrada + columnas (cabecera, funcion)."""
    rows = []
    for r in rows_of(len(names)):
        rows.append(list(r) + [int(fn(*r)) for _, fn in cols])
    return c.table(tid, caption, list(names) + [h for h, _ in cols], rows)


# --------------------------------------------------------------------------
# Mapas de Karnaugh y Quine-McCluskey
# --------------------------------------------------------------------------


def gray(nbits: int) -> list[int]:
    return [i ^ (i >> 1) for i in range(1 << nbits)]


def kmap_dims(n: int) -> tuple[int, int]:
    return {2: (1, 1), 3: (1, 2), 4: (2, 2)}[n]


def kmap_text(n: int, names: list[str], title: str, cell) -> str:
    """Mapa de Karnaugh en ASCII. cell(minterm) -> str. Ejes en orden Gray."""
    rb, cb = kmap_dims(n)
    rv, cv = "".join(names[:rb]), "".join(names[rb:])
    rlab = [f"{rv}={g:0{rb}b}" for g in gray(rb)]
    labw = max(len(x) for x in rlab)
    ccodes = [f"{g:0{cb}b}" for g in gray(cb)]
    cells = [[cell((rg << cb) | cg) for cg in gray(cb)] for rg in gray(rb)]
    w = max(len(ccodes[0]), max(len(x) for row in cells for x in row))
    out = [f"# {title}"]
    out.append(" " * (labw + 3) + cv)
    out.append(" " * (labw + 3) + " ".join(x.rjust(w) for x in ccodes))
    for lab, row in zip(rlab, cells):
        out.append(lab.ljust(labw) + " | " + " ".join(x.rjust(w) for x in row))
    return "\n".join(out)


def prime_implicants(ons: list[int], n: int) -> list[str]:
    cubes = {format(m, f"0{n}b") for m in ons}
    primes: set[str] = set()
    while cubes:
        used: set[str] = set()
        nxt: set[str] = set()
        cl = sorted(cubes)
        for i, a in enumerate(cl):
            for b in cl[i + 1:]:
                d = [k for k in range(n) if a[k] != b[k]]
                if len(d) == 1 and a[d[0]] != "-" and b[d[0]] != "-":
                    nxt.add(a[: d[0]] + "-" + a[d[0] + 1:])
                    used.add(a)
                    used.add(b)
        primes |= cubes - used
        cubes = nxt
    return sorted(primes)


def cube_minterms(cube: str) -> list[int]:
    res = [0]
    for ch in cube:
        res = [r * 2 + b for r in res for b in ((0, 1) if ch == "-" else (int(ch),))]
    return sorted(res)


def cube_order_key(cube: str) -> tuple[int, ...]:
    """Orden de presentacion: las variables mas a la izquierda primero."""
    return tuple({"1": 0, "0": 1, "-": 2}[ch] for ch in cube)


def cube_literals(cube: str) -> int:
    return sum(ch != "-" for ch in cube)


def cube_term(cube: str, names: list[str]) -> str:
    parts = [(names[i] if ch == "1" else "¬" + names[i]) for i, ch in enumerate(cube) if ch != "-"]
    return "·".join(parts) if parts else "1"


def min_cover(primes: list[str], ons: list[int]) -> list[str]:
    """Cobertura minima: primero menos terminos, luego menos literales."""
    from itertools import combinations

    best = None
    for k in range(1, len(primes) + 1):
        for sub in combinations(primes, k):
            cov = set()
            for cb in sub:
                cov |= set(cube_minterms(cb))
            if cov >= set(ons):
                lits = sum(cube_literals(cb) for cb in sub)
                if best is None or lits < best[0]:
                    best = (lits, sub)
        if best is not None:
            break
    assert best is not None
    return sorted(best[1], key=cube_order_key)


def min_cover_dc(ons: list[int], dcs: list[int], n: int) -> list[str]:
    """Cobertura minima de los unos usando las casillas 'no importa' como comodin.

    Los implicantes primos se buscan sobre unos + X; la cobertura solo exige cubrir los unos.
    """
    return min_cover(prime_implicants(sorted(set(ons) | set(dcs)), n), ons)


def sop_cost(cubes: list[str]) -> dict[str, int]:
    """Coste de una suma de productos con puertas de 2 entradas y NOT compartidos."""
    negs = {i for cb in cubes for i, ch in enumerate(cb) if ch == "0"}
    nots = len(negs)
    ands = sum(max(cube_literals(cb) - 1, 0) for cb in cubes)
    ors = len(cubes) - 1
    return {"NOT": nots, "AND": ands, "OR": ors, "total": nots + ands + ors}


# --------------------------------------------------------------------------
# Busqueda exhaustiva de circuitos minimos (BFS sobre conjuntos de senales)
# --------------------------------------------------------------------------


def min_gate_counts(inputs: list[int], gates: list[tuple[object, int]], mask: int, max_gates: int) -> dict[int, int]:
    """Para cada funcion (codigo) alcanzable con <= max_gates puertas, el minimo.

    gates: lista de (funcion_sobre_enteros, aridad). Una puerta puede reusar
    cualquier senal ya disponible, incluida la misma dos veces.
    """
    best = {f: 0 for f in inputs}
    level = {frozenset(inputs)}
    seen = set(level)
    for d in range(1, max_gates + 1):
        nxt: set[frozenset[int]] = set()
        for s in level:
            lst = sorted(s)
            new: set[int] = set()
            for fn, ar in gates:
                if ar == 1:
                    for x in lst:
                        new.add(fn(x) & mask)
                else:
                    for i, x in enumerate(lst):
                        for y in lst[i:]:
                            new.add(fn(x, y) & mask)
            for z in new - s:
                t = s | {z}
                if t not in seen:
                    seen.add(t)
                    nxt.add(t)
                best.setdefault(z, d)
        level = nxt
    return best


def int_gates(names: list[str], n: int):
    """Puertas sobre codigos enteros de n variables (mascara 2**(2**n)-1)."""
    mask = (1 << (1 << n)) - 1
    table = {
        "AND": (lambda x, y: x & y, 2), "OR": (lambda x, y: x | y, 2),
        "XOR": (lambda x, y: x ^ y, 2), "NAND": (lambda x, y: ~(x & y), 2),
        "NOR": (lambda x, y: ~(x | y), 2), "NOT": (lambda x: ~x, 1),
    }
    return [table[g] for g in names], mask


def var_codes(n: int) -> list[int]:
    """Codigo de cada variable de entrada (la primera es el bit alto de la fila)."""
    return [func_code(lambda *r, i=i: r[i], n) for i in range(n)]


def closure_size(names: list[str], n: int, consts: tuple[int, ...] = ()) -> int:
    """Funciones alcanzables conectando esas puertas (y, si se indica, constantes)."""
    gates, mask = int_gates(names, n)
    s = set(var_codes(n)) | {mask if k else 0 for k in consts}
    changed = True
    while changed:
        changed = False
        lst = sorted(s)
        new = set()
        for fn, ar in gates:
            if ar == 1:
                new |= {fn(x) & mask for x in lst}
            else:
                new |= {fn(x, y) & mask for x in lst for y in lst}
        if not new <= s:
            s |= new
            changed = True
    return len(s)


# --------------------------------------------------------------------------
# Netlists (circuitos en texto)
# --------------------------------------------------------------------------

TARGETS = {
    "NOT": (1, lambda a: 1 - a), "AND": (2, b_and), "OR": (2, b_or), "XOR": (2, b_xor),
    "NAND": (2, b_nand), "NOR": (2, b_nor), "XNOR": (2, b_xnor), "MAJ": (3, maj3),
}


class Net:
    def __init__(self, target: str, base: str, inputs: list[str], gates: list[tuple[str, str, list[str]]]):
        self.target, self.base, self.inputs, self.gates = target, base, inputs, gates

    def n_gates(self) -> int:
        return len(self.gates)

    def value(self, env: dict[str, int]) -> int:
        """Salida para una asignacion concreta de las entradas."""
        env = dict(env)
        for name, g, args in self.gates:
            env[name] = GATE_FN[g](*[env[a] for a in args])
        return env[self.gates[-1][0]]

    def evaluate(self) -> int:
        """Codigo de la salida (la ultima puerta) sobre todas las filas."""
        code = 0
        for k, r in enumerate(rows_of(len(self.inputs))):
            env = dict(zip(self.inputs, r))
            for name, g, args in self.gates:
                env[name] = GATE_FN[g](*[env[a] for a in args])
            code |= env[self.gates[-1][0]] << k
        return code

    def depth(self) -> int:
        d = {i: 0 for i in self.inputs}
        for name, _, args in self.gates:
            d[name] = 1 + max(d[a] for a in args)
        return d[self.gates[-1][0]]

    def text(self) -> str:
        head = (
            f"# objetivo: {self.target} | base: {self.base} | puertas: {self.n_gates()}"
            f" | entradas: {', '.join(self.inputs)}"
        )
        body = [f"{n} = {g}({', '.join(a)})" for n, g, a in self.gates]
        return "\n".join([head] + body)

    def verify(self) -> None:
        k, fn = TARGETS[self.target]
        assert k == len(self.inputs)
        assert self.evaluate() == func_code(fn, k), self.text()


def net_signals(net: Net) -> dict[str, list[int]]:
    """Valor de cada senal en cada fila (para tablas de traza)."""
    out: dict[str, list[int]] = {n: [] for n, _, _ in net.gates}
    for r in rows_of(len(net.inputs)):
        env = dict(zip(net.inputs, r))
        for name, g, args in net.gates:
            env[name] = GATE_FN[g](*[env[a] for a in args])
            out[name].append(env[name])
    return out
