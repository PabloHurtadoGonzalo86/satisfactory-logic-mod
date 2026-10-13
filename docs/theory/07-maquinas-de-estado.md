# 07 · Máquinas de estado

## Objetivo
Entender qué es una máquina de estados (estados, entradas, función de transición y función de salida), la diferencia entre Moore y Mealy y cómo se implementa con flip-flops D y puertas.  
Al terminar habrás diseñado dos máquinas completas, un detector de la secuencia 101 y un controlador con histéresis, y sabrás por qué todo circuito síncrono es, en el fondo, una de ellas.

## Idea intuitiva
Una máquina de estados es una libreta con **una sola anotación**. Cada vez que llega un dato haces lo mismo: miras lo que pone la libreta (el **estado**) y el dato nuevo; con esas dos cosas decides qué anotar a continuación (el **estado siguiente**) y qué contestar (la **salida**). Un semáforo, un ascensor o una máquina expendedora funcionan así: no importa **cómo** llegaste a donde estás, solo importa **dónde** estás.

La libreta tiene sitio para un número finito de anotaciones, así que la máquina no recuerda todo el pasado, solo un resumen: lo justo para responder bien. Diseñar una máquina de estados es decidir **qué hace falta recordar**.

Un ejemplo cotidiano con memoria es un termostato que enciende la calefacción por debajo de una temperatura y la apaga por encima de otra más alta. Entre las dos no puede decidir mirando solo la temperatura: tiene que **recordar** si estaba encendida. Ese bit de memoria es el estado (lo verás en el ejemplo 2).

## Explicación paso a paso

### Paso 0. Tiempo y notación

**Cambio de escala (modelo didáctico).** En el documento 06 cada puerta tenía un retardo de 1 paso del modelo. Aquí cambiamos de escala: un paso t es un **ciclo de reloj** completo y asumimos que la lógica combinacional (las puertas, que no tienen memoria) tiene tiempo de asentarse dentro del ciclo, antes del siguiente flanco. Eso se cumple si el periodo del reloj es suficiente (es la idea del «P mínimo» del documento 06). Con esa hipótesis la máquina queda muy limpia: S(t+1) = δ(S(t), x(t)).

Es una versión a escala de ciclo de las dos fases del 06. *Fase 1*: la lógica calcula el estado siguiente leyendo solo el estado actual y las entradas. *Fase 2*: en el flanco, todos los flip-flops capturan su nuevo valor a la vez. Al final del ejemplo 1 comprobaré que la versión abstracta y un circuito de puertas con retardo unitario dan los mismos resultados cuando el periodo del reloj es suficiente. Sigue siendo un modelo didáctico: el simulador real del mod se decidirá después.

La notación es la misma que en el 06: bits como `0b0110`, hexadecimal como `0x6`, y en las fórmulas `·` es AND, `+` es OR, `¬` es NOT (se pone delante: ¬x se lee «no x») y `⊕` es XOR (precedencia como en el documento 02: primero ¬, luego ·, luego +):

| Operación | En fórmulas | Ejemplo con a = 0b1 y b = 0b0 |
| :--- | :--- | :---: |
| AND | a · b | 0b0 |
| OR | a + b | 0b1 |
| NOT | ¬a | 0b0 |
| XOR | a ⊕ b | 0b1 |
| NAND / NOR / XNOR | ¬(a · b) / ¬(a + b) / ¬(a ⊕ b) | 0b1 / 0b0 / 0b0 |

Como en el 06, un literal binario lleva tantos dígitos como bits tenga la señal (`0b01` es un estado de 2 bits y `0b1` una señal de 1 bit), y en las ecuaciones lógicas el `+` es OR pero en una cuenta con números (como (s + 1) mod 16) es la suma de siempre; la multiplicación se escribe `×`. Una combinación «no importa» se escribe «-» en las tablas de verdad y X en los mapas de Karnaugh (documento 02).

Las ondas se dibujan con un carácter por ciclo: `#` es 1 y `_` es 0.

### Paso 1. Definición formal

Una máquina de estados finita (FSM, *finite-state machine*) es una séxtupla M = (S, X, Y, δ, λ, s0):

- **S**: conjunto **finito** de estados.
- **X**: conjunto de entradas posibles (con k bits de entrada hay 2^k).
- **Y**: conjunto de salidas posibles.
- **δ** (delta): S × X → S, la **función de transición**. Dice a qué estado pasas.
- **λ** (lambda): la **función de salida**. Dice qué contestas (Paso 2).
- **s0**: el estado inicial.

En cada ciclo t, el estado siguiente es S(t+1) = δ(S(t), x(t)) y la salida es Y(t) = λ(S(t), x(t)) o Y(t) = λ(S(t)) según el tipo de máquina.

Una máquina se puede escribir de tres formas equivalentes: **tabla de estados** (una fila por estado, una columna por entrada), **diagrama de estados** (círculos y flechas) y **ecuaciones** (lógica). Los dos ejemplos las usan todas. Y se diseña así: (1) ¿qué necesito recordar?, (2) cada respuesta distinta es un estado, (3) para cada estado y cada entrada, ¿a dónde voy y qué contesto?

