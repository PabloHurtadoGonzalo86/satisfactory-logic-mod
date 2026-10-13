# 02. API del núcleo y contrato con el adaptador

Estado: borrador de diseño del 13-oct-2026. Todo es **[P]** salvo lo que cita la investigación: `C<n>` es de `runtime-api`; `PR`, `MC`, `FA`, `TH`, `OT` y `EN`, de `redstone-and-logic-theory`; `c<n>`, de `wires-and-connections`. Semántica en [01](01-semantica.md), pruebas en [03](03-pruebas.md), decisiones en [04](04-decisiones.md).

## 1. Capas

- `core/`, espacio `slg`: `Netlist`, `Simulator`, lint, codec y huellas. C++20 sin tipos de Unreal, sin excepciones ni RTTI, sin `float`, sin reloj, sin globales mutables.
- `core/src/host/` y `core/include/slg/host/`, espacio `slg::host`: `StepPacer` y `FrameDriver`, la política entera de una llamada del anfitrión. Depende de `slg`, nunca al revés: **ninguna API del núcleo conoce "llamada"**. Se prueba en Linux. `slg/host/cone_graph.hpp` (`slg::host::detail`) es un detalle del `FrameDriver` que solo es público para que este lo guarde por valor: el adaptador no lo usa.
- `mod/` (Unreal, pendiente): el adaptador; aquí solo su contrato (§8).
- Un solo evaluador, **Full**; uno incremental no entra hasta pasar T9 y justificarse con el benchmark (D20). Sin promesa de camino caliente sin asignaciones hasta medir (runtime-api C22).

## 2. Cabeceras

