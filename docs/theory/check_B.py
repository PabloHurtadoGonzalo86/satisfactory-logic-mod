#!/usr/bin/env python3
"""check_B.py  --  clave B: documentos 04 (memoria/biestables) y 05 (aritmetica binaria).

Contiene:
  1. El simulador didactico (retardo unitario, evaluacion sincrona en dos fases).
  2. Los circuitos y todas las funciones que GENERAN tablas, trazas y formas de onda.
  3. Un verificador: regenera cada bloque y comprueba con assert que aparece
     literalmente en el .md correspondiente.

Uso:
  python3 check_B.py            verifica los .md (exit 0 si todo coincide)
  python3 check_B.py --dump 04  imprime los bloques del documento 04 (para pegarlos)
  python3 check_B.py --dump 05  imprime los bloques del documento 05
  python3 check_B.py --fill 04  sustituye los marcadores {{bloque}} del .md por el bloque generado (una sola vez)
"""
import sys

sys.dont_write_bytecode = True

import itertools
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
DOC04 = HERE / "04-memoria-biestables.md"
DOC05 = HERE / "05-aritmetica-binaria.md"

# ---------------------------------------------------------------------------
# 1. Simulador: retardo unitario + evaluacion sincrona en dos fases
# ---------------------------------------------------------------------------

OPS = {
    "NOT": lambda a: 1 - a,
    "AND": lambda *a: int(all(a)),
    "OR": lambda *a: int(any(a)),
    "XOR": lambda *a: sum(a) % 2,
    "NAND": lambda *a: 1 - int(all(a)),
    "NOR": lambda *a: 1 - int(any(a)),
    "XNOR": lambda *a: 1 - (sum(a) % 2),
}


class Circuit:
    """Puertas con retardo unitario. Estado = salida de cada puerta en el instante t."""

    def __init__(self, name, inputs, gates):
        self.name = name
        self.inputs = list(inputs)
        self.gates = [(n, op, tuple(srcs)) for n, op, srcs in gates]
        self.names = [g[0] for g in self.gates]
        assert len(set(self.names + self.inputs)) == len(self.names) + len(self.inputs), name
        self.in_idx = {n: i for i, n in enumerate(self.inputs)}
        self.g_idx = {n: i for i, n in enumerate(self.names)}
        self._plan = []
        for n, op, srcs in self.gates:
            assert op in OPS, op
            refs = []
            for s in srcs:
                if s in self.in_idx:
                    refs.append((True, self.in_idx[s]))
                else:
                    assert s in self.g_idx, (name, n, s)
                    refs.append((False, self.g_idx[s]))
            self._plan.append((OPS[op], refs))
        self._fast = self._compile()

    def _compile(self):
        """Version rapida de la fase 1 (expresiones Python generadas de la lista de puertas)."""
        py = {"AND": " & ", "OR": " | ", "XOR": " ^ ", "NAND": " & ", "NOR": " | ", "XNOR": " ^ "}
        exprs = []
        for (_, refs), (_, op, _) in zip(self._plan, self.gates):
            args = [(f"x[{i}]" if is_in else f"st[{i}]") for is_in, i in refs]
            if op == "NOT":
                e = f"(1 - {args[0]})"
            elif op in ("AND", "OR", "XOR"):
                e = "(" + py[op].join(args) + ")"
            else:
                e = "(1 - (" + py[op].join(args) + "))"
            exprs.append(e)
        return eval("lambda st, x: (" + ", ".join(exprs) + ",)")

    def self_check(self, rng, trials=200):
        """La version rapida y la lenta (la de la explicacion) dan siempre lo mismo."""
        for _ in range(trials):
            st = tuple(rng.randint(0, 1) for _ in self.names)
            x = tuple(rng.randint(0, 1) for _ in self.inputs)
            assert self.phase1_compute(st, x) == self._fast(st, x), self.name

    def state(self, **kw):
        """Estado inicial: todo a 0 salvo lo indicado."""
        s = [0] * len(self.names)
        for k, v in kw.items():
            s[self.g_idx[k]] = v
        return tuple(s)

    def x(self, **kw):
        v = [0] * len(self.inputs)
        for k, val in kw.items():
            v[self.in_idx[k]] = val
        return tuple(v)

    def phase1_compute(self, state, x):
        """Fase 1: cada puerta calcula su salida de t+1 leyendo SOLO el instante t."""
        nxt = []
        for fn, refs in self._plan:
            nxt.append(fn(*[(x[i] if is_in else state[i]) for is_in, i in refs]))
        return tuple(nxt)

    def step(self, state, x):
        """Fase 1 (calcular todo) + fase 2 (confirmar todo a la vez)."""
        nxt = self._fast(state, x)  # fase 1 (version rapida, equivalente a phase1_compute)
        return nxt  # fase 2: el estado de t+1 pasa a ser `nxt` de golpe

    def run(self, state, xs):
        """Devuelve la lista de estados s(0), s(1), ..., s(len(xs)); xs[t] = entradas en t."""
        states = [state]
        for x in xs:
            states.append(self.step(states[-1], x))
        return states

    def val(self, state, name):
        return state[self.g_idx[name]]

    def settle(self, state, x, limit=500):
        """Mantiene x constante. Devuelve (estado_final, k, periodo).
        k = paso en el que cambia por ultima vez (0 si ya estaba quieto).
        periodo = None si converge; si entra en ciclo, su longitud."""
        seen = {state: 0}
        hist = [state]
        for t in range(1, limit + 1):
            state = self.step(state, x)
            if state == hist[-1]:
                # punto fijo: ultimo cambio en el paso anterior a la primera repeticion
                k = t - 1
                while k > 0 and hist[k] == hist[k - 1]:
                    k -= 1
                return state, k, None
            if state in seen:
                return state, None, t - seen[state]
            seen[state] = t
            hist.append(state)
        raise AssertionError("no converge ni cicla")

    def equations(self, names=None):
        out = []
        for n, op, srcs in self.gates:
            if names is not None and n not in names:
                continue
            args = ", ".join(f"{s}(t)" for s in srcs)
            out.append(f"{n}(t+1) = {op}({args})")
        return out




# ---------------------------------------------------------------------------
# 2. Formato: tablas Markdown, trazas, formas de onda ASCII
# ---------------------------------------------------------------------------

def md_table(headers, rows):
    lines = ["| " + " | ".join(headers) + " |", "|" + "|".join("---" for _ in headers) + "|"]
    for r in rows:
        lines.append("| " + " | ".join(str(c) for c in r) + " |")
    return "\n".join(lines)


def trace_table(circ, s0, xs, in_cols, out_cols):
    """Tabla vertical: la fila t muestra x(t) y s(t). len(xs) = numero de filas."""
    states = [s0]
    for t in range(1, len(xs)):
        states.append(circ.step(states[-1], xs[t - 1]))
    rows = []
    for t, (s, x) in enumerate(zip(states, xs)):
        rows.append([t] + [x[circ.in_idx[n]] for n in in_cols] + [circ.val(s, n) for n in out_cols])
    return md_table(["t"] + [f"{n}(t)" for n in in_cols + out_cols], rows), states


def htrace(circ, s0, xs, in_cols, out_cols):
    """Traza horizontal compacta: una fila por senal, una columna por instante."""
    states = [s0]
    for t in range(1, len(xs)):
        states.append(circ.step(states[-1], xs[t - 1]))
    lab = max(len(n) for n in in_cols + out_cols + ["t"])
    lines = ["t".ljust(lab) + "  " + " ".join(str(t) for t in range(len(xs)))]
    for n in in_cols:
        lines.append(n.ljust(lab) + "  " + " ".join(str(x[circ.in_idx[n]]) for x in xs))
    for n in out_cols:
        lines.append(n.ljust(lab) + "  " + " ".join(str(circ.val(s, n)) for s in states))
    return "\n".join(l.rstrip() for l in lines), states


