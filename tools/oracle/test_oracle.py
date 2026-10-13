"""Pruebas del oraculo contra las cifras que el texto de docs/spec deriva a mano.

    python3 -I -m unittest        (desde tools/oracle)

Las tablas de la spec se extraen del propio markdown, no se copian; donde el texto solo
da prosa, el caso lleva la cita literal. `-I` implica `-P`: el directorio del fichero se
anade a mano para poder importar `oracle`.
"""

import itertools
import os
import random
import re
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import make_goldens  # noqa: E402
import oracle  # noqa: E402
from oracle import Kind, Status  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SPEC_DIR = os.path.join(ROOT, "docs", "spec")
GOLDEN_DIR = os.path.join(ROOT, "core", "golden")
ORACLE_PY = os.path.join(HERE, "oracle.py")
NOT15 = "gate op=or inv=1 k=1 seed=15"


# ---------------------------------------------------------------- utilidades

def spec_text(name):
    with open(os.path.join(SPEC_DIR, name), encoding="utf-8") as handle:
        return handle.read()


def golden_bytes(name):
    with open(os.path.join(GOLDEN_DIR, name), "rb") as handle:
        return handle.read()


def golden_text(name):
    return golden_bytes(name).decode("ascii")


def golden_files():
    """Ficheros regulares de primer nivel; `regressions/` es de los fuzzers de C++ (03 seccion 6)."""
    return [n for n in os.listdir(GOLDEN_DIR) if os.path.isfile(os.path.join(GOLDEN_DIR, n))]


def fenced_blocks(text):
    blocks, current = [], None
    for line in text.split("\n"):
        if line.startswith("```"):
            if current is None:
                current = []
            else:
                blocks.append(current)
                current = None
        elif current is not None:
            current.append(line)
    return blocks


def block_with(text, needle):
    for block in fenced_blocks(text):
        if any(needle in line for line in block):
            return block
    raise AssertionError("no hay bloque con %r" % needle)


def nibble(token):
    return int(token, 16)


def sim_of(net_text, step_micros=0):
    return oracle.build_simulator(oracle.parse_lnet(net_text), host_step_micros=step_micros)


def trace_of(net_text, scn_text):
    return oracle.parse_ltrace(oracle.run_texts(net_text, scn_text))


def golden_trace(name):
    return oracle.parse_ltrace(golden_text(name + ".ltrace"))


def state_of(row, cid):
    return dict(row["st"])[cid]


def wave(rows, extract):
    return "".join("#" if extract(r) == 15 else "_" for r in rows)


def tail_fixed(body):
    return body + oracle.u64(oracle.fnv1a64(body))


def ring_net(n, bits):
    lines = ["lnet 1"]
    for i in range(n):
        lines.append("c %d gate op=or inv=1 k=1 seed=%d" % (i + 1, 15 if bits >> i & 1 else 0))
    for i in range(n):
        lines.append("w %d.o %d.0" % (i + 1, (i + 1) % n + 1))
    return "\n".join(lines) + "\n"


def sim_with(*specs_seeds):
    sim = oracle.Simulator()
    batch = sim.new_batch()
    for spec, seed in specs_seeds:
        batch.add(spec, seed)
    assert sim.apply(batch) == Status.Ok
    return sim


# ---------------------------------------------------------------- FNV y blob (02 seccion 6)

