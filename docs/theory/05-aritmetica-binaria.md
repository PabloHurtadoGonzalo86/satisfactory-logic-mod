# 05 · Aritmética binaria: sumadores, complemento a dos y desbordamiento

## Objetivo

Construir con puertas un sumador de 4 bits (semisumador, sumador completo y acarreo en cadena) y entender el complemento a dos, el desbordamiento y la resta con ese mismo sumador.  
Al final sabrás cuántos pasos tarda en asentarse el resultado en el modelo de retardo unitario, medido con un simulador, no estimado a ojo.

## Idea intuitiva

Sumar en binario es sumar en decimal, columna a columna de derecha a izquierda, con el famoso "me llevo una". En cada columna hay que sumar **tres** bits (el de A, el de B y el acarreo que llega de la columna anterior) y producir **dos**: el bit de la suma de esa columna y el acarreo que sale hacia la siguiente.

Ese trabajo de una columna es un circuito pequeño, el **sumador completo**. Si pones cuatro en fila, pasándose el acarreo de uno a otro, tienes un sumador de 4 bits. Todo lo demás de este documento sale de ahí:

- Los números negativos no necesitan un sumador distinto: con una convención (el **complemento a dos**) el mismo circuito suma con y sin signo, y solo cambia cómo lees los bits.
- Restar tampoco necesita un restador: se suma el negativo.
- Como el sumador tiene un tamaño fijo, a veces el resultado **no cabe** (desbordamiento) y hay que detectarlo.
- Y el acarreo tiene que recorrer la cadena de columnas, así que el resultado no está listo de golpe: **hay que esperar**.

## Explicación paso a paso

### Paso 0. Notación, el signo + y el modelo temporal

> **Notación de la serie.** Bits como `0b0110`, hexadecimal como `0x6`; operadores AND, OR, NOT, XOR, NAND, NOR y XNOR. En fórmulas lógicas: `·` es AND, `+` es OR, `¬` es NOT y `⊕` es XOR.

Aviso sobre el `+`, porque aquí conviven dos: en las **ecuaciones lógicas** (los bloques de ecuaciones) significa OR; en una **suma de números** (por ejemplo, las sumas con prefijo `0b` o `0x` y las columnas "suma aritmética" de las tablas) es la suma de siempre. Cuando haga falta multiplicar uso `×`, nunca `·`, que es AND.

Modelo temporal (recordatorio del documento 04): cada puerta tarda **un paso** en reaccionar (retardo unitario) y todo se actualiza en **dos fases**: primero todas las puertas calculan su salida de t+1 leyendo solo el instante t, y después se confirman todas a la vez. Es un **modelo didáctico**; el simulador real del mod se decidirá más adelante. Un sumador no tiene memoria, pero con este modelo sí se puede preguntar cuándo está listo su resultado (Paso 7). Ojo con la palabra: «Paso 3», con mayúscula y número, es un apartado de este documento; «paso», en minúscula, es siempre la unidad de tiempo del modelo.

### Paso 1. El semisumador: sumar dos bits

Un **semisumador** suma dos bits, A y B, y devuelve dos: la suma S y el acarreo C. S vale 1 cuando los dos bits son distintos (es una XOR); C vale 1 solo cuando los dos son 1 (es una AND), igual que en decimal cuando una columna se pasa y escribes la cifra de abajo y te llevas una.

```text
         +---------------+
  A -----|               |----- S  (A XOR B)
         |  semisumador  |
  B -----|               |----- C  (A AND B)
         +---------------+
```

- S = A ⊕ B
- C = A · B

La última columna de la tabla comprueba que el par (C, S) escrito como número es de verdad la suma aritmética de los dos bits:

**Tabla 5.1. Semisumador**

| A | B | S = A ⊕ B | C = A · B | suma aritmética = 2×C + S |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 = 2×0 + 0 |
| 0 | 1 | 1 | 0 | 1 = 2×0 + 1 |
| 1 | 0 | 1 | 0 | 1 = 2×0 + 1 |
| 1 | 1 | 0 | 1 | 2 = 2×1 + 0 |

El semisumador son 2 puertas (una XOR y una AND) y sus dos salidas se asientan en 1 paso.

Le falta una cosa: no tiene entrada para el acarreo que llega de la columna anterior. Por eso solo vale para la primera columna.

### Paso 2. El sumador completo: sumar tres bits

El **sumador completo** suma A, B y el acarreo de entrada Cin, y devuelve S y el acarreo de salida Cout. Se hace con **dos semisumadores y una OR**: el primero suma A y B; el segundo suma ese resultado parcial con Cin; y el acarreo sale si cualquiera de los dos semisumadores generó uno.