def wave(sigs, n, edge_row=None):
    """Forma de onda ASCII: '_' = 0, '#' = 1; una columna = un paso. sigs = [(etiqueta, [bits])]."""
    lw = max(len(l) for l, _ in sigs) + 2
    lw = max(lw, 7)
    tens = "".join(str((t // 10) % 10) if t % 10 == 0 else " " for t in range(n))
    units = "".join(str(t % 10) for t in range(n))
    lines = ["t".ljust(lw) + tens, " " * lw + units]
    for label, vals in sigs:
        lines.append(label.ljust(lw) + "".join("#" if v else "_" for v in vals[:n]))
        if edge_row is not None and label == edge_row[0]:
            sig = vals
            marks = "".join("^" if (t > 0 and sig[t] == 1 and sig[t - 1] == 0) else " " for t in range(n))
            lines.append(edge_row[1].ljust(lw) + marks)
    return "\n".join(l.rstrip() for l in lines)


def explain_step(circ, state, x, t):
    """Una transicion t -> t+1 con las dos fases escritas. Devuelve (lineas, estado_siguiente)."""
    nxt = circ.phase1_compute(state, x)
    w = max(len(f"{n}({t + 1})") for n in circ.names)
    lines = [f"t={t} -> t={t + 1}", f"  fase 1 (calcular, leyendo solo el instante t={t}):"]
    for (n, op, srcs), nv in zip(circ.gates, nxt):
        vals = [x[circ.in_idx[s]] if s in circ.in_idx else state[circ.g_idx[s]] for s in srcs]
        lhs = f"{n}({t + 1})".ljust(w)
        sym = f"{op}(" + ", ".join(f"{s}({t})" for s in srcs) + ")"
        num = f"{op}(" + ", ".join(str(v) for v in vals) + ")"
        lines.append(f"    {lhs} = {sym} = {num} = {nv}")
    lines.append("  fase 2 (confirmar todo a la vez): " + ", ".join(f"{n}({t + 1}) = {v}" for n, v in zip(circ.names, nxt)))
    return lines, nxt


def trace_wide(circ, states, xs, in_cols, out_cols, t0, t1):
    """Bloque de texto con columnas alineadas, filas t0..t1 (ambas incluidas)."""
    cols = in_cols + out_cols
    widths = [max(len(c), 1) for c in cols]
    head = " t  " + " ".join(c.rjust(w) for c, w in zip(cols, widths))
    if in_cols:
        k = len(in_cols)
        head = " t  " + " ".join(c.rjust(w) for c, w in zip(in_cols, widths[:k])) + " | " + " ".join(
            c.rjust(w) for c, w in zip(out_cols, widths[k:]))
    lines = [head]
    for t in range(t0, t1 + 1):
        iv = [str(xs[t][circ.in_idx[n]]).rjust(w) for n, w in zip(in_cols, widths)]
        ov = [str(circ.val(states[t], n)).rjust(w) for n, w in zip(out_cols, widths[len(in_cols):])]
        lines.append(f"{t:>2}  " + " ".join(iv) + (" | " if in_cols else "") + " ".join(ov))
    return "\n".join(l.rstrip() for l in lines)


def levels(events, n, init=0):
    """Senal por tramos: events = [(t, valor), ...] ordenados."""
    out, cur, k = [], init, 0
    for t in range(n):
        while k < len(events) and events[k][0] <= t:
            cur = events[k][1]
            k += 1
        out.append(cur)
    return out


def clock(n, half):
    """Reloj: bajo durante `half` pasos, alto durante `half` pasos, y asi sucesivamente."""
    return [1 if (t % (2 * half)) >= half else 0 for t in range(n)]


# ---------------------------------------------------------------------------
# 3. Circuitos del documento 04
# ---------------------------------------------------------------------------

RING = Circuit("anillo de dos NOT", [], [("A", "NOT", ["B"]), ("B", "NOT", ["A"])])


def ring_circuit(n):
    """Anillo de n puertas NOT: cada una alimenta a la siguiente y la ultima a la primera."""
    return Circuit(f"anillo de {n} NOT", [], [(f"N{i}", "NOT", [f"N{(i - 1) % n}"]) for i in range(n)])


def ring_parity_measure(sizes):
    """Por cada n: lista de estados estables (puntos fijos: estados que no cambian al avanzar un paso).
    Comprueba ademas que todo estado que no es estable acaba oscilando (nunca se asienta)."""
    out = {}
    for n in sizes:
        c = ring_circuit(n)
        fixed = []
        for init in itertools.product((0, 1), repeat=n):
            f, k, per = c.settle(init, ())
            if c.step(init, ()) == init:
                assert per is None
                fixed.append(init)
            else:
                assert per is not None, (n, init)  # oscila: nunca llega a un estado quieto
        out[n] = fixed
    return out


SR_NOR = Circuit("latch SR con NOR", ["S", "R"], [("Q", "NOR", ["R", "Qn"]), ("Qn", "NOR", ["S", "Q"])])
SR_NAND = Circuit("latch SR con NAND", ["Sn", "Rn"], [("Q", "NAND", ["Sn", "Qn"]), ("Qn", "NAND", ["Rn", "Q"])])
D_LATCH = Circuit(
    "latch D con habilitacion",
    ["D", "E"],
    [
        ("nD", "NOT", ["D"]),
        ("a", "NAND", ["D", "E"]),
        ("b", "NAND", ["nD", "E"]),
        ("Q", "NAND", ["a", "Qn"]),
        ("Qn", "NAND", ["b", "Q"]),
    ],
)
_FF_CORE = [
    ("nCLK", "NOT", ["CLK"]),
    ("nD", "NOT", ["D"]),
    ("a1", "NAND", ["D", "nCLK"]),
    ("b1", "NAND", ["nD", "nCLK"]),
    ("M", "NAND", ["a1", "Mn"]),
    ("Mn", "NAND", ["b1", "M"]),
    ("a2", "NAND", ["M", "CLK"]),
    ("b2", "NAND", ["Mn", "CLK"]),
    ("Q", "NAND", ["a2", "Qn"]),
    ("Qn", "NAND", ["b2", "Q"]),
]
DFF = Circuit("flip-flop D maestro-esclavo", ["D", "CLK"], _FF_CORE)
TFF = Circuit("flip-flop T", ["T", "CLK"], [("D", "XOR", ["T", "Q"])] + _FF_CORE)
FF_GATES = [g[0] for g in _FF_CORE]


def settled(circ, state, x):
    """Estado quieto al mantener x. Falla si el circuito oscila (no se usa para arrancar desde estados raros)."""
    f, k, per = circ.settle(state, x)
    assert per is None, (circ.name, "oscila", per)
    return f


FF_LOW = 6   # pasos con CLK=0 en la prueba de arranque
FF_HIGH = 6  # pasos con CLK=1 en la prueba de arranque
HALF_SWEEP = range(1, 11)  # semiperiodos probados para el T-FF


def powerup_dff(d):
    """Estado consistente de DFF con Q = d: un ciclo de reloj completo con D fija (ver prueba de las 1024 condiciones)."""
    xs = [DFF.x(D=d, CLK=0)] * FF_LOW + [DFF.x(D=d, CLK=1)] * FF_HIGH + [DFF.x(D=d, CLK=0)] * FF_LOW
    s = DFF.run(DFF.state(), xs)[-1]
    assert DFF.step(s, DFF.x(D=d, CLK=0)) == s
    assert DFF.val(s, "Q") == d and DFF.val(s, "Qn") == 1 - d
    return s


def powerup_tff(q):
    """Estado consistente de TFF con Q = q y T = 0, CLK = 0 (se aprovecha el del DFF y se anade la puerta D)."""
    sd = powerup_dff(q)
    kw = {n: DFF.val(sd, n) for n in DFF.names}
    kw["D"] = q
    s = TFF.state(**kw)
    assert TFF.step(s, TFF.x(T=0, CLK=0)) == s
    return s


def valid_change_times(edges, lo, hi, unstable_offsets):
    """Instantes t en [lo, hi) donde D puede empezar a valer un valor nuevo sin caer en la ventana prohibida."""
    out = []
    for t in range(lo, hi):
        if all((t - e) not in unstable_offsets for e in edges):
            out.append(t)
    return out


def dff_trial(db, da, off, half=10):
    """D vale db hasta e+off-1 y da desde e+off (e = primer instante con CLK=1). Devuelve
    ('nuevo'|'viejo'|'INESTABLE', estado_final)."""
    n = 3 * half
    clk = clock(n, half)
    e = half
    xs = [DFF.x(D=(db if t < e + off else da), CLK=clk[t]) for t in range(n)]
    states = DFF.run(powerup_dff(db), xs[:-1])
    s_end = states[-1]
    f, k, per = DFF.settle(s_end, DFF.x(D=da, CLK=0))
    if per is not None:
        return "INESTABLE", s_end
    q = DFF.val(f, "Q")
    assert DFF.val(f, "Qn") == 1 - q
    return ("nuevo" if q == da else "viejo"), f


def measure_dff_window():
    """Desplazamientos (respecto al flanco) en los que un cambio de D da resultado INESTABLE."""
    unstable = set()
    table = {}
    for off in range(-6, 7):
        for db, da in ((0, 1), (1, 0)):
            r, _ = dff_trial(db, da, off)
            table[(off, db, da)] = r
            if r == "INESTABLE":
                unstable.add(off)
    return unstable, table


def random_sequences(rng, n, edges, unstable, count, min_gap, signal_lo=1):
    """Genera `count` listas de instantes de cambio (valor que alterna) separados >= min_gap,
    todos fuera de la ventana prohibida de cada flanco."""
    ok = valid_change_times(edges, signal_lo, n - 1, unstable)
    seqs = []
    for _ in range(count):
        k = rng.randint(1, 8)
        chosen = sorted(rng.sample(ok, min(k, len(ok))))
        keep = []
        for t in chosen:
            if not keep or t - keep[-1] >= min_gap:
                keep.append(t)
        seqs.append(keep)
    return seqs


def level_from_changes(changes, n, init=0):
    out, cur, k = [], init, 0
    for t in range(n):
        while k < len(changes) and changes[k] <= t:
            cur = 1 - cur
            k += 1
        out.append(cur)
    return out


def _note_q_offsets(stats, circ, states, e, end):
    """Anota en `stats` el menor y el mayor numero de pasos tras el flanco e en que Q cambia (hasta el flanco siguiente)."""
    if stats is None:
        return
    for t in range(e + 1, end + 1):
        if circ.val(states[t], "Q") != circ.val(states[t - 1], "Q"):
            stats["qlo"] = min(stats.get("qlo", t - e), t - e)
            stats["qhi"] = max(stats.get("qhi", t - e), t - e)


def dff_random_test(unstable, count=400, half=6, cycles=8, min_gap=3, seed=2024, stats=None):
    import random

    rng = random.Random(seed)
    n = cycles * 2 * half
    clk = clock(n, half)
    edges = [t for t in range(1, n) if clk[t] == 1 and clk[t - 1] == 0]
    worst = 0
    for changes in random_sequences(rng, n, edges, unstable, count, min_gap):
        d = level_from_changes(changes, n)
        xs = [DFF.x(D=d[t], CLK=clk[t]) for t in range(n)]
        states = DFF.run(powerup_dff(d[0]), xs[:-1])
        for i, e in enumerate(edges):
            end = (edges[i + 1] - 1) if i + 1 < len(edges) else n - 1
            q_end, qn_end = DFF.val(states[end], "Q"), DFF.val(states[end], "Qn")
            assert q_end == d[e] and qn_end == 1 - d[e], (changes, e)
            _note_q_offsets(stats, DFF, states, e, end)
            for t in range(e + 1, end + 1):
                if (DFF.val(states[t], "Q"), DFF.val(states[t], "Qn")) != (DFF.val(states[t - 1], "Q"), DFF.val(states[t - 1], "Qn")):
                    worst = max(worst, t - e)
    return count, worst


TFF_RANDOM_COUNT = 400


def tff_test(unstable, half, random_cases, count=TFF_RANDOM_COUNT, cycles=10, min_gap=1, seed=77, stats=None):
    """True si el T-FF hace toggle correcto en todos los flancos con un reloj simetrico de ese semiperiodo
    (`half` pasos con CLK=0 y `half` con CLK=1; periodo = 2 * half).
    Siempre prueba T=1 fija (el divisor de frecuencia). Con random_cases=True anade `count` secuencias aleatorias de T
    cuyos cambios caen fuera de la ventana prohibida (los instantes e-2 y e-1 de cada flanco e).
    Regla comprobada: en cada flanco e, Q <- Q xor T(e-1), y Q se lee un paso antes del flanco siguiente
    (2*half - 1 pasos despues de e: un ciclo de reloj entero para asentarse). Se simulan `half` pasos mas para que el ultimo
    flanco tambien tenga su ciclo entero: si no, se juzgaria su Q antes de que haya terminado de moverse."""
    import random

    rng = random.Random(seed)
    n = (2 * cycles + 1) * half
    clk = clock(n, half)
    edges = [t for t in range(1, n) if clk[t] == 1 and clk[t - 1] == 0]
    assert len(edges) == cycles and edges[-1] + 2 * half - 1 == n - 1
    cases = [[1] * n]
    if random_cases:
        for changes in random_sequences(rng, n, edges, unstable, count, min_gap):
            cases.append(level_from_changes(changes, n))
        assert any(len(set(c)) > 1 for c in cases) or half < 3  # evita pasar "en vacio"
    for t_sig in cases:
        xs = [TFF.x(T=t_sig[t], CLK=clk[t]) for t in range(n)]
        states = TFF.run(powerup_tff(0), xs[:-1])
        q_prev = 0
        for e in edges:
            end = e + 2 * half - 1
            want = q_prev ^ t_sig[e - 1]
            q_end, qn_end = TFF.val(states[end], "Q"), TFF.val(states[end], "Qn")
            if not (q_end == want and qn_end == 1 - want):
                return False
            _note_q_offsets(stats, TFF, states, e, end)
            q_prev = want
    return True


_TFF_LIMITS = {}


def tff_clock_limits():
    """Mediciones del T-FF de puertas que comparten el documento 04 y (importadas por check_C.py) el 06.
    min_fixed / min_any: menor semiperiodo de un reloj simetrico con el que acierta T=1 fija / T=1 fija mas T aleatoria
    (todos los semiperiodos mayores, hasta el final del barrido, tambien aciertan).
    q_fixed / q_any: (menor, mayor) numero de pasos tras el flanco en que Q cambia, con T=1 fija / con T aleatoria."""
    if not _TFF_LIMITS:
        unstable, table = measure_tff_window()
        assert unstable == {-2, -1}
        ok_fixed = [h for h in HALF_SWEEP if tff_test(unstable, h, False)]
        ok_any = [h for h in HALF_SWEEP if tff_test(unstable, h, True)]
        min_fixed, min_any = ok_fixed[0], ok_any[0]
        assert ok_fixed == list(range(min_fixed, HALF_SWEEP.stop)) and ok_any == list(range(min_any, HALF_SWEEP.stop))
        assert min_fixed < min_any
        half = 6  # el semiperiodo de los diagramas
        st_fixed, st_any = {}, {}
        assert tff_test(unstable, half, False, stats=st_fixed)
        assert tff_test(unstable, half, True, stats=st_any)  # mismo reloj que los diagramas, con las mismas secuencias aleatorias
        _TFF_LIMITS.update(
            unstable=unstable,
            table=table,
            min_fixed=min_fixed,
            min_any=min_any,
            fails_fixed=[h for h in HALF_SWEEP if h < min_fixed],
            fails_any=[h for h in HALF_SWEEP if h < min_any],
            q_fixed=(st_fixed["qlo"], st_fixed["qhi"]),
            q_any=(st_any["qlo"], st_any["qhi"]),
            stats=st_any,
        )
    return _TFF_LIMITS


def tff_trial(tb, ta, off, half=10):
    """T vale tb hasta e+off-1 y ta desde e+off; Q parte de 0. Resultado segun lo que hizo Q en ese flanco."""
    n = 3 * half
    clk = clock(n, half)
    e = half
    xs = [TFF.x(T=(tb if t < e + off else ta), CLK=clk[t]) for t in range(n)]
    states = TFF.run(powerup_tff(0), xs[:-1])
    s_end = states[-1]
    f, k, per = TFF.settle(s_end, TFF.x(T=ta, CLK=0))
    if per is not None:
        return "INESTABLE"
    q, qn = TFF.val(f, "Q"), TFF.val(f, "Qn")
    if q == qn:
        return "INESTABLE"
    if q == ta:  # Q parte de 0, asi que Q = 0 xor T(e) = T(e)
        return "nuevo"
    return "viejo"


def measure_tff_window():
    unstable, table = set(), {}
    for off in range(-6, 7):
        for tb, ta in ((0, 1), (1, 0)):
            r = tff_trial(tb, ta, off)
            table[(off, tb, ta)] = r
            if r == "INESTABLE":
                unstable.add(off)
    return unstable, table


# ---------------------------------------------------------------------------
# 4. Bloques generados del documento 04
# ---------------------------------------------------------------------------

def run_states(circ, s0, xs):
    """Estados s(0..len(xs)-1) con xs[t] = entradas en t."""
    states = [s0]
    for t in range(1, len(xs)):
        states.append(circ.step(states[-1], xs[t - 1]))
    return states


def join_t(ts):
    return ", ".join(str(t) for t in ts)


def join_y(items):
    """Lista en castellano: 'a', 'a y b', 'a, b y c'."""
    items = [str(i) for i in items]
    return items[0] if len(items) == 1 else ", ".join(items[:-1]) + " y " + items[-1]




def build04():
    B = {}

    # ---------------- anillo de dos NOT: memoria minima y modelo temporal ----------------
    B["ring_eq"] = "\n".join(RING.equations())
    rows, info = [], {}
    for a, b in itertools.product((0, 1), repeat=2):
        s0 = RING.state(A=a, B=b)
        states = RING.run(s0, [()] * 4)
        cells = [f"{RING.val(s, 'A')}{RING.val(s, 'B')}" for s in states]
        _, _, per = RING.settle(s0, ())
        info[(a, b)] = per
        rows.append([f"{a}{b}"] + cells + ["estable" if per is None else f"oscila (periodo {per})"])
    assert info == {(0, 0): 2, (0, 1): None, (1, 0): None, (1, 1): 2}
    B["ring_table"] = md_table(["AB en t=0", "t=0", "t=1", "t=2", "t=3", "t=4", "veredicto"], rows)

    s0 = RING.state(A=0, B=0)
    l1, s1 = explain_step(RING, s0, (), 0)
    l2, s2 = explain_step(RING, s1, (), 1)
    B["ring_explain"] = "\n".join(l1 + l2)
    a_new = 1 - RING.val(s0, "B")
    b_bad = 1 - a_new
    assert (a_new, b_bad) == (1, 0) and s1 == (1, 1)
    B["f_ring_seq"] = (
        f"Contraejemplo: si evaluaras las puertas una detrás de otra, usando el A que acabas de calcular, "
        f"desde AB=00 saldría AB={a_new}{b_bad} en t=1; con las dos fases sale AB={RING.val(s1, 'A')}{RING.val(s1, 'B')}."
    )
    # paridad del bucle: memoria = realimentacion positiva (numero par de inversiones)
    even_sizes, odd_sizes = (2, 4, 6), (1, 3, 5)
    parity = ring_parity_measure(even_sizes + odd_sizes)
    for n in even_sizes:  # los unicos estados estables son los dos patrones alternados 0101... y 1010...
        alt = tuple(i % 2 for i in range(n))
        assert sorted(parity[n]) == sorted([alt, tuple(1 - v for v in alt)]), (n, parity[n])
    assert all(parity[n] == [] for n in odd_sizes)
    assert len(parity[2]) == sum(1 for v in info.values() if v is None)  # coincide con la Tabla 4.1
    B["f_ring_parity"] = (
        f"Comprobado con el simulador: un anillo de {join_y(even_sizes)} puertas NOT tiene {len(parity[even_sizes[0]])} estados estables "
        f"(los dos patrones alternados); uno de {join_y(odd_sizes)} puertas NOT no tiene ninguno y todos sus estados oscilan."
    )

    # ---------------- latch SR con NOR ----------------
    C = SR_NOR
    B["srnor_eq"] = "\n".join(C.equations())
    names = {(0, 0): "mantener", (0, 1): "reset", (1, 0): "set", (1, 1): "PROHIBIDO"}
    rows = []
    res = {}
    sr_steps = []
    for (S, R) in [(0, 0), (0, 1), (1, 0), (1, 1)]:
        for q0 in (0, 1):
            f, k, per = C.settle(C.state(Q=q0, Qn=1 - q0), C.x(S=S, R=R))
            assert per is None
            q, qn = C.val(f, "Q"), C.val(f, "Qn")
            exp = {(0, 0): (q0, 1 - q0), (0, 1): (0, 1), (1, 0): (1, 0), (1, 1): (0, 0)}[(S, R)]
            assert (q, qn) == exp
            res[(S, R, q0)] = (q, qn)
            sr_steps.append(k)
            rows.append([S, R, q0, q, qn, k, names[(S, R)]])
    B["srnor_behaviour"] = md_table(
        ["S", "R", "Q antes", "Q después", "Qn después", "pasos hasta quedar quieto", "modo"], rows
    )

    # traza principal: pulso de set (t=1..2) y pulso de reset (t=6..7)
    xs = [C.x(S=1 if t in (1, 2) else 0, R=1 if t in (6, 7) else 0) for t in range(12)]
    s0 = C.state(Q=0, Qn=1)
    tab, states = trace_table(C, s0, xs, ["S", "R"], ["Q", "Qn"])
    B["srnor_trace"] = tab
    qs = [C.val(s, "Q") for s in states]
    qns = [C.val(s, "Qn") for s in states]
    t_qn_fall = next(t for t in range(1, 12) if qns[t] != qns[t - 1])
    t_q_rise = next(t for t in range(1, 12) if qs[t] != qs[t - 1])
    t_q_fall = next(t for t in range(t_q_rise + 1, 12) if qs[t] != qs[t - 1])
    t_qn_rise = next(t for t in range(t_q_fall - 1, 12) if qns[t] != qns[t - 1] and t >= t_q_fall)
    assert (qs[-1], qns[-1]) == (0, 1) and (qs[5], qns[5]) == (1, 0)
    idle = [t for t in range(3, 12) if xs[t] == C.x(S=0, R=0)]
    idle = [t for t in idle if t < 6]
    assert idle == [3, 4, 5] and all((qs[t], qns[t]) == (qs[5], qns[5]) for t in idle)
    B["f_srnor_trace"] = (
        f"Con el pulso de set en t=1 y t=2: Qn baja primero (en t={t_qn_fall}) y Q sube un paso después (en t={t_q_rise}); "
        f"con el pulso de reset en t=6 y t=7: Q baja en t={t_q_fall} y Qn sube en t={t_qn_rise}. "
        f"Entre los dos pulsos (con S=R=0 en t={idle[0]} a t={idle[-1]}) el latch mantiene (Q, Qn) = ({qs[5]}, {qns[5]})."
    )
    e1, st1 = explain_step(C, states[1], xs[1], 1)
    e2, st2 = explain_step(C, st1, xs[2], 2)
    assert st1 == states[2] and st2 == states[3]
    B["srnor_explain"] = "\n".join(e1 + e2)

    # ancho minimo del pulso
    rows = []
    out = {}
    for w in (1, 2, 3):
        outs = []
        for kind in ("S", "R"):
            q0 = 0 if kind == "S" else 1
            xs_p = [C.x(**{kind: 1 if 1 <= t < 1 + w else 0}) for t in range(14)]
            stp = run_states(C, C.state(Q=q0, Qn=1 - q0), xs_p)
            f, k, per = C.settle(stp[-1], C.x(S=0, R=0))
            if per is not None:
                outs.append(f"oscila (periodo {per})")
            else:
                outs.append(f"queda en Q={C.val(f, 'Q')}, Qn={C.val(f, 'Qn')}")
            out[(w, kind)] = outs[-1]
        rows.append([w] + outs)
    assert out[(1, "S")].startswith("oscila") and out[(1, "R")].startswith("oscila")
    assert out[(2, "S")] == "queda en Q=1, Qn=0" and out[(2, "R")] == "queda en Q=0, Qn=1"
    assert out[(3, "S")] == out[(2, "S")] and out[(3, "R")] == out[(2, "R")]
    B["srnor_pulse"] = md_table(["ancho del pulso (pasos)", "pulso en S, partiendo de Q=0", "pulso en R, partiendo de Q=1"], rows)
    xs1 = [C.x(S=1 if t == 1 else 0, R=0) for t in range(8)]
    txt1, st1 = htrace(C, C.state(Q=0, Qn=1), xs1, ["S", "R"], ["Q", "Qn"])
    B["srnor_pulse1_trace"] = txt1
    f1_, k1_, p1_ = C.settle(st1[-1], C.x(S=0, R=0))
    assert p1_ == 2 and (C.val(st1[2], "Q"), C.val(st1[2], "Qn")) == (0, 0) and (C.val(st1[3], "Q"), C.val(st1[3], "Qn")) == (1, 1)
    B["f_srnor_pulse1"] = (
        f"Con S=1 solo en t=1: Qn baja a 0 en t=2, pero en t=2 S ya vale 0 y nada mantiene a Qn abajo. En t=3 Q sube (porque Qn valía 0) "
        f"y Qn también sube (porque Q valía 0): el latch está en Q=Qn=1 y desde ahí entra en la misma oscilación de periodo {p1_} del estado prohibido."
    )

    # estado prohibido y su salida
    def forb(rel_s, rel_r):
        xs_f = []
        for t in range(10):
            xs_f.append(C.x(S=1 if t < rel_s else 0, R=1 if t < rel_r else 0))
        return htrace(C, C.state(Q=0, Qn=1), xs_f, ["S", "R"], ["Q", "Qn"])

    ta, sta = forb(3, 3)
    tb, stb = forb(3, 6)
    tc, stc = forb(6, 3)
    B["srnor_forb_both"] = ta
    B["srnor_forb_s_first"] = tb  # S se suelta antes (t=3), R sigue hasta t=5
    B["srnor_forb_r_first"] = tc
    assert (C.val(sta[1], "Q"), C.val(sta[1], "Qn")) == (0, 0)
    _, _, per_a = C.settle(sta[3], C.x(S=0, R=0))
    assert per_a == 2
    seq_a = [f"{C.val(s, 'Q')}{C.val(s, 'Qn')}" for s in sta[3:9]]
    assert seq_a == ["00", "11", "00", "11", "00", "11"]
    fb, kb, pb = C.settle(stb[-1], C.x(S=0, R=0))
    fc, kc, pc = C.settle(stc[-1], C.x(S=0, R=0))
    assert pb is None and pc is None
    assert (C.val(fb, "Q"), C.val(fb, "Qn")) == (0, 1) and (C.val(fc, "Q"), C.val(fc, "Qn")) == (1, 0)
    B["f_srnor_forb"] = (
        f"Con S=R=1 el latch llega en un solo paso a Q=Qn=0 (t=1 en la primera traza). Si sueltas S y R en el mismo instante (t=3), "
        f"el estado pasa por {' -> '.join(seq_a[:4])} -> ... con periodo {per_a} y no se estabiliza nunca "
        f"(el estado se repite exactamente cada {per_a} pasos y las entradas ya no cambian)."
    )
    B["f_srnor_forb_order"] = (
        f"Si sueltas S antes que R, el latch acaba en Q={C.val(fb, 'Q')}, Qn={C.val(fb, 'Qn')} (gana R). "
        f"Si sueltas R antes que S, acaba en Q={C.val(fc, 'Q')}, Qn={C.val(fc, 'Qn')} (gana S)."
    )

    # ---------------- latch SR con NAND ----------------
    C = SR_NAND
    B["srnand_eq"] = "\n".join(C.equations())
    names = {(1, 1): "mantener", (1, 0): "reset", (0, 1): "set", (0, 0): "PROHIBIDO"}
    rows = []
    for (Sn, Rn) in [(1, 1), (1, 0), (0, 1), (0, 0)]:
        for q0 in (0, 1):
            f, k, per = C.settle(C.state(Q=q0, Qn=1 - q0), C.x(Sn=Sn, Rn=Rn))
            assert per is None
            q, qn = C.val(f, "Q"), C.val(f, "Qn")
            exp = {(1, 1): (q0, 1 - q0), (1, 0): (0, 1), (0, 1): (1, 0), (0, 0): (1, 1)}[(Sn, Rn)]
            assert (q, qn) == exp
            rows.append([Sn, Rn, q0, q, qn, k, names[(Sn, Rn)]])
    B["srnand_behaviour"] = md_table(
        ["Sn", "Rn", "Q antes", "Q después", "Qn después", "pasos hasta quedar quieto", "modo"], rows
    )
    xs = [C.x(Sn=0 if t in (1, 2) else 1, Rn=0 if t in (6, 7) else 1) for t in range(12)]
    txt, st = htrace(C, C.state(Q=0, Qn=1), xs, ["Sn", "Rn"], ["Q", "Qn"])
    B["srnand_trace"] = txt
    assert (C.val(st[5], "Q"), C.val(st[5], "Qn")) == (1, 0) and (C.val(st[-1], "Q"), C.val(st[-1], "Qn")) == (0, 1)
    xs = [C.x(Sn=0 if t < 3 else 1, Rn=0 if t < 3 else 1) for t in range(10)]
    txt, st = htrace(C, C.state(Q=0, Qn=1), xs, ["Sn", "Rn"], ["Q", "Qn"])
    B["srnand_forb"] = txt
    _, _, per_n = C.settle(st[3], C.x(Sn=1, Rn=1))
    assert per_n == 2 and (C.val(st[1], "Q"), C.val(st[1], "Qn")) == (1, 1)
    seq_n = [f"{C.val(s, 'Q')}{C.val(s, 'Qn')}" for s in st[3:9]]
    assert seq_n == ["11", "00", "11", "00", "11", "00"]
    B["f_srnand_forb"] = (
        f"Con Sn=Rn=0 el latch NAND llega a Q=Qn=1 (t=1); al soltar los dos a la vez (t=3) oscila "
        f"{' -> '.join(seq_n[:4])} -> ... con periodo {per_n}."
    )

    # ---------------- latch D con habilitacion ----------------
    C = D_LATCH
    B["dl_eq"] = "\n".join(C.equations())
    rows = []
    dl_steps = []
    for (E, D) in [(0, 0), (0, 1), (1, 0), (1, 1)]:
        for q0 in (0, 1):
            s_q0 = settled(C, C.state(), C.x(D=q0, E=1))
            assert C.val(s_q0, "Q") == q0
            f, k, per = C.settle(s_q0, C.x(D=D, E=E))
            assert per is None
            q, qn = C.val(f, "Q"), C.val(f, "Qn")
            want = D if E else q0
            assert (q, qn) == (want, 1 - want)
            dl_steps.append(k)
            rows.append([E, D, q0, q, qn, k, "transparente (Q sigue a D)" if E else "memoria (mantiene)"])
    B["dl_behaviour"] = md_table(
        ["E", "D", "Q antes", "Q después", "Qn después", "pasos hasta quedar quieto", "modo"], rows
    )
    d_ev = [(0, 0), (3, 1), (9, 0), (18, 1)]
    e_ev = [(0, 1), (15, 0)]
    n = 24
    dl, el = levels(d_ev, n), levels(e_ev, n)
    xs = [C.x(D=dl[t], E=el[t]) for t in range(n)]
    s0 = settled(C, C.state(), C.x(D=0, E=1))
    states = run_states(C, s0, xs)
    B["dl_trace"] = trace_wide(C, states, xs, ["D", "E"], ["nD", "a", "b", "Q", "Qn"], 0, n - 1)
    glitch = [t for t in range(n) if C.val(states[t], "Q") == 1 and C.val(states[t], "Qn") == 1]
    both0 = [t for t in range(n) if C.val(states[t], "a") == 0 and C.val(states[t], "b") == 0]
    q_changes = [t for t in range(1, n) if C.val(states[t], "Q") != C.val(states[t - 1], "Q")]
    d_rise_t, d_fall_t = 3, 9
    assert dl[d_rise_t] == 1 and dl[d_rise_t - 1] == 0 and dl[d_fall_t] == 0 and dl[d_fall_t - 1] == 1
    g_rise = [t for t in glitch if d_rise_t < t < d_fall_t]
    g_fall = [t for t in glitch if t > d_fall_t]
    assert len(g_rise) == 1 and len(g_fall) == 1 and both0 == [4]
    B["f_dl_glitch"] = (
        f"Cuando D sube (t={d_rise_t}), en t={both0[0]} las dos órdenes del latch interno (a y b) valen 0 a la vez y en t={g_rise[0]} "
        f"Q y Qn valen 1 a la vez durante un paso. Cuando D baja (t={d_fall_t}) pasa algo parecido: Q y Qn valen 1 a la vez en t={g_fall[0]}. "
        f"Es un glitch (un valor falso breve) porque nD llega un paso tarde; al final Q acaba siendo D."
    )
    assert glitch and both0
    B["f_dl_hold"] = (
        f"Con E=0 (desde t=15) D cambia a 1 en t=18 y Q no se mueve: Q vale {C.val(states[n - 1], 'Q')} en t={n - 1}. "
        f"Q solo cambia en t = {join_t(q_changes)}, siempre mientras E=1."
    )
    assert all(t < 15 for t in q_changes) and C.val(states[n - 1], "Q") == 0
    ok = 0
    for D in (0, 1):
        for init in itertools.product((0, 1), repeat=len(C.names)):
            f, k, per = C.settle(init, C.x(D=D, E=1))
            assert per is None and C.val(f, "Q") == D and C.val(f, "Qn") == 1 - D
            ok += 1
    n_init = 2 ** len(C.names)
    B["f_dl_conv"] = (
        f"Comprobado con el simulador: con E=1 y D fija, desde cualquiera de los {n_init} estados internos posibles "
        f"de sus {len(C.names)} puertas el latch D acaba en Q=D ({ok} casos en total, con D=0 y D=1)."
    )
    s_bad = C.state(nD=1, a=1, b=1, Q=1, Qn=1)
    _, _, per_bad = C.settle(s_bad, C.x(D=0, E=0))
    assert per_bad == 2
    B["f_dl_poweron"] = (
        f"Aviso: si el latch arranca con Q=Qn=1 y E=0 no tiene nada que lo corrija y oscila con periodo {per_bad}; "
        f"basta mantener E=1 unos pasos con D fija para dejarlo consistente."
    )

    # ---------------- flip-flop D maestro-esclavo ----------------
    C = DFF
    B["dff_eq"] = "\n".join(C.equations())
    n_gates = len(C.names)
    n_not = sum(1 for g in C.gates if g[1] == "NOT")
    n_nand = sum(1 for g in C.gates if g[1] == "NAND")
    B["f_dff_count"] = f"El flip-flop D completo usa {n_gates} puertas: {n_not} NOT y {n_nand} NAND."
    ok = 0
    for D in (0, 1):
        for init in itertools.product((0, 1), repeat=n_gates):
            xs_c = [C.x(D=D, CLK=0)] * FF_LOW + [C.x(D=D, CLK=1)] * FF_HIGH
            f = C.run(init, xs_c)[-1]
            assert C.val(f, "Q") == D and C.val(f, "Qn") == 1 - D
            ok += 1
    B["f_dff_conv"] = (
        f"Comprobado con el simulador: desde cualquiera de los {2 ** n_gates} estados internos posibles, con D fija, {FF_LOW} pasos con CLK=0 "
        f"y después {FF_HIGH} pasos con CLK=1, el flip-flop acaba en Q=D ({ok} casos con D=0 y D=1)."
    )

    half = 6
    n = 48
    clk = clock(n, half)
    d_ev = [(0, 0), (2, 1), (9, 0), (21, 1), (34, 0)]
    dsig = levels(d_ev, n)
    xs = [C.x(D=dsig[t], CLK=clk[t]) for t in range(n)]
    s0 = powerup_dff(0)
    states = run_states(C, s0, xs)
    edges = [t for t in range(1, n) if clk[t] == 1 and clk[t - 1] == 0]
    unstable_d, dtab = measure_dff_window()
    assert unstable_d == {-1, 0}
    for e in edges:  # ningun cambio de D en la ventana prohibida de ningun flanco
        for off in unstable_d:
            t = e + off
            assert dsig[t] == dsig[t - 1], (e, off)
    B["dff_wave"] = wave(
        [
            ("CLK", clk),
            ("D", dsig),
            ("M", [C.val(s, "M") for s in states]),
            ("Q", [C.val(s, "Q") for s in states]),
            ("Qn", [C.val(s, "Qn") for s in states]),
        ],
        n,
        edge_row=("CLK", "flanco"),
    )
    qs = [C.val(s, "Q") for s in states]
    q_changes = [t for t in range(1, n) if qs[t] != qs[t - 1]]
    for e in edges:
        end = e + 2 * half - 1
        assert qs[min(end, n - 1)] == dsig[e]
    changes_after = [min(t - e for e in edges if e < t) for t in q_changes]
    assert all(1 <= c <= 4 for c in changes_after)
    d_changes = [t for t in range(1, n) if dsig[t] != dsig[t - 1]]
    ms = [C.val(s, "M") for s in states]
    m_changes = [t for t in range(1, n) if ms[t] != ms[t - 1]]
    falls = [t for t in range(1, n) if clk[t] == 0 and clk[t - 1] == 1]
    d_low = [t for t in d_changes if clk[t] == 0]
    d_high = [t for t in d_changes if clk[t] == 1]
    assert d_low == [2] and d_high == [9, 21, 34]
    seen_m, seen_q = [], []
    for t in d_high:
        f_t = min(f for f in falls if f > t)
        m_t = min(m for m in m_changes if m > f_t)
        e_t = min(e for e in edges if e > t)
        q_t = min(q for q in q_changes if q > e_t)
        assert t < f_t < m_t < e_t < q_t
        seen_m.append((f_t, m_t))
        seen_q.append((e_t, q_t))
    m_first = min(m for m in m_changes if m > 2)
    q_first_ch = q_changes[0]
    B["f_dff_ignored"] = (
        f"D cambia en t = {join_t(d_changes)}. El cambio de t=2 ocurre con CLK=0: el maestro lo copia (M cambia en t={m_first}) y el "
        f"esclavo lo recoge en el flanco de t={edges[0]} (Q cambia en t={q_first_ch}). Los cambios de t = {join_t(d_high)} ocurren con CLK=1 "
        f"y el maestro está cerrado: M no se entera hasta que CLK baja (CLK baja en t = {join_t([a for a, _ in seen_m])} y M cambia en "
        f"t = {join_t([b for _, b in seen_m])}) y Q no cambia hasta el flanco siguiente (flancos en t = {join_t([a for a, _ in seen_q])}, "
        f"Q cambia en t = {join_t([b for _, b in seen_q])})."
    )

    B["f_dff_wave"] = (
        f"Flancos de subida de CLK (primer paso con CLK=1): t = {join_t(edges)}. "
        f"Q cambia en t = {join_t(q_changes)}, es decir {join_t(changes_after)} pasos después del flanco correspondiente, "
        f"y siempre al valor que D tenía en ese flanco ({', '.join(str(dsig[e]) for e in edges)})."
    )

    # ejemplo resuelto: tabla de las 10 puertas alrededor del flanco t=6
    t0, t1 = 2, 12
    B["dff_example"] = trace_wide(C, states, xs, ["D", "CLK"], C.names, t0, t1)
    e = edges[0]
    gv = lambda name, t: C.val(states[t], name)
    q_first = next(t for t in range(e + 1, n) if gv("Q", t) != gv("Q", t - 1))
    qn_first = next(t for t in range(e + 1, n) if gv("Qn", t) != gv("Qn", t - 1))
    assert gv("nCLK", e) == 1 and gv("nCLK", e + 1) == 0
    assert gv("a2", e) == 1 and gv("a2", e + 1) == 0  # esclavo reacciona un paso despues
    B["f_dff_edge"] = (
        f"El flanco es t={e} (CLK pasa de {clk[e - 1]} a {clk[e]}). nCLK todavía vale {gv('nCLK', e)} en t={e} y baja a "
        f"{gv('nCLK', e + 1)} en t={e + 1}: el NOT del reloj tarda un paso, así que el maestro sigue abierto un paso más. "
        f"El maestro ya tenía M={gv('M', e)} (copiaba D={dsig[e]}) y el esclavo lo recoge: a2 baja a {gv('a2', e + 1)} en t={e + 1}, "
        f"Q sube a {gv('Q', q_first)} en t={q_first} y Qn baja a {gv('Qn', qn_first)} en t={qn_first}."
    )
    assert q_first == e + 2 and qn_first == e + 3
    d_rise = d_changes[0]
    a1_fall = next(t for t in range(d_rise, e) if gv("a1", t) == 0 and gv("a1", t - 1) == 1)
    m_rise = next(t for t in range(d_rise, e) if gv("M", t) == 1 and gv("M", t - 1) == 0)
    mn_fall = next(t for t in range(d_rise, e + 1) if gv("Mn", t) == 0 and gv("Mn", t - 1) == 1)
    assert all(gv("a2", t) == 1 and gv("b2", t) == 1 for t in range(t0, e + 1))
    B["f_dff_ex_master"] = (
        f"Con CLK=0 el maestro está abierto. D vale 1 desde t={d_rise}: a1 baja a 0 en t={a1_fall}, M sube a 1 en t={m_rise} y "
        f"Mn baja a 0 en t={mn_fall}. Mientras tanto Q sigue en {gv('Q', e)}, porque el esclavo está cerrado (a2 y b2 valen 1 hasta t={e})."
    )
    assert gv("Q", q_first) == 1 and gv("Qn", q_first) == 1 and gv("Qn", qn_first) == 0
    B["f_dff_ex_overlap"] = (
        f"En la fila de t={q_first}, Q ya vale {gv('Q', q_first)} pero Qn todavía vale {gv('Qn', q_first)} (baja en t={qn_first}): "
        f"durante un paso Q y Qn valen 1 a la vez, el mismo efecto de \"una puerta ve el valor de hace un paso\" que en el latch NAND."
    )
    d_fall = d_changes[1]
    closed_from = next(t for t in range(e + 1, t1 + 1) if gv("a1", t) == 1 and gv("b1", t) == 1)
    assert all(gv("a1", t) == 1 and gv("b1", t) == 1 for t in range(closed_from, t1 + 1))
    assert gv("M", t1) == 1 and gv("Q", t1) == 1 and dsig[d_fall] == 0 and clk[d_fall] == 1
    B["f_dff_ex_closed"] = (
        f"D baja a 0 en t={d_fall}, ya con CLK=1. Como a1 y b1 valen 1 desde t={closed_from} (maestro cerrado), el cambio no entra: "
        f"en t={t1} M sigue valiendo {gv('M', t1)} y Q sigue valiendo {gv('Q', t1)}."
    )
    assert gv("a1", e + 1) == 1 - dsig[e] and gv("a1", e + 2) == 1 and gv("b1", e + 2) == 1  # maestro abierto en e+1, cerrado en e+2

    # ventana prohibida (setup/hold en este modelo)
    rows = []
    for off in range(-4, 4):
        lab = "e" if off == 0 else (f"e{off:+d}")
        rows.append([lab, dtab[(off, 0, 1)], dtab[(off, 1, 0)]])
    B["dff_window"] = md_table(
        ["D empieza a valer su valor nuevo en el instante", "D: 0 -> 1", "D: 1 -> 0"], rows
    )
    # comprobar que INESTABLE es de verdad oscilacion (Q=Qn) y medir su periodo
    unst_periods = set()
    for (off, db, da) in [(-1, 1, 0), (0, 0, 1)]:
        r, s_end = dff_trial(db, da, off)
        assert r == "INESTABLE"
        _, _, p = C.settle(s_end, C.x(D=da, CLK=0))
        assert C.val(s_end, "Q") == C.val(s_end, "Qn")
        unst_periods.add(p)
    assert len(unst_periods) == 1
    unst_per = unst_periods.pop()
    assert unst_per == 2
    B["f_dff_window"] = (
        f"Medido: si D cambia en el instante e-1 o en el instante e (e = primer instante con CLK=1) el resultado puede ser INESTABLE: "
        f"Q y Qn quedan iguales y oscilan con periodo {unst_per}. "
        f"Para dejarlo otra vez consistente basta aplicar, con D fija, {FF_LOW} pasos con CLK=0 y {FF_HIGH} con CLK=1 "
        f"(se comprueba desde cualquier estado, en «Y sobre el arranque» del Paso 6). "
        f"Con D estable desde e-2 o cambiando a partir de e+1, el flip-flop se porta bien."
    )
    gap = 3
    d_stats = {}
    cnt, worst = dff_random_test(unstable_d, half=half, min_gap=gap, stats=d_stats)
    B["f_dff_random"] = (
        f"Comprobado con {cnt} secuencias aleatorias de D (cambios fuera de la ventana prohibida y separados al menos {gap} pasos, "
        f"periodo de reloj {2 * half}): en cada flanco Q acaba valiendo D, y Q y Qn dejan de moverse como mucho {worst} pasos después del flanco."
    )

    # ---------------- flip-flop T ----------------
    C = TFF
    B["tff_eq"] = "\n".join(C.equations(names=["D"]))
    lim = tff_clock_limits()
    unstable_t, ttab = lim["unstable"], lim["table"]
    min_fixed, min_any = lim["min_fixed"], lim["min_any"]
    half = 6
    assert half >= min_any
    t_stats = lim["stats"]  # T=1 fija mas T aleatoria con el reloj de los diagramas
    n = 72
    clk = clock(n, half)
    t_ev = [(0, 1), (12, 0), (24, 1), (60, 0)]
    tsig = levels(t_ev, n)
    xs = [C.x(T=tsig[t], CLK=clk[t]) for t in range(n)]
    states = run_states(C, powerup_tff(0), xs)
    edges = [t for t in range(1, n) if clk[t] == 1 and clk[t - 1] == 0]
    for e in edges:
        for off in unstable_t:
            assert tsig[e + off] == tsig[e + off - 1]
    qs = [C.val(s, "Q") for s in states]
    q_prev = 0
    exp_q = []
    for i, e in enumerate(edges):
        end = (edges[i + 1] - 1) if i + 1 < len(edges) else n - 1
        q_prev ^= tsig[e - 1]
        assert qs[end] == q_prev
        exp_q.append(q_prev)
    tq_changes = [t for t in range(1, n) if qs[t] != qs[t - 1]]
    tff_after = [min(t - e for e in edges if e < t) for t in tq_changes]
    B["tff_wave"] = wave(
        [("CLK", clk), ("T", tsig), ("D", [C.val(s, "D") for s in states]), ("Q", qs)], n, edge_row=("CLK", "flanco")
    )
    B["f_tff_wave"] = (
        f"Flancos en t = {join_t(edges)}. T vale {', '.join(str(tsig[e - 1]) for e in edges)} justo antes de cada uno, "
        f"y Q vale {', '.join(str(q) for q in exp_q)} después: cambia (hace toggle) cuando T=1 y se queda igual cuando T=0."
    )
    # divisor de frecuencia: T=1 fijo
    n2 = 60
    clk2 = clock(n2, half)
    xs2 = [C.x(T=1, CLK=clk2[t]) for t in range(n2)]
    st2 = run_states(C, powerup_tff(0), xs2)
    q2 = [C.val(s, "Q") for s in st2]
    B["tff_div"] = wave([("CLK", clk2), ("Q", q2)], n2, edge_row=("CLK", "flanco"))
    rises = [t for t in range(1, n2) if q2[t] == 1 and q2[t - 1] == 0]
    falls = [t for t in range(1, n2) if q2[t] == 0 and q2[t - 1] == 1]
    assert len(rises) >= 2
    per_q = rises[1] - rises[0]
    per_clk = 2 * half
    assert per_q == 2 * per_clk
    B["f_tff_div"] = (
        f"Con T=1 fija, Q sube en t = {join_t(rises)}: entre dos subidas pasan {per_q} pasos, el doble que el periodo de CLK "
        f"({per_clk} pasos). Un T-FF con T=1 es un divisor de frecuencia entre dos."
    )
    rows = []
    for off in range(-4, 4):
        lab = "e" if off == 0 else (f"e{off:+d}")
        rows.append([lab, ttab[(off, 0, 1)], ttab[(off, 1, 0)]])
    B["tff_window"] = md_table(
        ["T empieza a valer su valor nuevo en el instante", "T: 0 -> 1", "T: 1 -> 0"], rows
    )
    assert ttab[(0, 0, 1)] == "viejo" and ttab[(0, 1, 0)] == "viejo"
    B["f_tff_window"] = (
        f"Medido: en el flip-flop T los instantes prohibidos para un cambio de T son e-2 y e-1, un paso antes que en el D, "
        f"porque T tiene que atravesar la puerta XOR antes de llegar a D. Si T cambia en el instante e, Q todavía usa el T antiguo: "
        f"el T-FF usa el valor que T tenía en e-1."
    )
    # Los dos minimos y su causa. La regla: D = T xor Q cambia 1 paso despues de que Q cambie, y ese cambio tiene que
    # acabar antes de la ventana prohibida de D del flanco siguiente, que empieza `lead` pasos antes de ese flanco.
    qf_hi, qa_hi = lim["q_fixed"][1], lim["q_any"][1]
    assert lim["q_fixed"][0] == lim["q_any"][0] == 2 and qf_hi == 3 and qa_hi == 4
    lead = -min(unstable_d)

    def rule_min(q_hi):
        return next(h for h in range(1, 30) if q_hi + 1 < 2 * h - lead)

    assert rule_min(qf_hi) == min_fixed and rule_min(qa_hi) == min_any  # la regla reproduce las dos medidas
    assert 2 * min_fixed == 6  # el 06 mide lo mismo con su simulador: check_C.py exige gt_div_P == 2 * min_fixed (divisor con T=1, periodo 6)
    xor_delay = 1
    # el retardo mayor sale con T bajando de 1 a 0 en e-3 (justo antes de la ventana prohibida): Q asoma un pulso falso y se corrige
    late = 1 - min(unstable_t)  # 3
    n_g = 6 * half
    clk_g = clock(n_g, half)
    xs_g = [TFF.x(T=(1 if t < half - late else 0), CLK=clk_g[t]) for t in range(n_g)]
    qs_g = [TFF.val(st, "Q") for st in TFF.run(powerup_tff(0), xs_g[:-1])]
    glitch = [t - half for t in range(half + 1, 3 * half) if qs_g[t] != qs_g[t - 1]]
    assert glitch == [lim["q_any"][0], qa_hi] and qs_g[3 * half - 1] == 0  # sube a los 2 pasos y vuelve a bajar a los 4: Q acaba bien
    B["f_tff_half"] = (
        f"Medido con un reloj simétrico y leyendo Q un paso antes del flanco siguiente (cuando ya ha pasado un ciclo entero desde el flanco): "
        f"con T=1 fija (el divisor de frecuencia) el T-FF hace toggle en todos los flancos desde semiperiodo {min_fixed} (periodo {2 * min_fixed}) "
        f"y falla con semiperiodo {join_y(lim['fails_fixed'])}. Se probaron semiperiodos de {HALF_SWEEP.start} a {HALF_SWEEP.stop - 1}."
    )
    B["f_tff_half_any"] = (
        f"Si T cambia en instantes aleatorios, siempre fuera de la ventana prohibida ({TFF_RANDOM_COUNT} secuencias aleatorias por semiperiodo, "
        f"además de T=1 fija), el mínimo es mayor: semiperiodo {min_any} (periodo {2 * min_any}); con semiperiodo {join_y(lim['fails_any'])} falla."
    )
    B["f_tff_half_why"] = (
        f"La diferencia viene de cuánto tarda Q en dejar de moverse después del flanco: como mucho {qf_hi} pasos con T=1 fija, pero hasta {qa_hi} "
        f"si T baja de 1 a 0 en el instante e-{late}, justo antes de la ventana prohibida, porque entonces Q asoma un pulso falso breve "
        f"(sube a los {glitch[0]} pasos y vuelve a bajar a los {glitch[1]}). Q llega a D a través de la XOR, un paso más tarde "
        f"(como mucho {qf_hi + xor_delay} y {qa_hi + xor_delay} pasos después del flanco), y ese cambio de D tiene que acabar antes de la ventana "
        f"prohibida de D del flanco siguiente (Tabla 4.7), que empieza {lead} paso antes de ese flanco: a {2 * min_fixed - lead} pasos del flanco anterior "
        f"con semiperiodo {min_fixed} y a {2 * min_any - lead} con semiperiodo {min_any}. Con T=1 fija, {qf_hi + xor_delay} es menor que {2 * min_fixed - lead} "
        f"y basta con semiperiodo {min_fixed}; con T cambiante, {qa_hi + xor_delay} no es menor que {2 * min_fixed - lead} y hay que subir a semiperiodo {min_any}."
    )
    # tabla de excitacion verificada contra el comportamiento gate-level del T-FF, 1 flanco
    for q0 in (0, 1):
        for T in (0, 1):
            q_sim = None
            nn = 36
            clk3 = clock(nn, half)
            xs3 = [C.x(T=T, CLK=clk3[t]) for t in range(nn)]
            s_init = powerup_tff(q0)
            st3 = run_states(C, s_init, xs3)
            q_after = C.val(st3[half + 2 * half - 1], "Q")  # justo antes del 2.o flanco
            assert q_after == (q0 ^ T)

    # ---------------- tablas caracteristicas y de excitacion ----------------
    # Q(t+1) en las tablas = valor en el que SE ASIENTA Q, no un unico paso del simulador
    q_lo = min(d_stats["qlo"], t_stats["qlo"])
    q_hi = max(d_stats["qhi"], t_stats["qhi"])
    assert max(sr_steps) >= 2 and max(dl_steps) >= 2 and q_hi >= 2
    assert set(changes_after + tff_after) <= set(range(q_lo, q_hi + 1))  # las formas de onda de los Pasos 6 y 8 caen dentro
    B["f_q_settle"] = (
        f"Medido en el modelo de puertas: el latch SR tarda hasta {max(sr_steps)} pasos en quedar quieto (Tabla 4.2); "
        f"el latch D, hasta {max(dl_steps)} pasos en quedar todo quieto, puertas internas incluidas (Tabla 4.6); "
        f"y en los flip-flops D y T, con un reloj de semiperiodo {half} y datos aleatorios, Q cambia entre {q_lo} y {q_hi} pasos "
        f"después del flanco (en el D, además, Q y Qn dejan de moverse como mucho {worst} pasos después)."
    )
    # SR (latch NOR): Q+ = S + (no R)*Q  cuando S*R = 0
    rows = []
    for S, R in [(0, 0), (0, 1), (1, 0), (1, 1)]:
        vals = {res[(S, R, q0)][0] for q0 in (0, 1)}
        if (S, R) == (1, 1):
            cell = "prohibido (Q = Qn = 0)"
        else:
            for q0 in (0, 1):
                assert res[(S, R, q0)][0] == (S | ((1 - R) & q0))
            cell = "Q(t)" if len(vals) == 2 else str(vals.pop())
        rows.append([S, R, cell])
    B["char_sr"] = md_table(["S", "R", "Q(t+1)"], rows)
    # D latch con habilitacion
    rows = []
    for E, D in [(0, 0), (0, 1), (1, 0), (1, 1)]:
        assert all((D if E else q0) == (E * D) | ((1 - E) * q0) for q0 in (0, 1))
        rows.append([E, D, "Q(t)" if E == 0 else str(D)])
    B["char_dl"] = md_table(["E", "D", "Q(t+1)"], rows)
    B["char_dff"] = md_table(["D", "Q(t+1) tras el flanco de subida"], [[0, 0], [1, 1]])
    rows = []
    for T in (0, 1):
        for q0 in (0, 1):
            rows.append([T, q0, q0 ^ T])
    B["char_tff"] = md_table(["T", "Q(t)", "Q(t+1) tras el flanco de subida"], rows)
    B["char_eqs"] = "\n".join(
        [
            "Latch SR con NOR (si S · R = 0):  Q(t+1) = S + ¬R · Q(t)",
            "Latch D con habilitación E:  Q(t+1) = E · D + ¬E · Q(t)",
            "Flip-flop D (flanco de subida):  Q(t+1) = D",
            "Flip-flop T (flanco de subida):  Q(t+1) = T ⊕ Q(t)",
        ]
    )
    # excitacion
    rows = []
    for q0, q1 in itertools.product((0, 1), repeat=2):
        ok_sr = [(S, R) for S in (0, 1) for R in (0, 1) if (S, R) != (1, 1) and (S | ((1 - R) & q0)) == q1]
        s_vals = {s for s, _ in ok_sr}
        r_vals = {r for _, r in ok_sr}
        s_cell = "X" if len(s_vals) == 2 else str(next(iter(s_vals)))
        r_cell = "X" if len(r_vals) == 2 else str(next(iter(r_vals)))
        d_need = q1
        t_need = q0 ^ q1
        assert all((q0 ^ T) == q1 for T in (t_need,)) and (q0 ^ (1 - t_need)) != q1
        rows.append([q0, q1, s_cell, r_cell, d_need, t_need])
    assert [r[2:4] for r in rows] == [["0", "X"], ["1", "0"], ["0", "1"], ["X", "0"]]
    B["excitation"] = md_table(["Q(t)", "Q(t+1)", "S", "R", "D", "T"], rows)

    # ---------------- ejercicio ----------------
    C = SR_NOR
    ex_in = [(0, 0), (1, 0), (1, 0), (1, 0), (0, 0), (0, 0), (0, 0)]
    xs = [C.x(S=s, R=r) for s, r in ex_in]
    s0 = C.state(Q=0, Qn=1)
    tab, states = trace_table(C, s0, xs, ["S", "R"], ["Q", "Qn"])
    lines = tab.split("\n")
    q_rows = lines[:2]
    for t, (s, r) in enumerate(ex_in):
        if t == 0:
            q_rows.append(f"| {t} | {s} | {r} | 0 | 1 |")
        else:
            q_rows.append(f"| {t} | {s} | {r} | ? | ? |")
    B["ex04_question"] = "\n".join(q_rows)
    B["ex04_solution"] = tab
    final = (C.val(states[-1], "Q"), C.val(states[-1], "Qn"))
    assert final == (1, 0)
    f1, k1, p1 = C.settle(run_states(C, s0, [C.x(S=1 if t == 1 else 0, R=0) for t in range(8)])[-1], C.x(S=0, R=0))
    assert p1 == 2
    B["f_ex04"] = (
        f"Solución: Q y Qn terminan en ({final[0]}, {final[1]}) en t={len(ex_in) - 1}. "
        f"Con un pulso de S de un solo paso (S=1 solo en t=1) el latch no memoriza: entra en una oscilación de periodo {p1}."
    )
    return B


# ---------------------------------------------------------------------------
# 5. Circuitos y bloques del documento 05 (aritmetica binaria)
# ---------------------------------------------------------------------------

HA = Circuit("semisumador", ["A", "B"], [("S", "XOR", ["A", "B"]), ("C", "AND", ["A", "B"])])
FA = Circuit(
    "sumador completo",
    ["A", "B", "Cin"],
    [
        ("p", "XOR", ["A", "B"]),  # propagate
        ("g", "AND", ["A", "B"]),  # generate
        ("S", "XOR", ["p", "Cin"]),
        ("pc", "AND", ["p", "Cin"]),  # acarreo propagado: llega Cin y p lo deja pasar
        ("Cout", "OR", ["g", "pc"]),
    ],
)
FA_NAND = Circuit(
    "sumador completo solo NAND",
    ["A", "B", "Cin"],
    [
        ("u", "NAND", ["A", "B"]),
        ("v", "NAND", ["A", "u"]),
        ("w", "NAND", ["B", "u"]),
        ("p", "NAND", ["v", "w"]),
        ("y", "NAND", ["p", "Cin"]),
        ("z", "NAND", ["p", "y"]),
        ("q", "NAND", ["Cin", "y"]),
        ("S", "NAND", ["z", "q"]),
        ("Cout", "NAND", ["u", "y"]),
    ],
)

SYM = {"XOR": "⊕", "AND": "·", "OR": "+"}


def sym_equations(circ):
    out = []
    for n, op, srcs in circ.gates:
        if op in SYM:
            out.append(f"{n} = " + f" {SYM[op]} ".join(srcs))
        elif op == "NOT":
            out.append(f"{n} = ¬{srcs[0]}")
        else:
            out.append(f"{n} = {op}(" + ", ".join(srcs) + ")")
    return out


def make_adder(n, style="aoi", sub=False):
    ins = [f"A{i}" for i in range(n)] + [f"B{i}" for i in range(n)] + (["SUB"] if sub else ["Cin"])
    cin0 = "SUB" if sub else "Cin"
    gates = []
    for i in range(n):
        a, b = f"A{i}", f"B{i}"
        if sub:
            gates.append((f"Bx{i}", "XOR", [b, "SUB"]))
            b = f"Bx{i}"
        ci = cin0 if i == 0 else f"C{i}"
        if style == "aoi":
            gates += [
                (f"p{i}", "XOR", [a, b]),
                (f"S{i}", "XOR", [f"p{i}", ci]),
                (f"g{i}", "AND", [a, b]),
                (f"pc{i}", "AND", [f"p{i}", ci]),
                (f"C{i + 1}", "OR", [f"g{i}", f"pc{i}"]),
            ]
        else:
            gates += [
                (f"u{i}", "NAND", [a, b]),
                (f"v{i}", "NAND", [a, f"u{i}"]),
                (f"w{i}", "NAND", [b, f"u{i}"]),
                (f"p{i}", "NAND", [f"v{i}", f"w{i}"]),
                (f"y{i}", "NAND", [f"p{i}", ci]),
                (f"z{i}", "NAND", [f"p{i}", f"y{i}"]),
                (f"q{i}", "NAND", [ci, f"y{i}"]),
                (f"S{i}", "NAND", [f"z{i}", f"q{i}"]),
                (f"C{i + 1}", "NAND", [f"u{i}", f"y{i}"]),
            ]
    kind = ("sumador/restador" if sub else "sumador") + f" de {n} bits ({'solo NAND' if style != 'aoi' else 'XOR/AND/OR'})"
    return Circuit(kind, ins, gates)


def adder_x(circ, n, a, b, c):
    kw = {f"A{i}": (a >> i) & 1 for i in range(n)}
    kw.update({f"B{i}": (b >> i) & 1 for i in range(n)})
    kw["SUB" if "SUB" in circ.in_idx else "Cin"] = c
    return circ.x(**kw)


def word(circ, state, prefix, n):
    return sum(circ.val(state, f"{prefix}{i}") << i for i in range(n))


def run_adder(circ, n, a, b, c):
    """Valores en regimen permanente: (S, C4, lista de acarreos C0..Cn, estado)."""
    st = settled(circ, circ.state(), adder_x(circ, n, a, b, c))
    carries = [c] + [circ.val(st, f"C{i}") for i in range(1, n + 1)]
    return word(circ, st, "S", n), carries[n], carries, st


def last_change_outputs(circ, s_old, x_new, outs, limit=100):
    oi = [circ.g_idx[o] for o in outs]
    s, last = s_old, 0
    for t in range(1, limit):
        ns = circ.step(s, x_new)
        if ns == s:
            return last
        if any(ns[i] != s[i] for i in oi):
            last = t
        s = ns
    raise AssertionError("no se asienta")


def worst_settle_exhaustive(circ, outs):
    oi = [circ.g_idx[o] for o in outs]
    xs_all = list(itertools.product((0, 1), repeat=len(circ.inputs)))
    sm = {x: settled(circ, circ.state(), x) for x in xs_all}
    worst, wit = 0, None
    for xo in xs_all:
        so = sm[xo]
        for xn in xs_all:
            s, last = so, 0
            for t in range(1, 100):
                ns = circ.step(s, xn)
                if ns == s:
                    break
                if any(ns[i] != s[i] for i in oi):
                    last = t
                s = ns
            else:
                raise AssertionError("no se asienta")
            if last > worst:
                worst, wit = last, (xo, xn)
    return worst, wit


def depth_to(circ, outs, src=None):
    """Mayor numero de puertas en un camino desde `src` (o desde cualquier entrada) hasta cada salida."""
    memo = {}

    def d(name):
        if name in memo:
            return memo[name]
        if name in circ.in_idx:
            r = 0 if (src is None or name == src) else None
        else:
            _, _, srcs = circ.gates[circ.g_idx[name]]
            best = None
            for s in srcs:
                v = d(s)
                if v is not None and (best is None or v > best):
                    best = v
            r = None if best is None else best + 1
        memo[name] = r
        return r

    vals = [d(o) for o in outs]
    vals = [v for v in vals if v is not None]
    return max(vals) if vals else None


def bstr(v, n=4):
    return "0b" + format(v & ((1 << n) - 1), f"0{n}b")


def hstr(v, n=4):
    return "0x" + format(v & ((1 << n) - 1), "X")


def sval(v, n=4):
    v &= (1 << n) - 1
    return v - (1 << n) if v >> (n - 1) else v


def bit_rows(circ, n, a, b, c, sub=False):
    s, cout, carries, st = run_adder(circ, n, a, b, c)
    rows = []
    for i in range(n):
        ai = (a >> i) & 1
        bi = circ.val(st, f"Bx{i}") if sub else (b >> i) & 1
        if sub:
            rows.append([i, ai, (b >> i) & 1, bi, carries[i], (s >> i) & 1, carries[i + 1]])
        else:
            rows.append([i, ai, bi, carries[i], (s >> i) & 1, carries[i + 1]])
    return rows, s, cout, carries, st


def adder_table(circ, n, a, b, c, sub=False):
    rows, s, cout, carries, st = bit_rows(circ, n, a, b, c, sub)
    if sub:
        hdr = ["bit i", "A_i", "B_i", "NOT B_i", "acarreo que entra", "S_i", "acarreo que sale"]
    else:
        hdr = ["bit i", "A_i", "B_i", "acarreo que entra", "S_i", "acarreo que sale"]
    return md_table(hdr, rows), s, cout, carries


def adder_trace(circ, n, a, b, c, a0=0, b0=0, c0=0):
    """Traza del asentamiento desde entradas (a0, b0, c0) a (a, b, c), hasta el ultimo cambio de las salidas.
    Devuelve (texto, ultimo_cambio_de_salidas, lecturas)."""
    s_old = settled(circ, circ.state(), adder_x(circ, n, a0, b0, c0))
    x_new = adder_x(circ, n, a, b, c)
    states = [s_old]
    for _ in range(60):
        ns = circ.step(states[-1], x_new)
        if ns == states[-1]:
            break
        states.append(ns)
    outs = [f"S{i}" for i in range(n)] + [f"C{n}"]
    oi = [circ.g_idx[o] for o in outs]
    last = max([t for t in range(1, len(states)) if any(states[t][k] != states[t - 1][k] for k in oi)] or [0])
    cn = [f"C{i}" for i in range(n, 0, -1)]
    sn = [f"S{i}" for i in range(n - 1, -1, -1)]
    lines = [" t | " + " ".join(cn) + " | " + " ".join(sn) + " | S como numero"]
    reads = []
    for t in range(last + 1):
        st = states[t]
        cv = " ".join(f" {circ.val(st, nme)}" for nme in cn)
        sv = " ".join(f" {circ.val(st, nme)}" for nme in sn)
        sw = word(circ, st, "S", n)
        reads.append((circ.val(st, f"C{n}"), sw))
        lines.append(f"{t:>2} | {cv} | {sv} | {bstr(sw, n)} = {sw}")
    return "\n".join(l.rstrip() for l in lines), last, reads


def build05():
    B = {}
    N = 4
    MASK = (1 << N) - 1
    MODULUS = 1 << N
    outs4 = [f"S{i}" for i in range(N)] + [f"C{N}"]

    # ---------------- semisumador ----------------
    rows = []
    for a, b in itertools.product((0, 1), repeat=2):
        st = settled(HA, HA.state(), HA.x(A=a, B=b))
        s_, c_ = HA.val(st, "S"), HA.val(st, "C")
        assert 2 * c_ + s_ == a + b and s_ == a ^ b and c_ == a & b
        rows.append([a, b, s_, c_, f"{a + b} = 2×{c_} + {s_}"])
    B["ha_truth"] = md_table(["A", "B", "S = A ⊕ B", "C = A · B", "suma aritmética = 2×C + S"], rows)
    B["ha_eq"] = "\n".join(sym_equations(HA))
    B["f_ha_cost"] = f"El semisumador son {len(HA.gates)} puertas (una XOR y una AND) y sus dos salidas se asientan en {depth_to(HA, ['S', 'C'])} paso."

    # ---------------- sumador completo ----------------
    rows = []
    for a, b, c in itertools.product((0, 1), repeat=3):
        st = settled(FA, FA.state(), FA.x(A=a, B=b, Cin=c))
        s_, co = FA.val(st, "S"), FA.val(st, "Cout")
        assert 2 * co + s_ == a + b + c
        assert s_ == a ^ b ^ c and co == ((a & b) | (c & (a ^ b))) and co == int(a + b + c >= 2)
        # nombres de la literatura: g = generate (A AND B), p = propagate (A XOR B), y Cout = g + p . Cin
        p_, g_, pc_ = FA.val(st, "p"), FA.val(st, "g"), FA.val(st, "pc")
        assert p_ == a ^ b and g_ == a & b and pc_ == p_ & c and co == (g_ | (p_ & c))
        # si p = 1 el acarreo que llega pasa de largo; si g = 1 se genera uno aunque no llegue ninguno
        assert (p_ == 1 and c == 1) <= (co == 1) and (g_ == 1) <= (co == 1) and (p_ & g_) == 0
        rows.append([a, b, c, s_, co, f"{a + b + c} = 2×{co} + {s_}"])
    B["fa_truth"] = md_table(["A", "B", "Cin", "S", "Cout", "suma aritmética = 2×Cout + S"], rows)
    B["fa_eq"] = "\n".join(sym_equations(FA))
    B["fa_formulas"] = "S    = A ⊕ B ⊕ Cin\nCout = A · B + Cin · (A ⊕ B)"
    n_fa = len(FA.gates)
    d = {}
    rows = []
    for inp in ("A", "B", "Cin"):
        ds, dc = depth_to(FA, ["S"], src=inp), depth_to(FA, ["Cout"], src=inp)
        d[inp] = (ds, dc)
        rows.append([inp, ds, dc])
    B["fa_delays"] = md_table(["entrada que cambia", "pasos hasta S", "pasos hasta Cout (camino más largo)"], rows)
    assert d["Cin"][1] == 2 and d["A"][1] == 3
    B["f_fa_cost"] = (
        f"El sumador completo son {n_fa} puertas (dos XOR, dos AND y una OR). "
        f"Lo importante para más adelante: desde Cin hasta Cout hay {d['Cin'][1]} puertas en el camino más largo, "
        f"mientras que desde A o B hasta Cout hay {d['A'][1]}."
    )
    # version solo NAND
    for a, b, c in itertools.product((0, 1), repeat=3):
        st = settled(FA_NAND, FA_NAND.state(), FA_NAND.x(A=a, B=b, Cin=c))
        assert FA_NAND.val(st, "S") == a ^ b ^ c and FA_NAND.val(st, "Cout") == int(a + b + c >= 2)
    dn = {inp: (depth_to(FA_NAND, ["S"], src=inp), depth_to(FA_NAND, ["Cout"], src=inp)) for inp in ("A", "B", "Cin")}
    B["fa_nand_eq"] = "\n".join(FA_NAND.equations())
    B["f_fa_nand"] = (
        f"Con solo puertas NAND el sumador completo necesita {len(FA_NAND.gates)} puertas (comprobado en las 8 combinaciones); "
        f"el camino de Cin a Cout sigue teniendo {dn['Cin'][1]} puertas, pero de A o B a Cout hay {dn['A'][1]} y de A o B a S hay {dn['A'][0]}."
    )

    # ---------------- sumador de 4 bits ----------------
    A4 = make_adder(N)
    B["rip_eq"] = "\n".join(A4.equations(names=[f"{nm}{i}" for i in (0, 1) for nm in ("p", "S", "g", "pc")] + ["C1", "C2"]))
    B["f_rip_count"] = f"El sumador de {N} bits son {len(A4.gates)} puertas ({N} sumadores completos de {n_fa} puertas) y {len(A4.inputs)} entradas (A0 a A{N - 1}, B0 a B{N - 1} y Cin)."
    n_comb = 0
    for a, b, c in itertools.product(range(16), range(16), (0, 1)):
        s_, co, carries, _ = run_adder(A4, N, a, b, c)
        assert (co << N) + s_ == a + b + c
        n_comb += 1
    B["f_rip_verify"] = (
        f"Comprobado con el simulador: el sumador de {N} bits hecho de puertas da, en las {n_comb} combinaciones posibles de A, B y Cin, "
        f"un resultado de cinco bits (C{N} y S{N - 1} a S0) igual a la suma aritmética."
    )

    # ---------------- complemento a dos ----------------
    rows = []
    for v in range(16):
        inv = (~v) & MASK
        neg = (inv + 1) & MASK
        assert sval(neg) == (-sval(v) if v != 8 else -8)
        assert sval(v) == -8 * ((v >> 3) & 1) + 4 * ((v >> 2) & 1) + 2 * ((v >> 1) & 1) + (v & 1)
        assert (v + inv) == MASK
        rows.append([bstr(v), hstr(v), v, sval(v), bstr(inv), bstr(neg), sval(neg)])
    B["twos_table"] = md_table(
        ["patrón", "hex", "sin signo", "con signo", "NOT patrón", "NOT patrón + 1", "valor con signo de NOT patrón + 1"], rows
    )
    B["twos_formula"] = "valor con signo de b3 b2 b1 b0 = -8×b3 + 4×b2 + 2×b1 + 1×b0"
    B["f_twos_range"] = (
        f"Con {N} bits hay {2 ** N} patrones: sin signo representan de 0 a {MASK}; con signo, de {-(2 ** (N - 1))} a {2 ** (N - 1) - 1}."
    )
    B["f_twos_neg8"] = (
        f"Excepción: negar {bstr(8)} (que vale {sval(8)}) da otra vez {bstr(((~8) & MASK) + 1)} ({sval(((~8) & MASK) + 1)}), "
        f"porque {2 ** (N - 1)} no cabe en {N} bits con signo."
    )
    B["f_twos_why"] = (
        f"Por qué funciona: cualquier patrón x y NOT x suman siempre {bstr(MASK)} (comprobado con los {2 ** N} patrones); "
        f"sumando 1 se llega a {bstr(MASK + 1, N + 1)} ({MASK + 1}), que con {N} bits es {bstr(0)}: módulo {MODULUS} vale 0. "
        f"Es decir, NOT x + 1 hace de -x."
    )
    # por que el mismo sumador sirve con y sin signo: todo se calcula modulo 2^n
    for v in range(16):
        assert sval(v) == v - MODULUS * ((v >> (N - 1)) & 1)  # el bit de signo pesa -8 en vez de +8: diferencia de 16
        assert (v + ((~v) & MASK) + 1) == MODULUS  # x + NOT x + 1 = 2^n, que es 0 modulo 2^n
    for a, b in itertools.product(range(16), repeat=2):
        s_, co, carries, _ = run_adder(A4, N, a, b, 0)
        assert s_ == (a + b) % MODULUS == (a + b) & MASK  # el sumador conserva la suma modulo 2^n
        assert (sval(a) + sval(b) - (a + b)) % MODULUS == 0  # leer con signo no cambia el resultado modulo 2^n
    mp, mb = 0b1101, 0b0101  # -3 y 5: el ejemplo 3
    m_s, m_co, _, _ = run_adder(A4, N, mp, mb, 0)
    assert (sval(mp), mp + mb, (mp + mb) % MODULUS, sval(mp) + mb) == (-3, 18, 2, 2) and m_s == 2 and m_co == 1
    assert (-3 + 5) & MASK == 2 and (-3) & MASK == mp  # el mismo calculo con enteros de PHP o Python
    B["f_mod_clock"] = (
        f"Con {N} bits todo se calcula módulo {MODULUS}: es como un cuentakilómetros de {N} dígitos binarios, que después del {MASK} vuelve al {0}. "
        f"Por eso el acarreo que sale del último bit se descarta, y en PHP o Python `& {bstr(MASK)}` hace exactamente lo mismo "
        f"(quedarse con los {N} bits de la derecha)."
    )
    B["f_mod_sign"] = (
        f"Con esa idea, el bit de signo solo cambia cómo se lee un patrón: pesa -{2 ** (N - 1)} en vez de +{2 ** (N - 1)}, "
        f"una diferencia de {MODULUS}. Si b{N - 1} = 0, el patrón vale k (lo que vale sin signo); si b{N - 1} = 1, vale k - {MODULUS}. "
        f"Por ejemplo, {bstr(mp)} es {mp} sin signo y {mp} - {MODULUS} = {sval(mp)} con signo. "
        f"Como k y k - {MODULUS} son el mismo número módulo {MODULUS}, el sumador, que se queda con los {N} bits de la derecha, "
        f"entrega los mismos bits con las dos lecturas."
    )
    B["f_mod_example"] = (
        f"Ejemplo: {bstr(mp)} + {bstr(mb)}. Sin signo es {mp} + {mb} = {mp + mb}, y {mp + mb} módulo {MODULUS} es {(mp + mb) % MODULUS}; "
        f"con signo es ({sval(mp)}) + {mb} = {sval(mp) + mb}. En los dos casos el sumador entrega el mismo patrón, {bstr(m_s)}. "
        f"Lo que cambia es si ese patrón es la respuesta correcta: con signo sí, porque {sval(mp) + mb} cabe en {-(2 ** (N - 1))} a {2 ** (N - 1) - 1}; "
        f"sin signo no, porque {mp + mb} no cabe en {N} bits (C = {m_co})."
    )

    # ---------------- desbordamiento ----------------
    cat = {}
    sign_rule = {}
    for a, b in itertools.product(range(16), repeat=2):
        s_, co, carries, _ = run_adder(A4, N, a, b, 0)
        v_flag = carries[N - 1] ^ carries[N]
        true_signed = sval(a) + sval(b)
        v_range = int(not (-8 <= true_signed <= 7))
        sa, sb, sr = (a >> 3) & 1, (b >> 3) & 1, (s_ >> 3) & 1
        v_sign = int(sa == sb and sr != sa)
        assert v_flag == v_range == v_sign
        assert co == int(a + b > MASK)
        cat.setdefault((co, v_flag), []).append((a, b))
        sign_rule.setdefault((sa, sb, sr), set()).add(v_flag)
    assert all(len(v) == 1 for v in sign_rule.values())
    lect = {
        (0, 0): "sin desbordamiento de ningún tipo",
        (1, 0): "desborda sin signo; con signo el resultado es correcto",
        (0, 1): "desborda con signo; sin signo el resultado es correcto",
        (1, 1): "desborda de las dos formas",
    }
    rows = []
    for key in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        lst = cat[key]
        a, b = {(0, 0): (0b0010, 0b0011), (1, 0): (0b1111, 0b0001), (0, 1): (0b0101, 0b0011), (1, 1): (0b1000, 0b1000)}[key]
        assert (a, b) in lst
        rows.append([key[0], key[1], len(lst), f"{bstr(a)} + {bstr(b)}", lect[key]])
    assert sum(len(v) for v in cat.values()) == 256
    B["ovf_table"] = md_table(["C", "V", "pares (de 256)", "un ejemplo", "lectura"], rows)
    rows = []
    for (sa, sb, sr), v in sorted(sign_rule.items()):
        rows.append([sa, sb, sr, next(iter(v))])
    B["ovf_sign"] = md_table(["bit de signo de A", "bit de signo de B", "bit de signo del resultado", "V"], rows)
    B["f_ovf_rule"] = (
        f"Comprobado con las 256 sumas (A y B de {N} bits, con Cin = 0) y el sumador hecho de puertas: "
        f"V = C{N - 1} ⊕ C{N} coincide siempre con la regla de los signos y con comprobar a mano si la suma cabe entre "
        f"{-(2 ** (N - 1))} y {2 ** (N - 1) - 1}; y C = C{N} coincide siempre con comprobar si la suma sin signo pasa de {MASK}."
    )
    B["ovf_formula"] = f"C = C{N}\nV = C{N - 1} ⊕ C{N}"

    # ---------------- resta con el sumador ----------------
    AS4 = make_adder(N, sub=True)
    B["addsub_eq"] = "\n".join(AS4.equations(names=[f"Bx{i}" for i in range(N)] + ["p0", "S0", "g0", "pc0", "C1"]))
    ok_c = ok_v = 0
    for a, b in itertools.product(range(16), repeat=2):
        s_, co, carries, _ = run_adder(AS4, N, a, b, 1)
        assert s_ == (a - b) & MASK
        assert co == int(a >= b)
        assert (carries[N - 1] ^ carries[N]) == int(not (-8 <= sval(a) - sval(b) <= 7))
        ok_c += 1
        ok_v += 1
    B["f_sub_rule"] = (
        f"Comprobado con las 256 restas (A - B con SUB = 1): el resultado de {N} bits es siempre (A - B) módulo {2 ** N}; "
        f"C{N} vale 1 exactamente cuando A >= B sin signo (es decir, C{N} = 0 significa que \"hubo que pedir prestado\"); "
        f"y V = C{N - 1} ⊕ C{N} sigue detectando que la resta con signo no cabe."
    )
    cases = [(7, 3), (3, 5), (5, 5), (8, 1)]
    rows = []
    for a, b in cases:
        s_, co, carries, st = run_adder(AS4, N, a, b, 1)
        v_flag = carries[N - 1] ^ carries[N]
        assert s_ == (a - b) & MASK
        rows.append(
            [
                f"{bstr(a)} - {bstr(b)}",
                f"{sval(a)} - ({sval(b)}) = {sval(a) - sval(b)}",
                bstr(word(AS4, st, "Bx", N)),
                bstr(s_),
                sval(s_),
                co,
                v_flag,
            ]
        )
    B["sub_table"] = md_table(
        ["resta", "con signo", "B tras el XOR (NOT B)", "resultado (4 bits)", "leído con signo", "C", "V"], rows
    )

    # ---------------- tiempo de asentamiento ----------------
    def outs_n(n):
        return [f"S{i}" for i in range(n)] + [f"C{n}"]

    rows = []
    measured = {}

    def trans(circ):
        return (2 ** len(circ.inputs)) ** 2

    for n in (1, 2, 3, 4):
        circ = make_adder(n)
        worst, wit = worst_settle_exhaustive(circ, outs_n(n))
        dep = depth_to(circ, outs_n(n))
        assert worst == dep == 2 * n + 1
        measured[("aoi", n)] = worst
        rows.append(["XOR/AND/OR", n, len(circ.gates), dep, worst, f"exhaustivo: las {trans(circ)} transiciones posibles"])
    for n in (8, 16):
        circ = make_adder(n)
        dep = depth_to(circ, outs_n(n))
        s_old = settled(circ, circ.state(), adder_x(circ, n, 0, 0, 0))
        last = last_change_outputs(circ, s_old, adder_x(circ, n, 0, (1 << n) - 1, 1), outs_n(n))
        assert last == dep == 2 * n + 1
        measured[("aoi", n)] = last
        rows.append(["XOR/AND/OR", n, len(circ.gates), dep, last, "cota por profundidad + una transición que la alcanza"])
    for n in (1, 2, 3, 4):
        circ = make_adder(n, "nand")
        worst, _ = worst_settle_exhaustive(circ, outs_n(n))
        dep = depth_to(circ, outs_n(n))
        assert worst == dep == 2 * n + 4
        measured[("nand", n)] = worst
        rows.append(["solo NAND", n, len(circ.gates), dep, worst, f"exhaustivo: las {trans(circ)} transiciones posibles"])
    worst_as, _ = worst_settle_exhaustive(AS4, outs4)
    dep_as = depth_to(AS4, outs4)
    assert worst_as == dep_as
    rows.append(
        ["XOR/AND/OR + XOR en B (restador)", 4, len(AS4.gates), dep_as, worst_as, f"exhaustivo: las {trans(AS4)} transiciones posibles"]
    )
    B["settle_table"] = md_table(
        ["variante", "bits n", "puertas", "profundidad (cota)", "peor caso medido (pasos)", "cómo se midió"], rows
    )
    B["f_settle_main"] = (
        f"Resultado medido: el peor caso del sumador de {N} bits (XOR/AND/OR) tarda {measured[('aoi', N)]} pasos en dejar de cambiar "
        f"sus salidas, cuando las entradas que cambian lo hacen todas a la vez en t=0 desde el estado ya asentado de las entradas anteriores, y con Cin como entrada normal. "
        f"En la versión con restador (un XOR más en cada B) tarda {worst_as}."
    )
    B["f_settle_nand"] = (
        f"En la versión solo NAND, para n = 1, 2, 3, 4 el peor caso es {', '.join(str(measured[('nand', n)]) for n in (1, 2, 3, 4))} pasos "
        f"(2n + 4): cada bit sigue añadiendo dos pasos, pero la base es más alta porque las puertas XOR hechas con NAND son más profundas."
    )
    B["f_settle_pattern"] = (
        f"Con esta descomposición en puertas, para n = {', '.join(str(n) for n in (1, 2, 3, 4, 8, 16))} bits el peor caso es "
        f"{', '.join(str(measured[('aoi', n)]) for n in (1, 2, 3, 4, 8, 16))} pasos, o sea 2n + 1: "
        f"cada bit más añade dos pasos, uno por cada puerta del camino del acarreo."
    )
    txt, last, reads = adder_trace(A4, N, 0, 15, 1)
    assert last == measured[("aoi", N)]
    B["settle_trace"] = txt
    distinct = []
    for r in reads:
        if not distinct or distinct[-1] != r:
            distinct.append(r)
    five = [r[0] * 16 + r[1] for r in distinct]
    st_tr = [settled(A4, A4.state(), adder_x(A4, N, 0, 0, 0))]
    xn = adder_x(A4, N, 0, 15, 1)
    for _ in range(last):
        st_tr.append(A4.step(st_tr[-1], xn))
    car_t = [next(t for t in range(len(st_tr)) if A4.val(st_tr[t], f"C{i}") == 1) for i in range(1, N + 1)]
    assert car_t == [3, 5, 7, 9] and five[-1] == 16 and all(v != 16 for v in five[:-1])
    B["f_settle_trace"] = (
        f"Antes de t=0 todas las entradas valen 0; en t=0 pasan a A = {bstr(0)}, B = {bstr(15)} y Cin = 1. "
        f"Los acarreos se encienden uno cada dos pasos ({', '.join(f'C{i + 1} en t={t}' for i, t in enumerate(car_t))}). "
        f"Mientras tanto, la lectura de cinco bits (C{N} S{N - 1} S{N - 2} S{N - 3} S{N - 4}) vale {', '.join(str(v) for v in five[:-1])} "
        f"y ninguno de esos valores es el correcto: la suma buena, {five[-1]}, no aparece hasta t={last}."
    )

    # ---------------- tres ejemplos completos ----------------
    def example(prefix, a, b, label_a, label_b, signed_note=None):
        tbl, s_, co, carries = adder_table(A4, N, a, b, 0)
        B[f"{prefix}_bits"] = tbl
        v_flag = carries[N - 1] ^ carries[N]
        u_true, s_true = a + b, sval(a) + sval(b)
        fits_u, fits_s = u_true <= MASK, -8 <= s_true <= 7
        assert co == int(not fits_u) and v_flag == int(not fits_s)
        _, last_k, _ = adder_trace(A4, N, a, b, 0)
        B[f"{prefix}_trace"] = adder_trace(A4, N, a, b, 0)[0]
        line1 = f"Resultado: {label_a} + {label_b} = {bstr(s_)} ({hstr(s_)}), con acarreo final C = {co} y V = {v_flag} (C{N - 1} = {carries[N - 1]}, C{N} = {carries[N]})."
        if fits_u:
            line2 = f"Leído sin signo: {a} + {b} = {u_true}, cabe en {N} bits (0 a {MASK}), así que C = 0."
        else:
            line2 = (
                f"Leído sin signo: {a} + {b} = {u_true} ({bstr(u_true, N + 1)}) no cabe en {N} bits (0 a {MASK}): C = 1 y el hardware "
                f"solo guarda los {N} bits de la derecha, {bstr(s_)} ({s_})."
            )
        if fits_s:
            line3 = f"Leído con signo: ({sval(a)}) + ({sval(b)}) = {s_true}, cabe en -8 a 7, así que V = 0 y {bstr(s_)} se lee como {sval(s_)}."
        else:
            line3 = (
                f"Leído con signo: ({sval(a)}) + ({sval(b)}) = {s_true}, no cabe en -8 a 7: V = 1 y {bstr(s_)} se lee como {sval(s_)}, que es incorrecto."
            )
        line4 = f"Tiempo: partiendo de todas las entradas a 0, las salidas dejan de cambiar en t={last_k}."
        B[f"f_{prefix}"] = "\n".join([line1, line2, line3, line4])
        return s_, co, v_flag, last_k

    ex1 = example("ex1", 0b0110, 0b0111, bstr(0b0110), bstr(0b0111))
    assert ex1[:3] == (0b1101, 0, 1)
    ex2 = example("ex2", 0x9, 0x8, hstr(0x9), hstr(0x8))
    assert ex2[:3] == (0x1, 1, 1)
    inv3 = (~3) & MASK
    neg3 = (inv3 + 1) & MASK
    assert neg3 == (-3) & MASK and sval(neg3) == -3
    B["f_ex3_neg"] = (
        f"Para escribir -3 en {N} bits: 3 = {bstr(3)}, NOT {bstr(3)} = {bstr(inv3)}, y {bstr(inv3)} + 1 = {bstr(neg3)}."
    )
    ex3 = example("ex3", (-3) & MASK, 5, f"-3 ({bstr((-3) & MASK)})", f"5 ({bstr(5)})")
    assert ex3[:3] == (0b0010, 1, 0)
    seen_cv = {(ex1[1], ex1[2]), (ex2[1], ex2[2]), (ex3[1], ex3[2])}
    missing_cv = set(cat) - seen_cv
    assert len(seen_cv) == 3 and len(missing_cv) == 1 and seen_cv | missing_cv == {(0, 0), (0, 1), (1, 0), (1, 1)}
    (m_c, m_v), = missing_cv
    B["f_ex_summary"] = (
        f"Los tres casos juntos: ({ex1[1]}, {ex1[2]}), ({ex2[1]}, {ex2[2]}) y ({ex3[1]}, {ex3[2]}) para (C, V). "
        f"Con el ({m_c}, {m_v}) de la Tabla 5.8 salen las cuatro combinaciones: "
        f"C y V son preguntas distintas y pueden dar cualquier combinación."
    )

    # ---------------- ejercicio ----------------
    a1, b1 = 0b0101, 0b0100
    a2, b2 = 0b0011, 0b0101
    B["ex05_question"] = (
        f"(a) Con el sumador de {N} bits, calcula {bstr(a1)} + {bstr(b1)}: resultado, C y V, y cómo se lee con signo. "
        f"(b) Con el restador, calcula {bstr(a2)} - {bstr(b2)}: el patrón de NOT B, el resultado, C y V."
    )
    tbl, s_, co, carries = adder_table(A4, N, a1, b1, 0)
    B["ex05_a_bits"] = tbl
    va = carries[N - 1] ^ carries[N]
    assert (s_, co, va) == (0b1001, 0, 1)
    B["f_ex05_a"] = (
        f"(a) {bstr(a1)} + {bstr(b1)} = {bstr(s_)}, C = {co}, V = {va}. Sin signo: {a1} + {b1} = {a1 + b1}, cabe, y {bstr(s_)} = {s_}. "
        f"Con signo: {sval(a1)} + {sval(b1)} = {sval(a1) + sval(b1)} no cabe en -8 a 7, y {bstr(s_)} se lee como {sval(s_)}."
    )
    tbl, s_, co, carries = adder_table(AS4, N, a2, b2, 1, sub=True)
    B["ex05_b_bits"] = tbl
    vb = carries[N - 1] ^ carries[N]
    _, _, _, st_b = run_adder(AS4, N, a2, b2, 1)
    nb = word(AS4, st_b, "Bx", N)
    assert (s_, co, vb) == (0b1110, 0, 0) and nb == 0b1010
    B["f_ex05_b"] = (
        f"(b) NOT {bstr(b2)} = {bstr(nb)}; {bstr(a2)} + {bstr(nb)} + 1 = {bstr(s_)}, C = {co}, V = {vb}. Con signo: {sval(a2)} - ({sval(b2)}) = "
        f"{sval(a2) - sval(b2)} y {bstr(s_)} se lee como {sval(s_)}: correcto, sin desbordamiento. C = 0 indica que, sin signo, "
        f"{a2} < {b2} (hubo que pedir prestado)."
    )
    return B


# ---------------------------------------------------------------------------
# 6. Dibujos ASCII estaticos (sin numeros de datos) y verificacion de los .md
# ---------------------------------------------------------------------------

STATIC04 = {
    "sr_nor_diagram": r"""
         +-----+                     +-----+
  R -----|     |              S -----|     |
         | NOR |----- Q              | NOR |----- Qn
  Qn ----|     |              Q -----|     |
         +-----+                     +-----+
""",
    "sr_nand_diagram": r"""
         +------+                    +------+
  Sn ----|      |             Rn ----|      |
         | NAND |----- Q             | NAND |----- Qn
  Qn ----|      |             Q -----|      |
         +------+                    +------+
""",
    "d_latch_diagram": r"""
          +------+                  +------+
  D ------|      |            a ----|      |
          | NAND |----- a           | NAND |----- Q
  E ------|      |           Qn ----|      |
          +------+                  +------+

          +------+                  +------+
  nD -----|      |            b ----|      |
          | NAND |----- b           | NAND |----- Qn
  E ------|      |            Q ----|      |
          +------+                  +------+

  D ----[NOT]---- nD
""",
    "dff_diagram": r"""
  CLK ---+----------------------------------------+
         |                                        |
       [NOT]                                      |
         |                                        |
        nCLK                                     CLK
         |                                        |
         v                                        v
      +---------------------+              +---------------------+
  D --| latch D MAESTRO     |-- M y Mn ----| latch D ESCLAVO     |-- Q y Qn
      | (abierto con        |              | (abierto con        |
      |  nCLK = 1, CLK = 0) |              |  CLK = 1)           |
      +---------------------+              +---------------------+
""",
    "tff_diagram": r"""
          +-----+         +-------------+
  T ------| XOR |-- D ----|             |
     +----|     |         | flip-flop D |----+---- Q
     |    +-----+  CLK ---|>            |    |
     |                    +-------------+    |
     +---------------------------------------+
""",
}

STATIC05 = {
    "ha_box": r"""
         +---------------+
  A -----|               |----- S  (A XOR B)
         |  semisumador  |
  B -----|               |----- C  (A AND B)
         +---------------+
""",
    "fa_box": r"""
         +---------------+
  A -----|               |
  B -----|   sumador     |----- S
  Cin ---|   completo    |----- Cout
         +---------------+
""",
    "fa_from_ha": r"""
  A, B   ---> [ semisumador 1 ] ---> p (suma parcial) y g (acarreo parcial)
  p, Cin ---> [ semisumador 2 ] ---> S (suma final)   y pc (acarreo parcial)
  g, pc  ---> [ OR ]            ---> Cout
""",
    "addsub_diagram": r"""
           +-----+
  B0 ------| XOR |---- B0' --> entra como B en FA0   (igual con B1, B2 y B3)
  SUB -+---|     |
       |   +-----+
       +--------------------> entra como Cin en FA0
""",
}


def clean_static(txt):
    return "\n".join(l.rstrip() for l in txt.strip("\n").split("\n"))


def rip_diagram_text(n=4, width=11, gap=6):
    """Cadena de n sumadores completos; el acarreo viaja de derecha a izquierda."""
    boxes = []
    x = 6
    for k in range(n):  # k = 0 es el bit n-1 (el de la izquierda)
        boxes.append(x)
        x += width + gap
    total = x + 8
    rows = [[" "] * total for _ in range(7)]

    def put(r, c, text):
        for i, ch in enumerate(text):
            rows[r][c + i] = ch

    for k, x0 in enumerate(boxes):
        bit = n - 1 - k
        va, vb = x0 + 3, x0 + 7
        put(0, va - 1, f"A{bit}")
        put(0, vb - 1, f"B{bit}")
        put(1, va, "|")
        put(1, vb, "|")
        put(2, x0, "+--v---v--+")
        put(3, x0, f"|   FA{bit}   |")
        put(4, x0, "+----+----+")
        put(5, x0 + 5, "|")
        put(6, x0 + 4, f"S{bit}")
        if k > 0:
            put(3, boxes[k - 1] + width, f"<-C{bit + 1}--")
    put(3, 0, "C4 <--")
    put(3, boxes[-1] + width, "<-- Cin")
    return "\n".join("".join(r).rstrip() for r in rows)


STATIC05["rip_diagram"] = rip_diagram_text()
STATIC05["guide_table"] = md_table(
    ["Quieres saber...", "Cómo lees los bits", "Indicador", "Cómo se calcula"],
    [
        ["si la suma no cabe", "sin signo", "C", "C = C4, el acarreo que sale del último bit"],
        ["si la suma no cabe", "con signo (complemento a dos)", "V", "V = C3 ⊕ C4"],
        ["si en la resta hubo que pedir prestado", "sin signo", "C", "C = 0 en una resta hecha con A + NOT B + 1"],
        ["si la resta no cabe", "con signo (complemento a dos)", "V", "V = C3 ⊕ C4, igual que en la suma"],
    ],
)

STATIC04 = {k: clean_static(v) for k, v in STATIC04.items()}
STATIC05 = {k: clean_static(v) for k, v in STATIC05.items()}


# ---------------------------------------------------------------------------
# 7. Rellenar marcadores {{bloque}} y verificar los .md
# ---------------------------------------------------------------------------

import re

LINEWISE_EXTRA = {
    "ha_eq", "fa_eq", "fa_formulas", "twos_formula", "ovf_formula", "char_eqs", "ex05_question",
}


def is_linewise(name):
    return name.startswith("f_") or name in LINEWISE_EXTRA


def all_blocks(which):
    if which == "04":
        B = build04()
        S = STATIC04
        path = DOC04
    else:
        B = build05()
        S = STATIC05
        path = DOC05
    dup = set(B) & set(S)
    assert not dup, dup
    return {**B, **S}, set(S), path


def fill(which):
    blocks, _, path = all_blocks(which)
    md = path.read_text(encoding="utf-8")

    def repl(m):
        name, _, mod = m.group(1).partition("|")
        assert name in blocks, f"bloque desconocido: {name}"
        text = blocks[name]
        if mod == "bullets":
            text = "\n".join("- " + l for l in text.split("\n"))
        else:
            assert mod == "", mod
        return text

    new = re.sub(r"\{\{([A-Za-z0-9_|]+)\}\}", repl, md)
    path.write_text(new, encoding="utf-8")
    print(f"relleno {path.name}: {len(re.findall(r'[{][{]', md))} marcadores")


def extract_fences(md):
    out, cur, inside = [], [], False
    for line in md.split("\n"):
        if line.startswith("```"):
            if inside:
                out.append("\n".join(cur))
                cur = []
            inside = not inside
            continue
        if inside:
            cur.append(line)
    assert not inside, "fence sin cerrar"
    return out


def strip_fences(md):
    out, inside = [], False
    for line in md.split("\n"):
        if line.startswith("```"):
            inside = not inside
            continue
        if not inside:
            out.append(line)
    return "\n".join(out)


def extract_tables(md):
    out, cur = [], []
    for line in strip_fences(md).split("\n"):
        if line.startswith("|"):
            cur.append(line)
        else:
            if cur:
                out.append("\n".join(cur))
            cur = []
    if cur:
        out.append("\n".join(cur))
    return out


TOKEN = re.compile(r"(?<![A-Za-z_0-9])(0[bB][01]+|0[xX][0-9A-Fa-f]+|\d+)")
ALLOW = {
    # 0 y 1 son los valores de un bit; 00, 01, 10 y 11 son las etiquetas de estado de dos bits (todas las posibles);
    # 0b0110 y 0x6 son los ejemplos de notacion de la serie
    "04": {"0", "1", "00", "01", "10", "11", "0b0110", "0x6"},
    # 4 es el ancho de palabra de todo el documento; 0b0110 y 0x6 son los ejemplos de notacion; 0b0110, 0b0111, 0x9, 0x8, 3 y 5 son
    # los operandos que pide el enunciado de los tres ejemplos; 0b1101 y 0b0101 son -3 y 5 en 4 bits
    "05": {"0", "1", "4", "0b0110", "0x6", "0b0111", "0x9", "0x8", "3", "5", "0b1101", "0b0101"},
}


def verify_doc(which, extra_allow=()):
    blocks, static_names, path = all_blocks(which)
    assert path.exists(), f"falta {path.name}"
    md = path.read_text(encoding="utf-8")
    assert "{{" not in md and "}}" not in md, "quedan marcadores sin rellenar"
    n_lines = 0
    for name, text in blocks.items():
        if is_linewise(name):
            for line in text.split("\n"):
                assert line in md, f"[{path.name}] falta la linea del bloque {name}: {line!r}"
                n_lines += 1
        else:
            assert text in md, f"[{path.name}] falta el bloque {name} (completo y contiguo)"
            n_lines += len(text.split("\n"))
    # sentido inverso: todo lo que esta en un cerco o es una tabla sale del generador o es un dibujo estatico
    fence_texts = {t for n, t in blocks.items() if not is_linewise(n) and not t.startswith("|")}
    for f in extract_fences(md):
        assert f in fence_texts, f"[{path.name}] cerco que no sale del generador:\n{f}"
        assert f.isascii(), f"[{path.name}] cerco con caracteres no ASCII:\n{f}"
    table_texts = {t for n, t in blocks.items() if t.startswith("|")}
    n_tables = 0
    for t in extract_tables(md):
        assert t in table_texts, f"[{path.name}] tabla que no sale del generador:\n{t}"
        n_tables += 1
    # numeros en la prosa: solo 0, 1 y los permitidos; lo demas debe venir de un bloque generado
    prose = strip_fences(md)
    facts = sorted({line for name, text in blocks.items() if is_linewise(name) for line in text.split("\n")}, key=len, reverse=True)
    for line in facts:  # primero las lineas largas, para que una ecuacion corta no parta una frase generada
        prose = prose.replace(line, " ")
    lines = []
    for line in prose.split("\n"):
        if line.startswith("|") or line.startswith("#"):
            continue
        lines.append(line)
    allow = ALLOW[which] | set(extra_allow)
    bad = []
    for line in lines:
        if "minecraft.wiki" in line:  # cifras citadas de fuentes externas (se citan con su enlace)
            continue
        line = re.sub(r"^\s*\d+\. ", " ", line)  # numeracion de listas
        line = re.sub(r"(Tabla|Tablas|Figura) \d+\.\d+|Paso \d+|Fase \d+|documento \d+|Documento \d+", " ", line)
        for m in TOKEN.finditer(line):
            if m.group(1) not in allow:
                bad.append((m.group(1), line.strip()))
    assert not bad, f"[{path.name}] numeros en prosa que no vienen de un bloque generado: {bad[:12]}"
    return len(blocks), n_lines, n_tables, len(extract_fences(md))


def dump(which):
    blocks, _, _ = all_blocks(which)
    for name, text in blocks.items():
        print(f"===== {name}")
        print(text)


def main(argv):
    import random

    if len(argv) >= 2 and argv[1] == "--dump":
        dump(argv[2])
        return 0
    if len(argv) >= 2 and argv[1] == "--fill":
        fill(argv[2])
        return 0
    rng = random.Random(5)
    circuits = [RING, SR_NOR, SR_NAND, D_LATCH, DFF, TFF, HA, FA, FA_NAND, make_adder(4), make_adder(4, "nand"), make_adder(4, sub=True)]
    for c in circuits:
        c.self_check(rng)
    print(f"simulador: version rapida == version de dos fases en {len(circuits)} circuitos")
    for which, doc in (("04", DOC04), ("05", DOC05)):
        nb, nl, nt, nf = verify_doc(which)
        print(f"OK {doc.name}: {nb} bloques generados verificados ({nl} lineas), {nt} tablas y {nf} cercos de codigo sin huerfanos, prosa sin numeros inventados")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
