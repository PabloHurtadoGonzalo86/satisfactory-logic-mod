# 04 · Memoria: latches y flip-flops

## Objetivo

Entender cómo un circuito de puertas puede guardar un bit (realimentación positiva: un bucle con un número par de inversiones) y construir con puertas los biestables básicos: el latch SR, el latch D, el flip-flop D disparado por flanco y el flip-flop T.  
Cada tabla, traza y forma de onda de este documento sale de un simulador de retardo unitario escrito para la ocasión (un script de Python del repositorio, `docs/theory/check_B.py`, que además comprueba que el texto coincide con lo que simula), no de memoria.

## Idea intuitiva

Hasta ahora todo era combinacional: la salida depende solo de las entradas de este momento, como una función pura de PHP. Una función pura no recuerda nada. Para recordar hace falta **estado**, y en un circuito el estado se consigue con una idea muy simple: **devolver la salida a la entrada** (realimentación). La salida de ahora depende de la salida de hace un momento, como una variable que se reasigna a partir de su propio valor anterior.

Empieza por lo más pequeño: dos puertas NOT en anillo, cada una alimentando a la otra.

```text
A(t+1) = NOT(B(t))
B(t+1) = NOT(A(t))
```

**Tabla 4.1. Anillo de dos NOT, desde cada estado inicial**

| AB en t=0 | t=0 | t=1 | t=2 | t=3 | t=4 | veredicto |
|---|---|---|---|---|---|---|
| 00 | 00 | 11 | 00 | 11 | 00 | oscila (periodo 2) |
| 01 | 01 | 01 | 01 | 01 | 01 | estable |
| 10 | 10 | 10 | 10 | 10 | 10 | estable |
| 11 | 11 | 00 | 11 | 00 | 11 | oscila (periodo 2) |

Lee cada fila de izquierda a derecha: cada columna se calcula a partir de la anterior (el porqué exacto, en el Paso 0).

- Con AB=01 y AB=10 el anillo se queda quieto: son **dos estados estables**, es decir, **un bit guardado** (si te vale la convención, A es el bit y B su negado).
- Con AB=00 y AB=11 no se asienta: las dos puertas cambian a la vez, una y otra vez. Esa oscilación vuelve a aparecer enseguida.
- ¿Por qué hay dos estados estables? Porque el bucle tiene **dos inversiones**: cada NOT invierte, y al dar la vuelta completa al anillo los dos cambios se anulan, así que el anillo puede quedarse con el valor que tiene. Eso es **realimentación positiva**: el bucle refuerza lo que ya hay, y de ahí sale la memoria. Con un número impar de inversiones la vuelta completa invertiría el valor y el anillo se contradiría a sí mismo: no tendría ningún estado estable y oscilaría (lo verás en el documento 06). Comprobado con el simulador: un anillo de 2, 4 y 6 puertas NOT tiene 2 estados estables (los dos patrones alternados); uno de 1, 3 y 5 puertas NOT no tiene ninguno y todos sus estados oscilan.
- ¿Y el retardo? No crea la memoria: solo hace que el modelo esté bien definido, porque así cada puerta calcula su salida a partir del instante anterior y siempre existe un estado siguiente, incluso en un bucle que no tiene ningún valor consistente.
- Falta lo importante: este anillo guarda un bit pero **no hay forma de escribirlo**. La solución es cambiar cada NOT por una NOR (o una NAND) para tener una entrada de control en cada puerta. Eso es un **latch**. En español, a los circuitos con dos estados estables se les llama en general **biestables**: el *latch* responde al nivel de sus entradas mientras duran, y el *flip-flop* (Paso 6) solo en el instante en que sube un reloj (el reloj y su flanco se definen al final del Paso 5).

## Explicación paso a paso

### Paso 0. Notación y el modelo temporal

> **Notación de la serie.** Bits como `0b0110`, hexadecimal como `0x6`; operadores AND, OR, NOT, XOR, NAND, NOR y XNOR. En fórmulas: `·` es AND, `+` es OR (no una suma), `¬` es NOT y `⊕` es XOR.

Para hablar de memoria hay que hablar de tiempo ("el valor de hace un momento"). Usaremos un modelo muy simple, pensado para enseñar:

1. **Retardo unitario.** Cada puerta tarda exactamente un paso en reaccionar: su salida en el instante t+1 depende solo de sus entradas en el instante t. Un "paso" no son segundos ni ticks del juego: es solo la unidad del modelo. (Ojo con la palabra: «Paso 3», con mayúscula y número, es un apartado de este documento; «paso», en minúscula, es siempre esta unidad de tiempo.)
2. **Evaluación síncrona en dos fases.** Aquí «síncrona» significa solo que todas las puertas se actualizan a la vez, en cada paso del modelo; todavía no hay ningún reloj (aparece en el Paso 5). En electrónica, «circuito síncrono» suele significar otra cosa: uno cuyos flip-flops cambian todos al ritmo de un mismo reloj (documento 06 en adelante). Para pasar de t a t+1 se hace así:
   - **Fase 1, calcular:** todas las puertas leen los valores del instante t y calculan su salida de t+1, sin escribir nada todavía.
   - **Fase 2, confirmar:** todos los valores nuevos se escriben a la vez.

Así se ve con el anillo de dos NOT empezando en AB=00 (cada bloque es una transición completa, con las dos fases):

```text
t=0 -> t=1
  fase 1 (calcular, leyendo solo el instante t=0):
    A(1) = NOT(B(0)) = NOT(0) = 1
    B(1) = NOT(A(0)) = NOT(0) = 1
  fase 2 (confirmar todo a la vez): A(1) = 1, B(1) = 1
t=1 -> t=2
  fase 1 (calcular, leyendo solo el instante t=1):
    A(2) = NOT(B(1)) = NOT(1) = 0
    B(2) = NOT(A(1)) = NOT(1) = 0
  fase 2 (confirmar todo a la vez): A(2) = 0, B(2) = 0
```

