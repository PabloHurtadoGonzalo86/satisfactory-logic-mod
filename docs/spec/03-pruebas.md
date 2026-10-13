# 03. Pruebas

Estado: borrador de diseño del 13-oct-2026. Nada de esto se ha ejecutado: el núcleo aún no existe. Semántica en [01](01-semantica.md), API en [02](02-api-nucleo.md), decisiones en [04](04-decisiones.md).

## 1. Principios

- Todo número de diseño se deriva en el texto o se reproduce con los flujos de bytes y fórmulas que el texto da; T10 y T12 fijan los hashes, T6 la profundidad NAND y G1 a G14 las trazas. Las cifras de PR1 a PR7 no se usan (critic: no guardaron código).
- El VPS de desarrollo (compartido, sin motor, unos 2 GB libres, sin swap, OOM del kernel el 9 y el 13 de octubre, 0001 §6) solo ejecuta `g++` y `python3` con pocos casos; lo pesado va a CI o al PC del propietario.
- Cada propiedad dice qué **no** afirma.
- `ReferenceSim` (solo en `tests/`) es un intérprete ingenuo basado en mapas, independiente de `Simulator`: `step(muestras, orden)` con doble búfer real y `stepInPlace(muestras, orden)` como control negativo.

## 2. Propiedades T1 a T14