```text
         +---------------+
  A -----|               |
  B -----|   sumador     |----- S
  Cin ---|   completo    |----- Cout
         +---------------+
```

```text
  A, B   ---> [ semisumador 1 ] ---> p (suma parcial) y g (acarreo parcial)
  p, Cin ---> [ semisumador 2 ] ---> S (suma final)   y pc (acarreo parcial)
  g, pc  ---> [ OR ]            ---> Cout
```

- p = A ⊕ B
- g = A · B
- S = p ⊕ Cin
- pc = p · Cin
- Cout = g + pc

Las letras p y g son las que usa la literatura de sumadores rápidos, y conviene aprenderlas con su significado. **g** (*generate*, «generar») vale 1 cuando A y B generan un acarreo ellos solos (los dos valen 1). **p** (*propagate*, «propagar») vale 1 cuando solo uno de los dos vale 1: entonces, si llega un acarreo por Cin, pasa de largo (se propaga). Por eso **pc = p · Cin** es el acarreo que llega y se propaga, y se puede leer Cout = g + p · Cin: sale acarreo si se genera uno aquí o si llega uno y se propaga. Compactado:

- S    = A ⊕ B ⊕ Cin
- Cout = A · B + Cin · (A ⊕ B)

**Tabla 5.2. Sumador completo**

| A | B | Cin | S | Cout | suma aritmética = 2×Cout + S |
|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | 0 = 2×0 + 0 |
| 0 | 0 | 1 | 1 | 0 | 1 = 2×0 + 1 |
| 0 | 1 | 0 | 1 | 0 | 1 = 2×0 + 1 |
| 0 | 1 | 1 | 0 | 1 | 2 = 2×1 + 0 |
| 1 | 0 | 0 | 1 | 0 | 1 = 2×0 + 1 |
| 1 | 0 | 1 | 0 | 1 | 2 = 2×1 + 0 |
| 1 | 1 | 0 | 0 | 1 | 2 = 2×1 + 0 |
| 1 | 1 | 1 | 1 | 1 | 3 = 2×1 + 1 |

Esas puertas, en el modelo de retardo unitario, no tardan lo mismo desde todas las entradas. El camino más largo (mayor número de puertas seguidas) desde cada entrada hasta cada salida:

**Tabla 5.3. Pasos desde cada entrada hasta cada salida del sumador completo**

| entrada que cambia | pasos hasta S | pasos hasta Cout (camino más largo) |
|---|---|---|
| A | 2 | 3 |
| B | 2 | 3 |
| Cin | 1 | 2 |

El sumador completo son 5 puertas (dos XOR, dos AND y una OR). Lo importante para más adelante: desde Cin hasta Cout hay 2 puertas en el camino más largo, mientras que desde A o B hasta Cout hay 3. Guarda ese dato: lo que más cuenta al encadenar sumadores es el camino de Cin a Cout.

Variante solo con puertas NAND (por si el mod ofreciera una sola puerta; el documento 03 explica por qué se puede):

```text
u(t+1) = NAND(A(t), B(t))
v(t+1) = NAND(A(t), u(t))
w(t+1) = NAND(B(t), u(t))
p(t+1) = NAND(v(t), w(t))
y(t+1) = NAND(p(t), Cin(t))
z(t+1) = NAND(p(t), y(t))
q(t+1) = NAND(Cin(t), y(t))
S(t+1) = NAND(z(t), q(t))
Cout(t+1) = NAND(u(t), y(t))
```

Con solo puertas NAND el sumador completo necesita 9 puertas (comprobado en las 8 combinaciones); el camino de Cin a Cout sigue teniendo 2 puertas, pero de A o B a Cout hay 5 y de A o B a S hay 6.

### Paso 3. Sumador de 4 bits con acarreo en cadena (ripple-carry)

Para sumar números de 4 bits (A3 A2 A1 A0 con B3 B2 B1 B0) se colocan cuatro sumadores completos en fila. El bit 0 recibe el acarreo inicial Cin (que vale 0 en una suma normal), y el acarreo de salida de cada sumador entra en el siguiente:

```text
        A3  B3           A2  B2           A1  B1           A0  B0
         |   |            |   |            |   |            |   |
      +--v---v--+      +--v---v--+      +--v---v--+      +--v---v--+
C4 <--|   FA3   |<-C3--|   FA2   |<-C2--|   FA1   |<-C1--|   FA0   |<-- Cin
      +----+----+      +----+----+      +----+----+      +----+----+
           |                |                |                |
          S3               S2               S1               S0
```

