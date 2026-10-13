# 06 · Relojes y contadores

## Objetivo
Entender cómo se fabrica el «tiempo» en un circuito digital: relojes, divisores de frecuencia, contadores y circuitos que reaccionan a un cambio de la señal.  
Al terminar sabrás predecir el periodo de cada montaje y explicar por qué un contador asíncrono (ripple) se equivoca si el reloj va demasiado rápido.

## Idea intuitiva
Un circuito digital solo sabe de ceros y unos. No tiene un reloj de pared ni sabe «esperar un segundo». Para que algo ocurra cada cierto tiempo hace falta una señal que suba y baje sola, una y otra vez: eso es un **reloj**. Piensa en un metrónomo: tic, tac, tic, tac.

Con el metrónomo se construye todo lo demás:

- **Divisor de frecuencia**: un engranaje reductor. Por cada dos vueltas de la rueda de entrada, la de salida da una.
- **Contador**: el cuentakilómetros de un coche. Cada rueda de dígitos avanza cuando la anterior pasa de 9 a 0; en binario, cada bit cambia cuando el anterior pasa de 1 a 0.
- **Detector de flanco**: el timbre de una puerta. Suena en el instante en que pulsas, no mientras mantienes el dedo apretado.
- **Alargador de pulso**: la luz de la escalera. Un toque corto y se queda encendida un rato.

Todo se mide en **pasos**, la unidad mínima de tiempo de nuestro modelo (no son segundos ni «ticks» del juego).

## Explicación paso a paso

### Paso 0. Notación (la misma en toda la serie)

Los bits se escriben en binario con prefijo `0b` y en hexadecimal con prefijo `0x`. El mismo número en tres formas:

| decimal | binario | hexadecimal |
| ---: | :---: | :---: |
| 6 | 0b0110 | 0x6 |
| 10 | 0b1010 | 0xA |
| 255 | 0b11111111 | 0xFF |

Un literal binario lleva tantos dígitos como bits tenga la señal: `0b1` es una señal de 1 bit, `0b101` una de 3 bits y `0b0110` una de 4 (los ceros de la izquierda cuentan).

Las operaciones lógicas se escriben con su nombre en las listas de circuitos y en los diagramas (AND, OR, NOT, XOR, NAND, NOR, XNOR) y con símbolos en las fórmulas: `·` es AND, `+` es OR, `¬` es NOT (se pone delante: ¬Q se lee «no Q») y `⊕` es XOR.

| Operación | En fórmulas | Ejemplo con a = 0b1 y b = 0b0 |
| :--- | :--- | :---: |
| AND | a · b | 0b0 |
| OR | a + b | 0b1 |
| NOT | ¬a | 0b0 |
| XOR | a ⊕ b | 0b1 |
| NAND / NOR / XNOR | ¬(a · b) / ¬(a + b) / ¬(a ⊕ b) | 0b1 / 0b0 / 0b0 |

Ojo con el `+` y con el `·`: en las ecuaciones lógicas `+` es OR y `·` es AND, pero en una cuenta con números (pasos, anchuras, módulos) el `+` es la suma de siempre y la multiplicación se escribe `×`.

Las ondas se dibujan con un carácter por paso: `#` es 1 y `_` es 0. Encima va una regla de pasos: la primera fila son las decenas y la segunda las unidades. Cuando una fila muestra un grupo de bits como un número (la fila CUENTA), escribo su cifra hexadecimal en el paso en que el valor cambia y un `.` mientras sigue igual.

### Paso 1. El modelo temporal del curso (y por qué es solo un modelo)

Estas son las reglas del juego. Valen para todos los circuitos de este documento.

1. El tiempo avanza a saltos: t = 0, 1, 2... Cada salto es un **paso**: no son segundos ni ticks del juego, solo la unidad del modelo. (Ojo: «Paso» con mayúscula numera los apartados de este documento; «paso» con minúscula es esta unidad de tiempo.)
2. El **estado** del circuito en el instante t es una «foto»: el valor de todas las señales (la salida de cada puerta y de cada flip-flop) en ese paso.
3. **Retardo unitario**: cada elemento tarda exactamente 1 paso en reaccionar. Si en el instante t sus entradas valen algo, su salida lo refleja en t+1: salida(t+1) = función(entradas(t)). Los cables (unir una salida con una entrada) no añaden retardo; solo lo añaden los elementos. Las fuentes (el temporizador del Paso 3 y las entradas de prueba) dan su valor en cada paso por definición.
4. **Evaluación síncrona en dos fases**. *Fase 1*: todos los elementos calculan su valor siguiente leyendo **solo** la foto del instante t (nadie escribe todavía). *Fase 2*: se confirman todos los valores a la vez y esa es la foto de t+1. Consecuencia: da igual en qué orden recorras los elementos (he probado 30 órdenes aleatorios en el ejemplo de abajo y el resultado es idéntico).

Un ejemplo mínimo: dos puertas NOT en cadena. La lista de elementos es la que simula el script (una netlist, como las del documento 02 y las del 03). **Ojo, aquí cambia el significado**: en esas netlists cada línea era instantánea (daba el valor final, sin tiempo), mientras que aquí cada línea `y = OP(a, b)` añade un paso: y(t+1) = OP(a(t), b(t)). `arranca en 1` es el valor inicial:

```text
x = ENTRADA
a = NOT(x)   arranca en 1
b = NOT(a)
```

| t | x(t) | a(t) | b(t) |
| ---: | ---: | ---: | ---: |
| 0 | 0 | 1 | 0 |
| 1 | 0 | 1 | 0 |
| 2 | 0 | 1 | 0 |
| 3 | 1 | 1 | 0 |
| 4 | 1 | 0 | 0 |
| 5 | 1 | 0 | 1 |
| 6 | 0 | 0 | 1 |
| 7 | 0 | 1 | 1 |
| 8 | 0 | 1 | 0 |
| 9 | 0 | 1 | 0 |

Lee la tabla: `x` sube a 1 en t=3; `a` lo nota un paso después (cambia en t=4) y `b` otro paso después (cambia en t=5). Dos puertas en cadena, dos pasos de retraso. Y así se ve una transición en las dos fases:

```text
Instante t=3  (foto):  x=1 a=1 b=0
Fase 1 (solo LEE la foto de t=3):  a' = NOT x = NOT 1 = 0 ; b' = NOT a = NOT 1 = 0
Fase 2 (ESCRIBE todo a la vez):  instante t=4  ->  a=0 b=0
```

Fíjate en que `b'` se calcula con el `a` **viejo** (el de la foto), no con el `a'` recién calculado. Eso es lo que hace al modelo independiente del orden.

5. **Flanco de subida**: el instante en que una señal pasa de 0 a 1. Se detecta comparando con el paso anterior: subida(t) = CLK(t) · ¬CLK(t−1). Todo lo que sigue (flip-flops, detector de flancos, contadores) usa esta misma definición.
6. **Flip-flop T** («toggle»): un bit de memoria con una entrada de reloj CLK y una entrada T. Solo actúa cuando hay flanco de subida en CLK: si T = 1 cambia de valor (Q(t+1) = Q(t) ⊕ T(t)) y si T = 0 se queda como está. Para detectar el flanco, el flip-flop recuerda dentro el valor anterior del reloj. Tiene dos salidas que cambian a la vez: Q y su contraria, que se llama `Qn` (= ¬Q), como en el documento 04.

**El flip-flop es aquí una pieza primitiva.** En el documento 04 lo construiste con puertas y allí tardaba varios pasos en reaccionar y exigía que T estuviera quieto cerca del flanco. En este documento lo trato como una caja negra ideal con retardo de 1 paso: Q y Qn cambian a la vez, un paso después del flanco. Las cifras de aquí (periodos mínimos, escalones del ripple) dependen de esa idealización: con el flip-flop del 04 cambian los números, no las ideas, pero cambian mucho. Lo he medido montando en este simulador la misma lista de puertas del 04:

- Con T = 1, Q y Qn cambian 2 o 3 pasos después del flanco (no 1) y no a la vez: en cada cambio hay 1 paso en el que los dos valen 1.
- El divisor de frecuencia con T = 1 necesita un reloj de periodo 6 o más (con el flip-flop ideal, 2). Es el mismo mínimo que mide el documento 04 para el flip-flop T con T = 1 fija. Con un reloj más rápido el flip-flop de puertas no llega a responder a cada flanco y el resultado es incorrecto, no solo más lento.
- Un contador de 4 bits necesita un reloj de periodo 7 si es síncrono (con el flip-flop ideal, 2) y 9 si es ripple (con el ideal, 4). La tabla completa está en «Tablas».

Son los mínimos de **estos** montajes: T = 1 fija, o un T que calcula el propio contador y que solo cambia cuando cambian sus Q, no en un instante cualquiera. No son el mínimo para un T cualquiera: si T puede cambiar en instantes arbitrarios (fuera de la ventana prohibida), el documento 04 mide, con un reloj simétrico, un periodo de 8 o más, porque Q puede tardar hasta 4 pasos en dejar de moverse después del flanco. Por eso el síncrono de 4 bits (periodo 7) puede quedar por debajo de ese valor: su T no es arbitrario. Aquí «periodo» es el número de pasos de un ciclo completo del reloj; en estas medidas el reloj está en alto la mitad del periodo, redondeando hacia abajo, así que con un periodo impar pasa un paso más a 0 que a 1 y no es simétrico como el del 04. Por todo esto los periodos de reloj de este documento solo valen con el flip-flop ideal.

> **Aviso**: esto es un **modelo didáctico**. Sirve para razonar y para que cada cifra de este documento sea reproducible (todas salen de un script de Python del repositorio, `docs/theory/check_C.py`, que simula exactamente estas reglas). El simulador real del mod se decidirá más adelante y podría tener otras reglas: otro retardo por elemento, otra equivalencia entre un paso y el tiempo del juego u otra forma de tratar los flancos.

### Paso 2. Oscilador en anillo: el reloj más simple

Una puerta NOT cuya salida vuelve a su propia entrada nunca se queda quieta: si vale 0 pasa a 1 y si vale 1 pasa a 0. Con retardo unitario, x(t+1) = ¬x(t): un reloj de periodo 2. Para periodos más largos se encadenan n puertas NOT (con **n impar**) y se cierra el anillo. Para n = 3:

```text
      +--------------------------------------------+
      |                                            |
      |     +-----+       +-----+       +-----+    |
      +---->| NOT |------>| NOT |------>| NOT |----+
            +-----+       +-----+       +-----+
              g1            g2            g3
```