class FnvAndBlobTests(unittest.TestCase):
    def test_fnv1a_public_vectors(self):
        self.assertEqual(oracle.fnv1a64(b""), 0xCBF29CE484222325)
        self.assertEqual(oracle.fnv1a64(b"a"), 0xAF63DC4C8601EC8C)
        self.assertEqual(oracle.fnv1a64(b"foobar"), 0x85944171F73967E8)

    def test_hash_table_of_g1_matches_spec_text(self):
        text = spec_text("02-api-nucleo.md")
        rows = re.findall(r"^\|\s*(\d)\s*\|\s*(\w)\s*\|\s*(\w)\s*\|\s*(\w)\s*\|\s*`([0-9A-F]{16})`\s*\|",
                          text, re.M)
        self.assertEqual(len(rows), 5)
        net = golden_text("lat_not.lnet")
        scn = "lscn 1\nticks 4\ni 0 1=0\ni 2 1=15\n"
        got = trace_of(net, scn)
        for tau, e, n, a, h in rows:
            row = got[int(tau)]
            self.assertEqual(row["in"], [(1, nibble(e))], "e en tau=" + tau)
            self.assertEqual(state_of(row, 2), str(nibble(n)), "N en tau=" + tau)
            self.assertEqual(state_of(row, 3), str(nibble(a)), "A en tau=" + tau)
            self.assertEqual(row["h"], h, "huella en tau=" + tau)

    def test_state_stream_of_row_zero_is_the_17_bytes_in_the_spec(self):
        text = spec_text("02-api-nucleo.md")
        quoted = re.findall(r"`((?:[0-9A-F]{2} ){4}[0-9A-F]{2}(?: [0-9A-F]{2})?)`", text)
        wanted = {"01 00 00 00 02", "02 00 00 00 04 0F", "03 00 00 00 03 00"}
        self.assertTrue(wanted <= set(quoted))
        sim = sim_of(golden_text("lat_not.lnet"))
        stream = sim.state_stream()
        self.assertEqual(len(stream), 17)
        expected = b"".join(bytes.fromhex(q) for q in ("01 00 00 00 02", "02 00 00 00 04 0F",
                                                       "03 00 00 00 03 00"))
        self.assertEqual(stream, expected)

    def test_blob_g7_matches_every_number_in_the_spec(self):
        text = spec_text("02-api-nucleo.md")
        sim = sim_of(golden_text("lat_not.lnet"), step_micros=100000)
        blob = sim.encode()
        self.assertEqual(len(blob), 95)
        self.assertIn("mide 95 bytes", text)
        self.assertEqual(blob[:4], b"SLG" + b"C")
        self.assertEqual(blob[8:12], bytes.fromhex("A0860100"))
        self.assertEqual(blob[4:8], bytes([1, 0, 1, 0]))
        self.assertEqual(int.from_bytes(blob[12:20], "little"), 0)
        self.assertEqual([int.from_bytes(blob[o:o + 4], "little") for o in (20, 24, 28)], [4, 3, 2])
        tail = int(re.search(r"La cola es `0x([0-9A-F]{16})`", text).group(1), 16)
        structure = int(re.search(r"`structureHash` vale `0x([0-9A-F]{16})`", text).group(1), 16)
        self.assertEqual(int.from_bytes(blob[-8:], "little"), tail)
        self.assertEqual(oracle.fnv1a64(blob[:-8]), tail)
        self.assertEqual(sim.structure_hash(), structure)
        not_record = re.search(r"El registro del NOT es `([0-9A-F ]+)`", text).group(1)
        self.assertEqual(blob[32 + 11:32 + 11 + 12], bytes.fromhex(not_record))
        self.assertEqual(32 + (11 + 12 + 12) + 2 * 10 + 8, 95)

    def test_golden_blob_file_is_the_g7_blob(self):
        sim = sim_of(golden_text("lat_not.lnet"), step_micros=100000)
        self.assertEqual(golden_bytes("lat_not.slgc"), sim.encode())

    def test_roundtrip_and_canonical_encoding(self):
        net = golden_text("and_delay2.lnet")
        sim = sim_of(net, step_micros=100000)
        for _ in range(5):
            sim.step([9, 5])
        blob = sim.encode()
        status, again = oracle.decode(blob)
        self.assertEqual(status, Status.Ok)
        self.assertEqual(again.encode(), blob)
        self.assertEqual(again.state_hash(), sim.state_hash())
        self.assertEqual(again.tick, 5)
        self.assertEqual(again.host_step_micros, 100000)

    def test_next_id_survives_removals_in_the_blob(self):
        sim = sim_of(golden_text("lat_not.lnet"), step_micros=100000)
        batch = sim.new_batch()
        batch.remove(3)
        self.assertEqual(sim.apply(batch), Status.Ok)
        blob = sim.encode()
        self.assertEqual(int.from_bytes(blob[20:24], "little"), 4)
        self.assertEqual(int.from_bytes(blob[24:28], "little"), 2)
        status, again = oracle.decode(blob)
        self.assertEqual(status, Status.Ok)
        self.assertEqual(again.next_id, 4)
        batch = again.new_batch()
        batch.add_with_id(3, oracle.make_actuator(), 0)
        self.assertEqual(again.apply(batch), Status.RetiredId)
        batch = again.new_batch()
        batch.add(oracle.make_actuator(), 0)
        self.assertEqual(again.apply(batch), Status.Ok)
        self.assertIn(4, again.world.specs)

    def test_roundtrip_with_every_kind_and_continuation(self):
        sim = oracle.Simulator()
        batch = sim.new_batch()
        batch.add(oracle.make_sensor(), 0)
        batch.add(oracle.make_const(7), 0)
        batch.add(oracle.make_gate(oracle.GateOp.XOR, True, 3), 15)
        batch.add(oracle.make_delay(4), 3)
        batch.add(oracle.make_compare(oracle.CompareMode.SUBTRACT), 2)
        batch.add(oracle.make_latch(), 15)
        batch.add(oracle.make_toggle(), 15)
        batch.add(oracle.make_clock(7, 3), 5)
        batch.add(oracle.make_actuator(), 9)
        batch.connect((1, oracle.OUT), (3, 0))
        batch.connect((2, oracle.OUT), (3, 1))
        batch.connect((3, oracle.OUT), (4, 0))
        batch.connect((1, oracle.OUT), (5, 0))
        batch.connect((2, oracle.OUT), (5, 1))
        batch.connect((8, oracle.OUT), (9, 0))
        self.assertEqual(sim.apply(batch), Status.Ok)
        rng = random.Random(7)
        for _ in range(9):
            sim.step([rng.randrange(16)])
        status, copy = oracle.decode(sim.encode())
        self.assertEqual(status, Status.Ok)
        for _ in range(20):
            sample = [rng.randrange(16)]
            sim.step(sample)
            copy.step(sample)
            self.assertEqual(copy.state_hash(), sim.state_hash())
        self.assertEqual(copy.encode(), sim.encode())

    def test_every_single_byte_substitution_and_truncation_of_g7_is_rejected(self):
        blob = golden_bytes("lat_not.slgc")
        for pos in range(len(blob)):
            for value in range(256):
                if value == blob[pos]:
                    continue
                mutated = blob[:pos] + bytes([value]) + blob[pos + 1:]
                status, sim = oracle.decode(mutated)
                self.assertNotEqual(status, Status.Ok, "byte %d -> %d" % (pos, value))
                self.assertIsNone(sim)
        for length in range(len(blob)):
            self.assertNotEqual(oracle.decode(blob[:length])[0], Status.Ok, "truncado a %d" % length)
        self.assertEqual(oracle.decode(blob + b"\x00")[0], Status.FormatCorrupt)

    def test_newer_versions_are_too_new_without_looking_further(self):
        blob = bytearray(golden_bytes("lat_not.slgc"))
        newer_format = bytes(blob[:4]) + oracle.u16(2) + bytes(blob[6:])
        self.assertEqual(oracle.decode(newer_format)[0], Status.FormatTooNew)
        newer_semantics = bytes(blob[:6]) + oracle.u16(2) + bytes(blob[8:])
        self.assertEqual(oracle.decode(newer_semantics)[0], Status.FormatTooNew)
        self.assertEqual(oracle.decode(bytes(blob[:8]) + b"junk")[0], Status.FormatCorrupt)
        self.assertEqual(oracle.decode(blob[:4] + oracle.u16(2) + oracle.u16(1))[0], Status.FormatTooNew)

    def test_broken_invariants_with_recomputed_tail_give_format_corrupt(self):
        base = golden_bytes("lat_not.slgc")[:-8]
        not_state = 32 + 11 + 11
        self.assertEqual(base[not_state], 15)
        cases = {
            "estado de Gate 7": base[:not_state] + b"\x07" + base[not_state + 1:],
            "estado de Gate 14": base[:not_state] + b"\x0e" + base[not_state + 1:],
            "actuador 16": base[:-20 - 1] + b"\x10" + base[-20:],
            "kind 0": base[:36] + b"\x00" + base[37:],
            "kind 10": base[:36] + b"\x0a" + base[37:],
            "nextId igual al mayor id": base[:20] + oracle.u32(3) + base[24:],
            "nextId 0": base[:20] + oracle.u32(0) + base[24:],
            "byte de mas": base + b"\x00",
            "falta un byte": base[:-1],
            "sensor con p0 distinto de 0": base[:37] + oracle.u16(1) + base[39:],
            "gate con op 3": base[:32 + 11 + 5] + oracle.u16(3) + base[32 + 11 + 7:],
            "ids duplicados": base[:32 + 11] + oracle.u32(1) + base[32 + 15:],
            "ids no ascendentes": base[:32] + oracle.u32(2) + base[36:32 + 11] + oracle.u32(1) + base[32 + 15:],
        }
        for name, body in cases.items():
            self.assertEqual(oracle.decode(tail_fixed(body))[0], Status.FormatCorrupt, name)

    def test_duplicate_component_ids_are_rejected_in_isolation(self):
        body = sim_with((oracle.make_latch(), 0), (oracle.make_latch(), 0)).encode()[:-8]
        self.assertEqual(oracle.decode(tail_fixed(body))[0], Status.Ok)
        second = 32 + 12
        duplicated = body[:second] + oracle.u32(1) + body[second + 4:]
        self.assertEqual(oracle.decode(tail_fixed(duplicated))[0], Status.FormatCorrupt)

    def test_wire_invariants(self):
        base = golden_bytes("lat_not.slgc")[:-8]
        wires_at = 32 + 35
        first = base[wires_at:wires_at + 10]
        second = base[wires_at + 10:wires_at + 20]
        swapped_pins = first[5:10] + first[0:5]
        cases = {
            "cable sin normalizar": base[:wires_at] + swapped_pins + second,
            "cables fuera de orden": base[:wires_at] + second + first,
            "cable duplicado": base[:wires_at] + first + first,
            "pin inexistente": base[:wires_at + 4] + b"\x05" + base[wires_at + 5:],
            "componente inexistente": base[:wires_at + 5] + oracle.u32(9) + base[wires_at + 9:],
            "pin de salida del actuador": base[:wires_at + 15 - 1] + b"" + base[wires_at + 15 - 1:wires_at + 18]
            + b"\xff" + base[wires_at + 19:],
        }
        for name, body in cases.items():
            self.assertEqual(oracle.decode(tail_fixed(body))[0], Status.FormatCorrupt, name)

    def test_state_domain_violations(self):
        sim = sim_with((oracle.make_clock(4, 2), 3), (oracle.make_delay(3), 1),
                       (oracle.make_toggle(), 0), (oracle.make_compare(oracle.CompareMode.COMPARE), 0))
        body = sim.encode()[:-8]
        start = 32 + 11
        self.assertEqual(int.from_bytes(body[start:start + 2], "little"), 3)
        bad_phase = body[:start] + oracle.u16(4) + body[start + 2:]
        self.assertEqual(oracle.decode(tail_fixed(bad_phase))[0], Status.FormatCorrupt)
        delay_at = start + 2 + 11
        bad_slot = body[:delay_at + 1] + b"\x10" + body[delay_at + 2:]
        self.assertEqual(oracle.decode(tail_fixed(bad_slot))[0], Status.FormatCorrupt)
        toggle_at = delay_at + 3 + 11
        bad_p = body[:toggle_at + 1] + b"\x02" + body[toggle_at + 2:]
        self.assertEqual(oracle.decode(tail_fixed(bad_p))[0], Status.FormatCorrupt)
        bad_q = body[:toggle_at] + b"\x01" + body[toggle_at + 1:]
        self.assertEqual(oracle.decode(tail_fixed(bad_q))[0], Status.FormatCorrupt)
        good = tail_fixed(body)
        self.assertEqual(oracle.decode(good)[0], Status.Ok)

    def test_limits_give_limit_exceeded_with_recomputed_tail(self):
        base = golden_bytes("lat_not.slgc")[:-8]
        many_components = base[:24] + oracle.u32(8193) + base[28:]
        self.assertEqual(oracle.decode(tail_fixed(many_components))[0], Status.LimitExceeded)
        many_wires = base[:28] + oracle.u32(32769) + base[32:]
        self.assertEqual(oracle.decode(tail_fixed(many_wires))[0], Status.LimitExceeded)
        self.assertEqual(oracle.decode(golden_bytes("lat_not.slgc"), oracle.Limits(max_components=2))[0],
                         Status.LimitExceeded)
        huge = base[:24] + oracle.u32(5) + base[28:]
        self.assertEqual(oracle.decode(tail_fixed(huge))[0], Status.FormatCorrupt)
        delay = sim_with((oracle.make_delay(200), 0), (oracle.make_delay(200), 0))
        self.assertEqual(oracle.decode(delay.encode(), oracle.Limits(max_delay_cells=399))[0],
                         Status.LimitExceeded)
        self.assertEqual(oracle.decode(delay.encode(), oracle.Limits(max_delay_cells=400))[0], Status.Ok)
        big_delay = base_with_delay(256)
        self.assertEqual(oracle.decode(tail_fixed(big_delay))[0], Status.LimitExceeded)


def base_with_delay(length):
    sim = sim_with((oracle.make_delay(2), 0))
    body = sim.encode()[:-8]
    return body[:32 + 5] + oracle.u16(length) + body[32 + 7:32 + 11] + bytes(2)


# ---------------------------------------------------------------- semantica de 01

