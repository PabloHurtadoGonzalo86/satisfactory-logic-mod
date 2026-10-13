# Núcleo (`core/`)

C++20 puro, sin Unreal, sin excepciones ni RTTI, sin `float`, sin reloj. Diseño en
[docs/spec](../docs/spec/01-semantica.md) (semántica, API, pruebas, decisiones). Este fichero cuenta cómo se
compila, se prueba y se vigila el núcleo; no repite la spec.

## Estructura

```
core/include/slg/    cabeceras públicas (types, netlist, simulator, lint, host/*; host/cone_graph.hpp es un detalle de FrameDriver)
core/src/            implementación; core/src/internal y core/src/host son parte de la misma biblioteca
core/tests/          suite: un solo ejecutable, build/slg_tests (todos los core/tests/*.cpp)
core/tests/mutants/  M01.patch ... M100.patch, equivalent.txt (lo que lee el script), equivalents.md (las pruebas) y README.md (la tabla)
core/golden/         trazas doradas; las emite el oráculo (tools/oracle), ver core/golden/README.md
core/bench/          slg_bench (03 §8)
core/selfcheck/      slg_selfcheck (03 §9, comprobación de MSVC)
core/tools/          opcional: un ejecutable `slg_run NET SCN` que escribe un ltrace en stdout
core/fuzz/           opcional: fuzz_*.cpp con LLVMFuzzerTestOneInput (libFuzzer, solo clang)
scripts/             check_forbidden.py, check_golden.py, run_mutants.sh y sus pruebas
tools/oracle/        oráculo Python independiente (N-versión)
Makefile             todos los puntos de entrada; .github/workflows/core-ci.yml hace lo mismo en CI
```

Los `*.cpp` se encuentran con comodines: un fichero nuevo entra en el siguiente `make` sin tocar nada.
`core/tests/*_nosim.cpp` (el sustituto sin núcleo) no se enlaza nunca en `make test`.

## En una máquina normal

Hace falta `g++` o `clang++` con C++20, `make`, `python3` y `patch`. Los scripts de `scripts/` solo usan la
biblioteca estándar (probados con Python 3.14).

```sh
make test                      # compila build/slg_tests con las banderas estrictas y lo ejecuta
make test CXX=clang++ OPT=-O0
make test-ubsan                # UBSan, se detiene en el primer aviso
make test-asan                 # ASan + UBSan juntos: solo en CI o en el PC del propietario, nunca en el VPS
make forbidden                 # tokens prohibidos de 03 §9 en core/include y core/src
make golden-oracle             # solo oráculo contra core/golden, sin compilar C++ (salvo un slg_run opcional)
make golden                    # lo anterior y las pruebas golden de C++ (GOLDEN/*)
make mutants                   # los parches de core/tests/mutants deben hacer fallar sus pruebas
make bench                     # contadores deterministas y tiempos de slg_bench
make selfcheck                 # imprime `digest ...`, igual en todo compilador y plataforma
make clean                     # borra solo lo que crea el Makefile dentro de build/
```

| Variable | Defecto | Para qué |
|---|---|---|
| `CXX` | `g++` | compilador (CI: `g++` y `clang++`) |
| `OPT` | `-O2` | nivel de optimización de la variante normal; las de sanitizers usan `-O1 -g` |
| `EXTRA_FLAGS`, `EXTRA_LDFLAGS` | vacío | banderas añadidas al final (`-m32`, `--coverage`...) |
| `SLG_CASES` | `200` | casos por propiedad (03 §6); CI usa 2000, de noche 100000 |
| `MUTANTS` | todos | ids a ejecutar, p. ej. `MUTANTS="M01 M05"` |
| `GOLDEN_DIR`, `GOLDEN_TESTS` | `core/golden`, `GOLDEN/` | dónde están los goldens y qué subcadena de nombre selecciona sus pruebas C++ |
| `ORACLE_CMD`, `RUN_CMD` | ver Makefile | cómo se invocan el oráculo y, si existe, un `slg_run` independiente (`{lnet}`, `{lscn}`, `{out}`) |
| `FUZZ_CXX`, `FUZZ_SECONDS`, `FUZZ_ALLOW_EMPTY` | `clang++`, `60`, vacío | `make fuzz`: compilador, segundos por objetivo, aceptar que no haya objetivos |
| `SLG_BENCH_BIG`, `SLG_BENCH_BUDGET_NS`, `SLG_BENCH_MARGIN` | `0`, sin definir, `4` | `1` añade N = 100000; los otros dos imprimen la estimación de N_max de 03 §8 |