| Id | Enunciado | Método | No se afirma |
|---|---|---|---|
| T1 | Para toda netlist válida, estado inicial legal y `e`, la traza (estados y mandos) es idéntica en `Simulator`, en `ReferenceSim` con doble búfer evaluando en cualquier permutación (nueva en cada tick) y en la netlist reconstruida insertando en cualquier orden con los mismos ids | Corpus aleatorio con ciclos y varios conductores. Control negativo: `stepInPlace` con dos órdenes **debe** diferir (G5) | Nada para actualización en sitio |
| T2 | Para toda biyección `φ` de ids, la traza de `φ(N)` es `φ(traza de N)`; los ids de `add` en distinto orden se prueban aquí | Permutaciones sobre el corpus de T1 | Coordenadas: el núcleo no las recibe |
| T3 | Teorema de 01 §8, (a) y (b), en todo cono acíclico de `Const`, `Sensor`, `Gate`, `Compare`, `Delay` | DAG generados sobre el grafo inducido por redes (todo lector posterior a todos los conductores de su red), dos `X0`, `L+8` ticks, evaluador nivelado independiente; G13 | Cota ajustada; ausencia de glitches; nada con `Latch`, `Toggle`, `Clock` ni ciclos |
| T4 | En esos conos con `e` constante, `X_τ = X_{τ+1}` para `τ ≥ τ0 + L`; todo `Delay(d)` cumple `out(τ+d) = in(τ)` | Tras T3, `2L+16` ticks | Los sensores que oscilan dan salidas que oscilan: no es un fallo |
| T5 | Fusión conmutativa, asociativa, idempotente, identidad 0 y `t(a max b) = t(a) ∨ t(b)` (16³); `Gate(op,inv,k)` con `k` 1..3; `Compare` y `Subtract` (16²); `Latch`, `Toggle`; `Clock` con `P ≤ 12`, toda `H` y fase; `Delay(d)` ≡ cadena de `d` `Delay(1)` (`d ≤ 8`) | Tablas del oráculo con otra formulación | Nada fuera de esos dominios |
| T6 | Las 256 funciones booleanas de 3 variables se construyen con tres `Sensor`, NAND (`Gate(And, inv, 2)`) y un actuador, con profundidad NAND ≤ 5; en las 8 asignaciones constantes (también con lecturas 1..14) el mando en `τ ≥ L` (≤ 6) vale `15·f` | BFS de cierre desde las 3 proyecciones; 256 × 8 ejecuciones | Mínimo de puertas; funciones de nibble |
| T7 | `groups` es exactamente el conjunto de componentes de un camino cerrado del grafo inducido por redes (autoaristas y lazos por fusión incluidos), agrupado por SCC; sin desbordar la pila | Cierre transitivo O(n³), `n ≤ 40`; G14; cadena de 10⁵ con `ulimit -s 256` | Coste fuera de lo medido (§8) |
| T8 | Con `e` constante toda traza es finalmente periódica: `μ ≥ 0`, `λ ≥ 1`, `X_{μ+λ} = X_μ`, `μ+λ ≤ ∏\|S_c\|`. Anillo de `n` NOT con los `2^n` seeds (theory/06): n=3 → 2 estados de periodo 2 y 6 de 6; n=5 → 2 y 30 (periodo 10); n=7 → 2 y 126 (14); n=9 → 2 (2), 6 (6) y 504 (18); todos `μ = 0`; con un defecto, `2n`. `NOT` + `Delay(m)` en lazo desde la cadena asentada: `2(m+1)`; `Clock(P,H)`: `P`; NOR cruzado soltado de S=R: 2; `Latch` nativo: 1 | Brent comparando el estado **completo**, no el hash | Cota de `μ` mejor que `\|S\|` |
| T9 | `Simulator`, `ReferenceSim` y el oráculo Python dan estado completo y mandos iguales en cada tick, con lotes de edición aleatorios; todo evaluador incremental futuro debe pasarla | Netlists, marcos y lotes aleatorios | Rendimiento |
| T10 | Mismo `(N0, seeds, lotes por tick, marcos)` → mismo `stateHash` en cada tick; `encode` en el tick `k`, `decode` y continuar ≡ ejecución ininterrumpida; cambiar cualquier campo de estado de cualquier kind cambia `stateHash`, cambiar `tick` no; G1 y G7 coinciden en toda la matriz | Grabador de pruebas; goldens versionados; autotest de MSVC (§9) | Float→nibble; el no-determinismo del juego |
| T11 | `apply(lote)` + `step` ≡ reconstruir con la netlist editada y el estado trasplantado (supervivientes intactos, nuevos en su seed) + `step`; un lote inválido o fuera de límites deja todo idéntico; `connect`→`remove` del mismo componente vale, `remove`→`add` del mismo id da `RetiredId`, `replace` conserva cables; G8, G9 | Lotes aleatorios válidos e inválidos | Fallos de memoria |
| T12 | `encode(decode(b)) == b`; toda sustitución de un byte y toda truncación de los fixtures se rechaza; invariantes rotas con cola recalculada (estado de `Gate` 7, fase de `Clock` ≥ P, ranura de `Delay` 16, `nextId` ≤ id presente, `nextId = 0`, id `0xFFFFFFFF`, versión 0, cable sin normalizar) dan `FormatCorrupt`; versión mayor da `FormatTooNew` sin mutar; cada límite excedido da `LimitExceeded` | Corrupción exhaustiva de un byte sobre G7; fuzz bajo sanitizers | Cambios de varios bytes que preserven la cola |
| T13 | `StepPacer`: pasos ≤ máximo, `acc < step` y la conservación de 02 §7.3 (G12). `FrameDriver`: ráfaga G11; con sensores `Level` constantes, `n` pasos en una llamada ≡ `n` llamadas de un paso (mismo `stateHash`); `Pulse` difiere por diseño; retención, `unresolved`, guarda de asentamiento, entregas pendientes y ida y vuelta de su blob | Secuencias aleatorias de `dt`, pausas y lecturas | Que el mundo vea lo mismo con otra agrupación |
| T14 | Las trazas de `docs/theory` se reproducen sin desplazar filas, con 1 ↦ 15 (§3); el flip-flop D maestro-esclavo (theory/04 Paso 6) y el detector 101 (theory/07) se construyen con el catálogo | Adaptador Python que exporta las trazas de `Circuit` y `Net` (`check_B.py`, `check_C.py`) a `lscn`/`ltrace` | Lo que v0 no tiene (§3) |

## 3. Puente con theory (T14)