class WorkedExampleTests(unittest.TestCase):
    def test_ring3_matches_the_states_listed_in_01_section_9_1(self):
        block = block_with(spec_text("01-semantica.md"), "X0=(F,0,F)")
        listed = []
        for line in block:
            m = re.match(r"X(\d)=\(([0F,]+)\):.*-> X(\d)=\(([0F,]+)\)", line)
            self.assertIsNotNone(m, line)
            if not listed:
                listed.append(tuple(nibble(c) for c in m.group(2).split(",")))
            listed.append(tuple(nibble(c) for c in m.group(4).split(",")))
        self.assertEqual(len(listed), 7)
        rows = golden_trace("ring3")
        got = [tuple(int(v) for _, v in r["st"]) for r in rows]
        self.assertEqual(got, listed)
        self.assertEqual(got[6], got[0])
        self.assertEqual(len(set(got[:6])), 6)

    def test_ring3_with_zero_seeds_has_period_two(self):
        rows = golden_trace("ring3_zero")
        got = [tuple(int(v) for _, v in r["st"]) for r in rows]
        self.assertEqual(got[:3], [(0, 0, 0), (15, 15, 15), (0, 0, 0)])
        self.assertEqual(got, [got[i % 2] for i in range(5)])

    def test_g3_table_matches_01_section_9_2(self):
        block = block_with(spec_text("01-semantica.md"), "eA eB")
        rows = golden_trace("and_delay2")
        seen = 0
        for line in block:
            m = re.match(r"^(\d+)\s+(\w+)\s+(\w+)\s*\|\s*(\w+)\s+(\w+)\s+(\w+)\s*\|\s*(\w+)\s*$", line)
            if not m:
                continue
            tau = int(m.group(1))
            row = rows[tau]
            a, b, and_, f1, f2, o = (nibble(m.group(i)) for i in (2, 3, 4, 5, 6, 7))
            self.assertEqual(row["in"], [(1, a), (2, b)], "tau=%d" % tau)
            self.assertEqual(state_of(row, 3), str(and_), "and tau=%d" % tau)
            self.assertEqual(state_of(row, 4), "[%d,%d]" % (f1, f2), "delay tau=%d" % tau)
            self.assertEqual(state_of(row, 5), str(o), "o tau=%d" % tau)
            self.assertEqual(row["cmd"], [(5, o)])
            seen += 1
        self.assertEqual(seen, 9)
        self.assertEqual(len(rows), 9)

    def test_g3_prose_pulse_of_two_ticks_survives(self):
        rows = golden_trace("and_delay2")
        wave_and = [r for r in rows if state_of(r, 3) == "15"]
        self.assertEqual([r["tau"] for r in wave_and], [3, 4])
        self.assertEqual([r["tau"] for r in rows if dict(r["cmd"])[5] == 15], [6, 7])

    def test_g4_latch_waveforms_match_01_section_9_3(self):
        text = spec_text("01-semantica.md")
        block = block_with(text, "S=R      ###")
        waves = {}
        for line in block:
            m = re.match(r"^(S=R|Q  \(NOR\)|Qn \(NOR\)|q  nativo)\s+([#_]+)", line)
            if m:
                waves[m.group(1)] = m.group(2)
        self.assertEqual(len(waves), 4)
        both = golden_trace("latch_both")
        self.assertEqual(wave(both, lambda r: r["in"][0][1]), waves["S=R"])
        self.assertEqual(wave(both, lambda r: int(state_of(r, 3))), waves["Q  (NOR)"])
        self.assertEqual(wave(both, lambda r: int(state_of(r, 4))), waves["Qn (NOR)"])
        self.assertEqual(wave(both, lambda r: int(state_of(r, 5))), waves["q  nativo"])
        inline = dict(re.findall(r"`(Q|Qn|q) = ([#_]+)`", text))
        pulse = golden_trace("latch_pulse")
        self.assertEqual(wave(pulse, lambda r: int(state_of(r, 3))), inline["Q"])
        self.assertEqual(wave(pulse, lambda r: int(state_of(r, 4))), inline["Qn"])
        self.assertEqual(wave(pulse, lambda r: int(state_of(r, 5))), inline["q"])

    def test_g4_intermediate_nor_values_from_the_prose(self):
        both = golden_trace("latch_both")
        pairs = [(state_of(r, 3), state_of(r, 4)) for r in both]
        self.assertEqual(pairs[:6], [("0", "15"), ("0", "0"), ("0", "0"), ("0", "0"), ("15", "15"), ("0", "0")])
        pulse = golden_trace("latch_pulse")
        self.assertEqual([(state_of(r, 3), state_of(r, 4)) for r in pulse[1:4]],
                         [("0", "15"), ("0", "0"), ("15", "15")])

    def test_g5_double_buffer_and_in_place_negative_control(self):
        rows = golden_trace("ring2")
        self.assertEqual([tuple(int(v) for _, v in r["st"]) for r in rows[:3]],
                         [(0, 0), (15, 15), (0, 0)])
        text = spec_text("03-pruebas.md")
        self.assertIn("`X1 = (F,F)`", text)
        for order, expected in (((1, 2), (15, 0)), ((2, 1), (0, 15))):
            sim = sim_of(golden_text("ring2.lnet"))
            oracle.step_in_place(sim, [], order)
            self.assertEqual(tuple(sim.world.states[c] for c in (1, 2)), expected, order)

    def test_g6_wired_or_is_max_not_saturated_sum(self):
        rows = golden_trace("wired9")
        self.assertEqual(rows[1]["cmd"], [(3, 9)])
        self.assertIn("la suma daría 15", spec_text("03-pruebas.md"))

    def test_g8_and_g9_toggle_edges(self):
        new_high = golden_trace("toggle_new_high")
        self.assertEqual([state_of(r, 2) for r in new_high[1:]], ["0/1"] * 4)
        edit = golden_trace("toggle_edit_connect")
        self.assertEqual([state_of(r, 2) for r in edit[1:]], ["0/1", "0/0", "0/0", "15/1", "15/1", "15/1"])
        flips = sum(1 for a, b in zip(edit[1:], edit[2:]) if state_of(a, 2)[:-2] != state_of(b, 2)[:-2])
        self.assertEqual(flips, 1)

    def test_g10_clock_waveforms(self):
        self.assertIn("`P=8, H=4` da `____####____####`", spec_text("01-semantica.md"))
        sim = sim_with((oracle.make_clock(8, 4), 0))
        out = ""
        for _ in range(16):
            out += "#" if oracle.output_of(sim.world.specs[1], sim.world.states[1], 0) else "_"
            sim.step([])
        self.assertEqual(out, "____####____####")
        shown = re.search(r"`(___###___###)`", spec_text("03-pruebas.md")).group(1)
        rows = golden_trace("clock6_3")[:12]
        self.assertEqual([int(state_of(r, 1)) for r in rows], [0, 1, 2, 3, 4, 5] * 2)
        self.assertEqual("".join("#" if int(state_of(r, 1)) >= 3 else "_" for r in rows), shown)
        delayed = golden_trace("clock6_3")
        self.assertEqual(wave(delayed[1:13], lambda r: dict(r["cmd"])[2]), shown)

    def test_g11_burst_trace_matches_02_section_7_2(self):
        block = block_with(spec_text("02-api-nucleo.md"), "paso 0:")
        expected = []
        for line in block:
            m = re.search(r"-> X(\d)=\((\d+),(\d+)\)", line)
            if m:
                expected.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
        self.assertEqual(expected, [(1, 15, 15), (2, 15, 0), (3, 15, 0)])
        rows = golden_trace("burst3")
        for tau, a1, a2 in expected:
            self.assertEqual(rows[tau]["cmd"], [(3, a1), (4, a2)])
        self.assertIn("changes = { A1: 0→15 }", spec_text("02-api-nucleo.md"))
        self.assertIn("changes 3:0>15", golden_text("burst3.lburst"))

    def test_g13_chain_reaches_the_bound_exactly(self):
        text = spec_text("01-semantica.md")
        for quote in ("`X0 = (0,0,0)`", "`X1 = (F,F,F)`", "(F,0,0)", "(F,0,F)", "`X4 = X3`"):
            self.assertIn(quote, text)
        rows = golden_trace("chain3")
        got = [tuple(int(v) for _, v in r["st"]) for r in rows]
        self.assertEqual(got[:5], [(0, 0, 0), (15, 15, 15), (15, 0, 0), (15, 0, 15), (15, 0, 15)])
        self.assertNotEqual(got[2], got[3])
        self.assertEqual(got[3], got[5])

    def test_g14_merge_loop_both_cases(self):
        text = spec_text("01-semantica.md")
        self.assertIn("periodo 2", text)
        self.assertIn("`X1.B = NOT(max(9,15)) = 0`", text)
        zero = golden_trace("mergeloop")
        self.assertEqual([state_of(r, 2) for r in zero], ["15", "0", "15", "0", "15", "0", "15"])
        nine = golden_trace("mergeloop_e9")
        self.assertEqual([state_of(r, 2) for r in nine], ["15"] + ["0"] * 6)
        self.assertEqual([dict(r["cmd"])[3] for r in nine], [0, 15, 9, 9, 9, 9, 9])

    def test_construction_transient_of_01_section_6_3(self):
        sim = sim_of("lnet 1\nc 1 sensor\nc 2 %s\nc 3 %s\nw 1.o 2.0\nw 2.o 3.0\n" % (NOT15, NOT15))
        seen = [tuple(sim.world.states[c] for c in (2, 3))]
        for _ in range(3):
            sim.step([15])
            seen.append(tuple(sim.world.states[c] for c in (2, 3)))
        self.assertEqual(seen, [(15, 15), (0, 0), (0, 15), (0, 15)])

    def test_latency_table_in_rows_of_01_section_2_5(self):
        direct = trace_of("lnet 1\nc 1 sensor\nc 2 actuator\nw 1.o 2.0\n", "lscn 1\nticks 8\ni 3 1=15\n")
        self.assertEqual(first_change(direct, 2, from_row=3), 3 + 1)
        settled = "lscn 1\nticks 10\ni 4 1=15\n"
        via_not = trace_of("lnet 1\nc 1 sensor\nc 2 %s\nc 3 actuator\nw 1.o 2.0\nw 2.o 3.0\n" % NOT15, settled)
        self.assertEqual(first_change(via_not, 3, from_row=4), 4 + 2)
        g3 = golden_trace("and_delay2")
        self.assertEqual(first_change(g3, 5, from_row=2), 2 + 4)

    def test_latency_in_calls_is_w_minus_one(self):
        def calls_until_change(net_text, offer_call, extra_calls=6):
            sim = sim_of(net_text)
            driver = oracle.BurstDriver(sim)
            for index in range(offer_call + extra_calls):
                value = 15 if index >= offer_call else 0
                changes = driver.run_call(1, {1: value} if index in (0, offer_call) else {})
                driver.acknowledge_all()
                if index >= offer_call and changes:
                    return index - offer_call
            return None

        self.assertEqual(calls_until_change("lnet 1\nc 1 sensor\nc 2 actuator\nw 1.o 2.0\n", 5), 0)
        self.assertEqual(calls_until_change(
            "lnet 1\nc 1 sensor\nc 2 %s\nc 3 actuator\nw 1.o 2.0\nw 2.o 3.0\n" % NOT15, 5), 1)


def first_change(rows, actuator, from_row):
    before = dict(rows[from_row]["cmd"])[actuator]
    for row in rows[from_row:]:
        if dict(row["cmd"])[actuator] != before:
            return row["tau"]
    return None


