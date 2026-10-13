#!/usr/bin/env python3
"""Oraculo independiente del nucleo logico (segunda implementacion, N-version).

Solo biblioteca estandar; se ejecuta con `python3 -I`. Implementa docs/spec/01..04
de la forma mas simple y legible posible, sin compartir nada con el codigo C++:
doble bufer real, redes reconstruidas desde el grafo de pines en cada tick,
lotes de edicion sobre una copia, codec de blob y FNV-1a propios.

Uso:
    oracle.py run NET SCN [-o SALIDA]          escribe un ltrace
    oracle.py encode NET [--scn SCN] [--tick K] [--step-micros M] [-o BLOB]
    oracle.py decode BLOB                      valida y resume un blob
    oracle.py selftest                         comprueba las cifras del texto de la spec

Todas las decisiones donde la spec es ambigua estan en docs/spec/ambiguities.md.
"""

import argparse
import collections
import enum
import sys

SEMANTICS_VERSION = 1
FORMAT_VERSION = 1
MAGIC = b"SLGC"
OUT = 0xFF
MAX_ID = 0xFFFFFFFE
MASK64 = (1 << 64) - 1
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
HEADER_SIZE = 32
COMPONENT_RECORD = 11
WIRE_RECORD = 10
TAIL_SIZE = 8


class Status(enum.IntEnum):
    Ok = 0
    BadSpec = 1
    BadPin = 2
    UnknownComponent = 3
    DuplicateId = 4
    RetiredId = 5
    UnknownWire = 6
    DuplicateWire = 7
    StaleBatch = 8
    LimitExceeded = 9
    BadFrame = 10
    WrongPhase = 11
    FormatCorrupt = 12
    FormatTooNew = 13


class Kind(enum.IntEnum):
    CONST = 1
    SENSOR = 2
    ACTUATOR = 3
    GATE = 4
    DELAY = 5
    COMPARE = 6
    LATCH = 7
    TOGGLE = 8
    CLOCK = 9


class GateOp(enum.IntEnum):
    AND = 0
    OR = 1
    XOR = 2


class CompareMode(enum.IntEnum):
    COMPARE = 0
    SUBTRACT = 1


KIND_TEXT = {
    Kind.CONST: "const", Kind.SENSOR: "sensor", Kind.ACTUATOR: "actuator",
    Kind.GATE: "gate", Kind.DELAY: "delay", Kind.COMPARE: "cmp",
    Kind.LATCH: "latch", Kind.TOGGLE: "toggle", Kind.CLOCK: "clock",
}
TEXT_KIND = {v: k for k, v in KIND_TEXT.items()}
# Clave de lnet -> posicion en (p0, p1, p2), por kind.
KEY_SLOT = {
    Kind.CONST: {"v": 0},
    Kind.GATE: {"op": 0, "inv": 1, "k": 2},
    Kind.DELAY: {"d": 0},
    Kind.COMPARE: {"m": 0},
    Kind.CLOCK: {"p": 0, "h": 1},
}


class OracleError(Exception):
    """Error de entrada (texto mal formado, lote rechazado al construir, blob invalido)."""


Spec = collections.namedtuple("Spec", "kind p0 p1 p2")


def make_const(v):
    return Spec(Kind.CONST, v, 0, 0)


def make_sensor():
    return Spec(Kind.SENSOR, 0, 0, 0)


def make_actuator():
    return Spec(Kind.ACTUATOR, 0, 0, 0)


def make_gate(op, invert, arity):
    return Spec(Kind.GATE, int(op), 1 if invert else 0, arity)


def make_not():
    return make_gate(GateOp.OR, True, 1)


def make_delay(length):
    return Spec(Kind.DELAY, length, 0, 0)


def make_compare(mode):
    return Spec(Kind.COMPARE, int(mode), 0, 0)


def make_latch():
    return Spec(Kind.LATCH, 0, 0, 0)


def make_toggle():
    return Spec(Kind.TOGGLE, 0, 0, 0)


def make_clock(period, high):
    return Spec(Kind.CLOCK, period, high, 0)


class Limits:
    def __init__(self, max_components=8192, max_wires=32768, max_net_pins=256,
                 max_delay_cells=65536, max_delay=255, max_clock_period=4096, max_arity=8):
        self.max_components = max_components
        self.max_wires = max_wires
        self.max_net_pins = max_net_pins
        self.max_delay_cells = max_delay_cells
        self.max_delay = max_delay
        self.max_clock_period = max_clock_period
        self.max_arity = max_arity


# ---------------------------------------------------------------- bytes y FNV-1a

def u16(x):
    return x.to_bytes(2, "little")


def u32(x):
    return x.to_bytes(4, "little")


def u64(x):
    return x.to_bytes(8, "little")


def fnv1a64(data, h=FNV_OFFSET):
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & MASK64
    return h


def hex16(h):
    return "%016X" % h


# ---------------------------------------------------------------- catalogo

def input_count(spec):
    k = spec.kind
    if k in (Kind.CONST, Kind.SENSOR, Kind.CLOCK):
        return 0
    if k in (Kind.ACTUATOR, Kind.DELAY, Kind.TOGGLE):
        return 1
    if k == Kind.GATE:
        return spec.p2
    return 2


def has_output(spec):
    return spec.kind != Kind.ACTUATOR


def rest_seed(spec):
    if spec.kind == Kind.GATE and spec.p1 == 1:
        return 15
    return 0