Como cada sumador tiene que esperar el acarreo del anterior, el acarreo "ondula" (en inglés, *ripple*) de derecha a izquierda, y por eso se llama sumador **ripple-carry** o de acarreo en cadena. La wiki de Minecraft usa el mismo nombre: "adders that calculate carries one at a time in this fashion are called Ripple Carry adders" (fuente en la última sección). Las ecuaciones de los dos primeros bits del circuito (los demás son iguales):

```text
p0(t+1) = XOR(A0(t), B0(t))
S0(t+1) = XOR(p0(t), Cin(t))
g0(t+1) = AND(A0(t), B0(t))
pc0(t+1) = AND(p0(t), Cin(t))
C1(t+1) = OR(g0(t), pc0(t))
p1(t+1) = XOR(A1(t), B1(t))
S1(t+1) = XOR(p1(t), C1(t))
g1(t+1) = AND(A1(t), B1(t))
pc1(t+1) = AND(p1(t), C1(t))
C2(t+1) = OR(g1(t), pc1(t))
```

- El sumador de 4 bits son 20 puertas (4 sumadores completos de 5 puertas) y 9 entradas (A0 a A3, B0 a B3 y Cin).
- Comprobado con el simulador: el sumador de 4 bits hecho de puertas da, en las 512 combinaciones posibles de A, B y Cin, un resultado de cinco bits (C4 y S3 a S0) igual a la suma aritmética.

### Paso 4. Complemento a dos: números negativos con el mismo circuito

Para repartir los patrones de 4 bits entre positivos y negativos se usa el **complemento a dos**: el bit de la izquierda es el **bit de signo** y pesa en negativo, como muestra la fórmula:

- valor con signo de b3 b2 b1 b0 = -8×b3 + 4×b2 + 2×b1 + 1×b0

Todos los patrones leídos de las dos formas están en la Tabla 5.7 (más abajo). Tres cosas que salen de esa tabla:

- Con 4 bits hay 16 patrones: sin signo representan de 0 a 15; con signo, de -8 a 7.
- **Los bits de la suma son los mismos** lees como lo leas: el sumador no sabe si estás usando números con signo o sin signo; solo cambia la interpretación que haces del resultado. La razón está justo debajo, en «Por qué el mismo sumador sirve para los dos».
- **Negar un número** es invertir todos sus bits y sumar 1 (columna "NOT patrón + 1" de la tabla). Por qué funciona: cualquier patrón x y NOT x suman siempre 0b1111 (comprobado con los 16 patrones); sumando 1 se llega a 0b10000 (16), que con 4 bits es 0b0000: módulo 16 vale 0. Es decir, NOT x + 1 hace de -x.

Excepción: negar 0b1000 (que vale -8) da otra vez 0b1000 (-8), porque 8 no cabe en 4 bits con signo.

**Por qué el mismo sumador sirve para los dos.** Si vienes de PHP o Python, estás acostumbrado a enteros con tantos bits que casi nunca se nota su ancho; aquí son solo 4, y eso lo explica todo. Con 4 bits todo se calcula módulo 16: es como un cuentakilómetros de 4 dígitos binarios, que después del 15 vuelve al 0. Por eso el acarreo que sale del último bit se descarta, y en PHP o Python `& 0b1111` hace exactamente lo mismo (quedarse con los 4 bits de la derecha).

Con esa idea, el bit de signo solo cambia cómo se lee un patrón: pesa -8 en vez de +8, una diferencia de 16. Si b3 = 0, el patrón vale k (lo que vale sin signo); si b3 = 1, vale k - 16. Por ejemplo, 0b1101 es 13 sin signo y 13 - 16 = -3 con signo. Como k y k - 16 son el mismo número módulo 16, el sumador, que se queda con los 4 bits de la derecha, entrega los mismos bits con las dos lecturas.

Ejemplo: 0b1101 + 0b0101. Sin signo es 13 + 5 = 18, y 18 módulo 16 es 2; con signo es (-3) + 5 = 2. En los dos casos el sumador entrega el mismo patrón, 0b0010. Lo que cambia es si ese patrón es la respuesta correcta: con signo sí, porque 2 cabe en -8 a 7; sin signo no, porque 18 no cabe en 4 bits (C = 1). Saber si el patrón es la respuesta correcta es justo lo que avisan C (sin signo) y V (con signo) en el Paso 5, y el Ejemplo 3 de más abajo lo recorre bit a bit.

### Paso 5. Desbordamiento: cuando el resultado no cabe

Un sumador de 4 bits solo guarda 4 bits del resultado. Si la suma real no cabe, lo que sobra se pierde y hay que **avisar**. Hay dos formas de que no quepa, y dependen de cómo leas los bits:

- **Sin signo:** la suma no cabe si pasa del máximo (todos los bits en 1). El acarreo que sale del último sumador, **C**, vale 1 en ese caso.
- **Con signo:** la suma no cabe si cae fuera del rango del complemento a dos. El indicador es **V** (de *overflow*) y se calcula con los dos últimos acarreos.

Los dos indicadores, en fórmulas:

- C = C4
- V = C3 ⊕ C4

Intuición de V: con signo, un acarreo que **entra** en el bit de signo pero no sale (o al revés) significa que el bit de signo se ha ensuciado con un desbordamiento. Si los dos coinciden, el bit de signo es el correcto.

Comprobado con las 256 sumas (A y B de 4 bits, con Cin = 0) y el sumador hecho de puertas: V = C3 ⊕ C4 coincide siempre con la regla de los signos y con comprobar a mano si la suma cabe entre -8 y 7; y C = C4 coincide siempre con comprobar si la suma sin signo pasa de 15.

C y V son preguntas distintas y pueden dar cualquier combinación (Tabla 5.8). Para V hay además una regla de los signos que no necesita mirar acarreos (Tabla 5.9): hay desbordamiento con signo solo cuando los dos operandos tienen el **mismo** signo y el resultado tiene el **contrario**; mezclar un positivo con un negativo nunca desborda.

### Paso 6. Restar con el mismo sumador

Como negar es "invertir y sumar 1" (Paso 4), restar es: A - B = A + NOT B + 1 (aquí NOT B es B con todos sus bits invertidos, y los dos `+` son sumas aritméticas). El circuito es el mismo sumador con dos cambios:

- Cada bit de B pasa antes por una XOR con una señal **SUB**: con SUB=0, B pasa igual (XOR con 0); con SUB=1, B sale invertido (XOR con 1).
- La misma señal SUB entra como acarreo inicial Cin, que pone el "+ 1".

```text
           +-----+
  B0 ------| XOR |---- B0' --> entra como B en FA0   (igual con B1, B2 y B3)
  SUB -+---|     |
       |   +-----+
       +--------------------> entra como Cin en FA0
```

Ecuaciones del circuito (las XOR de B y el primer bit; el resto es la cadena del Paso 3):

```text
Bx0(t+1) = XOR(B0(t), SUB(t))
p0(t+1) = XOR(A0(t), Bx0(t))
S0(t+1) = XOR(p0(t), SUB(t))
g0(t+1) = AND(A0(t), Bx0(t))
pc0(t+1) = AND(p0(t), SUB(t))
C1(t+1) = OR(g0(t), pc0(t))
Bx1(t+1) = XOR(B1(t), SUB(t))
Bx2(t+1) = XOR(B2(t), SUB(t))
Bx3(t+1) = XOR(B3(t), SUB(t))
```

Comprobado con las 256 restas (A - B con SUB = 1): el resultado de 4 bits es siempre (A - B) módulo 16; C4 vale 1 exactamente cuando A >= B sin signo (es decir, C4 = 0 significa que "hubo que pedir prestado"); y V = C3 ⊕ C4 sigue detectando que la resta con signo no cabe.

La Tabla 5.10 (más abajo) muestra cuatro restas, entre ellas una que desborda.

### Paso 7. Cuánto tarda en asentarse el resultado

Hasta ahora el sumador era una fórmula; con retardo unitario es un circuito que **tarda**. Si cambias las entradas, las salidas pasan por valores intermedios (que no son la suma de nada) hasta que la última señal llega al final. Definición que uso: el **tiempo de asentamiento** es el paso en el que las salidas (S y el último acarreo) cambian por última vez después de que cambien las entradas en t=0.

Se mide así, sin trampas: el simulador prueba **todas** las transiciones posibles entre dos combinaciones de entradas (cada una arranca desde el estado ya asentado de las entradas antiguas) y se queda con el peor caso. Para sumadores muy anchos, donde probar todo es inviable, se calcula el camino más largo del circuito (que es una cota: nada puede tardar más) y se busca una transición concreta que la alcanza (con lo que la cota es exacta).

Resultado medido: el peor caso del sumador de 4 bits (XOR/AND/OR) tarda 9 pasos en dejar de cambiar sus salidas, cuando las entradas que cambian lo hacen todas a la vez en t=0 desde el estado ya asentado de las entradas anteriores, y con Cin como entrada normal. En la versión con restador (un XOR más en cada B) tarda 10.

Las condiciones importan: la cifra depende de **cómo está descompuesto el sumador completo en puertas** (aquí, el de las ecuaciones del Paso 2), de que Cin sea una entrada normal que también puede cambiar y de que las entradas que cambian lo hagan todas a la vez en t=0, desde el estado ya asentado de las entradas anteriores. Con otra descomposición saldrían otros números. La Tabla 5.11 (más abajo) recoge todas las variantes medidas.