class RingCountTests(unittest.TestCase):
    def test_ring_period_census_matches_theory_counts(self):
        expected = {
            3: {2: 2, 6: 6},
            5: {2: 2, 10: 30},
            7: {2: 2, 14: 126},
            9: {2: 2, 6: 6, 18: 504},
        }
        for n, census in expected.items():
            found = {}
            for bits in range(1 << n):
                sim = sim_of(ring_net(n, bits))
                seen = {}
                tau = 0
                while True:
                    key = tuple(sim.world.states[c] for c in sorted(sim.world.specs))
                    if key in seen:
                        mu, lam = seen[key], tau - seen[key]
                        break
                    seen[key] = tau
                    sim.step([])
                    tau += 1
                self.assertEqual(mu, 0, "n=%d bits=%d" % (n, bits))
                found[lam] = found.get(lam, 0) + 1
            self.assertEqual(found, census, "n=%d" % n)

    def test_single_defect_ring_has_period_2n(self):
        for n in (3, 5, 7):
            rows = trace_of(ring_net(n, 0b101 if n == 3 else 0b10101 if n == 5 else 0b1010101),
                            "lscn 1\nticks %d\n" % (4 * n))
            hashes = [r["h"] for r in rows]
            self.assertEqual(hashes[2 * n], hashes[0])
            self.assertNotIn(hashes[0], hashes[1:2 * n])


# ---------------------------------------------------------------- catalogo y fusion

class CatalogTests(unittest.TestCase):
    def one_step(self, specs_seeds, wires):
        sim = sim_with(*specs_seeds)
        batch = sim.new_batch()
        for a, b in wires:
            batch.connect(a, b)
        self.assertEqual(sim.apply(batch), Status.Ok)
        sim.step([])
        return sim

    def test_wired_merge_is_max_with_its_algebra(self):
        values = range(16)
        for a, b in itertools.product(values, values):
            sim = self.one_step([(oracle.make_const(a), 0), (oracle.make_const(b), 0),
                                 (oracle.make_actuator(), 0)],
                                [((1, oracle.OUT), (3, 0)), ((2, oracle.OUT), (3, 0))])
            self.assertEqual(sim.world.states[3], max(a, b))
            swapped = self.one_step([(oracle.make_const(b), 0), (oracle.make_const(a), 0),
                                     (oracle.make_actuator(), 0)],
                                    [((1, oracle.OUT), (3, 0)), ((2, oracle.OUT), (3, 0))])
            self.assertEqual(swapped.world.states[3], sim.world.states[3])
        for a, b, c in itertools.product((0, 1, 7, 9, 15), repeat=3):
            sim = self.one_step([(oracle.make_const(a), 0), (oracle.make_const(b), 0),
                                 (oracle.make_const(c), 0), (oracle.make_actuator(), 0)],
                                [((1, oracle.OUT), (4, 0)), ((2, oracle.OUT), (4, 0)),
                                 ((3, oracle.OUT), (4, 0))])
            self.assertEqual(sim.world.states[4], max(a, b, c))
        for a in values:
            twice = self.one_step([(oracle.make_const(a), 0), (oracle.make_const(a), 0),
                                   (oracle.make_actuator(), 0)],
                                  [((1, oracle.OUT), (3, 0)), ((2, oracle.OUT), (3, 0))])
            self.assertEqual(twice.world.states[3], a)

    def test_gate_truth_tables_for_arity_one_to_three(self):
        for op, inv, k in itertools.product(oracle.GateOp, (0, 1), (1, 2, 3)):
            spec = oracle.make_gate(op, inv, k)
            domain = (0, 1, 2, 9, 15)
            for u in itertools.product(domain, repeat=k):
                veracious = [x for x in u if x > 0]
                if op == oracle.GateOp.AND:
                    truth = len(veracious) == k
                elif op == oracle.GateOp.OR:
                    truth = len(veracious) >= 1
                else:
                    truth = len(veracious) % 2 == 1
                want = 15 if truth != bool(inv) else 0
                for old in (0, 15):
                    self.assertEqual(oracle.next_state(spec, old, u), want, (op, inv, u))

    def test_compare_and_subtract_over_all_pairs(self):
        for mode in oracle.CompareMode:
            for a, b in itertools.product(range(16), repeat=2):
                sim = self.one_step([(oracle.make_const(a), 0), (oracle.make_const(b), 0),
                                     (oracle.make_compare(mode), 0)],
                                    [((1, oracle.OUT), (3, 0)), ((2, oracle.OUT), (3, 1))])
                if mode == oracle.CompareMode.COMPARE:
                    want = a if a >= b else 0
                else:
                    want = a - b if a > b else 0
                self.assertEqual(sim.world.states[3], want, (mode, a, b))

    def test_threshold_detector_with_const_in_side_input(self):
        for k in range(1, 16):
            for a in range(16):
                sim = self.one_step([(oracle.make_const(a), 0), (oracle.make_const(k), 0),
                                     (oracle.make_compare(oracle.CompareMode.COMPARE), 0)],
                                    [((1, oracle.OUT), (3, 0)), ((2, oracle.OUT), (3, 1))])
                self.assertEqual(sim.world.states[3] > 0, a >= k, (a, k))

    def test_latch_is_reset_dominant_over_all_inputs(self):
        spec = oracle.make_latch()
        for s, r, q in itertools.product(range(16), range(16), (0, 15)):
            want = 0 if r > 0 else (15 if s > 0 else q)
            self.assertEqual(oracle.next_state(spec, q, (s, r)), want, (s, r, q))

    def test_toggle_follows_an_independent_edge_model(self):
        rng = random.Random(2026101301)
        for _ in range(50):
            seed = rng.choice((0, 15))
            sim = sim_with((oracle.make_sensor(), 0), (oracle.make_toggle(), seed))
            batch = sim.new_batch()
            batch.connect((1, oracle.OUT), (2, 0))
            sim.apply(batch)
            q, previous_high = seed, True
            for _ in range(40):
                e = rng.choice((0, 0, 1, 15, 15, 9))
                high = e > 0
                if high and not previous_high:
                    q = 15 - q
                previous_high = high
                sim.step([e])
                self.assertEqual(sim.world.states[2], (q, 1 if high else 0))

    def test_clock_waveform_for_every_period_width_and_phase(self):
        for period in range(2, 13):
            for high in range(1, period):
                for phase in range(period):
                    sim = sim_with((oracle.make_clock(period, high), phase))
                    for tau in range(3 * period):
                        out = oracle.output_of(sim.world.specs[1], sim.world.states[1], 0)
                        self.assertEqual(out, 15 if (phase + tau) % period >= period - high else 0)
                        sim.step([])

    def test_delay_is_a_chain_of_unit_delays(self):
        rng = random.Random(2026101302)
        for d in range(1, 9):
            inputs = [rng.randrange(16) for _ in range(30)]
            one = sim_with((oracle.make_sensor(), 0), (oracle.make_delay(d), 0), (oracle.make_actuator(), 0))
            chain = sim_with((oracle.make_sensor(), 0), *[(oracle.make_delay(1), 0)] * d,
                             (oracle.make_actuator(), 0))
            for sim, ids in ((one, [(1, 2), (2, 3)]), (chain, [(i, i + 1) for i in range(1, d + 2)])):
                batch = sim.new_batch()
                for src, dst in ids:
                    batch.connect((src, oracle.OUT), (dst, 0))
                self.assertEqual(sim.apply(batch), Status.Ok)
            for e in inputs:
                one.step([e])
                chain.step([e])
                self.assertEqual(one.commands(), chain.commands())
            fifo = sim_with((oracle.make_sensor(), 0), (oracle.make_delay(d), 0), (oracle.make_actuator(), 0))
            batch = fifo.new_batch()
            batch.connect((1, oracle.OUT), (2, 0))
            batch.connect((2, oracle.OUT), (3, 0))
            fifo.apply(batch)
            seen = []
            for e in inputs + [0] * (d + 2):
                fifo.step([e])
                seen.append(fifo.commands()[0])
            self.assertEqual(seen[d + 0:d + len(inputs)], inputs)

    def test_not_plus_delay_loop_has_period_2_times_m_plus_1(self):
        for m in range(1, 9):
            for gate_seed, slot in itertools.product((0, 15), repeat=2):
                net = ("lnet 1\nc 1 gate op=or inv=1 k=1 seed=%d\nc 2 delay d=%d seed=%d\n"
                       "w 1.o 2.0\nw 2.o 1.0\n" % (gate_seed, m, slot))
                sim = sim_of(net)
                states = []
                for _ in range(2 * (m + 1) + 1):
                    states.append(tuple(sim.world.states[c] for c in (1, 2)))
                    sim.step([])
                self.assertEqual(states[-1], states[0])
                self.assertNotIn(states[0], states[1:-1])

    def test_rest_seeds_are_stable_with_all_inputs_at_zero(self):
        for op, inv, k in itertools.product(oracle.GateOp, (0, 1), (1, 2, 8)):
            spec = oracle.make_gate(op, inv, k)
            rest = oracle.rest_seed(spec)
            self.assertEqual(oracle.next_state(spec, rest, (0,) * k), rest)
            self.assertEqual(rest, 15 if inv else 0)
        for spec in (oracle.make_latch(), oracle.make_toggle(), oracle.make_actuator(),
                     oracle.make_delay(3), oracle.make_compare(oracle.CompareMode.SUBTRACT),
                     oracle.make_clock(5, 2), oracle.make_const(4), oracle.make_sensor()):
            self.assertEqual(oracle.rest_seed(spec), 0)

    def test_seed_effects_per_kind(self):
        toggle = sim_with((oracle.make_toggle(), 15))
        self.assertEqual(toggle.world.states[1], (15, 1))
        delay = sim_with((oracle.make_delay(3), 9))
        self.assertEqual(delay.world.states[1], (9, 9, 9))
        clock = sim_with((oracle.make_clock(6, 2), 4))
        self.assertEqual(clock.world.states[1], 4)
        gate = sim_with((oracle.make_not(), 0))
        self.assertEqual(gate.world.states[1], 0)

    def test_sensor_saturates_and_counts_clamped_samples(self):
        sim = sim_with((oracle.make_sensor(), 0), (oracle.make_actuator(), 0))
        batch = sim.new_batch()
        batch.connect((1, oracle.OUT), (2, 0))
        sim.apply(batch)
        sim.step([200])
        sim.step([15])
        sim.step([16])
        self.assertEqual(sim.clamped_samples, 2)
        self.assertEqual(sim.world.states[2], 15)
        self.assertEqual(sim.step([1, 2]), Status.BadFrame)
        self.assertEqual(sim.tick, 3)
        self.assertEqual(sim.clamped_samples, 2)

    def test_validate_domains(self):
        lim = oracle.Limits()
        ok = oracle.validate
        self.assertEqual(ok(oracle.make_const(15), 0, lim), Status.Ok)
        self.assertEqual(ok(oracle.make_const(16), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.Spec(Kind.SENSOR, 1, 0, 0), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_gate(oracle.GateOp.AND, False, 2), 7, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_gate(oracle.GateOp.AND, False, 0), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_gate(oracle.GateOp.AND, False, 8), 0, lim), Status.Ok)
        self.assertEqual(ok(oracle.make_gate(oracle.GateOp.AND, False, 9), 0, lim), Status.LimitExceeded)
        self.assertEqual(ok(oracle.Spec(Kind.GATE, 3, 0, 1), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_delay(0), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_delay(255), 15, lim), Status.Ok)
        self.assertEqual(ok(oracle.make_delay(256), 0, lim), Status.LimitExceeded)
        self.assertEqual(ok(oracle.make_delay(2), 16, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.Spec(Kind.COMPARE, 2, 0, 0), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_latch(), 1, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_clock(1, 1), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_clock(4096, 4095), 4095, lim), Status.Ok)
        self.assertEqual(ok(oracle.make_clock(4097, 1), 0, lim), Status.LimitExceeded)
        self.assertEqual(ok(oracle.make_clock(4, 4), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_clock(4, 0), 0, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.make_clock(4, 2), 4, lim), Status.BadSpec)
        self.assertEqual(ok(oracle.Spec(99, 0, 0, 0), 0, lim), Status.BadSpec)