¿Por qué dos fases? Porque si cada puerta usara el valor recién calculado de su vecina, el resultado dependería del orden en que las recorras. Contraejemplo: si evaluaras las puertas una detrás de otra, usando el A que acabas de calcular, desde AB=00 saldría AB=10 en t=1; con las dos fases sale AB=11. Con las dos fases el orden de las puertas da igual.

> **Aviso: modelo didáctico.** Esto es una herramienta para razonar y para comprobar los ejemplos. El simulador real del mod se decidirá más adelante, y nada de lo que sigue promete cómo funcionará.

Cómo se leen las trazas de este documento: la fila de un instante t muestra las entradas y las salidas en t, y la fila siguiente se obtiene aplicando las dos fases a esa.

### Paso 1. Latch SR con NOR

Cambia cada NOT del anillo por una NOR. Una NOR vale 1 solo cuando sus dos entradas valen 0, así que un 1 en cualquiera de sus entradas obliga a la salida a 0. Eso da dos entradas de control:

- **S** (set, "poner a 1") entra en la puerta que produce Qn: con S=1, Qn baja a 0 y entonces la otra puerta ve Qn=0 y sube Q a 1.
- **R** (reset, "poner a 0") entra en la puerta que produce Q: con R=1, Q baja a 0 y Qn sube.

```text
         +-----+                     +-----+
  R -----|     |              S -----|     |
         | NOR |----- Q              | NOR |----- Qn
  Qn ----|     |              Q -----|     |
         +-----+                     +-----+
```

En el dibujo, las etiquetas repetidas son el mismo cable: la salida Q de la puerta de la izquierda es la entrada Q de la de la derecha, y la salida Qn de la derecha vuelve a la izquierda. En el modelo de dos fases son estas dos ecuaciones:

```text
Q(t+1) = NOR(R(t), Qn(t))
Qn(t+1) = NOR(S(t), Q(t))
```

Lo que hace, medido con el simulador desde cada estado inicial posible y con las entradas fijas:

**Tabla 4.2. Latch SR con NOR: comportamiento**

| S | R | Q antes | Q después | Qn después | pasos hasta quedar quieto | modo |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 1 | 0 | mantener |
| 0 | 0 | 1 | 1 | 0 | 0 | mantener |
| 0 | 1 | 0 | 0 | 1 | 0 | reset |
| 0 | 1 | 1 | 0 | 1 | 2 | reset |
| 1 | 0 | 0 | 1 | 0 | 2 | set |
| 1 | 0 | 1 | 1 | 0 | 0 | set |
| 1 | 1 | 0 | 0 | 0 | 1 | PROHIBIDO |
| 1 | 1 | 1 | 0 | 0 | 1 | PROHIBIDO |

- **S=0, R=0: mantiene.** Esto es la memoria: no hay entrada activa y Q conserva lo que tenía.
- **S=1, R=0: set.** Q acaba en 1.
- **S=0, R=1: reset.** Q acaba en 0.
- **S=1, R=1: prohibido** (lo ves en el Paso 2).

La columna "pasos hasta quedar quieto" es lo que tarda el latch en asentarse; vale 0 cuando ya estaba en ese estado.

Ahora una traza con un pulso de set y, más tarde, uno de reset. Recuerda: la fila de t da las entradas de t y las salidas de t.

**Tabla 4.3. Traza del latch SR: set, mantener, reset, mantener**

| t | S(t) | R(t) | Q(t) | Qn(t) |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 1 |
| 1 | 1 | 0 | 0 | 1 |
| 2 | 1 | 0 | 0 | 0 |
| 3 | 0 | 0 | 1 | 0 |
| 4 | 0 | 0 | 1 | 0 |
| 5 | 0 | 0 | 1 | 0 |
| 6 | 0 | 1 | 1 | 0 |
| 7 | 0 | 1 | 0 | 0 |
| 8 | 0 | 0 | 0 | 1 |
| 9 | 0 | 0 | 0 | 1 |
| 10 | 0 | 0 | 0 | 1 |
| 11 | 0 | 0 | 0 | 1 |

Con el pulso de set en t=1 y t=2: Qn baja primero (en t=2) y Q sube un paso después (en t=3); con el pulso de reset en t=6 y t=7: Q baja en t=7 y Qn sube en t=8. Entre los dos pulsos (con S=R=0 en t=3 a t=5) el latch mantiene (Q, Qn) = (1, 0).

Las dos primeras transiciones del set, con las dos fases escritas, para ver que cada puerta lee solo el instante anterior (por ejemplo, el Qn del instante siguiente usa el Q del instante actual, nunca el Q del siguiente, que aún no existe):

```text
t=1 -> t=2
  fase 1 (calcular, leyendo solo el instante t=1):
    Q(2)  = NOR(R(1), Qn(1)) = NOR(0, 1) = 0
    Qn(2) = NOR(S(1), Q(1)) = NOR(1, 0) = 0
  fase 2 (confirmar todo a la vez): Q(2) = 0, Qn(2) = 0
t=2 -> t=3
  fase 1 (calcular, leyendo solo el instante t=2):
    Q(3)  = NOR(R(2), Qn(2)) = NOR(0, 0) = 1
    Qn(3) = NOR(S(2), Q(2)) = NOR(1, 0) = 0
  fase 2 (confirmar todo a la vez): Q(3) = 1, Qn(3) = 0
```

### Paso 2. El estado prohibido: S=R=1