```cpp
// slg/types.hpp
namespace slg {
using Signal = std::uint8_t;        // invariant 0..15
using ComponentId = std::uint32_t;  // 1..0xFFFFFFFE, never reused; F never reads it
using Tick = std::uint64_t;
inline constexpr std::uint8_t kOut = 0xFF;                // index of the output pin
enum class Status : std::uint8_t { Ok, BadSpec, BadPin, UnknownComponent, DuplicateId, RetiredId,
  UnknownWire, DuplicateWire, StaleBatch, LimitExceeded, BadFrame, WrongPhase, FormatCorrupt, FormatTooNew };
enum class Kind : std::uint8_t { Const = 1, Sensor, Actuator, Gate, Delay, Compare, Latch, Toggle, Clock };
enum class GateOp : std::uint8_t { And, Or, Xor };
enum class CompareMode : std::uint8_t { Compare, Subtract };
struct Limits {                                           // provisional (01 section 7)
  std::uint32_t maxComponents = 8192, maxWires = 32768, maxNetPins = 256, maxDelayCells = 65536;
  std::uint16_t maxDelay = 255, maxClockPeriod = 4096;  std::uint8_t maxArity = 8;
};
struct Spec { Kind kind = Kind::Const;                    // meaning of p0..p2: section 6.1
              std::uint16_t p0 = 0, p1 = 0, p2 = 0;       // unused ones must be 0
              friend bool operator==(const Spec&, const Spec&) = default; };
// one factory per kind: makeConst(v), makeSensor(), makeActuator(), makeNot(), makeGate(op, invert, arity),
// makeDelay(length), makeCompare(mode), makeLatch(), makeToggle(), makeClock(period, high)
std::uint16_t restSeed(const Spec&) noexcept;             // 01 section 6.1, last column
Status validate(const Spec&, std::uint16_t seed, const Limits&) noexcept;

// slg/netlist.hpp
struct Pin { ComponentId component = 0; std::uint8_t index = 0;        // input number, or kOut
             friend auto operator<=>(const Pin&, const Pin&) = default; };
struct Wire { Pin a, b; };                                // stored normalized (a < b), sorted, unique
class EditBatch {                                         // applied in order, all-or-nothing
 public:
  ComponentId add(Spec, std::uint16_t seed);              // id = base nextId + add() calls so far (§4); 0 when exhausted
  void addWithId(ComponentId, Spec, std::uint16_t seed);  // explicit id >= nextId (decode, tests)
  void remove(ComponentId);                               // also its wires; the id is retired
  void replace(ComponentId, Spec, std::uint16_t seed);    // same id and wires; state reset to seed
  void connect(Pin, Pin);  void disconnect(Pin, Pin);
};
class Netlist {                                           // structure only, no state
 public:
  Status apply(const EditBatch&);                         // all-or-nothing (seeds ignored)
  std::span<const std::pair<ComponentId, Spec>> components() const noexcept;   // ascending id
  std::span<const Wire> wires() const noexcept;
  ComponentId nextId() const noexcept;  std::uint64_t structureHash() const noexcept;
};

// slg/simulator.hpp
class Simulator {                                         // single thread
 public:
  explicit Simulator(Limits = {}, std::uint32_t hostStepMicros = 0);   // empty, tick 0
  EditBatch newBatch() const;                             // captures nextId
  Status apply(const EditBatch&);                         // only between ticks
  Status step(std::span<const Signal> samples);           // ONE tick; samples[i] feeds sensorIds()[i]
  const Netlist& netlist() const noexcept;  Tick tick() const noexcept;
  std::uint32_t hostStepMicros() const noexcept;          // opaque to F; saved with the world
  std::span<const ComponentId> sensorIds() const noexcept;     // ascending
  std::span<const ComponentId> actuatorIds() const noexcept;   // ascending
  std::span<const Signal> commands() const noexcept;      // o of each actuator in X_tick
  std::uint32_t clampedSamples() const noexcept;          // diagnostic; in no hash
  std::uint64_t stateHash() const noexcept;
  std::vector<std::uint8_t> encode() const;
  static Status decode(std::span<const std::uint8_t>, const Limits&, Simulator& out);
};

// slg/lint.hpp
struct FeedbackReport { std::vector<std::vector<ComponentId>> groups;   // SCCs, ascending ids
                       std::vector<Pin> undrivenInputs;                // read 0
                       std::vector<std::vector<Pin>> multiDriverNets; };  // merged by max
FeedbackReport analyze(const Netlist&);                   // iterative Tarjan + self-edges (01 section 5)
std::vector<std::optional<std::uint32_t>> settlingBounds(const Netlist&);   // L_a per actuator

// slg/host/*.hpp -- namespace slg::host
inline constexpr std::uint32_t kDefaultStepMicros = 100'000, kDefaultMaxStepsPerCall = 4;   // 7.3
constexpr std::uint32_t effectiveStepMicros(std::uint32_t hostStepMicros) noexcept;         // 0 = unset -> default
class StepPacer {                                         // integer microseconds
 public:
  StepPacer(std::uint32_t stepMicros, std::uint32_t maxStepsPerCall) noexcept;  // clamped to >= 1
  static StepPacer forHostStep(std::uint32_t hostStepMicros,
                               std::uint32_t maxStepsPerCall = kDefaultMaxStepsPerCall) noexcept;  // 7.3
  std::uint32_t advance(std::uint32_t elapsedMicros, bool paused) noexcept;     // steps to run now
  std::uint64_t droppedSteps() const noexcept;  std::uint64_t pausedMicros() const noexcept;
  std::uint64_t accumulatedMicros() const noexcept;     // acc, 0 <= acc < stepMicros
  std::uint32_t stepMicros() const noexcept;  std::uint32_t maxStepsPerCall() const noexcept;
};
enum class SensorMode : std::uint8_t { Level, Pulse };
inline constexpr Signal kUnconfirmed = 0xFF;              // ActuatorChange::previous after resync(); outside 0..15
struct ActuatorChange { ComponentId id; Signal previous; Signal value; };   // previous = last acknowledged
struct DriverOptions { std::uint32_t resolveTimeoutMicros = 5'000'000;      // counted in executed steps (7.4)
                       bool settleGuard = true; };
class FrameDriver {
 public:
  explicit FrameDriver(Simulator&, DriverOptions = {});
  Status apply(const EditBatch&);                         // WrongPhase between runCall and endCall
  void setSensorMode(ComponentId, SensorMode);
  void resync();                                          // forget acknowledged values
  void offer(ComponentId sensor, Signal reading);         // only for successful reads
  Status runCall(std::uint32_t steps, std::uint32_t elapsedMicros);   // ONE sample, n steps; elapsedMicros is informational (7.4)
  std::span<const ActuatorChange> changes() const noexcept;           // final values only
  void acknowledge(ComponentId actuator);                 // the world accepted the write
  void endCall();
  std::vector<std::uint8_t> encode() const;  Status decode(std::span<const std::uint8_t>);  // 7.5
};
```

`fullness(num, den)` (en `slg::host`, enteros: 0 si `num ≤ 0` o `den ≤ 0`, si no `min(15, 1 + 14·num/den)`) es el comparador de Minecraft, `floor(1 + llenado × 14)` con vacío = 0 (redstone MC5, PR6), en enteros: 50 de 200 → `14·50/200 = 3`, resultado 4; 200 de 200 → resultado 15; 1 de 200 → `14/200 = 0`, resultado 1. Vanilla no ofrece "IsFull" (runtime-api C9).

## 3. Invariantes