# ---------------------------------------------------------------- ediciones (02 secciones 4 y 5)

class EditTests(unittest.TestCase):
    def setUp(self):
        self.sim = sim_of(golden_text("lat_not.lnet"))

    def test_ids_are_monotonic_and_the_batch_is_atomic(self):
        before = self.sim.encode()
        batch = self.sim.new_batch()
        batch.add(oracle.make_not(), 15)
        batch.add(oracle.make_delay(0), 0)
        self.assertEqual(self.sim.apply(batch), Status.BadSpec)
        self.assertEqual(self.sim.encode(), before)
        good = self.sim.new_batch()
        good.add(oracle.make_latch(), 0)
        good.add(oracle.make_latch(), 0)
        self.assertEqual(self.sim.apply(good), Status.Ok)
        self.assertEqual(sorted(self.sim.world.specs), [1, 2, 3, 4, 5])
        self.assertEqual(self.sim.next_id, 6)

    def test_stale_batch(self):
        old = self.sim.new_batch()
        other = self.sim.new_batch()
        other.add(oracle.make_latch(), 0)
        self.assertEqual(self.sim.apply(other), Status.Ok)
        old.add(oracle.make_latch(), 0)
        before = self.sim.encode()
        self.assertEqual(self.sim.apply(old), Status.StaleBatch)
        self.assertEqual(self.sim.encode(), before)

    def test_remove_retires_the_id_and_connect_then_remove_is_valid(self):
        batch = self.sim.new_batch()
        batch.remove(2)
        batch.add_with_id(2, oracle.make_not(), 15)
        self.assertEqual(self.sim.apply(batch), Status.RetiredId)
        batch = self.sim.new_batch()
        batch.remove(2)
        batch.connect((2, oracle.OUT), (3, 0))
        self.assertEqual(self.sim.apply(batch), Status.UnknownComponent)
        batch = self.sim.new_batch()
        batch.connect((1, oracle.OUT), (3, 0))
        batch.remove(3)
        self.assertEqual(self.sim.apply(batch), Status.Ok)
        self.assertEqual(sorted(self.sim.world.wires), [((1, 255), (2, 0))])
        batch = self.sim.new_batch()
        batch.add_with_id(3, oracle.make_actuator(), 0)
        self.assertEqual(self.sim.apply(batch), Status.RetiredId)
        batch = self.sim.new_batch()
        batch.add_with_id(2, oracle.make_actuator(), 0)
        self.assertEqual(self.sim.apply(batch), Status.DuplicateId)

    def test_an_id_created_and_removed_in_the_same_batch_is_retired(self):
        batch = self.sim.new_batch()
        batch.add_with_id(7, oracle.make_latch(), 0)
        batch.remove(7)
        batch.add_with_id(7, oracle.make_latch(), 0)
        self.assertEqual(self.sim.apply(batch), Status.RetiredId)
        batch = self.sim.new_batch()
        batch.add(oracle.make_latch(), 0)
        batch.remove(4)
        batch.add_with_id(4, oracle.make_latch(), 0)
        self.assertEqual(self.sim.apply(batch), Status.RetiredId)
        self.assertEqual(self.sim.next_id, 4)

    def test_add_with_id_any_order_and_gaps(self):
        sim = oracle.Simulator()
        batch = sim.new_batch()
        batch.add_with_id(9, oracle.make_latch(), 0)
        batch.add_with_id(4, oracle.make_latch(), 0)
        self.assertEqual(sim.apply(batch), Status.Ok)
        self.assertEqual(sim.next_id, 10)
        again = sim.new_batch()
        again.add(oracle.make_latch(), 0)
        self.assertEqual(sim.apply(again), Status.Ok)
        self.assertIn(10, sim.world.specs)

    def test_add_takes_the_base_id_plus_the_adds_before_it(self):
        # 02 seccion 2: un id explicito no mueve el contador de add(); si choca, es DuplicateId o RetiredId al aplicar.
        sim = oracle.Simulator()
        batch = sim.new_batch()
        batch.add_with_id(1, oracle.make_latch(), 0)
        batch.add(oracle.make_latch(), 0)
        before = sim.encode()
        self.assertEqual(sim.apply(batch), Status.DuplicateId)
        self.assertEqual(sim.encode(), before)
        batch = sim.new_batch()
        batch.add_with_id(1, oracle.make_latch(), 0)
        batch.remove(1)
        batch.add(oracle.make_latch(), 0)
        self.assertEqual(sim.apply(batch), Status.RetiredId)
        batch = sim.new_batch()
        batch.add(oracle.make_latch(), 0)
        batch.add_with_id(5, oracle.make_latch(), 0)
        batch.add(oracle.make_latch(), 0)
        self.assertEqual(sim.apply(batch), Status.Ok)
        self.assertEqual(sorted(sim.world.specs), [1, 2, 5])
        self.assertEqual(sim.next_id, 6)

    def test_add_with_the_id_space_exhausted_is_limit_exceeded_at_the_first_id_that_does_not_exist(self):
        sim = oracle.Simulator()
        sim.world.next_id = 0xFFFFFFFE
        batch = sim.new_batch()
        batch.add(oracle.make_latch(), 0)
        self.assertEqual(sim.apply(batch), Status.Ok)
        self.assertEqual(sim.next_id, 0xFFFFFFFF)
        sim.world.next_id = 0xFFFFFFFE
        sim.world.specs.clear()
        sim.world.states.clear()
        batch = sim.new_batch()
        batch.add(oracle.make_latch(), 0)
        batch.add(oracle.make_latch(), 0)
        self.assertEqual(sim.apply(batch), Status.LimitExceeded)

    def test_replace_keeps_id_and_wires_and_resets_state(self):
        self.sim.step([15])
        self.sim.step([15])
        survivor = self.sim.world.states[3]
        batch = self.sim.new_batch()
        batch.replace(2, oracle.make_gate(oracle.GateOp.OR, False, 1), 15)
        self.assertEqual(self.sim.apply(batch), Status.Ok)
        self.assertEqual(self.sim.world.states[2], 15)
        self.assertEqual(self.sim.world.states[3], survivor)
        self.assertEqual(len(self.sim.world.wires), 2)
        batch = self.sim.new_batch()
        batch.replace(2, oracle.make_const(3), 0)
        self.assertEqual(self.sim.apply(batch), Status.BadPin)
        batch = self.sim.new_batch()
        batch.replace(3, oracle.make_sensor(), 0)
        self.assertEqual(self.sim.apply(batch), Status.BadPin)
        batch = self.sim.new_batch()
        batch.replace(9, oracle.make_sensor(), 0)
        self.assertEqual(self.sim.apply(batch), Status.UnknownComponent)

    def test_wire_errors(self):
        for ops, status in (
            ([("connect", (1, 255), (2, 0))], Status.DuplicateWire),
            ([("connect", (2, 0), (1, 255))], Status.DuplicateWire),
            ([("disconnect", (1, 255), (3, 0))], Status.UnknownWire),
            ([("disconnect", (1, 0), (2, 0))], Status.BadPin),
            ([("connect", (1, 0), (2, 0))], Status.BadPin),
            ([("connect", (3, 255), (2, 0))], Status.BadPin),
            ([("connect", (2, 1), (1, 255))], Status.BadPin),
            ([("connect", (9, 255), (2, 0))], Status.UnknownComponent),
            ([("connect", (2, 0), (2, 0))], Status.BadPin),
        ):
            batch = self.sim.new_batch()
            batch.ops.extend(ops)
            before = self.sim.encode()
            self.assertEqual(self.sim.apply(batch), status, ops)
            self.assertEqual(self.sim.encode(), before)

    def test_survivors_keep_their_state_bit_for_bit_and_edits_do_not_step(self):
        sim = sim_of(golden_text("and_delay2.lnet"))
        for e in ([9, 0], [9, 5], [9, 5], [0, 5]):
            sim.step(e)
        before = {c: sim.world.states[c] for c in sim.world.specs}
        tick = sim.tick
        batch = sim.new_batch()
        batch.add(oracle.make_toggle(), 0)
        batch.disconnect((1, 255), (3, 0))
        self.assertEqual(sim.apply(batch), Status.Ok)
        self.assertEqual({c: sim.world.states[c] for c in before}, before)
        self.assertEqual(sim.tick, tick)
        self.assertEqual(sim.world.states[6], (0, 1))

    def test_removed_output_leaves_no_ghost_in_the_net(self):
        sim = sim_of("lnet 1\nc 1 const v=9\nc 2 const v=3\nc 3 actuator\nw 1.o 3.0\nw 2.o 3.0\n")
        sim.step([])
        self.assertEqual(sim.world.states[3], 9)
        batch = sim.new_batch()
        batch.remove(1)
        sim.apply(batch)
        sim.step([])
        self.assertEqual(sim.world.states[3], 3)

    def test_limits(self):
        sim = oracle.Simulator(oracle.Limits(max_components=2, max_wires=1, max_net_pins=3))
        batch = sim.new_batch()
        for _ in range(3):
            batch.add(oracle.make_latch(), 0)
        self.assertEqual(sim.apply(batch), Status.LimitExceeded)
        self.assertEqual(sim.next_id, 1)
        batch = sim.new_batch()
        batch.add(oracle.make_const(1), 0)
        batch.add(oracle.make_actuator(), 0)
        batch.connect((1, 255), (2, 0))
        batch.connect((1, 255), (2, 0))
        self.assertEqual(sim.apply(batch), Status.DuplicateWire)
        sim = oracle.Simulator(oracle.Limits(max_net_pins=3))
        batch = sim.new_batch()
        batch.add(oracle.make_const(1), 0)
        for _ in range(3):
            batch.add(oracle.make_actuator(), 0)
        for target in (2, 3, 4):
            batch.connect((1, 255), (target, 0))
        self.assertEqual(sim.apply(batch), Status.LimitExceeded)
        sim = oracle.Simulator(oracle.Limits(max_delay_cells=5))
        batch = sim.new_batch()
        batch.add(oracle.make_delay(3), 0)
        batch.add(oracle.make_delay(3), 0)
        self.assertEqual(sim.apply(batch), Status.LimitExceeded)
        sim.world.next_id = 0xFFFFFFFF
        batch = sim.new_batch()
        batch.add(oracle.make_latch(), 0)
        self.assertEqual(sim.apply(batch), Status.LimitExceeded)

    def test_scenario_edit_lines_form_one_batch_per_tau_in_line_order(self):
        net = "lnet 1\nc 1 const v=15\n"
        applied = trace_of(net, "lscn 1\nticks 2\ne 1 add c 2 actuator\ne 1 add w 1.o 2.0\n")
        self.assertEqual([r["cmd"] for r in applied], [[], [(2, 0)], [(2, 15)]])
        notes = []
        text = oracle.run_texts(net, "lscn 1\nticks 2\ne 1 add w 1.o 2.0\ne 1 add c 2 actuator\n", notes=notes)
        self.assertEqual(len(notes), 1)
        self.assertIn("UnknownComponent", notes[0])
        self.assertNotIn("2=", text)