¿Y si S=1 y R=1 a la vez? Las dos NOR tienen un 1 en una entrada, así que **las dos salidas bajan a 0**: Q=Qn=0. Eso rompe la promesa de que Qn sea lo contrario de Q. Mientras mantengas S=R=1 el latch está tranquilo en ese estado; el problema llega al soltar.

Primera traza: S y R valen 1 durante las primeras columnas y se sueltan en el mismo instante; el latch parte de Q=0 y Qn=1:

```text
t   0 1 2 3 4 5 6 7 8 9
S   1 1 1 0 0 0 0 0 0 0
R   1 1 1 0 0 0 0 0 0 0
Q   0 0 0 0 1 0 1 0 1 0
Qn  1 0 0 0 1 0 1 0 1 0
```

Con S=R=1 el latch llega en un solo paso a Q=Qn=0 (t=1 en la primera traza). Si sueltas S y R en el mismo instante (t=3), el estado pasa por 00 -> 11 -> 00 -> 11 -> ... con periodo 2 y no se estabiliza nunca (el estado se repite exactamente cada 2 pasos y las entradas ya no cambian).

Es exactamente el comportamiento del anillo de dos NOT que empezó en 00 (Tabla 4.1): con S=R=0 el latch es ese anillo, y el anillo desde 00 no se asienta.

Si no los sueltas a la vez, el resultado depende de quién se suelta antes. Segunda traza (S se suelta antes que R) y tercera (R antes que S):

```text
t   0 1 2 3 4 5 6 7 8 9
S   1 1 1 0 0 0 0 0 0 0
R   1 1 1 1 1 1 0 0 0 0
Q   0 0 0 0 0 0 0 0 0 0
Qn  1 0 0 0 1 1 1 1 1 1
```

```text
t   0 1 2 3 4 5 6 7 8 9
S   1 1 1 1 1 1 0 0 0 0
R   1 1 1 0 0 0 0 0 0 0
Q   0 0 0 0 1 1 1 1 1 1
Qn  1 0 0 0 0 0 0 0 0 0
```

Si sueltas S antes que R, el latch acaba en Q=0, Qn=1 (gana R). Si sueltas R antes que S, acaba en Q=1, Qn=0 (gana S).

**Qué pasaría en un circuito real** (conocimiento general de electrónica; este simulador no lo modela): las dos puertas nunca son exactamente idénticas, así que la simetría perfecta del modelo no se mantiene. El latch puede oscilar un instante o quedarse indeciso, ni en 0 ni en 1 (eso es la **metaestabilidad**), y acaba en uno de los dos estados, pero no puedes saber en cuál ni cuánto tardará en decidirse: en teoría no hay un tiempo máximo que puedas garantizar. En el modelo la oscilación es perfecta porque las dos puertas son idénticas por definición. En los dos mundos la regla de diseño es la misma: **S=R=1 no se pide nunca**. El latch D del Paso 5 lo evita en régimen estable (con un matiz que verás allí).

### Paso 3. Un pulso demasiado corto

Segunda trampa del mismo latch, esta vez en el tiempo: ¿cuánto tiene que durar un pulso de S o de R para que el latch lo memorice? El simulador dice:

**Tabla 4.4. Ancho del pulso de set o de reset**

| ancho del pulso (pasos) | pulso en S, partiendo de Q=0 | pulso en R, partiendo de Q=1 |
|---|---|---|
| 1 | oscila (periodo 2) | oscila (periodo 2) |
| 2 | queda en Q=1, Qn=0 | queda en Q=0, Qn=1 |
| 3 | queda en Q=1, Qn=0 | queda en Q=0, Qn=1 |

Traza del pulso más corto (S=1 solo durante un paso, partiendo de Q=0 y Qn=1):

```text
t   0 1 2 3 4 5 6 7
S   0 1 0 0 0 0 0 0
R   0 0 0 0 0 0 0 0
Q   0 0 0 1 0 1 0 1
Qn  1 1 0 1 0 1 0 1
```

Con S=1 solo en t=1: Qn baja a 0 en t=2, pero en t=2 S ya vale 0 y nada mantiene a Qn abajo. En t=3 Q sube (porque Qn valía 0) y Qn también sube (porque Q valía 0): el latch está en Q=Qn=1 y desde ahí entra en la misma oscilación de periodo 2 del estado prohibido.

Regla práctica para este modelo: las entradas de un latch tienen que durar lo bastante (aquí, el ancho de la fila que sí memoriza). Un pulso que llega y se va antes de que el latch reaccione puede dejarlo oscilando.

### Paso 4. Latch SR con NAND

El mismo anillo hecho con NAND también guarda un bit, pero con las entradas **al revés**: una NAND vale 0 solo cuando sus dos entradas valen 1, así que ahora el 0 es el que "actúa". Por eso las entradas se llaman Sn y Rn (la n es de "negado": activas a nivel bajo). En reposo valen 1, y 0 significa "hazlo".

```text
         +------+                    +------+
  Sn ----|      |             Rn ----|      |
         | NAND |----- Q             | NAND |----- Qn
  Qn ----|      |             Q -----|      |
         +------+                    +------+
```

```text
Q(t+1) = NAND(Sn(t), Qn(t))
Qn(t+1) = NAND(Rn(t), Q(t))
```

**Tabla 4.5. Latch SR con NAND: comportamiento**

| Sn | Rn | Q antes | Q después | Qn después | pasos hasta quedar quieto | modo |
|---|---|---|---|---|---|---|
| 1 | 1 | 0 | 0 | 1 | 0 | mantener |
| 1 | 1 | 1 | 1 | 0 | 0 | mantener |
| 1 | 0 | 0 | 0 | 1 | 0 | reset |
| 1 | 0 | 1 | 0 | 1 | 2 | reset |
| 0 | 1 | 0 | 1 | 0 | 2 | set |
| 0 | 1 | 1 | 1 | 0 | 0 | set |
| 0 | 0 | 0 | 1 | 1 | 1 | PROHIBIDO |
| 0 | 0 | 1 | 1 | 1 | 1 | PROHIBIDO |