- **I1** Toda señal observable está en 0..15; `q` de `Gate`, `Latch` y `Toggle` en {0,15}; `p` en {0,1}; ranuras de `Delay` ≤ 15; `c` de `Clock` < P.
- **I2** `step` es función pura de (estado, muestras): no lee ids, orden de inserción ni reloj.
- **I3** Un `Status` ≠ `Ok` deja netlist, estado, tick, `nextId`, huellas y contadores intactos.
- **I4** Todo orden observable (componentes, sensores, actuadores, huellas, blob) es por id ascendente.
- **I5** `nextId` es mayor que todo id que haya existido; un id retirado no se reutiliza.
- **I6** `decode(encode(S))` restaura `S` y `encode(decode(b)) == b` para todo `b` aceptado (una sola codificación por netlist con estado; netlists de comportamiento igual pueden diferir en bytes).
- **I7** Un solo hilo, el del juego (runtime-api C1, C13).
- **I8** Una muestra > 15 se satura a 15 y suma a `clampedSamples()`; un marco con longitud ≠ nº de sensores da `BadFrame` sin cambios.

## 4. Ids y edificios

- El núcleo asigna ids monótonos desde `nextId`; un lote con otra base da `StaleBatch`. `EditBatch::add` devuelve el `nextId` base más el número de llamadas a `add` anteriores del lote: un id explícito (`addWithId`) no mueve esa cuenta. Cuando ese id llegaría a `0xFFFFFFFF`, `add` devuelve 0 y el lote da `LimitExceeded` al aplicarse. `remove` retira el id (`remove` + `add` del mismo id es `RetiredId`). Cambiar un parámetro es `replace`: conserva id y cables (si un pin cableado deja de existir, el lote falla con `BadPin`) y reinicia el estado al seed (sin seed explícito, el de reposo del `Spec` **nuevo**; un `Toggle` vuelve a `p = 1`), así no se redimensionan FIFOs con pulsos en vuelo.
- **Ids explícitos** (`addWithId`: decodificación, `lnet`, reconstrucciones y tests; el `>= nextId` del comentario de §2 se entiende contra la base del lote). Se juzgan contra el `nextId` del **inicio del lote** (`EditBatch::base`), no contra el que va quedando, y pueden venir en cualquier orden dentro del lote (T1 reconstruye "insertando en cualquier orden con los mismos ids"). Un id presente en la netlist o añadido antes en el mismo lote es `DuplicateId`; si no, uno menor que la base y ausente (el 0 incluido), o quitado en el mismo lote, es `RetiredId`; `0xFFFFFFFF` o más es `LimitExceeded`; ese es el orden de las comprobaciones, y tras ellas se juzga el `Spec` (01 §7). El id que devolvió un `add` se juzga igual: si choca con uno explícito del mismo lote es `DuplicateId` al aplicar (`RetiredId` si el explícito se quitó en ese lote), así que `addWithId(1, …)` seguido de `add(…)` con `nextId = 1` falla. Tras el lote, `nextId = max(base, mayor id añadido + 1)`, contando los ids añadidos aunque se quiten en el mismo lote (I5).
- **Adaptador [P]**: cada edificio lógico guarda un `uint32 LogicComponentId` como propiedad SaveGame (runtime-api C17); es una **pista** y manda el blob. La tabla `id → edificio` vive en memoria con referencias débiles (ningún puntero crudo entre cargas, critic "Teardown").
- **Tras cargar**: un edificio con un id desconocido o ya reclamado (un blueprint pegado duplica la propiedad) pide un componente nuevo. Un componente sin edificio es un huérfano y **no se borra de inmediato** (el orden de carga y `BeginPlay` no está verificado): se borra tras una gracia en tiempo del adaptador (suma de dt; 10 s [P]), no en pasos del núcleo, que en pausa no avanzan.
- Los enlaces a objetos vanilla son del adaptador, con su propio entero de versión; los ids y punteros de circuito de vanilla no se cachean (runtime-api C11). La clave estable del edificio queda para el spike.

## 5. Ediciones en vivo

