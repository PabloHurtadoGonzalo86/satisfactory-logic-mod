# 04. Decisiones

Estado: borrador de diseño del 13-oct-2026. Estados: **por defecto, revisable** (se implementa así; cambiarlo exige un `semanticsVersion` o `formatVersion` nuevo) y **abierta, decide el propietario** (hay valor por defecto, pero necesita su confirmación). Detalle en [01](01-semantica.md), [02](02-api-nucleo.md) y [03](03-pruebas.md).

## 1. Tabla de decisiones

| Id | Decisión | Estado | Motivo | Alternativas | Riesgo |
|---|---|---|---|---|---|
| D01 | Fusión de red = `max` (01 §4) | por defecto, revisable | idempotente, sin sumador oculto; el único precedente verificado es aditivo (FA1) | suma saturada; fusión por red | la red no cuenta; falta `Sum` |
| D02 | Sin retardo cero; ciclos legales; Tarjan solo lint (01 §5) | por defecto, revisable | `F` definida sin comprobar ciclos (PR1, PR4) | retardo cero con regla de Lustre (TH6); rechazar ciclos | los lazos por fusión sorprenden |
| D03 | Sensor = fuente de retardo cero, actuador registrado; `W` filas = `W−1` llamadas (01 §2.5) | por defecto, revisable | las tablas de theory valen sin desplazar filas | sensor registrado (P1, +1 frente a theory); ambos registrados (P2) | el actuador añade un tick visible |
| D04 | `Latch` nativo de reset dominante | abierta, decide el propietario (defecto: gana R) | el de puertas oscila desde S=R=1 (theory/04 Paso 2) | set dominante; parámetro | quien espere "gana S" |
| D05 | `Toggle`: `p←1` al crear; conectar es un flanco; sin rearme (01 §6.2) | por defecto, revisable | local; igual que `check_C.py` | rearme al conectar (P2); `p←0` (P3) | cablear tarde a una red alta lo conmuta |
| D06 | `Clock(P,H)` nativo, fase = seed | por defecto, revisable | cualquier periodo y ancho (theory/06) | solo anillos | — |
| D07 | 9 kinds; fuera aritmética, flip-flops con reloj, contadores, `Node`, `DLatch` | por defecto, revisable | superficie mínima que cubre el encargo | clase por puerta; ALU | decidir `Split`/`Join` antes de v1 |
| D08 | Seed explícito y canónico; reposo por defecto; lo cargado verbatim (01 §6.1) | por defecto, revisable | un dato, sin reglas ocultas | ceros (P3); punto fijo al insertar | transitorio de construcción (D16) |
| D09 | Nibble, veracidad `x > 0`, verdadero canónico 15 | abierta, decide el propietario | restaura como el repetidor (MC3, PR5); el 0/1 vale | verdadero = 1; puertas bit a bit | quien espere 1 ve 15 |
| D10 | Ids monótonos y no reutilizados; `replace` conserva id y cables (02 §4) | por defecto, revisable | un id retirado no apunta a otro objeto | reutilizar huecos; id derivado del edificio | agotar 2³² da `LimitExceeded` |
| D11 | El id del edificio es una pista y manda el blob; huérfanos con gracia; referencias débiles (02 §4) | por defecto, revisable (spike) | critic "Teardown"; orden de carga sin verificar | id derivado del edificio | gracia de 10 s inventada |
| D12 | Ediciones en lotes atómicos en el límite de tick (02 §5) | por defecto, revisable | `F` sigue siendo función de `(X, e)` | editar dentro del tick | los cambios de vanilla deben encolarse |
| D13 | Paso 100 ms, máximo 4 por llamada, descarte, `hostStepMicros` guardado (02 §7.3) | abierta, decide el propietario (medir) | MC2, C2, C19 | recuperar el retraso; pasos proporcionales a `dt` | 29,9 ticks/s solo con mundo vacío (critic) |
| D14 | Una muestra por llamada, `n` pasos, `Level`/`Pulse`, valor final; `FrameDriver` fuera del núcleo (02 §7.2) | por defecto, revisable | el mundo está congelado dentro de la llamada (C1) | muestrear por paso (imposible) | un pulso breve al mundo es invisible |
| D15 | Actuadores: solo cambios, confirmación persistida, sin escribir al crear ni al cargar (02 §7.5) | por defecto, revisable | C16; setters sin verificar (C3, C5, C23) | reescribir cada tick | discrepancia tras un cambio manual |
| D16 | Guarda de asentamiento tras ediciones, donde `L_a` esté definido | abierta, decide el propietario (defecto: activa) | evita el transitorio de 01 §6.3 | aceptarlo por escrito | no cubre conos con memoria |
| D17 | Binario canónico little-endian, FNV-1a 64, dos versiones, modo congelado; sin secciones opcionales ni migraciones en v0 (02 §6) | por defecto, revisable | bytes iguales = semántica igual; C17 | JSON; propiedad por componente | FNV no es criptográfico |
| D18 | `Status` sin excepciones; límites [P]; fan-in lógico (`maxNetPins`) distinto del físico (adaptador; wires c10, c11) | por defecto, revisable | módulos de Unreal sin excepciones (no verificado) | excepciones; truncar | límites sin medir |
| D19 | Determinismo: enteros, orden por id, PRNG propio, CI proporcional, autotest de MSVC (03 §9) | por defecto, revisable | MSVC solo en el PC del propietario | CI con MSVC | digest manual |
| D20 | Un evaluador, Full; incremental solo tras T9 y benchmark | por defecto, revisable | no hay medidas (C22) | incremental desde el principio | coste de CPU |
| D21 | Teorema de asentamiento por cono, cota superior (01 §8) | por defecto, revisable | PR2 con alcance exacto | afirmarlo con estado | no cubre redes con memoria |
| D22 | Núcleo agnóstico del cableado (pines y cables); el adaptador elige la opción A, B o E (wires) | por defecto, revisable | el spike decide | núcleo que lea el motor | diferenciar circuitos de vanilla cuesta |
| D23 | Texto público: "diseño desde cero" (README, 0001 §4), no "clean-room" | abierta, decide el propietario | critic: no lo puede reclamar quien leyó el código | "clean-room" tal cual | reclamación falsa |
| D24 | Muestra > 15 se satura y se cuenta; marco erróneo da `BadFrame` (I8) | por defecto, revisable | un fallo del adaptador no detiene la lógica | error siempre | un valor erróneo pasa |
| D25 | Oráculo Python por otro agente, mutantes como parches (03 §5, §7) | por defecto, revisable | N-versión | tests del mismo autor | coste de proceso |
| D26 | Formato de texto de los goldens fijado por el oráculo: `ticks n` da `n + 1` filas, valores decimales, huella en 16 hex mayúsculas, las cuatro secciones de la fila siempre (03 §4) | por defecto, revisable | el texto no lo fijaba y `slg_run` y el oráculo se comparan con `cmp` | `n` filas; hex en minúsculas; omitir secciones vacías | regenerar todos los goldens |
| D27 | Un lote se juzga como un todo: ids explícitos contra la base del lote y en cualquier orden; límites sobre la netlist final, salvo `add` con `nextId` agotado (02 §4, §5) | por defecto, revisable | T1 reconstruye insertando en cualquier orden y D12 hace el lote atómico | ids explícitos contra el `nextId` corriente y ascendentes; límites tras cada operación | un lote que rebasa un tope a medias vale si lo repara después |
| D28 | Clasificación del `Status` de una edición (B2, B4). El tope de `Limits` es la única cota superior de un parámetro: `k > maxArity`, `d > maxDelay` y `P > maxClockPeriod` dan siempre `LimitExceeded`, también por encima del rango del catálogo; `BadSpec` queda para lo mal formado. `validate` juzga primero lo mal formado, luego los topes (en el `Clock`: `P < 2` o parámetro no usado, `P` contra el tope y después `H`) y por último el seed. Al decodificar, un parámetro sobre su tope es `LimitExceeded` y cualquier otro inválido, `FormatCorrupt`. `disconnect` juzga sus pines como `connect` (`UnknownComponent`, `BadPin` con `a == b` y la salida del `Actuator`, y solo después `UnknownWire`; el pin `a` entero antes que el `b`). El `Status` de un lote es el de su primera operación que falla y los límites se juzgan tras la última (01 §7, 02 §5, §6.3) | por defecto, revisable | el texto admitía las dos lecturas y rige la del oráculo (ambiguities.md §0) mientras el texto normativo no la contradiga. La regla anterior del C++ (`classifyUpper`) era incoherente: con un solo tope `maxDelay = 4` daba `LimitExceeded` para `d = 5..255` y `BadSpec` desde 256. Quitar un componente y desconectar un cable suyo en el mismo lote recibe el mismo código que con `connect`. La revisión adversarial contó 109 líneas distintas entre el oráculo y el C++, todas por las causas de D28, D29 y D35 y ninguna visible para la suite, porque `ReferenceSim`, T11 y T12 aceptaban cualquiera de los dos estados | `BadSpec` fuera del catálogo (lectura antigua del C++); `UnknownWire` para todo `disconnect`; un `Status` agregado por lote | reclasifica los estados de lotes y blobs inválidos: cambiarlo exige regenerar T11, T12 y el oráculo |
| D29 | Orden de los límites en `decode` (B8). Los conteos de la cabecera se comparan primero con los límites (`LimitExceeded`) y después, cada uno por su cuenta, con los bytes restantes (`componentCount × 11` y `wireCount × 10`, no su suma; `FormatCorrupt`) y la lectura sigue; el primer registro que falla decide. El total de ranuras de `Delay` y después los pines por red se juzgan **al final**, tras ids, `nextId`, cables y la comprobación de que no sobra ningún byte, así que un blob corrupto en cualquier punto da `FormatCorrupt` aunque además rebase uno de esos totales (02 §6.3) | por defecto, revisable | lectura del oráculo: 02 §6.3 reserva `LimitExceeded` para los conteos de la cabecera y los parámetros sobre su tope, y la comprobación por campo es segura en memoria (las reservas siguen acotadas por `restantes / 11` y `restantes / 10`). La comparación de la suma no figuraba en ambiguities.md: la halló la revisión adversarial | juzgar el total de ranuras mientras se leen los componentes; comparar la suma de los dos conteos con los bytes restantes | un blob que rebasa un total y además está corrupto da `FormatCorrupt`, no `LimitExceeded` |
| D30 | Ids de un `EditBatch`. `add` devuelve el `nextId` de la base más el número de llamadas a `add` anteriores del lote; un id explícito (`addWithId`) no mueve esa cuenta. Al aplicar, un id de `add` que choca con uno explícito del mismo lote es `DuplicateId` (`RetiredId` si el explícito se quitó en el lote), con el orden de comprobaciones de `addWithId`. Cuando la base más la cuenta llegaría a `0xFFFFFFFF`, `add` devuelve 0 y el lote da `LimitExceeded` en esa operación. `nextId` final = `max(base, mayor id añadido + 1)` (02 §2, §4) | por defecto, revisable | es el texto literal de 02 §2 (base más las llamadas a `add` anteriores) y de 03 §4 (`nextId` más las altas previas), y hace el id entregado predecible e independiente de los explícitos. El oráculo tomaba el `nextId` corriente (`add_with_id(1)` seguido de `add` valía con los ids 1 y 2) y se corrigió; los goldens no cambian porque usan ids explícitos. Resuelve el hallazgo 3 y M45 de la revisión de mutación (el valor devuelto con los ids agotados ya es 0 y se comprueba) | `add` entrega el siguiente id libre del lote, que sube con los explícitos (lectura del C++ antes del arbitraje); prohibir mezclar `add` y `addWithId` | mezclar las dos formas en un lote puede dar `DuplicateId` |
| D31 | Tiempo de espera de un sensor sin lectura, en pasos ejecutados y no en tiempo: `⌈resolveTimeoutMicros / paso⌉` con `paso = effectiveStepMicros(hostStepMicros)` (0 es el paso por defecto de 100000 µs, no 1 µs). Cada llamada acredita los pasos que corre, `runCall(0, …)` ninguno, y `resolveTimeoutMicros = 0` resuelve todo en la primera llamada. El `elapsedMicros` de `runCall` es informativo y el `FrameDriver` es una función pura de lecturas, pasos y ediciones (02 §7.1, §7.3, §7.4) | por defecto, revisable | con el bucle de 7.1, las tramas de 0 pasos no llegaban al driver y los 5 s se estiraban a 15 y 60 s con tramas de 33 y 8 ms; un solo tirón largo podía acreditar la espera entera. Contar pasos corrige ambas cosas y mantiene válido el registro de reproducción | sumar el `dt` real que el adaptador pasa a `runCall`; retirar `elapsedMicros` de la firma (toca el golden runner, los tests y 02 §2) | la espera se dilata cuando el pacer descarta pasos; `elapsedMicros` queda sin uso hasta que se retire |
| D32 | Un sensor sin resolver retiene solo a los actuadores de cuyo cono forma parte (sus ancestros en el grafo inducido por redes, el que recorre `settlingBounds`); un sensor sin cables o fuera del cono no retiene a nadie. Al resolverse, cada actuador de su cono con `L_a` definido retiene `max(retención, L_a)` pasos a partir de esa llamada; sin `settleGuard` no hay retención (02 §7.4) | por defecto, revisable | un sensor sin cables congelaba todos los actuadores del mundo hasta la espera, y el cono pasaba del 0 por defecto al valor real y ese parpadeo llegaba al mundo. Estrecha D16 sin cerrarla: los conos con memoria o ciclos siguen sin guarda | retener todos los actuadores (lectura literal anterior) | la retención al resolverse cuesta latencia aunque la primera lectura valga lo mismo que el valor por defecto |
| D33 | Guarda de asentamiento tras `apply`: solo los actuadores cuyo cono cambió se retienen, con `max(la retención que ya tenían, L_a)`. El cono cambió si alguno de sus componentes tiene otra huella antes y después (id, `Spec`, bytes de estado y ids de los conductores de cada entrada; un componente nuevo o un conductor quitado cuenta) (02 §7.5) | por defecto, revisable | un flujo de ediciones ajenas retrasaba o dejaba sin salida a todos los actuadores. Los bytes de estado entran porque `replace` con el mismo `Spec` y otro seed reinicia el estado y por tanto el transitorio, con ids, `Spec` y cables iguales. La huella es equivalente a una huella del cono de cada actuador y cuesta O(N + W) por edición | rearmar todos los actuadores sobrescribiendo (lectura anterior); una huella del cono por actuador | la huella lee el estado por `Simulator::encode()`, la única ventana pública, y una edición hecha en el `Simulator` sin pasar por `FrameDriver::apply` no arma la guarda |
| D34 | Blob del driver v1 (sin publicar, cambiado en el sitio): `38 + 6·S + 9·A + 8` bytes, con `tick`, `stateHash` y `structureHash` del `Simulator` emparejado y una retención `u32` por actuador. Decodificación estricta: pareja, `Level` retenido ≤ 15, `Pulse` 0 y retención ≤ `maxComponents + maxDelayCells`. Una lectura sobre 15 se satura donde se consume y suma **una** vez a `clampedSamples()`; cambiar el modo de un sensor olvida su valor retenido. La retención se guarda tal cual y sin guarda se ignora, de modo que `encode(decode(b)) == b` (02 §7.4, §7.5) | por defecto, revisable | emparejar rechaza un `Simulator` y un driver guardados en momentos distintos (el guardado asíncrono de 7.6); `structureHash` va además de `tick` y `stateHash` porque este no incluye los cables. El tope de la retención no es el `L_a` de hoy porque una edición que acorta un cono deja armada la retención anterior y se rechazaría un blob legítimo | atarlo solo por los conjuntos de ids; exigir retención ≤ `L_a`; saturar dentro de `offer()` (el `Simulator` no ofrece API para sumar a `clampedSamples()`) | una lectura consumida por una llamada de 0 pasos no llega a ningún paso y no cuenta |
| D35 | Adoptar un `Simulator` cargado es crear un `FrameDriver` nuevo ligado a él y hacer `decode` del blob del driver (7.6). `runCall(0, …)` retiene las lecturas `Level` (y resuelve su sensor), descarta las `Pulse`, no acredita espera y abre la llamada igualmente. `ActuatorChange::previous` vale `kUnconfirmed` (255, fuera de 0..15) tras `resync()`. `hostStepMicros = 0` es el paso por defecto (`effectiveStepMicros`, `StepPacer::forHostStep`) y un `dt` NaN o negativo es 0 (02 §7.1 a 7.6) | por defecto, revisable | un driver vivo ligado a un `Simulator` sobre el que se asignó el cargado conservaba su sesión (sensores resueltos, confirmaciones). `runCall(0, …)` sigue la lectura del oráculo (B9) y no es literalmente una llamada vacía | `rebind(Simulator&)`; hacer de `runCall(0, …)` un no-op | el adaptador no puede usar `previous` como índice ni compararlo como señal |

