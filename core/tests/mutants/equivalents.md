# Mutantes que ninguna prueba puede matar

Estado: 13-oct-2026. Un mutante de esta carpeta que sobrevive a toda la suite es un test que falta, salvo que
aparezca aquí. `equivalent.txt` (una línea por id) es lo que lee `scripts/run_mutants.sh` para no darlo por
superviviente; este fichero guarda la prueba de cada caso. Si un mutante de esta lista llega a morir, el script
avisa (`NOTE: listed in equivalent.txt but killed`) y el caso se revisa.

Hay dos clases, y no se mezclan:

- **A. Equivalentes.** La función del mutante es la del original en todo el dominio alcanzable: no existe
  entrada (ni fallo de prueba posible) que los distinga. Cada uno lleva su demostración y, además, se comprobó que
  **la suite completa** (`slg_tests` entero, 100 casos) pasa sobre el mutante.
- **B. Indeterminados por la spec.** Los dos programas difieren en algo observable, pero el texto no fija cuál es el
  correcto; una prueba que lo fijara elegiría entre dos lecturas legítimas. No se añade la prueba y se deja
  constancia para quien cierre el texto (hoy esta clase está vacía).

## A. Equivalentes

### M22: `Subtract` con `a >= b` en lugar de `a > b`

`Compare` está en `simulator.cpp`, fase C: `result = a > b ? a - b : 0` frente a `result = a >= b ? a - b : 0`, con
`a` y `b` sin signo y en 0..15 (valores de red, I1). Las dos expresiones solo pueden discrepar cuando `a == b`
(si `a > b` ambas valen `a - b`; si `a < b` ambas valen 0). Con `a == b` el original da 0 y el mutante da
`a - b = 0`. La resta de 01 §3 es `max(A - B, 0)`, que vale lo mismo en ambos. La tabla exhaustiva de 16² de T5 no
puede, por tanto, distinguirlos, y no hay nada más que comprobar.

### M24: `And` con `truthy >= inputs` en lugar de `truthy == inputs`

`truthy` cuenta, entre las `slot.inputs` entradas de la puerta, las que valen más de 0: se incrementa como mucho una
vez por entrada, así que `truthy <= inputs` siempre. Con esa cota, `truthy >= inputs` y `truthy == inputs` son el
mismo predicado. (La aridad mínima es 1: `validate` rechaza `k = 0`, de modo que tampoco hay una puerta sin entradas
cuyo `And` vacío valdría verdadero con las dos formas.)

### M29: `Clock` que da la vuelta con `phase >= P` en lugar de `phase == P`

En la transición, `phase = c + 1` con `c < P`: la semilla cumple `seed < P` (`seedInDomain`), `decode` rechaza un
`c >= P` (`stateInDomain`, probado con cola recalculada por T12) y la propia transición produce `c' = phase < P` o 0.
Luego `phase <= P`, y con esa cota `phase >= P` y `phase == P` coinciden. Es una equivalencia **condicionada a I1**
(`c < P`); si algún día `stateInDomain` se relajara, dejaría de serlo y habría que matarlo.

### M48: `StepPacer` que recorta con `steps >= max` en lugar de `steps > max`

Cuando `steps == maxSteps_` el bloque suma `steps - maxSteps_ = 0` a `dropped_` y asigna `steps = maxSteps_`, que ya
valía lo mismo; cuando `steps > maxSteps_` hace lo mismo que el original. Ni el valor devuelto ni `droppedSteps()` ni
el acumulador cambian (02 §7.3).

## B. Indeterminados por la spec

Ninguno. M45 (`EditBatch::add` con los ids agotados devuelve 0 o `0xFFFFFFFF`) estuvo aquí hasta que 02 §2 y §4 fijaron
el valor (decisión D30: 0, y el lote da `LimitExceeded` al aplicarse); `T11_edit_semantics` y
`T02_add_returns_the_assigned_id` lo comprueban con `lastBatchIdsMatched()` y el mutante muere.