def validate_spec(spec, limits):
    """Parametros solamente; la semilla se valida aparte."""
    try:
        kind = Kind(spec.kind)
    except ValueError:
        return Status.BadSpec
    p0, p1, p2 = spec.p0, spec.p1, spec.p2
    if kind == Kind.CONST:
        return Status.Ok if p0 <= 15 and p1 == 0 and p2 == 0 else Status.BadSpec
    if kind in (Kind.SENSOR, Kind.ACTUATOR, Kind.LATCH, Kind.TOGGLE):
        return Status.Ok if p0 == 0 and p1 == 0 and p2 == 0 else Status.BadSpec
    if kind == Kind.GATE:
        if p0 > 2 or p1 > 1 or p2 < 1:
            return Status.BadSpec
        return Status.LimitExceeded if p2 > limits.max_arity else Status.Ok
    if kind == Kind.DELAY:
        if p0 < 1 or p1 != 0 or p2 != 0:
            return Status.BadSpec
        return Status.LimitExceeded if p0 > limits.max_delay else Status.Ok
    if kind == Kind.COMPARE:
        return Status.Ok if p0 <= 1 and p1 == 0 and p2 == 0 else Status.BadSpec
    if p0 < 2 or p2 != 0:
        return Status.BadSpec
    if p0 > limits.max_clock_period:
        return Status.LimitExceeded
    return Status.Ok if 1 <= p1 <= p0 - 1 else Status.BadSpec


def seed_ok(spec, seed):
    k = spec.kind
    if k in (Kind.CONST, Kind.SENSOR):
        return seed == 0
    if k in (Kind.ACTUATOR, Kind.DELAY, Kind.COMPARE):
        return 0 <= seed <= 15
    if k in (Kind.GATE, Kind.LATCH, Kind.TOGGLE):
        return seed in (0, 15)
    return 0 <= seed <= spec.p0 - 1


def validate(spec, seed, limits):
    status = validate_spec(spec, limits)
    if status != Status.Ok:
        return status
    return Status.Ok if seed_ok(spec, seed) else Status.BadSpec


def initial_state(spec, seed):
    k = spec.kind
    if k in (Kind.CONST, Kind.SENSOR):
        return None
    if k == Kind.DELAY:
        return (seed,) * spec.p0
    if k == Kind.TOGGLE:
        return (seed, 1)
    return seed


def truthy(x):
    return x > 0


def canonical(b):
    return 15 if b else 0


def output_of(spec, state, sample):
    k = spec.kind
    if k == Kind.CONST:
        return spec.p0
    if k == Kind.SENSOR:
        return min(sample, 15)
    if k == Kind.DELAY or k == Kind.TOGGLE:
        return state[0]
    if k == Kind.CLOCK:
        return 15 if state >= spec.p0 - spec.p1 else 0
    return state


def next_state(spec, state, u):
    k = spec.kind
    if k in (Kind.CONST, Kind.SENSOR):
        return None
    if k == Kind.ACTUATOR:
        return u[0]
    if k == Kind.GATE:
        lit = [truthy(x) for x in u]
        if spec.p0 == GateOp.AND:
            g = all(lit)
        elif spec.p0 == GateOp.OR:
            g = any(lit)
        else:
            g = sum(lit) % 2 == 1
        return canonical(g != bool(spec.p1))
    if k == Kind.DELAY:
        return state[1:] + (u[0],)
    if k == Kind.COMPARE:
        a, b = u[0], u[1]
        if spec.p0 == CompareMode.COMPARE:
            return a if b <= a else 0
        return max(a - b, 0)
    if k == Kind.LATCH:
        if truthy(u[1]):
            return 0
        if truthy(u[0]):
            return 15
        return state
    if k == Kind.TOGGLE:
        q, p = state
        rising = truthy(u[0]) and p == 0
        return (15 - q if rising else q, 1 if truthy(u[0]) else 0)
    return (state + 1) % spec.p0


def state_bytes(spec, state):
    k = spec.kind
    if k in (Kind.CONST, Kind.SENSOR):
        return b""
    if k == Kind.TOGGLE:
        return bytes(state)
    if k == Kind.CLOCK:
        return u16(state)
    if k == Kind.DELAY:
        return bytes(state)
    return bytes([state])


def state_text(spec, state):
    if spec.kind == Kind.DELAY:
        return "[" + ",".join(str(f) for f in state) + "]"
    if spec.kind == Kind.TOGGLE:
        return "%d/%d" % state
    return str(state)


# ---------------------------------------------------------------- redes

def build_nets(wires):
    """Union-find sobre los pines que aparecen en algun cable."""
    parent = {}

    def find(p):
        while parent[p] != p:
            parent[p] = parent[parent[p]]
            p = parent[p]
        return p

    for a, b in wires:
        parent.setdefault(a, a)
        parent.setdefault(b, b)
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra
    return find, parent


def max_net_pins_used(wires):
    find, parent = build_nets(wires)
    counts = collections.Counter(find(p) for p in parent)
    return max(counts.values(), default=0)


def pin_valid(specs, pin):
    cid, index = pin
    spec = specs.get(cid)
    if spec is None:
        return Status.UnknownComponent
    if index == OUT:
        return Status.Ok if has_output(spec) else Status.BadPin
    return Status.Ok if index < input_count(spec) else Status.BadPin


def normalize_wire(a, b):
    return (a, b) if a < b else (b, a)


# ---------------------------------------------------------------- lotes de edicion

class EditBatch:
    def __init__(self, base_next_id):
        self.base = base_next_id
        self.ops = []

    def add(self, spec, seed):
        self.ops.append(("add", spec, seed))

    def add_with_id(self, cid, spec, seed):
        self.ops.append(("add_with_id", cid, spec, seed))

    def remove(self, cid):
        self.ops.append(("remove", cid))

    def replace(self, cid, spec, seed):
        self.ops.append(("replace", cid, spec, seed))

    def connect(self, a, b):
        self.ops.append(("connect", a, b))

    def disconnect(self, a, b):
        self.ops.append(("disconnect", a, b))