## 2. Conflictos entre propuestas

- **Base y injertos**: P1 es la base (ambos jueces la rankean primera). De P3, la convención de sensor, el formato y las huellas byte a byte, el texto de trazas, el oráculo y la CI; de P2, ráfagas, acumulador, retención de sensores, actuadores solo-cambios, mapa de ids, modo congelado y `replace` (su `set_params`). Nuevos aquí: el teorema por cono y la confirmación persistida como entrega pendiente implícita.
- **Resueltos**: sensor registrado (P1) frente a fuente (P3), D03: se elige P3 y se re-derivan teorema, pesos y ejemplos (no se copian los 12.2 y 12.3 de P1). `p←0` de P3, D05: rechazado. Regla de ids de P3, contradictoria: la resuelve `replace`. Estado oculto de P2 (`Pulse`, búfer persistente): pasa a `FrameDriver`.
- **No adoptado o corregido**: `Node`, `DLatch`, secciones opcionales, envenenamiento y `work_budget` (P2); evaluador incremental y mutantes con macros (P3). Se corrigen el `T8` de P1 (`μ ≥ 1`) y el de P3 ("n impar da `2n`"), el ejemplo F de P3 y la cita del flip-flop D (theory/04 Paso 6, no 07).

## 3. Lo que señalaron los jueces