Las banderas son las de 03 §9 (`-std=c++20 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
-Wsign-conversion -Wold-style-cast -Wfloat-equal`) más `-fno-exceptions -fno-rtti`. Los objetos se reconstruyen
si cambia el compilador o cualquier bandera. Las variantes de sanitizers añaden `-fno-sanitize-recover=all
-D_GLIBCXX_ASSERTIONS -ftrivial-auto-var-init=pattern`, y `-fno-sanitize=vptr` porque vptr exige RTTI.

## En este VPS (producción compartida, unos 2 GB libres, sin clang ni cmake)

Es una máquina de producción que se queda sin memoria con facilidad (OOM del kernel el 9 y el 13 de octubre) y que
no es nuestra: aquí **ni ASan ni valgrind**, ni clang, libFuzzer, `-m32`, la matriz completa ni `N = 100000` del
benchmark. Eso va a CI o al PC del propietario. Aquí solo se compila con `g++`, se prueba con pocos casos y se
puede usar UBSan (`make test-ubsan SLG_CASES=50`). Política, para cualquier compilación o prueba:

```sh
LOCK=/ruta/al/candado.lock      # el mismo fichero para todos los procesos y agentes que tocan el repo
flock "$LOCK" nice -n 19 ionice -c3 /ruta/al/repo/scripts/guard.sh \
  bash -c 'ulimit -v 1500000; cd /ruta/al/repo && SLG_CASES=100 make -j1 test'
```

- **Un solo proceso pesado a la vez**: el `flock` serializa, y no se lanzan bucles de espera ni procesos en segundo
  plano que sobrevivan a la orden. `-j1` siempre; `SLG_CASES <= 100` (50 con UBSan).
- **`nice -n 19`, `ionice -c3` y `ulimit -v 1500000`** (KiB) acotan prioridad, disco y memoria virtual. ASan no
  arranca bajo `ulimit -v`: otra razón para dejarlo en CI.
- **`scripts/guard.sh <orden>`** es el guarda de calma: espera (hasta unos 20 minutos, mirando cada 10 s) a que la
  presión de memoria (`some avg10` de `/proc/pressure/memory`) sea inferior al 10 % y la carga de un minuto, a 12, y
  entonces ejecuta la orden. Si el host no se calma sale con **código 75** y no ejecuta nada: se espera y se
  reintenta, **nunca se salta el guarda** ni se baja el umbral.
- Sin `docker` ni `sudo`; todo dentro del repositorio, y lo que se genere (copias, registros) en `build/`, que git
  ignora.
- `make mutants` ejecuta los mutantes de uno en uno y puede tardar horas aquí. Envuélvelo igual y añade
  `MUTANT_GUARD=1` (el script espera al guarda antes de cada mutante y, si no se calma, para con código 75 tras
  imprimir lo que ha hecho), `MUTANT_KEEP_BASE=1` (conserva `build/mutants/base`: solo se recompila lo que cambió) y
  `MUTANT_TIMEOUT=120`:

  ```sh
  flock "$LOCK" nice -n 19 ionice -c3 /ruta/al/repo/scripts/guard.sh bash -c 'ulimit -v 1500000; \
    cd /ruta/al/repo && SLG_CASES=100 MUTANT_TIMEOUT=120 MUTANT_KEEP_BASE=1 MUTANT_GUARD=1 JOBS=1 make mutants'
  ```

  Con código 75, relanza con `MUTANTS="Mnn Mmm ..."` para los que no llegaron a correr. Un `flock` exterior basta
  para serializar; si además exportas `SLG_LOCK=$LOCK`, el script lo toma él mismo y detecta que ya lo tienes.

## Contratos que asume el Makefile

Son convenciones elegidas porque la spec no las fija; cada una es una variable o un fichero fácil de cambiar.

- **Selección de pruebas**: la primera línea de un mutante nombra pruebas con símbolos `T<n>` y `G<n>`;
  `run_mutants.sh` los resuelve con `slg_tests --list` (una prueba cuyo nombre empieza por la letra y el número,
  con o sin ceros a la izquierda: `T5` encuentra `T05_...`, no `T15`) y pasa los nombres completos en `SLG_ONLY`
  (lista de subcadenas separada por comas, `core/tests/harness.hpp`). Un símbolo sin prueba es `ERROR`.
  `MUTANT_PASS_ARGS=1` añade los nombres también como argumentos.