```text
g1 = NOT(g3)   arranca en 1
g2 = NOT(g1)
g3 = NOT(g2)   arranca en 1
```

**Cómo arranca.** Imagina que abres el anillo y mantienes a 0 la entrada de g1. La cadena se asienta sola: g1 = 1, g2 = 0, g3 = 1. Es el estado 0b101 (g1=1 g2=0 g3=1; escribo g1 g2 g3 como bits, con g1 el más significativo). Ahora cierras el anillo: g1 mira a g3, que vale 1, así que g1 «debería» valer 0 pero vale 1. Esa puerta en desacuerdo (la llamo **defecto**) es lo único que empuja al anillo.

| t | g1 | g2 | g3 |
| ---: | ---: | ---: | ---: |
| 0 | 1 | 0 | 1 |
| 1 | 0 | 0 | 1 |
| 2 | 0 | 1 | 1 |
| 3 | 0 | 1 | 0 |
| 4 | 1 | 1 | 0 |
| 5 | 1 | 0 | 0 |
| 6 | 1 | 0 | 1 |
| 7 | 0 | 0 | 1 |
| 8 | 0 | 1 | 1 |
| 9 | 0 | 1 | 0 |
| 10 | 1 | 1 | 0 |
| 11 | 1 | 0 | 0 |
| 12 | 1 | 0 | 1 |
| 13 | 0 | 0 | 1 |
| 14 | 0 | 1 | 1 |
| 15 | 0 | 1 | 0 |

```text
         0         1
t        0123456789012345
g1       #___###___###___
g2       __###___###___##
g3       ###___###___###_
```

Qué se ve: (a) cada nodo es el anterior invertido y retrasado un paso; (b) el defecto avanza una puerta por paso; (c) da la vuelta al anillo en n pasos pero **vuelve invertido** (hay n inversiones y n es impar), así que necesita **dos vueltas** para repetirse. Resultado: periodo = 2n = 6 pasos, 3 en alto y 3 en bajo (ciclo de trabajo del 50 %).

Comprobado por simulación para varios n, siempre desde la cadena abierta y asentada:

| n (NOT) | retardo total | periodo previsto 2n | periodo simulado | pasos en 1 | pasos en 0 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 2 | 2 | 1 | 1 |
| 3 | 3 | 6 | 6 | 3 | 3 |
| 5 | 5 | 10 | 10 | 5 | 5 |
| 7 | 7 | 14 | 14 | 7 | 7 |
| 9 | 9 | 18 | 18 | 9 | 9 |

**El estado inicial importa.** Si en el modelo arrancas con todo a 0b000, las 3 puertas están en desacuerdo a la vez, todas cambian a la vez y el anillo oscila con periodo 2, no con 2n:

```text
         0
t        01234567
g1       _#_#_#_#
g2       _#_#_#_#
g3       _#_#_#_#
```

Los 8 estados iniciales posibles de n = 3:

| estado inicial g1 g2 g3 | puertas inestables | periodo |
| :---: | ---: | ---: |
| 0b000 | 3 | 2 |
| 0b001 | 1 | 6 |
| 0b010 | 1 | 6 |
| 0b011 | 1 | 6 |
| 0b100 | 1 | 6 |
| 0b101 | 1 | 6 |
| 0b110 | 1 | 6 |
| 0b111 | 3 | 2 |

Y para otros n, contando cuántos estados iniciales dan cada periodo:

| n | estados iniciales posibles | periodo | cuántos estados lo dan | es 2n? |
| ---: | ---: | ---: | ---: | :---: |
| 1 | 2 | 2 | 2 | sí |
| 3 | 8 | 2 | 2 | no |
| 3 | 8 | 6 | 6 | sí |
| 5 | 32 | 2 | 2 | no |
| 5 | 32 | 10 | 30 | sí |
| 7 | 128 | 2 | 2 | no |
| 7 | 128 | 14 | 126 | sí |
| 9 | 512 | 2 | 2 | no |
| 9 | 512 | 6 | 6 | no |
| 9 | 512 | 18 | 504 | sí |

Lo que dice la simulación: los defectos son siempre una cantidad impar y cada paso se desplazan una puerta. Con **un solo defecto** el periodo es 2n (comprobado para todos los n de la lista: 1, 3, 5, 7, 9); con varios defectos puede salir más corto. Ejemplo: para n = 9 hay 6 estados de periodo 6, que son los de 3 defectos separados 3 puertas entre sí. Lección de diseño: **el estado inicial de un bucle hay que decidirlo a propósito**.

**¿Y con n par?** No hay oscilación fiable: con un número par de inversiones la señal vuelve igual que salió (es lo que se llama realimentación positiva), así que, si le pones uno de sus estados estables, el anillo no tiene motivo para cambiar y se queda en él. Con n = 2 los estados 0b01, 0b10 son estables (el anillo «se acuerda» de un bit: es la base de la memoria; estos circuitos de dos estados estables se llaman biestables, documento 04) y arrancando en 0b00 el modelo síncrono oscila con periodo 2, un efecto del modelo que no debes tomar como comportamiento físico.

**Más retardo, más periodo.** El periodo no depende de cuántas NOT hay sino del **retardo total D** del bucle. Si metes elementos de retardo puro (BUF: copian su entrada con 1 paso de retraso) el periodo sigue siendo 2D mientras el número de inversiones sea impar:

| NOT | BUF (retardo) | retardo total D | periodo simulado | forma |
| ---: | ---: | ---: | ---: | :--- |
| 1 | 0 | 1 | 2 | 1 en 1 / 1 en 0 |
| 1 | 1 | 2 | 4 | 2 en 1 / 2 en 0 |
| 1 | 2 | 3 | 6 | 3 en 1 / 3 en 0 |
| 1 | 3 | 4 | 8 | 4 en 1 / 4 en 0 |
| 3 | 0 | 3 | 6 | 3 en 1 / 3 en 0 |
| 3 | 1 | 4 | 8 | 4 en 1 / 4 en 0 |
| 3 | 2 | 5 | 10 | 5 en 1 / 5 en 0 |
| 5 | 1 | 6 | 12 | 6 en 1 / 6 en 0 |

### Paso 3. Reloj generado por un temporizador

El anillo da relojes simétricos (50 %) y de periodo 2D. Para otro periodo u otro ciclo de trabajo se usa un **temporizador**: un elemento con un contador interno c(t+1) = (c(t) + 1) mod P (una suma aritmética) y salida 1 mientras c ≥ P − H. Tiene dos parámetros: el **periodo** P (pasos por ciclo) y los pasos en alto H. El **ciclo de trabajo** es H/P y la **frecuencia** es 1/P ciclos por paso. Empieza en bajo para que no haya un flanco falso en t=0.

| periodo P (pasos) | pasos en alto H | ciclo de trabajo H/P | frecuencia (ciclos por paso) |
| ---: | ---: | ---: | ---: |
| 8 | 4 | 50 % | 1/8 |
| 8 | 2 | 25 % | 1/8 |
| 4 | 2 | 50 % | 1/4 |
| 2 | 1 | 50 % | 1/2 |

```text
          0         1         2
t         012345678901234567890123
P=8 H=4   ____####____####____####
P=8 H=2   ______##______##______##
P=4 H=2   __##__##__##__##__##__##
P=2 H=1   _#_#_#_#_#_#_#_#_#_#_#_#
```

El temporizador no tiene magia: es un contador (como los del Paso 5, pero módulo P en vez de módulo 2^n) más un comparador. La diferencia con el anillo es que aquí eliges P y H libremente.

### Paso 4. Divisor de frecuencia con un flip-flop T

Un flip-flop T con T = 1 cambia en **cada** flanco de subida: sube en uno, baja en el siguiente. Para completar un ciclo (subir y volver a bajar) hacen falta dos flancos del reloj, es decir, **dos periodos**. Su salida Q tiene periodo doble. Si la salida de cada etapa es el reloj de la siguiente, k etapas multiplican el periodo por 2^k. Aquí, tres etapas con un reloj de periodo 4 (las llamo Z1, Z2 y Z3, y no Q, para no confundirlas con los bits Q0..Q3 del contador del Paso 5):

```text
CLK = TEMPORIZADOR(P=4, H=2)
ONE = CONSTANTE 1
Z1  = FF-T(T=ONE, reloj=CLK)   salidas: Z1, Z1n
Z2  = FF-T(T=ONE, reloj=Z1)   salidas: Z2, Z2n
Z3  = FF-T(T=ONE, reloj=Z2)   salidas: Z3, Z3n
```

```text
         0         1         2         3         4
t        012345678901234567890123456789012345678901234567
CLK      __##__##__##__##__##__##__##__##__##__##__##__##
Z1       ___####____####____####____####____####____####_
Z2       ____########________########________########____
Z3       _____################________________###########
```

| señal | periodo (pasos) | veces el periodo de CLK | pasos en 1 / en 0 | ciclo de trabajo |
| :--- | ---: | ---: | :---: | ---: |
| CLK | 4 | 1 | 2 / 2 | 50 % |
| Z1 | 8 | 2 | 4 / 4 | 50 % |
| Z2 | 16 | 4 | 8 / 8 | 50 % |
| Z3 | 32 | 8 | 16 / 16 | 50 % |

Los periodos medidos son 4, 8, 16, 32: cada etapa dobla al anterior. Ojo al retardo: Z1 cambia un paso **después** del flanco que la provoca, Z2 otro paso después, y así sucesivamente.

Hay una propiedad útil: la salida queda siempre al 50 %, sea cual sea el ciclo de trabajo de la entrada. Con una entrada en alto 1 de cada 4 pasos (25 %), la salida está en alto 4 de cada 8 pasos (50 %). La razón es que Q solo cambia en los flancos de subida, que están separados un periodo exacto: se queda en alto un periodo de entrada completo y en bajo otro.

### Paso 5. Contadores

Un contador de n bits recorre las cuentas desde 0 hasta 2^n − 1 y vuelve a 0. Con n = 4 son 16 cuentas, de 0b0000 (0x0) a 0b1111 (0xF); la lista completa está en «Tablas». Cada bit cambia la mitad de veces que el anterior:

| bit | cambia cada ... cuentas | periodo (en cuentas) | cambia cuando ... |
| :--- | ---: | ---: | :--- |
| Q0 | 1 | 2 | cada flanco del reloj |
| Q1 | 2 | 4 | Q0 pasa de 1 a 0 |
| Q2 | 4 | 8 | Q1 pasa de 1 a 0 |
| Q3 | 8 | 16 | Q2 pasa de 1 a 0 |

