#!/usr/bin/env python3
"""check_C.py  --  clave C: documentos 06 (relojes y contadores) y 07 (maquinas de estado).

Que contiene:
  1. Un simulador didactico con el modelo temporal del curso: retardo unitario y
     evaluacion sincrona en dos fases (fase 1: calcular todo leyendo solo el instante t;
     fase 2: confirmar todo a la vez como instante t+1).
  2. Todas las funciones que GENERAN las tablas, trazas, formas de onda y cifras del texto.
  3. Las plantillas de los dos .md (la prosa; cada numero sale de un {{marcador}}).
  4. Un verificador que relee los .md del disco, regenera cada bloque y comprueba con
     assert que aparece literalmente.
  5. El flip-flop T construido con puertas del documento 04 (GATE_TFF), montado con el mismo
     simulador `Net`: con el se miden los periodos minimos de los montajes del 06 frente al
     flip-flop ideal, y se dibujan los mapas de Karnaugh (con casillas X) del 07.

Uso:
  python3 -W error check_C.py          verifica los .md del disco (exit 0 si todo coincide)
  python3 -W error check_C.py --write  (re)genera los .md a partir de las plantillas
  python3 -W error check_C.py --dump   imprime los valores generados (depuracion)
"""
import sys

sys.dont_write_bytecode = True

import itertools
import pathlib
import random
import re

HERE = pathlib.Path(__file__).resolve().parent
DOC06 = HERE / "06-relojes-y-contadores.md"
DOC07 = HERE / "07-maquinas-de-estado.md"

# ---------------------------------------------------------------------------
# 1. Simulador: retardo unitario + evaluacion sincrona en dos fases
# ---------------------------------------------------------------------------

OPS = {
    "NOT": lambda a: 1 - a,
    "BUF": lambda a: a,
    "AND": lambda *a: int(all(a)),
    "OR": lambda *a: int(any(a)),
    "XOR": lambda *a: sum(a) % 2,
    "NAND": lambda *a: 1 - int(all(a)),
    "NOR": lambda *a: 1 - int(any(a)),
    "XNOR": lambda *a: 1 - (sum(a) % 2),
}


class Net:
    """Circuito de elementos con retardo unitario.

    Elementos:
      src  : fuente externa, valor = f(t) (temporizador, entrada de prueba, constante)
      gate : salida(t+1) = op(entradas(t))
      tff  : flip-flop T. Q(t+1) = Q(t) XOR T(t) si hay flanco de subida; ofrece Q y Qn
      dff  : flip-flop D. Q(t+1) = D(t) si hay flanco de subida
    Flanco de subida: rise(t) = CLK(t) AND NOT CLK(t-1). Cada flip-flop guarda dentro
    el valor anterior del reloj.
    """

    def __init__(self):
        self.order = []
        self.el = {}

    def _add(self, name, spec):
        assert name not in self.el, name
        self.order.append(name)
        self.el[name] = spec

    def src(self, name, fn, desc="ENTRADA"):
        self._add(name, dict(kind="src", fn=fn, desc=desc))

    def gate(self, name, op, ins, init=0):
        assert op in OPS, op
        self._add(name, dict(kind="gate", op=op, ins=tuple(ins), init=init))

    def tff(self, name, tin, clk, init=0):
        self._add(name, dict(kind="tff", tin=tin, clk=clk, init=init))

    def dff(self, name, d, clk, init=0):
        self._add(name, dict(kind="dff", d=d, clk=clk, init=init))

    def describe(self, only=None):
        """Lista de elementos del circuito (la misma que se simula)."""
        rows = []
        for name in self.order:
            if only is not None and name not in only:
                continue
            e = self.el[name]
            k = e["kind"]
            if k == "src":
                rows.append((name, e["desc"]))
            elif k == "gate":
                rows.append((name, f"{e['op']}({', '.join(e['ins'])})" + (f"   arranca en {e['init']}" if e["init"] else "")))
            elif k == "tff":
                rows.append((name, f"FF-T(T={e['tin']}, reloj={e['clk']})   salidas: {name}, {name}n"))
            else:
                rows.append((name, f"FF-D(D={e['d']}, reloj={e['clk']})   salidas: {name}, {name}n"))
        w = max(len(r[0]) for r in rows)
        return "\n".join(f"{n.ljust(w)} = {d}" for n, d in rows)

    def initial(self):
        snap, hid = {}, {}
        for name in self.order:
            e = self.el[name]
            if e["kind"] == "src":
                snap[name] = e["fn"](0)
            elif e["kind"] == "gate":
                snap[name] = e["init"]
            else:
                snap[name] = e["init"]
                snap[name + "n"] = 1 - e["init"]
        for name in self.order:  # el reloj "anterior" de t=0 se toma igual al de t=0: sin flanco falso
            e = self.el[name]
            if e["kind"] in ("tff", "dff"):
                hid[name] = snap[e["clk"]]
        return snap, hid

    def step(self, snap, hid, t, order=None):
        """Fase 1: calcular todo leyendo SOLO `snap` (instante t). Fase 2: confirmar a la vez."""
        nxt, nh = {}, {}
        for name in (order or self.order):  # fase 1
            e = self.el[name]
            k = e["kind"]
            if k == "src":
                nxt[name] = e["fn"](t + 1)
            elif k == "gate":
                nxt[name] = OPS[e["op"]](*[snap[s] for s in e["ins"]])
            else:
                clk = snap[e["clk"]]
                rise = clk == 1 and hid[name] == 0
                q = snap[name]
                if k == "tff":
                    q2 = q ^ snap[e["tin"]] if rise else q
                else:
                    q2 = snap[e["d"]] if rise else q
                nxt[name] = q2
                nxt[name + "n"] = 1 - q2
                nh[name] = clk
        return nxt, nh  # fase 2: el llamador sustituye snap/hid de golpe

    def run(self, T, order=None):
        """Devuelve {senal: [valor en t=0..T-1]}."""
        snap, hid = self.initial()
        out = {k: [v] for k, v in snap.items()}
        for t in range(T - 1):
            snap, hid = self.step(snap, hid, t, order)
            for k, v in snap.items():
                out[k].append(v)
        return out


def check_b():
    """Carga check_B.py (documentos 04 y 05) como modulo: de el salen los minimos de reloj del flip-flop T de puertas
    que el 06 cita, para que los dos documentos digan lo mismo. check_B.py comprueba a su vez su cifra con T = 1 contra la
    de este script (`gt_div_P`)."""
    import importlib.util

    if "check_B" not in sys.modules:
        spec = importlib.util.spec_from_file_location("check_B", HERE / "check_B.py")
        mod = importlib.util.module_from_spec(spec)
        sys.modules["check_B"] = mod
        spec.loader.exec_module(mod)
    return sys.modules["check_B"]


# Flip-flop T construido con puertas. Es el del documento 04 (Pasos 6 y 8): una XOR (D = T xor Q) delante de
# un flip-flop D maestro-esclavo de 10 puertas (2 NOT y 8 NAND). Se monta aqui con `Net`, que tiene el mismo
# modelo temporal (retardo unitario, dos fases) que el simulador de ese documento.
GATE_TFF = [  # (salida, puerta, entradas); T y CLK son las entradas del flip-flop, Q y Qn sus salidas
    ("D", "XOR", ("T", "Q")),
    ("nCLK", "NOT", ("CLK",)),
    ("nD", "NOT", ("D",)),
    ("a1", "NAND", ("D", "nCLK")),
    ("b1", "NAND", ("nD", "nCLK")),
    ("M", "NAND", ("a1", "Mn")),
    ("Mn", "NAND", ("b1", "M")),
    ("a2", "NAND", ("M", "CLK")),
    ("b2", "NAND", ("Mn", "CLK")),
    ("Q", "NAND", ("a2", "Qn")),
    ("Qn", "NAND", ("b2", "Q")),
]


def add_gate_tff(net, name, tin, clk, init=None):
    """Anade el flip-flop T de puertas. Salidas: `name` (Q) y `name`+"n" (Qn); internas: `name`_<local>."""

    def sig(local):
        return {"T": tin, "CLK": clk, "Q": name, "Qn": name + "n"}.get(local, f"{name}_{local}")

    for out, op, ins in GATE_TFF:
        net.gate(sig(out), op, [sig(i) for i in ins], init=(init or {}).get(out, 0))


_GATE_INIT = {}


def gate_tff_init(t0, c0):
    """Estado quieto del flip-flop de puertas con Q=0 cuando T vale t0 y CLK vale c0 (arranque coherente).
    Como en el documento 04: el nucleo D (sin la XOR) arranca desde todo a 0 con D=0 y 6 pasos con CLK=0,
    6 con CLK=1 y 6 con CLK=0, y queda en Q=0. Despues se monta el flip-flop T completo con ese estado,
    se pone el reloj a c0 (con T=0 no hay cambio) y T a t0, y se espera a que quede quieto."""
    key = (t0, c0)
    if key not in _GATE_INIT:
        core = Net()
        core.src("CLK", lambda t: 1 if 6 <= t < 12 else 0)
        core.src("D", lambda t: 0)
        for out, op, ins in GATE_TFF:
            if out != "D":
                core.gate(out, op, ins)
        oc = core.run(30)
        assert all(v[-1] == v[-2] for v in oc.values()), "el nucleo D no queda quieto"
        assert (oc["Q"][-1], oc["Qn"][-1]) == (0, 1)
        start = {out: oc[out][-1] for out, _, _ in GATE_TFF if out != "D"}
        start["D"] = 0
        net = Net()
        net.src("CLK", lambda t: c0)
        net.src("T", lambda t: 0 if t < 12 else t0)
        add_gate_tff(net, "P", "T", "CLK", start)
        o = net.run(50)
        assert all(v[-1] == v[-2] for v in o.values()), "el flip-flop de puertas no queda quieto"
        assert (o["P"][-1], o["Pn"][-1]) == (0, 1)
        loc = {"Q": "P", "Qn": "Pn"}
        _GATE_INIT[key] = {out: o[loc.get(out, "P_" + out)][-1] for out, _, _ in GATE_TFF}
    return _GATE_INIT[key]


def tff_cell(net, ff, name, tin, clk, t0, c0):
    """Un flip-flop T: ideal (pieza primitiva) o de puertas. t0 y c0 = valores de T y del reloj en t=0."""
    if ff == "ideal":
        net.tff(name, tin, clk)
    else:
        assert ff == "gate", ff
        add_gate_tff(net, name, tin, clk, gate_tff_init(t0, c0))


def timer_desc(P, H):
    return f"TEMPORIZADOR(P={P}, H={H})"


def timer_fn(P, H):
    """Temporizador: contador interno c(t+1)=(c+1) mod P; salida 1 si c >= P-H."""
    return lambda t: int((t % P) >= P - H)


def seq_fn(seq, last=None):
    """Entrada de prueba: seq[t]; pasado el final, mantiene el ultimo valor (o `last`)."""
    fin = seq[-1] if last is None else last
    return lambda t: seq[t] if t < len(seq) else fin


# ---------------------------------------------------------------------------
# 2. Formato: ondas ASCII, reglas, tablas
# ---------------------------------------------------------------------------


def wave(vals):
    return "".join("#" if v else "_" for v in vals)


def bus(vals):
    out, prev = [], None
    for v in vals:
        out.append(f"{v:X}" if v != prev else ".")
        prev = v
    return "".join(out)


