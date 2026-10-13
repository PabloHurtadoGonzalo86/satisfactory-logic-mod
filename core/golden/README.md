# Trazas doradas (`semanticsVersion = 1`)

Todo lo que hay aquí lo emite el oráculo (`tools/oracle/`), no el núcleo C++. Se regenera con

```
python3 -I tools/oracle/make_goldens.py            # escribe
python3 -I tools/oracle/make_goldens.py --check    # solo compara con el disco
```

y se revisa contra el texto de la spec con `python3 -I -m unittest` en `tools/oracle`. Según 03 §5, estos
ficheros solo cambian en commits que también tocan `docs/spec/`. 02 §6.5 pide un directorio por
`semanticsVersion`; mientras haya una sola, viven todos aquí.

Formato de texto (03 §4), tal como lo fija el oráculo; cada punto donde la spec era ambigua está
razonado en `docs/spec/ambiguities.md`:

- Los ficheros no llevan comentarios ni tabuladores, usan `\n` y terminan en `\n`.
- `ticks n` da **n+1 filas** (`τ = 0..n`): la fila `n` muestra el estado tras el último paso.
- Los valores son enteros decimales (`15`, no `F`); la huella son 16 dígitos hexadecimales en mayúsculas.
- `row` es una sola línea con `|` literal; las secciones vacías se escriben igual (`in |`, `cmd |`).
- `st` lista solo los componentes con estado (no `const` ni `sensor`): `s`, `[f1,f2,…]` (Delay),
  `q/p` (Toggle), `c` (Clock).
- `op` y `m` se escriben con nombre (`and|or|xor`, `compare|subtract`); el oráculo acepta también 0, 1, 2.
- `e <τ> add c <id> <kind> …` lleva el id explícito (el oráculo acepta también `add c <kind> …`); todas
  las líneas `e` de un mismo `τ` forman un único lote atómico, en orden de línea, y se aplican antes que
  las líneas `i` de ese `τ`.

| Golden | Ficheros | Origen | Qué fija |
|---|---|---|---|
| G1 `lat_not` | `.lnet .lscn .ltrace` | 02 §6.4 | sensor → NOT → actuador; las cinco huellas de la tabla |
| G7 | `lat_not.slgc` | 02 §6.4 | blob de G1 (`hostStepMicros = 100000`, `tick = 0`): 95 bytes, cola `3D144EE313AE954F`, `structureHash` `84C3FB7A3ACB833C` |
| G2 `ring3`, `ring3_zero` | `.lnet .lscn .ltrace` | 01 §9.1 | anillo de 3 NOT con semillas `(F,0,F)` (periodo 6) y `(0,0,0)` (periodo 2) |
| G3 `and_delay2` | `.lnet .lscn .ltrace` | 01 §9.2 | dos sensores → AND → `Delay(2)` → actuador; W = 4 filas |
| G4 `latch_both`, `latch_pulse` | `.lnet .lscn .ltrace` | 01 §9.3 | `Latch` nativo (id 5) frente a dos NOR cruzadas (ids 3 y 4) |
| G5 `ring2` | `.lnet .lscn .ltrace` | 03 §4 | doble búfer: `X1 = (F,F)`; el control en sitio no es un golden |
| G6 `wired9` | `.lnet .lscn .ltrace` | 03 §4 | dos `Const(9)` en un actuador: `o = 9`, no 15 |
| G8 `toggle_new_high` | `.lnet .lscn .ltrace` | 01 §6.2 | `Toggle` creado y cableado en el mismo lote a una red alta: no conmuta |
| G9 `toggle_edit_connect` | `.lnet .lscn .ltrace` | 01 §6.2 | `Toggle` cableado después: conmuta una vez (fila 4) |
| G10 `clock6_3` | `.lnet .lscn .ltrace` | 03 §4 | `Clock(6,3)`: `c = 0..5`; el actuador (id 2) ve la salida un tick después |
| G11 `burst3` | `.lnet .lscn .ltrace` | 02 §7.2 | traza del núcleo de la ráfaga: muestras `(15,15)`, `(15,0)`, `(15,0)` |
| G11 `burst3.lburst`, `burst3_single.lburst` | texto | 02 §7.2 | llamada de 3 pasos y tres llamadas de 1 paso; `changes` esperado |
| G12 `pacer.lpacer` | texto | 02 §7.3 | `dt = 250000, 30000, 530000` → 2, 0, 4 pasos, 2 descartados, `acc = 10000` |
| G13 `chain3` | `.lnet .lscn .ltrace` | 01 §8 | cota alcanzada: `X2 ≠ z`, `X3 = z` |
| G14 `mergeloop`, `mergeloop_e9` | `.lnet .lscn .ltrace` | 01 §5 | lazo creado por fusión: periodo 2 con `e = 0`; `B = 0` fijo y `C = 15, 9, 9…` con `e = 9` |
| extra `all_kinds` | `.lnet .lscn .ltrace`, `all_kinds.slgc` | oráculo (no está en 03 §4) | los 9 kinds, `Compare` con `A = B`, `Subtract`, `Xor`, todas las ediciones (`add c`, `rep c`, `del c`, `add w`, `del w`); el blob a `tick = 8` lleva `Delay(3)` con `[0,0,15]`, `Toggle` `0/1`, `Clock` con `c = 3` y `nextId = 16` con 14 componentes. Las filas 1 a 5 se derivaron a mano (ver `test_oracle.py`). Motivo: `docs/spec/ambiguities.md` C1 |

## Formatos del oráculo fuera de 03 §4

G11 y G12 no caben en `lnet/lscn/ltrace` (Level/Pulse, `changes` y el acumulador son del anfitrión), así
que el oráculo define dos formatos de texto mínimos. Las líneas después de `|` son lo esperado y las
regenera el oráculo.

```
lburst 1
net burst3.lnet
sensor 1 level                 # level | pulse
sensor 2 pulse
call 3 offer 1=15 2=15 | changes 3:0>15      # call <pasos> offer <id>=<lectura> … | changes <id>:<confirmado>><final> …
end tick 3 h=6C3AEAE8B44B71F0                # tick final y stateHash

lpacer 1
step 100000 max 4
call 250000 0 | steps 2 dropped 0 paused 0 acc 50000     # call <dt> <en pausa 0|1> | acumulados tras la llamada
```

En `lburst` el anfitrión confirma todos los cambios al terminar cada llamada; un `Level` sin lectura en una
llamada conserva la última, un `Pulse` sin lectura vale 0. El modelo no cubre sensores sin resolver, la
guarda de asentamiento, `resync` ni el blob del `FrameDriver`.
