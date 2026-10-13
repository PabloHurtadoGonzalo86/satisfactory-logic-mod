#!/usr/bin/env python3
"""Genera core/golden/ con el oraculo (G1..G14). `--check` solo compara con lo que hay en disco.

    python3 -I tools/oracle/make_goldens.py [--check]

Los textos de entrada (lnet, lscn) estan aqui, a mano; las trazas, el blob y los ficheros
de anfitrion los emite el oraculo y se revisan contra el texto de la spec en test_oracle.py.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import oracle  # noqa: E402

GOLDEN_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "core", "golden"))
NOT_SEED_15 = "gate op=or inv=1 k=1 seed=15"
NOT_SEED_0 = "gate op=or inv=1 k=1 seed=0"

LAT_NOT_NET = f"""lnet 1
c 1 sensor
c 2 {NOT_SEED_15}
c 3 actuator
w 1.o 2.0
w 2.o 3.0
"""

RING3_NET = f"""lnet 1
c 1 {NOT_SEED_15}
c 2 {NOT_SEED_0}
c 3 {NOT_SEED_15}
w 3.o 1.0
w 1.o 2.0
w 2.o 3.0
"""

RING3_ZERO_NET = f"""lnet 1
c 1 {NOT_SEED_0}
c 2 {NOT_SEED_0}
c 3 {NOT_SEED_0}
w 3.o 1.0
w 1.o 2.0
w 2.o 3.0
"""

LATCH_NET = f"""lnet 1
c 1 sensor
c 2 sensor
c 3 gate op=or inv=1 k=2 seed=0
c 4 gate op=or inv=1 k=2 seed=15
c 5 latch seed=0
w 2.o 3.0
w 4.o 3.1
w 1.o 4.0
w 3.o 4.1
w 1.o 5.0
w 2.o 5.1
"""

MERGELOOP_NET = f"""lnet 1
c 1 sensor
c 2 {NOT_SEED_15}
c 3 actuator
w 1.o 2.0
w 3.0 1.o
w 3.0 2.o
"""

BURST_NET = """lnet 1
c 1 sensor
c 2 sensor
c 3 actuator
c 4 actuator
w 1.o 3.0
w 2.o 4.0
"""

ALL_KINDS_NET = f"""lnet 1
c 1 sensor
c 2 sensor
c 3 const v=5
c 4 gate op=xor inv=0 k=3
c 5 gate op=and inv=1 k=2 seed=15
c 6 delay d=3
c 7 cmp m=compare
c 8 cmp m=subtract
c 9 latch
c 10 toggle seed=0
c 11 clock p=7 h=3 seed=2
c 12 actuator
c 13 actuator
w 1.o 4.0
w 2.o 4.1
w 3.o 4.2
w 1.o 5.0
w 2.o 5.1
w 4.o 6.0
w 1.o 7.0
w 3.o 7.1
w 1.o 8.0
w 2.o 8.1
w 7.o 9.0
w 2.o 9.1
w 2.o 10.0
w 6.o 12.0
w 8.o 13.0
w 11.o 13.0
"""

ALL_KINDS_SCN = """lscn 1
ticks 12
i 0 1=0 2=0
i 1 1=9 2=3
i 3 1=15
i 4 2=0
i 6 2=15
i 8 1=5 2=1
e 5 add c 14 actuator
e 5 add w 9.o 14.0
e 6 add c 15 const v=3
e 7 del c 15
e 7 rep c 5 gate op=or inv=0 k=2 seed=0
e 9 del w 11.o 13.0
e 10 del c 12
"""

# (nombre, texto lnet, texto lscn)
TRACE_GOLDENS = [
    ("lat_not", LAT_NOT_NET, "lscn 1\nticks 4\ni 0 1=0\ni 2 1=15\n"),
    ("ring3", RING3_NET, "lscn 1\nticks 6\n"),
    ("ring3_zero", RING3_ZERO_NET, "lscn 1\nticks 4\n"),
    ("and_delay2", """lnet 1
c 1 sensor
c 2 sensor
c 3 gate op=and inv=0 k=2
c 4 delay d=2
c 5 actuator
w 1.o 3.0
w 2.o 3.1
w 3.o 4.0
w 4.o 5.0
""", "lscn 1\nticks 8\ni 0 1=0 2=0\ni 1 1=9\ni 2 2=5\ni 4 1=0\ni 5 2=0\n"),
    ("latch_both", LATCH_NET, "lscn 1\nticks 9\ni 0 1=15 2=15\ni 3 1=0 2=0\n"),
    ("latch_pulse", LATCH_NET, "lscn 1\nticks 7\ni 1 1=15\ni 2 1=0\n"),
    ("ring2", f"""lnet 1