Un `EditBatch` se valida entero sobre una copia y solo si todas sus operaciones valen sustituye al original. Las operaciones valen en orden (`connect` y luego `remove` del mismo componente vale; al revés, no). El `Status` de un lote inválido es el de la **primera** operación que falla, en el orden del lote. Dentro de una operación con varios defectos se juzga en este orden: componente inexistente (`UnknownComponent`), pin inexistente (`BadPin`) y, por último, `DuplicateWire` (`connect`) o `UnknownWire` (`disconnect`). `connect` y `disconnect` juzgan igual sus pines: primero el pin `a` entero (su componente y luego el pin), después el `b`, y un cable de un pin consigo mismo es `BadPin`. Un cable entre dos entradas o entre dos salidas es legal (una red sin conductores vale 0; varios se fusionan por `max`) y el `Actuator` no tiene pin de salida, así que `n.o` es `BadPin`. Por eso quitar un componente y desconectar en el mismo lote un cable suyo da `UnknownComponent` y no `UnknownWire`. En `add` y `replace`, `validate` (01 §7) juzga lo mal formado, luego los topes y luego el seed, y en `replace` el `BadPin` por un pin cableado que deja de existir va después de `validate`. Los límites de componentes, cables, ranuras de `Delay` y pines por red (01 §7) se juzgan sobre la netlist **final** del lote, no tras cada operación: un lote que rebasa un tope y lo repara con una operación posterior vale; `maxNetPins` cuenta los pines distintos de una red que aparecen en algún cable. Una operación que falla gana siempre a un límite, porque los límites se juzgan tras la última. Única excepción: un `add` que halla agotados los ids da `LimitExceeded` en el momento, en su lugar del lote (§4). Los supervivientes conservan su estado bit a bit (T11); `add` y `replace` nacen en su seed; `connect` y `disconnect` unen o dividen redes con efecto en el siguiente tick; `remove` saca su salida de las redes sin fantasma; no hay rearme de flancos (01 §6.2).

Solo en el **límite de tick**, nunca dentro de `step`; un lote se registra como `(τ, lote)`. Si el adaptador deriva las redes de los circuitos de vanilla (wires, opción A), solo puede entregar cambios entre llamadas: el orden en que el motor los reconstruye decide en qué llamada llega el lote y nunca entra en `F` (critic, "Architecture source of truth").

## 6. Serialización

### 6.1 Parámetros y estado por kind

Los códigos de kind (enum de §2) no se renumeran ni reutilizan. `p0, p1, p2`: `Const` valor; `Gate` `op` (And 0, Or 1, Xor 2), `inv` 0/1 y arity; `Delay` `d`; `Compare` modo (Compare 0, Subtract 1); `Clock` `P` y `H`; el resto, 0. Bytes de estado: `Const` y `Sensor` 0; `Actuator`, `Gate`, `Compare` y `Latch` 1; `Toggle` 2 (`q`, `p`); `Clock` 2 (`c`, u16); `Delay` `d`, con `f1` (el más antiguo) primero.

### 6.2 Formato v1 (little-endian, sin flotantes, punteros ni relleno)

| Campo | Bytes | Contenido |
|---|---|---|
| cabecera | 32 | magia `53 4C 47 43` ("SLGC"); `u16 formatVersion = 1`; `u16 semanticsVersion = 1`; `u32 hostStepMicros` (opaca para `F`, 0 = sin fijar); `u64 tick`; `u32 nextId`; `u32 componentCount`; `u32 wireCount` |
| componentes, por id ascendente | 11 + estado | `u32 id`, `u8 kind`, `u16 p0, p1, p2`, estado de §6.1 |
| cables, normalizados, ascendentes, sin duplicados | 10 | `u32 id`, `u8 index` del pin menor y lo mismo del mayor (`kOut` = 255) |
| cola | 8 | FNV-1a de 64 bits de todos los bytes anteriores |

FNV-1a de 64 bits: base `0xcbf29ce484222325`, primo `0x100000001b3` (no criptográfico). **`stateHash`**: FNV-1a del flujo `u32 id, u8 kind, estado` por componente en id ascendente, sin `tick`, `nextId` ni `hostStepMicros`. **`structureHash`**: FNV-1a de `u32 componentCount, u32 wireCount`, los registros de componente sin estado y los cables.

### 6.3 Decodificación estricta y total

`decode` construye un `Simulator` nuevo y el adaptador lo adopta solo con `Ok`. Orden: magia; versiones (mayor que la soportada: `FormatTooNew` sin mirar más; menor que 1 en cualquiera de los dos campos: `FormatCorrupt`, porque no hay migración desde v0, §6.5); cola; lectura secuencial con los bytes restantes comprobados en cada campo y los conteos ≤ límites **y** ≤ bytes restantes antes de reservar memoria; ids ascendentes y ≤ `0xFFFFFFFE`; `nextId` ≥ 1 y mayor que todos los ids (el vacío válido tiene `nextId = 1`); parámetros válidos (los no usados a 0); estados en su dominio; cables normalizados, ascendentes y con ambos pines existentes; ni un byte de más. Un fallo es `FormatCorrupt` (`LimitExceeded` si un conteo excede un límite). Los conteos de la cabecera se comparan primero con los límites (`LimitExceeded`) y después con los bytes restantes (`FormatCorrupt`), cada conteo por su cuenta (`componentCount` × 11 y `wireCount` × 10 frente a los bytes restantes, no su suma) y la lectura sigue después. Los registros se juzgan en orden y el primero que falla decide: un parámetro por encima de su tope (01 §7) da `LimitExceeded` en cuanto se lee el registro, antes de su estado, y cualquier otro parámetro inválido, `FormatCorrupt`. Los dos límites de la netlist entera, el total de ranuras de `Delay` y los pines por red (en ese orden), se juzgan **al final**, tras ids, `nextId`, cables y «ni un byte de más», de modo que un blob corrupto en cualquier punto da `FormatCorrupt` aunque además rebase uno de ellos. Sustituir un solo byte siempre cambia la cola (cada paso de FNV-1a es biyectivo).