Para comparar contadores uso **lecturas**: el valor de los cuatro bits en el paso de cada flanco de reloj, que es justo lo que capturaría un flip-flop que los mire.

#### 5.1 Contador asíncrono (ripple)

La etapa 0 usa el reloj externo. Cada etapa siguiente usa como reloj la salida `Qn` de la anterior: cuando Q(k−1) baja de 1 a 0 (el «acarreo», como pasar de 9 a 0 en un cuentakilómetros), `Qn` sube y eso es el flanco que despierta a la etapa k. Lista de elementos (R0..R3 son los bits Q0..Q3; `R0n` es la contraria de R0):

```text
CLK = TEMPORIZADOR(P=8, H=4)
ONE = CONSTANTE 1
R0  = FF-T(T=ONE, reloj=CLK)   salidas: R0, R0n
R1  = FF-T(T=ONE, reloj=R0n)   salidas: R1, R1n
R2  = FF-T(T=ONE, reloj=R1n)   salidas: R2, R2n
R3  = FF-T(T=ONE, reloj=R2n)   salidas: R3, R3n
```

Si conectaras `Q` en vez de `Qn`, el contador contaría hacia atrás: las primeras lecturas serían 0x0, 0xF, 0xE, 0xD, 0xC, 0xB.

El problema es el nombre: *ripple* significa «ondulación». Cada etapa espera a la anterior, así que los bits cambian **escalonados** (es el mismo problema que el del sumador ripple-carry del documento 05: el acarreo viaja de etapa en etapa y el resultado tarda en asentarse). Alrededor de la cuenta 7 → 8 (de 0b0111 a 0b1000), con el mismo reloj, comparado con el contador síncrono de la siguiente sección (`e` es el paso en el que el flip-flop 0 ve el flanco):

| paso | relativo | CLK | ripple Q3 Q2 Q1 Q0 | ripple (decimal) | síncrono Q3 Q2 Q1 Q0 | síncrono (decimal) |
| ---: | :---: | ---: | :---: | ---: | :---: | ---: |
| 59 | e-1 | 0 | 0b0111 | 7 | 0b0111 | 7 |
| 60 | e | 1 | 0b0111 | 7 | 0b0111 | 7 |
| 61 | e+1 | 1 | 0b0110 | 6 | 0b1000 | 8 |
| 62 | e+2 | 1 | 0b0100 | 4 | 0b1000 | 8 |
| 63 | e+3 | 1 | 0b0000 | 0 | 0b1000 | 8 |
| 64 | e+4 | 0 | 0b1000 | 8 | 0b1000 | 8 |
| 65 | e+5 | 0 | 0b1000 | 8 | 0b1000 | 8 |
| 66 | e+6 | 0 | 0b1000 | 8 | 0b1000 | 8 |

```text
             6
t        67890123456789
CLK      ____####____##
Q0       #####________#
Q1       ######________
Q2       #######_______
Q3       ________######
```

Y el ciclo completo del ripple, con el reloj más rápido que aguanta (periodo 4). En la fila CUENTA, entre una cuenta y la siguiente, se cuelan los valores fantasma (cifras de más):

```text
         0         1         2         3         4         5         6
t        0123456789012345678901234567890123456789012345678901234567890123456789
CLK      __##__##__##__##__##__##__##__##__##__##__##__##__##__##__##__##__##__
Q0       ___####____####____####____####____####____####____####____####____###
Q1       ________########________########________########________########______
Q2       _________________################________________################_____
Q3       __________________________________################################____
CUENTA   0..1...02..3...204.5...46..7...64089...8A..B...A8C.D...CE..F...EC801..
```

El ripple pasa por 0b0110, 0b0100, 0b0000: valores fantasma que la cuenta nunca debería mostrar. Si alguien lee el contador en mitad de la onda, lee basura. La onda tarda 4 pasos en asentarse (un paso por etapa).

**Límite de velocidad.** Si el siguiente flanco llega antes de que acabe la onda, la lectura sale mal. Con un reloj de periodo 2:

| flanco j | lectura esperada | ripple lee | síncrono lee | ripple acierta |
| ---: | :---: | :---: | :---: | :---: |
| 0 | 0b0000 | 0b0000 | 0b0000 | sí |
| 1 | 0b0001 | 0b0001 | 0b0001 | sí |
| 2 | 0b0010 | 0b0010 | 0b0010 | sí |
| 3 | 0b0011 | 0b0011 | 0b0011 | sí |
| 4 | 0b0100 | 0b0000 | 0b0100 | NO |
| 5 | 0b0101 | 0b0101 | 0b0101 | sí |
| 6 | 0b0110 | 0b0110 | 0b0110 | sí |
| 7 | 0b0111 | 0b0111 | 0b0111 | sí |
| 8 | 0b1000 | 0b0100 | 0b1000 | NO |
| 9 | 0b1001 | 0b1001 | 0b1001 | sí |
| 10 | 0b1010 | 0b1010 | 0b1010 | sí |
| 11 | 0b1011 | 0b1011 | 0b1011 | sí |