- **Goldens**: `core/golden/<n>.ltrace` con `<n>.lnet` y `<n>.lscn` al lado. `make golden-oracle` hace las dos primeras y `make golden` las tres:
  `tools/oracle/make_goldens.py --check` (todos los ficheros, incluidos blobs y de anfitrión, contra lo que emite
  el oráculo); `check_golden.py`, que recorre los `.ltrace` (también en subdirectorios) y los compara byte a byte
  con `oracle.py run NET SCN` y, si existe `core/tools/*.cpp`, con `build/slg_run NET SCN`; y las pruebas C++ de
  `slg_tests` cuyo nombre contiene `GOLDEN/` (`core/tests/golden_runner.cpp`: una por pareja). Falla si no hay
  ningún `.ltrace`, si una prueba golden se salta (`SKIP`) o si no corre ninguna.
- **Mutantes**: `core/tests/mutants/Mnn.patch`, aplicables con `patch -p1` desde la raíz; la **primera línea** es
  un comentario con los nombres de las pruebas que lo matan, p. ej. `# tests: T5 G6`. Se usan los símbolos
  `T<n>` y `G<n>` de esa línea. Un mutante cuyas pruebas no pasan antes del parche, que no aplica o no compila es
  `ERROR`, nunca "matado"; uno que sobrevive hace fallar el objetivo salvo que esté en `equivalent.txt`
  (`M07 motivo`, un id por línea). Con menos de 12 parches (03 §7) falla. Los símbolos de la cabecera tienen que
  existir en `--list`: copiar 03 §7 tal cual (M14: `T13, G12`) da `ERROR` si no hay una prueba con ese nombre.
- **Fuzz**: `core/fuzz/fuzz_*.cpp`, uno por objetivo (`fuzz_decode`, `fuzz_lnet`, `fuzz_frame`), compilados con
  todo `core/src` y libFuzzer+ASan+UBSan. Corpus semilla opcional en `core/fuzz/corpus/<objetivo>/`. Sin
  objetivos, `make fuzz` falla. Como CI no sube artefactos, un fallo se reproduce con la línea `Base64:` que
  libFuzzer imprime en el registro.
- **Prohibidos** (`scripts/check_forbidden.py`): `float`, `double` y literales en coma flotante, `<random>`,
  `rand(`, `std::shuffle`, `unordered_*`, `std::hash`, `<chrono>` y demás relojes, `throw`, `try`, `catch`,
  `long`, `memcpy`/`memmove`, `std::sort`. Comentarios y cadenas se ignoran. `std::sort` solo se admite con
  `// slg-allow(sort): <por qué las claves son únicas>` en la misma línea o la anterior; el resto no admite
  excepciones. Avisos que no fallan: `static` mutable y `thread_local`. Mira `core/include`, `core/src` y, si
  existe, `core/host`; no mira tests, bench ni selfcheck.

## Lecturas del oráculo aplicadas (13-oct-2026)

Donde la especificación admitía dos lecturas y el oráculo y el C++ leían distinto, rige la del oráculo
([ambigüedades](../docs/spec/ambiguities.md) § 5, [decisiones](../docs/spec/04-decisiones.md) D28 a D35).
Antes de este cierre la suite no lo veía: `ReferenceSim`, T11 y T12 aceptaban cualquiera de los dos estados;
ahora cada uno espera el estado exacto del oráculo y los mutantes M72 a M77 fijan cada regla.

| Punto | Qué rige | Dónde |
|---|---|---|
| B2, topes | el tope de `Limits` es la única cota superior: `LimitExceeded` sobre él, `BadSpec` solo para lo mal formado; `Clock`: `P < 2`, `P` contra el tope, `H`; `decode`: `LimitExceeded` o `FormatCorrupt` | `core/src/internal/spec_rules.cpp` |
| B4, `disconnect` | juzga los pines como `connect` (pin `a` entero antes que `b`) y después `UnknownWire` | `core/src/netlist.cpp` |
| B8, `decode` | cada conteo contra los bytes restantes por su cuenta; el total de ranuras de `Delay` y los pines por red, **al final** | `core/src/simulator_codec.cpp` |
| D30, `add` | `add()` devuelve la base más las llamadas a `add` anteriores del lote; con los ids agotados, 0. **Aquí se movió el oráculo** (`World._create`), no el C++ | `core/src/netlist.cpp`, `tools/oracle/oracle.py` |
| D31 a D35, `FrameDriver` | espera en pasos ejecutados, retención por cono, blob v1 emparejado con el `Simulator`, `runCall(0, …)` y adopción con un driver nuevo | `core/src/host/` y 02 §7 |