### 6.4 Ejemplo dorado G1 y G7

Sensor 1 → NOT 2 (`Gate(Or, inv, 1)`, seed 15) → actuador 3; cables `out(1)–in(2,0)` y `out(2)–in(3,0)`. `e = 0` para `τ < 2` y `15` desde `τ = 2`. Cada fila sale de la anterior con `N' = NOT(e)` y `A' = N`.

| τ | e | N | A | `stateHash(X_τ)` |
|---|---|---|---|---|
| 0 | 0 | F | 0 | `9D16793581913B31` |
| 1 | 0 | F | F | `9D16723581912F4C` |
| 2 | F | F | F | `9D16723581912F4C` |
| 3 | F | 0 | F | `4622C4783DE15BB9` |
| 4 | F | 0 | 0 | `4622CD783DE16B04` |

La muestra que cambia en `τ = 2` baja el actuador en la fila 4: `W = 2` (01 §2.5). El flujo de la fila 0 son 17 bytes: `01 00 00 00 02` (sensor), `02 00 00 00 04 0F` (NOT, `s = 15`), `03 00 00 00 03 00` (actuador, `o = 0`); las filas 1 y 2 tienen el mismo estado y por tanto la misma huella. El blob G7, con `hostStepMicros = 100000` (`A0 86 01 00`) y `tick = 0`, mide 95 bytes: cabecera 32 (`nextId = 4`, 3 componentes, 2 cables) + componentes `11 + 12 + 12` + cables `2 × 10` + cola 8. El registro del NOT es `02 00 00 00 04 01 00 01 00 01 00 0F`. La cola es `0x3D144EE313AE954F` y `structureHash` vale `0x84C3FB7A3ACB833C`; T10 y T12 los fijan.

### 6.5 Versionado

- `formatVersion` cambia el contenedor; `semanticsVersion` cambia `F` (toda transición, fusión o desempate modificado lo incrementa aunque los bytes no cambien); las trazas doradas viven por `semanticsVersion`: con la 1 son planas en `core/golden/`; cuando aparezca la 2, pasan a `core/golden/v<semanticsVersion>/` (los de la 1 a `core/golden/v1/`) y los antiguos quedan como fixtures de migración.
- Versión más nueva: `FormatTooNew` y **modo congelado** del adaptador (conserva el blob sin tocarlo, no ejecuta lógica, avisa); guardar encima destruiría datos que no sabe leer. Versión más antigua: cadena de funciones puras `vN → vN+1`, cada una con su blob dorado; v0 no tiene ninguna ni secciones opcionales.
- Los números de versión del guardado y del juego son del juego (runtime-api C17): el adaptador guarda enteros propios.
- `hostStepMicros` viaja en el mundo: cambiar la configuración reinterpretaría todos los `Delay` y `Clock` construidos; si difiere, manda el guardado y se avisa.

## 7. La llamada del adaptador

### 7.1 Secuencia por `PreFactoryTick`

Solo `PreFactoryTick` está documentado como seguro para leer y escribir cualquier edificio (runtime-api C1).

1. Salir si no hay autoridad (`HasAuthority`, C6), el mundo se desmonta o el modo es congelado.
2. Reintentar las ediciones diferidas (`FrameDriver::apply`).
3. `dtMicros = clamp(round(dt·10⁶), 0, 2³²−1)` (único punto con `float`; un `dt` NaN o negativo es 0, porque convertir un NaN a entero no está definido); `steps = pacer.advance(dtMicros, paused)` con `pacer = StepPacer::forHostStep(sim.hostStepMicros(), maxStepsPerCall)` (7.3); si es 0, salir. `steps` sale **solo** del pacer: ninguna otra cuenta de tiempo pasa al `FrameDriver`.
4. Por cada sensor con enlace resuelto, `offer(id, cuantizar(leer))`; sin lectura no se llama.
5. `runCall(steps, dtMicros)`. `dtMicros` es informativo: ninguna decisión del `FrameDriver` depende de él (7.4).
6. Por cada `changes()`: con autoridad y objetivo existente, escribir en vanilla y `acknowledge(id)`; si no, no se confirma y se reofrece en la llamada siguiente.
7. `endCall()`.

