# 01. Semántica formal del núcleo

Estado: borrador de diseño del 13-oct-2026, `semanticsVersion = 1`. Piezas hermanas: [02 API](02-api-nucleo.md), [03 pruebas](03-pruebas.md), [04 decisiones](04-decisiones.md).

## 0. Cómo leer

- **[F]** fijado por el propietario ([0001](../decisions/0001-decisiones-iniciales.md)) o por la investigación, con cita. **[P]** propuesta nuestra, sin fuente primaria; toda cifra de límites, tiempos y tamaños es [P] hasta medirla (runtime-api C22).
- Citas: "redstone PR1" es `docs/research/redstone-and-logic-theory.md`; "runtime-api C1", "wires c3" y "critic" son sus páginas homónimas; "theory/04" es `docs/theory/04-memoria-biestables.md`. Memoria, retardos y relojes se explican en [theory/04](../theory/04-memoria-biestables.md), [06](../theory/06-relojes-y-contadores.md) y [07](../theory/07-maquinas-de-estado.md); aquí solo se fija el modelo.
- Diseño propio desde cero, sin copiar código de Circuitry (sin licencia) ni de FicsIt-Networks (GPL-3.0) (0001 §4); sobre escribir "clean-room" en público, D23.
- `F` es 15; en las ondas `#` vale F y `_` vale 0. El tiempo del modelo se mide en **ticks**; cada invocación del gancho del juego es una **llamada**.
- redstone PR1 a PR7 son propuestas sin fuente primaria ni código guardado (critic): se adoptan como diseño y cada afirmación comprobable es una prueba (T1 a T14, [03](03-pruebas.md)). Los ejemplos se derivan a mano aquí y los fijan trazas doradas (G1 a G14).

## 1. Qué está fijado

[F] Nibble 0..15 (el 0/1 es el caso particular) y tiempo discreto determinista con retardo (0001 §2, provisional). [F] Tick síncrono de doble búfer (PR1; análogos TH3, MC8; la frase de TH2 contra compute-then-commit es opinión del autor y contradice TH3). [F] Todo componente activo tarda al menos 1 tick (PR1; FA1 solo lo afirma para los combinadores aritmético y decisor). [F] Cables = redes de retardo cero con fusión conmutativa y asociativa; ciclos legales (PR1, PR3, FA1). [F] Tick en tres fases (PR6). [F] Lógica solo en servidor, hilo del juego, dentro de `PreFactoryTick`, y versión propia en el guardado (runtime-api C1, C7, C17).

## 2. Modelo

### 2.1 Dominio [P]

`V = {0..15}`. Veracidad `t(x) = 1` si `x > 0`. Verdadero canónico `B(b) = 15` si `b`, si no 0 (como el repetidor, MC3, PR5). Solo enteros: ningún `float`.

### 2.2 Componentes, cables y redes

Una netlist es un conjunto finito de componentes (id opaco ≥ 1, `kind`, parámetros) y de cables. Un componente tiene pines de entrada `(c, j)` y, salvo el `Actuator`, un pin de salida `(c, out)`. Un **cable** es un par no ordenado de pines (la dirección la da el tipo de pin). Una **red** es una componente conexa del grafo de pines (un pin suelto es una red de un pin). Los postes de Satisfactory no son componentes: son concentradores que resuelve el adaptador (wires c3).

### 2.3 Estado, valor de red y transición

Cada componente tiene estado `s_c` y salida `out_c(s_c)` que solo depende de él (Moore). **Única excepción**: el `Sensor` no tiene estado y su salida es la muestra `e_c` del tick. Valor de una red `n`: `ν_n(X, e) = max({0} ∪ {salidas de los pines de salida de n})`. Una entrada lee el valor de su red; sin conductores vale 0. La transición es

`X' = F(X, e)`, con `s'_c = δ_c(s_c, u_c)`.

`δ_c` lee su estado y los valores de red de la instantánea, y escribe solo `s'_c`. Un valor de red depende de salidas, y una salida del estado o de la muestra, nunca de otro valor de red del mismo tick: **no existe lazo combinacional** y `F` está bien definida sin comprobar ciclos.

### 2.4 Un tick

El tick `τ → τ+1` tiene tres fases [F, PR6]:

1. **Muestreo.** El anfitrión entrega `e_τ`: un nibble por `Sensor`, por id ascendente.
2. **Cómputo.** `X_{τ+1} = F(X_τ, e_τ)`. El orden de evaluación, de redes y de cables no es entrada de `F`.
3. **Aplicación.** El mando de cada `Actuator` `a` es `c_{τ+1}(a) = o_a` en `X_{τ+1}`; el adaptador lo aplica por id ascendente.

Las ediciones de la netlist (02 §5) se aplican en el **límite de tick**, antes de la fase 1; no son una fase.

**Convención de filas.** Toda traza muestra en la fila `τ` la muestra `e_τ`, el estado `X_τ` (con las ediciones del límite `τ` ya aplicadas) y los mandos `o` de los actuadores en `X_τ` (`commands()`, 02 §2), no lo que se entrega al mundo (02 §7.2). Es la de theory (theory/06 Paso 1, regla 3; `Circuit` de `check_B.py`; `src` de `check_C.py`), así que sus tablas sirven de goldens sin desplazar filas (T14).

### 2.5 Latencia, en filas y en llamadas

Pesos `w`: `Const`, `Sensor`, `Clock` 0; `Gate`, `Compare`, `Latch`, `Toggle`, `Actuator` 1; `Delay(d)` `d`. Una muestra tomada en `τ` llega al mando del actuador en la fila `τ + W`, con `W` la suma de pesos del camino (actuador incluido). Con un paso por llamada, el adaptador lo aplica al final de la llamada `τ + W − 1`: `W − 1` llamadas después de la que muestreó.

| Camino | Σ pesos sin el actuador | Filas `W` | Llamadas `W−1` |
|---|---|---|---|
| sensor → actuador | 0 | 1 | 0 |
| sensor → NOT → actuador | 1 | 2 | 1 |
| 2 sensores → AND → `Delay(2)` → actuador | 3 | 4 | 3 |

La segunda fila es el golden G1 (02 §6.4); la tercera se deriva en 9.2. El bucle actuador → mundo → sensor dura al menos 1 tick (PR6).

## 3. Catálogo v0 [P]

`u0, u1…` son los valores de red de las entradas; `seed`, el dato de creación (§6.1).

| Kind | Entradas | Parámetros | Estado | Salida | Transición `s'` |
|---|---|---|---|---|---|
| `Const(v)` | 0 | `v` 0..15 | ninguno | `v` | no cambia |
| `Sensor` | 0 | ninguno | ninguno | `e` (saturada a 15) | no cambia |
| `Actuator` | `u0` | ninguno | `o` en V | sin pin de salida | `o' = u0` |
| `Gate(op,inv,k)` | `u0..u(k-1)`, k 1..8 | `op` {And, Or, Xor}, `inv` | `s` {0,15} | `s` | `g` = And: todas veraces; Or: alguna; Xor: nº impar. `s' = B(g xor inv)` |
| `Delay(d)` | `u0` | `d` 1..255 | `(f1..fd)` en V^d | `f1` | `(f1..fd)' = (f2..fd, u0)` |
| `Compare(m)` | `u0` = A (trasera), `u1` = B (lateral) | `m` {Compare, Subtract} | `s` en V | `s` | Compare: `A` si `B ≤ A`, si no 0. Subtract: `max(A−B, 0)` |
| `Latch` | `u0` = S, `u1` = R | ninguno | `q` {0,15} | `q` | `0` si `t(R)`; si no `15` si `t(S)`; si no `q` |
| `Toggle` | `u0` | ninguno | `(q,p)`, `q` {0,15}, `p` {0,1} | `q` | `r = t(u0) ∧ ¬p`; `q' = 15−q` si `r`, si no `q`; `p' = t(u0)` |
| `Clock(P,H)` | 0 | `P` 2..4096, `H` 1..P−1 | `c` en [0,P) | 15 si `c ≥ P−H`, si no 0 | `c' = (c+1) mod P` |