class World:
    """Estructura y estado de una netlist. Se copia entera para validar un lote."""

    def __init__(self):
        self.specs = {}
        self.states = {}
        self.wires = set()
        self.next_id = 1
        self.retired = set()
        self.adds = 0  # llamadas a add() del lote en curso

    def copy(self):
        w = World()
        w.specs = dict(self.specs)
        w.states = dict(self.states)
        w.wires = set(self.wires)
        w.next_id = self.next_id
        w.retired = set(self.retired)
        return w

    def _insert(self, cid, spec, seed):
        self.specs[cid] = spec
        self.states[cid] = initial_state(spec, seed)

    def _create(self, cid, spec, seed, base, limits):
        if cid in self.specs:
            return Status.DuplicateId
        if cid < base or cid in self.retired:
            return Status.RetiredId
        if cid >= 0xFFFFFFFF:
            return Status.LimitExceeded
        status = validate(spec, seed, limits)
        if status != Status.Ok:
            return status
        self._insert(cid, spec, seed)
        self.next_id = max(self.next_id, cid + 1)
        return Status.Ok

    def apply_op(self, op, base, limits):
        name = op[0]
        if name == "add":
            _, spec, seed = op
            # 02 seccion 2: el id es el nextId base mas los add() anteriores del lote; un id explicito no lo mueve.
            cid = base + self.adds
            self.adds += 1
            if cid >= 0xFFFFFFFF:
                return Status.LimitExceeded
            return self._create(cid, spec, seed, base, limits)
        if name == "add_with_id":
            _, cid, spec, seed = op
            return self._create(cid, spec, seed, base, limits)
        if name == "remove":
            cid = op[1]
            if cid not in self.specs:
                return Status.UnknownComponent
            del self.specs[cid]
            del self.states[cid]
            self.wires = {w for w in self.wires if w[0][0] != cid and w[1][0] != cid}
            self.retired.add(cid)
            return Status.Ok
        if name == "replace":
            _, cid, spec, seed = op
            if cid not in self.specs:
                return Status.UnknownComponent
            status = validate(spec, seed, limits)
            if status != Status.Ok:
                return status
            candidate = dict(self.specs)
            candidate[cid] = spec
            for a, b in self.wires:
                if cid in (a[0], b[0]):
                    for pin in (a, b):
                        if pin[0] == cid and pin_valid(candidate, pin) != Status.Ok:
                            return Status.BadPin
            self._insert(cid, spec, seed)
            return Status.Ok
        a, b = op[1], op[2]
        for pin in (a, b):
            status = pin_valid(self.specs, pin)
            if status != Status.Ok:
                return status
        if a == b:
            return Status.BadPin
        wire = normalize_wire(a, b)
        if name == "connect":
            if wire in self.wires:
                return Status.DuplicateWire
            self.wires.add(wire)
            return Status.Ok
        if wire not in self.wires:
            return Status.UnknownWire
        self.wires.remove(wire)
        return Status.Ok

    def check_limits(self, limits):
        if len(self.specs) > limits.max_components or len(self.wires) > limits.max_wires:
            return Status.LimitExceeded
        cells = sum(s.p0 for s in self.specs.values() if s.kind == Kind.DELAY)
        if cells > limits.max_delay_cells:
            return Status.LimitExceeded
        if max_net_pins_used(self.wires) > limits.max_net_pins:
            return Status.LimitExceeded
        return Status.Ok


# ---------------------------------------------------------------- simulador

def evaluate(specs, states, wires, sample_of, ids):
    """Nuevo estado de `ids` leyendo solo de `states` (la instantanea)."""
    find, parent = build_nets(wires)
    net_value = {}
    for cid, spec in specs.items():
        if not has_output(spec):
            continue
        pin = (cid, OUT)
        if pin in parent:
            root = find(pin)
            value = output_of(spec, states[cid], sample_of.get(cid, 0))
            net_value[root] = max(net_value.get(root, 0), value)

    def read(cid, j):
        pin = (cid, j)
        if pin not in parent:
            return 0
        return net_value.get(find(pin), 0)

    result = {}
    for cid in ids:
        spec = specs[cid]
        u = tuple(read(cid, j) for j in range(input_count(spec)))
        result[cid] = next_state(spec, states[cid], u)
    return result