- Con esta descomposición en puertas, para n = 1, 2, 3, 4, 8, 16 bits el peor caso es 3, 5, 7, 9, 17, 33 pasos, o sea 2n + 1: cada bit más añade dos pasos, uno por cada puerta del camino del acarreo.
- En la versión solo NAND, para n = 1, 2, 3, 4 el peor caso es 6, 8, 10, 12 pasos (2n + 4): cada bit sigue añadiendo dos pasos, pero la base es más alta porque las puertas XOR hechas con NAND son más profundas.

Y así se ve el peor caso por dentro (entradas que provocan que el acarreo recorra toda la cadena):

```text
 t | C4 C3 C2 C1 | S3 S2 S1 S0 | S como numero
 0 |  0  0  0  0 |  0  0  0  0 | 0b0000 = 0
 1 |  0  0  0  0 |  0  0  0  1 | 0b0001 = 1
 2 |  0  0  0  0 |  1  1  1  0 | 0b1110 = 14
 3 |  0  0  0  1 |  1  1  1  0 | 0b1110 = 14
 4 |  0  0  0  1 |  1  1  0  0 | 0b1100 = 12
 5 |  0  0  1  1 |  1  1  0  0 | 0b1100 = 12
 6 |  0  0  1  1 |  1  0  0  0 | 0b1000 = 8
 7 |  0  1  1  1 |  1  0  0  0 | 0b1000 = 8
 8 |  0  1  1  1 |  0  0  0  0 | 0b0000 = 0
 9 |  1  1  1  1 |  0  0  0  0 | 0b0000 = 0
```

Antes de t=0 todas las entradas valen 0; en t=0 pasan a A = 0b0000, B = 0b1111 y Cin = 1. Los acarreos se encienden uno cada dos pasos (C1 en t=3, C2 en t=5, C3 en t=7, C4 en t=9). Mientras tanto, la lectura de cinco bits (C4 S3 S2 S1 S0) vale 0, 1, 14, 12, 8, 0 y ninguno de esos valores es el correcto: la suma buena, 16, no aparece hasta t=9.

Conclusión práctica: **no leas el resultado de un sumador antes de que haya pasado su tiempo de asentamiento**; antes de eso puedes leer una suma que nunca existió. En un circuito con reloj, el periodo del reloj tiene que ser mayor que ese tiempo; en el modelo del mod, lo que importe lo decidirá su simulador.

## Ejemplo resuelto completo: tres sumas de 4 bits

Los tres ejemplos usan el sumador de 4 bits de puertas del Paso 3 con Cin = 0. En cada tabla, la fila del bit i muestra qué entra y qué sale de su sumador completo, empezando por el bit 0 (el acarreo "que entra" en un bit es el "que sale" del anterior). Tras la tabla, la traza muestra cómo se va asentando el resultado y las líneas siguientes lo interpretan con y sin signo.

### Ejemplo 1: 0b0110 + 0b0111

**Tabla 5.4. Suma bit a bit de 0b0110 + 0b0111**

| bit i | A_i | B_i | acarreo que entra | S_i | acarreo que sale |
|---|---|---|---|---|---|
| 0 | 0 | 1 | 0 | 1 | 0 |
| 1 | 1 | 1 | 0 | 0 | 1 |
| 2 | 1 | 1 | 1 | 1 | 1 |
| 3 | 0 | 0 | 1 | 1 | 0 |

```text
 t | C4 C3 C2 C1 | S3 S2 S1 S0 | S como numero
 0 |  0  0  0  0 |  0  0  0  0 | 0b0000 = 0
 1 |  0  0  0  0 |  0  0  0  0 | 0b0000 = 0
 2 |  0  1  1  0 |  0  0  0  1 | 0b0001 = 1
 3 |  0  1  1  0 |  1  1  0  1 | 0b1101 = 13
```

- Resultado: 0b0110 + 0b0111 = 0b1101 (0xD), con acarreo final C = 0 y V = 1 (C3 = 1, C4 = 0).
- Leído sin signo: 6 + 7 = 13, cabe en 4 bits (0 a 15), así que C = 0.
- Leído con signo: (6) + (7) = 13, no cabe en -8 a 7: V = 1 y 0b1101 se lee como -3, que es incorrecto.
- Tiempo: partiendo de todas las entradas a 0, las salidas dejan de cambiar en t=3.

Fíjate en que **el mismo patrón** 0b1101 es un resultado correcto leído sin signo y un resultado equivocado leído con signo: por eso existen C y V por separado.