### Paso 2. Moore frente a Mealy

Hay dos variantes, que llevan el nombre de Edward F. Moore y George H. Mealy:

- **Moore**: λ: S → Y. La salida depende **solo del estado**. Como el estado solo cambia en el flanco, la salida reacciona 1 ciclo después de la entrada que la provoca.
- **Mealy**: λ: S × X → Y. La salida depende del estado **y de la entrada actual**: reacciona en el mismo ciclo y suele necesitar menos estados, porque parte de la información la lleva la propia entrada.

En el detector de 101 del ejemplo 1, el Mealy necesita 3 estados y el Moore 4, y la salida del Moore llega 1 ciclo más tarde (comprobado con todas las cadenas de hasta 12 bits). La comparación resumida está en «Tablas».

### Paso 3. Codificación de estados: binaria frente a one-hot

El hardware no entiende nombres como S0 o S1: cada estado se guarda como un patrón de bits en flip-flops. Elegir ese patrón es **codificar** los estados. Dos opciones habituales:

- **Binaria**: el número del estado escrito en binario. N estados necesitan ceil(log2 N) flip-flops: pocos, pero cada ecuación mezcla todos los bits.
- **One-hot** (un solo bit activo): un flip-flop por estado y solo uno vale 1 a la vez. Más flip-flops, pero cada ecuación es simplemente «OR de las flechas que llegan a ese estado» y la salida suele ser un cable.

| estados N | FF binaria | códigos sin usar (binaria) | FF one-hot | códigos sin usar (one-hot) |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 1 | 0 | 2 | 2 |
| 3 | 2 | 1 | 3 | 5 |
| 4 | 2 | 0 | 4 | 12 |
| 5 | 3 | 3 | 5 | 27 |
| 8 | 3 | 0 | 8 | 248 |
| 16 | 4 | 0 | 16 | 65520 |