- `NOT = Gate(Or, inv, 1)`; NAND, NOR y XNOR son And, Or y Xor con `inv`. `Gate` con `k = 1` sin `inv` es un **digitalizador** (restaura a 0/15); `Delay` pasa el nibble intacto. Un AND de dos señales exige dos puertos: dos cables al mismo puerto hacen un OR por fusión.
- `Compare` es el comparador de Minecraft (MC5); "el mayor de los laterales" sale de la fusión `max` en B. Con `Const(k)`, `k ≥ 1`, en B detecta umbral: veraz si y solo si `A ≥ k`.
- `Delay(d)` equivale por definición a `d` `Delay(1)` en cadena: `out(τ+d) = in(τ)`. FIFO exacto: ningún pulso se pierde, a diferencia del repetidor de Java (MC3).
- `Latch` es de **reset dominante** [P]: con S y R veraces gana R (salida determinista al "S=R=1 indefinido", EN1).
- `Toggle` conmuta con cada flanco de subida **visto en su pin** (`p` es la veracidad del tick anterior): es el flip-flop T con T=1 de theory/06. `Clock` arranca en bajo con fase 0 como el temporizador de theory/06 Paso 3: `P=8, H=4` da `____####____####`.
- Recetas (theory/06 Pasos 2, 6, 7): detector de flanco `AND(x, NOT x)`, anillo `NOT` + `Delay(m)` de periodo `2(m+1)`, alargador de pulso. El flip-flop D maestro-esclavo (theory/04 Paso 6) y el detector 101 (theory/07) son el criterio de que el catálogo basta (T14).
- Fuera de v0: aritmética y operaciones por bit (PR5), flip-flops con reloj, contadores, sensores de pulso (del adaptador, 02 §7) y canales inalámbricos (en el núcleo son cables).

## 4. Fusión de red: `max` [P]

La alternativa es la suma saturada `min(15, a+b)`, cuyo único precedente verificado es la red aditiva de Factorio (FA1); el OR cableado de Oxygen Not Included es secundario (OT1). Se elige `max` por méritos: es conmutativa, asociativa, **idempotente**, de identidad 0 y cerrada en 0..15, y `t(a max b) = t(a) ∨ t(b)` (un OR cableado sin retardo). Con suma, duplicar un conductor cambia el valor (dos de 9 dan `min(15,18) = 15`); con `max` dan 9 (G6). Unir cables solo sube la red hasta su mayor conductor: no hay sumador oculto y el número de cables no cambia el significado. Sumar o contar será un componente con retardo visible (`Sum`, fuera de v0).

## 5. Retardo cero, ciclos y lint [P]

**No existe ningún componente de retardo cero**: todo componente con entradas pesa al menos 1; `Const`, `Sensor` y `Clock` no tienen entradas. Por eso la corrección no comprueba ciclos y los ciclos son legales: son máquinas de estados finitas y con entradas constantes toda traza es finalmente periódica (PR3, T8). No hay iteración hasta converger ni tope tipo Verilator (TH5): un tick cuesta siempre lo mismo. Si algún día hubiera retardo cero, regiría la política de Lustre (TH6; el algoritmo exacto no se atribuye a Lustre) con SCC más autolazo sobre el subgrafo de retardo cero (TH7), y sería `semanticsVersion = 2`.

El **lint** (`analyze`, 02 §2) informa, no rechaza. Trabaja sobre el **grafo inducido por redes**: arista `c → c'` si `(c, out)` está en la misma red que una entrada de `c'`. Informa de grupos de realimentación (SCC con más de un nodo o con autoarista; Tarjan **iterativo**, TH7), de entradas sin conductor (leen 0) y de redes con varios conductores.

Como una red une todos sus lectores con todos sus conductores, **añadir un conductor puede crear un lazo sin cable de vuelta** (G14): sensor `A`, NOT `B`, actuador `C`; cables `out(A)–in(B,0)`, `in(C,0)–out(A)`, `in(C,0)–out(B)`. Los cuatro pines son una sola red: `B' = NOT(max(e, B))`. Con `e = 0` y `B` en reposo (15): `X1.B = NOT(max(0,15)) = 0`, `X2.B = NOT(max(0,0)) = 15`: periodo 2. Con `e = 9`: `X1.B = NOT(max(9,15)) = 0`, `X2.B = NOT(max(9,0)) = 0` para siempre. `C` lee `max(9, B)` de la red: su **mando** vale 15 en la fila `X1` (lo lee en `X0`, con `B = 15`) y 9 desde `X2`; la red ya vale 9 en `X1`, porque allí `B = 0`.

## 6. Memoria y estado inicial [P]

**Por qué nativas.** Un latch SR de puertas soltado desde S=R=1 oscila con periodo 2 (theory/04 Paso 2; PR3), y un pulso de S de un tick lo deja oscilando (theory/04 Paso 3, Tabla 4.4); el nativo memoriza todo pulso de al menos 1 tick y define S=R=1 (9.3). `Toggle` y `Clock` los recomienda la investigación (PR3; OT1); `Clock(P,H)` da cualquier periodo, no solo los pares del anillo.