El digest de `slg_selfcheck` no cambió con estas lecturas: el autotest solo usa `add`, lotes bien formados y bytes
sustituidos sin recalcular la cola, así que no ejercita ninguna de ellas. Las cubre la suite, no el digest.

## CI

`.github/workflows/core-ci.yml`. Solo usa `actions/checkout` fijado por SHA y permisos `contents: read`.

| Trabajo | Qué hace | Cuándo |
|---|---|---|
| `forbidden` | `make forbidden` y la prueba del propio script | push, PR |
| `build` | `g++` y `clang++`, `-O0` y `-O2`: compila todo, `make test` con 2000 casos y el digest de la celda | push, PR |
| `sanitize` | `make test-asan` con `g++` y `clang++` | push, PR |
| `oracle` | `python3 -I -m unittest` en `tools/oracle`, `make golden-oracle` y `make golden` (en pasos separados) | push, PR |
| `digest` | las cuatro celdas compilador × optimización en un solo trabajo; exige el mismo digest | push, PR |
| `mutation`, `fuzz` | `make mutants` y `make fuzz` | lunes 03:00 UTC y manual |
| `bench` | `slg_bench` con N = 100000 | solo manual |

Fuera de este workflow, a propósito: la fila `theory` de 03 §9 (la cubre `theory-checks.yml`), `msvc` (a mano, ver
abajo) y `extra` (libc++, aarch64 con qemu-user, `-m32`, s390x: opcionales y de noche; `EXTRA_FLAGS=-m32` y
`CXX` bastan para probarlas a mano).

`digest` compara dentro de un trabajo porque pasar valores entre celdas de una matriz exige artefactos y el
único `action` permitido es `checkout`.

## Comprobación de MSVC

MSVC solo existe en el PC del propietario. El digest incluye resultados propios del núcleo actual (estados devueltos
por un blob corrupto, orden del lint): cambia con cualquier cambio del núcleo, así que solo importa la igualdad entre
compiladores y plataformas y no se fija como constante. `slg_selfcheck` comprueba contra las constantes de la spec (G1, G7,
SplitMix64, FNV-1a), ejecuta 500 escenarios sembrados (ediciones, lotes inválidos, ida y vuelta del blob, lint) y
escribe una línea `digest <16 hex>`. Si todo está bien termina con `result OK` y código 0.

1. En CI, abre el resumen de cualquier celda de `build` y copia su `digest`.
2. En Windows, compila `core/src/**/*.cpp` y `core/selfcheck/selfcheck.cpp` con `/std:c++20 /EHs-c- /GR- /W4`
   e `/I core\include` (**no verificado**: aquí no hay MSVC; ajusta las banderas si algún aviso propio sale) y
   ejecuta `slg_selfcheck.exe --expect <digest>`: sale con código 1 si no coincide.
3. Antes de cada release, no después.

## Benchmark

`slg_bench` mide mediana, p99 y máximo de ns por tick tras calentar, en redes `random`, `chain`, `fanin`
(redes de 121 pines) y `ring` con N en {1000, 10000} (más 100000 con `SLG_BENCH_BIG=1`), con semilla fija. Cada
escenario imprime dos líneas:

- `counters ...`: componentes evaluados y lecturas de red por tick, estructura (cables, sensores, grupos de
  realimentación, redes con varios conductores), bytes por componente de la netlist y del blob, y un checksum del
  estado tras los ticks medidos. Son deterministas para un mismo compilador y ABI (los campos basados en `sizeof`
  difieren entre libstdc++, libc++, MSVC y 32 bits) y son lo único que CI debería vigilar.
- `timing ...`: informativa; los tiempos buenos se miden en el PC del propietario y en el servidor con fábrica real.
  Con `SLG_BENCH_BUDGET_NS=<B>` añade `n_max_estimate = B · N / (p99 · margen)`.