El `FrameDriver` es así una función pura de lo que se le da (lecturas, pasos y ediciones): no lee reloj ni tiempo de pared, y el registro de reproducción (7.3) basta para reproducirlo.

### 7.2 Contrato de ráfaga

El mundo está congelado dentro de la llamada: hay **una** muestra y hasta `steps` pasos. Los sensores `Level` valen lo mismo en todos; los `Pulse` valen la lectura solo en el primer paso y 0 después; cada actuador llega al mundo con su **valor final**, y un cambio que va y vuelve dentro de la llamada es invisible. Traza (`L` Level y `P` Pulse, actuadores `A1` ← `L` y `A2` ← `P`, todo en 0, lectura 15, una llamada de 3 pasos):

```
paso 0: e=(L15,P15)  A1'=15  A2'=15  -> X1=(15,15)
paso 1: e=(L15,P0)   A1'=15  A2'=0   -> X2=(15,0)
paso 2: e=(L15,P0)   A1'=15  A2'=0   -> X3=(15,0)
fin: mandos (15,0) frente a lo confirmado (0,0): changes = { A1: 0→15 }
```

El 15 de `A2` en `X1` nunca llega al mundo (un pulso hacia el mundo se alarga con componentes). Con tres llamadas de un paso el estado final es el mismo `X3`, pero el mundo ve subir y bajar `A2` (T13). Esto vale si el sensor `Pulse` se ofrece **solo en la primera** de las tres llamadas, que es lo que 7.1 produce cuando el pulso se lee una vez: sin lectura, un `Pulse` vale 0 y un `Level` conserva el último valor (7.4). Si se ofreciera en cada llamada, cada una sería un pulso nuevo, `A2` se quedaría en 15 y el estado final diferiría de `X3` (T13). Es la lectura del oráculo (ambiguities.md B9, resuelto).

Una lectura `Pulse` vale solo en el primer paso de la **misma** llamada que la recibe. `runCall(0, …)` no corre ningún paso: retiene las lecturas `Level` (y resuelve su sensor, 7.4), **descarta** las `Pulse` y no acredita tiempo de espera. No es una llamada vacía: abre la llamada como cualquier otra (`changes()` puede traer entregas pendientes y hay que cerrarla con `endCall()`). 7.1 nunca la hace (sale antes si `steps` es 0); queda fijado igualmente, como en el oráculo.

### 7.3 Paso, acumulador y descarte

`stepMicros = 100000` (un "tick de redstone", MC2) y `maxStepsPerCall = 4` son [P]. `advance`: en pausa suma `dt` a `pausedMicros` y devuelve 0 (no acumula: sin ráfaga al reanudar). Si no: `acc += dt`; `n = acc / step`; si `n > max`, `dropped += n − max` y `n = max`; `acc %= step`. Se **descarta** el retraso: el tiempo lógico se dilata bajo carga y no hay espiral. Invariante: `Σ dt = pausedMicros + (Σ pasos + dropped)·stepMicros + acc`, con `0 ≤ acc < stepMicros`.

Ejemplo: `dt = 250000` → `acc = 250000`, `n = 2`, `acc = 50000`. `dt = 30000` → `acc = 80000`, `n = 0`. `dt = 530000` → `acc = 610000`, `n = 6 > 4`, `dropped = 2`, `n = 4`, `acc = 10000`. Comprobación: `Σ dt = 810000 = (6 + 2)·100000 + 10000`. Los pasos por llamada dependen del tiempo real, así que el registro de reproducción guarda los de cada llamada (con las lecturas y las ediciones: es todo lo que el `FrameDriver` necesita). Autopausa (C18, sin verificar): si el manejador no se llama, el tiempo lógico se congela con la fábrica; si se llama sobre un mundo congelado, `paused = true` cuando el adaptador pueda saberlo y, si no, avanzan solo `Clock` y anillos.

El paso de un mundo es su `hostStepMicros`; **0 significa "sin fijar", no "1 µs"**. `effectiveStepMicros(h)` devuelve 100000 si `h` es 0 y `h` si no, y es la única traducción: `StepPacer::forHostStep(h, max)` construye el pacer con ella y el tiempo de espera de 7.4 la usa también, así que pasos y tiempo no se desacoplan. (El constructor `StepPacer(step, max)` sigue limitando ambos a un mínimo de 1.)

### 7.4 Sensores