El ripple falla en 2 de 12 lecturas (la primera en el flanco 4), justo cuando el acarreo tiene que cruzar varias etapas; el síncrono acierta todas. Buscando el menor periodo P que da lecturas correctas para n = 2..8 bits (tabla completa más abajo) sale, con el flip-flop ideal, que **P mínimo = n**: la onda tarda un paso por etapa, así que cuantos más bits, más lento tiene que ir el reloj.

#### 5.2 Contador síncrono

Aquí todos los flip-flops comparten el mismo reloj, así que todos cambian **en el mismo paso**. Para decidir quién debe cambiar se mira el acarreo: la etapa k cambia cuando todos los bits inferiores valen 1, es decir, T_k = Q0 · Q1 · ... · Q(k−1), con T_0 = 1. Ejemplo: de 0b0111 a 0b1000 cambian los cuatro bits a la vez porque T_3 = Q0 · Q1 · Q2 = 1.

```text
CLK = TEMPORIZADOR(P=8, H=4)
ONE = CONSTANTE 1
S0  = FF-T(T=ONE, reloj=CLK)   salidas: S0, S0n
S1  = FF-T(T=S0, reloj=CLK)   salidas: S1, S1n
A2  = AND(S0, S1)
S2  = FF-T(T=A2, reloj=CLK)   salidas: S2, S2n
A3  = AND(S0, S1, S2)
S3  = FF-T(T=A3, reloj=CLK)   salidas: S3, S3n
```

Mismo intervalo que antes, ahora para el síncrono (S0..S3 de la lista son los bits Q0..Q3):

```text
             6
t        67890123456789
CLK      ____####____##
Q0       #####________#
Q1       #####_________
Q2       #####_________
Q3       _____#########
```

Y un ciclo completo con un reloj de periodo 2 (la fila CUENTA muestra una cifra por cuenta y vuelve a 0x0 en el paso 32):

```text
         0         1         2         3
t        0123456789012345678901234567890123
CLK      _#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#
Q0       __##__##__##__##__##__##__##__##__
Q1       ____####____####____####____####__
Q2       ________########________########__
Q3       ________________################__
CUENTA   0.1.2.3.4.5.6.7.8.9.A.B.C.D.E.F.0.
```

El precio son las puertas AND. En el modelo, una AND de varias entradas tarda 1 paso, así que en las simulaciones, con el flip-flop ideal, el síncrono admite un reloj de periodo 2 con todos los tamaños probados (de 2 a 8 bits). Con el flip-flop T de puertas del documento 04 el periodo mínimo es mayor y crece con el tamaño (7 para 4 bits; tabla en «Tablas»). Si solo dispones de ANDs de 2 entradas y las encadenas, el retardo vuelve a crecer: el acarreo ahora ondula por las puertas en vez de por los flip-flops (última columna de la tabla de P mínimo).

Regla práctica: **ripple** = barato, lento y con valores fantasma; **síncrono** = más puertas, rápido y todos los bits a la vez.

### Paso 6. Detector de flanco (generador de pulso)

Quieres una señal que valga 1 **un solo paso** cuando x sube, aunque x se quede en 1 mucho tiempo. Truco: comparar x con x retrasada. En subida, x ya vale 1 pero la copia retrasada todavía vale 0. Aquí el propio NOT hace de retardo (tarda 1 paso), así que nx(t) = ¬x(t−1) y no hace falta ningún elemento más:

```text
x      = ENTRADA
nx     = NOT(x)   arranca en 1
SUBIDA = AND(x, nx)
BAJADA = NOR(x, nx)
AMBOS  = XNOR(x, nx)
```

- SUBIDA(t+1) = x(t) · ¬x(t−1): 1 solo cuando x acaba de subir.
- BAJADA(t+1) = ¬x(t) · x(t−1) (es el NOR de x y nx): 1 solo cuando x acaba de bajar.
- AMBOS(t+1) = x(t) ⊕ x(t−1) (es el XNOR de x y nx): 1 en cualquier cambio.

```text
         0         1         2
t        012345678901234567890123
x        ___####___#__##_____#___
nx       ####____###_##__#####_##
SUBIDA   ____#______#__#______#__
BAJADA   ________#___#___#_____#_
AMBOS    ____#___#__##_#_#____##_
```

Las subidas de x producen pulsos que empiezan en los pasos 4, 11, 14, 21 y las bajadas en 8, 12, 16, 22; todos duran exactamente 1 paso.

**Pulso más ancho.** Si entre x y el NOT metes k elementos BUF más, la copia retrasada llega k pasos más tarde y el pulso dura 1 + k pasos (como mucho lo que dure la entrada):

| k (BUF extra) | entrada de 1 paso(s) | entrada de 2 paso(s) | entrada de 3 paso(s) | entrada de 6 paso(s) |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 1 | 1 | 1 | 1 |
| 1 | 1 | 2 | 2 | 2 |
| 2 | 1 | 2 | 3 | 3 |
| 3 | 1 | 2 | 3 | 4 |

### Paso 7. Alargador de pulso

Es lo contrario: un pulso corto de entrada, un pulso largo de salida. Se hace con un OR de N copias de x, cada una retrasada un paso más: la salida vale 1 si x valió 1 en cualquiera de los N últimos pasos. Con N = 4:

```text
x     = ENTRADA
b1    = BUF(x)
b2    = BUF(b1)
b3    = BUF(b2)
ANCHO = OR(x, b1, b2, b3)
```