Se **espera** por construcción (mismas ecuaciones, fuentes de retardo cero): theory/04 Tablas 4.1 a 4.8 y Pasos 2 y 3 (latches con NOR y NAND con sensores para S y R; latch D y flip-flops D y T hechos con puertas); de theory/06, anillos, bucles con BUF (`Gate` con `k = 1` o `Delay(1)`), temporizadores (`Clock`), divisores (`Toggle` con la entrada en CLK, que es T=1), detector de flancos y alargador. Comprobado **a mano** aquí: los Pasos 2 y 3 de theory/04 (01 §9.3) y el anillo de 3 (01 §9.1); lo demás lo decide T14. **No reproducen** en v0: los contadores de theory/06 Paso 5 (flip-flop T con T como entrada y `Qn` simultánea) ni las máquinas de theory/07 con flip-flops D primitivos, salvo construidas con puertas.

## 4. Trazas doradas de texto

UTF-8 con `\n`, abiertos en modo binario; los números se formatean con código propio, no con la configuración regional.

```
lnet 1                         # netlist + seeds
c <id> <kind> [clave=valor …]  # const sensor actuator gate delay cmp latch toggle clock; claves v op inv k d m p h seed
w <pin> <pin>                  # pin = <id>.o | <id>.<n>
lscn 1                         # escenario
ticks <n>
i <τ> <id>=<v> …               # sensores desde τ, se mantienen (0 antes de la primera orden)
e <τ> add c … | del c <id> | rep c <id> … | add w <pin> <pin> | del w <pin> <pin>   # lote de τ, en orden de línea
ltrace 1                       # salida
row <τ> in <id>=<v> … | st <id>=<estado> … | cmd <id>=<v> … | h=<hex16>
```

Estados: `s`; `[f1,f2,…]` para `Delay`; `q/p` para `Toggle`; `c` para `Clock`; todo por id ascendente. Los escenarios se generan una vez y se escriben a fichero: el oráculo nunca reproduce el PRNG del generador.

**Contrato del formato.** Lo fija el oráculo; el porqué de cada punto está en [ambiguities.md](ambiguities.md) (A1 a A11). `slg_run` y el oráculo tienen que coincidir byte a byte con él.