El estado prohibido ahora es Sn=Rn=0, y deja **Q=Qn=1** (en el NOR dejaba 00). Trazas: un pulso de set (Sn=0) y uno de reset (Rn=0), y después el estado prohibido soltado a la vez:

```text
t   0 1 2 3 4 5 6 7 8 9 10 11
Sn  1 0 0 1 1 1 1 1 1 1 1 1
Rn  1 1 1 1 1 1 0 0 1 1 1 1
Q   0 0 1 1 1 1 1 1 0 0 0 0
Qn  1 1 1 0 0 0 0 1 1 1 1 1
```

```text
t   0 1 2 3 4 5 6 7 8 9
Sn  0 0 0 1 1 1 1 1 1 1
Rn  0 0 0 1 1 1 1 1 1 1
Q   0 1 1 1 0 1 0 1 0 1
Qn  1 1 1 1 0 1 0 1 0 1
```

Con Sn=Rn=0 el latch NAND llega a Q=Qn=1 (t=1); al soltar los dos a la vez (t=3) oscila 11 -> 00 -> 11 -> 00 -> ... con periodo 2.

Misma historia que con NOR: sale una oscilación. Lo que cambia es el 0 y el 1 intercambiados.

### Paso 5. Latch D con habilitación

El latch SR tiene dos problemas: un estado prohibido y dos órdenes (set y reset) que tienes que coordinar. Se arreglan con una sola señal de datos **D** y una de habilitación **E** (enable):

- Con E=1 el latch copia D: el latch está **abierto** o transparente, y Q sigue a D.
- Con E=0 el latch no mira D: está **cerrado** y recuerda lo último que copió.

Construcción: un latch SR con NAND al que se le dan las órdenes a=NAND(D, E) (que es el Sn) y b=NAND(¬D, E) (que es el Rn). Como D y ¬D no pueden ser 1 a la vez, en régimen estable a y b nunca valen 0 a la vez: **el estado prohibido desaparece en régimen estable**. Durante un cambio de D, en cambio, puede asomar un instante muy breve (se ve en la traza de más abajo).

```text
          +------+                  +------+
  D ------|      |            a ----|      |
          | NAND |----- a           | NAND |----- Q
  E ------|      |           Qn ----|      |
          +------+                  +------+

          +------+                  +------+
  nD -----|      |            b ----|      |
          | NAND |----- b           | NAND |----- Qn
  E ------|      |            Q ----|      |
          +------+                  +------+

  D ----[NOT]---- nD
```

```text
nD(t+1) = NOT(D(t))
a(t+1) = NAND(D(t), E(t))
b(t+1) = NAND(nD(t), E(t))
Q(t+1) = NAND(a(t), Qn(t))
Qn(t+1) = NAND(b(t), Q(t))
```

**Tabla 4.6. Latch D con habilitación: comportamiento**

| E | D | Q antes | Q después | Qn después | pasos hasta quedar quieto | modo |
|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 1 | 1 | memoria (mantiene) |
| 0 | 0 | 1 | 1 | 0 | 1 | memoria (mantiene) |
| 0 | 1 | 0 | 0 | 1 | 1 | memoria (mantiene) |
| 0 | 1 | 1 | 1 | 0 | 1 | memoria (mantiene) |
| 1 | 0 | 0 | 0 | 1 | 0 | transparente (Q sigue a D) |
| 1 | 0 | 1 | 0 | 1 | 4 | transparente (Q sigue a D) |
| 1 | 1 | 0 | 1 | 0 | 3 | transparente (Q sigue a D) |
| 1 | 1 | 1 | 1 | 0 | 0 | transparente (Q sigue a D) |

En las filas con E=0 la columna "pasos hasta quedar quieto" cuenta también los cambios de las puertas internas (nD, a y b), aunque Q y Qn no se muevan; por eso no vale 0 como en la Tabla 4.2.

Traza con D cambiando mientras E=1 y, después, con E=0 (columnas: entradas, y las cinco puertas):

```text
 t  D E | nD a b Q Qn
 0  0 1 |  1 1 0 0  1
 1  0 1 |  1 1 0 0  1
 2  0 1 |  1 1 0 0  1
 3  1 1 |  1 1 0 0  1
 4  1 1 |  0 0 0 0  1
 5  1 1 |  0 0 1 1  1
 6  1 1 |  0 0 1 1  0
 7  1 1 |  0 0 1 1  0
 8  1 1 |  0 0 1 1  0
 9  0 1 |  0 0 1 1  0
10  0 1 |  1 1 1 1  0
11  0 1 |  1 1 0 1  0
12  0 1 |  1 1 0 1  1
13  0 1 |  1 1 0 0  1
14  0 1 |  1 1 0 0  1
15  0 0 |  1 1 0 0  1
16  0 0 |  1 1 1 0  1
17  0 0 |  1 1 1 0  1
18  1 0 |  1 1 1 0  1
19  1 0 |  0 1 1 0  1
20  1 0 |  0 1 1 0  1
21  1 0 |  0 1 1 0  1
22  1 0 |  0 1 1 0  1
23  1 0 |  0 1 1 0  1
```

Con E=0 (desde t=15) D cambia a 1 en t=18 y Q no se mueve: Q vale 0 en t=23. Q solo cambia en t = 5, 13, siempre mientras E=1.

Fíjate en la traza: en régimen estable a y b nunca valen 0 a la vez, pero **durante un cambio de D sí pueden**.