- Nunca se escribe un 0 por defecto: sin lectura (objetivo sin resolver o destruido) la lógica ve el último valor bueno; un 0 falso al cargar podría verse como "depósito vacío" y encender generadores.
- Un sensor recién creado lee 0 y está `unresolved` hasta su primera lectura; tras cargar lo están **todos**. Un sensor `unresolved` retiene los cambios (`changes()` sale vacío para ellos; los pasos sí corren) **solo de los actuadores cuyo cono** (el actuador y sus ancestros en el grafo inducido por redes, el que recorre `settlingBounds`, 01 §8) contiene algún sensor `unresolved`. Un sensor sin cables o fuera del cono de un actuador no lo retiene. Deja de estar `unresolved` con su primera lectura (también la que llega en `runCall(0, …)`) o al agotar su espera.
- **La espera se cuenta en pasos ejecutados, nunca en tiempo**: `límite = ⌈resolveTimeoutMicros / paso⌉` pasos, con `paso = effectiveStepMicros(sim.hostStepMicros())` (5 s [P] son 50 pasos de 100000 µs). Cada llamada acredita a cada sensor sin lectura **los pasos que corre**, ni uno más: tras un tirón de 5 s el pacer deja pasar 4 pasos y la llamada acredita 4, no 5 s; `runCall(0, …)` no acredita nada; con `resolveTimeoutMicros = 0` todos se resuelven en la primera llamada. Como el `elapsedMicros` de `runCall` no cuenta, las tramas que 7.1 omite (0 pasos) tampoco alargan la espera.
- **Transitorio de resolución**: cuando un sensor pasa de `unresolved` a resuelto, cada actuador de su cono con `settlingBounds` definido retiene su entrega `L_a` pasos a partir de esa llamada (máximo con la retención que ya tenga; con `settleGuard` falso no hay ninguna). Sin ella, el cono pasaría del 0 que alimentaba por defecto al valor real y ese transitorio llegaría al mundo aunque el valor final fuera el que el mundo ya tiene. Para conos con memoria o ciclos no hay guarda (D16).
- Una lectura por encima de 15 se satura a 15 al tomarla (se retiene y se guarda saturada) y suma **una** vez a `clampedSamples()` (entra en el núcleo en el primer paso que la usa), no una por paso (I8); una lectura que toma una llamada sin pasos no llega a ningún paso y no cuenta. Cambiar el modo de un sensor (`setSensorMode`) olvida el valor retenido: un `Pulse` no retiene nada.
- Tras cargar, un `Level` retiene su valor guardado y un `Pulse` arranca en 0 (si no, el pulso se repetiría).

### 7.5 Actuadores

- Se entregan **solo cambios** respecto a lo confirmado: el mando sigue siendo un nivel, pero no se llama a setters de efectos no verificados (C3, C5, C23) en cada tick.
- Lo confirmado se guarda en el blob del `FrameDriver`, de versión propia (little-endian, sin relleno): magia `53 4C 47 44` ("SLGD"), `u16 version = 1`, `u64 tick`, `u64 stateHash` y `u64 structureHash` del `Simulator` con el que se guarda, `u32` sensores, `u32` actuadores; por sensor `u32 id`, `u8` valor retenido (0 en un `Pulse`) y `u8` modo (0 `Level`, 1 `Pulse`); por actuador `u32 id`, `u8` confirmado (0..15 o `0xFF`) y `u32` retención en pasos; cola FNV-1a de 64 bits de todo lo anterior. Mide 38 + 6·S + 9·A + 8 bytes. Aún no se ha publicado ninguna versión de este blob: su formato 1 es este.
- **Decodificación estricta y total** (en este orden): magia; versión (mayor que 1: `FormatTooNew` sin mirar más; 0: `FormatCorrupt`); longitud mínima; cola; los conteos (la suma de registros iguala los bytes que quedan) y que coincidan con los sensores y actuadores del `Simulator`; la **pareja**: `tick`, `stateHash` y `structureHash` iguales a los del `Simulator` tal como está ahora, que rechaza un par tomado en momentos distintos (el guardado asíncrono de 7.6: otro tick, otro estado u otros cables, aunque los ids sean los mismos); ids iguales a los suyos y en su orden; modo ≤ 1; `Level` retenido ≤ 15 y `Pulse` retenido 0; confirmado ≤ 15 o `0xFF`; retención ≤ `maxComponents + maxDelayCells` (el camino más largo que cabe en el `Simulator`; no se exige ≤ `L_a` de hoy, porque una edición que acorta un cono deja la retención que armó antes); ni un byte de más. Un fallo es `FormatCorrupt` y deja el driver intacto, y `encode(decode(b)) == b`.
- Tras cargar la diferencia es vacía salvo lo pendiente: **no se reaplica nada**, porque el estado vanilla ya persiste (C16). `resync()` lo olvida todo y reentrega, bajo petición; mientras tanto `ActuatorChange::previous` vale `kUnconfirmed` (255, fuera de 0..15): el adaptador no lo usa como índice ni lo compara como señal.
- Un actuador creado empieza con `confirmado = o` inicial: **no se escribe al crear**.
- **Override manual**: si el jugador cambia a mano un interruptor gobernado, el cambio dura hasta que el valor ordenado cambie o se llame `resync()`; reafirmar cada N llamadas es una opción del adaptador, desactivada.
- **Guarda de asentamiento**: tras un `apply`, solo los actuadores cuyo **cono cambió** retienen su entrega `L_a` pasos (01 §6.3), con `L_a` donde `settlingBounds` lo define y quedándose con el mayor entre la retención que ya tenían y `L_a` (no se acumulan ni se reinician). El cono cambió si contiene un componente nuevo, reemplazado (también por sí mismo con otro seed, que reinicia su estado) o con otros conductores en alguna entrada que antes de la edición: se comparan, antes y después, la huella de cada componente (id, `Spec`, estado y ids de los conductores de cada entrada), y quitar un componente cambia a quienes lo leían. Una edición que no toca el cono de un actuador no lo retiene, así que un flujo de ediciones ajenas no lo retrasa. La retención se cuenta en pasos ejecutados (cada llamada descuenta los suyos) y va en el blob. Sin `L_a` definido (cono con latches, toggles, relojes o ciclos) no hay guarda y el transitorio puede llegar al mundo (D16). Una edición hecha directamente en el `Simulator`, sin pasar por `FrameDriver::apply`, no arma la guarda.