class Simulator:
    def __init__(self, limits=None, host_step_micros=0):
        self.limits = limits or Limits()
        self.host_step_micros = host_step_micros
        self.tick = 0
        self.world = World()
        self.clamped_samples = 0

    @property
    def next_id(self):
        return self.world.next_id

    def new_batch(self):
        return EditBatch(self.world.next_id)

    def apply(self, batch):
        if batch.base != self.world.next_id:
            return Status.StaleBatch
        work = self.world.copy()
        for op in batch.ops:
            status = work.apply_op(op, batch.base, self.limits)
            if status != Status.Ok:
                return status
        status = work.check_limits(self.limits)
        if status != Status.Ok:
            return status
        work.retired = set()
        self.world = work
        return Status.Ok

    def sensor_ids(self):
        return sorted(c for c, s in self.world.specs.items() if s.kind == Kind.SENSOR)

    def actuator_ids(self):
        return sorted(c for c, s in self.world.specs.items() if s.kind == Kind.ACTUATOR)

    def commands(self):
        return [self.world.states[c] for c in self.actuator_ids()]

    def step(self, samples):
        ids = self.sensor_ids()
        if len(samples) != len(ids):
            return Status.BadFrame
        sample_of = {}
        for cid, v in zip(ids, samples):
            if v > 15:
                self.clamped_samples += 1
            sample_of[cid] = min(v, 15)
        w = self.world
        w.states = evaluate(w.specs, w.states, w.wires, sample_of, sorted(w.specs))
        self.tick += 1
        return Status.Ok

    def state_stream(self):
        out = bytearray()
        for cid in sorted(self.world.specs):
            spec = self.world.specs[cid]
            out += u32(cid) + bytes([int(spec.kind)]) + state_bytes(spec, self.world.states[cid])
        return bytes(out)

    def state_hash(self):
        return fnv1a64(self.state_stream())

    def _component_records(self):
        out = bytearray()
        for cid in sorted(self.world.specs):
            s = self.world.specs[cid]
            out += u32(cid) + bytes([int(s.kind)]) + u16(s.p0) + u16(s.p1) + u16(s.p2)
        return bytes(out)

    def _wire_records(self):
        out = bytearray()
        for a, b in sorted(self.world.wires):
            out += u32(a[0]) + bytes([a[1]]) + u32(b[0]) + bytes([b[1]])
        return bytes(out)

    def structure_hash(self):
        data = (u32(len(self.world.specs)) + u32(len(self.world.wires))
                + self._component_records() + self._wire_records())
        return fnv1a64(data)

    def encode(self):
        w = self.world
        out = bytearray(MAGIC)
        out += u16(FORMAT_VERSION) + u16(SEMANTICS_VERSION) + u32(self.host_step_micros)
        out += u64(self.tick) + u32(w.next_id) + u32(len(w.specs)) + u32(len(w.wires))
        for cid in sorted(w.specs):
            s = w.specs[cid]
            out += u32(cid) + bytes([int(s.kind)]) + u16(s.p0) + u16(s.p1) + u16(s.p2)
            out += state_bytes(s, w.states[cid])
        out += self._wire_records()
        out += u64(fnv1a64(out))
        return bytes(out)


def step_in_place(sim, samples, order):
    """Control negativo (G5): actualiza en sitio en el orden dado. No es semantica de v0."""
    w = sim.world
    ids = sim.sensor_ids()
    sample_of = {cid: min(v, 15) for cid, v in zip(ids, samples)}
    for cid in order:
        w.states[cid] = evaluate(w.specs, w.states, w.wires, sample_of, [cid])[cid]
    sim.tick += 1


# ---------------------------------------------------------------- decodificacion estricta

def state_size(spec):
    k = spec.kind
    if k in (Kind.CONST, Kind.SENSOR):
        return 0
    if k in (Kind.TOGGLE, Kind.CLOCK):
        return 2
    if k == Kind.DELAY:
        return spec.p0
    return 1


def decode_state(spec, raw):
    k = spec.kind
    if k in (Kind.CONST, Kind.SENSOR):
        return True, None
    if k == Kind.ACTUATOR or k == Kind.COMPARE:
        return raw[0] <= 15, raw[0]
    if k in (Kind.GATE, Kind.LATCH):
        return raw[0] in (0, 15), raw[0]
    if k == Kind.TOGGLE:
        return raw[0] in (0, 15) and raw[1] in (0, 1), (raw[0], raw[1])
    if k == Kind.CLOCK:
        c = int.from_bytes(raw, "little")
        return c < spec.p0, c
    return all(f <= 15 for f in raw), tuple(raw)


def decode(blob, limits=None):
    """Devuelve (Status, Simulator | None). Orden de comprobaciones: 02 seccion 6.3."""
    limits = limits or Limits()
    corrupt = (Status.FormatCorrupt, None)
    if len(blob) < 4 or blob[:4] != MAGIC or len(blob) < 8:
        return corrupt
    fmt = int.from_bytes(blob[4:6], "little")
    sem = int.from_bytes(blob[6:8], "little")
    if fmt > FORMAT_VERSION or sem > SEMANTICS_VERSION:
        return Status.FormatTooNew, None
    if fmt < 1 or sem < 1 or len(blob) < HEADER_SIZE + TAIL_SIZE:
        return corrupt
    body = blob[:-TAIL_SIZE]
    if fnv1a64(body) != int.from_bytes(blob[-TAIL_SIZE:], "little"):
        return corrupt
    pos = [8]

    def take(n):
        if pos[0] + n > len(body):
            return None
        chunk = body[pos[0]:pos[0] + n]
        pos[0] += n
        return chunk

    host = int.from_bytes(take(4), "little")
    tick = int.from_bytes(take(8), "little")
    next_id = int.from_bytes(take(4), "little")
    ccount = int.from_bytes(take(4), "little")
    wcount = int.from_bytes(take(4), "little")
    if ccount > limits.max_components or wcount > limits.max_wires:
        return Status.LimitExceeded, None
    remaining = len(body) - pos[0]
    if ccount * COMPONENT_RECORD > remaining or wcount * WIRE_RECORD > remaining:
        return corrupt
    world = World()
    previous = 0
    for _ in range(ccount):
        head = take(COMPONENT_RECORD)
        if head is None:
            return corrupt
        cid = int.from_bytes(head[0:4], "little")
        if cid <= previous or cid > MAX_ID:
            return corrupt
        previous = cid
        try:
            kind = Kind(head[4])
        except ValueError:
            return corrupt
        spec = Spec(kind, int.from_bytes(head[5:7], "little"),
                    int.from_bytes(head[7:9], "little"), int.from_bytes(head[9:11], "little"))
        status = validate_spec(spec, limits)
        if status == Status.LimitExceeded:
            return status, None
        if status != Status.Ok:
            return corrupt
        raw = take(state_size(spec))
        if raw is None:
            return corrupt
        ok, state = decode_state(spec, raw)
        if not ok:
            return corrupt
        world.specs[cid] = spec
        world.states[cid] = state
    if next_id <= previous or next_id < 1:
        return corrupt
    world.next_id = next_id
    last = None
    for _ in range(wcount):
        rec = take(WIRE_RECORD)
        if rec is None:
            return corrupt
        a = (int.from_bytes(rec[0:4], "little"), rec[4])
        b = (int.from_bytes(rec[5:9], "little"), rec[9])
        if not a < b or (last is not None and not last < (a, b)):
            return corrupt
        last = (a, b)
        if pin_valid(world.specs, a) != Status.Ok or pin_valid(world.specs, b) != Status.Ok:
            return corrupt
        world.wires.add((a, b))
    if pos[0] != len(body):
        return corrupt
    status = world.check_limits(limits)
    if status != Status.Ok:
        return status, None
    sim = Simulator(limits, host)
    sim.tick = tick
    sim.world = world
    return Status.Ok, sim