### Ejemplo 2: 0x9 + 0x8 con desbordamiento de 4 bits

**Tabla 5.5. Suma bit a bit de 0x9 + 0x8**

| bit i | A_i | B_i | acarreo que entra | S_i | acarreo que sale |
|---|---|---|---|---|---|
| 0 | 1 | 0 | 0 | 1 | 0 |
| 1 | 0 | 0 | 0 | 0 | 0 |
| 2 | 0 | 0 | 0 | 0 | 0 |
| 3 | 1 | 1 | 0 | 0 | 1 |

```text
 t | C4 C3 C2 C1 | S3 S2 S1 S0 | S como numero
 0 |  0  0  0  0 |  0  0  0  0 | 0b0000 = 0
 1 |  0  0  0  0 |  0  0  0  0 | 0b0000 = 0
 2 |  1  0  0  0 |  0  0  0  1 | 0b0001 = 1
```

- Resultado: 0x9 + 0x8 = 0b0001 (0x1), con acarreo final C = 1 y V = 1 (C3 = 0, C4 = 1).
- Leído sin signo: 9 + 8 = 17 (0b10001) no cabe en 4 bits (0 a 15): C = 1 y el hardware solo guarda los 4 bits de la derecha, 0b0001 (1).
- Leído con signo: (-7) + (-8) = -15, no cabe en -8 a 7: V = 1 y 0b0001 se lee como 1, que es incorrecto.
- Tiempo: partiendo de todas las entradas a 0, las salidas dejan de cambiar en t=2.

Aquí desborda de las dos formas: C=1 y V=1. El hardware devuelve el patrón que cabe y el indicador es lo único que avisa de que el resultado no es la suma.

### Ejemplo 3: -3 + 5 en complemento a dos de 4 bits

El -3 se escribe negando el 3 con la regla del Paso 4. Para escribir -3 en 4 bits: 3 = 0b0011, NOT 0b0011 = 0b1100, y 0b1100 + 1 = 0b1101. Después se suma como si nada:

**Tabla 5.6. Suma bit a bit de -3 (0b1101) + 5 (0b0101)**

| bit i | A_i | B_i | acarreo que entra | S_i | acarreo que sale |
|---|---|---|---|---|---|
| 0 | 1 | 1 | 0 | 0 | 1 |
| 1 | 0 | 0 | 1 | 1 | 0 |
| 2 | 1 | 1 | 0 | 0 | 1 |
| 3 | 1 | 0 | 1 | 0 | 1 |

```text
 t | C4 C3 C2 C1 | S3 S2 S1 S0 | S como numero
 0 |  0  0  0  0 |  0  0  0  0 | 0b0000 = 0
 1 |  0  0  0  0 |  0  0  0  0 | 0b0000 = 0
 2 |  0  1  0  1 |  1  0  0  0 | 0b1000 = 8
 3 |  0  1  0  1 |  0  0  1  0 | 0b0010 = 2
 4 |  1  1  0  1 |  0  0  1  0 | 0b0010 = 2
```

- Resultado: -3 (0b1101) + 5 (0b0101) = 0b0010 (0x2), con acarreo final C = 1 y V = 0 (C3 = 1, C4 = 1).
- Leído sin signo: 13 + 5 = 18 (0b10010) no cabe en 4 bits (0 a 15): C = 1 y el hardware solo guarda los 4 bits de la derecha, 0b0010 (2).
- Leído con signo: (-3) + (5) = 2, cabe en -8 a 7, así que V = 0 y 0b0010 se lee como 2.
- Tiempo: partiendo de todas las entradas a 0, las salidas dejan de cambiar en t=4.

Este es el caso que más despista: **sale acarreo (C=1) y aun así la suma con signo es correcta (V=0)**. En complemento a dos, el acarreo que sale del bit de signo no es un error: se descarta. Lo que decide si hay desbordamiento es V.

Los tres casos juntos: (0, 1), (1, 1) y (1, 0) para (C, V). Con el (0, 0) de la Tabla 5.8 salen las cuatro combinaciones: C y V son preguntas distintas y pueden dar cualquier combinación.

## Tablas

Tablas de referencia a las que apuntan los Pasos anteriores (las tablas más pequeñas están en el Paso donde se usan).

**Tabla 5.7. Los dieciséis patrones de 4 bits, con y sin signo, y su negación**