### 7.6 Carga, guardado, desmontaje y retirada del mod

- **Empezar el mundo** (solo con autoridad): crear el `Simulator`; si hay blob, `decode` en uno nuevo y adoptarlo con `Ok`. **Adoptarlo es crear un `FrameDriver` nuevo ligado a ese `Simulator`** y hacer `decode` del blob del driver en él (7.5): un driver está ligado a su `Simulator` toda su vida, y reasignar el cargado sobre el que usa un driver vivo conservaría la sesión de este (sensores ya resueltos, confirmaciones) y se saltaría el "tras cargar todos `unresolved`" de 7.4. Si el blob del driver falta o no hace pareja (`FormatCorrupt`), el driver nuevo empieza sin blob: nada se escribe (cada actuador parte confirmado en su mando actual) y se avisa. Registrar el manejador con `AddFactoryTickHandler` (transitorio: repetir en cada mundo, C1).
- **Guardar**: `ShouldSave` verdadero (C13); copiar a la propiedad SaveGame, **en el mismo instante y entre llamadas**, `encode()` del `Simulator` y `encode()` del driver (guardado asíncrono del motor: pregunta abierta; si el motor los tomara en momentos distintos, el `decode` del par lo detecta, 7.5).
- **Desmontar**: dar de baja el manejador (el nombre no está en la investigación; StructuralPower lo hace al limpiar el mundo, C1), soltar las referencias débiles y destruir el núcleo.
- **Retirada del mod**: el contenido modificado desaparece y el estado vanilla escrito persiste (C16); guardar sin el mod hace la pérdida definitiva. Mitigación: un comando "liberar actuadores" antes de retirar.

## 8. Contrato con Unreal

**Con respaldo en la investigación**: `AModSubsystem` + `IFGSaveInterface` (C13); `PreFactoryTick` con `AFGBuildableSubsystem::AddFactoryTickHandler` (C1); `GetFactorySimulationTime()` como alternativa de delta (C2); `HasAuthority()` antes de escribir (C6); ediciones del cliente por RCO, nunca multicast (C15); propiedad SaveGame con versión propia (C17).

**El adaptador promete**: un solo hilo; cuantización fuera del núcleo; escrituras solo por `changes()` y con autoridad; ediciones solo entre llamadas; dos blobs con versiones propias; ningún puntero crudo. **El núcleo promete** I1 a I8 y las pruebas de [03](03-pruebas.md).

**Lo que solo resuelve el spike en el PC del propietario** (no se inventa ningún nombre): (1) si `PreFactoryTick` corre por fotograma o por subpaso, con qué delta y en qué orden respecto al circuito de potencia (C1, C2); (2) Autopausa (C18); (3) autoridad y efectos de `SetSwitchOn`, `SetIsProductionPaused`, `SetSortRules` y `SetLightEnabled` (C3, C5, C23); (4) orden de carga y `BeginPlay`, evento de construcción, pegado de blueprint y clave estable del edificio; (5) ticks por segundo con fábrica real (C19) y coste por evaluación (C22); (6) guardado asíncrono; (7) nombres de la baja del manejador y de la comprobación de hilo; (8) `<span>`, excepciones y RTTI en UE 5.6.1; (9) cómo devuelve el servidor el `Status` de una edición del cliente sin multicast (C15).