# ---------------------------------------------------------------- formatos de texto

def _lines(text):
    for number, raw in enumerate(text.split("\n"), 1):
        line = raw.split("#", 1)[0].strip()
        if line:
            yield number, line.split()


def _int(token, where, lo=0, hi=None):
    if not (token.isascii() and token.isdigit()):
        raise OracleError("%s: se esperaba un entero decimal, hay %r" % (where, token))
    value = int(token)
    if value < lo or (hi is not None and value > hi):
        raise OracleError("%s: %d fuera de rango" % (where, value))
    return value


def parse_pin(token, where):
    head, _, tail = token.partition(".")
    cid = _int(head, where, 1, MAX_ID)
    if tail == "o":
        return (cid, OUT)
    return (cid, _int(tail, where, 0, OUT - 1))


def _named_value(kind, key, text, where):
    if kind == Kind.GATE and key == "op":
        names = {"and": 0, "or": 1, "xor": 2}
    elif kind == Kind.COMPARE and key == "m":
        names = {"compare": 0, "subtract": 1}
    else:
        names = {}
    if text.lower() in names:
        return names[text.lower()]
    return _int(text, where)


def parse_component(tokens, where):
    """`<id> <kind> [clave=valor ...]` -> (id, Spec, seed)."""
    if len(tokens) < 2:
        raise OracleError("%s: componente incompleto" % where)
    cid = _int(tokens[0], where, 1, MAX_ID)
    spec, seed = parse_component_body(tokens[1:], where)
    return cid, spec, seed


def parse_component_body(tokens, where):
    """`<kind> [clave=valor ...]` -> (Spec, seed)."""
    if not tokens or tokens[0] not in TEXT_KIND:
        raise OracleError("%s: kind desconocido o ausente" % where)
    kind = TEXT_KIND[tokens[0]]
    slots = KEY_SLOT.get(kind, {})
    params = [0, 0, 0]
    seed = None
    seen = set()
    for item in tokens[1:]:
        key, eq, value = item.partition("=")
        if not eq or key in seen:
            raise OracleError("%s: parametro mal formado o repetido %r" % (where, item))
        seen.add(key)
        if key == "seed":
            seed = _int(value, where)
        elif key in slots:
            params[slots[key]] = _named_value(kind, key, value, where)
        else:
            raise OracleError("%s: la clave %r no existe en %s" % (where, key, tokens[0]))
    spec = Spec(kind, params[0], params[1], params[2])
    return spec, rest_seed(spec) if seed is None else seed


NetDesc = collections.namedtuple("NetDesc", "components wires")
Scenario = collections.namedtuple("Scenario", "ticks inputs edits")


def parse_lnet(text):
    components, wires = [], []
    header = False
    for number, tokens in _lines(text):
        where = "lnet:%d" % number
        if not header:
            if tokens != ["lnet", "1"]:
                raise OracleError("%s: se esperaba la cabecera `lnet 1`" % where)
            header = True
        elif tokens[0] == "c":
            components.append(parse_component(tokens[1:], where))
        elif tokens[0] == "w" and len(tokens) == 3:
            wires.append((parse_pin(tokens[1], where), parse_pin(tokens[2], where)))
        else:
            raise OracleError("%s: linea no reconocida" % where)
    if not header:
        raise OracleError("lnet: falta la cabecera")
    return NetDesc(components, wires)


def parse_lscn(text):
    ticks = None
    inputs, edits = [], []
    header = False
    for number, tokens in _lines(text):
        where = "lscn:%d" % number
        if not header:
            if tokens != ["lscn", "1"]:
                raise OracleError("%s: se esperaba la cabecera `lscn 1`" % where)
            header = True
        elif tokens[0] == "ticks" and len(tokens) == 2 and ticks is None:
            ticks = _int(tokens[1], where)
        elif tokens[0] == "i" and len(tokens) >= 3:
            tau = _int(tokens[1], where)
            pairs = []
            for item in tokens[2:]:
                cid, eq, value = item.partition("=")
                if not eq:
                    raise OracleError("%s: se esperaba id=valor, hay %r" % (where, item))
                pairs.append((_int(cid, where, 1, MAX_ID), _int(value, where, 0, 255)))
            inputs.append((tau, pairs, where))
        elif tokens[0] == "e" and len(tokens) >= 5:
            edits.append((_int(tokens[1], where), _parse_edit(tokens[2:], where), where))
        else:
            raise OracleError("%s: linea no reconocida" % where)
    if not header or ticks is None:
        raise OracleError("lscn: falta la cabecera o `ticks`")
    for tau, _, where in inputs + edits:
        if tau > ticks:
            raise OracleError("%s: tau %d mayor que ticks %d" % (where, tau, ticks))
    return Scenario(ticks, inputs, edits)