ANCHO(t+1) = x(t) + x(t−1) + x(t−2) + x(t−3) (aquí `+` es OR). Cada 1 de entrada se «mantiene» N − 1 pasos de más:

```text
         0         1         2
t        01234567890123456789012345
x        __#_____##_____#_#________
ANCHO    ___####__#####__######____
```

La anchura de salida, contada en pasos (suma aritmética, no OR), es W + N − 1:

| anchura de entrada W | anchura de salida | fórmula W + N - 1 |
| ---: | ---: | :---: |
| 1 | 4 | 1 + 4 - 1 |
| 2 | 5 | 2 + 4 - 1 |
| 3 | 6 | 3 + 4 - 1 |

Fíjate en los dos pulsos cercanos del final: sus ventanas se solapan y se funden en uno solo (el alargador es **reactivable**: cada nuevo pulso vuelve a empezar la cuenta).

Hay otra forma de construirlo: un contador que se recarga a N cada vez que x vale 1 y baja de uno en uno; la salida vale 1 mientras el contador no sea 0. Es el mismo circuito visto de otra manera: lo he comprobado con las 4096 entradas posibles de 12 pasos y las salidas coinciden en todas.

## Ejemplo resuelto completo: un «latido» cada 16 pasos

**Problema.** Tienes un temporizador de periodo 2 (H = 1). Quieres una señal PULSO que dé un pulso de 1 paso cada 16 pasos y otra, ANCHO, con esos pulsos alargados a 4 pasos.

**(a) Dividir.** Hace falta periodo 16 = 2 × 2^k, luego k = 3 flip-flops T en cascada. Su última salida es Z3 y, medido en la simulación, tiene periodo 16.

**(b) Detectar el flanco de subida de Z3.** AND(Z3, NOT Z3) da un pulso de 1 paso cada vez que Z3 sube.

**(c) Alargar.** OR de PULSO con tres copias retrasadas: 4 pasos de ancho.

```text
CLK   = TEMPORIZADOR(P=2, H=1)
ONE   = CONSTANTE 1
Z1    = FF-T(T=ONE, reloj=CLK)   salidas: Z1, Z1n
Z2    = FF-T(T=ONE, reloj=Z1)   salidas: Z2, Z2n
Z3    = FF-T(T=ONE, reloj=Z2)   salidas: Z3, Z3n
nZ3   = NOT(Z3)   arranca en 1
PULSO = AND(Z3, nZ3)
b1    = BUF(PULSO)
b2    = BUF(b1)
b3    = BUF(b2)
ANCHO = OR(PULSO, b1, b2, b3)
```

```text
         0         1         2         3         4         5
t        01234567890123456789012345678901234567890123456789012345
CLK      _#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#_#
Z1       __##__##__##__##__##__##__##__##__##__##__##__##__##__##
Z2       ___####____####____####____####____####____####____####_
Z3       ____########________########________########________####
PULSO    _____#_______________#_______________#_______________#__
ANCHO    ______####____________####____________####____________##
```

Los pulsos de PULSO empiezan en los pasos 5, 21, 37, 53: separación 16, justo lo previsto. ANCHO dura 4 pasos. El primer pulso no aparece en el paso 0 sino en el 5: es el **retardo acumulado**, porque cada elemento de la cadena (flip-flops, NOT, AND) suma 1 paso. Lo que importa para un diseño es la separación entre pulsos, no ese arranque.

**Con el flip-flop T de puertas del documento 04** este montaje no funciona con el reloj de periodo 2: el flip-flop no llega a responder a cada flanco. El reloj más rápido que aguanta tiene periodo 6, y entonces el latido sale cada 48 pasos (6 × 8) en lugar de cada 16.

## Tablas

Las 16 cuentas de un contador de 4 bits:

| cuenta | binario (Q3 Q2 Q1 Q0) | hex |
| ---: | :---: | :---: |
| 0 | 0b0000 | 0x0 |
| 1 | 0b0001 | 0x1 |
| 2 | 0b0010 | 0x2 |
| 3 | 0b0011 | 0x3 |
| 4 | 0b0100 | 0x4 |
| 5 | 0b0101 | 0x5 |
| 6 | 0b0110 | 0x6 |
| 7 | 0b0111 | 0x7 |
| 8 | 0b1000 | 0x8 |
| 9 | 0b1001 | 0x9 |
| 10 | 0b1010 | 0xA |
| 11 | 0b1011 | 0xB |
| 12 | 0b1100 | 0xC |
| 13 | 0b1101 | 0xD |
| 14 | 0b1110 | 0xE |
| 15 | 0b1111 | 0xF |

Periodo de reloj mínimo para que las lecturas sean correctas con el flip-flop ideal (comprobado simulando cada contador con relojes de periodo creciente; el reloj está en alto la mitad del periodo, redondeando hacia abajo):

| bits n | cuentas (2^n) | P mínimo ripple | P mínimo síncrono (AND ancha) | P mínimo síncrono (ANDs de 2 entradas) |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 4 | 2 | 2 | 2 |
| 3 | 8 | 3 | 2 | 2 |
| 4 | 16 | 4 | 2 | 3 |
| 5 | 32 | 5 | 2 | 4 |
| 6 | 64 | 6 | 2 | 5 |
| 7 | 128 | 7 | 2 | 6 |
| 8 | 256 | 8 | 2 | 7 |