- **Ficheros**: ASCII con `\n`, un registro por línea y un `\n` final; sin tabuladores, sin espacios dobles ni al final de línea, y **sin comentarios**: los `#` del bloque de arriba anotan este documento, no son sintaxis de los goldens. Un lector puede tolerar `#` hasta fin de línea; un escritor no lo emite.
- **`ticks n`** ejecuta `n` pasos y escribe `n + 1` filas (`τ = 0..n`): la fila `n` muestra el estado tras el último paso y la muestra `e_n` retenida. G1 usa `ticks 4` (filas 0 a 4) y G3 `ticks 8` (filas 0 a 8).
- **Fila**: una sola línea con las cuatro secciones `in`, `st`, `cmd` y `h=` **siempre**, también vacías (`row 0 in | st | cmd | h=…`); el `|` es literal (en la línea `e` del bloque, en cambio, separa alternativas) y las fichas van separadas por un solo espacio.
- **Valores**: enteros en decimal (`15`, nunca `F`; `F` y `#` son solo notación de lectura en las ondas y tablas de la spec). `h=` son 16 dígitos hexadecimales **en mayúsculas**, con ceros a la izquierda; un lector puede aceptar minúsculas, un escritor no las emite.
- **Qué lista cada sección**, por id ascendente: `in`, todos los sensores; `cmd`, todos los actuadores; `st`, solo los componentes con estado (ni `const` ni `sensor`), así que un actuador sale en `st` y en `cmd` (su `o`). Notación en `st`: `Delay` como `[0,15,15]` (sin espacios, `f1` primero); `Toggle` como `q/p` (`0/1`); `Clock` como su `c`; el resto, el entero.
- **`in` y `cmd`**: `in` es el valor retenido **tal cual** lo da el escenario, sin saturar (el núcleo satura a 15 y cuenta, I8; `Signal` es de 8 bits: 0..255); un sensor no nombrado conserva el suyo y antes de la primera orden vale 0. `cmd` de la fila `τ` son los `o` de los actuadores en `X_τ` (`commands()`), no los mandos entregados al mundo (`changes()` del `FrameDriver`, 02 §7.2). Una línea `i` sobre un id que en ese `τ` no es un sensor, o con `τ > ticks` (y lo mismo una `e`), es un error de entrada.
- **`c <id> <kind> [clave=valor …]`**, en `lnet` y en `add c` y `rep c`. Claves por kind: `const`, `v`; `gate`, `op`, `inv`, `k`; `delay`, `d`; `cmp`, `m`; `clock`, `p`, `h`; `sensor`, `actuator`, `latch` y `toggle` solo `seed`, que admiten todos. Una clave ausente vale 0 (como un `Spec` por defecto) y un `seed` ausente vale `restSeed` del `Spec` (01 §6.1); en `rep c`, el del `Spec` **nuevo**. `op` se escribe `and`, `or` o `xor` y `m`, `compare` o `subtract` (un lector puede aceptar también los códigos 0, 1, 2 y 0, 1 de 02 §6.1); `inv` es 0 o 1. Una clave que no existe en el kind es un error de entrada.
- **Pines**: en `w <pin> <pin>` los dos van en cualquier orden y el núcleo normaliza (`a < b`, con `.o` = 255 mayor que cualquier entrada). Los goldens mezclan el orden a propósito (`w 3.0 1.o` en `mergeloop`).
- **Líneas `e`**: cada línea es **una** operación. Todas las del mismo `τ` forman **un único** `EditBatch` atómico, en orden de línea, aplicado en el límite de tick: antes de las líneas `i` de ese `τ` y antes de la fila `τ`. `add c` lleva el **id explícito**, como en `lnet` (`e 1 add c 2 toggle seed=0`, `addWithId`): el bloque de arriba escribe `<id>` en `del c` y `rep c` pero no en `add c`, y los goldens usan siempre la forma explícita (un lector puede aceptar `add c toggle seed=0`, con id = `nextId` más las altas previas del lote). Un lote rechazado no deja rastro en el `ltrace`; el lector lo avisa fuera de él.
- **Netlist inicial**: se carga como un único lote con los ids explícitos, en cualquier orden (02 §4), y tras cargarla `nextId = max(id) + 1` (1 si está vacía); 02 §6.4 lo confirma para G7 (`nextId = 4` con los ids 1, 2 y 3).
- **Blobs**: `hostStepMicros` y el `tick` de un blob no caben en `lnet` ni en `lscn`: son argumentos de la herramienta que lo escribe (el oráculo: `encode --step-micros M --tick K`, donde `K > 0` ejecuta antes el escenario hasta el tick `K`).
- **Anfitrión**: G11 y G12 no caben en `lnet`/`lscn`/`ltrace` (Level/Pulse, `changes` y el acumulador son del anfitrión, y G12 no tiene netlist): usan los formatos de texto `lburst` y `lpacer`, definidos en `core/golden/README.md`.