Cuando D sube (t=3), en t=4 las dos órdenes del latch interno (a y b) valen 0 a la vez y en t=5 Q y Qn valen 1 a la vez durante un paso. Cuando D baja (t=9) pasa algo parecido: Q y Qn valen 1 a la vez en t=12. Es un glitch (un valor falso breve) porque nD llega un paso tarde; al final Q acaba siendo D.

Sobre el arranque (qué pasa si el latch empieza en un estado raro):

- Comprobado con el simulador: con E=1 y D fija, desde cualquiera de los 32 estados internos posibles de sus 5 puertas el latch D acaba en Q=D (64 casos en total, con D=0 y D=1).
- Aviso: si el latch arranca con Q=Qn=1 y E=0 no tiene nada que lo corrija y oscila con periodo 2; basta mantener E=1 unos pasos con D fija para dejarlo consistente.

Un problema del latch D: mientras E=1 es transparente, así que si encadenas latches con la misma habilitación el dato se cuela a través de todos, sin control. Para que el dato avance una etapa por vez hace falta que el dato se capture **solo en un instante**, y ese instante lo marca un **reloj**: una señal, que llamaremos CLK, que alterna entre 0 y 1 de forma periódica (el documento 06 explica cómo se fabrica; aquí la damos hecha, como una entrada más). Su **periodo** es el número de pasos de un ciclo completo (CLK a 0 y luego a 1); si el reloj es simétrico, con tantos pasos a 0 como a 1, el **semiperiodo** es la mitad del periodo, o sea, los pasos que dura cada nivel. El **flanco de subida** es el instante en que CLK pasa de 0 a 1; en este documento llamaremos «el flanco» al primer instante con CLK=1.

### Paso 6. Flip-flop D disparado por flanco (maestro-esclavo)

Idea: dos latches D en serie con **habilitaciones opuestas**. El primero (maestro) está abierto cuando CLK=0 y el segundo (esclavo) cuando CLK=1. La idea es que nunca estén los dos abiertos a la vez (en este circuito concreto hay un breve solape, que se ve en el Paso 7), así que el dato no se cuela de un golpe: el maestro copia D mientras CLK=0, y cuando CLK sube (flanco de subida) el maestro se **cierra** reteniendo lo que copió y el esclavo se **abre** y lo pasa a Q. Q solo cambia justo después de un flanco de subida. Eso es un flip-flop D.

```text
  CLK ---+----------------------------------------+
         |                                        |
       [NOT]                                      |
         |                                        |
        nCLK                                     CLK
         |                                        |
         v                                        v
      +---------------------+              +---------------------+
  D --| latch D MAESTRO     |-- M y Mn ----| latch D ESCLAVO     |-- Q y Qn
      | (abierto con        |              | (abierto con        |
      |  nCLK = 1, CLK = 0) |              |  CLK = 1)           |
      +---------------------+              +---------------------+
```

Hecho con puertas, el esclavo reutiliza M y Mn del maestro en lugar de tener su propio NOT:

```text
nCLK(t+1) = NOT(CLK(t))
nD(t+1) = NOT(D(t))
a1(t+1) = NAND(D(t), nCLK(t))
b1(t+1) = NAND(nD(t), nCLK(t))
M(t+1) = NAND(a1(t), Mn(t))
Mn(t+1) = NAND(b1(t), M(t))
a2(t+1) = NAND(M(t), CLK(t))
b2(t+1) = NAND(Mn(t), CLK(t))
Q(t+1) = NAND(a2(t), Qn(t))
Qn(t+1) = NAND(b2(t), Q(t))
```

El flip-flop D completo usa 10 puertas: 2 NOT y 8 NAND.

Forma de onda del simulador con CLK lento y D cambiando en momentos distintos (`_` vale 0, `#` vale 1, una columna es un paso, `^` marca el flanco de subida; M es la salida del maestro):

```text
t      0         1         2         3         4
       012345678901234567890123456789012345678901234567
CLK    ______######______######______######______######
flanco       ^           ^           ^           ^
D      __#######____________#############______________
M      ____############___________#############________
Q      ________#############___________#############___
Qn     #########___________#############___________####
```

- Flancos de subida de CLK (primer paso con CLK=1): t = 6, 18, 30, 42. Q cambia en t = 8, 21, 32, 45, es decir 2, 3, 2, 3 pasos después del flanco correspondiente, y siempre al valor que D tenía en ese flanco (1, 0, 1, 0).
- D cambia en t = 2, 9, 21, 34. El cambio de t=2 ocurre con CLK=0: el maestro lo copia (M cambia en t=4) y el esclavo lo recoge en el flanco de t=6 (Q cambia en t=8). Los cambios de t = 9, 21, 34 ocurren con CLK=1 y el maestro está cerrado: M no se entera hasta que CLK baja (CLK baja en t = 12, 24, 36 y M cambia en t = 16, 27, 40) y Q no cambia hasta el flanco siguiente (flancos en t = 18, 30, 42, Q cambia en t = 21, 32, 45).

Y sobre el arranque:

- Comprobado con el simulador: desde cualquiera de los 1024 estados internos posibles, con D fija, 6 pasos con CLK=0 y después 6 pasos con CLK=1, el flip-flop acaba en Q=D (2048 casos con D=0 y D=1).

### Paso 7. La ventana prohibida del flip-flop D

Hay un detalle que el modelo de retardo unitario deja a la vista. Cuando CLK sube, el NOT que genera nCLK tarda un paso, así que durante un paso **el maestro sigue abierto y el esclavo ya está abierto**. Si D cambia justo en ese momento, el flip-flop puede equivocarse. El simulador lo mide: se cambia D un número de pasos antes o después del flanco (e es el primer instante con CLK=1) y se mira qué captura.

**Tabla 4.7. ¿Qué captura el flip-flop D si D cambia cerca del flanco?**