| Aviso | Resolución |
|---|---|
| J1-1, J2-1 contrato de marco y de llamada | 02 §7.1, §7.2; D14; G11 |
| J1-2, J2-2 una convención de latencia, en filas y llamadas | 01 §2.5; D03; G1; "latencia 4" corregido a 3 llamadas (01 §9.2) |
| J1-3, J2-9 formatos, hashes y persistencia | 02 §6; D17; G7 |
| J1-4 flanco del `Toggle` | 01 §6.2; D05; G8, G9; M09, M10 |
| J1-5, J2-7 semántica de ediciones | 02 §4, §5; T11 |
| J1-6 un teorema con alcance y pesos | 01 §8; T3, T4; G13 |
| J1-7 independencia del orden | T1 con control negativo; G5; M01 |
| J1-8 enunciados falsables | T8 (`μ ≥ 0`, anillos de theory/06) |
| J1-9, J2-12 procedencia de las cifras | 01 §0; 03 §1 |
| J1-10 decisiones del propietario | §4 de este documento |
| J1-11 proporción del texto | CI, mutantes y oráculo en 03 como listas; el total son unas 9400 palabras sin contar separadores de tablas, algo por encima de las 9000 pedidas |
| J2-3 `hostStepMicros` persistido, acumulador | 02 §6.2, §6.5, §7.3 |
| J2-4, J2-5 sensores y actuadores | 02 §7.4, §7.5; D15, D16 |
| J2-6 id ↔ edificio, huérfanos, teardown | 02 §4, §7.6; D11 |
| J2-8 estado inicial | 01 §6.1; D08 |
| J2-10 alcance de v0 | 01 §3; D07, D20 |
| J2-11 tests mínimos | T1, T3, T5, T9, T10, T12, T13; 03 §9 |
| J2-13 determinismo y CI proporcionados | 03 §9; D19 |
| J2-14 spike previo | 02 §8 |
| J2-15 "clean-room" | D23 |