| Golden | Contenido | Origen |
|---|---|---|
| G1 `lat_not` | sensor → NOT → actuador, con huellas | 02 §6.4 |
| G2 `ring3`, `ring3_zero` | anillo de 3 NOT, periodos 6 y 2 | 01 §9.1 |
| G3 `and_delay2` | 2 sensores → AND → `Delay(2)` → actuador | 01 §9.2 |
| G4 `latch_both`, `latch_pulse` | `Latch` frente a NOR | 01 §9.3 |
| G5 `ring2` | dos NOT cruzados, seeds `(0,0)`. Doble búfer: `X1 = (F,F)`. En sitio con orden `a,b`: `a ← NOT 0 = F`, `b ← NOT F = 0`, da `(F,0)`; con orden `b,a`, `(0,F)` | theory/04 Paso 0 |
| G6 `wired9` | dos `Const(9)` en la entrada de un actuador: `o = max(9,9) = 9` en `X1` (la suma daría 15) | aquí |
| G7 `lat_not.slgc` | blob de G1 (misma base `lat_not`, con `hostStepMicros = 100000` y `tick = 0`), 95 bytes | 02 §6.4 |
| G8, G9 | `toggle_new_high`, `toggle_edit_connect` | 01 §6.2 |
| G10 `clock6_3` | `Clock(6,3)`: `c = 0..5`, salida 15 si `c ≥ 3`: `___###___###`. Ninguna columna de `ltrace` muestra esa salida (`st` da `c`), así que el golden lleva un actuador (id 2) cableado a ella: su `cmd` reproduce la onda un tick después (peso 1 del actuador) | theory/06 Paso 3 |
| G11 `burst3`, G12 `pacer` | ráfaga de 3 pasos (traza del núcleo `burst3.{lnet,lscn,ltrace}`, más `burst3.lburst` con una llamada de 3 pasos y `burst3_single.lburst` con tres de 1 paso); `dt = 250000, 30000, 530000` → 2, 0 y 4 pasos, 2 descartados, `acc = 10000` (`pacer.lpacer`) | 02 §7.2, §7.3 |
| G13 `chain3`, G14 `mergeloop` y `mergeloop_e9` | cota alcanzada; lazo creado por fusión, en dos ejecuciones: `e = 0` (`mergeloop`, periodo 2) y `e = 9` (`mergeloop_e9`, `B = 0` fijo) | 01 §8, §5 |
| extra `all_kinds` | los 9 kinds, `Compare` con `A = B`, `Subtract`, `Xor` y todas las ediciones (`add c`, `rep c`, `del c`, `add w`, `del w`); su blob `all_kinds.slgc`, a `tick = 8`, lleva `Delay(3)` con `[0,0,15]`, `Toggle` `0/1`, `Clock` con `c = 3` y `nextId = 16` con 14 componentes, que es lo que el blob de G7 no ejercita (el `Delay`, el orden de los dos bytes de la fase de `Clock`, el par `q/p` y un `nextId` distinto de `componentCount + 1`) | el oráculo (T10, T12, M13) |

## 5. Oráculo Python (N-versión)

`tools/oracle/oracle.py`, solo biblioteca estándar, escrito por **otro agente con acceso solo a 01, 02 y a este documento**, sin ver el C++. Lee `lnet` y `lscn`, escribe `ltrace` y tiene su propio codec de blob y de FNV-1a. Cada discrepancia es un bug o una ambigüedad (estas van a `docs/spec/ambiguities.md`, se arregla el texto y el estado de cada una consta en [04](04-decisiones.md) §5). Los goldens los emite el oráculo, se revisan a mano y solo cambian en commits que tocan `docs/spec/`. De noche, `slg_run` y el oráculo comparan con `cmp` un corpus de escenarios; al primer desacuerdo se relanzan con detalle completo. `Circuit` y `Net` de `docs/theory` son una tercera implementación ya existente.

## 6. Fuzz y semillas

PRNG propio SplitMix64 (algoritmo público; un test fija sus primeras salidas con vectores del oráculo) y Fisher–Yates propio; prohibidos `<random>`, `std::shuffle` y `rand`. El caso `i` de `Tn` usa `SplitMix64(base_Tn XOR i)` con `base_Tn = 2026101300 + n`. En CI `SLG_CASES = 2000`; de noche 100000, con base derivada de la fecha e impresa. Un fallo imprime `SLG_SEED`, `SLG_CASE` y el escenario reducido por delta-debugging, listo para `core/golden/regressions/`. Los generadores cubren DAG, redes cíclicas, anillos, cadenas de 10⁵, fan-in ancho, latches y lotes de edición, siempre con pines sin cable, autolazos y varios conductores. Fuzz de bytes: `fuzz_decode`, `fuzz_lnet` y `fuzz_frame` (ediciones y marcos, comparando `Simulator` y `ReferenceSim`), con libFuzzer (clang, ASan+UBSan) en CI. En el VPS, `SLG_CASES = 200` con UBSan.

## 7. Mutantes (al menos 12; aquí 14)

Cada mutante es un parche de una línea, `tests/mutants/Mnn.patch`, que un script nocturno aplica a una copia temporal del árbol (sin macros en el código de producción). Todo mutante debe hacer fallar el test indicado; un superviviente es un test que falta o un equivalente demostrado en `mutants.md`.