### 6.1 Seed

Un componente creado (construcción, pegado, edición) arranca en su **seed**; uno **cargado** se restaura verbatim. `replace` también crea: reinicia el componente a su seed, que sin dato explícito es el de reposo del `Spec` **nuevo** (no el del anterior); un `Toggle` vuelve a `p ← 1`. `validate` rechaza (`BadSpec`) todo seed fuera de dominio y todo parámetro no usado distinto de 0.

| Kind | Seed válido | Efecto | Defecto (reposo) |
|---|---|---|---|
| `Const`, `Sensor` | {0} | ninguno (sin estado) | 0 |
| `Gate` | {0,15} | `s ← seed` | 15 si `inv`, si no 0 (salida con todas las entradas a 0) |
| `Latch` | {0,15} | `q ← seed` | 0 |
| `Toggle` | {0,15} | `q ← seed` y **`p ← 1`** | 0 |
| `Delay` | 0..15 | las `d` ranuras ← seed | 0 |
| `Compare`, `Actuator` | 0..15 | `s` u `o` ← seed | 0 |
| `Clock` | 0..P−1 | `c ← seed` | 0 |

### 6.2 Flancos del `Toggle`

`p ← 1` hace que el primer tick nunca detecte un flanco, como `check_C.py` (el reloj anterior de t=0 se toma igual al de t=0). Con golden (T11):

- **G8 `toggle_new_high`**: creado y cableado en el mismo lote a una red alta, no conmuta. Seed 0, entrada 15: `r = t(15) ∧ ¬1 = 0`, `q' = 0`, `p' = 1`.
- **G9 `toggle_edit_connect`**: creado sin cable lee 0 y tras un tick tiene `p = 0`; si luego se cablea a una red alta, ve un 0→15 real y **conmuta una vez** (`r = t(15) ∧ ¬0 = 1`, `q' = 15`). Conectar un cable es un flanco. No hay "rearme" en las ediciones: es local y predecible; quien no quiera ese flanco crea y cablea en un lote (D05).

### 6.3 Transitorio de construcción

Un componente nuevo en reposo puede emitir transitorios. Sensor en 15 → NOT `g1` → NOT `g2`, ambos nacidos en 15: `X0 = (15,15)`; tick 0: `g1' = NOT(15) = 0`, `g2' = NOT(g1=15) = 0`, `X1 = (0,0)`; tick 1: `g2' = NOT(0) = 15`, `X2 = (0,15)`. La salida de `g2` vale 15, 0, 15 aunque su estable sea 15: un actuador se ordenaría 15, 0, 15. Ninguna política de seed lo evita en general; la mitigación es la guarda de asentamiento del adaptador (02 §7.5, D16), basada en `ℓ` (§8).

## 7. Límites, errores y determinismo [P]

Toda operación inválida o que excede un límite **se rechaza entera** con un `Status` (02 §2); nunca se trunca ni se aplica a medias. Sin excepciones ni RTTI (los módulos de Unreal suelen compilarse sin ellas: no verificado, spike). Límites por defecto, provisionales: 8192 componentes, 32768 cables, aridad 8, `maxNetPins` 256 (**fan-in lógico**: pines por red), `Delay` 255 y 65536 ranuras de `Delay` en total, periodo de `Clock` 4096. El **fan-in físico** (vínculos por conector: 1 en el nativo, postes de 4, 7 y 10; wires c10, c11) es del adaptador. Se recalibran con el benchmark sin Unreal ([03](03-pruebas.md) §8). Qué `Status` da un parámetro que supera el rango del catálogo (§3: `k` 1..8, `d` 1..255, `P` 2..4096) o el tope de `Limits` correspondiente, `BadSpec` o `LimitExceeded`: **Pendiente de arbitraje (ver ambiguities.md B2)**.

Determinismo entre plataformas: solo enteros de ancho fijo (`A − B` en `int`, recortado a 0), ni un `float`, ni en el planificador; iteración por id ascendente, nunca contenedores no ordenados, orden por puntero ni `std::hash`; serialización y huellas byte a byte, little-endian; PRNG propio en los tests. Fuera de lo garantizado: el float→nibble del adaptador y el no-determinismo del juego (PR7).

## 8. Teorema de asentamiento por cono

