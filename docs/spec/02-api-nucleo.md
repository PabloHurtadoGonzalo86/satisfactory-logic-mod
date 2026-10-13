# 02. API del núcleo y contrato con el adaptador

Estado: borrador de diseño del 13-oct-2026. Todo es **[P]** salvo lo que cita la investigación: `C<n>` es de `runtime-api`; `PR`, `MC`, `FA`, `TH`, `OT` y `EN`, de `redstone-and-logic-theory`; `c<n>`, de `wires-and-connections`. Semántica en [01](01-semantica.md), pruebas en [03](03-pruebas.md), decisiones en [04](04-decisiones.md).

## 1. Capas

- `core/`, espacio `slg`: `Netlist`, `Simulator`, lint, codec y huellas. C++20 sin tipos de Unreal, sin excepciones ni RTTI, sin `float`, sin reloj, sin globales mutables.
- `core/host/`, espacio `slg::host`: `StepPacer` y `FrameDriver`, la política entera de una llamada del anfitrión. Depende de `slg`, nunca al revés: **ninguna API del núcleo conoce "llamada"**. Se prueba en Linux.
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
  ComponentId add(Spec, std::uint16_t seed);              // id = base nextId + adds so far
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
class StepPacer {                                         // integer microseconds
 public:
  StepPacer(std::uint32_t stepMicros, std::uint32_t maxStepsPerCall) noexcept;  // clamped to >= 1
  std::uint32_t advance(std::uint32_t elapsedMicros, bool paused) noexcept;     // steps to run now
  std::uint64_t droppedSteps() const noexcept;  std::uint64_t pausedMicros() const noexcept;
};
enum class SensorMode : std::uint8_t { Level, Pulse };
struct ActuatorChange { ComponentId id; Signal previous; Signal value; };   // previous = last acknowledged
struct DriverOptions { std::uint32_t resolveTimeoutMicros = 5'000'000; bool settleGuard = true; };
class FrameDriver {
 public:
  explicit FrameDriver(Simulator&, DriverOptions = {});
  Status apply(const EditBatch&);                         // WrongPhase between runCall and endCall
  void setSensorMode(ComponentId, SensorMode);
  void resync();                                          // forget acknowledged values
  void offer(ComponentId sensor, Signal reading);         // only for successful reads
  Status runCall(std::uint32_t steps, std::uint32_t elapsedMicros);   // ONE sample, n steps
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

- El núcleo asigna ids monótonos desde `nextId`; un lote con otra base da `StaleBatch`; con `nextId = 0xFFFFFFFF`, `add` da `LimitExceeded`. `remove` retira el id (`remove` + `add` del mismo id es `RetiredId`). Cambiar un parámetro es `replace`: conserva id y cables (si un pin cableado deja de existir, el lote falla con `BadPin`) y reinicia el estado al seed, así no se redimensionan FIFOs con pulsos en vuelo.
- **Adaptador [P]**: cada edificio lógico guarda un `uint32 LogicComponentId` como propiedad SaveGame (runtime-api C17); es una **pista** y manda el blob. La tabla `id → edificio` vive en memoria con referencias débiles (ningún puntero crudo entre cargas, critic "Teardown").
- **Tras cargar**: un edificio con un id desconocido o ya reclamado (un blueprint pegado duplica la propiedad) pide un componente nuevo. Un componente sin edificio es un huérfano y **no se borra de inmediato** (el orden de carga y `BeginPlay` no está verificado): se borra tras una gracia en tiempo del adaptador (suma de dt; 10 s [P]), no en pasos del núcleo, que en pausa no avanzan.
- Los enlaces a objetos vanilla son del adaptador, con su propio entero de versión; los ids y punteros de circuito de vanilla no se cachean (runtime-api C11). La clave estable del edificio queda para el spike.

## 5. Ediciones en vivo

Un `EditBatch` se valida entero sobre una copia y solo si todas sus operaciones valen sustituye al original. Las operaciones valen en orden (`connect` y luego `remove` del mismo componente vale; al revés, no). Los supervivientes conservan su estado bit a bit (T11); `add` y `replace` nacen en su seed; `connect` y `disconnect` unen o dividen redes con efecto en el siguiente tick; `remove` saca su salida de las redes sin fantasma; no hay rearme de flancos (01 §6.2).

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

`decode` construye un `Simulator` nuevo y el adaptador lo adopta solo con `Ok`. Orden: magia; versiones (mayor que la soportada: `FormatTooNew` sin mirar más); cola; lectura secuencial con los bytes restantes comprobados en cada campo y los conteos ≤ límites **y** ≤ bytes restantes antes de reservar memoria; ids ascendentes y `nextId` mayor que todos; parámetros válidos (los no usados a 0); estados en su dominio; cables normalizados, ascendentes y con ambos pines existentes; ni un byte de más. Un fallo es `FormatCorrupt` (`LimitExceeded` si un conteo excede un límite). Sustituir un solo byte siempre cambia la cola (cada paso de FNV-1a es biyectivo).

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

- `formatVersion` cambia el contenedor; `semanticsVersion` cambia `F` (toda transición, fusión o desempate modificado lo incrementa aunque los bytes no cambien); las trazas doradas viven en un directorio por `semanticsVersion` y los antiguos quedan como fixtures de migración.
- Versión más nueva: `FormatTooNew` y **modo congelado** del adaptador (conserva el blob sin tocarlo, no ejecuta lógica, avisa); guardar encima destruiría datos que no sabe leer. Versión más antigua: cadena de funciones puras `vN → vN+1`, cada una con su blob dorado; v0 no tiene ninguna ni secciones opcionales.
- Los números de versión del guardado y del juego son del juego (runtime-api C17): el adaptador guarda enteros propios.
- `hostStepMicros` viaja en el mundo: cambiar la configuración reinterpretaría todos los `Delay` y `Clock` construidos; si difiere, manda el guardado y se avisa.

## 7. La llamada del adaptador

### 7.1 Secuencia por `PreFactoryTick`

Solo `PreFactoryTick` está documentado como seguro para leer y escribir cualquier edificio (runtime-api C1).

1. Salir si no hay autoridad (`HasAuthority`, C6), el mundo se desmonta o el modo es congelado.
2. Reintentar las ediciones diferidas (`FrameDriver::apply`).
3. `dtMicros = clamp(round(dt·10⁶), 0, 2³²−1)` (único punto con `float`); `steps = pacer.advance(dtMicros, paused)`; si es 0, salir.
4. Por cada sensor con enlace resuelto, `offer(id, cuantizar(leer))`; sin lectura no se llama.
5. `runCall(steps, dtMicros)`.
6. Por cada `changes()`: con autoridad y objetivo existente, escribir en vanilla y `acknowledge(id)`; si no, no se confirma y se reofrece en la llamada siguiente.
7. `endCall()`.

### 7.2 Contrato de ráfaga

El mundo está congelado dentro de la llamada: hay **una** muestra y hasta `steps` pasos. Los sensores `Level` valen lo mismo en todos; los `Pulse` valen la lectura solo en el primer paso y 0 después; cada actuador llega al mundo con su **valor final**, y un cambio que va y vuelve dentro de la llamada es invisible. Traza (`L` Level y `P` Pulse, actuadores `A1` ← `L` y `A2` ← `P`, todo en 0, lectura 15, una llamada de 3 pasos):

```
paso 0: e=(L15,P15)  A1'=15  A2'=15  -> X1=(15,15)
paso 1: e=(L15,P0)   A1'=15  A2'=0   -> X2=(15,0)
paso 2: e=(L15,P0)   A1'=15  A2'=0   -> X3=(15,0)
fin: mandos (15,0) frente a lo confirmado (0,0): changes = { A1: 0→15 }
```

El 15 de `A2` en `X1` nunca llega al mundo (un pulso hacia el mundo se alarga con componentes). Con tres llamadas de un paso el estado final es el mismo `X3`, pero el mundo ve subir y bajar `A2` (T13).

### 7.3 Paso, acumulador y descarte

`stepMicros = 100000` (un "tick de redstone", MC2) y `maxStepsPerCall = 4` son [P]. `advance`: en pausa suma `dt` a `pausedMicros` y devuelve 0 (no acumula: sin ráfaga al reanudar). Si no: `acc += dt`; `n = acc / step`; si `n > max`, `dropped += n − max` y `n = max`; `acc %= step`. Se **descarta** el retraso: el tiempo lógico se dilata bajo carga y no hay espiral. Invariante: `Σ dt = pausedMicros + (Σ pasos + dropped)·stepMicros + acc`, con `0 ≤ acc < stepMicros`.

Ejemplo: `dt = 250000` → `acc = 250000`, `n = 2`, `acc = 50000`. `dt = 30000` → `acc = 80000`, `n = 0`. `dt = 530000` → `acc = 610000`, `n = 6 > 4`, `dropped = 2`, `n = 4`, `acc = 10000`. Comprobación: `Σ dt = 810000 = (6 + 2)·100000 + 10000`. Los pasos por llamada dependen del tiempo real, así que el registro de reproducción guarda los de cada llamada. Autopausa (C18, sin verificar): si el manejador no se llama, el tiempo lógico se congela con la fábrica; si se llama sobre un mundo congelado, `paused = true` cuando el adaptador pueda saberlo y, si no, avanzan solo `Clock` y anillos.

### 7.4 Sensores

- Nunca se escribe un 0 por defecto: sin lectura (objetivo sin resolver o destruido) la lógica ve el último valor bueno; un 0 falso al cargar podría verse como "depósito vacío" y encender generadores.
- Un sensor recién creado lee 0 y está `unresolved` hasta su primera lectura; tras cargar lo están **todos**. Mientras alguno lo esté, `changes()` sale vacío (los pasos sí corren) hasta la primera lectura o `resolveTimeoutMicros` de tiempo del adaptador (5 s [P]).
- Tras cargar, un `Level` retiene su valor guardado y un `Pulse` arranca en 0 (si no, el pulso se repetiría).

### 7.5 Actuadores

- Se entregan **solo cambios** respecto a lo confirmado: el mando sigue siendo un nivel, pero no se llama a setters de efectos no verificados (C3, C5, C23) en cada tick.
- Lo confirmado se guarda en el blob del `FrameDriver` (versión propia; por sensor `u32 id`, valor retenido y modo; por actuador `u32 id` y valor confirmado o `0xFF`; cola FNV-1a). Tras cargar la diferencia es vacía salvo lo pendiente: **no se reaplica nada**, porque el estado vanilla ya persiste (C16). `resync()` lo olvida todo y reentrega, bajo petición.
- Un actuador creado empieza con `confirmado = o` inicial: **no se escribe al crear**.
- **Override manual**: si el jugador cambia a mano un interruptor gobernado, el cambio dura hasta que el valor ordenado cambie o se llame `resync()`; reafirmar cada N llamadas es una opción del adaptador, desactivada.
- **Guarda de asentamiento**: tras un `apply`, cada actuador con `settlingBounds` definido retiene su entrega `L_a` pasos más (01 §6.3). Sin definir (cono con latches, toggles, relojes o ciclos) no hay guarda y el transitorio puede llegar al mundo (D16).

### 7.6 Carga, guardado, desmontaje y retirada del mod

- **Empezar el mundo** (solo con autoridad): crear el `Simulator`; si hay blob, `decode` en uno nuevo y adoptarlo con `Ok`; registrar el manejador con `AddFactoryTickHandler` (transitorio: repetir en cada mundo, C1).
- **Guardar**: `ShouldSave` verdadero (C13); copiar `encode()` a la propiedad SaveGame en ese momento (guardado asíncrono del motor: pregunta abierta).
- **Desmontar**: dar de baja el manejador (el nombre no está en la investigación; StructuralPower lo hace al limpiar el mundo, C1), soltar las referencias débiles y destruir el núcleo.
- **Retirada del mod**: el contenido modificado desaparece y el estado vanilla escrito persiste (C16); guardar sin el mod hace la pérdida definitiva. Mitigación: un comando "liberar actuadores" antes de retirar.

## 8. Contrato con Unreal

**Con respaldo en la investigación**: `AModSubsystem` + `IFGSaveInterface` (C13); `PreFactoryTick` con `AFGBuildableSubsystem::AddFactoryTickHandler` (C1); `GetFactorySimulationTime()` como alternativa de delta (C2); `HasAuthority()` antes de escribir (C6); ediciones del cliente por RCO, nunca multicast (C15); propiedad SaveGame con versión propia (C17).

**El adaptador promete**: un solo hilo; cuantización fuera del núcleo; escrituras solo por `changes()` y con autoridad; ediciones solo entre llamadas; dos blobs con versiones propias; ningún puntero crudo. **El núcleo promete** I1 a I8 y las pruebas de [03](03-pruebas.md).

**Lo que solo resuelve el spike en el PC del propietario** (no se inventa ningún nombre): (1) si `PreFactoryTick` corre por fotograma o por subpaso, con qué delta y en qué orden respecto al circuito de potencia (C1, C2); (2) Autopausa (C18); (3) autoridad y efectos de `SetSwitchOn`, `SetIsProductionPaused`, `SetSortRules` y `SetLightEnabled` (C3, C5, C23); (4) orden de carga y `BeginPlay`, evento de construcción, pegado de blueprint y clave estable del edificio; (5) ticks por segundo con fábrica real (C19) y coste por evaluación (C22); (6) guardado asíncrono; (7) nombres de la baja del manejador y de la comprobación de hilo; (8) `<span>`, excepciones y RTTI en UE 5.6.1; (9) cómo devuelve el servidor el `Status` de una edición del cliente sin multicast (C15).