def diagram(rows, T, lw=9, start=0, axis="t"):
    """rows = [(etiqueta, cadena_de_T_caracteres)]. Regla de pasos arriba."""
    l1 = " " * lw + "".join(str(((start + t) // 10) % 10) if (start + t) % 10 == 0 else " " for t in range(T))
    l2 = axis.ljust(lw) + "".join(str((start + t) % 10) for t in range(T))
    lines = [l1.rstrip(), l2]
    for label, s in rows:
        assert len(s) == T, (label, len(s), T)
        lines.append((label.ljust(lw) + s).rstrip())
    return "\n".join(lines)


def table(headers, rows, align=None):
    """Tabla markdown. Cada fila es una linea; el verificador exige cada una literalmente."""
    ncol = len(headers)
    assert all(len(r) == ncol for r in rows)
    align = align or ["---"] * ncol
    lines = ["| " + " | ".join(headers) + " |", "|" + "|".join(f" {a} " for a in align) + "|"]
    for r in rows:
        lines.append("| " + " | ".join(str(c) for c in r) + " |")
    return "\n".join(lines)


def bstr(v, n):
    return f"0b{v:0{n}b}"


def hx(v):
    return f"0x{v:X}"


def pct(num, den):
    assert (100 * num) % den == 0, (num, den)
    return f"{100 * num // den} %"


def rises(v):
    return [t for t in range(1, len(v)) if v[t] == 1 and v[t - 1] == 0]


def falls(v):
    return [t for t in range(1, len(v)) if v[t] == 0 and v[t - 1] == 1]


def measure(v):
    """(periodo, ticks_en_alto) de una senal periodica; exige que todos los periodos coincidan."""
    r, f = rises(v), falls(v)
    assert len(r) >= 3 and len(f) >= 2, "senal demasiado corta"
    ds = {b - a for a, b in zip(r, r[1:])}
    assert len(ds) == 1, ds
    hs = set()
    for a in r:
        nf = [x for x in f if x > a]
        if nf:
            hs.add(nf[0] - a)
    assert len(hs) == 1, hs
    return ds.pop(), hs.pop()


# ---------------------------------------------------------------------------
# 3. Documento 06: generadores
# ---------------------------------------------------------------------------


def ring_next(s):
    """Anillo de n NOT, retardo unitario: g_i(t+1) = NOT g_{i-1}(t); g_1 realimentado desde g_n."""
    n = len(s)
    return tuple(1 - s[(i - 1) % n] for i in range(n))


def orbit_len(s):
    seen, t = {}, 0
    while s not in seen:
        seen[s] = t
        s = ring_next(s)
        t += 1
    return t - seen[s]


def ring_net(n, init):
    net = Net()
    for i in range(n):
        net.gate(f"g{i + 1}", "NOT", [f"g{(i - 1) % n + 1}"], init=init[i])
    return net


def settled_chain(n, k=0):
    """Cadena abierta (entrada fija a 0) de n NOT + k BUF, ya estabilizada. Devuelve sus valores."""
    vals = []
    prev = 0
    for i in range(n):
        prev = 1 - prev
        vals.append(prev)
    return vals + [prev] * k  # los BUF copian el valor de g_n


def loop_net(n, k):
    """Anillo de n NOT seguidos de k BUF, arrancado desde la cadena abierta estabilizada."""
    init = settled_chain(n, k)
    net = Net()
    names = [f"g{i + 1}" for i in range(n)] + [f"b{i + 1}" for i in range(k)]
    for i, nm in enumerate(names):
        src = names[i - 1]
        net.gate(nm, "NOT" if nm.startswith("g") else "BUF", [src], init=init[i])
    return net, names


def sig_period(v, warm=0):
    """Menor p>0 tal que v[t+p]==v[t] para todo t>=warm."""
    for p in range(1, len(v) // 2):
        if all(v[t + p] == v[t] for t in range(warm, len(v) - p)):
            return p
    raise AssertionError("sin periodo")


def counter_net(n, mode, P, H, ff="ideal"):
    """Contador de n bits. ff = "ideal" (pieza primitiva, retardo 1) o "gate" (flip-flop T de puertas del documento 04).
    tff_cell recibe el valor inicial de T y del reloj de cada etapa (solo lo usa el flip-flop de puertas)."""
    net = Net()
    net.src("CLK", timer_fn(P, H), timer_desc(P, H))
    net.src("ONE", lambda t: 1, "CONSTANTE 1")
    if mode == "ripple":
        for k in range(n):
            tff_cell(net, ff, f"R{k}", "ONE", "CLK" if k == 0 else f"R{k - 1}n", 1, 0 if k == 0 else 1)
    elif mode == "ripple_q":  # reloj de cada etapa = Q (no Qn) de la anterior
        for k in range(n):
            tff_cell(net, ff, f"R{k}", "ONE", "CLK" if k == 0 else f"R{k - 1}", 1, 0)
    elif mode == "sync":  # T_k = Q0 AND ... AND Q_{k-1}, una puerta AND ancha por etapa
        for k in range(n):
            if k == 0:
                tin = "ONE"
            elif k == 1:
                tin = "S0"
            else:
                net.gate(f"A{k}", "AND", [f"S{j}" for j in range(k)])
                tin = f"A{k}"
            tff_cell(net, ff, f"S{k}", tin, "CLK", 1 if k == 0 else 0, 0)
    elif mode == "sync2":  # T_k con ANDs de 2 entradas en cadena
        for k in range(n):
            if k == 0:
                tin = "ONE"
            elif k == 1:
                tin = "S0"
            elif k == 2:
                net.gate("A2", "AND", ["S0", "S1"])
                tin = "A2"
            else:
                net.gate(f"A{k}", "AND", [f"A{k - 1}", f"S{k - 1}"])
                tin = f"A{k}"
            tff_cell(net, ff, f"S{k}", tin, "CLK", 1 if k == 0 else 0, 0)
    else:
        raise ValueError(mode)
    return net


def qnames(n, mode):
    return [("R" if mode.startswith("ripple") else "S") + str(k) for k in range(n)]


def code_at(out, names, t):
    return sum(out[nm][t] << k for k, nm in enumerate(names))


def edge_ticks(P, H, T):
    t0 = P - H
    return list(range(t0, T, P))


def readings(n, mode, P, H, nedges, ff="ideal"):
    T = (P - H) + nedges * P + 2
    out = counter_net(n, mode, P, H, ff).run(T)
    names = qnames(n, mode)
    return [code_at(out, names, t) for t in edge_ticks(P, H, T)][:nedges]


def counter_ok(n, mode, P, H, ff="ideal"):
    ne = 2**n + 4
    exp = [j % 2**n for j in range(ne)]
    return readings(n, mode, P, H, ne, ff) == exp


def pmin(n, mode):
    """Menor periodo P (H=P//2) a partir del cual todas las lecturas son correctas hasta Pmax."""
    pmax = 2 * n + 8
    ok = {P: counter_ok(n, mode, P, max(1, P // 2)) for P in range(2, pmax + 1)}
    first = min(P for P in ok if ok[P])
    assert all(ok[P] for P in range(first, pmax + 1)), (n, mode, ok)
    return first, ok


def pmin_gate(n, mode):
    """Menor P (H = P//2) con lecturas correctas usando el flip-flop T de puertas; los 4 periodos siguientes tambien."""
    P = 2
    while not counter_ok(n, mode, P, max(1, P // 2), "gate"):
        P += 1
        assert P <= 8 * n + 20, (n, mode)
    for Q in range(P + 1, P + 5):
        assert counter_ok(n, mode, Q, max(1, Q // 2), "gate"), (n, mode, P, Q)
    return P


def divider_ok(P, H, stages, ff):
    """Cada etapa debe dar exactamente el doble de periodo que la anterior (periodos y anchos uniformes)."""
    o = divider_net(P, H, stages, ff).run(12 * P * 2**stages)
    try:
        return [measure(o[f"Z{k}"])[0] for k in range(1, stages + 1)] == [P * 2**k for k in range(1, stages + 1)]
    except AssertionError:
        return False


def pmin_divider(stages, ff):
    P = 2
    while not divider_ok(P, max(1, P // 2), stages, ff):
        P += 1
        assert P <= 40, ff
    for Q in range(P + 1, P + 5):
        assert divider_ok(Q, max(1, Q // 2), stages, ff), (ff, P, Q)
    return P


def beat_net(P, H, ff):
    """El «latido»: divisor de 3 etapas + detector de flanco de subida + alargador a 4 pasos."""
    net = Net()
    net.src("CLK", timer_fn(P, H), timer_desc(P, H))
    net.src("ONE", lambda t: 1, "CONSTANTE 1")
    for k in range(3):
        tff_cell(net, ff, f"Z{k + 1}", "ONE", "CLK" if k == 0 else f"Z{k}", 1, 0)
    net.gate("nZ3", "NOT", ["Z3"], init=1)
    net.gate("PULSO", "AND", ["Z3", "nZ3"], init=0)
    net.gate("b1", "BUF", ["PULSO"], init=0)
    net.gate("b2", "BUF", ["b1"], init=0)
    net.gate("b3", "BUF", ["b2"], init=0)
    net.gate("ANCHO", "OR", ["PULSO", "b1", "b2", "b3"], init=0)
    return net


def beat_ok(P, ff):
    """Pulsos de 1 paso separados exactamente 8 periodos de reloj."""
    o = beat_net(P, max(1, P // 2), ff).run(P * 8 * 6)
    pr = rises(o["PULSO"])
    return len(pr) >= 4 and all(b - a == 8 * P for a, b in zip(pr, pr[1:])) and all(sum(o["PULSO"][t : t + 3]) == 1 for t in pr[:-1])


def pmin_beat(ff):
    P = 2
    while not beat_ok(P, ff):
        P += 1
        assert P <= 40, ff
    for Q in range(P + 1, P + 5):
        assert beat_ok(Q, ff), (ff, P, Q)
    return P


def edge_net(kind, k, xfn):
    """Detector de flancos. nx = NOT(x retrasada k pasos extra). kind: rise|fall|any."""
    net = Net()
    net.src("x", xfn, "ENTRADA")
    prev = "x"
    for i in range(k):
        net.gate(f"d{i + 1}", "BUF", [prev], init=0)
        prev = f"d{i + 1}"
    net.gate("nx", "NOT", [prev], init=1)
    op = dict(rise="AND", fall="NOR", any="XNOR")[kind]
    net.gate("out", op, ["x", "nx"], init=0)
    return net


def edge_all_net(xfn):
    net = Net()
    net.src("x", xfn, "ENTRADA")
    net.gate("nx", "NOT", ["x"], init=1)
    net.gate("SUBIDA", "AND", ["x", "nx"])
    net.gate("BAJADA", "NOR", ["x", "nx"])
    net.gate("AMBOS", "XNOR", ["x", "nx"])
    return net


def divider_net(Pc, Hc, stages, ff="ideal"):
    """Divisor de frecuencia: etapas Z1, Z2, ... (se numeran desde 1 y no se llaman Q para no chocar con los bits Q0..Q3 del contador)."""
    net = Net()
    net.src("CLK", timer_fn(Pc, Hc), timer_desc(Pc, Hc))
    net.src("ONE", lambda t: 1, "CONSTANTE 1")
    for k in range(stages):
        tff_cell(net, ff, f"Z{k + 1}", "ONE", "CLK" if k == 0 else f"Z{k}", 1, 0)
    return net


def stretch_net(N, xfn):
    net = Net()
    net.src("x", xfn, "ENTRADA")
    prev = "x"
    for i in range(N - 1):
        net.gate(f"b{i + 1}", "BUF", [prev], init=0)
        prev = f"b{i + 1}"
    net.gate("ANCHO", "OR", ["x"] + [f"b{i + 1}" for i in range(N - 1)], init=0)
    return net


def monostable(xs, N):
    """Monoestable reactivable con contador: c(t+1)=N si x(t) sino max(c-1,0); salida(t)=[c(t)>0]."""
    c, out = 0, []
    for x in xs:
        out.append(int(c > 0))
        c = N if x else max(c - 1, 0)
    return out


def make_ops_table():
    a_, b_ = 1, 0
    return table(
        ["Operación", "En fórmulas", f"Ejemplo con a = {bstr(a_, 1)} y b = {bstr(b_, 1)}"],
        [
            ["AND", "a · b", f"{bstr(OPS['AND'](a_, b_), 1)}"],
            ["OR", "a + b", f"{bstr(OPS['OR'](a_, b_), 1)}"],
            ["NOT", "¬a", f"{bstr(OPS['NOT'](a_), 1)}"],
            ["XOR", "a ⊕ b", f"{bstr(OPS['XOR'](a_, b_), 1)}"],
            [
                "NAND / NOR / XNOR",
                "¬(a · b) / ¬(a + b) / ¬(a ⊕ b)",
                f"{bstr(OPS['NAND'](a_, b_), 1)} / {bstr(OPS['NOR'](a_, b_), 1)} / {bstr(OPS['XNOR'](a_, b_), 1)}",
            ],
        ],
        [":---", ":---", ":---:"],
    )


STATIC = {
    "static_ring_sketch": """      +--------------------------------------------+
      |                                            |
      |     +-----+       +-----+       +-----+    |
      +---->| NOT |------>| NOT |------>| NOT |----+
            +-----+       +-----+       +-----+
              g1            g2            g3""",
    "static_fsm_block": """                  +-------------------------------+
   x(t) --------->|                               |
                  |     LOGICA COMBINACIONAL      |--------> Y(t)
      +---------->|   (puertas AND, OR, NOT)      |
      |           +-------------------------------+
      |                          |
      |                          |  D = S(t+1)  (estado siguiente)
      |                          v
      |           +-------------------------------+
      |           |   REGISTRO: k flip-flops D    |<------- CLK
      |           +-------------------------------+
      |                          |
      |                          |  Q = S(t)  (estado actual)
      +--------------------------+""",
}


def notation_keys():
    """Literales de ejemplo para la regla «tantos digitos como bits tenga la senal»."""
    return {
        "nt_lit1": bstr(1, 1),
        "nt_lit2": bstr(1, 2),
        "nt_lit3": bstr(5, 3),
        "nt_lit4": bstr(6, 4),
        "nt_n2": 2,
        "nt_n3": 3,
        "nt_n4": 4,
    }


def values06():
    V = dict(STATIC)
    # ---- 2. Modelo temporal: dos NOT en cadena y las dos fases ----------------------
    xs = [0, 0, 0, 1, 1, 1, 0, 0, 0, 0]
    net = Net()
    net.src("x", seq_fn(xs), "ENTRADA")
    net.gate("a", "NOT", ["x"], init=1)
    net.gate("b", "NOT", ["a"], init=0)
    o = net.run(len(xs))
    V["net_two_not"] = net.describe()
    V["two_not_table"] = table(
        ["t", "x(t)", "a(t)", "b(t)"],
        [[t, o["x"][t], o["a"][t], o["b"][t]] for t in range(len(xs))],
        ["---:"] * 4,
    )
    # una transicion explicada en dos fases (t=3 -> t=4)
    t0 = 3
    snap = {k: v[t0] for k, v in o.items()}
    a_new, b_new = 1 - snap["x"], 1 - snap["a"]
    V["two_phase_t"] = t0
    V["two_phase_snap"] = f"x={snap['x']} a={snap['a']} b={snap['b']}"
    V["two_phase_p1"] = f"a' = NOT x = NOT {snap['x']} = {a_new} ; b' = NOT a = NOT {snap['a']} = {b_new}"
    V["two_phase_p2"] = f"a={o['a'][t0 + 1]} b={o['b'][t0 + 1]}"
    assert (a_new, b_new) == (o["a"][t0 + 1], o["b"][t0 + 1])
    V["two_phase_block"] = "\n".join(
        [
            f"Instante t={t0}  (foto):  {V['two_phase_snap']}",
            f"Fase 1 (solo LEE la foto de t={t0}):  {V['two_phase_p1']}",
            f"Fase 2 (ESCRIBE todo a la vez):  instante t={t0 + 1}  ->  {V['two_phase_p2']}",
        ]
    )
    # el orden de evaluacion no importa
    rng = random.Random(6)
    for _ in range(30):
        order = net.order[:]
        rng.shuffle(order)
        assert net.run(len(xs), order=order) == o
    V["order_trials"] = 30
    # ---- 3. Oscilador en anillo ------------------------------------------------------
    # 3.1 traza n=3 desde la cadena estabilizada
    n = 3
    init = settled_chain(n)
    rnet = ring_net(n, init)
    T = 16
    ro = rnet.run(T)
    V["net_ring3"] = rnet.describe()
    # la red simulada coincide con la funcion pura
    s = tuple(init)
    for t in range(T):
        assert tuple(ro[f"g{i + 1}"][t] for i in range(n)) == s
        s = ring_next(s)
    V["ring3_init"] = bstr(sum(b << (n - 1 - i) for i, b in enumerate(init)), n)
    V["ring3_init_note"] = "g1=%d g2=%d g3=%d" % tuple(init)
    V["ring3_table"] = table(
        ["t", "g1", "g2", "g3"],
        [[t] + [ro[f"g{i + 1}"][t] for i in range(n)] for t in range(T)],
        ["---:"] * 4,
    )
    V["ring3_wave"] = diagram([(f"g{i + 1}", wave(ro[f"g{i + 1}"])) for i in range(n)], T)
    p3, h3 = measure(rnet.run(48)["g1"])
    V["ring3_period"], V["ring3_high"] = p3, h3
    # cada nodo es el anterior invertido y retrasado 1 paso
    for i in range(n):
        for t in range(T - 1):
            assert ro[f"g{(i + 1) % n + 1}"][t + 1] == 1 - ro[f"g{i + 1}"][t]
    # 3.2 periodo para n impar desde la cadena estabilizada + ciclo de trabajo
    rows = []
    ring_periods = {}
    for n in (1, 3, 5, 7, 9):
        init = settled_chain(n)
        o2 = ring_net(n, init).run(8 * n + 8)
        p = sig_period(o2["g1"])
        assert p == 2 * n
        hi = sum(o2["g1"][:p])
        assert hi * 2 == p
        # un unico defecto: solo g1 inestable en t=0
        unstable = [i for i in range(n) if init[i] == init[(i - 1) % n]]
        assert unstable == [0]
        rows.append([n, n, 2 * n, p, hi, p - hi])
        ring_periods[n] = p
    V["ring_period_table"] = table(
        ["n (NOT)", "retardo total", "periodo previsto 2n", "periodo simulado", "pasos en 1", "pasos en 0"],
        rows,
        ["---:"] * 6,
    )
    # 3.3 todos los estados iniciales
    rows = []
    hist_all = {}
    for n in (1, 3, 5, 7, 9):
        cnt = {}
        for s in itertools.product((0, 1), repeat=n):
            p = orbit_len(s)
            cnt[p] = cnt.get(p, 0) + 1
        hist_all[n] = cnt
        for p in sorted(cnt):
            rows.append([n, 2**n, p, cnt[p], "sí" if p == 2 * n else "no"])
        assert sum(cnt.values()) == 2**n
    V["ring_allstates_table"] = table(
        ["n", "estados iniciales posibles", "periodo", "cuántos estados lo dan", "es 2n?"],
        rows,
        ["---:", "---:", "---:", "---:", ":---:"],
    )
    V["ring_hist"] = hist_all
    # los estados con periodo 2: todo igual
    for n in (1, 3, 5, 7, 9):
        for v in (0, 1):
            assert orbit_len(tuple([v] * n)) == 2
    # n=3: tabla de los 8 estados
    rows = []
    for s in itertools.product((0, 1), repeat=3):
        unst = sum(1 for i in range(3) if s[i] == s[(i - 1) % 3])
        rows.append([bstr(sum(b << (2 - i) for i, b in enumerate(s)), 3), unst, orbit_len(s)])
    V["ring3_all8_table"] = table(
        ["estado inicial g1 g2 g3", "puertas inestables", "periodo"], rows, [":---:", "---:", "---:"]
    )
    # all-zero trace (n=3), periodo 2
    z = ring_net(3, [0, 0, 0]).run(8)
    V["ring3_zero_wave"] = diagram([(f"g{i + 1}", wave(z[f"g{i + 1}"])) for i in range(3)], 8)
    # pares: n=2
    fixed = [s for s in itertools.product((0, 1), repeat=2) if ring_next(s) == s]
    V["ring2_fixed"] = ", ".join(bstr(s[0] * 2 + s[1], 2) for s in fixed)
    V["ring2_period_00"] = orbit_len((0, 0))
    assert len(fixed) == 2
    # 3.4 retardo total D => periodo 2D (NOT + BUF)
    rows = []
    for n, k in [(1, 0), (1, 1), (1, 2), (1, 3), (3, 0), (3, 1), (3, 2), (5, 1)]:
        net2, names = loop_net(n, k)
        o2 = net2.run(12 * (n + k))
        p = sig_period(o2["g1"])
        D = n + k
        assert p == 2 * D
        rows.append([n, k, D, p, f"{p // 2} en 1 / {p // 2} en 0"])
    V["loop_delay_table"] = table(
        ["NOT", "BUF (retardo)", "retardo total D", "periodo simulado", "forma"], rows, ["---:", "---:", "---:", "---:", ":---"]
    )
    # ---- 4. Reloj con temporizador ----------------------------------------------------
    rows, waves = [], []
    V["timer_cfgs"] = [(8, 4), (8, 2), (4, 2), (2, 1)]
    for P, H in V["timer_cfgs"]:
        net3 = Net()
        net3.src("CLK", timer_fn(P, H), timer_desc(P, H))
        o3 = net3.run(24)
        p, h = measure(o3["CLK"]) if P <= 8 else (None, None)
        assert (p, h) == (P, H)
        # contador interno explicito
        c, vals = 0, []
        for t in range(24):
            vals.append(int(c >= P - H))
            c = (c + 1) % P
        assert vals == o3["CLK"]
        rows.append([P, H, pct(H, P), f"1/{P}"])
        waves.append((f"P={P} H={H}", wave(o3["CLK"])))
    V["timer_table"] = table(
        ["periodo P (pasos)", "pasos en alto H", "ciclo de trabajo H/P", "frecuencia (ciclos por paso)"], rows, ["---:"] * 4
    )
    V["timer_wave"] = diagram(waves, 24, lw=10)
    # ---- 5. Divisor de frecuencia -----------------------------------------------------
    def divider(Pc, Hc, stages, T):
        return divider_net(Pc, Hc, stages).run(T)

    V["net_divider"] = divider_net(4, 2, 3).describe()
    o4 = divider(4, 2, 3, 48)
    V["div_wave"] = diagram([("CLK", wave(o4["CLK"]))] + [(f"Z{k}", wave(o4[f"Z{k}"])) for k in (1, 2, 3)], 48)
    rows = []
    o4L = divider(4, 2, 3, 240)
    base = measure(o4L["CLK"])[0]
    for nm in ("CLK", "Z1", "Z2", "Z3"):
        p, h = measure(o4L[nm])
        rows.append([nm, p, f"{p // base}", f"{h} / {p - h}", pct(h, p)])
    V["div_table"] = table(
        ["señal", "periodo (pasos)", "veces el periodo de CLK", "pasos en 1 / en 0", "ciclo de trabajo"],
        rows,
        [":---", "---:", "---:", ":---:", "---:"],
    )
    V["div_periods"] = [measure(o4L[k])[0] for k in ("CLK", "Z1", "Z2", "Z3")]
    o4b = divider(4, 1, 1, 64)
    pq, hq = measure(o4b["Z1"])
    pc, hc = measure(o4b["CLK"])
    V["div_duty_in"] = pct(hc, pc)
    V["div_duty_out"] = pct(hq, pq)
    V["div_duty_in_ticks"] = f"{hc} de cada {pc}"
    V["div_duty_out_ticks"] = f"{hq} de cada {pq}"
    assert 2 * hq == pq
    # ---- 6. Contadores ----------------------------------------------------------------
    n = 4
    V["count_table"] = table(
        ["cuenta", "binario (Q3 Q2 Q1 Q0)", "hex"], [[v, bstr(v, 4), hx(v)] for v in range(16)], ["---:", ":---:", ":---:"]
    )
    rows = []
    for k in range(4):
        rows.append([f"Q{k}", 2**k, 2 ** (k + 1), "1" if k == 0 else f"Q{k - 1} pasa de 1 a 0"])
    V["bit_period_table"] = table(
        ["bit", "cambia cada ... cuentas", "periodo (en cuentas)", "cambia cuando ..."],
        [[r[0], r[1], r[2], "cada flanco del reloj" if r[3] == "1" else r[3]] for r in rows],
        [":---", "---:", "---:", ":---"],
    )
    # 6.1 diagrama de tiempos del contador sincrono 0..15 (reloj P=2)
    Tsync = 34
    os_ = counter_net(4, "sync", 2, 1).run(Tsync)
    cnt = [code_at(os_, ["S0", "S1", "S2", "S3"], t) for t in range(Tsync)]
    V["sync_wave"] = diagram(
        [("CLK", wave(os_["CLK"]))] + [(f"Q{k}", wave(os_[f"S{k}"])) for k in range(4)] + [("CUENTA", bus(cnt))],
        Tsync,
    )
    V["sync_wave_wrap_tick"] = [t for t in range(1, Tsync) if cnt[t] == 0 and cnt[t - 1] == 15][0]
    assert V["sync_wave_wrap_tick"] == 32
    # el contador sincrono (P=2) lee bien: 0..15 y vuelve a 0
    assert readings(4, "sync", 2, 1, 20) == [j % 16 for j in range(20)]
    # 6.2 ripple vs sincrono alrededor de la transicion 7 -> 8 (mismo reloj P=8, H=4)
    P, H = 8, 4
    Tz = (P - H) + 12 * P
    orr = counter_net(4, "ripple", P, H).run(Tz)
    ors = counter_net(4, "sync", P, H).run(Tz)
    V["net_ripple4"] = counter_net(4, "ripple", P, H).describe()
    V["net_sync4"] = counter_net(4, "sync", P, H).describe()
    e7 = edge_ticks(P, H, Tz)[7]  # octavo flanco: la cuenta pasa de 7 a 8
    assert code_at(orr, ["R0", "R1", "R2", "R3"], e7) == 7 and code_at(ors, ["S0", "S1", "S2", "S3"], e7) == 7
    rows = []
    for t in range(e7 - 1, e7 + 7):
        cr = code_at(orr, ["R0", "R1", "R2", "R3"], t)
        cs = code_at(ors, ["S0", "S1", "S2", "S3"], t)
        rows.append([t, f"e{t - e7:+d}" if t != e7 else "e", orr["CLK"][t], bstr(cr, 4), cr, bstr(cs, 4), cs])
    V["ripple_trans_table"] = table(
        ["paso", "relativo", "CLK", "ripple Q3 Q2 Q1 Q0", "ripple (decimal)", "síncrono Q3 Q2 Q1 Q0", "síncrono (decimal)"],
        rows,
        ["---:", ":---:", "---:", ":---:", "---:", ":---:", "---:"],
    )
    W0, WT = e7 - 4, 14
    V["ripple_wave"] = diagram(
        [("CLK", wave(orr["CLK"][W0 : W0 + WT]))] + [(f"Q{k}", wave(orr[f"R{k}"][W0 : W0 + WT])) for k in range(4)],
        WT,
        start=W0,
    )
    V["sync_zoom_wave"] = diagram(
        [("CLK", wave(ors["CLK"][W0 : W0 + WT]))] + [(f"Q{k}", wave(ors[f"S{k}"][W0 : W0 + WT])) for k in range(4)],
        WT,
        start=W0,
    )
    V["ripple_e7"] = e7
    seen_r = [code_at(orr, ["R0", "R1", "R2", "R3"], t) for t in range(e7 + 1, e7 + 6)]
    assert seen_r == [6, 4, 0, 8, 8]
    V["ripple_wrong_values"] = ", ".join(bstr(v, 4) for v in seen_r[:3])
    first8 = [t for t in range(e7 + 1, e7 + 12) if code_at(orr, ["R0", "R1", "R2", "R3"], t) == 8][0]
    V["ripple_settle_ticks"] = first8 - e7
    assert V["ripple_settle_ticks"] == 4
    # los 4 valores intermedios solo existen en el ripple
    assert all(code_at(ors, ["S0", "S1", "S2", "S3"], t) in (7, 8) for t in range(e7 - 1, e7 + 7))
    # 6.2b ciclo completo del ripple con el reloj mas rapido que aguanta
    Pm = pmin(4, "ripple")[0]
    Hm = max(1, Pm // 2)
    Tfull = (Pm - Hm) + 16 * Pm + 4
    ofull = counter_net(4, "ripple", Pm, Hm).run(Tfull)
    rn = ["R0", "R1", "R2", "R3"]
    cfull = [code_at(ofull, rn, t) for t in range(Tfull)]
    V["ripple_full_P"] = Pm
    V["ripple_full_wave"] = diagram(
        [("CLK", wave(ofull["CLK"]))] + [(f"Q{k}", wave(ofull[f"R{k}"])) for k in range(4)] + [("CUENTA", bus(cfull))],
        Tfull,
    )
    assert [cfull[t] for t in edge_ticks(Pm, Hm, Tfull)][:17] == [j % 16 for j in range(17)]
    # 6.3 reloj rapido (P=2): el ripple lee mal
    Pf = 2
    rr = readings(4, "ripple", Pf, 1, 12)
    rs = readings(4, "sync", Pf, 1, 12)
    V["fast_table"] = table(
        ["flanco j", "lectura esperada", "ripple lee", "síncrono lee", "ripple acierta"],
        [[j, bstr(j % 16, 4), bstr(rr[j], 4), bstr(rs[j], 4), "sí" if rr[j] == j % 16 else "NO"] for j in range(12)],
        ["---:", ":---:", ":---:", ":---:", ":---:"],
    )
    V["fast_wrong"] = sum(1 for j in range(12) if rr[j] != j % 16)
    V["fast_wrong_from"] = [j for j in range(12) if rr[j] != j % 16][0]
    assert rs == [j % 16 for j in range(12)]
    # 6.4 periodo minimo
    rows = []
    pm = {}
    for nb in range(2, 9):
        a, _ = pmin(nb, "ripple")
        b, _ = pmin(nb, "sync")
        c, _ = pmin(nb, "sync2")
        pm[nb] = (a, b, c)
        rows.append([nb, 2**nb, a, b, c])
    V["pmin_table"] = table(
        ["bits n", "cuentas (2^n)", "P mínimo ripple", "P mínimo síncrono (AND ancha)", "P mínimo síncrono (ANDs de 2 entradas)"],
        rows,
        ["---:"] * 5,
    )
    V["pmin"] = pm
    assert all(pm[nb][0] == nb for nb in pm), pm
    assert all(pm[nb][1] == 2 for nb in pm), pm
    pm10 = pmin(10, "ripple")[0]
    V["pmin10"] = pm10
    assert pm10 == 10
    V["pmin_sync2_note"] = pm
    # reloj Q en vez de Qn: cuenta atras
    oq = counter_net(4, "ripple_q", 8, 4).run((8 - 4) + 20 * 8)
    dq = [code_at(oq, ["R0", "R1", "R2", "R3"], t) for t in edge_ticks(8, 4, len(oq["CLK"]))][:6]
    V["down_seq"] = ", ".join(hx(v) for v in dq)
    assert dq == [0, 15, 14, 13, 12, 11]
    # ---- 7. Detector de flancos -------------------------------------------------------
    xs = [0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0]
    Te = len(xs)
    enet = edge_all_net(seq_fn(xs))
    eo = enet.run(Te)
    V["net_edge"] = enet.describe()
    V["edge_wave"] = diagram(
        [("x", wave(xs)), ("nx", wave(eo["nx"])), ("SUBIDA", wave(eo["SUBIDA"])), ("BAJADA", wave(eo["BAJADA"])), ("AMBOS", wave(eo["AMBOS"]))],
        Te,
    )
    xp = [0] + xs  # x(-1)=0
    for t in range(Te - 1):
        a, b = xp[t + 1], xp[t]  # x(t), x(t-1)
        assert eo["SUBIDA"][t + 1] == (a and not b)
        assert eo["BAJADA"][t + 1] == (b and not a)
        assert eo["AMBOS"][t + 1] == (a ^ b)
    # el detector con k=0 de edge_net es exactamente la rama SUBIDA
    assert edge_net("rise", 0, seq_fn(xs)).run(Te)["out"] == eo["SUBIDA"]
    V["edge_rise_ticks"] = ", ".join(str(t) for t in rises(eo["SUBIDA"]))
    V["edge_fall_ticks"] = ", ".join(str(t) for t in rises(eo["BAJADA"]))
    # anchura del pulso segun retardo extra k y anchura W de la entrada
    rows = []
    Ws = [1, 2, 3, 6]
    for k in range(4):
        row = [k]
        for W in Ws:
            xw = [0] * 4 + [1] * W + [0] * 12
            o6 = edge_net("rise", k, seq_fn(xw)).run(len(xw))
            width = sum(o6["out"])
            assert width == min(W, 1 + k), (k, W, width)
            row.append(width)
        rows.append(row)
    V["edge_width_k0"] = {r[1:][i] for r in rows if r[0] == 0 for i in range(len(Ws))}.pop()
    V["edge_width_table"] = table(
        ["k (BUF extra)"] + [f"entrada de {W} paso(s)" for W in Ws], rows, ["---:"] * (1 + len(Ws))
    )
    # ---- 8. Alargador de pulso --------------------------------------------------------
    N = 4
    V["stretch_N"] = N
    xs = [0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0]
    Ts = len(xs)
    o7 = stretch_net(N, seq_fn(xs)).run(Ts)
    V["net_stretch"] = stretch_net(N, seq_fn(xs)).describe()
    V["stretch_wave"] = diagram([("x", wave(xs)), ("ANCHO", wave(o7["ANCHO"]))], Ts)
    # formula: out(t+1) = OR de x(t), x(t-1), ..., x(t-N+1)
    for t in range(Ts - 1):
        assert o7["ANCHO"][t + 1] == int(any(xs[t - i] for i in range(N) if t - i >= 0))
    rows = []
    for W in (1, 2, 3):
        xw = [0] * 3 + [1] * W + [0] * 12
        o8 = stretch_net(N, seq_fn(xw)).run(len(xw))
        w = sum(o8["ANCHO"])
        assert w == W + N - 1
        rows.append([W, w, f"{W} + {N} - 1"])
    V["stretch_width_table"] = table(
        ["anchura de entrada W", "anchura de salida", "fórmula W + N - 1"], rows, ["---:", "---:", ":---:"]
    )
    # equivalencia exhaustiva con el monoestable de contador (todas las entradas de 12 pasos)
    L = 12
    for bits_ in itertools.product((0, 1), repeat=L):
        xl = list(bits_) + [0] * (N + 1)
        so = stretch_net(N, seq_fn(xl)).run(len(xl))["ANCHO"]
        # el monoestable da salida(t) = [c(t)>0]; la red da out(t+1) = OR(...): mismo valor en t+1
        mo = monostable(xl, N)
        assert so[1:] == mo[1:], bits_
    V["stretch_exhaustive"] = 2**L
    # ---- Ejemplo resuelto: latido --------------------------------------------------------
    net9 = beat_net(2, 1, "ideal")
    T9 = 56
    o9 = net9.run(T9)
    V["net_beat"] = net9.describe()
    V["beat_wave"] = diagram(
        [("CLK", wave(o9["CLK"])), ("Z1", wave(o9["Z1"])), ("Z2", wave(o9["Z2"])), ("Z3", wave(o9["Z3"])), ("PULSO", wave(o9["PULSO"])), ("ANCHO", wave(o9["ANCHO"]))],
        T9,
    )
    pr = rises(o9["PULSO"])
    V["beat_pulse_ticks"] = ", ".join(str(t) for t in pr)
    V["beat_pulse_gap"] = pr[1] - pr[0]
    assert all(b - a == 16 for a, b in zip(pr, pr[1:]))
    assert all(sum(o9["PULSO"][t : t + 16]) == 1 for t in range(pr[0], T9 - 16, 16))
    p9, h9 = measure(o9["ANCHO"])
    V["beat_wide_width"] = h9
    V["beat_first"] = pr[0]
    assert (p9, h9) == (16, 4)
    V["beat_q3_period"] = measure(o9["Z3"])[0]
    # ---- Mini-ejercicio ---------------------------------------------------------------
    o10 = divider(4, 2, 3, 40)
    o10L = divider(4, 2, 3, 240)
    V["ex_wave"] = diagram([("CLK", wave(o10["CLK"])), ("Z1", wave(o10["Z1"])), ("Z2", wave(o10["Z2"])), ("Z3", wave(o10["Z3"]))], 40)
    V["ex_periods"] = [measure(o10L[k])[0] for k in ("CLK", "Z1", "Z2", "Z3")]
    V["ex_stages"] = 3
    assert V["ex_periods"] == [4, 8, 16, 32]
    V["ex_n"] = 10
    # comprobacion de que 2 etapas no llegan
    V["ex_two_stage"] = V["ex_periods"][2]
    # ---- comprobaciones adicionales del anillo (defectos que viajan) -----------------------------
    for n in (3, 5, 7, 9):
        for s0 in itertools.product((0, 1), repeat=n):
            e0 = [int(s0[i] == s0[(i - 1) % n]) for i in range(n)]  # 1 = puerta inestable
            s1 = ring_next(s0)
            e1 = [int(s1[i] == s1[(i - 1) % n]) for i in range(n)]
            assert e1 == [e0[(i - 1) % n] for i in range(n)]  # los defectos se desplazan 1 puerta por paso
            assert sum(e0) % 2 == 1
            if sum(e0) == 1:
                assert orbit_len(s0) == 2 * n
    V["ring_defect_odd"] = True
    for s0 in itertools.product((0, 1), repeat=9):
        if orbit_len(s0) == 6:
            pos = [i for i in range(9) if s0[i] == s0[(i - 1) % 9]]
            assert len(pos) == 3 and pos[1] - pos[0] == 3 and pos[2] - pos[1] == 3
    V["ring9_defects"] = 3
    V["ring9_spacing"] = 3
    # ---- el mismo analisis con el flip-flop T construido con puertas (documento 04) -----------------
    # (a) cuando cambian Q y Qn tras el flanco (T = 1, semiperiodo holgado)
    hh = 6
    og = divider_net(2 * hh, hh, 1, "gate").run(24 * hh)
    qg, qng = og["Z1"], og["Z1n"]
    eg = rises(og["CLK"])[:-1]
    dq, dqn = set(), set()
    for e in eg:
        tq = next(t for t in range(e + 1, len(qg)) if qg[t] != qg[t - 1])
        tn = next(t for t in range(e + 1, len(qg)) if qng[t] != qng[t - 1])
        dq.add(tq - e)
        dqn.add(tn - e)
        # Q y Qn valen lo mismo durante exactamente 1 paso por cambio, y siempre 1 los dos
        same = [t for t in range(e, e + 2 * hh) if qg[t] == qng[t]]
        assert len(same) == 1 and qg[same[0]] == 1, (e, same)
    Bm = check_b()  # el flip-flop T de puertas es el del documento 04: sus dos minimos de reloj los mide check_B.py
    lim = Bm.tff_clock_limits()
    assert dq == dqn == set(range(lim["q_fixed"][0], lim["q_fixed"][1] + 1)), (dq, dqn, lim["q_fixed"])  # igual que en el 04, con T = 1
    V["gt_delay"] = " o ".join(str(x) for x in sorted(dq))
    V["gt_both1"] = 1
    # (b) periodos minimos con el flip-flop ideal y con el de puertas
    V["gt_div_P"], V["gt_div_P_ideal"] = pmin_divider(3, "gate"), pmin_divider(3, "ideal")
    assert V["gt_div_P_ideal"] == 2 and V["gt_div_P"] == 6
    assert V["gt_div_P"] == 2 * lim["min_fixed"]  # el mismo minimo que el 04 con T=1 fija (reloj simetrico: periodo = 2 x semiperiodo)
    V["gt_any_P"], V["gt_any_qhi"] = 2 * lim["min_any"], lim["q_any"][1]
    assert V["gt_any_P"] > V["gt_div_P"]
    for hh_ in range(lim["min_fixed"], lim["min_fixed"] + 4):  # mismo circuito en los dos simuladores (desde el minimo; con menos, otro arranque)
        n_ = 14 * 2 * hh_
        clk_ = Bm.clock(n_, hh_)
        st_ = Bm.run_states(Bm.TFF, Bm.powerup_tff(0), [Bm.TFF.x(T=1, CLK=clk_[t]) for t in range(n_)])
        og_ = divider_net(2 * hh_, hh_, 1, "gate").run(n_)
        assert [Bm.TFF.val(x, "Q") for x in st_] == og_["Z1"] and [Bm.TFF.val(x, "Qn") for x in st_] == og_["Z1n"], hh_
    rows, gpm = [], {}
    for nb in range(2, 7):
        gr, gs = pmin_gate(nb, "ripple"), pmin_gate(nb, "sync")
        gpm[nb] = (gr, gs)
        assert gr > pm[nb][0] and gs > pm[nb][1]
        rows.append([nb, pm[nb][0], gr, pm[nb][1], gs])
    V["gt_pmin_table"] = table(
        [
            "bits n",
            "ripple con flip-flop ideal",
            "ripple con flip-flop T de puertas",
            "síncrono (AND ancha) con flip-flop ideal",
            "síncrono (AND ancha) con flip-flop T de puertas",
        ],
        rows,
        ["---:"] * 5,
    )
    V["gt_cnt_bits"] = 4
    V["gt_ripple4"], V["gt_ripple4_ideal"] = gpm[4][0], pm[4][0]
    V["gt_sync4"], V["gt_sync4_ideal"] = gpm[4][1], pm[4][1]
    V["gt_n_lo"], V["gt_n_hi"] = 2, 6
    # (c) el latido con el flip-flop de puertas: reloj mas rapido que aguanta y separacion entre pulsos
    V["gt_beat_P"], V["gt_beat_P_ideal"] = pmin_beat("gate"), pmin_beat("ideal")
    assert V["gt_beat_P_ideal"] == 2  # el temporizador del ejemplo resuelto
    og9 = beat_net(V["gt_beat_P"], V["gt_beat_P"] // 2, "gate").run(V["gt_beat_P"] * 8 * 6)
    prg = rises(og9["PULSO"])
    V["gt_beat_gap"] = prg[1] - prg[0]
    V["gt_beat_mult"] = 8  # 3 etapas: 2^3
    assert V["gt_beat_gap"] == V["gt_beat_P"] * V["gt_beat_mult"]
    # ---- tablas de apoyo de notacion --------------------------------------------------------------
    V["notation_table"] = table(
        ["decimal", "binario", "hexadecimal"],
        [[v, bstr(v, 4 if v < 16 else 8), hx(v)] for v in (6, 10, 255)],
        ["---:", ":---:", ":---:"],
    )
    V["ops_table"] = make_ops_table()
    # ---- recetas ----------------------------------------------------------------------------------
    pm = V["pmin"]
    V["recipes_table"] = table(
        ["Necesito...", "Montaje", "Resultado comprobado por simulación"],
        [
            ["reloj de periodo P y H pasos en alto", "temporizador(P, H)", f"con P={V['timer_cfgs'][0][0]} y H={V['timer_cfgs'][0][1]}: periodo y ancho exactos"],
            ["reloj con periodo 2D y 50 %", "número impar de NOT + BUF, retardo total D", "periodo 2D (tabla del anillo)"],
            ["dividir la frecuencia entre 2^k", "k flip-flops T en cascada (T=1)", f"periodos {', '.join(str(x) for x in V['div_periods'])} con k = 0, 1, 2, 3"],
            ["contar de 0 a 2^n - 1", "n flip-flops T (ripple o síncrono)", f"{2**4} cuentas con n = 4"],
            ["pulso de 1 paso en un flanco de subida", "AND(x, NOT x), el NOT es el retardo", f"ancho {V['edge_width_k0']} sea cual sea la entrada"],
            ["alargar un pulso", "OR de N copias retrasadas", f"ancho W + N - 1 (N = {V['stretch_N']})"],
        ],
        [":---", ":---", ":---"],
    )
    # ---- numeros auxiliares para la prosa ---------------------------------------------------------
    xr = [t for t in range(len(o["x"])) if o["x"][t] == 1][0]
    V["tn_x"] = xr
    V["tn_a"] = [t for t in range(len(o["a"])) if o["a"][t] == 0][0]
    V["tn_b"] = [t for t in range(len(o["b"])) if o["b"][t] == 1][0]
    assert (V["tn_x"], V["tn_a"], V["tn_b"]) == (3, 4, 5)
    V["ring_n_example"] = 3
    V["ring_list_n"] = ", ".join(str(n_) for n_ in (1, 3, 5, 7, 9))
    V["ring_zero_period"] = orbit_len((0, 0, 0))
    V["ring_zero_defects"] = sum(1 for i in range(3) if (0, 0, 0)[i] == (0, 0, 0)[(i - 1) % 3])
    V["fast_P"], V["fast_total"] = 2, 12
    V["cnt_total"] = 2**4
    V["ex_target"] = V["ex_periods"][3]
    V["ex_base"] = V["ex_periods"][0]
    V["div_k"] = 3
    V["beat_base"] = 2
    V["beat_stages"] = 3
    V["beat_target"] = V["beat_q3_period"]
    assert V["beat_target"] == V["beat_base"] * 2 ** V["beat_stages"]
    V["bit0_label"] = hx(0)
    V["count_last_bin"], V["count_last_hex"] = bstr(15, 4), hx(15)
    V["count_first_bin"], V["count_first_hex"] = bstr(0, 4), hx(0)
    V["ring9_n"] = 9
    V["ring9_short_period"] = 6
    V["ring9_short_states"] = V["ring_hist"][9][6]
    V["div_periods_txt"] = ", ".join(str(x) for x in V["div_periods"])
    V["rt_from_dec"], V["rt_to_dec"] = 7, 8
    V["rt_from"], V["rt_to"] = bstr(7, 4), bstr(8, 4)
    V["pmin_sync"] = V["pmin"][4][1]
    assert all(V["pmin"][n_][1] == V["pmin_sync"] for n_ in V["pmin"])
    V["stretch_L"] = V["stretch_exhaustive"].bit_length() - 1
    assert 2 ** V["stretch_L"] == V["stretch_exhaustive"]
    V["ex_periods_arrow"] = " -> ".join(str(x) for x in V["ex_periods"])
    V["ex_hint_n"] = V["ex_n"]
    # cifras de la wiki de Minecraft citadas en la prosa (no salen de una simulacion; fuente: pagina Redstone Torch)
    V["mc_burn_n"], V["mc_burn_ticks"] = 8, 60
    V["ring_n1_period"] = ring_periods[1]  # anillo de 1 NOT
    V.update(notation_keys())
    return V


# ---------------------------------------------------------------------------
# 4. Documento 07: maquinas de estado
# ---------------------------------------------------------------------------


class Canvas:
    """Lienzo ASCII para los diagramas de estados."""

    def __init__(self, h, w):
        self.g = [[" "] * w for _ in range(h)]

    def put(self, r, c, s):
        for i, ch in enumerate(s):
            self.g[r][c + i] = ch

    def hline(self, r, c1, c2, ch="-"):
        for c in range(min(c1, c2), max(c1, c2) + 1):
            self.g[r][c] = ch

    def vline(self, c, r1, r2, ch="|"):
        for r in range(min(r1, r2), max(r1, r2) + 1):
            self.g[r][c] = ch

    def box(self, r, c, w, lines):
        h = len(lines) + 2
        self.hline(r, c, c + w - 1)
        self.hline(r + h - 1, c, c + w - 1)
        self.g[r][c] = self.g[r][c + w - 1] = "+"
        self.g[r + h - 1][c] = self.g[r + h - 1][c + w - 1] = "+"
        for i, ln in enumerate(lines):
            self.put(r + 1 + i, c, "|" + ln.center(w - 2) + "|")

    def text(self):
        return "\n".join("".join(row).rstrip() for row in self.g).strip("\n")


def qm_primes(on, dc):
    cur = {(m, 0) for m in set(on) | set(dc)}
    primes = set()
    while cur:
        nxt, used = set(), set()
        for a, b in itertools.combinations(sorted(cur), 2):
            if a[1] == b[1]:
                d = a[0] ^ b[0]
                if d and d & (d - 1) == 0:
                    nxt.add((a[0] & ~d, a[1] | d))
                    used.update((a, b))
        primes |= cur - used
        cur = nxt
    return primes


def imp_covers(imp, m):
    return (m & ~imp[1]) == imp[0]


def imp_literals(imp, nvars):
    return nvars - bin(imp[1]).count("1")


def qm_minimize(nvars, on, dc=()):
    """Suma de productos minima (menos terminos, luego menos literales) por Quine-McCluskey."""
    on = sorted(set(on))
    if not on:
        return []
    primes = sorted(qm_primes(on, dc))
    best = None
    for k in range(1, len(primes) + 1):
        for combo in itertools.combinations(primes, k):
            if all(any(imp_covers(i, m) for i in combo) for m in on):
                cost = (k, sum(imp_literals(i, nvars) for i in combo), combo)
                if best is None or cost < best:
                    best = cost
        if best is not None:
            break
    return list(best[2])


def lit_txt(n):
    return f"{n} literal" if n == 1 else f"{n} literales"


def kmap3(title, rname, cnames, cell):
    """Mapa de Karnaugh de 3 variables con el formato del documento 02: una variable en las filas y dos en las
    columnas, en orden Gray. cell(r, a, b) -> "0", "1" o "X" (casilla «no importa»)."""
    gray2 = [(0, 0), (0, 1), (1, 1), (1, 0)]
    rlab = [f"{rname}={r}" for r in (0, 1)]
    labw = max(len(x) for x in rlab)
    codes = [f"{a}{b}" for a, b in gray2]
    cells = [[cell(r, a, b) for a, b in gray2] for r in (0, 1)]
    w = max(2, max(len(x) for row in cells for x in row))
    out = [f"# {title}", " " * (labw + 3) + " ".join(cnames), " " * (labw + 3) + " ".join(x.rjust(w) for x in codes)]
    for lab, row in zip(rlab, cells):
        out.append(lab.ljust(labw) + " | " + " ".join(x.rjust(w) for x in row))
    return "\n".join(ln.rstrip() for ln in out)


def kmap_cells(on, dc):
    """cell(r, a, b) para kmap3 a partir de los minterms con 1 y de los «no importa» (minterm = r*4 + a*2 + b)."""
    on, dc = set(on), set(dc)

    def cell(r, a, b):
        m = r << 2 | a << 1 | b
        return "1" if m in on else ("X" if m in dc else "0")

    return cell


def group_desc(names, imp, on, dc, nvars):
    """Descripcion en prosa de un grupo del mapa: tamano, cuantas casillas son 1 y cuantas X, y el producto que da."""
    cells = [m for m in range(2**nvars) if imp_covers(imp, m)]
    ones = sum(1 for m in cells if m in on)
    xs = sum(1 for m in cells if m in dc)
    assert ones + xs == len(cells) and ones >= 1 and len(cells) >= 2, (imp, cells)
    conds = " y ".join(f"{nm} = {imp[0] >> (nvars - 1 - i) & 1}" for i, nm in enumerate(names) if not (imp[1] >> (nvars - 1 - i) & 1))
    return f"{len(cells)} casillas ({ones} con valor 1 y {xs} X), las de {conds}, que da {sop_string(names, [imp], nvars)}"


def groups_prose(names, cover, on, dc, nvars):
    """«un grupo de ...; otro de ...» para todos los grupos de una cobertura."""
    parts = [group_desc(names, imp, set(on), set(dc), nvars) for imp in sorted(cover)]
    return "; ".join((("un grupo de " if i == 0 else "otro de ") + p) for i, p in enumerate(parts))


def sop_string(names, cover, nvars):
    if not cover:
        return "0"
    terms = []
    for imp in sorted(cover):
        lits = []
        for i, nm in enumerate(names):
            bit = nvars - 1 - i
            if imp[1] >> bit & 1:
                continue
            lits.append(nm if imp[0] >> bit & 1 else "¬" + nm)
        terms.append("·".join(lits) if lits else "1")
    return " + ".join(terms)


def canonical_string(names, on):
    n = len(names)
    if not on:
        return "0"
    out = []
    for m in sorted(on):
        out.append("·".join(nm if m >> (n - 1 - i) & 1 else "¬" + nm for i, nm in enumerate(names)))
    return " + ".join(out)


def eval_sop(expr, env):
    expr = expr.strip()
    if expr == "0":
        return 0
    if expr == "1":
        return 1
    for term in expr.split(" + "):
        ok = 1
        for lit in term.split("·"):
            if lit.startswith("¬"):
                ok &= 1 - env[lit[1:]]
            else:
                ok &= env[lit]
        if ok:
            return 1
    return 0


def sop_literals(expr):
    if expr in ("0", "1"):
        return 0
    return sum(len(t.split("·")) for t in expr.split(" + "))


def sop_terms(expr):
    return 0 if expr in ("0", "1") else len(expr.split(" + "))


def add_sop(net, out, expr, variables):
    """Construye en `net` la logica de `out = expr` con puertas de retardo unitario."""
    if expr in ("0", "1"):
        net.src(out, lambda t, v=int(expr): v)
        return
    sigs = []
    for i, term in enumerate(expr.split(" + ")):
        lits = []
        for lit in term.split("·"):
            if lit.startswith("¬"):
                nm = "n_" + lit[1:]
                if nm not in net.el:
                    net.gate(nm, "NOT", [lit[1:]])
                lits.append(nm)
            else:
                assert lit in variables or lit in net.el, lit
                lits.append(lit)
        if len(lits) == 1:
            sigs.append(lits[0])
        else:
            nm = f"{out}_t{i}"
            net.gate(nm, "AND", lits)
            sigs.append(nm)
    if len(sigs) == 1:
        net.gate(out, "BUF", [sigs[0]])
    else:
        net.gate(out, "OR", sigs)


# --- maquinas ---------------------------------------------------------------------------


class FSM:
    def __init__(self, name, kind, states, syms, delta, lam, s0):
        self.name, self.kind, self.states, self.syms = name, kind, states, syms
        self.delta, self.lam, self.s0 = delta, lam, s0

    def out(self, s, x):
        return self.lam[(s, x)] if self.kind == "mealy" else self.lam[s]

    def run(self, xs):
        rows, s = [], self.s0
        for t, x in enumerate(xs):
            s2 = self.delta[(s, x)]
            rows.append((t, x, s, s2, self.out(s, x)))
            s = s2
        return rows


def pattern_fsms(pat):
    """Detector de la secuencia `pat` (con solapamiento). Estado S_k = 'el mayor sufijo de lo recibido
    que es prefijo de pat tiene longitud k'. Devuelve (mealy, moore)."""
    L = len(pat)

    def longest(k, x):
        s = pat[:k] + str(x)
        for m in range(min(len(s), L), -1, -1):
            if s.endswith(pat[:m]):
                return m
        return 0

    def fail(m):
        for j in range(m - 1, -1, -1):
            if pat[:m].endswith(pat[:j]):
                return j
        return 0

    sm = [f"S{k}" for k in range(L)]
    dm, lm = {}, {}
    for k in range(L):
        for x in (0, 1):
            m = longest(k, x)
            lm[(sm[k], x)] = int(m == L)
            dm[(sm[k], x)] = sm[fail(L) if m == L else m]
    mealy = FSM(f"mealy{pat}", "mealy", sm, (0, 1), dm, lm, "S0")
    so = [f"S{k}" for k in range(L + 1)]
    do, lo = {}, {}
    for k in range(L + 1):
        for x in (0, 1):
            do[(so[k], x)] = so[longest(k, x)]
        lo[so[k]] = int(k == L)
    moore = FSM(f"moore{pat}", "moore", so, (0, 1), do, lo, "S0")
    return mealy, moore


def ends_with(xs, t, pat):
    L = len(pat)
    return t >= L - 1 and "".join(str(v) for v in xs[t - L + 1 : t + 1]) == pat


def hyst_next(s, level):
    """Especificacion en lenguaje llano: ON si nivel<20; OFF si nivel>80; si no, se queda como esta."""
    if level < 20:
        return "ON"
    if level > 80:
        return "OFF"
    return s


def hyst_fsm():
    # entradas: (BAJO, ALTO); (1,1) es imposible (nivel<20 y nivel>80 a la vez)
    syms = [(0, 0), (1, 0), (0, 1)]
    delta = {}
    for s in ("OFF", "ON"):
        for b, a in syms:
            delta[(s, (b, a))] = "ON" if b else ("OFF" if a else s)
    return FSM("histeresis", "moore", ["OFF", "ON"], syms, delta, {"OFF": 0, "ON": 1}, "OFF")


def bfs(start, succ):
    seen, todo = {start}, [start]
    while todo:
        s = todo.pop()
        for n in succ(s):
            if n not in seen:
                seen.add(n)
                todo.append(n)
    return seen


def gate_level_fsm(next_eqs, ffs, out_eqs, in_names, xseqs, observe, P=12, H=6):
    """Simula en puertas con retardo unitario (flip-flops D + SOP) y devuelve, por ciclo k, la
    tupla (codigo de estado, salidas) observada en el paso del flanco de reloj (antes de confirmar)."""
    net = Net()
    net.src("CLK", timer_fn(P, H), timer_desc(P, H))
    for i, nm in enumerate(in_names):
        net.src(nm, lambda t, i=i: xseqs[min(t // P, len(xseqs) - 1)][i])
    variables = set(in_names) | {q for q, _, _ in ffs}
    # los Q de los flip-flops existen desde el principio; se declaran al final (orden irrelevante)
    for d, expr in next_eqs.items():
        add_sop(net, d, expr, variables)
    for y, expr in out_eqs.items():
        add_sop(net, y, expr, variables)
    for q, d, init in ffs:
        net.dff(q, d, "CLK", init)
    T = P * len(xseqs)
    o = net.run(T)
    res = []
    for k in range(len(xseqs)):
        t = k * P + (P - H)
        res.append((tuple(o[q][t] for q, _, _ in ffs), tuple(o[y][t] for y in out_eqs)))
    return res


def cell_row(label, cells, lw=12, w=3):
    return (label.ljust(lw) + "".join(str(c).rjust(w) for c in cells)).rstrip()


def mealy_diagram(m, pat):
    lab = lambda s, x: f"{x}/{m.lam[(s, x)]}"  # noqa: E731
    boxw, gap, c0, r = 11, 9, 6, 5
    cs = [c0 + i * (boxw + gap) for i in range(3)]
    cv = Canvas(r + 9, cs[2] + boxw + 2)
    pre = ["nada", f"llevo {pat[:1]}", f"llevo {pat[:2]}"]
    for i, nm in enumerate(m.states):
        cv.box(r, cs[i], boxw, [nm, pre[i]])
    cv.put(r + 1, cs[0] - 5, "--->")
    for i in (0, 1):  # avance
        a, b = cs[i] + boxw, cs[i + 1] - 1
        cv.hline(r + 1, a, b)
        cv.put(r + 1, b, ">")
        cv.put(r, a + (gap - 3) // 2, lab(m.states[i], int(pat[i])))
    # vuelta S2 -> S1 (x=1), fila inferior
    a, b = cs[1] + boxw, cs[2] - 1
    cv.hline(r + 2, a, b)
    cv.put(r + 2, a, "<")
    cv.put(r + 3, a + (gap - 3) // 2, lab("S2", 1))
    # bucles superiores: S0 (x=0), S1 (x=1)
    for i, x in ((0, 0), (1, 1)):
        a_, b_ = cs[i] + 2, cs[i] + 5
        cv.vline(a_, r - 2, r - 1)
        cv.vline(b_, r - 2, r - 1)
        cv.hline(r - 2, a_, b_)
        cv.put(r - 2, a_, "+")
        cv.put(r - 2, b_, "+")
        cv.put(r - 1, b_, "v")
        cv.put(r - 3, a_ - 1, lab(m.states[i], x))
    # arco inferior S2 -> S0 (x=0)
    ca, cb = cs[0] + boxw // 2, cs[2] + boxw // 2
    cv.vline(cb, r + 4, r + 6)
    cv.hline(r + 6, ca, cb)
    cv.put(r + 6, ca, "+")
    cv.put(r + 6, cb, "+")
    cv.vline(ca, r + 4, r + 5)
    cv.put(r + 4, ca, "^")
    cv.put(r + 7, (ca + cb) // 2 - 1, lab("S2", 0))
    return cv.text()


def moore_diagram(m, pat):
    boxw, gap, c0, r = 11, 8, 6, 5
    cs = [c0 + i * (boxw + gap) for i in range(4)]
    cv = Canvas(r + 9, cs[3] + boxw + 2)
    pre = ["nada", f"llevo {pat[:1]}", f"llevo {pat[:2]}", f"llevo {pat[:3]}"]
    for i, nm in enumerate(m.states):
        cv.box(r, cs[i], boxw, [f"{nm}  Y={m.lam[nm]}", pre[i]])
    cv.put(r + 1, cs[0] - 5, "--->")
    fwd = {0: 1, 1: 0, 2: 1}  # entrada que avanza de S_i a S_i+1
    for i in (0, 1, 2):
        assert m.delta[(m.states[i], fwd[i])] == m.states[i + 1]
        a, b = cs[i] + boxw, cs[i + 1] - 1
        cv.hline(r + 1, a, b)
        cv.put(r + 1, b, ">")
        cv.put(r, a + (gap - 1) // 2, str(fwd[i]))
    # vuelta S3 -> S2 (x=0)
    assert m.delta[("S3", 0)] == "S2"
    a, b = cs[2] + boxw, cs[3] - 1
    cv.hline(r + 2, a, b)
    cv.put(r + 2, a, "<")
    cv.put(r + 3, a + (gap - 1) // 2, "0")
    # bucles S0 (x=0) y S1 (x=1)
    for i, x in ((0, 0), (1, 1)):
        assert m.delta[(m.states[i], x)] == m.states[i]
        a_, b_ = cs[i] + 2, cs[i] + 5
        cv.vline(a_, r - 2, r - 1)
        cv.vline(b_, r - 2, r - 1)
        cv.hline(r - 2, a_, b_)
        cv.put(r - 2, a_, "+")
        cv.put(r - 2, b_, "+")
        cv.put(r - 1, b_, "v")
        cv.put(r - 3, (a_ + b_) // 2, str(x))
    # arco superior S3 -> S1 (x=1)
    assert m.delta[("S3", 1)] == "S1"
    ca, cb = cs[1] + boxw - 3, cs[3] + boxw // 2
    cv.vline(ca, r - 3, r - 1)
    cv.put(r - 1, ca, "v")
    cv.vline(cb, r - 3, r - 1)
    cv.hline(r - 3, ca, cb)
    cv.put(r - 3, ca, "+")
    cv.put(r - 3, cb, "+")
    cv.put(r - 4, (ca + cb) // 2, "1")
    # arco inferior S2 -> S0 (x=0)
    assert m.delta[("S2", 0)] == "S0"
    ca, cb = cs[0] + boxw // 2, cs[2] + boxw // 2
    cv.vline(cb, r + 4, r + 6)
    cv.hline(r + 6, ca, cb)
    cv.put(r + 6, ca, "+")
    cv.put(r + 6, cb, "+")
    cv.vline(ca, r + 4, r + 5)
    cv.put(r + 4, ca, "^")
    cv.put(r + 7, (ca + cb) // 2, "0")
    return cv.text()


def hyst_diagram(m):
    boxw, gap, c0, r = 9, 18, 6, 4
    cs = [c0, c0 + boxw + gap]
    cv = Canvas(r + 6, cs[1] + boxw + 2)
    for i, nm in enumerate(m.states):
        cv.box(r, cs[i], boxw, [nm, f"Y={m.lam[nm]}"])
    cv.put(r + 1, cs[0] - 5, "--->")
    a, b = cs[0] + boxw, cs[1] - 1
    assert m.delta[("OFF", (1, 0))] == "ON" and m.delta[("ON", (0, 1))] == "OFF"
    cv.hline(r + 1, a, b)
    cv.put(r + 1, b, ">")
    cv.put(r, a + (gap - 6) // 2, "BAJO=1")
    cv.hline(r + 2, a, b)
    cv.put(r + 2, a, "<")
    cv.put(r + 3, a + (gap - 6) // 2, "ALTO=1")
    for i, txt, st, sym in ((0, "BAJO=0", "OFF", (0, 0)), (1, "ALTO=0", "ON", (0, 0))):
        assert m.delta[(st, sym)] == st
        a_, b_ = cs[i] + 2, cs[i] + 5
        cv.vline(a_, r - 2, r - 1)
        cv.vline(b_, r - 2, r - 1)
        cv.hline(r - 2, a_, b_)
        cv.put(r - 2, a_, "+")
        cv.put(r - 2, b_, "+")
        cv.put(r - 1, b_, "v")
        cv.put(r - 3, a_ - 1, txt)
    return cv.text()


def ceil_log2(n):
    return (n - 1).bit_length()


def values07():
    V = dict(STATIC)
    V["ops_table"] = make_ops_table()
    pat = "101"
    mealy, moore = pattern_fsms(pat)
    V["pat"] = pat
    # ---- comprobacion exhaustiva contra "los ultimos tres bits son 101" ---------------
    nseq = 0
    for L in range(0, 13):
        for xs in itertools.product((0, 1), repeat=L):
            nseq += 1
            rm = mealy.run(xs)
            ro = moore.run(xs)
            for t in range(L):
                exp = int(ends_with(xs, t, pat))
                assert rm[t][4] == exp
                assert ro[t][4] == (int(ends_with(xs, t - 1, pat)) if t >= 1 else 0)  # Moore: un ciclo despues
    V["exh_count"] = nseq
    V["exh_len"] = 12
    V["mealy_nstates"] = len(mealy.states)
    V["moore_nstates"] = len(moore.states)
    assert (V["mealy_nstates"], V["moore_nstates"]) == (3, 4)
    V.update(notation_keys())
    # ---- definicion formal -------------------------------------------------------------
    sset = lambda m: "{" + ", ".join(m.states) + "}"  # noqa: E731
    V["mealy_formal"] = "\n".join(
        [
            "M = (S, X, Y, delta, lambda, s0)",
            f"S = {sset(mealy)}   ({len(mealy.states)} estados)",
            "X = {0, 1}          (1 bit de entrada)",
            "Y = {0, 1}          (1 bit de salida)",
            f"s0 = {mealy.s0}",
            "delta : S x X -> S      (siguiente estado)",
            "lambda: S x X -> Y      (Mealy: la salida mira el estado junto con la entrada)",
        ]
    )
    V["moore_formal"] = "\n".join(
        [
            "M = (S, X, Y, delta, lambda, s0)",
            f"S = {sset(moore)}   ({len(moore.states)} estados)",
            "X = {0, 1}",
            "Y = {0, 1}",
            f"s0 = {moore.s0}",
            "delta : S x X -> S",
            "lambda: S -> Y         (Moore: la salida mira SOLO el estado)",
        ]
    )
    # ---- tablas de estados -----------------------------------------------------------------
    meaning = lambda k: "nada útil" if k == 0 else f"llevo '{pat[:k]}'"  # noqa: E731
    V["mealy_table"] = table(
        ["Estado", "significado", "x=0: siguiente / Y", "x=1: siguiente / Y"],
        [[s, meaning(int(s[1:])), *[f"{mealy.delta[(s, x)]} / {mealy.lam[(s, x)]}" for x in (0, 1)]] for s in mealy.states],
        [":---:", ":---", ":---:", ":---:"],
    )
    V["moore_table"] = table(
        ["Estado", "significado", "Y", "x=0: siguiente", "x=1: siguiente"],
        [
            [s, meaning(int(s[1:])) if s != "S3" else "acabo de ver '101'", moore.lam[s], moore.delta[(s, 0)], moore.delta[(s, 1)]]
            for s in moore.states
        ],
        [":---:", ":---", ":---:", ":---:", ":---:"],
    )
    V["mealy_diagram"] = mealy_diagram(mealy, pat)
    V["moore_diagram"] = moore_diagram(moore, pat)
    # ---- trazas -------------------------------------------------------------------------
    xs = [1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1]
    V["trace_x"] = "".join(str(v) for v in xs)
    V["trace_x_bin"] = "0b" + "".join(str(v) for v in xs)
    rm = mealy.run(xs)
    ro = moore.run(xs)
    V["mealy_trace"] = table(
        ["t", "x(t)", "S(t)", "S(t+1)", "Y(t)"],
        [[t, x, s, s2, y] for t, x, s, s2, y in rm],
        ["---:", "---:", ":---:", ":---:", "---:"],
    )
    V["moore_trace"] = table(
        ["t", "x(t)", "S(t)", "S(t+1)", "Y(t)"],
        [[t, x, s, s2, y] for t, x, s, s2, y in ro],
        ["---:", "---:", ":---:", ":---:", "---:"],
    )
    ym = [r[4] for r in rm]
    yo = [r[4] for r in ro]
    V["trace_wave"] = diagram(
        [("x", wave(xs)), ("Y Mealy", wave(ym)), ("Y Moore", wave(yo))], len(xs), lw=9, axis="ciclo t"
    )
    det = [t for t in range(len(xs)) if ends_with(xs, t, pat)]
    assert [t for t in range(len(xs)) if ym[t]] == det
    assert [t for t in range(len(xs)) if yo[t]] == [t + 1 for t in det]
    V["trace_det_mealy"] = ", ".join(map(str, det))
    V["trace_det_moore"] = ", ".join(str(t + 1) for t in det)
    V["trace_ndet"] = len(det)
    V["trace_overlap"] = [t for t in det if t - 1 in det or t - 2 in det]
    cex = None
    for t2, (_, x2, s2_, n2, _) in enumerate(rm):
        for t1, (_, x1, s1_, n1, _) in enumerate(rm[:t2]):
            if s1_ == s2_ and n1 != n2:
                cex = (t1, t2, s1_, n1, n2, x1, x2)
                break
        if cex:
            break
    assert cex is not None and cex[5] != cex[6]
    V["cex_t1"], V["cex_t2"], V["cex_s"], V["cex_n1"], V["cex_n2"], V["cex_x1"], V["cex_x2"] = cex
    assert det == [2, 4, 7, 12]
    # ---- codificacion de estados: binaria frente a one-hot --------------------------------
    rows = []
    for N in (2, 3, 4, 5, 8, 16):
        b = ceil_log2(N)
        rows.append([N, b, 2**b - N, N, 2**N - N])
    V["enc_table"] = table(
        ["estados N", "FF binaria", "códigos sin usar (binaria)", "FF one-hot", "códigos sin usar (one-hot)"],
        rows,
        ["---:"] * 5,
    )
    cb = {"S0": (0, 0), "S1": (0, 1), "S2": (1, 0)}
    V["mealy_codes_bin"] = table(
        ["Estado", "Q1 Q0"], [[s, "0b%d%d" % cb[s]] for s in mealy.states], [":---:", ":---:"]
    )
    ch = {"S0": (1, 0, 0), "S1": (0, 1, 0), "S2": (0, 0, 1)}
    V["mealy_codes_hot"] = table(
        ["Estado", "E2 E1 E0"], [[s, "0b%d%d%d" % tuple(reversed(ch[s]))] for s in mealy.states], [":---:", ":---:"]
    )
    # ---- Mealy 101 binario: tabla de verdad y ecuaciones ------------------------------------
    names = ["Q1", "Q0", "x"]
    rows, on = [], {"D1": [], "D0": [], "Y": []}
    dc = []
    inv = {v: k for k, v in cb.items()}
    for m_ in range(8):
        q1, q0, x = m_ >> 2 & 1, m_ >> 1 & 1, m_ & 1
        if (q1, q0) in inv:
            s = inv[(q1, q0)]
            s2, y = mealy.delta[(s, x)], mealy.lam[(s, x)]
            d1, d0 = cb[s2]
            rows.append([f"{s}", q1, q0, x, d1, d0, y])
            for nm, v in (("D1", d1), ("D0", d0), ("Y", y)):
                if v:
                    on[nm].append(m_)
        else:
            rows.append(["(sin usar)", q1, q0, x, "-", "-", "-"])
            dc.append(m_)
    V["mealy_bin_truth"] = table(
        ["Estado", "Q1", "Q0", "x", "D1", "D0", "Y"], rows, [":---:"] + ["---:"] * 6
    )
    V["mealy_bin_D0_canon"] = "D0 = " + canonical_string(names, on["D0"])
    eq = {}
    for nm in ("D1", "D0", "Y"):
        eq[nm] = sop_string(names, qm_minimize(3, on[nm], dc), 3)
    V["mealy_bin_eqs"] = "\n".join(f"{nm}{' ' if nm == 'Y' else ''} = {eq[nm]}" for nm in ("D1", "D0", "Y"))
    V["mealy_bin_eq"] = eq
    # verificacion exhaustiva de las ecuaciones contra la tabla (filas validas)
    for m_ in range(8):
        q1, q0, x = m_ >> 2 & 1, m_ >> 1 & 1, m_ & 1
        env = {"Q1": q1, "Q0": q0, "x": x}
        if (q1, q0) in inv:
            s = inv[(q1, q0)]
            assert (eval_sop(eq["D1"], env), eval_sop(eq["D0"], env)) == cb[mealy.delta[(s, x)]]
            assert eval_sop(eq["Y"], env) == mealy.lam[(s, x)]
    # mapas de Karnaugh de D1, D0 e Y (filas Q1; columnas Q0 x en orden Gray); las casillas sin usar son X
    mlist = lambda L: ",".join(str(v) for v in L)  # noqa: E731
    km_noX = {}
    for nm in ("D1", "D0", "Y"):
        cover = qm_minimize(3, on[nm], dc)
        assert sop_string(names, cover, 3) == eq[nm]
        V[f"km_{nm}"] = kmap3(
            f"K-mapa de {nm}, unos en los minterms {mlist(on[nm])} y X en los minterms {mlist(dc)}",
            "Q1",
            ["Q0", "x"],
            kmap_cells(on[nm], dc),
        )
        V[f"km_{nm}_txt"] = groups_prose(names, cover, on[nm], dc, 3)
        km_noX[nm] = sop_string(names, qm_minimize(3, on[nm], []), 3)
    V["km_ill_minterms"] = " y ".join(str(v) for v in dc)
    assert len(dc) == 2
    V["km_xcmp_table"] = table(
        ["Salida", "sin aprovechar las X", "aprovechando las X (el grupo del mapa)"],
        [
            [nm, f"{km_noX[nm]} ({lit_txt(sop_literals(km_noX[nm]))})", f"{eq[nm]} ({lit_txt(sop_literals(eq[nm]))})"]
            for nm in ("D1", "D0", "Y")
        ],
        [":---:", ":---", ":---"],
    )
    assert all(sop_literals(eq[nm]) < sop_literals(km_noX[nm]) for nm in eq)
    # que hacen las ecuaciones en el codigo sin usar 0b11
    ill = {}
    for x in (0, 1):
        env = {"Q1": 1, "Q0": 1, "x": x}
        ill[x] = (eval_sop(eq["D1"], env), eval_sop(eq["D0"], env), eval_sop(eq["Y"], env))
    V["mealy_bin_illegal"] = ill
    V["mealy_bin_illegal_txt"] = "; ".join(
        f"con x={x} pasa a 0b{ill[x][0]}{ill[x][1]} (que es {inv[(ill[x][0], ill[x][1])]}) y da Y={ill[x][2]}" for x in (0, 1)
    )
    # ---- Mealy 101 one-hot: ecuaciones leyendo flechas --------------------------------------
    eh = {}
    for j, tgt in enumerate(mealy.states):
        terms = []
        for i, src in enumerate(mealy.states):
            for x in (0, 1):
                if mealy.delta[(src, x)] == tgt:
                    terms.append(f"E{i}·{'x' if x else '¬x'}")
        eh[f"N{j}"] = " + ".join(terms)
    yt = [f"E{i}·{'x' if x else '¬x'}" for i, src in enumerate(mealy.states) for x in (0, 1) if mealy.lam[(src, x)]]
    eh["Y"] = " + ".join(yt)
    V["mealy_hot_eqs"] = "\n".join(f"{nm}{' ' if nm == 'Y' else ''} = {eh[nm]}" for nm in ("N0", "N1", "N2", "Y"))
    V["mealy_hot_eq"] = eh
    for s in mealy.states:
        for x in (0, 1):
            env = {"E0": ch[s][0], "E1": ch[s][1], "E2": ch[s][2], "x": x}
            nxt = tuple(eval_sop(eh[f"N{j}"], env) for j in range(3))
            assert nxt == ch[mealy.delta[(s, x)]], (s, x)
            assert eval_sop(eh["Y"], env) == mealy.lam[(s, x)]
            # simplificacion con la invariante "exactamente un E activo": N1 = x
            assert eval_sop("x", env) == nxt[1]
    V["hot_simplify"] = "N1 = x"
    # ---- Moore 101 binario ----------------------------------------------------------------------
    cm = {"S0": (0, 0), "S1": (0, 1), "S2": (1, 0), "S3": (1, 1)}
    invm = {v: k for k, v in cm.items()}
    rows, onm = [], {"D1": [], "D0": []}
    for m_ in range(8):
        q1, q0, x = m_ >> 2 & 1, m_ >> 1 & 1, m_ & 1
        s = invm[(q1, q0)]
        d1, d0 = cm[moore.delta[(s, x)]]
        rows.append([s, q1, q0, x, d1, d0])
        for nm, v in (("D1", d1), ("D0", d0)):
            if v:
                onm[nm].append(m_)
    V["moore_codes_bin"] = table(["Estado", "Q1 Q0"], [[s, "0b%d%d" % cm[s]] for s in moore.states], [":---:", ":---:"])
    V["moore_bin_truth"] = table(["Estado", "Q1", "Q0", "x", "D1", "D0"], rows, [":---:"] + ["---:"] * 5)
    eqm = {nm: sop_string(names, qm_minimize(3, onm[nm]), 3) for nm in ("D1", "D0")}
    onY = [m_ for m_ in range(4) if moore.lam[invm[(m_ >> 1 & 1, m_ & 1)]]]
    eqm["Y"] = sop_string(["Q1", "Q0"], qm_minimize(2, onY), 2)
    V["moore_bin_eqs"] = "\n".join(f"{nm}{' ' if nm == 'Y' else ''} = {eqm[nm]}" for nm in ("D1", "D0", "Y"))
    V["moore_bin_eq"] = eqm
    for m_ in range(8):
        q1, q0, x = m_ >> 2 & 1, m_ >> 1 & 1, m_ & 1
        env = {"Q1": q1, "Q0": q0, "x": x}
        s = invm[(q1, q0)]
        assert (eval_sop(eqm["D1"], env), eval_sop(eqm["D0"], env)) == cm[moore.delta[(s, x)]]
        assert eval_sop(eqm["Y"], env) == moore.lam[s]
    # ---- one-hot simplificado: mismo criterio que la binaria (los codigos sin usar son "no importa") -----------
    names_h = ["E2", "E1", "E0", "x"]
    on_h, legal_h = {k: [] for k in ("N0", "N1", "N2", "Y")}, set()
    for s_ in mealy.states:
        for x in (0, 1):
            e0, e1, e2 = ch[s_]
            m_ = e2 << 3 | e1 << 2 | e0 << 1 | x
            legal_h.add(m_)
            for j in range(3):
                if ch[mealy.delta[(s_, x)]][j]:
                    on_h[f"N{j}"].append(m_)
            if mealy.lam[(s_, x)]:
                on_h["Y"].append(m_)
    dc_h = [m_ for m_ in range(16) if m_ not in legal_h]
    emin = {k: sop_string(names_h, qm_minimize(4, on_h[k], dc_h), 4) for k in on_h}
    assert emin == {"N0": "¬E1·¬x", "N1": "x", "N2": "E1·¬x", "Y": "E2·x"}, emin
    V["mealy_hot_min_eqs"] = "\n".join(f"{nm}{' ' if nm == 'Y' else ''} = {emin[nm]}" for nm in ("N0", "N1", "N2", "Y"))
    V["hot_min_N0"], V["hot_min_N1"] = emin["N0"], emin["N1"]
    V["hot_nvars"] = len(names_h)
    for s_ in mealy.states:
        for x in (0, 1):
            env = {"E0": ch[s_][0], "E1": ch[s_][1], "E2": ch[s_][2], "x": x}
            assert tuple(eval_sop(emin[f"N{j}"], env) for j in range(3)) == ch[mealy.delta[(s_, x)]], (s_, x)
            assert eval_sop(emin["Y"], env) == mealy.lam[(s_, x)]
            # E0 + E2 = NOT E1 en los codigos legales (exactamente un E activo)
            assert (env["E0"] | env["E2"]) == 1 - env["E1"]
    # los codigos one-hot sin usar tambien vuelven a un estado legal en un ciclo
    n_ill = 0
    for code in range(8):
        e2, e1, e0 = code >> 2 & 1, code >> 1 & 1, code & 1
        if e2 + e1 + e0 == 1:
            continue
        n_ill += 1
        for x in (0, 1):
            env = {"E0": e0, "E1": e1, "E2": e2, "x": x}
            assert sum(eval_sop(emin[f"N{j}"], env) for j in range(3)) == 1, (code, x)
    V["hot_unused"] = n_ill
    assert n_ill == 2**3 - 3
    # ---- comparacion de costes (101 Mealy): la misma cuenta en las dos codificaciones -------------------------
    canon_bin = {nm: canonical_string(names, on[nm]) for nm in ("D1", "D0", "Y")}
    cnt = lambda E: (sum(sop_terms(v) for v in E.values()), sum(sop_literals(v) for v in E.values()))  # noqa: E731
    (term_bin, lit_bin), (term_hot_min, lit_hot_min) = cnt(eq), cnt(emin)
    (term_bin_c, lit_bin_c), (term_hot, lit_hot) = cnt(canon_bin), cnt(eh)
    assert term_bin < term_hot_min and lit_bin < lit_hot_min  # la binaria gana simplificada
    assert lit_hot < lit_bin_c  # y sin simplificar la one-hot leida de las flechas ni siquiera pierde en literales
    V["cmp_table"] = table(
        ["Mealy 101", "criterio", "flip-flops", "códigos sin usar", "términos AND", "literales"],
        [
            ["binaria", "simplificada (mapa de Karnaugh, X en los códigos sin usar)", 2, 2**2 - 3, term_bin, lit_bin],
            ["one-hot", "simplificada (X en los códigos sin usar)", 3, 2**3 - 3, term_hot_min, lit_hot_min],
            ["binaria", "sin simplificar (un término por cada fila con 1)", 2, 2**2 - 3, term_bin_c, lit_bin_c],
            ["one-hot", "sin simplificar (un término por cada flecha)", 3, 2**3 - 3, term_hot, lit_hot],
        ],
        [":---", ":---", "---:", "---:", "---:", "---:"],
    )
    V["ff_bin"], V["ff_hot"] = 2, 3
    V["term_bin"], V["lit_bin"], V["term_hot_min"], V["lit_hot_min"] = term_bin, lit_bin, term_hot_min, lit_hot_min
    V["term_bin_c"], V["lit_bin_c"], V["term_hot"], V["lit_hot"] = term_bin_c, lit_bin_c, term_hot, lit_hot
    # ---- verificacion en puertas con flip-flops D (retardo unitario, reloj de 12 pasos) ---------
    def seqs_bin(maxlen):
        for L in range(1, maxlen + 1):
            for xs_ in itertools.product((0, 1), repeat=L):
                yield [(x,) for x in xs_]

    ngl = 0
    for xseq in seqs_bin(8):
        xv = [v[0] for v in xseq]
        # Mealy binario
        r = gate_level_fsm({"D1": eq["D1"], "D0": eq["D0"]}, [("Q1", "D1", 0), ("Q0", "D0", 0)], {"Y": eq["Y"]}, ["x"], xseq, None)
        # Mealy one-hot
        r2 = gate_level_fsm(
            {"N0": eh["N0"], "N1": eh["N1"], "N2": eh["N2"]},
            [("E0", "N0", 1), ("E1", "N1", 0), ("E2", "N2", 0)],
            {"Y": eh["Y"]},
            ["x"],
            xseq,
            None,
        )
        # Mealy one-hot simplificado (X en los codigos sin usar)
        r4 = gate_level_fsm(
            {"N0": emin["N0"], "N1": emin["N1"], "N2": emin["N2"]},
            [("E0", "N0", 1), ("E1", "N1", 0), ("E2", "N2", 0)],
            {"Y": emin["Y"]},
            ["x"],
            xseq,
            None,
        )
        # Moore binario
        r3 = gate_level_fsm({"D1": eqm["D1"], "D0": eqm["D0"]}, [("Q1", "D1", 0), ("Q0", "D0", 0)], {"Y": eqm["Y"]}, ["x"], xseq, None)
        for k, row in enumerate(mealy.run(xv)):
            _, x, s, s2, y = row
            assert r[k] == (cb[s], (y,)), (xv, k)
            assert r2[k] == (ch[s], (y,)), (xv, k)
            assert r4[k] == (ch[s], (y,)), (xv, k)
        for k, row in enumerate(moore.run(xv)):
            _, x, s, s2, y = row
            assert r3[k] == (cm[s], (y,)), (xv, k)
        ngl += 1
    V["gate_seqs"] = ngl
    V["gate_maxlen"] = 8
    # ---- Ejemplo 2: histeresis -----------------------------------------------------------------
    hf = hyst_fsm()
    for lvl in range(0, 101):
        b, a = int(lvl < 20), int(lvl > 80)
        assert not (b and a)
        for s in ("OFF", "ON"):
            assert hf.delta[(s, (b, a))] == hyst_next(s, lvl)
    V["hyst_cmp_table"] = table(
        ["nivel L (%)", "BAJO = (L < 20)", "ALTO = (L > 80)"],
        [[l_, int(l_ < 20), int(l_ > 80)] for l_ in (0, 19, 20, 21, 50, 79, 80, 81, 100)],
        ["---:"] * 3,
    )
    rows = [[s, hf.lam[s]] + [hf.delta[(s, sy)] for sy in hf.syms] + ["imposible"] for s in hf.states]
    V["hyst_table"] = table(
        ["Estado", "Y (1 = ON)", "BAJO=0 ALTO=0", "BAJO=1 ALTO=0", "BAJO=0 ALTO=1", "BAJO=1 ALTO=1"],
        rows,
        [":---:"] * 6,
    )
    V["hyst_diagram"] = hyst_diagram(hf)
    # binaria: Q = 0 (OFF), 1 (ON); vars (Q, BAJO, ALTO)
    hn = ["Q", "BAJO", "ALTO"]
    hc = {"OFF": 0, "ON": 1}
    hinv = {0: "OFF", 1: "ON"}
    rows, onh, dch = [], [], []
    for m_ in range(8):
        q, b, a = m_ >> 2 & 1, m_ >> 1 & 1, m_ & 1
        if b and a:
            rows.append([hinv[q], q, b, a, "-"])
            dch.append(m_)
            continue
        d = hc[hf.delta[(hinv[q], (b, a))]]
        rows.append([hinv[q], q, b, a, d])
        if d:
            onh.append(m_)
    V["hyst_truth"] = table(["Estado", "Q", "BAJO", "ALTO", "D"], rows, [":---:"] + ["---:"] * 4)
    eqh = {"D": sop_string(hn, qm_minimize(3, onh, dch), 3), "Y": "Q"}
    V["hyst_eqs"] = "\n".join(f"{nm} = {eqh[nm]}" for nm in ("D", "Y"))
    hcover = qm_minimize(3, onh, dch)
    assert sop_string(hn, hcover, 3) == eqh["D"]
    V["km_hyst"] = kmap3(
        f"K-mapa de D, unos en los minterms {mlist(onh)} y X en los minterms {mlist(dch)}",
        "Q",
        ["BAJO", "ALTO"],
        kmap_cells(onh, dch),
    )
    V["km_hyst_txt"] = groups_prose(hn, hcover, onh, dch, 3)
    V["hyst_eq"] = eqh
    for m_ in range(8):
        q, b, a = m_ >> 2 & 1, m_ >> 1 & 1, m_ & 1
        if b and a:
            continue
        env = {"Q": q, "BAJO": b, "ALTO": a}
        assert eval_sop(eqh["D"], env) == hc[hf.delta[(hinv[q], (b, a))]]
    ngh = 0
    for L in range(1, 7):
        for xs_ in itertools.product(hf.syms, repeat=L):
            xseq = list(xs_)
            r = gate_level_fsm({"D": eqh["D"]}, [("Q", "D", 0)], {"Y": eqh["Y"]}, ["BAJO", "ALTO"], xseq, None)
            for k, row in enumerate(hf.run(xseq)):
                _, x, s, s2, y = row
                assert r[k] == ((hc[s],), (y,)), (xseq, k)
            ngh += 1
    V["gate_hyst_seqs"] = ngh
    # traza en lazo cerrado con un deposito de juguete
    drain, inflow, L0, T = 10, 30, 70, 25
    s, L = "OFF", L0
    rows, clamps, switches = [], [], 0
    lv, st = [], []
    for t in range(T):
        b, a = int(L < 20), int(L > 80)
        s2 = hf.delta[(s, (b, a))]
        assert s2 == hyst_next(s, L)
        rows.append([t, L, b, a, s, s2])
        lv.append(L)
        st.append(hf.lam[s])
        if s2 != s:
            switches += 1
        raw = L - drain + (inflow if s == "ON" else 0)
        newL = min(100, max(0, raw))
        if newL != raw:
            clamps.append((t + 1, raw, newL))
        L, s = newL, s2
    loop_rows = rows
    V["hyst_loop_table"] = table(
        ["t", "nivel L(t) (%)", "BAJO(t)", "ALTO(t)", "S(t)", "S(t+1)"], rows, ["---:", "---:", "---:", "---:", ":---:", ":---:"]
    )
    V["loop_drain"], V["loop_inflow"], V["loop_L0"], V["loop_T"] = drain, inflow, L0, T
    V["loop_net"] = inflow - drain
    V["loop_switches"] = switches
    V["loop_clamps"] = clamps
    V["loop_switch_ticks"] = ", ".join(str(r[0]) for r in rows if r[4] != r[5])
    V["loop_min"], V["loop_max"] = min(lv), max(lv)
    # ruido alrededor del umbral: umbral unico frente a histeresis
    lvls = [30, 24, 21, 19, 22, 18, 23, 17, 21, 19, 24, 18, 40, 70, 79, 81, 78, 82, 79, 83]
    s = "OFF"
    hy, nv = [], []
    sn = 0
    for L in lvls:
        hy.append(hf.lam[s])
        nv.append(sn)
        s = hyst_next(s, L)
        sn = int(L < 20)
    sw = lambda v: sum(1 for a, b in zip(v, v[1:]) if a != b)  # noqa: E731
    V["chatter_diagram"] = "\n".join(
        [
            cell_row("t", list(range(len(lvls)))),
            cell_row("nivel L", lvls),
            cell_row("umbral 20", ["#" if v else "_" for v in nv]),
            cell_row("histeresis", ["#" if v else "_" for v in hy]),
        ]
    )
    V["chatter_naive"], V["chatter_hyst"] = sw(nv), sw(hy)
    V["chatter_n"] = len(lvls)
    # ---- 2^n: todo sistema sincrono es una maquina de estados -----------------------------------
    V["pow_table"] = table(
        ["bits de memoria n", "estados posibles 2^n"], [[n_, 2**n_] for n_ in (1, 2, 3, 4, 5, 6, 8, 10)], ["---:", "---:"]
    )
    reach = []
    r16 = bfs(0, lambda s: [(s + 1) % 16])
    reach.append(["contador de 4 bits (doc 06)", 4, 16, len(r16)])
    rb = bfs((0, 0), lambda s: [cb[mealy.delta[(inv[s], x)]] for x in (0, 1)])
    reach.append(["detector 101 Mealy, binaria", 2, 4, len(rb)])
    rh = bfs((1, 0, 0), lambda s: [ch[mealy.delta[({v: k for k, v in ch.items()}[s], x)]] for x in (0, 1)])
    reach.append(["detector 101 Mealy, one-hot", 3, 8, len(rh)])
    rmb = bfs((0, 0), lambda s: [cm[moore.delta[(invm[s], x)]] for x in (0, 1)])
    reach.append(["detector 101 Moore, binaria", 2, 4, len(rmb)])
    rmh = bfs((1, 0, 0, 0), lambda s: [tuple(1 if j == moore.states.index(moore.delta[(moore.states[s.index(1)], x)]) else 0 for j in range(4)) for x in (0, 1)])
    reach.append(["detector 101 Moore, one-hot", 4, 16, len(rmh)])
    rhy = bfs(0, lambda s: [hc[hf.delta[(hinv[s], sy)]] for sy in hf.syms])
    reach.append(["histéresis, binaria", 1, 2, len(rhy)])
    rr = bfs(tuple(settled_chain(3)), lambda s: [ring_next(s)])
    reach.append(["anillo de 3 NOT desde cadena estabilizada", 3, 8, len(rr)])
    rz = bfs((0, 0, 0), lambda s: [ring_next(s)])
    reach.append(["anillo de 3 NOT desde 0b000", 3, 8, len(rz)])
    for name, nb, tot, got in reach:
        assert tot == 2**nb and got <= tot
    V["reach_table"] = table(
        ["máquina", "bits de memoria n", "2^n", "estados alcanzables"], reach, [":---", "---:", "---:", "---:"]
    )
    V["reach"] = {r[0]: r[3] for r in reach}
    # grafo de estados del anillo de 3 NOT: todos los ciclos
    seen, cycles = set(), []
    for s0 in itertools.product((0, 1), repeat=3):
        if s0 in seen:
            continue
        cyc, s = [], s0
        while s not in cyc:
            cyc.append(s)
            s = ring_next(s)
        seen |= set(cyc)
        cycles.append(cyc)
    code3 = lambda s: bstr(s[0] * 4 + s[1] * 2 + s[2], 3)  # noqa: E731
    rows = []
    for cyc in cycles:
        k = min(range(len(cyc)), key=lambda i: code3(cyc[i]))
        cyc = cyc[k:] + cyc[:k]
        rows.append([len(cyc), " -> ".join(code3(c) for c in cyc) + " -> " + code3(cyc[0])])
    rows_cyc = sorted(rows)
    V["ring_cycles_table"] = table(["longitud del ciclo", "estados (g1 g2 g3)"], rows_cyc, ["---:", ":---"])
    assert sum(r[0] for r in rows) == 8
    # el contador de 4 bits como maquina de Moore sin entradas: 16 estados, delta(s)=s+1 mod 16
    # (comprobado contra la simulacion de puertas del documento 06)
    o = counter_net(4, "sync", 8, 4).run(4 + 20 * 8)
    cnts = [code_at(o, ["S0", "S1", "S2", "S3"], t) for t in edge_ticks(8, 4, len(o["CLK"]))][:20]
    assert cnts == [j % 16 for j in range(20)]
    # ---- Mini-ejercicio: detector de "11" (Mealy) -----------------------------------------------
    m11, _ = pattern_fsms("11")
    ex_x = [1, 1, 1, 0, 1, 1]
    for L in range(0, 13):
        for xs_ in itertools.product((0, 1), repeat=L):
            for t, row in enumerate(m11.run(xs_)):
                assert row[4] == int(ends_with(xs_, t, "11"))
    V["ex_table"] = table(
        ["Estado", "significado", "x=0: siguiente / Y", "x=1: siguiente / Y"],
        [
            [s, "nada útil" if s == "S0" else "llevo '1'", *[f"{m11.delta[(s, x)]} / {m11.lam[(s, x)]}" for x in (0, 1)]]
            for s in m11.states
        ],
        [":---:", ":---", ":---:", ":---:"],
    )
    V["ex_nstates"] = len(m11.states)
    V["ex_trace"] = table(
        ["t", "x(t)", "S(t)", "S(t+1)", "Y(t)"],
        [[t, x, s, s2, y] for t, x, s, s2, y in m11.run(ex_x)],
        ["---:", "---:", ":---:", ":---:", "---:"],
    )
    V["ex_x"] = "".join(map(str, ex_x))
    V["ex_det"] = ", ".join(str(t) for t in range(len(ex_x)) if ends_with(ex_x, t, "11"))
    V["ex_eq_Y"] = sop_string(["Q", "x"], qm_minimize(2, [3]), 2)
    V["ex_eq_D"] = sop_string(["Q", "x"], qm_minimize(2, [1, 3]), 2)
    # binaria: S0=0, S1=1; D = x ; Y = Q.x
    for q in (0, 1):
        for x in (0, 1):
            s = "S%d" % q
            assert eval_sop(V["ex_eq_D"], {"Q": q, "x": x}) == int(m11.delta[(s, x)][1:])
            assert eval_sop(V["ex_eq_Y"], {"Q": q, "x": x}) == m11.lam[(s, x)]
    # ---- Moore frente a Mealy ----------------------------------------------------------------------
    lag = 1
    V["lag"] = lag
    V["cmp_mm_table"] = table(
        ["", "Moore", "Mealy"],
        [
            ["La salida depende de", "solo del estado", "del estado y de la entrada actual"],
            ["Cuándo reacciona a una entrada", f"{lag} ciclo después", "en el mismo ciclo"],
            ["Estados del detector 101", V["moore_nstates"], V["mealy_nstates"]],
            ["Si la entrada cambia a mitad de ciclo", "la salida no se entera hasta el siguiente flanco", "la salida puede cambiar a mitad de ciclo"],
        ],
        [":---", ":---", ":---"],
    )
    V["trace_x_list"] = ", ".join(str(v) for v in xs)
    V["trace_len"] = len(xs)
    V["ex_x_list"] = ", ".join(str(v) for v in ex_x)
    V["thr_low"], V["thr_high"] = 20, 80
    V["mealy_pat_first2"] = pat[:2]
    V["pat_len"] = len(pat)
    V["mealy_lastcode"] = "0b11"
    V["ill_code"] = "0b11"
    V["detect_first"] = det[0]
    V["detect_second"] = det[1]
    V["moore_first"] = det[0] + 1
    V["cycle_ticks"] = 12
    V["ex_len"] = len(ex_x)
    V["clk_half"] = 6
    V["pat1"], V["pat2"] = pat[:1], pat[:2]
    ov = pat + pat[1:]
    assert sum(1 for t in range(len(ov)) if ends_with([int(c) for c in ov], t, pat)) == 2
    V["pat_overlap"] = ov
    V["pat_commas"] = ", ".join(pat)
    assert det[1] - det[0] == 2
    V["overlap_shared"] = det[0]
    V["hyst_nstates"] = len(hf.states)
    V["gate_hyst_maxlen"] = 6
    V["cnt_total"] = len(r16)
    V["ring_cycle_long"] = max(r[0] for r in rows_cyc)
    t20 = [r[0] for r in loop_rows if r[1] == 20 and r[4] == "OFF"][0]
    t80 = [r[0] for r in loop_rows if r[1] == 80 and r[4] == "ON"][0]
    assert loop_rows[t20][5] == "OFF" and loop_rows[t80][5] == "ON"
    V["b20_t"], V["b80_t"] = t20, t80
    V["clamp_t"], V["clamp_raw"], V["clamp_val"] = V["loop_clamps"][0]
    assert len(V["loop_clamps"]) == 1
    return V


# ---------------------------------------------------------------------------
# 5. Plantillas (la prosa). Todo numero de la prosa sale de un {{marcador}}.
# ---------------------------------------------------------------------------

TPL06 = r'''# 06 · Relojes y contadores

## Objetivo
Entender cómo se fabrica el «tiempo» en un circuito digital: relojes, divisores de frecuencia, contadores y circuitos que reaccionan a un cambio de la señal.  
Al terminar sabrás predecir el periodo de cada montaje y explicar por qué un contador asíncrono (ripple) se equivoca si el reloj va demasiado rápido.

## Idea intuitiva
Un circuito digital solo sabe de ceros y unos. No tiene un reloj de pared ni sabe «esperar un segundo». Para que algo ocurra cada cierto tiempo hace falta una señal que suba y baje sola, una y otra vez: eso es un **reloj**. Piensa en un metrónomo: tic, tac, tic, tac.

Con el metrónomo se construye todo lo demás:

- **Divisor de frecuencia**: un engranaje reductor. Por cada dos vueltas de la rueda de entrada, la de salida da una.
- **Contador**: el cuentakilómetros de un coche. Cada rueda de dígitos avanza cuando la anterior pasa de 9 a 0; en binario, cada bit cambia cuando el anterior pasa de 1 a 0.
- **Detector de flanco**: el timbre de una puerta. Suena en el instante en que pulsas, no mientras mantienes el dedo apretado.
- **Alargador de pulso**: la luz de la escalera. Un toque corto y se queda encendida un rato.

Todo se mide en **pasos**, la unidad mínima de tiempo de nuestro modelo (no son segundos ni «ticks» del juego).

## Explicación paso a paso

### Paso 0. Notación (la misma en toda la serie)

Los bits se escriben en binario con prefijo `0b` y en hexadecimal con prefijo `0x`. El mismo número en tres formas:

{{notation_table}}

Un literal binario lleva tantos dígitos como bits tenga la señal: `{{nt_lit1}}` es una señal de 1 bit, `{{nt_lit3}}` una de {{nt_n3}} bits y `{{nt_lit4}}` una de {{nt_n4}} (los ceros de la izquierda cuentan).

Las operaciones lógicas se escriben con su nombre en las listas de circuitos y en los diagramas (AND, OR, NOT, XOR, NAND, NOR, XNOR) y con símbolos en las fórmulas: `·` es AND, `+` es OR, `¬` es NOT (se pone delante: ¬Q se lee «no Q») y `⊕` es XOR.

{{ops_table}}

Ojo con el `+` y con el `·`: en las ecuaciones lógicas `+` es OR y `·` es AND, pero en una cuenta con números (pasos, anchuras, módulos) el `+` es la suma de siempre y la multiplicación se escribe `×`.

Las ondas se dibujan con un carácter por paso: `#` es 1 y `_` es 0. Encima va una regla de pasos: la primera fila son las decenas y la segunda las unidades. Cuando una fila muestra un grupo de bits como un número (la fila CUENTA), escribo su cifra hexadecimal en el paso en que el valor cambia y un `.` mientras sigue igual.

### Paso 1. El modelo temporal del curso (y por qué es solo un modelo)

Estas son las reglas del juego. Valen para todos los circuitos de este documento.

1. El tiempo avanza a saltos: t = 0, 1, 2... Cada salto es un **paso**: no son segundos ni ticks del juego, solo la unidad del modelo. (Ojo: «Paso» con mayúscula numera los apartados de este documento; «paso» con minúscula es esta unidad de tiempo.)
2. El **estado** del circuito en el instante t es una «foto»: el valor de todas las señales (la salida de cada puerta y de cada flip-flop) en ese paso.
3. **Retardo unitario**: cada elemento tarda exactamente 1 paso en reaccionar. Si en el instante t sus entradas valen algo, su salida lo refleja en t+1: salida(t+1) = función(entradas(t)). Los cables (unir una salida con una entrada) no añaden retardo; solo lo añaden los elementos. Las fuentes (el temporizador del Paso 3 y las entradas de prueba) dan su valor en cada paso por definición.
4. **Evaluación síncrona en dos fases**. *Fase 1*: todos los elementos calculan su valor siguiente leyendo **solo** la foto del instante t (nadie escribe todavía). *Fase 2*: se confirman todos los valores a la vez y esa es la foto de t+1. Consecuencia: da igual en qué orden recorras los elementos (he probado {{order_trials}} órdenes aleatorios en el ejemplo de abajo y el resultado es idéntico).

Un ejemplo mínimo: dos puertas NOT en cadena. La lista de elementos es la que simula el script (una netlist, como las del documento 02 y las del 03). **Ojo, aquí cambia el significado**: en esas netlists cada línea era instantánea (daba el valor final, sin tiempo), mientras que aquí cada línea `y = OP(a, b)` añade un paso: y(t+1) = OP(a(t), b(t)). `arranca en 1` es el valor inicial:

```text
{{net_two_not}}
```

{{two_not_table}}

Lee la tabla: `x` sube a 1 en t={{tn_x}}; `a` lo nota un paso después (cambia en t={{tn_a}}) y `b` otro paso después (cambia en t={{tn_b}}). Dos puertas en cadena, dos pasos de retraso. Y así se ve una transición en las dos fases:

```text
{{two_phase_block}}
```

Fíjate en que `b'` se calcula con el `a` **viejo** (el de la foto), no con el `a'` recién calculado. Eso es lo que hace al modelo independiente del orden.

5. **Flanco de subida**: el instante en que una señal pasa de 0 a 1. Se detecta comparando con el paso anterior: subida(t) = CLK(t) · ¬CLK(t−1). Todo lo que sigue (flip-flops, detector de flancos, contadores) usa esta misma definición.
6. **Flip-flop T** («toggle»): un bit de memoria con una entrada de reloj CLK y una entrada T. Solo actúa cuando hay flanco de subida en CLK: si T = 1 cambia de valor (Q(t+1) = Q(t) ⊕ T(t)) y si T = 0 se queda como está. Para detectar el flanco, el flip-flop recuerda dentro el valor anterior del reloj. Tiene dos salidas que cambian a la vez: Q y su contraria, que se llama `Qn` (= ¬Q), como en el documento 04.

**El flip-flop es aquí una pieza primitiva.** En el documento 04 lo construiste con puertas y allí tardaba varios pasos en reaccionar y exigía que T estuviera quieto cerca del flanco. En este documento lo trato como una caja negra ideal con retardo de 1 paso: Q y Qn cambian a la vez, un paso después del flanco. Las cifras de aquí (periodos mínimos, escalones del ripple) dependen de esa idealización: con el flip-flop del 04 cambian los números, no las ideas, pero cambian mucho. Lo he medido montando en este simulador la misma lista de puertas del 04:

- Con T = 1, Q y Qn cambian {{gt_delay}} pasos después del flanco (no 1) y no a la vez: en cada cambio hay {{gt_both1}} paso en el que los dos valen 1.
- El divisor de frecuencia con T = 1 necesita un reloj de periodo {{gt_div_P}} o más (con el flip-flop ideal, {{gt_div_P_ideal}}). Es el mismo mínimo que mide el documento 04 para el flip-flop T con T = 1 fija. Con un reloj más rápido el flip-flop de puertas no llega a responder a cada flanco y el resultado es incorrecto, no solo más lento.
- Un contador de {{gt_cnt_bits}} bits necesita un reloj de periodo {{gt_sync4}} si es síncrono (con el flip-flop ideal, {{gt_sync4_ideal}}) y {{gt_ripple4}} si es ripple (con el ideal, {{gt_ripple4_ideal}}). La tabla completa está en «Tablas».

Son los mínimos de **estos** montajes: T = 1 fija, o un T que calcula el propio contador y que solo cambia cuando cambian sus Q, no en un instante cualquiera. No son el mínimo para un T cualquiera: si T puede cambiar en instantes arbitrarios (fuera de la ventana prohibida), el documento 04 mide, con un reloj simétrico, un periodo de {{gt_any_P}} o más, porque Q puede tardar hasta {{gt_any_qhi}} pasos en dejar de moverse después del flanco. Por eso el síncrono de {{gt_cnt_bits}} bits (periodo {{gt_sync4}}) puede quedar por debajo de ese valor: su T no es arbitrario. Aquí «periodo» es el número de pasos de un ciclo completo del reloj; en estas medidas el reloj está en alto la mitad del periodo, redondeando hacia abajo, así que con un periodo impar pasa un paso más a 0 que a 1 y no es simétrico como el del 04. Por todo esto los periodos de reloj de este documento solo valen con el flip-flop ideal.

> **Aviso**: esto es un **modelo didáctico**. Sirve para razonar y para que cada cifra de este documento sea reproducible (todas salen de un script de Python del repositorio, `docs/theory/check_C.py`, que simula exactamente estas reglas). El simulador real del mod se decidirá más adelante y podría tener otras reglas: otro retardo por elemento, otra equivalencia entre un paso y el tiempo del juego u otra forma de tratar los flancos.

### Paso 2. Oscilador en anillo: el reloj más simple

Una puerta NOT cuya salida vuelve a su propia entrada nunca se queda quieta: si vale 0 pasa a 1 y si vale 1 pasa a 0. Con retardo unitario, x(t+1) = ¬x(t): un reloj de periodo 2. Para periodos más largos se encadenan n puertas NOT (con **n impar**) y se cierra el anillo. Para n = {{ring_n_example}}:

```text
{{static_ring_sketch}}
```

```text
{{net_ring3}}
```

**Cómo arranca.** Imagina que abres el anillo y mantienes a 0 la entrada de g1. La cadena se asienta sola: g1 = 1, g2 = 0, g3 = 1. Es el estado {{ring3_init}} ({{ring3_init_note}}; escribo g1 g2 g3 como bits, con g1 el más significativo). Ahora cierras el anillo: g1 mira a g3, que vale 1, así que g1 «debería» valer 0 pero vale 1. Esa puerta en desacuerdo (la llamo **defecto**) es lo único que empuja al anillo.

{{ring3_table}}

```text
{{ring3_wave}}
```

Qué se ve: (a) cada nodo es el anterior invertido y retrasado un paso; (b) el defecto avanza una puerta por paso; (c) da la vuelta al anillo en n pasos pero **vuelve invertido** (hay n inversiones y n es impar), así que necesita **dos vueltas** para repetirse. Resultado: periodo = 2n = {{ring3_period}} pasos, {{ring3_high}} en alto y {{ring3_high}} en bajo (ciclo de trabajo del 50 %).

Comprobado por simulación para varios n, siempre desde la cadena abierta y asentada:

{{ring_period_table}}

**El estado inicial importa.** Si en el modelo arrancas con todo a 0b000, las {{ring_zero_defects}} puertas están en desacuerdo a la vez, todas cambian a la vez y el anillo oscila con periodo {{ring_zero_period}}, no con 2n:

```text
{{ring3_zero_wave}}
```

Los 8 estados iniciales posibles de n = {{ring_n_example}}:

{{ring3_all8_table}}

Y para otros n, contando cuántos estados iniciales dan cada periodo:

{{ring_allstates_table}}

Lo que dice la simulación: los defectos son siempre una cantidad impar y cada paso se desplazan una puerta. Con **un solo defecto** el periodo es 2n (comprobado para todos los n de la lista: {{ring_list_n}}); con varios defectos puede salir más corto. Ejemplo: para n = {{ring9_n}} hay {{ring9_short_states}} estados de periodo {{ring9_short_period}}, que son los de {{ring9_defects}} defectos separados {{ring9_spacing}} puertas entre sí. Lección de diseño: **el estado inicial de un bucle hay que decidirlo a propósito**.

**¿Y con n par?** No hay oscilación fiable: con un número par de inversiones la señal vuelve igual que salió (es lo que se llama realimentación positiva), así que, si le pones uno de sus estados estables, el anillo no tiene motivo para cambiar y se queda en él. Con n = 2 los estados {{ring2_fixed}} son estables (el anillo «se acuerda» de un bit: es la base de la memoria; estos circuitos de dos estados estables se llaman biestables, documento 04) y arrancando en 0b00 el modelo síncrono oscila con periodo {{ring2_period_00}}, un efecto del modelo que no debes tomar como comportamiento físico.

**Más retardo, más periodo.** El periodo no depende de cuántas NOT hay sino del **retardo total D** del bucle. Si metes elementos de retardo puro (BUF: copian su entrada con 1 paso de retraso) el periodo sigue siendo 2D mientras el número de inversiones sea impar:

{{loop_delay_table}}

### Paso 3. Reloj generado por un temporizador

El anillo da relojes simétricos (50 %) y de periodo 2D. Para otro periodo u otro ciclo de trabajo se usa un **temporizador**: un elemento con un contador interno c(t+1) = (c(t) + 1) mod P (una suma aritmética) y salida 1 mientras c ≥ P − H. Tiene dos parámetros: el **periodo** P (pasos por ciclo) y los pasos en alto H. El **ciclo de trabajo** es H/P y la **frecuencia** es 1/P ciclos por paso. Empieza en bajo para que no haya un flanco falso en t=0.

{{timer_table}}

```text
{{timer_wave}}
```

El temporizador no tiene magia: es un contador (como los del Paso 5, pero módulo P en vez de módulo 2^n) más un comparador. La diferencia con el anillo es que aquí eliges P y H libremente.

### Paso 4. Divisor de frecuencia con un flip-flop T

Un flip-flop T con T = 1 cambia en **cada** flanco de subida: sube en uno, baja en el siguiente. Para completar un ciclo (subir y volver a bajar) hacen falta dos flancos del reloj, es decir, **dos periodos**. Su salida Q tiene periodo doble. Si la salida de cada etapa es el reloj de la siguiente, k etapas multiplican el periodo por 2^k. Aquí, tres etapas con un reloj de periodo {{ex_base}} (las llamo Z1, Z2 y Z3, y no Q, para no confundirlas con los bits Q0..Q3 del contador del Paso 5):

```text
{{net_divider}}
```

```text
{{div_wave}}
```

{{div_table}}

Los periodos medidos son {{div_periods_txt}}: cada etapa dobla al anterior. Ojo al retardo: Z1 cambia un paso **después** del flanco que la provoca, Z2 otro paso después, y así sucesivamente.

Hay una propiedad útil: la salida queda siempre al 50 %, sea cual sea el ciclo de trabajo de la entrada. Con una entrada en alto {{div_duty_in_ticks}} pasos ({{div_duty_in}}), la salida está en alto {{div_duty_out_ticks}} pasos ({{div_duty_out}}). La razón es que Q solo cambia en los flancos de subida, que están separados un periodo exacto: se queda en alto un periodo de entrada completo y en bajo otro.

### Paso 5. Contadores

Un contador de n bits recorre las cuentas desde 0 hasta 2^n − 1 y vuelve a 0. Con n = 4 son {{cnt_total}} cuentas, de {{count_first_bin}} ({{count_first_hex}}) a {{count_last_bin}} ({{count_last_hex}}); la lista completa está en «Tablas». Cada bit cambia la mitad de veces que el anterior:

{{bit_period_table}}

Para comparar contadores uso **lecturas**: el valor de los cuatro bits en el paso de cada flanco de reloj, que es justo lo que capturaría un flip-flop que los mire.

#### 5.1 Contador asíncrono (ripple)

La etapa 0 usa el reloj externo. Cada etapa siguiente usa como reloj la salida `Qn` de la anterior: cuando Q(k−1) baja de 1 a 0 (el «acarreo», como pasar de 9 a 0 en un cuentakilómetros), `Qn` sube y eso es el flanco que despierta a la etapa k. Lista de elementos (R0..R3 son los bits Q0..Q3; `R0n` es la contraria de R0):

```text
{{net_ripple4}}
```

Si conectaras `Q` en vez de `Qn`, el contador contaría hacia atrás: las primeras lecturas serían {{down_seq}}.

El problema es el nombre: *ripple* significa «ondulación». Cada etapa espera a la anterior, así que los bits cambian **escalonados** (es el mismo problema que el del sumador ripple-carry del documento 05: el acarreo viaja de etapa en etapa y el resultado tarda en asentarse). Alrededor de la cuenta {{rt_from_dec}} → {{rt_to_dec}} (de {{rt_from}} a {{rt_to}}), con el mismo reloj, comparado con el contador síncrono de la siguiente sección (`e` es el paso en el que el flip-flop 0 ve el flanco):

{{ripple_trans_table}}

```text
{{ripple_wave}}
```

Y el ciclo completo del ripple, con el reloj más rápido que aguanta (periodo {{ripple_full_P}}). En la fila CUENTA, entre una cuenta y la siguiente, se cuelan los valores fantasma (cifras de más):

```text
{{ripple_full_wave}}
```

El ripple pasa por {{ripple_wrong_values}}: valores fantasma que la cuenta nunca debería mostrar. Si alguien lee el contador en mitad de la onda, lee basura. La onda tarda {{ripple_settle_ticks}} pasos en asentarse (un paso por etapa).

**Límite de velocidad.** Si el siguiente flanco llega antes de que acabe la onda, la lectura sale mal. Con un reloj de periodo {{fast_P}}:

{{fast_table}}

El ripple falla en {{fast_wrong}} de {{fast_total}} lecturas (la primera en el flanco {{fast_wrong_from}}), justo cuando el acarreo tiene que cruzar varias etapas; el síncrono acierta todas. Buscando el menor periodo P que da lecturas correctas para n = 2..8 bits (tabla completa más abajo) sale, con el flip-flop ideal, que **P mínimo = n**: la onda tarda un paso por etapa, así que cuantos más bits, más lento tiene que ir el reloj.

#### 5.2 Contador síncrono

Aquí todos los flip-flops comparten el mismo reloj, así que todos cambian **en el mismo paso**. Para decidir quién debe cambiar se mira el acarreo: la etapa k cambia cuando todos los bits inferiores valen 1, es decir, T_k = Q0 · Q1 · ... · Q(k−1), con T_0 = 1. Ejemplo: de {{rt_from}} a {{rt_to}} cambian los cuatro bits a la vez porque T_3 = Q0 · Q1 · Q2 = 1.

```text
{{net_sync4}}
```

Mismo intervalo que antes, ahora para el síncrono (S0..S3 de la lista son los bits Q0..Q3):

```text
{{sync_zoom_wave}}
```

Y un ciclo completo con un reloj de periodo {{fast_P}} (la fila CUENTA muestra una cifra por cuenta y vuelve a {{count_first_hex}} en el paso {{sync_wave_wrap_tick}}):

```text
{{sync_wave}}
```

El precio son las puertas AND. En el modelo, una AND de varias entradas tarda 1 paso, así que en las simulaciones, con el flip-flop ideal, el síncrono admite un reloj de periodo {{pmin_sync}} con todos los tamaños probados (de 2 a 8 bits). Con el flip-flop T de puertas del documento 04 el periodo mínimo es mayor y crece con el tamaño ({{gt_sync4}} para {{gt_cnt_bits}} bits; tabla en «Tablas»). Si solo dispones de ANDs de 2 entradas y las encadenas, el retardo vuelve a crecer: el acarreo ahora ondula por las puertas en vez de por los flip-flops (última columna de la tabla de P mínimo).

Regla práctica: **ripple** = barato, lento y con valores fantasma; **síncrono** = más puertas, rápido y todos los bits a la vez.

### Paso 6. Detector de flanco (generador de pulso)

Quieres una señal que valga 1 **un solo paso** cuando x sube, aunque x se quede en 1 mucho tiempo. Truco: comparar x con x retrasada. En subida, x ya vale 1 pero la copia retrasada todavía vale 0. Aquí el propio NOT hace de retardo (tarda 1 paso), así que nx(t) = ¬x(t−1) y no hace falta ningún elemento más:

```text
{{net_edge}}
```

- SUBIDA(t+1) = x(t) · ¬x(t−1): 1 solo cuando x acaba de subir.
- BAJADA(t+1) = ¬x(t) · x(t−1) (es el NOR de x y nx): 1 solo cuando x acaba de bajar.
- AMBOS(t+1) = x(t) ⊕ x(t−1) (es el XNOR de x y nx): 1 en cualquier cambio.

```text
{{edge_wave}}
```

Las subidas de x producen pulsos que empiezan en los pasos {{edge_rise_ticks}} y las bajadas en {{edge_fall_ticks}}; todos duran exactamente 1 paso.

**Pulso más ancho.** Si entre x y el NOT metes k elementos BUF más, la copia retrasada llega k pasos más tarde y el pulso dura 1 + k pasos (como mucho lo que dure la entrada):

{{edge_width_table}}

### Paso 7. Alargador de pulso

Es lo contrario: un pulso corto de entrada, un pulso largo de salida. Se hace con un OR de N copias de x, cada una retrasada un paso más: la salida vale 1 si x valió 1 en cualquiera de los N últimos pasos. Con N = {{stretch_N}}:

```text
{{net_stretch}}
```

ANCHO(t+1) = x(t) + x(t−1) + x(t−2) + x(t−3) (aquí `+` es OR). Cada 1 de entrada se «mantiene» N − 1 pasos de más:

```text
{{stretch_wave}}
```

La anchura de salida, contada en pasos (suma aritmética, no OR), es W + N − 1:

{{stretch_width_table}}

Fíjate en los dos pulsos cercanos del final: sus ventanas se solapan y se funden en uno solo (el alargador es **reactivable**: cada nuevo pulso vuelve a empezar la cuenta).

Hay otra forma de construirlo: un contador que se recarga a N cada vez que x vale 1 y baja de uno en uno; la salida vale 1 mientras el contador no sea 0. Es el mismo circuito visto de otra manera: lo he comprobado con las {{stretch_exhaustive}} entradas posibles de {{stretch_L}} pasos y las salidas coinciden en todas.

## Ejemplo resuelto completo: un «latido» cada {{beat_target}} pasos

**Problema.** Tienes un temporizador de periodo {{beat_base}} (H = 1). Quieres una señal PULSO que dé un pulso de 1 paso cada {{beat_target}} pasos y otra, ANCHO, con esos pulsos alargados a {{stretch_N}} pasos.

**(a) Dividir.** Hace falta periodo {{beat_target}} = {{beat_base}} × 2^k, luego k = {{beat_stages}} flip-flops T en cascada. Su última salida es Z3 y, medido en la simulación, tiene periodo {{beat_q3_period}}.

**(b) Detectar el flanco de subida de Z3.** AND(Z3, NOT Z3) da un pulso de 1 paso cada vez que Z3 sube.

**(c) Alargar.** OR de PULSO con tres copias retrasadas: {{stretch_N}} pasos de ancho.

```text
{{net_beat}}
```

```text
{{beat_wave}}
```

Los pulsos de PULSO empiezan en los pasos {{beat_pulse_ticks}}: separación {{beat_pulse_gap}}, justo lo previsto. ANCHO dura {{beat_wide_width}} pasos. El primer pulso no aparece en el paso 0 sino en el {{beat_first}}: es el **retardo acumulado**, porque cada elemento de la cadena (flip-flops, NOT, AND) suma 1 paso. Lo que importa para un diseño es la separación entre pulsos, no ese arranque.

**Con el flip-flop T de puertas del documento 04** este montaje no funciona con el reloj de periodo {{beat_base}}: el flip-flop no llega a responder a cada flanco. El reloj más rápido que aguanta tiene periodo {{gt_beat_P}}, y entonces el latido sale cada {{gt_beat_gap}} pasos ({{gt_beat_P}} × {{gt_beat_mult}}) en lugar de cada {{beat_target}}.

## Tablas

Las {{cnt_total}} cuentas de un contador de 4 bits:

{{count_table}}

Periodo de reloj mínimo para que las lecturas sean correctas con el flip-flop ideal (comprobado simulando cada contador con relojes de periodo creciente; el reloj está en alto la mitad del periodo, redondeando hacia abajo):

{{pmin_table}}

Lo mismo con el flip-flop T de puertas del documento 04 (mismo simulador y mismo criterio, de {{gt_n_lo}} a {{gt_n_hi}} bits; la columna ideal se repite para comparar):

{{gt_pmin_table}}

Recetas rápidas:

{{recipes_table}}

## Mini-ejercicio

Tienes un reloj de periodo {{ex_base}} pasos (temporizador con H = 2).

(a) ¿Cuántos flip-flops T en cascada necesitas para obtener una señal de periodo {{ex_target}} pasos?

(b) Quieres usar un contador ripple de {{ex_n}} bits hecho con flip-flops ideales. Extrapolando la tabla de P mínimo del flip-flop ideal, ¿cuál es el periodo de reloj mínimo para que sus lecturas sean fiables?

---

**Solución**

(a) {{ex_stages}} flip-flops. Cada etapa dobla el periodo: {{ex_periods_arrow}}. Con solo dos etapas el periodo sería {{ex_two_stage}}.

```text
{{ex_wave}}
```

(b) P mínimo = n, luego {{pmin10}} pasos (con flip-flops ideales). Lo he comprobado simulando el ripple de {{ex_n}} bits: con ese periodo todas las lecturas son correctas.

## En Redstone / en Satisfactory
- **Redstone**: según la wiki de Minecraft, la antorcha de redstone invierte la señal («como una puerta NOT») y tarda 2 game ticks en cambiar ([Redstone Torch](https://minecraft.wiki/w/Redstone_Torch)); el repetidor añade un retardo ajustable de 2, 4, 6 u 8 game ticks ([Redstone Repeater](https://minecraft.wiki/w/Redstone_Repeater)); los relojes se hacen cerrando bucles de repetidores y antorchas, y un flip-flop T sirve para duplicar el periodo de cualquier reloj ([Redstone circuits/Clock](https://minecraft.wiki/w/Redstone_circuits/Clock)). Es el anillo y el divisor de este documento.
- **Ojo con Minecraft**: según la wiki, una antorcha que se apaga a la fuerza más de {{mc_burn_n}} veces en {{mc_burn_ticks}} game ticks se «quema» ([Redstone Torch](https://minecraft.wiki/w/Redstone_Torch)), y un bucle de una sola antorcha, como el anillo de 1 NOT de periodo {{ring_n1_period}} de este documento, es el caso típico. Nuestro modelo no tiene esa protección. Además Java y Bedrock difieren en este detalle (en Bedrock, según la wiki, solo se quema si la antorcha se alimenta a sí misma sin retardo), así que las cifras de Minecraft son una referencia, no una especificación.
- **Satisfactory**: el mod aún no está diseñado, así que aquí solo quedan preguntas abiertas: ¿cuánto tiempo del juego dura un paso?, ¿cada pieza añade 1 paso de retardo?, ¿habrá un temporizador como pieza primitiva o se construirá con bucles?, ¿con qué estado arranca un bucle?

## Resumen
- Un reloj es una señal periódica con periodo P y ciclo de trabajo H/P; un anillo de n NOT (n impar, arrancado desde la cadena asentada) da periodo 2n, y con retardo total D da 2D.
- Un flip-flop T con T = 1 divide la frecuencia entre 2 y deja la salida al 50 %; k etapas en cascada dividen entre 2^k.
- Un contador de n bits cuenta de 0 a 2^n − 1: el ripple es simple pero sus bits cambian escalonados (valores fantasma) y, con el flip-flop ideal, exige un periodo de reloj mínimo igual a n; el síncrono cambia todos los bits a la vez con T_k = Q0 · ... · Q(k−1). Con el flip-flop T de puertas del documento 04 todos esos periodos mínimos son mayores (tabla en «Tablas»).
- El detector de flanco compara x con x retrasada (x · ¬x retrasada) y da un pulso de 1 paso; el NOT ya hace de retardo.
- El alargador es un OR de N copias retrasadas (o un contador que se recarga): el ancho pasa a W + N − 1.
'''


TPL07 = r'''# 07 · Máquinas de estado

## Objetivo
Entender qué es una máquina de estados (estados, entradas, función de transición y función de salida), la diferencia entre Moore y Mealy y cómo se implementa con flip-flops D y puertas.  
Al terminar habrás diseñado dos máquinas completas, un detector de la secuencia {{pat}} y un controlador con histéresis, y sabrás por qué todo circuito síncrono es, en el fondo, una de ellas.

## Idea intuitiva
Una máquina de estados es una libreta con **una sola anotación**. Cada vez que llega un dato haces lo mismo: miras lo que pone la libreta (el **estado**) y el dato nuevo; con esas dos cosas decides qué anotar a continuación (el **estado siguiente**) y qué contestar (la **salida**). Un semáforo, un ascensor o una máquina expendedora funcionan así: no importa **cómo** llegaste a donde estás, solo importa **dónde** estás.

La libreta tiene sitio para un número finito de anotaciones, así que la máquina no recuerda todo el pasado, solo un resumen: lo justo para responder bien. Diseñar una máquina de estados es decidir **qué hace falta recordar**.

Un ejemplo cotidiano con memoria es un termostato que enciende la calefacción por debajo de una temperatura y la apaga por encima de otra más alta. Entre las dos no puede decidir mirando solo la temperatura: tiene que **recordar** si estaba encendida. Ese bit de memoria es el estado (lo verás en el ejemplo 2).

## Explicación paso a paso

### Paso 0. Tiempo y notación

**Cambio de escala (modelo didáctico).** En el documento 06 cada puerta tenía un retardo de 1 paso del modelo. Aquí cambiamos de escala: un paso t es un **ciclo de reloj** completo y asumimos que la lógica combinacional (las puertas, que no tienen memoria) tiene tiempo de asentarse dentro del ciclo, antes del siguiente flanco. Eso se cumple si el periodo del reloj es suficiente (es la idea del «P mínimo» del documento 06). Con esa hipótesis la máquina queda muy limpia: S(t+1) = δ(S(t), x(t)).

Es una versión a escala de ciclo de las dos fases del 06. *Fase 1*: la lógica calcula el estado siguiente leyendo solo el estado actual y las entradas. *Fase 2*: en el flanco, todos los flip-flops capturan su nuevo valor a la vez. Al final del ejemplo 1 comprobaré que la versión abstracta y un circuito de puertas con retardo unitario dan los mismos resultados cuando el periodo del reloj es suficiente. Sigue siendo un modelo didáctico: el simulador real del mod se decidirá después.

La notación es la misma que en el 06: bits como `0b0110`, hexadecimal como `0x6`, y en las fórmulas `·` es AND, `+` es OR, `¬` es NOT (se pone delante: ¬x se lee «no x») y `⊕` es XOR (precedencia como en el documento 02: primero ¬, luego ·, luego +):

{{ops_table}}

Como en el 06, un literal binario lleva tantos dígitos como bits tenga la señal (`{{nt_lit2}}` es un estado de {{nt_n2}} bits y `{{nt_lit1}}` una señal de 1 bit), y en las ecuaciones lógicas el `+` es OR pero en una cuenta con números (como (s + 1) mod {{cnt_total}}) es la suma de siempre; la multiplicación se escribe `×`. Una combinación «no importa» se escribe «-» en las tablas de verdad y X en los mapas de Karnaugh (documento 02).

Las ondas se dibujan con un carácter por ciclo: `#` es 1 y `_` es 0.

### Paso 1. Definición formal

Una máquina de estados finita (FSM, *finite-state machine*) es una séxtupla M = (S, X, Y, δ, λ, s0):

- **S**: conjunto **finito** de estados.
- **X**: conjunto de entradas posibles (con k bits de entrada hay 2^k).
- **Y**: conjunto de salidas posibles.
- **δ** (delta): S × X → S, la **función de transición**. Dice a qué estado pasas.
- **λ** (lambda): la **función de salida**. Dice qué contestas (Paso 2).
- **s0**: el estado inicial.

En cada ciclo t, el estado siguiente es S(t+1) = δ(S(t), x(t)) y la salida es Y(t) = λ(S(t), x(t)) o Y(t) = λ(S(t)) según el tipo de máquina.

Una máquina se puede escribir de tres formas equivalentes: **tabla de estados** (una fila por estado, una columna por entrada), **diagrama de estados** (círculos y flechas) y **ecuaciones** (lógica). Los dos ejemplos las usan todas. Y se diseña así: (1) ¿qué necesito recordar?, (2) cada respuesta distinta es un estado, (3) para cada estado y cada entrada, ¿a dónde voy y qué contesto?

### Paso 2. Moore frente a Mealy

Hay dos variantes, que llevan el nombre de Edward F. Moore y George H. Mealy:

- **Moore**: λ: S → Y. La salida depende **solo del estado**. Como el estado solo cambia en el flanco, la salida reacciona {{lag}} ciclo después de la entrada que la provoca.
- **Mealy**: λ: S × X → Y. La salida depende del estado **y de la entrada actual**: reacciona en el mismo ciclo y suele necesitar menos estados, porque parte de la información la lleva la propia entrada.

En el detector de {{pat}} del ejemplo 1, el Mealy necesita {{mealy_nstates}} estados y el Moore {{moore_nstates}}, y la salida del Moore llega {{lag}} ciclo más tarde (comprobado con todas las cadenas de hasta {{exh_len}} bits). La comparación resumida está en «Tablas».

### Paso 3. Codificación de estados: binaria frente a one-hot

El hardware no entiende nombres como S0 o S1: cada estado se guarda como un patrón de bits en flip-flops. Elegir ese patrón es **codificar** los estados. Dos opciones habituales:

- **Binaria**: el número del estado escrito en binario. N estados necesitan ceil(log2 N) flip-flops: pocos, pero cada ecuación mezcla todos los bits.
- **One-hot** (un solo bit activo): un flip-flop por estado y solo uno vale 1 a la vez. Más flip-flops, pero cada ecuación es simplemente «OR de las flechas que llegan a ese estado» y la salida suele ser un cable.

{{enc_table}}

Los códigos sin usar son **estados ilegales**: solo se alcanzan por un fallo. Hay que decidir qué hacer con ellos; aquí los trataré como «no importa» (en inglés *don't care*; en las tablas de verdad se escribe «-» y en los mapas X, como en el documento 02) para simplificar más, y **comprobaré a dónde llevan**. Además, el arranque (reset) debe dejar los flip-flops en el código del estado inicial; en one-hot eso significa un 1 en exactamente un flip-flop.

### Paso 4. Implementación con flip-flops D y lógica combinacional

Un flip-flop D copia su entrada D cuando hay flanco de subida: Q(t+1) = D(t). Una máquina con k bits de estado son k flip-flops D (el **registro**) más lógica combinacional que calcula, a partir de Q y de x, las D (el estado siguiente) y la salida Y:

```text
{{static_fsm_block}}
```

Receta:

1. Codifica los estados (Paso 3).
2. Escribe la tabla de verdad: para cada combinación de Q y x, los valores de cada D y de Y.
3. Cada columna de salida es una suma de productos: cada fila con 1 aporta un término AND con todas las variables (negadas las que valen 0) y se suman con OR. Después se simplifica.
4. Cablea: las D a la lógica, las Q de vuelta a la lógica, un reloj común.

En una máquina de Moore, Y sale solo de las Q (a la lógica de salida no llega x).

### Paso 5. Todo sistema digital síncrono es una máquina de estados

Cualquier circuito síncrono (reloj, flip-flops y puertas) se puede ver así: sus n flip-flops guardan n bits, el estado completo es ese patrón de bits y la lógica combinacional calcula el siguiente a partir del actual y de las entradas. Por tanto es una máquina de estados con **como mucho 2^n estados**. (Ojo con la escala: este n cuenta los flip-flops, es decir, la escala del ciclo de reloj de este documento. A la escala de pasos del documento 06 la foto del circuito incluye además la salida de cada puerta y el valor anterior del reloj que guarda cada flip-flop, así que n cuenta todo eso; el anillo de 3 NOT de abajo es un ejemplo de esa segunda cuenta, una variable por puerta.)

{{pow_table}}

Ejemplos del documento 06. A escala de ciclo, el contador de 4 bits es una máquina de Moore sin entradas con {{cnt_total}} estados y δ(s) = (s + 1) mod {{cnt_total}} (aquí `+` es la suma de números; comprobado contra la simulación de puertas del 06). El anillo de 3 NOT, con una variable de estado por puerta, es una máquina autónoma de 8 estados; su diagrama de estados completo son dos ciclos (los estados se escriben como bits g1 g2 g3, con g1 el más significativo):

{{ring_cycles_table}}

El ciclo largo es el oscilador de periodo {{ring_cycle_long}}; el corto es el que sale si arrancas en 0b000.

La consecuencia es una idea muy potente, pero **solo para las máquinas autónomas**: las que no tienen entradas (o las tienen fijas), como un reloj, un contador o el anillo de NOT. Como son **deterministas** (el estado siguiente solo depende del actual) y tienen un número **finito** de estados, si alguna vez repiten un estado repiten todo lo que viene después. Con 2^n estados posibles, en como mucho 2^n pasos hay una repetición: toda trayectoria acaba en un ciclo. El «periodo» de un reloj es la longitud de ese ciclo y **nunca puede superar 2^n**. Y de los 2^n patrones posibles solo se alcanzan los que salen del estado inicial (tabla de estados alcanzables en «Tablas»).

Con entradas que cambian a voluntad esto **no** vale: repetir un estado no repite el futuro, porque el futuro también depende de las entradas que lleguen. Lo verás en la traza del detector {{pat}} (ejemplo 1, más abajo): el Mealy está en {{cex_s}} en t={{cex_t1}} y en t={{cex_t2}}, y pasa a {{cex_n1}} la primera vez y a {{cex_n2}} la segunda, porque la entrada fue {{cex_x1}} y después {{cex_x2}}.

## Ejemplo resuelto completo

### Ejemplo 1: detector de la secuencia {{pat}} (Mealy y Moore)

**Especificación.** Entra un bit por ciclo. La salida Y debe valer 1 cuando los {{pat_len}} últimos bits recibidos sean {{pat_commas}}, en ese orden. Se admite **solapamiento**: en {{pat_overlap}} aparece {{pat}} dos veces (comparten el 1 central).

**Qué hay que recordar.** No hace falta guardar todos los bits, solo «hasta dónde llevo del {{pat}}». El estado S_k significa «el mayor trozo final de lo recibido que coincide con el principio de {{pat}} tiene k bits». S0: nada útil. S1: llevo `{{pat1}}`. S2: llevo `{{pat2}}`.

#### Mealy ({{mealy_nstates}} estados)

```text
{{mealy_formal}}
```

{{mealy_table}}

Cada casilla es «estado siguiente / salida». Solo hay un 1 de salida: en S2 con x = 1 (llevaba `{{pat2}}` y llega un 1). Diagrama (las flechas se rotulan x/Y; la flecha de entrada marca el estado inicial):

```text
{{mealy_diagram}}
```

#### Moore ({{moore_nstates}} estados)

Para que la salida dependa solo del estado hace falta un estado más, S3 = «acabo de ver {{pat}}», que es el único con Y = 1.

```text
{{moore_formal}}
```

{{moore_table}}

```text
{{moore_diagram}}
```

En el diagrama de Moore la salida va dentro de cada estado (`Y=0`, `Y=1`) y las flechas solo llevan la entrada x.

#### Traza simulada

Entrada, un bit por ciclo, de izquierda a derecha: {{trace_x_list}} ({{trace_len}} ciclos). S(t) es el estado al principio del ciclo t y S(t+1) el que resulta tras leer x(t).

Mealy:

{{mealy_trace}}

Moore:

{{moore_trace}}

```text
{{trace_wave}}
```

Resultado: hay {{trace_ndet}} apariciones de {{pat}}, que terminan en los ciclos {{trace_det_mealy}}. El Mealy pone Y = 1 **en esos mismos ciclos** y el Moore **un ciclo después**, en {{trace_det_moore}}. Fíjate en el solapamiento: las apariciones que terminan en {{detect_first}} y en {{detect_second}} comparten el 1 del ciclo {{overlap_shared}}.

**Verificación exhaustiva.** He comparado ambas máquinas con la definición («los {{pat_len}} últimos bits son {{pat}}») para las {{exh_count}} cadenas de entre 0 y {{exh_len}} bits: el Mealy coincide en el mismo ciclo y el Moore un ciclo más tarde, sin excepciones.

#### Implementación del Mealy con flip-flops D

Codificación binaria (2 flip-flops, Q1 es el bit alto) y one-hot (3 flip-flops):

{{mealy_codes_bin}}

{{mealy_codes_hot}}

Tabla de verdad con la codificación binaria. El código {{ill_code}} no corresponde a ningún estado, así que sus filas valen «-» (no importa):

{{mealy_bin_truth}}

Para ver por qué conviene simplificar: D0 vale 1 en tres filas, y escrito como suma de productos son tres términos:

```formulas
{{mealy_bin_D0_canon}}
```

Pero las filas «sin usar» son libres: son combinaciones que no pueden ocurrir (o cuya salida da igual), y en un mapa de Karnaugh se marcan con una **X**. Cada X se puede contar como 0 o como 1, lo que deje los grupos más grandes. Para simplificar uso mapas de Karnaugh (documento 02, Paso 6): con tres variables basta un mapa por salida, con Q1 en las filas y Q0 x en las columnas, en orden Gray. El número de minterm sale de leer Q1 Q0 x como un número binario (Q1 es el bit alto, como en el documento 02); los minterms {{km_ill_minterms}} son las filas sin usar.

```text
{{km_D1}}
```

```text
{{km_D0}}
```

```text
{{km_Y}}
```

Los grupos que salen: en D1, {{km_D1_txt}}; en D0, {{km_D0_txt}}; en Y, {{km_Y_txt}}. Las X se notan: sin aprovecharlas, las ecuaciones serían más largas.

{{km_xcmp_table}}

Estas son las ecuaciones que resultan. El método mecánico equivalente al mapa es Quine-McCluskey, y es el que usa el script de comprobación (`docs/theory/check_C.py`) para calcularlas y verificar cada una contra la tabla para todas las entradas:

```formulas
{{mealy_bin_eqs}}
```

Se leen en voz alta: «el bit alto del siguiente estado es 1 si vengo de S1 (Q0 = 1) y llega un 0»; «el bit bajo es simplemente x» (cualquier 1 de entrada lleva a S1, de código 0b01); «Y = 1 si estoy en S2 (Q1 = 1) y llega un 1».

**¿Qué pasa si el circuito cae por error en el código ilegal {{ill_code}}?** Las ecuaciones dan: {{mealy_bin_illegal_txt}}. Es decir, vuelve a un estado legal en un solo ciclo y no se queda atrapado. (Conviene comprobarlo siempre: depende de cómo hayas simplificado.)

**One-hot por lectura de flechas.** Cada flip-flop N_j (el siguiente valor del estado S_j) es el OR de las flechas que llegan a S_j, cada una con su condición:

```formulas
{{mealy_hot_eqs}}
```

Como en one-hot siempre hay **exactamente un** E activo, E0 + E1 + E2 = 1 (aquí `+` es OR), y de ahí salen dos simplificaciones: `{{hot_simplify}}`, porque sus tres flechas cubren todos los estados, y E0 + E2 = ¬E1, así que N0 pasa a ser {{hot_min_N0}}. Además, los {{hot_unused}} códigos one-hot sin usar también son «no importa». Con todo eso (el mismo método, ahora con {{hot_nvars}} variables: E2, E1, E0 y x) queda:

```formulas
{{mealy_hot_min_eqs}}
```

Los {{hot_unused}} códigos ilegales vuelven también a un estado legal en un ciclo, con x = 0 y con x = 1 (comprobado).

Para comparar las dos codificaciones hay que contar de la misma manera, y la tabla de «Tablas» lo hace de dos formas. Con las dos simplificadas por igual, la binaria gana en flip-flops ({{ff_bin}} frente a {{ff_hot}}), en términos ({{term_bin}} frente a {{term_hot_min}}) y en literales ({{lit_bin}} frente a {{lit_hot_min}}), aunque la diferencia de lógica es pequeña. Sin simplificar, la one-hot leída de las flechas incluso queda algo más corta en literales que la forma canónica de la binaria ({{lit_hot}} frente a {{lit_bin_c}}), porque nombra cada estado con un literal en vez de dos. Es un único ejemplo: no lo extrapoles a otros tamaños sin medirlo.

**Moore, codificación binaria** (los 4 códigos están en uso, así que no hay X; se simplifica con los mismos mapas):

{{moore_codes_bin}}

{{moore_bin_truth}}

```formulas
{{moore_bin_eqs}}
```

**Verificación en puertas.** Las ecuaciones anteriores (binaria, one-hot leída de las flechas, one-hot simplificada y Moore) se han montado con flip-flops D y puertas de retardo unitario (NOT, AND, OR, y BUF para copiar una señal tal cual), con un reloj de periodo {{cycle_ticks}} pasos, y se han simulado contra la tabla abstracta con las {{gate_seqs}} cadenas de entrada de 1 a {{gate_maxlen}} bits. En cada ciclo coinciden el estado y la salida Y (observados en el paso del flanco de reloj, justo antes de capturar). Con un periodo de reloj suficiente (aquí, {{cycle_ticks}} pasos), el modelo abstracto de este documento y un circuito de puertas como los del documento 06 dan, por tanto, los mismos resultados.

### Ejemplo 2: controlador con histéresis (la bomba y el depósito)

**Especificación.** Un actuador (por ejemplo, una bomba que llena un depósito) se controla con el nivel L del depósito, en tanto por ciento y con valores enteros:

- Si L < {{thr_low}}, se **enciende** (ON).
- Si L > {{thr_high}}, se **apaga** (OFF).
- En cualquier otro caso (de {{thr_low}} a {{thr_high}} **ambos incluidos**) se queda como estaba.

Esa tercera regla es la histéresis: entre los dos umbrales la salida depende de la **historia**, y la historia es memoria. Estado = «¿estoy encendido?».

**Entradas.** Dos comparadores convierten el nivel en dos bits, BAJO = (L < {{thr_low}}) y ALTO = (L > {{thr_high}}). Nunca valen 1 a la vez, así que la combinación BAJO = 1, ALTO = 1 es **imposible** y se tratará como «no importa»:

{{hyst_cmp_table}}

**Máquina de Moore con {{hyst_nstates}} estados** (la salida Y = 1 significa ON y es el propio estado):

{{hyst_table}}

```text
{{hyst_diagram}}
```

Cada estado se queda en sí mismo mientras no llegue su condición de salida: OFF espera a BAJO = 1 y ON espera a ALTO = 1.

**Implementación.** Con una codificación binaria de 1 bit (OFF = 0b0, ON = 0b1) basta un flip-flop D. Tabla de verdad (las filas con BAJO = 1 y ALTO = 1 son imposibles: «-»):

{{hyst_truth}}

Mapa de Karnaugh de D (filas: Q; columnas: BAJO ALTO en orden Gray; las dos casillas imposibles son X):

```text
{{km_hyst}}
```

Salen estos grupos: {{km_hyst_txt}}. Ecuaciones:

```formulas
{{hyst_eqs}}
```

D = BAJO + Q · ¬ALTO se lee «estaré encendido si ahora tengo nivel bajo o si ya estaba encendido y no hay nivel alto». Equivale a un **biestable SR** (el nombre general de latches y flip-flops, documento 04; set = BAJO, reset = ALTO): la histéresis es un bit de memoria con dos entradas. Montada en puertas con flip-flop D y simulada contra la tabla con las {{gate_hyst_seqs}} secuencias de entradas posibles de 1 a {{gate_hyst_maxlen}} ciclos, coincide en todas.

**Traza simulada en lazo cerrado.** Para poder simular hace falta un depósito de juguete (no es el modelo de ningún juego): en cada paso pierde {{loop_drain}} puntos por consumo y, mientras el actuador está ON, gana {{loop_inflow}} (lo que aporte al depósito cuando está encendido), con el nivel limitado entre 0 y 100. Empieza con L = {{loop_L0}} y el actuador en OFF. Con el actuador ON el nivel sube un neto de {{loop_net}} puntos por paso.

{{hyst_loop_table}}

Lectura de la traza:

- La máquina cambia de estado al terminar los ciclos {{loop_switch_ticks}}: {{loop_switches}} cambios en {{loop_T}} ciclos.
- En t={{b20_t}} el nivel vale exactamente {{thr_low}} y **no** se enciende (la condición es L < {{thr_low}}); en t={{b80_t}} vale exactamente {{thr_high}} y **no** se apaga.
- La decisión se toma con el nivel de t y se nota en t+1: por eso el nivel sigue subiendo un ciclo más y en t={{clamp_t}} se saturaría en {{clamp_raw}} (se limita a {{clamp_val}}).

**Por qué hace falta histéresis.** Con un único umbral, si el nivel «tiembla» alrededor de él, el actuador conecta y desconecta sin parar. Se compara un controlador de umbral único en {{thr_low}} (ON en el ciclo siguiente a tener L < {{thr_low}}, OFF en los demás) con el de histéresis, con una entrada que oscila alrededor de {{thr_low}} (cada celda es un ciclo; `#` es ON):

```text
{{chatter_diagram}}
```

En {{chatter_n}} muestras, el umbral único cambia de estado {{chatter_naive}} veces; la histéresis, {{chatter_hyst}}.

## Tablas

Moore frente a Mealy:

{{cmp_mm_table}}

Coste de las dos codificaciones en el Mealy del ejemplo 1. Cada una se cuenta de dos formas, y solo son comparables las filas con el mismo criterio (las dos primeras, y las dos últimas): «simplificada» es la suma de productos mínima con los códigos sin usar como X; «sin simplificar» es la forma directa, un término por cada fila con 1 de la tabla de verdad (binaria) o por cada flecha (one-hot).

{{cmp_table}}

Estados posibles frente a estados alcanzables desde el estado inicial (n es el número de bits de memoria: flip-flops a escala de ciclo; en el anillo, una variable por puerta):

{{reach_table}}

## Mini-ejercicio

Diseña un detector de **dos unos seguidos** (secuencia 11, con solapamiento) de tipo Mealy.

(a) ¿Cuántos estados necesita?

(b) Escribe su tabla de estados.

(c) Simula la entrada {{ex_x_list}} (un bit por ciclo, de izquierda a derecha).

(d) Con un solo flip-flop D (S0 = 0b0, S1 = 0b1), ¿cuáles son las ecuaciones de D e Y?

---

**Solución**

(a) {{ex_nstates}} estados: S0 («nada útil», el último bit no fue 1) y S1 («llevo `1`»).

(b)

{{ex_table}}

(c) Y vale 1 en los ciclos {{ex_det}}. Con solapamiento, tres unos seguidos dan dos apariciones.

{{ex_trace}}

(d) D = {{ex_eq_D}} (cualquier 1 lleva a S1 y cualquier 0 a S0) e Y = {{ex_eq_Y}} (estoy en S1 y llega otro 1). Verificado contra la tabla y contra la definición para todas las cadenas de hasta {{exh_len}} bits.

## En Redstone / en Satisfactory
- **Redstone**: la wiki de Minecraft clasifica los circuitos de memoria en latches y flip-flops RS, T, D y JK ([Redstone circuits/Memory](https://minecraft.wiki/w/Redstone_circuits/Memory)) y construye las puertas con antorchas: una antorcha con una entrada es un NOT y con dos o más un NOR ([Redstone circuits/Logic](https://minecraft.wiki/w/Redstone_circuits/Logic)). Una máquina de estados es justo «memoria + puertas + reloj».
- **Satisfactory**: el mod aún no está diseñado, así que quedan como preguntas abiertas cuántos bits de memoria tendrá una pieza, si ese estado se guardará con la partida y qué estado tendrá al construirla.

## Resumen
- Una máquina de estados es M = (S, X, Y, δ, λ, s0) con S(t+1) = δ(S(t), x(t)): el estado es lo que recuerdas del pasado.
- Moore saca la salida solo del estado (en el 101, un estado más y un ciclo de retraso); Mealy la saca del estado y de la entrada (menos estados, misma sincronía que la entrada).
- Los estados se codifican en binario (ceil(log2 N) flip-flops) o en one-hot (N flip-flops, ecuaciones que se leen de las flechas).
- Se implementan con un registro de flip-flops D más lógica combinacional, y cada ciclo tiene dos fases, parecidas a las del documento 06: «calcular las D» y «capturar en el flanco»; se ha verificado también a nivel de puertas.
- La histéresis es un bit de memoria entre dos umbrales, y todo circuito síncrono con n bits de memoria es una máquina de estados con como mucho 2^n estados.
'''


# ---------------------------------------------------------------------------
# 6. Render, lint de plantillas y verificacion de los .md
# ---------------------------------------------------------------------------

PH = re.compile(r"\{\{(\w+)\}\}")
TOKEN = re.compile(r"0b[01]+|0x[0-9A-F]+|\d+")
FENCE = re.compile(r"^```(\w*)[^\n]*\n(.*?)^```[ \t]*$", re.S | re.M)
DOCS_H2 = [
    "Objetivo",
    "Idea intuitiva",
    "Explicación paso a paso",
    "Ejemplo resuelto completo",
    "Tablas",
    "Mini-ejercicio",
    "En Redstone / en Satisfactory",
    "Resumen",
]


def render(tpl, V):
    def sub(m):
        k = m.group(1)
        if k not in V:
            raise KeyError(f"marcador sin valor generado: {k}")
        return str(V[k])

    return PH.sub(sub, tpl)


def generated_tokens(*Vs):
    toks = set()
    for V in Vs:
        for v in V.values():
            toks.update(TOKEN.findall(str(v)))
    return toks


def lint_template(tpl, V, gen_tokens, name):
    """La prosa no puede traer cifras tecleadas a mano que el script no haya generado."""
    # 1. los bloques de codigo solo contienen un marcador
    for m in FENCE.finditer(tpl):
        inner = m.group(2).strip()
        mm = PH.fullmatch(inner)
        assert mm, f"{name}: bloque de codigo con texto a mano: {inner[:60]!r}"
        assert mm.group(1) in V, f"{name}: falta {mm.group(1)}"
    # 2. ninguna linea de tabla escrita a mano
    for ln in tpl.splitlines():
        assert not ln.lstrip().startswith("|"), f"{name}: tabla a mano: {ln[:60]!r}"
    # 3. cifras en prosa
    prose = FENCE.sub("", tpl)
    bad = []
    for ln in prose.splitlines():
        if ln.startswith("#"):
            continue
        ln = PH.sub("", ln)
        ln = re.sub(r"^\s*\d+\.\s", "", ln)
        ln = re.sub(r"\b(documentos?|doc|el|del|al) 0\d\b", "", ln)
        for tok in TOKEN.findall(ln):
            if tok in ("0", "1", "2"):
                continue
            if tok not in gen_tokens:
                bad.append((tok, ln.strip()[:80]))
    assert not bad, f"{name}: cifras tecleadas que el script no genera: {bad}"


def block_keys(tpl, V):
    """Claves usadas en la plantilla cuyo valor es multilinea o tabla."""
    ks = []
    for k in dict.fromkeys(PH.findall(tpl)):
        v = V[k]
        if isinstance(v, str) and ("\n" in v or v.startswith("|")):
            ks.append(k)
    return ks


def check_structure(text, name):
    # orden de secciones
    h2 = [ln[3:].strip() for ln in text.splitlines() if ln.startswith("## ")]
    h2n = [h.split(":")[0] if h.startswith("Ejemplo resuelto completo") else h for h in h2]
    assert h2n == DOCS_H2, (name, h2n)
    sec = {}
    cur = None
    for ln in text.splitlines():
        if ln.startswith("## "):
            cur = ln[3:].split(":")[0].strip()
            sec[cur] = []
        elif cur is not None:
            sec[cur].append(ln)
    # Objetivo: exactamente 2 lineas
    obj = [ln for ln in sec["Objetivo"] if ln.strip()]
    assert len(obj) == 2, (name, obj)
    # Redstone/Satisfactory: 2 a 5 lineas
    rs = [ln for ln in sec["En Redstone / en Satisfactory"] if ln.strip()]
    assert 2 <= len(rs) <= 5, (name, len(rs))
    # Resumen: exactamente 5 vinetas y nada mas
    rsm = [ln for ln in sec["Resumen"] if ln.strip()]
    assert len(rsm) == 5 and all(ln.startswith("- ") for ln in rsm), (name, rsm)
    # Mini-ejercicio: enunciado, separador, solucion
    me = sec["Mini-ejercicio"]
    assert me.count("---") == 1, name
    i = me.index("---")
    assert any(ln.strip() for ln in me[:i]) and any(ln.strip().startswith("**Solución**") for ln in me[i + 1 :]), name
    # solo ASCII en los bloques de codigo (diagramas)
    nblocks = 0
    for m in FENCE.finditer(text):
        info, inner = m.group(1), m.group(2)
        assert info in ("text", "formulas"), (name, info)
        if info == "formulas":  # ecuaciones: usan los simbolos de la notacion de la serie (·, +, ¬)
            continue
        nblocks += 1
        assert all(ord(ch) < 128 for ch in inner), (name, [ch for ch in inner if ord(ch) >= 128][:5])
        assert "\t" not in inner
        assert all(not ln.endswith(" ") for ln in inner.splitlines()), (name, "espacios al final")
    # sin marcadores sin resolver
    assert "{{" not in text and "}}" not in text, name
    return nblocks


def verify(path, tpl, V, name):
    assert path.exists(), f"no existe {path}"
    text = path.read_text(encoding="utf-8")
    exp = render(tpl, V)
    if text != exp:
        a, b = text.splitlines(), exp.splitlines()
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y:
                raise AssertionError(f"{name}: el .md difiere de lo generado en la linea {i + 1}:\n  disco : {x!r}\n  script: {y!r}")
        raise AssertionError(f"{name}: el .md difiere de lo generado (distinta longitud: {len(a)} vs {len(b)} lineas)")
    lines = set(text.splitlines())
    nlines = 0
    for k in block_keys(tpl, V):
        v = V[k]
        assert v in text, f"{name}: bloque {k} no aparece literalmente"
        for ln in v.splitlines():
            assert ln in lines, f"{name}: linea de {k} no aparece literalmente: {ln!r}"
            nlines += 1
    # cada valor escalar generado aparece tal cual en el texto
    nscalar = 0
    for k in dict.fromkeys(PH.findall(tpl)):
        v = V[k]
        if not (isinstance(v, str) and ("\n" in v or v.startswith("|"))):
            assert str(v) in text, f"{name}: valor {k}={v!r} no aparece"
            nscalar += 1
    nblocks = check_structure(text, name)
    return len(block_keys(tpl, V)), nlines, nscalar, nblocks


def main(argv):
    mode = argv[1] if len(argv) > 1 else ""
    V6, V7 = values06(), values07()
    gen = generated_tokens(V6, V7)
    docs = [(DOC06, TPL06, V6, "06"), (DOC07, TPL07, V7, "07")]
    for path, tpl, V, name in docs:
        lint_template(tpl, V, gen, name)
    if mode == "--dump":
        for _, _, V, name in docs:
            print(f"##### {name}")
            for k, v in V.items():
                print(f"=== {k}\n{v}")
        return 0
    if mode == "--write":
        for path, tpl, V, name in docs:
            path.write_text(render(tpl, V), encoding="utf-8")
            print(f"escrito {path.name}")
        return 0
    assert mode == "", f"argumento desconocido: {mode}"
    for path, tpl, V, name in docs:
        nb, nl, ns, nf = verify(path, tpl, V, name)
        print(f"OK {path.name}: {nb} bloques/tablas ({nl} lineas) y {ns} valores sueltos verificados; {nf} diagramas ASCII puros; estructura correcta")
    st = sorted(k for k in STATIC)
    print(f"OK diagramas estaticos (dibujados a mano, sin cifras): {', '.join(st)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