## 4. Para revisar por el propietario

Abiertas, con valor por defecto ya aplicado: D04, D09, D13, D16 y D23. Conviene confirmar también D01, D03, D05, D07 y D15, y la política del `FrameDriver` de D31 a D35 (que estrecha D16: la guarda ya solo cubre los conos que cambian o se resuelven). Para el spike en su PC: la lista de 02 §8 y las gracias de 5 s y 10 s del adaptador.

## 5. Resolución de las ambigüedades del oráculo

Estado de los 29 puntos de [ambiguities.md](ambiguities.md) (A1 a A11 formato de texto, B1 a B10 semántica, C1 a C8 errores del texto). Los 29 están resueltos en el texto citado. Cuatro (B2, B4, B8 y B9) eran puntos en los que el oráculo y la implementación C++ leían el texto de forma distinta; se arbitraron el 13-oct-2026 (D28 a D31 y D35) y el texto de 01 a 03 ya no lleva marcadores. [ambiguities.md](ambiguities.md) § 5 recoge qué rige en cada uno y, además, la comparación de bytes por campo de `decode`, el hallazgo 3 de la revisión de mutación (ids de `add`) y M45.

| Id | Resuelta en | Qué fija |
|---|---|---|
| A1 | 03 §4 (contrato, `ticks n`) | `n` pasos, `n + 1` filas |
| A2 | 03 §4 (contrato, fila) | una línea, separador de secciones literal, cuatro secciones siempre |
| A3 | 03 §4 (contrato, valores) | valores en decimal |
| A4 | 03 §4 (contrato, valores) | huella en 16 hexadecimales mayúsculas |
| A5 | 03 §4 (contrato, qué lista cada sección) | componentes y notación de `st`, `in` y `cmd` |
| A6 | 03 §4 (contrato, `in` y `cmd`) | muestra retenida sin saturar; `i` inválida es un error de entrada |
| A7 | 03 §4 (contrato, `c <id> <kind>`) | claves por kind, defectos y `seed` ausente |
| A8 | 03 §4 (contrato, pines) | cualquier orden, el núcleo normaliza |
| A9 | 03 §4 (contrato, líneas `e`) | una operación por línea, un lote atómico por `τ`, id explícito en `add c` |
| A10 | 03 §4 (contrato, netlist inicial); 02 §4 | `nextId = max(id) + 1` tras cargar |
| A11 | 03 §4 (contrato, ficheros y blobs) | sin comentarios; `hostStepMicros` y `tick` son argumentos de la herramienta |
| B1 | 01 §6.1 (tabla de seeds) | `Const` y `Sensor` solo admiten seed 0 |
| B2 | 01 §7; 02 §5, §6.3; D28 | el tope de `Limits` es la única cota superior: sobre él, `LimitExceeded`; `BadSpec` solo para lo mal formado; orden mal formado, topes, seed |
| B3 | 02 §4 (ids explícitos); D27, D30 | ids contra la base del lote, `DuplicateId` frente a `RetiredId`, `nextId` tras el lote; `add` entrega la base más las llamadas a `add` anteriores |
| B4 | 02 §5; D28 | primera operación que falla; `disconnect` juzga sus pines como `connect` (pin `a` entero antes que `b`) |
| B5 | 02 §5; D27 | límites sobre la netlist final; excepción de `add` con `nextId` agotado |
| B6 | 01 §6.1; 02 §4; 03 §4 (contrato, `c <id> <kind>`) | `replace` sin seed usa el reposo del `Spec` nuevo; `Toggle` vuelve a `p = 1` |
| B7 | 02 §6.3; 03 §2 (T12) | `nextId` ≥ 1, ids ≤ `0xFFFFFFFE`, versiones ≥ 1 |
| B8 | 02 §6.3; D29 | límites de la cabecera, bytes por campo, parámetro sobre su tope, y los totales de `Delay` y de pines por red al final |
| B9 | 02 §7.2, §7.4; D35 | un `Pulse` vale la lectura solo en el primer paso de la llamada que la recibe; `runCall(0, …)` descarta las `Pulse` |
| B10 | 01 §2.4; 03 §4 (contrato, `in` y `cmd`) | `cmd` de la fila `τ` es `o` en `X_τ` |
| C1 | 03 §4 (tabla, fila `all_kinds`); 03 §7 (M13) | M13 lo mata el blob `all_kinds`; G7 no tiene `Delay` |
| C2 | 03 §4 (tabla, G14) | `mergeloop` y `mergeloop_e9` |
| C3 | 01 §5 | el mando de `C` es 15 en `X1` (lo lee en `X0`); la red ya vale 9 |
| C4 | 03 §4 (contrato, anfitrión; tabla, G11 y G12) | formatos `lburst` y `lpacer` en `core/golden/README.md` |
| C5 | 02 §6.5 | goldens planos en `core/golden/` hasta `semanticsVersion = 2` |
| C6 | 03 §4 (tabla, G7) | `lat_not.slgc` |
| C7 | 03 §4 (tabla, G10) | el actuador de `clock6_3` muestra la onda un tick después |
| C8 | 03 §5 | el oráculo parte de 01, 02 y 03 |