Lo mismo con el flip-flop T de puertas del documento 04 (mismo simulador y mismo criterio, de 2 a 6 bits; la columna ideal se repite para comparar):

| bits n | ripple con flip-flop ideal | ripple con flip-flop T de puertas | síncrono (AND ancha) con flip-flop ideal | síncrono (AND ancha) con flip-flop T de puertas |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 2 | 6 | 2 | 6 |
| 3 | 3 | 7 | 2 | 7 |
| 4 | 4 | 9 | 2 | 7 |
| 5 | 5 | 11 | 2 | 8 |
| 6 | 6 | 13 | 2 | 8 |

Recetas rápidas:

| Necesito... | Montaje | Resultado comprobado por simulación |
| :--- | :--- | :--- |
| reloj de periodo P y H pasos en alto | temporizador(P, H) | con P=8 y H=4: periodo y ancho exactos |
| reloj con periodo 2D y 50 % | número impar de NOT + BUF, retardo total D | periodo 2D (tabla del anillo) |
| dividir la frecuencia entre 2^k | k flip-flops T en cascada (T=1) | periodos 4, 8, 16, 32 con k = 0, 1, 2, 3 |
| contar de 0 a 2^n - 1 | n flip-flops T (ripple o síncrono) | 16 cuentas con n = 4 |
| pulso de 1 paso en un flanco de subida | AND(x, NOT x), el NOT es el retardo | ancho 1 sea cual sea la entrada |
| alargar un pulso | OR de N copias retrasadas | ancho W + N - 1 (N = 4) |

## Mini-ejercicio

Tienes un reloj de periodo 4 pasos (temporizador con H = 2).

(a) ¿Cuántos flip-flops T en cascada necesitas para obtener una señal de periodo 32 pasos?

(b) Quieres usar un contador ripple de 10 bits hecho con flip-flops ideales. Extrapolando la tabla de P mínimo del flip-flop ideal, ¿cuál es el periodo de reloj mínimo para que sus lecturas sean fiables?

---

**Solución**

(a) 3 flip-flops. Cada etapa dobla el periodo: 4 -> 8 -> 16 -> 32. Con solo dos etapas el periodo sería 16.

```text
         0         1         2         3
t        0123456789012345678901234567890123456789
CLK      __##__##__##__##__##__##__##__##__##__##
Z1       ___####____####____####____####____####_
Z2       ____########________########________####
Z3       _____################________________###
```

(b) P mínimo = n, luego 10 pasos (con flip-flops ideales). Lo he comprobado simulando el ripple de 10 bits: con ese periodo todas las lecturas son correctas.

## En Redstone / en Satisfactory
- **Redstone**: según la wiki de Minecraft, la antorcha de redstone invierte la señal («como una puerta NOT») y tarda 2 game ticks en cambiar ([Redstone Torch](https://minecraft.wiki/w/Redstone_Torch)); el repetidor añade un retardo ajustable de 2, 4, 6 u 8 game ticks ([Redstone Repeater](https://minecraft.wiki/w/Redstone_Repeater)); los relojes se hacen cerrando bucles de repetidores y antorchas, y un flip-flop T sirve para duplicar el periodo de cualquier reloj ([Redstone circuits/Clock](https://minecraft.wiki/w/Redstone_circuits/Clock)). Es el anillo y el divisor de este documento.
- **Ojo con Minecraft**: según la wiki, una antorcha que se apaga a la fuerza más de 8 veces en 60 game ticks se «quema» ([Redstone Torch](https://minecraft.wiki/w/Redstone_Torch)), y un bucle de una sola antorcha, como el anillo de 1 NOT de periodo 2 de este documento, es el caso típico. Nuestro modelo no tiene esa protección. Además Java y Bedrock difieren en este detalle (en Bedrock, según la wiki, solo se quema si la antorcha se alimenta a sí misma sin retardo), así que las cifras de Minecraft son una referencia, no una especificación.
- **Satisfactory**: el mod aún no está diseñado, así que aquí solo quedan preguntas abiertas: ¿cuánto tiempo del juego dura un paso?, ¿cada pieza añade 1 paso de retardo?, ¿habrá un temporizador como pieza primitiva o se construirá con bucles?, ¿con qué estado arranca un bucle?

## Resumen
- Un reloj es una señal periódica con periodo P y ciclo de trabajo H/P; un anillo de n NOT (n impar, arrancado desde la cadena asentada) da periodo 2n, y con retardo total D da 2D.
- Un flip-flop T con T = 1 divide la frecuencia entre 2 y deja la salida al 50 %; k etapas en cascada dividen entre 2^k.
- Un contador de n bits cuenta de 0 a 2^n − 1: el ripple es simple pero sus bits cambian escalonados (valores fantasma) y, con el flip-flop ideal, exige un periodo de reloj mínimo igual a n; el síncrono cambia todos los bits a la vez con T_k = Q0 · ... · Q(k−1). Con el flip-flop T de puertas del documento 04 todos esos periodos mínimos son mayores (tabla en «Tablas»).
- El detector de flanco compara x con x retrasada (x · ¬x retrasada) y da un pulso de 1 paso; el NOT ya hace de retardo.
- El alargador es un OR de N copias retrasadas (o un contador que se recarga): el ancho pasa a W + N − 1.