**Definiciones.** El *cono* de un `Actuator` `a` es `a` más sus ancestros en el grafo inducido por redes (§5, autoaristas incluidas). `ℓ(c) = w(c) + max{ℓ(c') : c' → c}` (máximo vacío = 0); `L_a = ℓ(a)`. `settlingBounds` (02 §2) devuelve `L_a`, o vacío si no aplica.

**Teorema [P: la forma débil es PR2, sin fuente primaria; la fuerte es nuestra].** Sea `a` un actuador cuyo cono es acíclico y contiene solo `Const`, `Sensor`, `Gate`, `Compare`, `Delay` y `a`.

- **(a) Memoria que se desvanece.** Para dos estados iniciales cualesquiera del cono y la misma `e`, el estado de cada `c` coincide para `τ ≥ ℓ(c)`; en particular `c(a)` coincide para `τ ≥ L_a`.
- **(b) Punto fijo.** Si `e = e*` es constante desde `τ0`, el estado de `c` es `z(c)` para `τ ≥ τ0 + ℓ(c)`, con `z` la evaluación **nivelada de retardo cero**: `z` de un `Sensor` es `e*`, el de un `Const` es `v`, y el resto es la salida de `δ_c` con entradas `u* = max` de los `z` de los conductores; en un `Delay` todas las ranuras valen `u*`.

*Prueba de (a), por inducción en orden topológico.* `Const` y `Sensor` no tienen estado. `Gate`, `Compare` y `Actuator` (peso 1): `s'` solo depende de las entradas; si las salidas de los conductores coinciden desde `max ℓ(c')`, `s` coincide desde `max ℓ(c') + 1 = ℓ(c)`. `Delay(d)`: para `τ ≥ d`, la ranura `f_i` guarda `u0(τ−d−1+i)`, valores de `u0` en `[τ−d, τ−1]`, que coinciden si `τ ≥ max ℓ(c') + d = ℓ(c)`. *(b)*: el estado con todo igual a `z` es punto fijo bajo `e*`; (a) desde `τ0`, comparando la ejecución real con la que arranca en él, da el resultado. ∎

**Cota alcanzada (G13).** Sensor en 0, `g1 → g2 → g3` NOT con seed 0: `X0 = (0,0,0)`; `X1 = (F,F,F)`; `X2 = (NOT 0, NOT F, NOT F) = (F,0,0)`; `X3 = (F, NOT F, NOT 0) = (F,0,F)`; `X4 = X3`. El punto fijo nivelado es `(NOT 0, NOT F, NOT 0) = (F,0,F)` y `ℓ(g3) = 3`: `X2 ≠ z` y `X3 = z`.

**Lo que NO se afirma.** Nada con `Latch`, `Toggle`, `Clock` ni ciclos en el cono. `L` es cota **superior**, no ajustada para toda red. Ausencia de transitorios: en los primeros `L` ticks hay glitches y el detector de flancos depende de ellos. Nada sobre sensores que oscilan. Ni paridad con Minecraft ni equivalencia con Verilog o VHDL (de VHDL solo se toma actualizar todo antes de ejecutar nada, TH3).

## 9. Tres ejemplos resueltos

### 9.1 Anillo de tres NOT (G2)

`g1, g2, g3 = Gate(Or, inv, 1)`; cables `out(g3)–in(g1,0)`, `out(g1)–in(g2,0)`, `out(g2)–in(g3,0)`. `g1' = NOT g3`, `g2' = NOT g1`, `g3' = NOT g2`, leyendo la instantánea. Seeds `(F,0,F)`, el `(1,0,1)` de theory/06 Paso 2:

```
X0=(F,0,F): g1'=NOT(F)=0  g2'=NOT(F)=0  g3'=NOT(0)=F  -> X1=(0,0,F)
X1=(0,0,F): g1'=NOT(F)=0  g2'=NOT(0)=F  g3'=NOT(0)=F  -> X2=(0,F,F)
X2=(0,F,F): g1'=NOT(F)=0  g2'=NOT(0)=F  g3'=NOT(F)=0  -> X3=(0,F,0)
X3=(0,F,0): g1'=NOT(0)=F  g2'=NOT(0)=F  g3'=NOT(F)=0  -> X4=(F,F,0)
X4=(F,F,0): g1'=NOT(0)=F  g2'=NOT(F)=0  g3'=NOT(F)=0  -> X5=(F,0,0)
X5=(F,0,0): g1'=NOT(0)=F  g2'=NOT(F)=0  g3'=NOT(0)=F  -> X6=(F,0,F)=X0
```