Los códigos sin usar son **estados ilegales**: solo se alcanzan por un fallo. Hay que decidir qué hacer con ellos; aquí los trataré como «no importa» (en inglés *don't care*; en las tablas de verdad se escribe «-» y en los mapas X, como en el documento 02) para simplificar más, y **comprobaré a dónde llevan**. Además, el arranque (reset) debe dejar los flip-flops en el código del estado inicial; en one-hot eso significa un 1 en exactamente un flip-flop.

### Paso 4. Implementación con flip-flops D y lógica combinacional

Un flip-flop D copia su entrada D cuando hay flanco de subida: Q(t+1) = D(t). Una máquina con k bits de estado son k flip-flops D (el **registro**) más lógica combinacional que calcula, a partir de Q y de x, las D (el estado siguiente) y la salida Y:

```text
                  +-------------------------------+
   x(t) --------->|                               |
                  |     LOGICA COMBINACIONAL      |--------> Y(t)
      +---------->|   (puertas AND, OR, NOT)      |
      |           +-------------------------------+
      |                          |
      |                          |  D = S(t+1)  (estado siguiente)
      |                          v
      |           +-------------------------------+
      |           |   REGISTRO: k flip-flops D    |<------- CLK
      |           +-------------------------------+
      |                          |
      |                          |  Q = S(t)  (estado actual)
      +--------------------------+
```

Receta:

1. Codifica los estados (Paso 3).
2. Escribe la tabla de verdad: para cada combinación de Q y x, los valores de cada D y de Y.
3. Cada columna de salida es una suma de productos: cada fila con 1 aporta un término AND con todas las variables (negadas las que valen 0) y se suman con OR. Después se simplifica.
4. Cablea: las D a la lógica, las Q de vuelta a la lógica, un reloj común.

En una máquina de Moore, Y sale solo de las Q (a la lógica de salida no llega x).

### Paso 5. Todo sistema digital síncrono es una máquina de estados

Cualquier circuito síncrono (reloj, flip-flops y puertas) se puede ver así: sus n flip-flops guardan n bits, el estado completo es ese patrón de bits y la lógica combinacional calcula el siguiente a partir del actual y de las entradas. Por tanto es una máquina de estados con **como mucho 2^n estados**. (Ojo con la escala: este n cuenta los flip-flops, es decir, la escala del ciclo de reloj de este documento. A la escala de pasos del documento 06 la foto del circuito incluye además la salida de cada puerta y el valor anterior del reloj que guarda cada flip-flop, así que n cuenta todo eso; el anillo de 3 NOT de abajo es un ejemplo de esa segunda cuenta, una variable por puerta.)

| bits de memoria n | estados posibles 2^n |
| ---: | ---: |
| 1 | 2 |
| 2 | 4 |
| 3 | 8 |
| 4 | 16 |
| 5 | 32 |
| 6 | 64 |
| 8 | 256 |
| 10 | 1024 |

Ejemplos del documento 06. A escala de ciclo, el contador de 4 bits es una máquina de Moore sin entradas con 16 estados y δ(s) = (s + 1) mod 16 (aquí `+` es la suma de números; comprobado contra la simulación de puertas del 06). El anillo de 3 NOT, con una variable de estado por puerta, es una máquina autónoma de 8 estados; su diagrama de estados completo son dos ciclos (los estados se escriben como bits g1 g2 g3, con g1 el más significativo):

| longitud del ciclo | estados (g1 g2 g3) |
| ---: | :--- |
| 2 | 0b000 -> 0b111 -> 0b000 |
| 6 | 0b001 -> 0b011 -> 0b010 -> 0b110 -> 0b100 -> 0b101 -> 0b001 |

El ciclo largo es el oscilador de periodo 6; el corto es el que sale si arrancas en 0b000.

La consecuencia es una idea muy potente, pero **solo para las máquinas autónomas**: las que no tienen entradas (o las tienen fijas), como un reloj, un contador o el anillo de NOT. Como son **deterministas** (el estado siguiente solo depende del actual) y tienen un número **finito** de estados, si alguna vez repiten un estado repiten todo lo que viene después. Con 2^n estados posibles, en como mucho 2^n pasos hay una repetición: toda trayectoria acaba en un ciclo. El «periodo» de un reloj es la longitud de ese ciclo y **nunca puede superar 2^n**. Y de los 2^n patrones posibles solo se alcanzan los que salen del estado inicial (tabla de estados alcanzables en «Tablas»).

Con entradas que cambian a voluntad esto **no** vale: repetir un estado no repite el futuro, porque el futuro también depende de las entradas que lleguen. Lo verás en la traza del detector 101 (ejemplo 1, más abajo): el Mealy está en S1 en t=1 y en t=5, y pasa a S2 la primera vez y a S1 la segunda, porque la entrada fue 0 y después 1.

## Ejemplo resuelto completo

### Ejemplo 1: detector de la secuencia 101 (Mealy y Moore)

**Especificación.** Entra un bit por ciclo. La salida Y debe valer 1 cuando los 3 últimos bits recibidos sean 1, 0, 1, en ese orden. Se admite **solapamiento**: en 10101 aparece 101 dos veces (comparten el 1 central).

**Qué hay que recordar.** No hace falta guardar todos los bits, solo «hasta dónde llevo del 101». El estado S_k significa «el mayor trozo final de lo recibido que coincide con el principio de 101 tiene k bits». S0: nada útil. S1: llevo `1`. S2: llevo `10`.

#### Mealy (3 estados)

```text
M = (S, X, Y, delta, lambda, s0)
S = {S0, S1, S2}   (3 estados)
X = {0, 1}          (1 bit de entrada)
Y = {0, 1}          (1 bit de salida)
s0 = S0
delta : S x X -> S      (siguiente estado)
lambda: S x X -> Y      (Mealy: la salida mira el estado junto con la entrada)
```

| Estado | significado | x=0: siguiente / Y | x=1: siguiente / Y |
| :---: | :--- | :---: | :---: |
| S0 | nada útil | S0 / 0 | S1 / 0 |
| S1 | llevo '1' | S2 / 0 | S1 / 0 |
| S2 | llevo '10' | S0 / 0 | S1 / 1 |

Cada casilla es «estado siguiente / salida». Solo hay un 1 de salida: en S2 con x = 1 (llevaba `10` y llega un 1). Diagrama (las flechas se rotulan x/Y; la flecha de entrada marca el estado inicial):

```text
       0/0                 1/0
        +--+                +--+
        |  v                |  v
      +---------+   1/0   +---------+   0/0   +---------+
 ---> |    S0   |-------->|    S1   |-------->|    S2   |
      |   nada  |         | llevo 1 |<--------| llevo 10|
      +---------+         +---------+   1/1   +---------+
           ^                                       |
           |                                       |
           +---------------------------------------+
                              0/0
```

#### Moore (4 estados)

Para que la salida dependa solo del estado hace falta un estado más, S3 = «acabo de ver 101», que es el único con Y = 1.

```text
M = (S, X, Y, delta, lambda, s0)
S = {S0, S1, S2, S3}   (4 estados)
X = {0, 1}
Y = {0, 1}
s0 = S0
delta : S x X -> S
lambda: S -> Y         (Moore: la salida mira SOLO el estado)
```

| Estado | significado | Y | x=0: siguiente | x=1: siguiente |
| :---: | :--- | :---: | :---: | :---: |
| S0 | nada útil | 0 | S0 | S1 |
| S1 | llevo '1' | 0 | S2 | S1 |
| S2 | llevo '10' | 0 | S0 | S3 |
| S3 | acabo de ver '101' | 1 | S2 | S1 |

```text
                                                  1
         0                  1    +----------------------------------+
        +--+               +--+  |                                  |
        |  v               |  v  v                                  |
      +---------+   1    +---------+   0    +---------+   1    +---------+
 ---> | S0  Y=0 |------->| S1  Y=0 |------->| S2  Y=0 |------->| S3  Y=1 |
      |   nada  |        | llevo 1 |        | llevo 10|<-------|llevo 101|
      +---------+        +---------+        +---------+   0    +---------+
           ^                                     |
           |                                     |
           +-------------------------------------+
                              0
```

En el diagrama de Moore la salida va dentro de cada estado (`Y=0`, `Y=1`) y las flechas solo llevan la entrada x.

#### Traza simulada

Entrada, un bit por ciclo, de izquierda a derecha: 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1 (14 ciclos). S(t) es el estado al principio del ciclo t y S(t+1) el que resulta tras leer x(t).

Mealy:

| t | x(t) | S(t) | S(t+1) | Y(t) |
| ---: | ---: | :---: | :---: | ---: |
| 0 | 1 | S0 | S1 | 0 |
| 1 | 0 | S1 | S2 | 0 |
| 2 | 1 | S2 | S1 | 1 |
| 3 | 0 | S1 | S2 | 0 |
| 4 | 1 | S2 | S1 | 1 |
| 5 | 1 | S1 | S1 | 0 |
| 6 | 0 | S1 | S2 | 0 |
| 7 | 1 | S2 | S1 | 1 |
| 8 | 0 | S1 | S2 | 0 |
| 9 | 0 | S2 | S0 | 0 |
| 10 | 1 | S0 | S1 | 0 |
| 11 | 0 | S1 | S2 | 0 |
| 12 | 1 | S2 | S1 | 1 |
| 13 | 1 | S1 | S1 | 0 |

Moore:

| t | x(t) | S(t) | S(t+1) | Y(t) |
| ---: | ---: | :---: | :---: | ---: |
| 0 | 1 | S0 | S1 | 0 |
| 1 | 0 | S1 | S2 | 0 |
| 2 | 1 | S2 | S3 | 0 |
| 3 | 0 | S3 | S2 | 1 |
| 4 | 1 | S2 | S3 | 0 |
| 5 | 1 | S3 | S1 | 1 |
| 6 | 0 | S1 | S2 | 0 |
| 7 | 1 | S2 | S3 | 0 |
| 8 | 0 | S3 | S2 | 1 |
| 9 | 0 | S2 | S0 | 0 |
| 10 | 1 | S0 | S1 | 0 |
| 11 | 0 | S1 | S2 | 0 |
| 12 | 1 | S2 | S3 | 0 |
| 13 | 1 | S3 | S1 | 1 |

```text
         0         1
ciclo t  01234567890123
x        #_#_##_#__#_##
Y Mealy  __#_#__#____#_
Y Moore  ___#_#__#____#
```

Resultado: hay 4 apariciones de 101, que terminan en los ciclos 2, 4, 7, 12. El Mealy pone Y = 1 **en esos mismos ciclos** y el Moore **un ciclo después**, en 3, 5, 8, 13. Fíjate en el solapamiento: las apariciones que terminan en 2 y en 4 comparten el 1 del ciclo 2.

**Verificación exhaustiva.** He comparado ambas máquinas con la definición («los 3 últimos bits son 101») para las 8191 cadenas de entre 0 y 12 bits: el Mealy coincide en el mismo ciclo y el Moore un ciclo más tarde, sin excepciones.

#### Implementación del Mealy con flip-flops D

Codificación binaria (2 flip-flops, Q1 es el bit alto) y one-hot (3 flip-flops):

| Estado | Q1 Q0 |
| :---: | :---: |
| S0 | 0b00 |
| S1 | 0b01 |
| S2 | 0b10 |

| Estado | E2 E1 E0 |
| :---: | :---: |
| S0 | 0b001 |
| S1 | 0b010 |
| S2 | 0b100 |

Tabla de verdad con la codificación binaria. El código 0b11 no corresponde a ningún estado, así que sus filas valen «-» (no importa):

| Estado | Q1 | Q0 | x | D1 | D0 | Y |
| :---: | ---: | ---: | ---: | ---: | ---: | ---: |
| S0 | 0 | 0 | 0 | 0 | 0 | 0 |
| S0 | 0 | 0 | 1 | 0 | 1 | 0 |
| S1 | 0 | 1 | 0 | 1 | 0 | 0 |
| S1 | 0 | 1 | 1 | 0 | 1 | 0 |
| S2 | 1 | 0 | 0 | 0 | 0 | 0 |
| S2 | 1 | 0 | 1 | 0 | 1 | 1 |
| (sin usar) | 1 | 1 | 0 | - | - | - |
| (sin usar) | 1 | 1 | 1 | - | - | - |

Para ver por qué conviene simplificar: D0 vale 1 en tres filas, y escrito como suma de productos son tres términos:

```formulas
D0 = ¬Q1·¬Q0·x + ¬Q1·Q0·x + Q1·¬Q0·x
```

Pero las filas «sin usar» son libres: son combinaciones que no pueden ocurrir (o cuya salida da igual), y en un mapa de Karnaugh se marcan con una **X**. Cada X se puede contar como 0 o como 1, lo que deje los grupos más grandes. Para simplificar uso mapas de Karnaugh (documento 02, Paso 6): con tres variables basta un mapa por salida, con Q1 en las filas y Q0 x en las columnas, en orden Gray. El número de minterm sale de leer Q1 Q0 x como un número binario (Q1 es el bit alto, como en el documento 02); los minterms 6 y 7 son las filas sin usar.

```text
# K-mapa de D1, unos en los minterms 2 y X en los minterms 6,7
       Q0 x
       00 01 11 10
Q1=0 |  0  0  0  1
Q1=1 |  0  0  X  X
```

```text
# K-mapa de D0, unos en los minterms 1,3,5 y X en los minterms 6,7
       Q0 x
       00 01 11 10
Q1=0 |  0  1  1  0
Q1=1 |  0  1  X  X
```

```text
# K-mapa de Y, unos en los minterms 5 y X en los minterms 6,7
       Q0 x
       00 01 11 10
Q1=0 |  0  0  0  0
Q1=1 |  0  1  X  X
```

Los grupos que salen: en D1, un grupo de 2 casillas (1 con valor 1 y 1 X), las de Q0 = 1 y x = 0, que da Q0·¬x; en D0, un grupo de 4 casillas (3 con valor 1 y 1 X), las de x = 1, que da x; en Y, un grupo de 2 casillas (1 con valor 1 y 1 X), las de Q1 = 1 y x = 1, que da Q1·x. Las X se notan: sin aprovecharlas, las ecuaciones serían más largas.

| Salida | sin aprovechar las X | aprovechando las X (el grupo del mapa) |
| :---: | :--- | :--- |
| D1 | ¬Q1·Q0·¬x (3 literales) | Q0·¬x (2 literales) |
| D0 | ¬Q1·x + ¬Q0·x (4 literales) | x (1 literal) |
| Y | Q1·¬Q0·x (3 literales) | Q1·x (2 literales) |

Estas son las ecuaciones que resultan. El método mecánico equivalente al mapa es Quine-McCluskey, y es el que usa el script de comprobación (`docs/theory/check_C.py`) para calcularlas y verificar cada una contra la tabla para todas las entradas:

```formulas
D1 = Q0·¬x
D0 = x
Y  = Q1·x
```

Se leen en voz alta: «el bit alto del siguiente estado es 1 si vengo de S1 (Q0 = 1) y llega un 0»; «el bit bajo es simplemente x» (cualquier 1 de entrada lleva a S1, de código 0b01); «Y = 1 si estoy en S2 (Q1 = 1) y llega un 1».

**¿Qué pasa si el circuito cae por error en el código ilegal 0b11?** Las ecuaciones dan: con x=0 pasa a 0b10 (que es S2) y da Y=0; con x=1 pasa a 0b01 (que es S1) y da Y=1. Es decir, vuelve a un estado legal en un solo ciclo y no se queda atrapado. (Conviene comprobarlo siempre: depende de cómo hayas simplificado.)

**One-hot por lectura de flechas.** Cada flip-flop N_j (el siguiente valor del estado S_j) es el OR de las flechas que llegan a S_j, cada una con su condición:

```formulas
N0 = E0·¬x + E2·¬x
N1 = E0·x + E1·x + E2·x
N2 = E1·¬x
Y  = E2·x
```

Como en one-hot siempre hay **exactamente un** E activo, E0 + E1 + E2 = 1 (aquí `+` es OR), y de ahí salen dos simplificaciones: `N1 = x`, porque sus tres flechas cubren todos los estados, y E0 + E2 = ¬E1, así que N0 pasa a ser ¬E1·¬x. Además, los 5 códigos one-hot sin usar también son «no importa». Con todo eso (el mismo método, ahora con 4 variables: E2, E1, E0 y x) queda:

```formulas
N0 = ¬E1·¬x
N1 = x
N2 = E1·¬x
Y  = E2·x
```

Los 5 códigos ilegales vuelven también a un estado legal en un ciclo, con x = 0 y con x = 1 (comprobado).

Para comparar las dos codificaciones hay que contar de la misma manera, y la tabla de «Tablas» lo hace de dos formas. Con las dos simplificadas por igual, la binaria gana en flip-flops (2 frente a 3), en términos (3 frente a 4) y en literales (5 frente a 7), aunque la diferencia de lógica es pequeña. Sin simplificar, la one-hot leída de las flechas incluso queda algo más corta en literales que la forma canónica de la binaria (14 frente a 15), porque nombra cada estado con un literal en vez de dos. Es un único ejemplo: no lo extrapoles a otros tamaños sin medirlo.

**Moore, codificación binaria** (los 4 códigos están en uso, así que no hay X; se simplifica con los mismos mapas):

| Estado | Q1 Q0 |
| :---: | :---: |
| S0 | 0b00 |
| S1 | 0b01 |
| S2 | 0b10 |
| S3 | 0b11 |

| Estado | Q1 | Q0 | x | D1 | D0 |
| :---: | ---: | ---: | ---: | ---: | ---: |
| S0 | 0 | 0 | 0 | 0 | 0 |
| S0 | 0 | 0 | 1 | 0 | 1 |
| S1 | 0 | 1 | 0 | 1 | 0 |
| S1 | 0 | 1 | 1 | 0 | 1 |
| S2 | 1 | 0 | 0 | 0 | 0 |
| S2 | 1 | 0 | 1 | 1 | 1 |
| S3 | 1 | 1 | 0 | 1 | 0 |
| S3 | 1 | 1 | 1 | 0 | 1 |

```formulas
D1 = Q0·¬x + Q1·¬Q0·x
D0 = x
Y  = Q1·Q0
```

**Verificación en puertas.** Las ecuaciones anteriores (binaria, one-hot leída de las flechas, one-hot simplificada y Moore) se han montado con flip-flops D y puertas de retardo unitario (NOT, AND, OR, y BUF para copiar una señal tal cual), con un reloj de periodo 12 pasos, y se han simulado contra la tabla abstracta con las 510 cadenas de entrada de 1 a 8 bits. En cada ciclo coinciden el estado y la salida Y (observados en el paso del flanco de reloj, justo antes de capturar). Con un periodo de reloj suficiente (aquí, 12 pasos), el modelo abstracto de este documento y un circuito de puertas como los del documento 06 dan, por tanto, los mismos resultados.

### Ejemplo 2: controlador con histéresis (la bomba y el depósito)

**Especificación.** Un actuador (por ejemplo, una bomba que llena un depósito) se controla con el nivel L del depósito, en tanto por ciento y con valores enteros:

- Si L < 20, se **enciende** (ON).
- Si L > 80, se **apaga** (OFF).
- En cualquier otro caso (de 20 a 80 **ambos incluidos**) se queda como estaba.

Esa tercera regla es la histéresis: entre los dos umbrales la salida depende de la **historia**, y la historia es memoria. Estado = «¿estoy encendido?».

**Entradas.** Dos comparadores convierten el nivel en dos bits, BAJO = (L < 20) y ALTO = (L > 80). Nunca valen 1 a la vez, así que la combinación BAJO = 1, ALTO = 1 es **imposible** y se tratará como «no importa»:

| nivel L (%) | BAJO = (L < 20) | ALTO = (L > 80) |
| ---: | ---: | ---: |
| 0 | 1 | 0 |
| 19 | 1 | 0 |
| 20 | 0 | 0 |
| 21 | 0 | 0 |
| 50 | 0 | 0 |
| 79 | 0 | 0 |
| 80 | 0 | 0 |
| 81 | 0 | 1 |
| 100 | 0 | 1 |

**Máquina de Moore con 2 estados** (la salida Y = 1 significa ON y es el propio estado):

| Estado | Y (1 = ON) | BAJO=0 ALTO=0 | BAJO=1 ALTO=0 | BAJO=0 ALTO=1 | BAJO=1 ALTO=1 |
| :---: | :---: | :---: | :---: | :---: | :---: |
| OFF | 0 | OFF | ON | OFF | imposible |
| ON | 1 | ON | ON | OFF | imposible |

```text
       BAJO=0                     ALTO=0
        +--+                       +--+
        |  v                       |  v
      +-------+      BAJO=1      +-------+
 ---> |  OFF  |----------------->|   ON  |
      |  Y=0  |<-----------------|  Y=1  |
      +-------+      ALTO=1      +-------+
```

Cada estado se queda en sí mismo mientras no llegue su condición de salida: OFF espera a BAJO = 1 y ON espera a ALTO = 1.

**Implementación.** Con una codificación binaria de 1 bit (OFF = 0b0, ON = 0b1) basta un flip-flop D. Tabla de verdad (las filas con BAJO = 1 y ALTO = 1 son imposibles: «-»):

| Estado | Q | BAJO | ALTO | D |
| :---: | ---: | ---: | ---: | ---: |
| OFF | 0 | 0 | 0 | 0 |
| OFF | 0 | 0 | 1 | 0 |
| OFF | 0 | 1 | 0 | 1 |
| OFF | 0 | 1 | 1 | - |
| ON | 1 | 0 | 0 | 1 |
| ON | 1 | 0 | 1 | 0 |
| ON | 1 | 1 | 0 | 1 |
| ON | 1 | 1 | 1 | - |

Mapa de Karnaugh de D (filas: Q; columnas: BAJO ALTO en orden Gray; las dos casillas imposibles son X):

```text
# K-mapa de D, unos en los minterms 2,4,6 y X en los minterms 3,7
      BAJO ALTO
      00 01 11 10
Q=0 |  0  0  X  1
Q=1 |  1  0  X  1
```

Salen estos grupos: un grupo de 4 casillas (2 con valor 1 y 2 X), las de BAJO = 1, que da BAJO; otro de 2 casillas (2 con valor 1 y 0 X), las de Q = 1 y ALTO = 0, que da Q·¬ALTO. Ecuaciones:

```formulas
D = BAJO + Q·¬ALTO
Y = Q
```

D = BAJO + Q · ¬ALTO se lee «estaré encendido si ahora tengo nivel bajo o si ya estaba encendido y no hay nivel alto». Equivale a un **biestable SR** (el nombre general de latches y flip-flops, documento 04; set = BAJO, reset = ALTO): la histéresis es un bit de memoria con dos entradas. Montada en puertas con flip-flop D y simulada contra la tabla con las 1092 secuencias de entradas posibles de 1 a 6 ciclos, coincide en todas.

**Traza simulada en lazo cerrado.** Para poder simular hace falta un depósito de juguete (no es el modelo de ningún juego): en cada paso pierde 10 puntos por consumo y, mientras el actuador está ON, gana 30 (lo que aporte al depósito cuando está encendido), con el nivel limitado entre 0 y 100. Empieza con L = 70 y el actuador en OFF. Con el actuador ON el nivel sube un neto de 20 puntos por paso.

| t | nivel L(t) (%) | BAJO(t) | ALTO(t) | S(t) | S(t+1) |
| ---: | ---: | ---: | ---: | :---: | :---: |
| 0 | 70 | 0 | 0 | OFF | OFF |
| 1 | 60 | 0 | 0 | OFF | OFF |
| 2 | 50 | 0 | 0 | OFF | OFF |
| 3 | 40 | 0 | 0 | OFF | OFF |
| 4 | 30 | 0 | 0 | OFF | OFF |
| 5 | 20 | 0 | 0 | OFF | OFF |
| 6 | 10 | 1 | 0 | OFF | ON |
| 7 | 0 | 1 | 0 | ON | ON |
| 8 | 20 | 0 | 0 | ON | ON |
| 9 | 40 | 0 | 0 | ON | ON |
| 10 | 60 | 0 | 0 | ON | ON |
| 11 | 80 | 0 | 0 | ON | ON |
| 12 | 100 | 0 | 1 | ON | OFF |
| 13 | 100 | 0 | 1 | OFF | OFF |
| 14 | 90 | 0 | 1 | OFF | OFF |
| 15 | 80 | 0 | 0 | OFF | OFF |
| 16 | 70 | 0 | 0 | OFF | OFF |
| 17 | 60 | 0 | 0 | OFF | OFF |
| 18 | 50 | 0 | 0 | OFF | OFF |
| 19 | 40 | 0 | 0 | OFF | OFF |
| 20 | 30 | 0 | 0 | OFF | OFF |
| 21 | 20 | 0 | 0 | OFF | OFF |
| 22 | 10 | 1 | 0 | OFF | ON |
| 23 | 0 | 1 | 0 | ON | ON |
| 24 | 20 | 0 | 0 | ON | ON |

Lectura de la traza:

- La máquina cambia de estado al terminar los ciclos 6, 12, 22: 3 cambios en 25 ciclos.
- En t=5 el nivel vale exactamente 20 y **no** se enciende (la condición es L < 20); en t=11 vale exactamente 80 y **no** se apaga.
- La decisión se toma con el nivel de t y se nota en t+1: por eso el nivel sigue subiendo un ciclo más y en t=13 se saturaría en 120 (se limita a 100).

**Por qué hace falta histéresis.** Con un único umbral, si el nivel «tiembla» alrededor de él, el actuador conecta y desconecta sin parar. Se compara un controlador de umbral único en 20 (ON en el ciclo siguiente a tener L < 20, OFF en los demás) con el de histéresis, con una entrada que oscila alrededor de 20 (cada celda es un ciclo; `#` es ON):

```text
t             0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
nivel L      30 24 21 19 22 18 23 17 21 19 24 18 40 70 79 81 78 82 79 83
umbral 20     _  _  _  _  #  _  #  _  #  _  #  _  #  _  _  _  _  _  _  _
histeresis    _  _  _  _  #  #  #  #  #  #  #  #  #  #  #  #  _  _  _  _
```

En 20 muestras, el umbral único cambia de estado 10 veces; la histéresis, 2.

## Tablas

Moore frente a Mealy:

|  | Moore | Mealy |
| :--- | :--- | :--- |
| La salida depende de | solo del estado | del estado y de la entrada actual |
| Cuándo reacciona a una entrada | 1 ciclo después | en el mismo ciclo |
| Estados del detector 101 | 4 | 3 |
| Si la entrada cambia a mitad de ciclo | la salida no se entera hasta el siguiente flanco | la salida puede cambiar a mitad de ciclo |

Coste de las dos codificaciones en el Mealy del ejemplo 1. Cada una se cuenta de dos formas, y solo son comparables las filas con el mismo criterio (las dos primeras, y las dos últimas): «simplificada» es la suma de productos mínima con los códigos sin usar como X; «sin simplificar» es la forma directa, un término por cada fila con 1 de la tabla de verdad (binaria) o por cada flecha (one-hot).

| Mealy 101 | criterio | flip-flops | códigos sin usar | términos AND | literales |
| :--- | :--- | ---: | ---: | ---: | ---: |
| binaria | simplificada (mapa de Karnaugh, X en los códigos sin usar) | 2 | 1 | 3 | 5 |
| one-hot | simplificada (X en los códigos sin usar) | 3 | 5 | 4 | 7 |
| binaria | sin simplificar (un término por cada fila con 1) | 2 | 1 | 5 | 15 |
| one-hot | sin simplificar (un término por cada flecha) | 3 | 5 | 7 | 14 |

Estados posibles frente a estados alcanzables desde el estado inicial (n es el número de bits de memoria: flip-flops a escala de ciclo; en el anillo, una variable por puerta):

| máquina | bits de memoria n | 2^n | estados alcanzables |
| :--- | ---: | ---: | ---: |
| contador de 4 bits (doc 06) | 4 | 16 | 16 |
| detector 101 Mealy, binaria | 2 | 4 | 3 |
| detector 101 Mealy, one-hot | 3 | 8 | 3 |
| detector 101 Moore, binaria | 2 | 4 | 4 |
| detector 101 Moore, one-hot | 4 | 16 | 4 |
| histéresis, binaria | 1 | 2 | 2 |
| anillo de 3 NOT desde cadena estabilizada | 3 | 8 | 6 |
| anillo de 3 NOT desde 0b000 | 3 | 8 | 2 |

## Mini-ejercicio

Diseña un detector de **dos unos seguidos** (secuencia 11, con solapamiento) de tipo Mealy.

(a) ¿Cuántos estados necesita?

(b) Escribe su tabla de estados.

(c) Simula la entrada 1, 1, 1, 0, 1, 1 (un bit por ciclo, de izquierda a derecha).

(d) Con un solo flip-flop D (S0 = 0b0, S1 = 0b1), ¿cuáles son las ecuaciones de D e Y?

---

**Solución**

(a) 2 estados: S0 («nada útil», el último bit no fue 1) y S1 («llevo `1`»).

(b)

| Estado | significado | x=0: siguiente / Y | x=1: siguiente / Y |
| :---: | :--- | :---: | :---: |
| S0 | nada útil | S0 / 0 | S1 / 0 |
| S1 | llevo '1' | S0 / 0 | S1 / 1 |

(c) Y vale 1 en los ciclos 1, 2, 5. Con solapamiento, tres unos seguidos dan dos apariciones.

| t | x(t) | S(t) | S(t+1) | Y(t) |
| ---: | ---: | :---: | :---: | ---: |
| 0 | 1 | S0 | S1 | 0 |
| 1 | 1 | S1 | S1 | 1 |
| 2 | 1 | S1 | S1 | 1 |
| 3 | 0 | S1 | S0 | 0 |
| 4 | 1 | S0 | S1 | 0 |
| 5 | 1 | S1 | S1 | 1 |

(d) D = x (cualquier 1 lleva a S1 y cualquier 0 a S0) e Y = Q·x (estoy en S1 y llega otro 1). Verificado contra la tabla y contra la definición para todas las cadenas de hasta 12 bits.

## En Redstone / en Satisfactory
- **Redstone**: la wiki de Minecraft clasifica los circuitos de memoria en latches y flip-flops RS, T, D y JK ([Redstone circuits/Memory](https://minecraft.wiki/w/Redstone_circuits/Memory)) y construye las puertas con antorchas: una antorcha con una entrada es un NOT y con dos o más un NOR ([Redstone circuits/Logic](https://minecraft.wiki/w/Redstone_circuits/Logic)). Una máquina de estados es justo «memoria + puertas + reloj».
- **Satisfactory**: el mod aún no está diseñado, así que quedan como preguntas abiertas cuántos bits de memoria tendrá una pieza, si ese estado se guardará con la partida y qué estado tendrá al construirla.

## Resumen
- Una máquina de estados es M = (S, X, Y, δ, λ, s0) con S(t+1) = δ(S(t), x(t)): el estado es lo que recuerdas del pasado.
- Moore saca la salida solo del estado (en el 101, un estado más y un ciclo de retraso); Mealy la saca del estado y de la entrada (menos estados, misma sincronía que la entrada).
- Los estados se codifican en binario (ceil(log2 N) flip-flops) o en one-hot (N flip-flops, ecuaciones que se leen de las flechas).
- Se implementan con un registro de flip-flops D más lógica combinacional, y cada ciclo tiene dos fases, parecidas a las del documento 06: «calcular las D» y «capturar en el flanco»; se ha verificado también a nivel de puertas.
- La histéresis es un bit de memoria entre dos umbrales, y todo circuito síncrono con n bits de memoria es una máquina de estados con como mucho 2^n estados.
