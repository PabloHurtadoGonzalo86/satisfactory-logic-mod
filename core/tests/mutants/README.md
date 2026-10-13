# Mutantes de `core/`

Estado: 13-oct-2026. Cada `Mnn.patch` cambia **una línea** de `core/src` (las excepciones son M13 y M74, que tocan dos líneas del
mismo fichero; ver las notas) y se aplica con `patch -p1` desde la raíz del repo. La **primera línea** es
`# kills: <pruebas>` (símbolos `T<n>` y `G<n>`, o `GOLDEN/<nombre>` para un golden concreto); la segunda es una
frase con lo que rompe. `scripts/run_mutants.sh` (`make mutants`) copia el árbol a `build/mutants/`, aplica el parche,
compila y ejecuta **solo** las pruebas nombradas, que deben fallar; un mutante que no aplica o no compila es `ERROR`,
nunca "muerto". Un superviviente es una prueba que falta o un equivalente demostrado: los equivalentes están en
`equivalent.txt` (lo que lee el script) y sus pruebas en [`equivalents.md`](equivalents.md).

## Cómo se ejecuta aquí (VPS compartido)

Un mutante a la vez, con el candado, el `nice`/`ionice` y el guarda de calma (`scripts/guard.sh`), siempre
`SLG_CASES <= 100` (política completa en `core/README.md`):

```sh
flock "$LOCK" nice -n 19 ionice -c3 scripts/guard.sh bash -c 'ulimit -v 1500000; cd /ruta/al/repo && \
  SLG_CASES=100 MUTANT_TIMEOUT=300 MUTANT_KEEP_BASE=1 MUTANT_GUARD=1 JOBS=1 make mutants'
```

`MUTANT_KEEP_BASE=1` conserva `build/mutants/base` entre llamadas (solo se recompila lo que cambió) y
`MUTANT_FULL=1` ejecuta la suite entera sobre un superviviente para decir qué prueba lo habría matado; no cambia el
veredicto. `MUTANT_GUARD=1` hace que el script espere, antes de cada mutante, a que `scripts/guard.sh` vea el host en
calma; si el guarda lo rechaza, sale con 75 tras imprimir lo que ha hecho y se relanza con `MUTANTS="Mnn ..."` para los
que faltan. Un tiempo agotado (`MUTANT_TIMEOUT`) lo cuenta el script como muerto y lo anota en el detalle: con un host cargado, busca `timed out` en la salida antes de fiarte de un muerto.

## Tabla

Última pasada completa de `make mutants` (`SLG_CASES = 100`, 13-oct-2026): 100 mutantes, 96 muertos, 4 equivalentes demostrados (clase A), 0 supervivientes, 0 errores y 0 sin ejecutar.