| D empieza a valer su valor nuevo en el instante | D: 0 -> 1 | D: 1 -> 0 |
|---|---|---|
| e-4 | nuevo | nuevo |
| e-3 | nuevo | nuevo |
| e-2 | nuevo | nuevo |
| e-1 | nuevo | INESTABLE |
| e | INESTABLE | viejo |
| e+1 | viejo | viejo |
| e+2 | viejo | viejo |
| e+3 | viejo | viejo |

- "nuevo": captura el valor al que D cambia; "viejo": captura el valor anterior; "INESTABLE": no captura nada válido (Q y Qn quedan iguales y oscilan).

Medido: si D cambia en el instante e-1 o en el instante e (e = primer instante con CLK=1) el resultado puede ser INESTABLE: Q y Qn quedan iguales y oscilan con periodo 2. Para dejarlo otra vez consistente basta aplicar, con D fija, 6 pasos con CLK=0 y 6 con CLK=1 (se comprueba desde cualquier estado, en «Y sobre el arranque» del Paso 6). Con D estable desde e-2 o cambiando a partir de e+1, el flip-flop se porta bien.

Esto se parece a lo que en electrónica real se llama tiempo de preparación (*setup*) y tiempo de mantenimiento (*hold*): D tiene que estar quieto alrededor del flanco. Aquí no es un número de nanosegundos, sino un número de pasos del modelo, y solo vale para **este** circuito en **este** modelo.

Comprobado con 400 secuencias aleatorias de D (cambios fuera de la ventana prohibida y separados al menos 3 pasos, periodo de reloj 12): en cada flanco Q acaba valiendo D, y Q y Qn dejan de moverse como mucho 4 pasos después del flanco.

Todas las formas de onda de este documento mantienen D (o T) fuera de la ventana prohibida de cada flanco; el script `check_B.py` que las genera lo comprueba.

### Paso 8. Flip-flop T

El flip-flop T cambia de valor (hace *toggle*) en cada flanco cuando T=1 y se queda igual cuando T=0. Se construye con un flip-flop D y una puerta XOR que calcula D = T ⊕ Q: si T=0, D=Q y no cambia nada; si T=1, D=¬Q y cambia siempre.

```text
          +-----+         +-------------+
  T ------| XOR |-- D ----|             |
     +----|     |         | flip-flop D |----+---- Q
     |    +-----+  CLK ---|>            |    |
     |                    +-------------+    |
     +---------------------------------------+
```

```text
D(t+1) = XOR(T(t), Q(t))
```

(Las demás puertas son las del flip-flop D del Paso 6.) Forma de onda con T cambiando en los momentos tranquilos de CLK; la fila D es la salida de la XOR:

```text
t      0         1         2         3         4         5         6         7
       012345678901234567890123456789012345678901234567890123456789012345678901
CLK    ______######______######______######______######______######______######
flanco       ^           ^           ^           ^           ^           ^
T      ############____________####################################____________
D      _########____############_________###########_____________###___________
Q      ________#########################___________#############_______________
```

- Flancos en t = 6, 18, 30, 42, 54, 66. T vale 1, 0, 1, 1, 1, 0 justo antes de cada uno, y Q vale 1, 1, 0, 1, 0, 0 después: cambia (hace toggle) cuando T=1 y se queda igual cuando T=0.

Con T=1 fija, el flip-flop T es un **divisor de frecuencia**:

```text
t      0         1         2         3         4         5
       012345678901234567890123456789012345678901234567890123456789
CLK    ______######______######______######______######______######
flanco       ^           ^           ^           ^           ^
Q      ________#############___________#############___________####
```

- Con T=1 fija, Q sube en t = 8, 32, 56: entre dos subidas pasan 24 pasos, el doble que el periodo de CLK (12 pasos). Un T-FF con T=1 es un divisor de frecuencia entre dos.

La XOR añade un paso entre T y D, y eso mueve la ventana prohibida (mismas etiquetas que en la Tabla 4.7, con "nuevo" y "viejo" referidos al valor de T que usa el flip-flop):

**Tabla 4.8. ¿Qué captura el flip-flop T si T cambia cerca del flanco?**

| T empieza a valer su valor nuevo en el instante | T: 0 -> 1 | T: 1 -> 0 |
|---|---|---|
| e-4 | nuevo | nuevo |
| e-3 | nuevo | nuevo |
| e-2 | nuevo | INESTABLE |
| e-1 | INESTABLE | viejo |
| e | viejo | viejo |
| e+1 | viejo | viejo |
| e+2 | viejo | viejo |
| e+3 | viejo | viejo |

- Medido: en el flip-flop T los instantes prohibidos para un cambio de T son e-2 y e-1, un paso antes que en el D, porque T tiene que atravesar la puerta XOR antes de llegar a D. Si T cambia en el instante e, Q todavía usa el T antiguo: el T-FF usa el valor que T tenía en e-1.
- Medido con un reloj simétrico y leyendo Q un paso antes del flanco siguiente (cuando ya ha pasado un ciclo entero desde el flanco): con T=1 fija (el divisor de frecuencia) el T-FF hace toggle en todos los flancos desde semiperiodo 3 (periodo 6) y falla con semiperiodo 1 y 2. Se probaron semiperiodos de 1 a 10.
- Si T cambia en instantes aleatorios, siempre fuera de la ventana prohibida (400 secuencias aleatorias por semiperiodo, además de T=1 fija), el mínimo es mayor: semiperiodo 4 (periodo 8); con semiperiodo 1, 2 y 3 falla.
- La diferencia viene de cuánto tarda Q en dejar de moverse después del flanco: como mucho 3 pasos con T=1 fija, pero hasta 4 si T baja de 1 a 0 en el instante e-3, justo antes de la ventana prohibida, porque entonces Q asoma un pulso falso breve (sube a los 2 pasos y vuelve a bajar a los 4). Q llega a D a través de la XOR, un paso más tarde (como mucho 4 y 5 pasos después del flanco), y ese cambio de D tiene que acabar antes de la ventana prohibida de D del flanco siguiente (Tabla 4.7), que empieza 1 paso antes de ese flanco: a 5 pasos del flanco anterior con semiperiodo 3 y a 7 con semiperiodo 4. Con T=1 fija, 4 es menor que 5 y basta con semiperiodo 3; con T cambiante, 5 no es menor que 5 y hay que subir a semiperiodo 4.
- Esto es una limitación de **este** montaje con puertas, no del flip-flop T en general. El documento 06 monta esta misma lista de puertas: con T=1 fija obtiene el mismo mínimo que arriba, y sus contadores, donde T lo calcula el propio circuito, tienen cada uno el suyo (sección «Tablas» de ese documento). Los relojes más rápidos de ese documento solo valen si el flip-flop se trata como una pieza ideal que reacciona en un solo paso, no como este circuito.