| Id | Mutante | Lo mata |
|---|---|---|
| M01 | actualización en sitio (un solo búfer) | T1 (control negativo), G5 |
| M02 | fusión = suma saturada | T5 `(9,9)`, G6 |
| M03 | fusión = "el último conductor gana" | T1 (orden de inserción), T5 |
| M04 | veracidad `x > 1` | T5 en `x = 1` |
| M05 | `Delay` con `d−1` o `d+1` | T4, G3 |
| M06 | `Compare` con `<` en lugar de `≤` | T5 con `A = B` |
| M07 | dominancia del `Latch` invertida | G4, T5 |
| M08 | `Toggle` por nivel en lugar de flanco | T5, T14 (divisores) |
| M09 | `Toggle` con `p` inicial 0 | G8 |
| M10 | `Toggle` que se rearma al conectar | G9 |
| M11 | sensor con un tick de retraso, o actuador que emite `X_τ` | G1, T14 |
| M12 | `nextId` no se guarda | T10, T11 |
| M13 | FIFO de `Delay` guardado al revés | T10 y T12 sobre el blob `all_kinds` (el de G7 no tiene `Delay`; la ida y vuelta no lo ve) |
| M14 | `StepPacer` pierde el resto (`acc = 0`) | T13, G12 |

## 8. Benchmark

`slg_bench`, sin dependencias, solo con el evaluador Full: `N` en {10³, 10⁴, 10⁵} con semilla fija; redes al azar, en cadena, de fan-in ancho y anillos. Mide mediana, p99 y máximo de ns por tick tras calentar, bytes por componente (`sizeof` real) y contadores deterministas (componentes evaluados, lecturas de red). En CI solo se vigilan los contadores; el tiempo se mide en el PC del propietario y en el servidor con fábrica real. `N_max = ⌊B / (c_p99 · margen)⌋`, con `B` la parte de la llamada asignada a la lógica y `c_p99` el coste medido por componente; el techo es el menor entre este y el del adaptador (runtime-api C3, C11). Hasta entonces los límites de 01 §7 son [P]. En este VPS, `N = 10⁵` solo con un `ulimit -v` explícito.

## 9. Avisos, sanitizers y matriz de CI

- g++ y clang++: `-std=c++20 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wfloat-equal`. "0 avisos" es solo para nuestro código (README).
- ASan+UBSan con `-fno-sanitize-recover=all`, `-D_GLIBCXX_ASSERTIONS` y `-ftrivial-auto-var-init=pattern` (una lectura sin inicializar cambiaría un hash). TSan no aplica: un solo hilo (C1). ASan suele no arrancar bajo `ulimit -v`: en el VPS solo UBSan; ASan completo, en CI.
- Un script falla si `core/` contiene `float`, `double`, `<random>`, `unordered_`, `rand(`, `<chrono>`, `throw`, `try`, `long` sin ancho fijo, `memcpy` en la serialización o `std::sort` sobre claves no únicas.

| Fila | Contenido | Cuándo |
|---|---|---|
| `build` | g++ y clang++, `-O0` y `-O2`, `-Werror`; `-fno-exceptions -fno-rtti` [P: no verificado que el adaptador los desactive] | cada PR |
| `sanitize` | ASan+UBSan en g++ y clang++ | cada PR (CI, no el VPS) |
| `oracle` | goldens contra el oráculo; corpus de 5000 escenarios | goldens cada PR, corpus de noche |
| `theory` | los `check_*.py` actuales y T14 | cada push |
| `digest` | cada celda imprime el FNV de los hashes dorados y de 500 escenarios sembrados; un job final exige igualdad | cada PR |
| `mutation`, `fuzz` | M01 a M14; 60 s por objetivo en PR y 1 h de noche | de noche |
| `bench` | contadores en CI; tiempos informativos | manual |
| `extra` | libc++, aarch64 con qemu-user, `-m32`, s390x (gran-endian) [P] | de noche u opcional |
| `msvc` | `slg_selfcheck` imprime el digest; lo ejecuta el propietario en su PC (único sitio con MSVC) | manual antes de cada release |

Con el plan Free los minutos de Actions son finitos [P]: las filas pesadas son nocturnas o manuales.