c 1 {NOT_SEED_0}
c 2 {NOT_SEED_0}
w 2.o 1.0
w 1.o 2.0
""", "lscn 1\nticks 4\n"),
    ("wired9", """lnet 1
c 1 const v=9
c 2 const v=9
c 3 actuator
w 1.o 3.0
w 2.o 3.0
""", "lscn 1\nticks 2\n"),
    ("toggle_new_high", "lnet 1\nc 1 const v=15\n",
     "lscn 1\nticks 4\ne 1 add c 2 toggle seed=0\ne 1 add w 1.o 2.0\n"),
    ("toggle_edit_connect", "lnet 1\nc 1 const v=15\n",
     "lscn 1\nticks 6\ne 1 add c 2 toggle seed=0\ne 3 add w 1.o 2.0\n"),
    ("clock6_3", "lnet 1\nc 1 clock p=6 h=3\nc 2 actuator\nw 1.o 2.0\n", "lscn 1\nticks 12\n"),
    ("burst3", BURST_NET, "lscn 1\nticks 3\ni 0 1=15 2=15\ni 1 2=0\n"),
    ("chain3", f"""lnet 1
c 1 sensor
c 2 {NOT_SEED_0}
c 3 {NOT_SEED_0}
c 4 {NOT_SEED_0}
w 1.o 2.0
w 2.o 3.0
w 3.o 4.0
""", "lscn 1\nticks 5\n"),
    ("mergeloop", MERGELOOP_NET, "lscn 1\nticks 6\n"),
    ("mergeloop_e9", MERGELOOP_NET, "lscn 1\nticks 6\ni 0 1=9\n"),
    ("all_kinds", ALL_KINDS_NET, ALL_KINDS_SCN),
]

# (golden de origen, fichero, hostStepMicros, tick del blob)
BLOB_GOLDENS = [("lat_not", "lat_not.slgc", 100000, 0), ("all_kinds", "all_kinds.slgc", 100000, 8)]

BURST_SKELETONS = [
    ("burst3", "lburst 1\nnet burst3.lnet\nsensor 1 level\nsensor 2 pulse\n"
               "call 3 offer 1=15 2=15\n"),
    ("burst3_single", "lburst 1\nnet burst3.lnet\nsensor 1 level\nsensor 2 pulse\n"
                      "call 1 offer 1=15 2=15\ncall 1 offer\ncall 1 offer\n"),
]

PACER_SKELETON = ("lpacer 1\nstep 100000 max 4\ncall 250000 0\ncall 30000 0\ncall 530000 0\n")


def generate():
    files = {}
    for name, net, scn in TRACE_GOLDENS:
        files[name + ".lnet"] = net.encode("ascii")
        files[name + ".lscn"] = scn.encode("ascii")
        files[name + ".ltrace"] = oracle.run_texts(net, scn).encode("ascii")
    sources = {n: (net, scn) for n, net, scn in TRACE_GOLDENS}
    for name, filename, step_micros, tick in BLOB_GOLDENS:
        net, scn = sources[name]
        sim = oracle.build_simulator(oracle.parse_lnet(net), host_step_micros=step_micros)
        oracle.run_scenario(sim, oracle.parse_lscn(scn), until=tick)
        files[filename] = sim.encode()
    nets = {n + ".lnet": t for n, t, _ in TRACE_GOLDENS}
    for name, skeleton in BURST_SKELETONS:
        files[name + ".lburst"] = oracle.regenerate_lburst(skeleton, nets.__getitem__).encode("ascii")
    files["pacer.lpacer"] = oracle.regenerate_lpacer(PACER_SKELETON).encode("ascii")
    return files


def main(argv):
    files = generate()
    if "--check" in argv:
        stale = []
        for name, data in sorted(files.items()):
            path = os.path.join(GOLDEN_DIR, name)
            try:
                with open(path, "rb") as handle:
                    current = handle.read()
            except OSError:
                current = None
            if current != data:
                stale.append(name)
        print("stale: %s" % (", ".join(stale) if stale else "none"))
        return 1 if stale else 0
    os.makedirs(GOLDEN_DIR, exist_ok=True)
    for name, data in sorted(files.items()):
        with open(os.path.join(GOLDEN_DIR, name), "wb") as handle:
            handle.write(data)
        print("wrote %s (%d bytes)" % (name, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