| patrón | hex | sin signo | con signo | NOT patrón | NOT patrón + 1 | valor con signo de NOT patrón + 1 |
|---|---|---|---|---|---|---|
| 0b0000 | 0x0 | 0 | 0 | 0b1111 | 0b0000 | 0 |
| 0b0001 | 0x1 | 1 | 1 | 0b1110 | 0b1111 | -1 |
| 0b0010 | 0x2 | 2 | 2 | 0b1101 | 0b1110 | -2 |
| 0b0011 | 0x3 | 3 | 3 | 0b1100 | 0b1101 | -3 |
| 0b0100 | 0x4 | 4 | 4 | 0b1011 | 0b1100 | -4 |
| 0b0101 | 0x5 | 5 | 5 | 0b1010 | 0b1011 | -5 |
| 0b0110 | 0x6 | 6 | 6 | 0b1001 | 0b1010 | -6 |
| 0b0111 | 0x7 | 7 | 7 | 0b1000 | 0b1001 | -7 |
| 0b1000 | 0x8 | 8 | -8 | 0b0111 | 0b1000 | -8 |
| 0b1001 | 0x9 | 9 | -7 | 0b0110 | 0b0111 | 7 |
| 0b1010 | 0xA | 10 | -6 | 0b0101 | 0b0110 | 6 |
| 0b1011 | 0xB | 11 | -5 | 0b0100 | 0b0101 | 5 |
| 0b1100 | 0xC | 12 | -4 | 0b0011 | 0b0100 | 4 |
| 0b1101 | 0xD | 13 | -3 | 0b0010 | 0b0011 | 3 |
| 0b1110 | 0xE | 14 | -2 | 0b0001 | 0b0010 | 2 |
| 0b1111 | 0xF | 15 | -1 | 0b0000 | 0b0001 | 1 |

**Tabla 5.8. Todas las sumas de 4 bits agrupadas por (C, V)**

| C | V | pares (de 256) | un ejemplo | lectura |
|---|---|---|---|---|
| 0 | 0 | 108 | 0b0010 + 0b0011 | sin desbordamiento de ningún tipo |
| 1 | 0 | 84 | 0b1111 + 0b0001 | desborda sin signo; con signo el resultado es correcto |
| 0 | 1 | 28 | 0b0101 + 0b0011 | desborda con signo; sin signo el resultado es correcto |
| 1 | 1 | 36 | 0b1000 + 0b1000 | desborda de las dos formas |

**Tabla 5.9. Regla de los signos para el desbordamiento con signo**

| bit de signo de A | bit de signo de B | bit de signo del resultado | V |
|---|---|---|---|
| 0 | 0 | 0 | 0 |
| 0 | 0 | 1 | 1 |
| 0 | 1 | 0 | 0 |
| 0 | 1 | 1 | 0 |
| 1 | 0 | 0 | 0 |
| 1 | 0 | 1 | 0 |
| 1 | 1 | 0 | 1 |
| 1 | 1 | 1 | 0 |

**Tabla 5.10. Cuatro restas con A + NOT B + 1**

| resta | con signo | B tras el XOR (NOT B) | resultado (4 bits) | leído con signo | C | V |
|---|---|---|---|---|---|---|
| 0b0111 - 0b0011 | 7 - (3) = 4 | 0b1100 | 0b0100 | 4 | 1 | 0 |
| 0b0011 - 0b0101 | 3 - (5) = -2 | 0b1010 | 0b1110 | -2 | 0 | 0 |
| 0b0101 - 0b0101 | 5 - (5) = 0 | 0b1010 | 0b0000 | 0 | 1 | 0 |
| 0b1000 - 0b0001 | -8 - (1) = -9 | 0b1110 | 0b0111 | 7 | 1 | 1 |

**Tabla 5.11. Tiempo de asentamiento (peor caso medido) de cada variante**

| variante | bits n | puertas | profundidad (cota) | peor caso medido (pasos) | cómo se midió |
|---|---|---|---|---|---|
| XOR/AND/OR | 1 | 5 | 3 | 3 | exhaustivo: las 64 transiciones posibles |
| XOR/AND/OR | 2 | 10 | 5 | 5 | exhaustivo: las 1024 transiciones posibles |
| XOR/AND/OR | 3 | 15 | 7 | 7 | exhaustivo: las 16384 transiciones posibles |
| XOR/AND/OR | 4 | 20 | 9 | 9 | exhaustivo: las 262144 transiciones posibles |
| XOR/AND/OR | 8 | 40 | 17 | 17 | cota por profundidad + una transición que la alcanza |
| XOR/AND/OR | 16 | 80 | 33 | 33 | cota por profundidad + una transición que la alcanza |
| solo NAND | 1 | 9 | 6 | 6 | exhaustivo: las 64 transiciones posibles |
| solo NAND | 2 | 18 | 8 | 8 | exhaustivo: las 1024 transiciones posibles |
| solo NAND | 3 | 27 | 10 | 10 | exhaustivo: las 16384 transiciones posibles |
| solo NAND | 4 | 36 | 12 | 12 | exhaustivo: las 262144 transiciones posibles |
| XOR/AND/OR + XOR en B (restador) | 4 | 24 | 10 | 10 | exhaustivo: las 262144 transiciones posibles |