def _parse_edit(tokens, where):
    verb, target = tokens[0], tokens[1]
    rest = tokens[2:]
    if target == "c" and verb == "add" and rest and rest[0] in TEXT_KIND:
        spec, seed = parse_component_body(rest, where)
        return ("add_c", None, spec, seed)
    if target == "c" and verb in ("add", "rep"):
        cid, spec, seed = parse_component(rest, where)
        return ("add_c" if verb == "add" else "rep_c", cid, spec, seed)
    if target == "c" and verb == "del" and len(rest) == 1:
        return ("del_c", _int(rest[0], where, 1, MAX_ID))
    if target == "w" and verb in ("add", "del") and len(rest) == 2:
        return ("add_w" if verb == "add" else "del_w",
                parse_pin(rest[0], where), parse_pin(rest[1], where))
    raise OracleError("%s: edicion no reconocida" % where)


def build_simulator(net, limits=None, host_step_micros=0):
    sim = Simulator(limits, host_step_micros)
    batch = sim.new_batch()
    for cid, spec, seed in net.components:
        batch.add_with_id(cid, spec, seed)
    for a, b in net.wires:
        batch.connect(a, b)
    status = sim.apply(batch)
    if status != Status.Ok:
        raise OracleError("netlist inicial rechazada: %s" % status.name)
    return sim


def _edit_batch(sim, ops):
    batch = sim.new_batch()
    for op in ops:
        if op[0] == "add_c" and op[1] is None:
            batch.add(op[2], op[3])
        elif op[0] == "add_c":
            batch.add_with_id(op[1], op[2], op[3])
        elif op[0] == "rep_c":
            batch.replace(op[1], op[2], op[3])
        elif op[0] == "del_c":
            batch.remove(op[1])
        elif op[0] == "add_w":
            batch.connect(op[1], op[2])
        else:
            batch.disconnect(op[1], op[2])
    return batch


Row = collections.namedtuple("Row", "tau inputs states commands hash")


def snapshot_row(sim, held):
    w = sim.world
    inputs = [(c, held.get(c, 0)) for c in sim.sensor_ids()]
    states = [(c, state_text(w.specs[c], w.states[c])) for c in sorted(w.specs)
              if w.specs[c].kind not in (Kind.CONST, Kind.SENSOR)]
    commands = list(zip(sim.actuator_ids(), sim.commands()))
    return Row(sim.tick, inputs, states, commands, sim.state_hash())


def format_row(row):
    def part(label, pairs):
        return [label] + ["%d=%s" % (i, v) for i, v in pairs]

    tokens = ["row", str(row.tau)] + part("in", row.inputs) + ["|"] + part("st", row.states)
    tokens += ["|"] + part("cmd", row.commands) + ["|", "h=" + hex16(row.hash)]
    return " ".join(tokens)


def run_scenario(sim, scn, until=None, notes=None):
    """Rows 0..ticks: fila tau = muestra e_tau y estado X_tau tras las ediciones del limite tau."""
    last = scn.ticks if until is None else until
    held = {}
    rows = []
    for tau in range(last + 1):
        ops = [op for t, op, _ in scn.edits if t == tau]
        if ops:
            status = sim.apply(_edit_batch(sim, ops))
            if status != Status.Ok and notes is not None:
                notes.append("lote de tau=%d rechazado: %s" % (tau, status.name))
        for t, pairs, where in scn.inputs:
            if t != tau:
                continue
            for cid, value in pairs:
                spec = sim.world.specs.get(cid)
                if spec is None or spec.kind != Kind.SENSOR:
                    raise OracleError("%s: %d no es un sensor en tau=%d" % (where, cid, tau))
                held[cid] = value
        rows.append(snapshot_row(sim, held))
        if tau < last:
            sim.step([held.get(c, 0) for c in sim.sensor_ids()])
    return rows


def render_ltrace(rows):
    return "ltrace 1\n" + "".join(format_row(r) + "\n" for r in rows)


def run_texts(net_text, scn_text, host_step_micros=0, notes=None):
    scn = parse_lscn(scn_text)
    sim = build_simulator(parse_lnet(net_text), host_step_micros=host_step_micros)
    return render_ltrace(run_scenario(sim, scn, notes=notes))


def parse_ltrace(text):
    """Inverso de render_ltrace: lista de dicts con tau, in, st, cmd, h (todo texto/enteros)."""
    lines = text.split("\n")
    if lines[0] != "ltrace 1" or lines[-1] != "":
        raise OracleError("ltrace: cabecera o salto de linea final incorrectos")
    rows = []
    for line in lines[1:-1]:
        fields = line.split(" | ")
        if len(fields) != 4 or not fields[3].startswith("h="):
            raise OracleError("ltrace: fila mal formada %r" % line)
        head = fields[0].split(" ")
        if head[0] != "row" or head[2:3] != ["in"]:
            raise OracleError("ltrace: fila mal formada %r" % line)

        def pairs(chunk, label):
            parts = chunk.split(" ")
            if parts[0] != label:
                raise OracleError("ltrace: se esperaba %s en %r" % (label, line))
            return [(int(p.split("=")[0]), p.split("=")[1]) for p in parts[1:]]

        rows.append({
            "tau": int(head[1]),
            "in": [(int(p.split("=")[0]), int(p.split("=")[1])) for p in head[3:]],
            "st": pairs(fields[1], "st"),
            "cmd": [(i, int(v)) for i, v in pairs(fields[2], "cmd")],
            "h": fields[3][2:],
        })
    return rows