## Ejemplo resuelto completo

**El flip-flop D, puerta a puerta, alrededor de un flanco.** Es la misma simulación de la forma de onda del Paso 6, con todas las puertas visibles entre el instante en que D sube, el flanco y el instante en que D vuelve a bajar. Léela de arriba abajo; cada fila sale de la anterior con las dos fases.

```text
 t  D CLK | nCLK nD a1 b1 M Mn a2 b2 Q Qn
 2  1   0 |    1  1  1  0 0  1  1  1 0  1
 3  1   0 |    1  0  0  0 0  1  1  1 0  1
 4  1   0 |    1  0  0  1 1  1  1  1 0  1
 5  1   0 |    1  0  0  1 1  0  1  1 0  1
 6  1   1 |    1  0  0  1 1  0  1  1 0  1
 7  1   1 |    0  0  0  1 1  0  0  1 0  1
 8  1   1 |    0  0  1  1 1  0  0  1 1  1
 9  0   1 |    0  0  1  1 1  0  0  1 1  0
10  0   1 |    0  1  1  1 1  0  0  1 1  0
11  0   1 |    0  1  1  1 1  0  0  1 1  0
12  0   0 |    0  1  1  1 1  0  0  1 1  0
```

1. **Antes del flanco, el maestro copia D.** Con CLK=0 el maestro está abierto. D vale 1 desde t=2: a1 baja a 0 en t=3, M sube a 1 en t=4 y Mn baja a 0 en t=5. Mientras tanto Q sigue en 0, porque el esclavo está cerrado (a2 y b2 valen 1 hasta t=6).
2. **El flanco.** El flanco es t=6 (CLK pasa de 0 a 1). nCLK todavía vale 1 en t=6 y baja a 0 en t=7: el NOT del reloj tarda un paso, así que el maestro sigue abierto un paso más. El maestro ya tenía M=1 (copiaba D=1) y el esclavo lo recoge: a2 baja a 0 en t=7, Q sube a 1 en t=8 y Qn baja a 0 en t=9.
3. **Después del flanco, D ya no importa.** D baja a 0 en t=9, ya con CLK=1. Como a1 y b1 valen 1 desde t=8 (maestro cerrado), el cambio no entra: en t=12 M sigue valiendo 1 y Q sigue valiendo 1.

Un detalle más de la tabla. En la fila de t=8, Q ya vale 1 pero Qn todavía vale 1 (baja en t=9): durante un paso Q y Qn valen 1 a la vez, el mismo efecto de "una puerta ve el valor de hace un paso" que en el latch NAND.

## Tablas

Resumen de todo lo que hacen los cuatro dispositivos: la **tabla característica** dice cuál es Q(t+1) en función de las entradas y de Q(t) (qué hace); la **tabla de excitación** dice qué entradas hay que poner para conseguir un cambio dado de Q (qué pedir). En la de excitación, X significa "da igual" (no importa su valor).

**Ojo con la notación.** En las ecuaciones de puertas (las de los Pasos anteriores y las del mini-ejercicio), Q(t+1) es el valor un paso después. En estas tablas, Q(t+1) significa otra cosa: *el valor en el que se asienta Q* cuando el dispositivo ha terminado de reaccionar, que en el modelo de puertas no es un único paso del simulador. Medido en el modelo de puertas: el latch SR tarda hasta 2 pasos en quedar quieto (Tabla 4.2); el latch D, hasta 4 pasos en quedar todo quieto, puertas internas incluidas (Tabla 4.6); y en los flip-flops D y T, con un reloj de semiperiodo 6 y datos aleatorios, Q cambia entre 2 y 4 pasos después del flanco (en el D, además, Q y Qn dejan de moverse como mucho 4 pasos después).

**Tabla 4.9. Característica del latch SR con NOR**

| S | R | Q(t+1) |
|---|---|---|
| 0 | 0 | Q(t) |
| 0 | 1 | 0 |
| 1 | 0 | 1 |
| 1 | 1 | prohibido (Q = Qn = 0) |

**Tabla 4.10. Característica del latch D con habilitación**

| E | D | Q(t+1) |
|---|---|---|
| 0 | 0 | Q(t) |
| 0 | 1 | Q(t) |
| 1 | 0 | 0 |
| 1 | 1 | 1 |

**Tabla 4.11. Característica del flip-flop D**

| D | Q(t+1) tras el flanco de subida |
|---|---|
| 0 | 0 |
| 1 | 1 |

**Tabla 4.12. Característica del flip-flop T**

| T | Q(t) | Q(t+1) tras el flanco de subida |
|---|---|---|
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 0 |

Las mismas tablas escritas como ecuaciones (con `·` AND, `+` OR, `¬` NOT, `⊕` XOR):