# ---------------------------------------------------------------- teorema de asentamiento (01 seccion 8)

class SettlingTheoremTests(unittest.TestCase):
    def random_cone(self, rng):
        sim = oracle.Simulator()
        batch = sim.new_batch()
        n = rng.randrange(3, 14)
        weights = {}
        arity = {}
        for cid in range(1, n + 1):
            kind = rng.choice(("sensor", "const", "gate", "gate", "delay", "compare", "actuator"))
            if cid == 1:
                kind = "sensor"
            if kind == "sensor":
                spec, w = oracle.make_sensor(), 0
            elif kind == "const":
                spec, w = oracle.make_const(rng.randrange(16)), 0
            elif kind == "gate":
                k = rng.randrange(1, 4)
                spec, w = oracle.make_gate(rng.choice(list(oracle.GateOp)), rng.randrange(2), k), 1
            elif kind == "delay":
                d = rng.randrange(1, 4)
                spec, w = oracle.make_delay(d), d
            elif kind == "compare":
                spec, w = oracle.make_compare(rng.choice(list(oracle.CompareMode))), 1
            else:
                spec, w = oracle.make_actuator(), 1
            weights[cid] = w
            arity[cid] = oracle.input_count(spec)
            batch.add_with_id(cid, spec, 0)
        wires = []
        for cid in range(2, n + 1):
            for j in range(arity[cid]):
                sources = [s for s in range(1, cid) if oracle.has_output(batch_spec(batch, s))]
                if sources and rng.random() < 0.85:
                    src = rng.choice(sources)
                    wires.append((src, cid, j))
                    batch.connect((src, oracle.OUT), (cid, j))
        return sim, batch, weights, wires

    def test_vanishing_memory_and_fixed_point(self):
        rng = random.Random(2026101303)
        for case in range(150):
            sim_a, batch, weights, wires = self.random_cone(rng)
            self.assertEqual(sim_a.apply(batch), Status.Ok)
            sim_b = oracle.Simulator()
            copy = sim_b.new_batch()
            copy.ops.extend(batch.ops)
            sim_b.apply(copy)
            self.randomize_states(sim_a, rng)
            self.randomize_states(sim_b, rng)
            level = {}
            for cid in sorted(weights):
                preds = [s for s, d, _ in wires if d == cid]
                level[cid] = weights[cid] + max((level[p] for p in preds), default=0)
            e = rng.randrange(16)
            bound = max(level.values()) + 4
            z = self.leveled_fixed_point(sim_a, wires, e)
            for tau in range(bound + 1):
                for cid in sorted(weights):
                    if tau >= level[cid]:
                        self.assertEqual(sim_a.world.states[cid], sim_b.world.states[cid],
                                         "caso %d (a) c=%d tau=%d" % (case, cid, tau))
                        self.assertEqual(sim_a.world.states[cid], z[cid],
                                         "caso %d (b) c=%d tau=%d" % (case, cid, tau))
                sim_a.step([e] * len(sim_a.sensor_ids()))
                sim_b.step([e] * len(sim_b.sensor_ids()))

    def randomize_states(self, sim, rng):
        for cid, spec in sim.world.specs.items():
            if spec.kind == Kind.GATE:
                sim.world.states[cid] = rng.choice((0, 15))
            elif spec.kind == Kind.DELAY:
                sim.world.states[cid] = tuple(rng.randrange(16) for _ in range(spec.p0))
            elif spec.kind in (Kind.COMPARE, Kind.ACTUATOR):
                sim.world.states[cid] = rng.randrange(16)

    def leveled_fixed_point(self, sim, wires, e):
        z_out, z_state = {}, {}
        for cid in sorted(sim.world.specs):
            spec = sim.world.specs[cid]
            u = tuple(max([z_out[s] for s, d, j in wires if d == cid and j == i], default=0)
                      for i in range(oracle.input_count(spec)))
            if spec.kind == Kind.SENSOR:
                z_out[cid], z_state[cid] = e, None
            elif spec.kind == Kind.CONST:
                z_out[cid], z_state[cid] = spec.p0, None
            elif spec.kind == Kind.DELAY:
                z_state[cid] = (u[0],) * spec.p0
                z_out[cid] = u[0]
            else:
                z_state[cid] = oracle.next_state(spec, 0, u)
                z_out[cid] = z_state[cid] if oracle.has_output(spec) else 0
        return z_state


def batch_spec(batch, cid):
    for op in batch.ops:
        if op[0] == "add_with_id" and op[1] == cid:
            return op[2]
    raise KeyError(cid)


# ---------------------------------------------------------------- anfitrion (02 seccion 7)