# ---------------------------------------------------------------- modelo del anfitrion

def fullness(num, den):
    if num <= 0 or den <= 0:
        return 0
    return min(15, 1 + 14 * num // den)


class StepPacer:
    def __init__(self, step_micros, max_steps_per_call):
        self.step = max(1, step_micros)
        self.max_steps = max(1, max_steps_per_call)
        self.acc = 0
        self.dropped = 0
        self.paused = 0

    def advance(self, elapsed_micros, paused):
        if paused:
            self.paused += elapsed_micros
            return 0
        self.acc += elapsed_micros
        n = self.acc // self.step
        if n > self.max_steps:
            self.dropped += n - self.max_steps
            n = self.max_steps
        self.acc %= self.step
        return n


class BurstDriver:
    """Solo lo que fija 02 seccion 7.2: una muestra por llamada, Level/Pulse, valores finales.

    No modela sensores sin resolver, guarda de asentamiento, resync ni el blob del FrameDriver.
    """

    def __init__(self, sim):
        self.sim = sim
        self.modes = {}
        self.retained = {}
        self.confirmed = dict(zip(sim.actuator_ids(), sim.commands()))

    def set_mode(self, sensor, mode):
        self.modes[sensor] = mode

    def run_call(self, steps, offers):
        for cid, value in offers.items():
            self.retained[cid] = value
        for step in range(steps):
            samples = []
            for cid in self.sim.sensor_ids():
                if self.modes.get(cid, "level") == "level":
                    samples.append(self.retained.get(cid, 0))
                else:
                    samples.append(offers.get(cid, 0) if step == 0 else 0)
            self.sim.step(samples)
        now = dict(zip(self.sim.actuator_ids(), self.sim.commands()))
        return [(cid, self.confirmed[cid], now[cid]) for cid in sorted(now)
                if now[cid] != self.confirmed[cid]]

    def acknowledge_all(self):
        self.confirmed = dict(zip(self.sim.actuator_ids(), self.sim.commands()))


def format_changes(changes):
    return " ".join("%d:%d>%d" % c for c in changes)


def regenerate_lburst(text, load_net):
    """Formato `lburst 1` (extension del oraculo para G11): entradas -> texto completo con lo esperado."""
    rows = list(_lines(text))
    if not rows or rows[0][1] != ["lburst", "1"]:
        raise OracleError("lburst: se esperaba la cabecera `lburst 1`")
    net_name, modes, calls = None, [], []
    for number, tokens in rows[1:]:
        where = "lburst:%d" % number
        if tokens[0] == "net" and len(tokens) == 2:
            net_name = tokens[1]
        elif tokens[0] == "sensor" and len(tokens) == 3 and tokens[2] in ("level", "pulse"):
            modes.append((_int(tokens[1], where, 1, MAX_ID), tokens[2]))
        elif tokens[0] == "call" and len(tokens) >= 3 and tokens[2] == "offer":
            offers = {}
            for item in tokens[3:]:
                if item == "|":
                    break
                cid, eq, value = item.partition("=")
                if not eq:
                    raise OracleError("%s: se esperaba id=valor, hay %r" % (where, item))
                offers[_int(cid, where, 1, MAX_ID)] = _int(value, where, 0, 255)
            calls.append((_int(tokens[1], where, 1), offers))
        elif tokens[0] == "end":
            continue
        else:
            raise OracleError("%s: linea no reconocida" % where)
    if net_name is None:
        raise OracleError("lburst: falta `net`")
    sim = build_simulator(parse_lnet(load_net(net_name)))
    driver = BurstDriver(sim)
    for cid, mode in modes:
        driver.set_mode(cid, mode)
    out = ["lburst 1", "net " + net_name]
    out += ["sensor %d %s" % m for m in modes]
    for steps, offers in calls:
        changes = driver.run_call(steps, offers)
        driver.acknowledge_all()
        line = "call %d offer" % steps + "".join(" %d=%d" % kv for kv in sorted(offers.items()))
        out.append(line + " | changes" + "".join(" " + format_changes([c]) for c in changes))
    out.append("end tick %d h=%s" % (sim.tick, hex16(sim.state_hash())))
    return "\n".join(out) + "\n"


def regenerate_lpacer(text):
    """Formato `lpacer 1` (extension del oraculo para G12)."""
    rows = list(_lines(text))
    if not rows or rows[0][1] != ["lpacer", "1"]:
        raise OracleError("lpacer: se esperaba la cabecera `lpacer 1`")
    pacer, out = None, ["lpacer 1"]
    for number, tokens in rows[1:]:
        where = "lpacer:%d" % number
        if tokens[0] == "step" and len(tokens) == 4 and tokens[2] == "max" and pacer is None:
            step, limit = _int(tokens[1], where), _int(tokens[3], where)
            pacer = StepPacer(step, limit)
            out.append("step %d max %d" % (step, limit))
        elif tokens[0] == "call" and len(tokens) >= 3 and pacer is not None:
            dt, paused = _int(tokens[1], where, 0, 0xFFFFFFFF), _int(tokens[2], where, 0, 1)
            steps = pacer.advance(dt, bool(paused))
            out.append("call %d %d | steps %d dropped %d paused %d acc %d" % (
                dt, paused, steps, pacer.dropped, pacer.paused, pacer.acc))
        else:
            raise OracleError("%s: linea no reconocida" % where)
    return "\n".join(out) + "\n"


# ---------------------------------------------------------------- autotest

def selftest():
    results = []

    def check(name, ok):
        results.append((name, bool(ok)))

    check("fnv1a64 vacio", fnv1a64(b"") == 0xCBF29CE484222325)
    check("fnv1a64 'a'", fnv1a64(b"a") == 0xAF63DC4C8601EC8C)
    check("fnv1a64 'foobar'", fnv1a64(b"foobar") == 0x85944171F73967E8)

    net = ("lnet 1\nc 1 sensor\nc 2 gate op=or inv=1 k=1 seed=15\nc 3 actuator\n"
           "w 1.o 2.0\nw 2.o 3.0\n")
    scn = "lscn 1\nticks 4\ni 2 1=15\n"
    rows = parse_ltrace(run_texts(net, scn))
    want = ["9D16793581913B31", "9D16723581912F4C", "9D16723581912F4C",
            "4622C4783DE15BB9", "4622CD783DE16B04"]
    check("G1 huellas (02 6.4)", [r["h"] for r in rows] == want)
    check("G1 mandos A (02 6.4)", [r["cmd"][0][1] for r in rows] == [0, 15, 15, 15, 0])

    sim = build_simulator(parse_lnet(net), host_step_micros=100000)
    blob = sim.encode()
    check("G7 95 bytes", len(blob) == 95)
    check("G7 cola", int.from_bytes(blob[-8:], "little") == 0x3D144EE313AE954F)
    check("G7 structureHash", sim.structure_hash() == 0x84C3FB7A3ACB833C)
    check("G7 flujo de 17 bytes", len(sim.state_stream()) == 17)
    status, again = decode(blob)
    check("G7 decode/encode", status == Status.Ok and again.encode() == blob)

    ring = ("lnet 1\nc 1 gate op=or inv=1 k=1 seed=15\nc 2 gate op=or inv=1 k=1 seed=0\n"
            "c 3 gate op=or inv=1 k=1 seed=15\nw 3.o 1.0\nw 1.o 2.0\nw 2.o 3.0\n")
    ring_rows = parse_ltrace(run_texts(ring, "lscn 1\nticks 6\n"))
    check("G2 anillo periodo 6",
          [r["st"] for r in ring_rows][0] == [(1, "15"), (2, "0"), (3, "15")]
          and ring_rows[6]["h"] == ring_rows[0]["h"]
          and len({r["h"] for r in ring_rows[:6]}) == 6)

    pacer = StepPacer(100000, 4)
    got = [pacer.advance(dt, False) for dt in (250000, 30000, 530000)]
    check("G12 pacer", got == [2, 0, 4] and pacer.dropped == 2 and pacer.acc == 10000)
    check("fullness (02 2)", [fullness(50, 200), fullness(200, 200), fullness(1, 200)] == [4, 15, 1])

    failures = [n for n, ok in results if not ok]
    for name, ok in results:
        print("%s %s" % ("ok  " if ok else "FAIL", name))
    print("selftest: %d comprobaciones, %d fallos" % (len(results), len(failures)))
    return 0 if not failures else 1


# ---------------------------------------------------------------- CLI

def _read_text(path):
    with open(path, "rb") as handle:
        return handle.read().decode("utf-8")


def _write_bytes(path, data):
    if path is None or path == "-":
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
    else:
        with open(path, "wb") as handle:
            handle.write(data)


def _cmd_run(args):
    notes = []
    text = run_texts(_read_text(args.net), _read_text(args.scn), notes=notes)
    for note in notes:
        print("oracle: " + note, file=sys.stderr)
    _write_bytes(args.output, text.encode("ascii"))
    return 0


def _cmd_encode(args):
    sim = build_simulator(parse_lnet(_read_text(args.net)), host_step_micros=args.step_micros)
    if args.scn:
        run_scenario(sim, parse_lscn(_read_text(args.scn)), until=args.tick)
    elif args.tick:
        raise OracleError("--tick > 0 requiere --scn")
    _write_bytes(args.output, sim.encode())
    return 0


def _cmd_decode(args):
    with open(args.blob, "rb") as handle:
        blob = handle.read()
    status, sim = decode(blob)
    print("status %s" % status.name)
    if sim is not None:
        print("tick %d nextId %d hostStepMicros %d components %d wires %d" % (
            sim.tick, sim.next_id, sim.host_step_micros, len(sim.world.specs), len(sim.world.wires)))
        print("stateHash %s structureHash %s" % (hex16(sim.state_hash()), hex16(sim.structure_hash())))
        print("tail %s reencode %s" % (hex16(int.from_bytes(blob[-8:], "little")),
                                       "identical" if sim.encode() == blob else "DIFFERENT"))
    return 0 if status == Status.Ok else 1


def main(argv=None):
    parser = argparse.ArgumentParser(prog="oracle.py", description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    run = sub.add_parser("run", help="simula un escenario y escribe un ltrace")
    run.add_argument("net")
    run.add_argument("scn")
    run.add_argument("-o", "--output")
    enc = sub.add_parser("encode", help="escribe el blob de una netlist (tras ejecutar --scn hasta --tick)")
    enc.add_argument("net")
    enc.add_argument("--scn")
    enc.add_argument("--tick", type=int, default=0)
    enc.add_argument("--step-micros", type=int, default=0)
    enc.add_argument("-o", "--output")
    dec = sub.add_parser("decode", help="valida un blob e imprime su resumen")
    dec.add_argument("blob")
    sub.add_parser("selftest", help="comprueba las cifras que la spec deriva a mano")
    args = parser.parse_args(argv)
    try:
        if args.command == "run":
            return _cmd_run(args)
        if args.command == "encode":
            return _cmd_encode(args)
        if args.command == "decode":
            return _cmd_decode(args)
        return selftest()
    except OracleError as error:
        print("oracle: error: %s" % error, file=sys.stderr)
        return 2
    except OSError as error:
        print("oracle: error: %s" % error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