| Id | Qué rompe | Tamaño | Pruebas nombradas | Último resultado |
|---|---|---|---|---|
| M01 | Actualización en sitio: cada Gate escribe su salida nueva en su red durante la fase C, así que los componentes posteriores del mismo tick la leen (el estado sigue en doble búfer, las redes no) | 1 línea | T1, G5 | muerto por G05_ring2[sim] T01_order_independence[sim] |
| M02 | La fusión de red es una suma saturada min(15, a + b) en lugar de max | 1 línea | T5, G6 | muerto por G06_wired9[sim] T05_component_tables[sim] |
| M03 | La fusión de red es «el último conductor gana» (por id ascendente) | 1 línea | T1, T5 | muerto por T01_order_independence[sim] T05_component_tables[sim] |
| M04 | La veracidad es x > 1 en lugar de x > 0 (entradas de Gate) | 1 línea | T5 | muerto por T05_component_tables[sim] |
| M05 | Delay(d) se comporta como Delay(d-1) para d >= 2 (d+1 no cabe en una línea sin ampliar el estado) | 1 línea | T4, G3 | muerto por G03_and_delay2[sim] T04_quiescence_and_fifo[sim] |
| M06 | Compare usa < en lugar de <= | 1 línea | T5 | muerto por T05_component_tables[sim] |
| M07 | Dominancia del Latch invertida (gana set sobre reset) | 1 línea | G4, T5 | muerto por G04_latch_both[sim] T05_component_tables[sim] |
| M08 | Toggle conmuta por nivel en lugar de por flanco de subida | 1 línea | T5, T14 | muerto por T05_component_tables[sim] T14_theory_bridge[sim] |
| M09 | Toggle nace con p = 0 en lugar de p = 1 | 1 línea | G8 | muerto por G08_toggle_new_high[sim] |
| M10 | Un Toggle se rearma (p <- 1) con cualquier lote que sobrevive, así que cablearlo después no es un flanco | 1 línea | G9, T11 | muerto por G09_toggle_edit_connect[sim] T11_edit_semantics[sim] |
| M11 | El actuador emite X_tau en lugar de X_tau+1 (los mandos se leen del búfer anterior) | 1 línea | G1, T14 | muerto por G01_lat_not[sim] T14_theory_bridge[sim] |
| M12 | decode no restaura nextId (lo deriva como el mayor id + 1) | 1 línea | T10, T11 | muerto por T10_determinism_and_hashes[sim] T11_edit_semantics[sim] |
| M13 | El FIFO de Delay se guarda al revés en encode y se lee al revés en decode (dos líneas: con una sola se rompe la ida y vuelta y ya no es el mutante que describe 03 §7) | 2 puntos | T9, GOLDEN/all_kinds | muerto por GOLDEN/all_kinds T09_differential[sim] |
| M14 | StepPacer pierde el resto (acc = 0 tras cada llamada) | 1 línea | T13, G12 | muerto por G12_pacer[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M15 | El FIFO de Delay se guarda al revés solo en encode (la ida y vuelta se rompe) | 1 línea | T10, T12 | muerto por T10_determinism_and_hashes[sim] T12_blob_robustness[sim] |
| M16 | La identidad de la fusión es 1 (una red sin conductor lee 1) | 1 línea | T5, T9 | muerto por T05_clock_wide_period[sim] T05_component_tables[sim] T09_differential[sim] |
| M17 | Los valores de red no se borran entre ticks (la fusión conserva el máximo anterior) | 1 línea | T5, T9 | muerto por T05_clock_wide_period[sim] T05_component_tables[sim] T09_differential[sim] |
| M18 | Las muestras se saturan en 16 en lugar de 15 | 1 línea | T9, T10, T11 | muerto por T09_differential[sim] |
| M19 | Delay escribe el valor nuevo en la primera ranura en lugar de la última (orden del FIFO) | 1 línea | T4, T5, G3 | muerto por G03_and_delay2[sim] T04_quiescence_and_fifo[sim] T05_component_tables[sim] |
| M20 | Delay desplaza una ranura de menos (longitud del FIFO), con guarda para que d = 1 siga definido | 1 línea | T4, T5 | muerto por T04_quiescence_and_fifo[sim] T05_component_tables[sim] |
| M21 | Subtract no recorta a 0 (el resultado da la vuelta como entero sin signo) | 1 línea | T5 | muerto por T05_component_tables[sim] |
| M22 | Subtract compara a >= b en lugar de a > b (equivalente: con a = b, a - b = 0) | 1 línea | T5 | equivalente demostrado (clase A), ver `equivalents.md` |
| M23 | Xor es «exactamente una entrada veraz» en lugar de paridad impar | 1 línea | T5 | muerto por T05_component_tables[sim] |
| M24 | And compara truthy >= entradas en lugar de == (equivalente: truthy <= entradas) | 1 línea | T5 | equivalente demostrado (clase A), ver `equivalents.md` |
| M25 | validate acepta un Gate de aridad 0 | 1 línea | T5 | muerto por T05_component_tables[sim] |
| M26 | La cota del Delay es exclusiva (longitud >= maxDelay ya es pasarse) | 1 línea | T5, T11 | muerto por T05_component_tables[sim] T11_edit_semantics[sim] T11_param_caps_are_limit_exceeded[sim] |
| M27 | Una longitud de Delay por encima de su tope y del rango del catálogo da BadSpec en vez de LimitExceeded (la lectura antigua de classifyUpper, B2) | 1 línea | T5, T11, T12 | muerto por T05_component_tables[sim] T11_edit_semantics[sim] T11_param_caps_are_limit_exceeded[sim] T12_blob_robustness[sim] T12_decode_check_order[sim] T12_decode_param_caps[sim] |
| M28 | La salida del Clock es alta H - 1 ticks (umbral > en lugar de >=) | 1 línea | T5, G10 | muerto por G10_clock6_3[sim] T05_clock_wide_period[sim] T05_component_tables[sim] |
| M29 | Clock da la vuelta con fase >= P en lugar de == P (equivalente mientras c < P) | 1 línea | T5 | equivalente demostrado (clase A), ver `equivalents.md` |
| M30 | El byte alto de la fase del Clock se guarda desplazado 9 bits (solo se ve con P > 256) | 1 línea | T5, T8 | muerto por T05_clock_wide_period[sim] |
| M31 | Una semilla de Clock mayor que 255 pierde su byte alto al crear el componente | 1 línea | T5, T11 | muerto por T05_clock_wide_period[sim] |
| M32 | decode acepta nextId igual al mayor id | 1 línea | T12 | muerto por T12_blob_robustness[sim] T12_decode_check_order[sim] |
| M33 | decode acepta un id de componente repetido | 1 línea | T12 | muerto por T12_blob_order_invariants[sim] |
| M34 | decode acepta un cable de un pin a sí mismo | 1 línea | T12 | muerto por T12_blob_order_invariants[sim] |
| M35 | decode acepta cables repetidos y descendentes que comparten el pin menor | 1 línea | T12 | muerto por T12_blob_order_invariants[sim] T12_blob_robustness[sim] |
| M36 | decode rechaza un blob con exactamente maxComponents componentes | 1 línea | T12 | muerto por T12_blob_robustness[sim] T12_decode_check_order[sim] |
| M37 | decode rechaza un blob con exactamente maxWires cables | 1 línea | T12 | muerto por T12_blob_robustness[sim] |
| M38 | decode rechaza exactamente maxDelayCells ranuras de Delay | 1 línea | T12 | muerto por T12_blob_robustness[sim] T12_decode_check_order[sim] |
| M39 | decode rechaza una red con exactamente maxNetPins pines | 1 línea | T12 | muerto por T12_blob_robustness[sim] T12_decode_check_order[sim] |
| M40 | apply rechaza una red con exactamente maxNetPins pines | 1 línea | T11 | muerto por T11_edit_semantics[sim] T11_limits_judge_the_final_netlist[sim] |
| M41 | apply rechaza exactamente maxComponents componentes | 1 línea | T11 | muerto por T11_edit_semantics[sim] T11_limits_judge_the_final_netlist[sim] |
| M42 | apply rechaza exactamente maxWires cables | 1 línea | T11 | muerto por T11_edit_semantics[sim] T11_limits_judge_the_final_netlist[sim] |
| M43 | apply rechaza exactamente maxDelayCells ranuras de Delay | 1 línea | T11 | muerto por T11_edit_semantics[sim] |
| M44 | Un add() que halló agotado el espacio de ids da RetiredId al aplicar en vez de LimitExceeded | 1 línea | T11, T2 | muerto por T11_edit_semantics[sim] |
| M45 | EditBatch::add devuelve el id centinela 0xFFFFFFFF en vez de 0 cuando se agota el espacio de ids | 1 línea | T11, T2 | muerto por T11_edit_semantics[sim] |
| M46 | addWithId acepta el id centinela 0xFFFFFFFF | 1 línea | T11, T2 | muerto por T11_edit_semantics[sim] |
| M47 | StepPacer cuenta todo el retraso como descartado en lugar del exceso sobre el tope | 1 línea | T13, G12 | muerto por G12_pacer[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M48 | StepPacer recorta con steps >= max en lugar de > max (equivalente: el exceso es 0) | 1 línea | T13, G12 | equivalente demostrado (clase A), ver `equivalents.md` |
| M49 | StepPacer acumula el tiempo en pausa (ráfaga al reanudar) | 1 línea | T13 | muerto por T13_host_layer[sim] T13_host_policy[sim] |
| M50 | StepPacer fija maxStepsPerCall = 0 a 0 en lugar de 1 | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M51 | Un sensor Level sin lectura nueva alimenta 0 en lugar del último valor bueno | 1 línea | T13 | muerto por T13_driver_settle_exact[sim] T13_host_blob[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M52 | El driver entrega en cada llamada todo mando distinto de 0, no solo los cambios | 1 línea | T13 | muerto por T13_driver_settle_exact[sim] T13_host_blob[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M53 | El tiempo de espera de resolución exige estrictamente más pasos ejecutados que ceil(resolveTimeoutMicros / paso) | 1 línea | T13 | muerto por T13_driver_resolve_boundary[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M54 | Un sensor Pulse repite su lectura en todos los pasos de una llamada | 1 línea | T13, G11 | muerto por G11_burst3[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M55 | decode acepta un valor retenido de un sensor Level por encima de 15 (la comprobación de dominio del blob del driver, D34) | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M56 | La guarda de asentamiento retiene un actuador un paso de más | 1 línea | T13 | muerto por T13_driver_settle_exact[sim] T13_host_blob[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M57 | El lint no informa de un componente que lee una red que él mismo maneja (autolazo): su SCC es el componente más el nodo de esa red, tamaño 2, y size > 2 nunca se cumple | 1 línea | T7 | muerto por T07_lint[sim] |
| M58 | El lint informa de una red con varios conductores solo a partir de tres | 1 línea | T7 | muerto por T07_multidriver_every_pin[sim] |
| M59 | settlingBounds no trata un Toggle como memoria | 1 línea | T3, T7, T13 | muerto por T07_lint[sim] |
| M60 | settlingBounds pesa Delay(d) como d + 1 | 1 línea | T3, T7, T13 | muerto por T07_lint[sim] T13_driver_settle_exact[sim] T13_host_policy[sim] |
| M61 | clampedSamples cuenta dos veces cada muestra saturada | 1 línea | T10, T11 | muerto por T11_edit_semantics[sim] |
| M62 | connect (y disconnect) aceptan un cable de un pin a sí mismo | 1 línea | T11 | muerto por T11_disconnect_judges_pins_like_connect[sim] T11_self_wire_is_bad_pin[sim] |
| M63 | Un lote vacío tomado antes de que nextId avanzara no es StaleBatch | 1 línea | T11 | muerto por T11_empty_batch_stale[sim] |
| M64 | Un actuador creado con salida inicial distinta de 0 se da por confirmado en 0, así que el driver lo escribe al crearlo (02 §7.5) | 1 línea | T13 | muerto por T13_host_blob[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M65 | decode rechaza el blob legal más corto (exactamente cabecera + cola: una netlist vacía) | 1 línea | T12 | muerto por T12_blob_empty_netlist[sim] T12_decode_check_order[sim] |
| M66 | FormatTooNew exige que las dos versiones superen la soportada | 1 línea | T12 | muerto por T12_blob_robustness[sim] |
| M67 | Una versión inferior a la soportada solo se rechaza cuando lo son las dos | 1 línea | T12 | muerto por T12_blob_robustness[sim] |
| M68 | runCall se acepta entre runCall y endCall | 1 línea | T13 | muerto por T13_driver_phase_misuse[sim] |
| M69 | El blob del driver se puede decodificar entre runCall y endCall | 1 línea | T13 | muerto por T13_driver_phase_misuse[sim] |
| M70 | offer no busca un sensor creado a espaldas del driver | 1 línea | T13 | muerto por T13_driver_offer_reconciles[sim] |
| M71 | Un id explícito igual o mayor que el siguiente automático adelanta la cuenta de add() (la regla antigua de EditBatch::add) | 1 línea | T2, T11 | muerto por T02_add_returns_the_assigned_id[sim] T11_edit_semantics[sim] |
| M72 | El tope del periodo del Clock se juzga después del ancho H (H inválida con un periodo por encima del tope da BadSpec) | 1 línea | T5, T11, T12 | muerto por T05_component_tables[sim] T11_edit_semantics[sim] T11_param_caps_are_limit_exceeded[sim] T12_decode_param_caps[sim] |
| M73 | disconnect no juzga sus pines (UnknownWire para todo, la lectura antigua de B4) | 1 línea | T11 | muerto por T11_disconnect_judges_pins_like_connect[sim] T11_edit_semantics[sim] |
| M74 | Un cable se juzga primero el pin b y después el pin a | 1 línea | T11 | muerto por T11_disconnect_judges_pins_like_connect[sim] T11_edit_semantics[sim] |
| M75 | decode juzga el total de ranuras de Delay mientras lee los componentes (antes de las demás comprobaciones del blob) | 1 línea | T12 | muerto por T12_decode_check_order[sim] |
| M76 | decode compara la suma de los dos conteos con los bytes restantes en vez de cada uno por separado | 1 línea | T12 | muerto por T12_decode_check_order[sim] |
| M77 | Una aridad de Gate por encima de su tope y del rango del catálogo da BadSpec en vez de LimitExceeded (la lectura antigua de classifyUpper, B2) | 1 línea | T5, T11, T12 | muerto por T05_component_tables[sim] T11_edit_semantics[sim] T11_param_caps_are_limit_exceeded[sim] T12_blob_robustness[sim] T12_decode_param_caps[sim] |
| M78 | La espera de un sensor sin lectura cuenta llamadas y no los pasos que corre cada una | 1 línea | T13 | muerto por T13_driver_resolve_boundary[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M79 | La espera de un sensor sin lectura se redondea hacia abajo en vez de hacia arriba (floor y no ceil de resolveTimeoutMicros / paso) | 1 línea | T13 | muerto por T13_host_layer[sim] T13_host_policy[sim] |
| M80 | Un sensor sin resolver retiene a todos los actuadores del mundo, no solo a los de cuyo cono forma parte | 1 línea | T13 | muerto por T13_host_layer[sim] T13_host_policy[sim] |
| M81 | Un sensor que se resuelve no arma la retención de los actuadores de su cono | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M82 | La retención se arma en todo actuador con L_a definido, sin mirar las marcas del cono (al resolverse un sensor y tras un apply) | 1 línea | T13 | muerto por T13_host_layer[sim] T13_host_policy[sim] |
| M83 | Un apply arma la retención de todos los actuadores, no solo de los cuyo cono cambió | 1 línea | T13 | muerto por T13_host_layer[sim] T13_host_policy[sim] |
| M84 | Al armar la retención se sobrescribe la que había en vez de quedarse con el máximo | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M85 | La huella de un componente no incluye sus bytes de estado (un replace con el mismo Spec y otro seed no cambia el cono) | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M86 | La huella de un componente no incluye los ids de los conductores de sus entradas (recablear no cambia el cono) | 1 línea | T13 | muerto por T13_host_policy[sim] |
| M87 | Una lectura Level por encima de 15 se retiene sin saturar | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M88 | Una lectura por encima de 15 entra en el Simulator en cada paso de la llamada y se cuenta una vez por paso | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M89 | Una lectura Pulse que no encontró ningún paso (llamada de 0 pasos) se conserva para la llamada siguiente | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M90 | Cambiar el modo de un sensor conserva el valor retenido | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M91 | El decode del driver no compara el tick del Simulator | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M92 | El decode del driver no compara el stateHash del Simulator | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M93 | El decode del driver no compara el structureHash del Simulator | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M94 | El decode del driver no limita la retención guardada | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M95 | Una retención restaurada de un blob frena la entrega aunque la guarda esté desactivada | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M96 | La retención no se descuenta con los pasos que corre cada llamada | 1 línea | T13 | muerto por T13_driver_settle_exact[sim] T13_host_blob[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M97 | El decode del driver descarta las retenciones guardadas | 1 línea | T13 | muerto por T13_host_blob[sim] |
| M98 | El tiempo de espera ignora hostStepMicros y usa siempre el paso por defecto | 1 línea | T13 | muerto por T13_host_layer[sim] |
| M99 | Los sensores sin resolver no retienen ningún actuador | 1 línea | T13 | muerto por T13_driver_resolve_boundary[sim] T13_host_blob[sim] T13_host_layer[sim] T13_host_policy[sim] |
| M100 | La primera lectura de un sensor no lo resuelve (solo la espera lo hace) | 1 línea | T13 | muerto por T13_driver_offer_reconciles[sim] T13_driver_settle_exact[sim] T13_host_blob[sim] T13_host_layer[sim] T13_host_policy[sim] |

## Cómo leer la tabla

- **Pruebas nombradas** es lo que dice la primera línea del parche: el script ejecuta solo esas y exige que alguna
  falle. **Último resultado** dice qué pruebas fallaron de verdad (con `[sim]`: la variante que ejecuta el núcleo; la
  `[ref]` corre el intérprete de referencia y no puede morir por un mutante del núcleo).
- "equivalente o indeterminado" remite a [`equivalents.md`](equivalents.md): clase A (misma función, con prueba) y
  clase B (difieren en algo que el texto deja abierto).
- Todas las cifras son de `SLG_CASES = 100` (el tope de este VPS). Las semillas de caso no dependen del número de
  casos (03 §6: `SplitMix64(base XOR i)`), así que lo que muere con 100 muere también con 2000; lo contrario no vale
  (un superviviente aleatorio a 100 no es una prueba de equivalencia, y ninguno de los de aquí lo es).

## Los 14 de 03 §7 tal como se escribieron

Son parches de una línea salvo M13 (y M74, añadido después, que invierte el orden de dos líneas). Donde el texto de 03 §7 no cabe en el código real, lo que se hizo es:

- **M01** (un solo búfer). Por lectura del código (no se ha ejecutado una variante "un solo `state_`"), en este diseño
  el doble búfer del **estado** no es lo que garantiza la semántica: la fase A
  copia todas las salidas a las redes antes de que la fase C escriba nada, y la fase C solo lee las redes y su
  propia ranura. Lo que rompe el doble búfer observable es dejar que una salida nueva llegue a la red durante la fase
  C, y eso es M01: cada `Gate` escribe su valor nuevo en su red (`nets[…] = …`), de modo que los componentes
  posteriores en el orden por id leen `X_{τ+1}` y no `X_τ`. Mueren T1 (el `Simulator` frente al `ReferenceSim` de
  doble búfer, con órdenes de evaluación distintas) y G5 (`ring2`: sale `(F,0)` en lugar de `(F,F)`).
- **M05** (`Delay` con `d−1` o `d+1`). `d+1` no cabe en una línea sin ampliar el estado; se hizo `d−1`
  (`length = d − 1`, con guarda para `d = 1`).
- **M10** (el `Toggle` se rearma al conectar). Aproximado: cualquier lote que el `Toggle` sobrevive le pone `p ← 1`,
  de modo que cablearlo después ya no es un flanco. Mueren G9 y T11 (los supervivientes conservan su estado bit a bit).
- **M11** (retardo de sensor o actuador que emite `X_τ`). Solo la segunda variante: `commands()` lee el búfer
  anterior. El sensor con un tick de retraso necesitaría estado nuevo y no es de una línea.
- **M13** (FIFO de `Delay` guardado al revés) **tiene dos líneas** (`encode` y `decode`) y es así a propósito: la
  descripción de 03 §7 dice que "la ida y vuelta no lo ve", y eso solo es cierto si `decode` invierte lo mismo que
  `encode`. Con una sola línea (solo `encode`, que es **M15**) la ida y vuelta sí lo ve y mueren T10 y T12. El M13 de
  verdad lo matan el golden `GOLDEN/all_kinds` (compara el `encode()` fresco con `all_kinds.slgc`, byte a byte) y
  T9 (que lee el estado por el codec independiente del test): **no** lo matan G7 (no tiene `Delay`, erratum C1 de
  [`ambiguities.md`](../../../docs/spec/ambiguities.md)) ni T10 ni T12, que solo comparan `encode(decode(b))` con `b`.
  Si 03 §7 dice "T10 y T12 sobre el blob `all_kinds`", vale para M15, no para el mutante simétrico.
- **M12** (`nextId` no se guarda). `decode` lo deriva como `id mayor + 1`. Lo ve T10 (el blob de `all_kinds` tiene
  `nextId = 16` con 14 componentes) y T11 (`RetiredId` tras cargar).

## Mutantes añadidos (M15 en adelante)

Elegidos contra reglas que los 14 no cubren: identidad y tope de la fusión (M16 a M18), orden y longitud del FIFO de
`Delay` (M19, M20), recorte de `Subtract` (M21, M22), paridad de `Xor` y cotas de aridad (M23 a M27), fase y `H` del
`Clock` (M28 a M31), reencode canónico (M15, M32 a M35, M65 a M67), límites de `decode` (M36 a M39) y de `apply` (M40 a
M43), agotamiento de `nextId` y cuenta de `add()` (M44 a M46, M71), clasificación de `Status` de B2, B4 y B8 (M26, M27, M72 a M77), `StepPacer` (M47 a M50), `FrameDriver` (M51 a M56, M64, M68 a M70, M78 a M100),
lint (M57 a M60), contadores y lotes (M61 a M63).

- **M26, M27, M72, M77** (B2, decisión D28). Con la regla resuelta, un parámetro por encima de su tope es siempre
  `LimitExceeded`. M26 hace exclusivo el tope del `Delay`; M27 y M77 reponen la lectura antigua de `classifyUpper`
  (por encima del tope **y** del rango del catálogo da `BadSpec`) para el `Delay` y para la aridad; M72 juzga el tope
  del periodo del `Clock` después de su ancho `H`. Los matan T5 (rejilla de 3 juegos de límites contra
  `checkSpec`), T11 (`T11_param_caps_are_limit_exceeded`, por `addWithId`, `add()` y `replace`) y T12
  (`T12_decode_param_caps`). Hasta D28 el parche de M27 era el contrario (la lectura del oráculo contra el núcleo).
- **M73, M74** (B4, D28). M73 deja que `disconnect` conteste `UnknownWire` a todo (la lectura antigua) y M74 juzga el pin
  `b` antes que el `a`; los matan `T11_disconnect_judges_pins_like_connect` y las fallas aleatorias de T11
  (`DisconnectUnknownComponent`, `DisconnectBadPin`, `WireOrder`). M62 ahora cambia la comprobación común de
  `connect` y `disconnect` (un cable de un pin a sí mismo es `BadPin` para los dos).
- **M75, M76** (B8, D29). M75 juzga el total de ranuras de `Delay` mientras lee los componentes y M76 compara la
  suma de los dos conteos con los bytes restantes; los mata `T12_decode_check_order`.
- **M44, M45, M46, M71** (D30). M45 ya no es indeterminado: `EditBatch::add` devuelve 0 con los ids agotados y
  `T11_edit_semantics` y `T02_add_returns_the_assigned_id` comprueban el valor devuelto (`lastBatchIdsMatched()`). M71 repone
  la regla antigua (un id explícito adelanta la cuenta de `add()`); M44 cambia el `Status` que da el lote al aplicarse.
- **M53, M55** (D31, D34). M53 ya no mide microsegundos: exige estrictamente más pasos ejecutados que
  `⌈resolveTimeoutMicros / paso⌉`. M55 ya no escribe el valor retenido de un `Pulse` (por construcción vale 0): quita
  del `decode` del driver la comprobación de que un `Level` retenido es ≤ 15.
- **M78 a M100** (D31 a D35). Son los mutantes con los que se probó la política nueva del `FrameDriver` en un arnés
  desechable (cada uno con la suite T13/G11/G12) pasados a parches: espera por llamadas y no por pasos (M78), por
  defecto y no por exceso (M79), `hostStepMicros` ignorado (M98), retención de todos los actuadores en vez de los del
  cono (M80, M82, M83, M99), retención que no se arma o no se descuenta (M81, M96), sobrescrita en vez de máximo (M84),
  huella sin estado o sin conductores (M85, M86), saturación y cuenta de `clampedSamples()` (M87, M88), pulso que
  sobrevive a una llamada de 0 pasos (M89), modo que conserva el valor retenido (M90), `decode` que no compara `tick`,
  `stateHash` ni `structureHash` o no acota ni conserva la retención (M91 a M94, M97), retención restaurada que frena
  sin guarda (M95) y primera lectura que no resuelve (M100).
- **M22**, **M24**, **M29**, **M48**: sin prueba que los mate, ver [`equivalents.md`](equivalents.md).

## Pruebas añadidas para matar supervivientes (`core/tests/kills_survivors.cpp`)

La suite original (76 pruebas) dejó vivos estos mutantes: `MUTANT_FULL=1` ejecutó las 76 sobre cada uno y ninguna
falló (excepción: M65, que el T12 nombrado no veía y mataban de rebote T1, T2, T4, T8 y T11 al no poder leer el blob
de una red vacía). Cada prueba nueva lleva el nombre de la propiedad que extiende, de modo que el símbolo `T<n>` de la
cabecera del parche la incluye.

| Prueba | Regla en la que se apoya | Mata |
|---|---|---|
| `T05_clock_wide_period` | 01 §3 y 02 §6.1: `c` es un `u16`; con `P > 256` hacen falta los dos bytes, la semilla puede pasar de 255 y la vuelta es a `P` | M30, M31 |
| `T12_blob_order_invariants` | 02 §6.3: ids ascendentes (sin repetir), cables normalizados (`a < b`), ascendentes y sin repetir | M33, M34 |
| `T12_blob_empty_netlist` | 02 §6.2 y §6.3: el blob mínimo (32 + 8 bytes, sin componentes) es válido y se reencoda igual | M65 |
| `T11_self_wire_is_bad_pin` | `ambiguities.md` B4: un cable de un pin consigo mismo es `BadPin`, el lote entero se rechaza | M62 |
| `T11_empty_batch_stale` | 02 §4: un lote con otra base da `StaleBatch`, también el vacío | M63 |
| `T07_multidriver_every_pin` | `lint.hpp` y `ambiguities-cpp.md` A7: una red con varios conductores sale con **todos** sus pines, en orden | M58 |
| `T13_driver_resolve_boundary` | 02 §7.4 (D31): el sensor sin lectura deja de retener a los actuadores tras exactamente `⌈resolveTimeoutMicros / paso⌉` pasos ejecutados (inclusive) y `elapsedMicros` no cuenta | M53 |
| `T13_driver_settle_exact` | 02 §7.5 y 01 §2.5: la guarda retiene exactamente `L_a` pasos | M56 |
| `T13_driver_phase_misuse` | `ambiguities-cpp.md` A11: `runCall` y `decode` dentro de una llamada dan `WrongPhase` y no cambian nada | M68, M69 |
| `T13_driver_offer_reconciles` | `ambiguities-cpp.md` A11: un sensor creado en el `Simulator` a espaldas del driver se descubre en `offer` y su lectura no se pierde (añade `Driver::simApply` al puente de la suite) | M70 |
| `T02_add_returns_the_assigned_id` | 02 §2 y §4 (D30): `EditBatch::add` devuelve el `nextId` base más las llamadas a `add` anteriores; un id explícito no mueve la cuenta y un choque es `DuplicateId` o `RetiredId` al aplicar | M71 |

Cada una pasa sobre el núcleo sin mutar y sobre la referencia (`[ref]`) cuando la propiedad corre en las dos.

## Hallazgos para quien cierre el texto

1. **03 §7, M13.** "T10 y T12 sobre el blob `all_kinds`" solo mata el mutante asimétrico (M15). El simétrico (el que
   la propia frase describe con "la ida y vuelta no lo ve") lo matan `GOLDEN/all_kinds` y T9. T10/T12 comparan
   `encode(decode(b))` con `b`, que un `decode` invertido deja pasar.
2. **03 §7 nombra `mutants.md`** para los equivalentes; aquí son `equivalents.md` (la prueba) y `equivalent.txt` (lo que
   lee el script). Conviene que el texto diga lo que hay.
3. **B2/B8 frente a `classifyUpper`** (resuelto, D28 y D29): un parámetro por encima de su tope es `LimitExceeded` (en
   `decode` también) y `classifyUpper` desapareció; M27 ya no es la lectura del oráculo sino la antigua.
4. **`EditBatch::add` con `nextId` agotado** (resuelto, D30): devuelve 0 y el lote da `LimitExceeded`; M45 no es ya
   indeterminado, lo mata la comprobación de `lastBatchIdsMatched()` de T11 y T2.
5. **T07 aceptaba cualquier conjunto entre los conductores y todos los pines** de una red con varios conductores, así
   que un informe con lectores en las redes de tres conductores y sin ellos en las de dos pasaba. T07 sigue igual; la
   prueba nueva fija la lectura documentada en `lint.hpp`.
6. **`add()` tras un id explícito, en el mismo lote** (resuelto, D30). 02 §2 dice `id = base nextId + adds so far`: ahora
   el núcleo, el `ReferenceSim` y el oráculo cuentan solo las llamadas a `add` (`base + n`), y para
   `{id explícito 1, add()}` con `nextId = 1` los tres contestan `DuplicateId`.