class HostTests(unittest.TestCase):
    def test_pacer_example_of_02_section_7_3(self):
        text = spec_text("02-api-nucleo.md")
        for quote in ("`dt = 250000` → `acc = 250000`, `n = 2`, `acc = 50000`",
                      "`dt = 30000` → `acc = 80000`, `n = 0`",
                      "`dropped = 2`, `n = 4`, `acc = 10000`",
                      "`Σ dt = 810000 = (6 + 2)·100000 + 10000`"):
            self.assertIn(quote, text)
        pacer = oracle.StepPacer(100000, 4)
        self.assertEqual(pacer.advance(250000, False), 2)
        self.assertEqual((pacer.acc, pacer.dropped), (50000, 0))
        self.assertEqual(pacer.advance(30000, False), 0)
        self.assertEqual(pacer.acc, 80000)
        self.assertEqual(pacer.advance(530000, False), 4)
        self.assertEqual((pacer.acc, pacer.dropped), (10000, 2))
        self.assertEqual(250000 + 30000 + 530000, 810000)
        self.assertEqual(810000, (6 + 2) * 100000 + pacer.acc)

    def test_pacer_pause_and_conservation_on_random_sequences(self):
        pacer = oracle.StepPacer(100000, 4)
        pacer.advance(150000, False)
        self.assertEqual(pacer.advance(900000, True), 0)
        self.assertEqual((pacer.paused, pacer.acc), (900000, 50000))
        rng = random.Random(2026101304)
        for _ in range(200):
            pacer = oracle.StepPacer(rng.choice((1, 7, 100000)), rng.choice((1, 4, 9)))
            total_dt, steps = 0, 0
            for _ in range(rng.randrange(1, 60)):
                dt, paused = rng.choice((0, 1, 33333, 250000, 5_000_000)), rng.random() < 0.2
                n = pacer.advance(dt, paused)
                total_dt += dt
                steps += n
                self.assertLessEqual(n, pacer.max_steps)
                self.assertTrue(0 <= pacer.acc < pacer.step)
                self.assertEqual(total_dt, pacer.paused + (steps + pacer.dropped) * pacer.step + pacer.acc)

    def test_pacer_clamps_its_parameters(self):
        pacer = oracle.StepPacer(0, 0)
        self.assertEqual((pacer.step, pacer.max_steps), (1, 1))
        self.assertEqual(pacer.advance(5, False), 1)
        self.assertEqual(pacer.dropped, 4)

    def test_fullness_examples(self):
        self.assertEqual(oracle.fullness(50, 200), 4)
        self.assertEqual(oracle.fullness(200, 200), 15)
        self.assertEqual(oracle.fullness(1, 200), 1)
        self.assertEqual(oracle.fullness(0, 200), 0)
        self.assertEqual(oracle.fullness(-3, 200), 0)
        self.assertEqual(oracle.fullness(5, 0), 0)
        self.assertEqual(oracle.fullness(500, 200), 15)

    def test_burst_in_one_call_and_in_three_calls(self):
        net = golden_text("burst3.lnet")
        one = oracle.BurstDriver(sim_of(net))
        one.set_mode(1, "level")
        one.set_mode(2, "pulse")
        self.assertEqual(one.run_call(3, {1: 15, 2: 15}), [(3, 0, 15)])
        three = oracle.BurstDriver(sim_of(net))
        three.set_mode(1, "level")
        three.set_mode(2, "pulse")
        seen = []
        for offers in ({1: 15, 2: 15}, {}, {}):
            seen.append(three.run_call(1, offers))
            three.acknowledge_all()
        self.assertEqual(seen, [[(3, 0, 15), (4, 0, 15)], [(4, 15, 0)], []])
        self.assertEqual(one.sim.state_hash(), three.sim.state_hash())

    def test_level_sensors_make_n_steps_equal_to_n_calls(self):
        rng = random.Random(2026101305)
        net = golden_text("and_delay2.lnet")
        for _ in range(30):
            a, b = rng.randrange(16), rng.randrange(16)
            steps = rng.randrange(1, 9)
            bulk = oracle.BurstDriver(sim_of(net))
            single = oracle.BurstDriver(sim_of(net))
            bulk.run_call(steps, {1: a, 2: b})
            single.run_call(1, {1: a, 2: b})
            for _ in range(steps - 1):
                single.run_call(1, {})
            self.assertEqual(bulk.sim.state_hash(), single.sim.state_hash())

    def test_pulse_differs_by_design_when_offered_in_every_call(self):
        net = golden_text("burst3.lnet")
        bulk = oracle.BurstDriver(sim_of(net))
        every = oracle.BurstDriver(sim_of(net))
        for driver in (bulk, every):
            driver.set_mode(2, "pulse")
        bulk.run_call(3, {1: 15, 2: 15})
        for _ in range(3):
            every.run_call(1, {1: 15, 2: 15})
        self.assertNotEqual(bulk.sim.state_hash(), every.sim.state_hash())


# ---------------------------------------------------------------- formatos de texto

class TextFormatTests(unittest.TestCase):
    def test_default_keys_names_and_numbers(self):
        net = oracle.parse_lnet("lnet 1\n# comentario\nc 1 gate op=1 inv=1 k=1  # fin\n"
                                "c 2 gate op=or inv=1 k=1\nc 3 cmp m=subtract\nc 4 cmp\n"
                                "c 5 clock p=6 h=3 seed=2\nc 6 gate op=XOR k=2\n")
        specs = {cid: (spec, seed) for cid, spec, seed in net.components}
        self.assertEqual(specs[1], specs[2])
        self.assertEqual(specs[1][1], 15)
        self.assertEqual(specs[3][0], oracle.make_compare(oracle.CompareMode.SUBTRACT))
        self.assertEqual(specs[4][0], oracle.make_compare(oracle.CompareMode.COMPARE))
        self.assertEqual(specs[5], (oracle.make_clock(6, 3), 2))
        self.assertEqual(specs[6], (oracle.make_gate(oracle.GateOp.XOR, False, 2), 0))

    def test_malformed_inputs_are_rejected(self):
        bad_nets = ["", "lnet 2\n", "c 1 sensor\n", "lnet 1\nc 1 widget\n", "lnet 1\nc 1 sensor v=3\n",
                    "lnet 1\nc 0 sensor\n", "lnet 1\nc 1 const v=1 v=2\n", "lnet 1\nw 1.o\n",
                    "lnet 1\nc 1 const v=x\n", "lnet 1\nc 1 const v=1\nw 1.256 1.o\n",
                    "lnet 1\nc 1 sensor\nc 1 sensor\n", "lnet 1\nc 1 const v=16\n",
                    "lnet 1\nc 1 const v=1\nc 2 actuator\nw 1.o 2.o\n"]
        for text in bad_nets:
            with self.assertRaises(oracle.OracleError, msg=repr(text)):
                sim_of(text)
        bad_scns = ["", "lscn 1\n", "lscn 1\nticks 3\nticks 4\n", "lscn 1\nticks 3\ni 4 1=1\n",
                    "lscn 1\nticks 3\ne 1 add c 2\n", "lscn 1\nticks 3\ni 1 1\n", "lscn 1\nticks 3\nx\n",
                    "lscn 1\nticks 3\ne 5 del c 1\n"]
        for text in bad_scns:
            with self.assertRaises(oracle.OracleError, msg=repr(text)):
                oracle.run_texts("lnet 1\nc 1 sensor\n", text)
        with self.assertRaises(oracle.OracleError):
            oracle.run_texts("lnet 1\nc 1 const v=1\n", "lscn 1\nticks 2\ni 1 1=3\n")

    def test_ltrace_roundtrip_and_empty_sections(self):
        text = oracle.run_texts("lnet 1\nc 1 const v=3\n", "lscn 1\nticks 1\n")
        self.assertEqual(text, "ltrace 1\nrow 0 in | st | cmd | h=%s\nrow 1 in | st | cmd | h=%s\n"
                         % ((oracle.hex16(oracle.fnv1a64(bytes.fromhex("0100000001"))),) * 2))
        rows = oracle.parse_ltrace(text)
        self.assertEqual([r["tau"] for r in rows], [0, 1])

    def test_scenario_rows_are_ticks_plus_one_and_inputs_are_held(self):
        rows = trace_of("lnet 1\nc 1 sensor\nc 2 sensor\n", "lscn 1\nticks 3\ni 1 2=7\ni 2 1=4\n")
        self.assertEqual([r["in"] for r in rows],
                         [[(1, 0), (2, 0)], [(1, 0), (2, 7)], [(1, 4), (2, 7)], [(1, 4), (2, 7)]])

    def test_edit_add_accepts_explicit_and_implicit_ids(self):
        net = "lnet 1\nc 1 const v=15\n"
        explicit = oracle.run_texts(net, "lscn 1\nticks 3\ne 1 add c 2 toggle seed=0\ne 2 add w 1.o 2.0\n")
        implicit = oracle.run_texts(net, "lscn 1\nticks 3\ne 1 add c toggle seed=0\ne 2 add w 1.o 2.0\n")
        self.assertEqual(explicit, implicit)
        self.assertIn("st 2=15/1", explicit)

    def test_edits_before_inputs_in_the_same_tau(self):
        rows = trace_of("lnet 1\nc 1 const v=1\n", "lscn 1\nticks 1\ne 1 add c 2 sensor\ni 1 2=5\n")
        self.assertEqual(rows[1]["in"], [(2, 5)])


# ---------------------------------------------------------------- puente con docs/theory (T14 a mano)

THEORY_DIR = os.path.join(ROOT, "docs", "theory")


def theory_text(name):
    with open(os.path.join(THEORY_DIR, name), encoding="utf-8") as handle:
        return handle.read()


def theory_rows(block):
    rows = {}
    for line in block:
        m = re.match(r"^(S|R|Q|Qn)\s+([01 ]+)$", line)
        if m:
            rows[m.group(1)] = [int(v) for v in m.group(2).split()]
    return rows