- Latch SR con NOR (si S · R = 0):  Q(t+1) = S + ¬R · Q(t)
- Latch D con habilitación E:  Q(t+1) = E · D + ¬E · Q(t)
- Flip-flop D (flanco de subida):  Q(t+1) = D
- Flip-flop T (flanco de subida):  Q(t+1) = T ⊕ Q(t)

**Tabla 4.13. Excitación de los cuatro dispositivos**

| Q(t) | Q(t+1) | S | R | D | T |
|---|---|---|---|---|---|
| 0 | 0 | 0 | X | 0 | 0 |
| 0 | 1 | 1 | 0 | 1 | 1 |
| 1 | 0 | 0 | 1 | 0 | 1 |
| 1 | 1 | X | 0 | 1 | 0 |

Cómo se lee: para pasar de Q(t)=0 a Q(t+1)=1 con un latch SR hay que poner S=1 y R=0; para pasar de 1 a 1 basta con R=0 y S puede valer lo que quiera (X). En el flip-flop D, la entrada que hay que dar es siempre el valor deseado, y en el T es 1 solo cuando Q tiene que cambiar (T = Q ⊕ Q(t+1)). Todas las filas se han comprobado contra los circuitos de puertas del simulador.

## Mini-ejercicio

Con el latch SR con NOR de este documento, empezando en Q=0 y Qn=1 en t=0, y con estas entradas, calcula **a mano** Q y Qn en cada instante de la tabla (usa Q(t+1) = NOR(R(t), Qn(t)) y Qn(t+1) = NOR(S(t), Q(t))). Después contesta: ¿qué pasaría si S=1 solo durase un paso?

**Tabla 4.14. Enunciado: rellena las columnas con ?**

| t | S(t) | R(t) | Q(t) | Qn(t) |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 1 |
| 1 | 1 | 0 | ? | ? |
| 2 | 1 | 0 | ? | ? |
| 3 | 1 | 0 | ? | ? |
| 4 | 0 | 0 | ? | ? |
| 5 | 0 | 0 | ? | ? |
| 6 | 0 | 0 | ? | ? |

---

**Solución**

**Tabla 4.15. Solución**

| t | S(t) | R(t) | Q(t) | Qn(t) |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 1 |
| 1 | 1 | 0 | 0 | 1 |
| 2 | 1 | 0 | 0 | 0 |
| 3 | 1 | 0 | 1 | 0 |
| 4 | 0 | 0 | 1 | 0 |
| 5 | 0 | 0 | 1 | 0 |
| 6 | 0 | 0 | 1 | 0 |

Solución: Q y Qn terminan en (1, 0) en t=6. Con un pulso de S de un solo paso (S=1 solo en t=1) el latch no memoriza: entra en una oscilación de periodo 2. La traza de ese pulso corto es la del Paso 3 (no se memoriza nada).

## En Redstone / en Satisfactory

- **En Redstone**, según la Minecraft Wiki, una antorcha invierte la señal (es un NOT) y tarda 2 game ticks (0,1 segundos) en cambiar de estado ([Redstone Torch](https://minecraft.wiki/w/Redstone_Torch)). La página de circuitos de memoria presenta el latch RS con NOR como el latch "original" (se puede hacer solo con antorchas y polvo de redstone), dice que con S=1 y R=1 el circuito no tiene comportamiento definido y que el latch con NAND es menos eficiente porque una antorcha hace de NOR pero hacen falta varias para una NAND ([Redstone circuits/Memory](https://minecraft.wiki/w/Redstone_circuits/Memory)). Esa condición sin comportamiento definido es el estado prohibido de arriba.
- **Diferencias con el modelo.** La wiki también documenta que una antorcha se «quema» (*burn-out*) cuando se ve forzada a apagarse más de ocho veces en 60 game ticks, y que Java Edition y Bedrock Edition no se comportan igual en esto: en Bedrock solo se quema si se realimenta a sí misma sin ningún retardo ([Redstone Torch](https://minecraft.wiki/w/Redstone_Torch)); los relojes de antorcha añaden retardo con repetidores para evitarlo ([Redstone circuits/Clock](https://minecraft.wiki/w/Redstone_circuits/Clock)). El modelo de retardo unitario de este documento no tiene esa protección: un bucle que oscila sigue oscilando para siempre. Por eso las cifras de Minecraft son una referencia, no una especificación de lo que hará el mod.
- **En Satisfactory** el mod aún no está diseñado. Este documento solo deja preguntas abiertas: si el mod ofrecerá la memoria como pieza ya hecha o habrá que construirla con puertas, qué se hará con el estado prohibido, y qué modelo de tiempo usará su simulador (el retardo unitario de aquí es didáctico).

## Resumen

- Memoria es realimentación positiva: la salida vuelve a la entrada a través de un número par de inversiones, así que el bucle tiene dos estados estables y, si le pones uno de los dos, se queda en él. Con un número impar de inversiones el bucle oscila (documento 06). El retardo no crea la memoria: solo hace que el modelo esté bien definido.
- El latch SR (con NOR o con NAND) mantiene, pone a 1 o pone a 0; con S=R=1 (o Sn=Rn=0) entra en un estado prohibido que, al soltarse a la vez, oscila en el modelo de retardo unitario.
- El latch D con habilitación elimina el estado prohibido en régimen estable (al cambiar D asoma un glitch breve, Paso 5) y es transparente mientras E=1.
- El flip-flop D maestro-esclavo solo captura D en el flanco de subida del reloj, y D tiene que estar quieto alrededor del flanco (ventana prohibida medida en el simulador).
- El flip-flop T es un D con una XOR de realimentación: hace toggle cuando T=1, y con T=1 fija divide la frecuencia entre dos.