Periodo 6 = 2n con n = 3 (theory/06): el único defecto (`g1`, con `s ≠ NOT(entrada)`) avanza una puerta por tick y vuelve invertido tras `n` pasos, así que hacen falta dos vueltas. Con seeds `(0,0,0)`: `X1 = (F,F,F)`, `X2 = (0,0,0)`, periodo 2. Los 8 estados forman un ciclo de 6 y otro de 2 (theory/07 Paso 5). Para n = 9, theory/06 cuenta 2 estados de periodo 2, 6 de periodo 6 y 504 de periodo 18 (T8).

### 9.2 AND de dos sensores y `Delay(2)` (G3)

`sA, sB` → `and = Gate(And, k=2)` → `dl = Delay(2)` → `act`; cables `out(sA)–in(and,0)`, `out(sB)–in(and,1)`, `out(and)–in(dl,0)`, `out(dl)–in(act,0)`. Ecuaciones: `and' = 15·t(eA)·t(eB)`; `(f1,f2)' = (f2, and)`; `o' = f1`. Todo en 0; `eA = 9` en `τ = 1..3`, `eB = 5` en `τ = 2..4`, resto 0. Ambas son veraces solo en `τ = 2, 3`, así que `and = F` en `X3, X4` (15 aunque las muestras sean 9 y 5); el FIFO lo retrasa dos ticks (`f1 = F` en `X5, X6`) y el actuador uno más (`o = F` en `X6, X7`).

```
τ   eA eB | and  f1 f2 | o      (fila τ = X_τ; e_τ se consume al pasar a τ+1)
0    0  0 |  0    0  0  | 0
1    9  0 |  0    0  0  | 0
2    9  5 |  0    0  0  | 0
3    9  5 |  F    0  0  | 0
4    0  5 |  F    0  F  | 0
5    0  0 |  0    F  F  | 0
6    0  0 |  0    F  0  | F
7    0  0 |  0    0  0  | F
8    0  0 |  0    0  0  | 0
```

Paso `3 → 4`: `and' = 15·1·1 = F`; `(f1,f2)' = (f2, and) = (0, F)`; `o' = f1 = 0`. El primer tick con ambas muestras veraces es `τ = 2` y el mando sube en la fila 6: `W = 1 + 2 + 1 = 4` filas, 3 llamadas. El pulso de 2 ticks sale intacto (el repetidor de Java podría perderlo, MC3).

### 9.3 Latch nativo frente a dos NOR cruzadas (G4)

Entradas por sensores `S` y `R`. NOR: `Q' = NOR(R, Qn)`, `Qn' = NOR(S, Q)`, seeds `(Q,Qn) = (0,F)`. Nativo: seed 0. Las filas coinciden con theory/04 sin desplazar.

**A. S=R=F en `τ = 0..2`, soltados a la vez en 3** (theory/04 Paso 2):

```
X0=(0,F) S=R=F: Q'=NOR(F,F)=0  Qn'=NOR(F,0)=0  -> X1=(0,0)
X1=(0,0) S=R=F: Q'=NOR(F,0)=0  Qn'=NOR(F,0)=0  -> X2=(0,0); igual en τ=2 -> X3=(0,0)
X3=(0,0) S=R=0: Q'=NOR(0,0)=F  Qn'=NOR(0,0)=F  -> X4=(F,F)
X4=(F,F) S=R=0: Q'=NOR(0,F)=0  Qn'=NOR(0,F)=0  -> X5=(0,0)   y así para siempre

τ        0123456789
S=R      ###_______
Q  (NOR) ____#_#_#_
Qn (NOR) #___#_#_#_
q  nativo __________     (R domina en τ=0..2; luego mantiene 0)
```

**B. S=F solo en `τ = 1`** (theory/04 Paso 3, Tabla 4.4). NOR: `X1 = (0,F)`; tick 1: `Q' = NOR(0,F) = 0`, `Qn' = NOR(F,0) = 0`, `X2 = (0,0)`; tick 2: `Q' = Qn' = NOR(0,0) = F`, `X3 = (F,F)` y oscila. Nativo: tick 1: S veraz, R no, `q' = 15`, y mantiene.

Las filas son `Q = ___#_#_#`, `Qn = ##_#_#_#` y `q = __######`, como en theory/04 Paso 3.

El nativo no oscila nunca y memoriza todo pulso de al menos 1 tick: por eso `Latch` es nativo.