class TheoryBridgeTests(unittest.TestCase):
    def test_nor_latch_traces_of_theory_04_steps_2_and_3(self):
        text = theory_text("04-memoria-biestables.md")
        section = text[text.index("### Paso 2."):text.index("### Paso 4.")]
        traces = [theory_rows(b) for b in fenced_blocks(section) if any(l.startswith("Qn") for l in b)]
        self.assertEqual(len(traces), 4)
        for rows in traces:
            length = len(rows["S"])
            scn = ["lscn 1", "ticks %d" % (length - 1)]
            for tau in range(length):
                scn.append("i %d 1=%d 2=%d" % (tau, 15 * rows["S"][tau], 15 * rows["R"][tau]))
            got = trace_of(make_goldens.LATCH_NET, "\n".join(scn) + "\n")
            self.assertEqual([1 if int(state_of(r, 3)) else 0 for r in got], rows["Q"])
            self.assertEqual([1 if int(state_of(r, 4)) else 0 for r in got], rows["Qn"])

    def test_timer_waveforms_of_theory_06_step_3(self):
        text = theory_text("06-relojes-y-contadores.md")
        waves = re.findall(r"^P=(\d+) H=(\d+)\s+([_#]+)$", text, re.M)
        self.assertEqual(len(waves), 4)
        for period, high, shape in waves:
            sim = sim_with((oracle.make_clock(int(period), int(high)), 0))
            out = ""
            for _ in shape:
                out += "#" if oracle.output_of(sim.world.specs[1], sim.world.states[1], 0) else "_"
                sim.step([])
            self.assertEqual(out, shape)

    def test_three_gate_ring_table_of_theory_06_step_2(self):
        text = theory_text("06-relojes-y-contadores.md")
        text = text[text.index("| t | g1 | g2 | g3 |"):]
        rows = re.findall(r"^\| (\d+) \| ([01]) \| ([01]) \| ([01]) \|$", text, re.M)[:16]
        self.assertEqual(len(rows), 16)
        got = trace_of(ring_net(3, 0b101), "lscn 1\nticks 15\n")
        for (t, g1, g2, g3), row in zip(rows, got):
            self.assertEqual(row["tau"], int(t))
            self.assertEqual([int(v) // 15 for _, v in row["st"]], [int(g1), int(g2), int(g3)])


class AllKindsTests(unittest.TestCase):
    """Golden extra `all_kinds`; las filas 1 a 5 se derivaron a mano de las ecuaciones de 01 seccion 3."""

    def test_hand_derived_rows(self):
        rows = golden_trace("all_kinds")
        by_id = lambda tau: dict(rows[tau]["st"])  # noqa: E731
        expected = {
            1: {4: "15", 5: "15", 6: "[0,0,0]", 7: "0", 8: "0", 9: "0", 10: "0/0", 11: "3", 13: "0"},
            2: {4: "15", 5: "0", 6: "[0,0,15]", 7: "9", 8: "6", 9: "0", 10: "15/1", 11: "4", 13: "0"},
            3: {4: "15", 5: "0", 6: "[0,15,15]", 7: "9", 8: "6", 9: "0", 10: "15/1", 11: "5", 13: "15"},
            4: {4: "15", 5: "0", 6: "[15,15,15]", 7: "15", 8: "12", 9: "0", 10: "15/1", 11: "6", 13: "15"},
            5: {4: "0", 5: "15", 6: "[15,15,15]", 7: "15", 8: "15", 9: "15", 10: "15/0", 11: "0",
                12: "15", 13: "15", 14: "0"},
        }
        for tau, states in expected.items():
            for cid, value in states.items():
                self.assertEqual(by_id(tau)[cid], value, "tau=%d c=%d" % (tau, cid))

    def test_compare_with_equal_operands_and_edits(self):
        rows = golden_trace("all_kinds")
        self.assertEqual(state_of(rows[9], 7), "5")
        self.assertEqual(state_of(rows[7], 5), "0")
        self.assertNotIn(12, dict(rows[10]["st"]))
        self.assertEqual(dict(rows[10]["cmd"]), {13: 4, 14: 0})
        self.assertEqual(dict(rows[9]["cmd"]), {12: 0, 13: 0, 14: 0})

    def test_blob_bytes_of_stateful_kinds_at_tick_8(self):
        blob = golden_bytes("all_kinds.slgc")
        status, sim = oracle.decode(blob)
        self.assertEqual(status, Status.Ok)
        self.assertEqual(sim.tick, 8)
        self.assertEqual((len(sim.world.specs), sim.next_id), (14, 16))
        self.assertEqual(int.from_bytes(blob[20:24], "little"), 16)
        self.assertEqual(sim.world.states[6], (0, 0, 15))
        for record in ("06 00 00 00 05 03 00 00 00 00 00 00 00 0F",
                       "0A 00 00 00 08 00 00 00 00 00 00 00 01",
                       "0B 00 00 00 09 07 00 03 00 00 00 03 00"):
            self.assertIn(bytes.fromhex(record), blob, record)
        self.assertEqual(len(blob), 32 + sum(11 + n for n in (0, 0, 0, 1, 1, 3, 1, 1, 1, 2, 2, 1, 1, 1))
                         + 17 * 10 + 8)



# ---------------------------------------------------------------- ficheros dorados

class GoldenFileTests(unittest.TestCase):
    NAMES = [n for n, _, _ in make_goldens.TRACE_GOLDENS]

    def test_inventory_is_exactly_the_expected_set(self):
        expected = set()
        for name in self.NAMES:
            expected |= {name + ".lnet", name + ".lscn", name + ".ltrace"}
        expected |= {"lat_not.slgc", "all_kinds.slgc", "burst3.lburst", "burst3_single.lburst",
                     "pacer.lpacer", "README.md"}
        self.assertEqual(set(golden_files()), expected)

    def test_files_on_disk_equal_a_fresh_generation(self):
        for name, data in make_goldens.generate().items():
            self.assertEqual(golden_bytes(name), data, name)

    def test_every_trace_is_regenerated_from_its_inputs_and_roundtrips(self):
        for name in self.NAMES:
            text = oracle.run_texts(golden_text(name + ".lnet"), golden_text(name + ".lscn"))
            self.assertEqual(text, golden_text(name + ".ltrace"), name)
            rows = oracle.parse_ltrace(text)
            ticks = int(re.search(r"^ticks (\d+)$", golden_text(name + ".lscn"), re.M).group(1))
            self.assertEqual(len(rows), ticks + 1, name)

    def test_files_are_plain_portable_text(self):
        for name in golden_files():
            if name.endswith((".slgc", ".md")):
                continue
            data = golden_bytes(name)
            text = data.decode("ascii")
            self.assertTrue(text.endswith("\n"), name)
            self.assertNotIn("\r", text)
            self.assertNotIn("\t", text)
            self.assertNotIn("#", text, name)
            self.assertNotIn("  ", text, name)
            self.assertFalse(any(line != line.rstrip() for line in text.split("\n")), name)

    def test_host_golden_files_are_stable_under_regeneration(self):
        nets = lambda n: golden_text(n)  # noqa: E731
        for name in ("burst3.lburst", "burst3_single.lburst"):
            self.assertEqual(oracle.regenerate_lburst(golden_text(name), nets), golden_text(name))
        self.assertEqual(oracle.regenerate_lpacer(golden_text("pacer.lpacer")), golden_text("pacer.lpacer"))
        self.assertEqual(golden_text("burst3_single.lburst").count("changes"), 3)
        pacer = golden_text("pacer.lpacer").split("\n")
        self.assertIn("call 530000 0 | steps 4 dropped 2 paused 0 acc 10000", pacer)
        self.assertIn("call 250000 0 | steps 2 dropped 0 paused 0 acc 50000", pacer)
        self.assertIn("call 30000 0 | steps 0 dropped 0 paused 0 acc 80000", pacer)

    def test_burst_files_end_in_the_same_state_hash_as_the_core_trace(self):
        final = golden_trace("burst3")[-1]["h"]
        for name in ("burst3.lburst", "burst3_single.lburst"):
            self.assertIn("end tick 3 h=" + final, golden_text(name))

    def test_equal_states_hash_equal_across_different_goldens(self):
        self.assertEqual(golden_trace("lat_not")[0]["h"], golden_trace("mergeloop")[0]["h"])

    def test_g1_scenario_matches_the_spec_wording(self):
        text = spec_text("02-api-nucleo.md")
        self.assertIn("`e = 0` para `τ < 2` y `15` desde `τ = 2`", text)
        self.assertEqual(golden_text("lat_not.lscn"), "lscn 1\nticks 4\ni 0 1=0\ni 2 1=15\n")


# ---------------------------------------------------------------- linea de ordenes

class CliTests(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run([sys.executable, "-I", ORACLE_PY, *args], capture_output=True, check=False)

    def golden_path(self, name):
        return os.path.join(GOLDEN_DIR, name)

    def test_run_prints_the_golden_trace(self):
        for name in ("lat_not", "and_delay2", "toggle_edit_connect"):
            done = self.run_cli("run", self.golden_path(name + ".lnet"), self.golden_path(name + ".lscn"))
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertEqual(done.stdout, golden_bytes(name + ".ltrace"))

    def test_encode_and_decode(self):
        done = self.run_cli("encode", self.golden_path("lat_not.lnet"), "--step-micros", "100000")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(done.stdout, golden_bytes("lat_not.slgc"))
        info = self.run_cli("decode", self.golden_path("lat_not.slgc"))
        self.assertEqual(info.returncode, 0)
        out = info.stdout.decode()
        self.assertIn("status Ok", out)
        self.assertIn("structureHash 84C3FB7A3ACB833C", out)
        self.assertIn("tail 3D144EE313AE954F reencode identical", out)

    def test_encode_after_running_a_scenario(self):
        done = self.run_cli("encode", self.golden_path("lat_not.lnet"), "--scn",
                            self.golden_path("lat_not.lscn"), "--tick", "3")
        sim = sim_of(golden_text("lat_not.lnet"))
        oracle.run_scenario(sim, oracle.parse_lscn(golden_text("lat_not.lscn")), until=3)
        self.assertEqual(done.stdout, sim.encode())
        self.assertEqual(oracle.decode(done.stdout)[1].tick, 3)

    def test_selftest_and_errors(self):
        done = self.run_cli("selftest")
        self.assertEqual(done.returncode, 0)
        self.assertIn(b"0 fallos", done.stdout)
        missing = self.run_cli("run", "/nonexistent.lnet", "/nonexistent.lscn")
        self.assertEqual(missing.returncode, 2)
        corrupt = self.run_cli("decode", self.golden_path("lat_not.lnet"))
        self.assertEqual(corrupt.returncode, 1)
        self.assertIn(b"status FormatCorrupt", corrupt.stdout)


if __name__ == "__main__":
    unittest.main()