**Tabla 5.12. Guía rápida: qué indicador mirar**

| Quieres saber... | Cómo lees los bits | Indicador | Cómo se calcula |
|---|---|---|---|
| si la suma no cabe | sin signo | C | C = C4, el acarreo que sale del último bit |
| si la suma no cabe | con signo (complemento a dos) | V | V = C3 ⊕ C4 |
| si en la resta hubo que pedir prestado | sin signo | C | C = 0 en una resta hecha con A + NOT B + 1 |
| si la resta no cabe | con signo (complemento a dos) | V | V = C3 ⊕ C4, igual que en la suma |

## Mini-ejercicio

(a) Con el sumador de 4 bits, calcula 0b0101 + 0b0100: resultado, C y V, y cómo se lee con signo. (b) Con el restador, calcula 0b0011 - 0b0101: el patrón de NOT B, el resultado, C y V.

---

**Solución**

**(a)** Suma bit a bit (el bit 0 es el de la derecha):

**Tabla 5.13. Solución (a)**

| bit i | A_i | B_i | acarreo que entra | S_i | acarreo que sale |
|---|---|---|---|---|---|
| 0 | 1 | 0 | 0 | 1 | 0 |
| 1 | 0 | 0 | 0 | 0 | 0 |
| 2 | 1 | 1 | 0 | 0 | 1 |
| 3 | 0 | 0 | 1 | 1 | 0 |

(a) 0b0101 + 0b0100 = 0b1001, C = 0, V = 1. Sin signo: 5 + 4 = 9, cabe, y 0b1001 = 9. Con signo: 5 + 4 = 9 no cabe en -8 a 7, y 0b1001 se lee como -7.

**(b)** Resta con el restador (SUB=1): primero se invierte B, y se suma con Cin=1:

**Tabla 5.14. Solución (b)**

| bit i | A_i | B_i | NOT B_i | acarreo que entra | S_i | acarreo que sale |
|---|---|---|---|---|---|---|
| 0 | 1 | 1 | 0 | 1 | 0 | 1 |
| 1 | 1 | 0 | 1 | 1 | 1 | 1 |
| 2 | 0 | 1 | 0 | 1 | 1 | 0 |
| 3 | 0 | 0 | 1 | 0 | 1 | 0 |

(b) NOT 0b0101 = 0b1010; 0b0011 + 0b1010 + 1 = 0b1110, C = 0, V = 0. Con signo: 3 - (5) = -2 y 0b1110 se lee como -2: correcto, sin desbordamiento. C = 0 indica que, sin signo, 3 < 5 (hubo que pedir prestado).

## En Redstone / en Satisfactory

- **En Redstone**, según la Minecraft Wiki, un sumador completo "relies on two XOR gates, two AND gates, and one OR gate", los sumadores que calculan los acarreos uno a uno se llaman "Ripple Carry adders" y el problema de esos sumadores es "carry propagation delay" ([Tutorial:Arithmetic logic](https://minecraft.wiki/w/Tutorial:Arithmetic_logic)). Es la misma estructura y el mismo problema que mide este documento.
- **En Satisfactory** el mod aún no está diseñado. Lo que este documento deja como preguntas abiertas: cuántos bits tendrá una palabra, si habrá una señal de desbordamiento, si se ofrecerán sumadores ya hechos o solo puertas, y qué retardo tendrá una puerta en el simulador real (el retardo unitario de aquí es didáctico).

## Resumen

- Un sumador completo suma tres bits (A, B y el acarreo de entrada) y da dos (la suma y el acarreo de salida); con dos semisumadores y una OR se hace con puertas.
- Encadenando sumadores completos se obtiene un sumador de 4 bits ripple-carry: el acarreo viaja de un bit al siguiente.
- En complemento a dos el bit de signo pesa en negativo, negar es invertir y sumar uno, y el mismo sumador vale con y sin signo; solo cambia cómo lees el resultado.
- C avisa de que la suma sin signo no cabe y V (C3 ⊕ C4) de que la suma con signo no cabe; son indicadores independientes. Restar es sumar con B invertido y Cin=1.
- Con retardo unitario el resultado no está listo de golpe: el peor caso del sumador de 4 bits tarda lo que mide el simulador (Tabla 5.11) y crece con cada bit que añades.
