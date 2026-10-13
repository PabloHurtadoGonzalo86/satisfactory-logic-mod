# 02 · Álgebra de Boole y puertas lógicas

## Objetivo

Aprender a describir un circuito lógico como una expresión con AND, OR y NOT, y a simplificarla sin cambiar lo que hace.  
Al terminar sabrás leer una tabla de verdad, demostrar identidades por fuerza bruta y minimizar con mapas de Karnaugh.

## Idea intuitiva

Un `if ($a && $b || !$c)` ya es álgebra de Boole. Cada variable vale 0 (falso) o 1 (verdadero) y un circuito lógico es una expresión así, hecha con puertas.

Como solo hay dos valores, una función de n variables tiene 2^n combinaciones de entrada: 2 con 1 variable, 4 con 2, 8 con 3 y 16 con 4. Mientras n sea pequeño, es tan poco que se puede **probar todo**: una tabla de verdad es un test exhaustivo. (2^n crece muy deprisa: con 32 variables ya son 4294967296 filas, así que con circuitos grandes la fuerza bruta se combina con el álgebra de este documento.) Eso te da dos herramientas:

- **demostrar** que dos expresiones son equivalentes mirando todas las filas;
- **simplificar** un circuito (menos puertas) sin cambiar su comportamiento.

En este documento y en el 03 todo es combinacional: la salida depende solo de las entradas de ese momento y nada se recuerda (la memoria llega en el documento 04).

## Explicación paso a paso

### Paso 1. Variables y operadores (la notación de la serie)

Una variable booleana (A, B, C...) vale 0 o 1. Los operadores se escriben así a partir de ahora:

- **AND**: `A·B` (el punto; a veces se omite, AB).
- **OR**: `A+B`.
- **NOT**: `¬A` (en los libros se escribe con una barra encima de la letra; aquí uso ¬ para poder escribirlo en texto plano).
- **XOR**: `A⊕B`.

Avisos para no confundirte con la aritmética del documento 01:

- Este `+` es OR, no una suma: A + A = A. La multiplicación aritmética se escribió con ×; el `·` de ahora es AND.
- Precedencia: primero ¬, luego ·, luego +. Por eso `A+B·C` significa `A+(B·C)`. Con ⊕ pongo siempre paréntesis.
- Las constantes son 0 y 1: `A·1` es A y `A+1` es 1 (Tabla 2.16).

### Paso 2. Las puertas

Una **puerta** es la versión en circuito de un operador. Las 7 que usaremos:

- **NOT** (1 entrada): invierte (Tabla 2.1).
- **AND**: 1 solo si todas las entradas valen 1.
- **OR**: 1 si al menos una entrada vale 1.
- **XOR**: 1 si las dos entradas son distintas.
- **NAND**: NOT de AND, es decir, 0 solo si las dos entradas valen 1.
- **NOR**: NOT de OR, es decir, 1 solo si las dos entradas valen 0.
- **XNOR**: NOT de XOR, es decir, 1 si las dos entradas son iguales.

**Tabla 2.1. Tabla de verdad de NOT**

| A | ¬A |
| --- | --- |
| 0 | 1 |
| 1 | 0 |

**Tabla 2.2. Las 6 puertas de dos entradas**

| A | B | AND = A·B | OR = A+B | XOR = A⊕B | NAND = ¬(A·B) | NOR = ¬(A+B) | XNOR = ¬(A⊕B) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 | 1 | 1 | 1 |
| 0 | 1 | 0 | 1 | 1 | 1 | 0 | 0 |
| 1 | 0 | 0 | 1 | 1 | 1 | 0 | 0 |
| 1 | 1 | 1 | 1 | 0 | 0 | 0 | 1 |

Para escribir circuitos en texto uso una **netlist**: una puerta por línea, `señal = PUERTA(entradas)`, y la última línea es la salida. Ejemplo: `p = AND(A, B)` y después `Y = NOT(p)` es lo mismo que NAND. En este documento y en el 03 una netlist no tiene tiempo: cada señal vale lo que dice su puerta en el mismo instante en que valen las entradas. Más adelante, en los documentos con memoria y relojes, la misma notación se usará con tiempo: cada puerta tardará un paso.

Cada puerta de dos entradas es una tabla de 4 filas, es decir, 4 bits de salida: un nibble (documento 01). La Tabla 2.17 los muestra. Una función de 3 entradas tiene 8 filas y su salida es un byte.

### Paso 3. Identidades

Una **identidad** es una igualdad que se cumple para todos los valores de las variables. Con 2 o 3 variables se demuestra por fuerza bruta: se calcula la tabla de verdad de cada lado y se comprueba que coinciden en todas las filas. Las que más vas a usar:

- **Idempotencia**: repetir una condición no cambia nada. A·A = A y A+A = A (Tabla 2.3).
- **Absorción**: si A ya basta, la rama «A y B» sobra. A + A·B = A y A·(A+B) = A (Tabla 2.4).
- **Distributiva**: A·(B+C) = A·B + A·C, como en el álgebra de siempre (Tabla 2.5). Y la segunda, que **no** existe con números: A + B·C = (A+B)·(A+C) (Tabla 2.6).

**Tabla 2.3. Idempotencia: las tres columnas coinciden en todas las filas**

| A | A·A | A+A |
| --- | --- | --- |
| 0 | 0 | 0 |
| 1 | 1 | 1 |

**Tabla 2.4. Absorción: A, A+A·B y A·(A+B) coinciden**

| A | B | A·B | A+A·B | A·(A+B) |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 |
| 0 | 1 | 0 | 0 | 0 |
| 1 | 0 | 0 | 1 | 1 |
| 1 | 1 | 1 | 1 | 1 |

**Tabla 2.5. Distributiva (1): A·(B+C) = A·B + A·C**

| A | B | C | B+C | A·(B+C) | A·B | A·C | A·B+A·C |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 |
| 0 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| 0 | 1 | 1 | 1 | 0 | 0 | 0 | 0 |
| 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 1 | 0 | 1 | 1 | 1 | 0 | 1 | 1 |
| 1 | 1 | 0 | 1 | 1 | 1 | 0 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

**Tabla 2.6. Distributiva (2): A + B·C = (A+B)·(A+C)**

| A | B | C | B·C | A+B·C | A+B | A+C | (A+B)·(A+C) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 0 | 0 | 1 | 0 | 0 | 0 | 1 | 0 |
| 0 | 1 | 0 | 0 | 0 | 1 | 0 | 0 |
| 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| 1 | 0 | 0 | 0 | 1 | 1 | 1 | 1 |
| 1 | 0 | 1 | 0 | 1 | 1 | 1 | 1 |
| 1 | 1 | 0 | 0 | 1 | 1 | 1 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

La Tabla 2.16 recoge estas y otras identidades (identidad, anulación, complemento, doble negación, conmutativa, asociativa, De Morgan) con el número de combinaciones comprobadas en cada una.

### Paso 4. Leyes de De Morgan

Negar una condición intercambia AND por OR y niega cada parte:

- ¬(A·B) = ¬A + ¬B
- ¬(A+B) = ¬A·¬B

En código: `!($a && $b)` equivale a `!$a || !$b`. Las dos leyes se demuestran por tabla exhaustiva (Tablas 2.7 y 2.8: la columna de la izquierda y la de la derecha de cada ley coinciden en las 4 filas).

**Tabla 2.7. De Morgan (1): ¬(A·B) = ¬A + ¬B**

| A | B | A·B | ¬(A·B) | ¬A | ¬B | ¬A+¬B |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 1 | 1 | 1 | 1 |
| 0 | 1 | 0 | 1 | 1 | 0 | 1 |
| 1 | 0 | 0 | 1 | 0 | 1 | 1 |
| 1 | 1 | 1 | 0 | 0 | 0 | 0 |

**Tabla 2.8. De Morgan (2): ¬(A+B) = ¬A · ¬B**

| A | B | A+B | ¬(A+B) | ¬A | ¬B | ¬A·¬B |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 1 | 1 | 1 | 1 |
| 0 | 1 | 1 | 0 | 1 | 0 | 0 |
| 1 | 0 | 1 | 0 | 0 | 1 | 0 |
| 1 | 1 | 1 | 0 | 0 | 0 | 0 |

Consecuencia que usaremos en el documento 03: un NAND es un OR de entradas negadas, y un NOR es un AND de entradas negadas.

### Paso 5. Formas canónicas (suma de productos y producto de sumas)

Toda función se puede escribir siempre de forma «mecánica», como suma de productos y como producto de sumas. Cada fila de la tabla de verdad es un número: la fila 5 de una función de 3 variables es A=1, B=0, C=1 (5 = 4 + 0 + 1: esta es una suma aritmética de números, no un OR), con la primera variable como bit más alto, igual que en el documento 01.

- **Minterm** de una fila: un AND de todas las variables, cada una directa si vale 1 en esa fila y negada si vale 0. Vale 1 solo en esa fila. Se escribe mₖ, con k el número de fila.
- **Suma de productos (SOP)**: el OR de los minterms de las filas donde la función vale 1. Se abrevia Σm(...).
- **Maxterm** de una fila: un OR de todas las variables, cada una directa si vale 0 en esa fila y negada si vale 1. Vale 0 solo en esa fila. Se escribe Mₖ.
- **Producto de sumas (POS)**: el AND de los maxterms de las filas donde la función vale 0. Se abrevia ΠM(...).

Siempre existen y siempre son correctas, pero suelen ser grandes: por eso se simplifican. El ejemplo resuelto las calcula para la mayoría de 3.

### Paso 6. Mapas de Karnaugh

La simplificación se apoya en una identidad: dos productos que difieren en una sola variable se funden y esa variable desaparece. A·B + A·¬B = A·(B+¬B) = A·1 = A (distributiva, complemento e identidad).

Un **mapa de Karnaugh** coloca las filas de la tabla de modo que **casillas vecinas difieran exactamente en una variable**. Para eso los ejes no van en orden binario (00, 01, 10, 11) sino en **orden Gray** (00, 01, 11, 10): entre una etiqueta y la siguiente cambia un solo bit. Estos son los números de minterm en cada casilla para 2, 3 y 4 variables:

```text
# Numero de minterm de cada casilla (2 variables)
      B
      0 1
A=0 | 0 1
A=1 | 2 3
```

```text
# Numero de minterm de cada casilla (3 variables)
      BC
      00 01 11 10
A=0 |  0  1  3  2
A=1 |  4  5  7  6
```

```text
# Numero de minterm de cada casilla (4 variables)
        CD
        00 01 11 10
AB=00 |  0  1  3  2
AB=01 |  4  5  7  6
AB=11 | 12 13 15 14
AB=10 |  8  9 11 10
```

Reglas para agrupar unos:

1. Los grupos son rectángulos de 1, 2, 4, 8 o 16 casillas (potencias de 2) y todas sus casillas valen 1 (o X, ver más abajo las casillas «no importa»).
2. El mapa es circular: el borde izquierdo es vecino del derecho, el superior del inferior, y las cuatro esquinas son vecinas entre sí.
3. Los grupos pueden solaparse y conviene hacerlos tan grandes como se pueda.
4. Hay que cubrir todos los unos con el menor número de grupos (las casillas X no hace falta cubrirlas).
5. Cada grupo da un producto con las variables que **no cambian** dentro del grupo: directa si valen 1, negada si valen 0. La función es la suma de esos productos.

Con 2 variables, un caso en que se agrupa y otro en que no. OR: sus 3 unos se cubren con 2 grupos de 2 casillas.

```text
# K-mapa de OR(A,B), unos en los minterms 1,2,3
      B
      0 1
A=0 | 0 1
A=1 | 1 1
```

Aquí el grupo {2, 3} (A=1) da A y el grupo {1, 3} (B=1) da B, así que OR(A,B) = A + B. XOR, en cambio, tiene los unos en diagonal, sin vecinos: no se agrupa nada y la forma mínima sigue siendo A·¬B + ¬A·B.

```text
# K-mapa de XOR(A,B), unos en los minterms 1,2
      B
      0 1
A=0 | 0 1
A=1 | 1 0
```

**Casillas «no importa» (X).** A veces hay combinaciones de entradas que no pueden ocurrir (un código que ningún estado usa) o en las que da igual qué salida se produzca. En el mapa se marcan con una **X** (en otras tablas se escriben con un guion, «-»). Una X es una libertad que te das al diseñar, parecida a la rama `default:` de un `switch` que sabes que nunca se ejecuta (ahí escribes lo que más te convenga): cada X se puede contar como 1, si así un grupo sale más grande, o como 0, si no hace falta, y nunca hay que cubrirla. Un grupo puede mezclar unos y X, pero nunca contener un 0.

Ejemplo con 3 variables. Un contador de 3 bits (A es el bit más alto) cuenta de 0 a 5 y vuelve a 0, así que sus valores 6 y 7 no aparecen nunca. FIN(A,B,C) vale 1 cuando el contador está en su último valor, 5; en 6 y 7 da igual:

```text
# K-mapa de FIN(A,B,C), unos en los minterms 5 y X en los minterms 6,7
      BC
      00 01 11 10
A=0 |  0  0  0  0
A=1 |  0  1  X  X
```

Con los números de casilla de los mapas de arriba: sin usar las X, el único 1 está solo y la forma mínima es A·¬B·C (3 puertas: 1 NOT y 2 AND). Contando la casilla 7 como 1 se forma el grupo {5, 7} (A=1, C=1): dentro de él cambia B, que desaparece, y queda A·C (una sola puerta, un AND). La casilla 6 se cuenta como 0: para unirla al 5 habría que tomar toda la fila A=1, que incluye la casilla 4, un 0.

```text
FIN(A,B,C) = A·¬B·C    # SOP minima sin usar las X
FIN(A,B,C) = A·C    # SOP minima usando las X
```

Ojo con la decisión que has tomado: el circuito A·C da 1 en la casilla 7 y 0 en la 6. Mientras el contador no llegue a esos valores, nadie lo nota; si llegara, el circuito daría eso. Por eso solo se marca X lo que de verdad no importa. Volverás a usar estas casillas en el documento 07.

### Paso 7. Contar puertas

Para comparar circuitos hace falta una unidad. En estos documentos: una puerta es un AND, OR o XOR de 2 entradas, o un NOT; un AND de 3 entradas cuenta como 2 puertas de 2 entradas; los NOT de una misma variable se comparten; las entradas no cuestan. Es una convención de conteo, no una ley.

## Ejemplo resuelto completo: la mayoría de 3 y una función de 4 variables

### Parte A. Mayoría de 3

La función MAJ(A,B,C) vale 1 cuando al menos 2 de las 3 entradas valen 1 (es una votación). Su tabla de verdad:

**Tabla 2.9. Mayoría de 3 (1 si al menos 2 entradas valen 1)**

| m | A | B | C | f = MAJ(A,B,C) |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 |
| 1 | 0 | 0 | 1 | 0 |
| 2 | 0 | 1 | 0 | 0 |
| 3 | 0 | 1 | 1 | 1 |
| 4 | 1 | 0 | 0 | 0 |
| 5 | 1 | 0 | 1 | 1 |
| 6 | 1 | 1 | 0 | 1 |
| 7 | 1 | 1 | 1 | 1 |

Si lees la columna de salida de abajo arriba (la fila 0 es el bit más bajo) sale un byte: 0b11101000 = 232 = 0xE8.

**1. Formas canónicas.** Vale 1 en las filas 3,5,6,7 y 0 en las filas 0,1,2,4:

```text
MAJ(A,B,C) = ¬A·B·C + A·¬B·C + A·B·¬C + A·B·C    # SOP canonica = suma de los minterms 3,5,6,7
```

```text
MAJ(A,B,C) = (A+B+C)·(A+B+¬C)·(A+¬B+C)·(¬A+B+C)    # POS canonica = producto de los maxterms 0,1,2,4
```

**2. Karnaugh.** Mapa de 3 variables y sus grupos:

```text
# K-mapa de MAJ(A,B,C), unos en los minterms 3,5,6,7
      BC
      00 01 11 10
A=0 |  0  0  1  0
A=1 |  0  1  1  1
```

**Tabla 2.10. Grupos de unos del mapa de la mayoría**

| Grupo | Casillas | Minterms | Variables que no cambian | Término |
| --- | --- | --- | --- | --- |
| 1 | 2 | 6, 7 | A=1, B=1 | A·B |
| 2 | 2 | 5, 7 | A=1, C=1 | A·C |
| 3 | 2 | 3, 7 | B=1, C=1 | B·C |

3 grupos de 2 casillas que comparten la casilla 7 (se solapan, y está permitido). La función mínima en suma de productos:

```text
MAJ(A,B,C) = A·B + A·C + B·C    # SOP minima por Karnaugh
```

**3. Circuitos.** La SOP mínima necesita 5 puertas; si además sacas factor común (A·B + C·(A+B)), bastan 4:

```text
# objetivo: MAJ | base: AND/OR | puertas: 5 | entradas: A, B, C
p = AND(A, B)
q = AND(A, C)
r = AND(B, C)
s = OR(p, q)
Y = OR(s, r)
```

```text
# objetivo: MAJ | base: AND/OR | puertas: 4 | entradas: A, B, C
p = AND(A, B)
q = OR(A, B)
r = AND(C, q)
Y = OR(p, r)
```

```text
MAJ(A,B,C) = A·B + C·(A+B)    # factorizada
```

**4. Contar puertas.**

**Tabla 2.11. Coste en puertas de dos entradas (los NOT de una misma variable se comparten)**

| Forma | Fórmula | NOT | AND | OR | Total |
| --- | --- | --- | --- | --- | --- |
| SOP canónica | ¬A·B·C + A·¬B·C + A·B·¬C + A·B·C | 3 | 8 | 3 | 14 |
| SOP mínima (Karnaugh) | A·B + A·C + B·C | 0 | 3 | 2 | 5 |
| Factorizada | A·B + C·(A+B) | 0 | 2 | 2 | 4 |

La forma canónica cuesta 14 puertas y la mínima de Karnaugh 5. La factorizada baja a 4: Karnaugh minimiza la suma de productos, que no siempre es el circuito más pequeño. ¿Se puede bajar de 4? El script de la serie (documento 01) probó **todos** los circuitos de AND, OR y NOT con 3 puertas o menos: ninguno calcula MAJ. Con 4 sí (los de arriba), así que 4 es el mínimo con esas puertas. Añadiendo XOR a las puertas permitidas el mínimo sigue siendo 4.

**5. Verificación.** Las 4 formas dan la misma salida que la definición en todas las filas:

**Tabla 2.12. Las 4 formas (más la definición) dan la misma salida en las 8 filas**

| A | B | C | MAJ(A,B,C) | SOP = ¬A·B·C + A·¬B·C + A·B·¬C + A·B·C | POS = (A+B+C)·(A+B+¬C)·(A+¬B+C)·(¬A+B+C) | mín = A·B + A·C + B·C | fact = A·B + C·(A+B) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 0 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| 0 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 1 | 0 | 1 | 1 | 1 | 1 | 1 | 1 |
| 1 | 1 | 0 | 1 | 1 | 1 | 1 | 1 |
| 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

### Parte B. Una función de 4 variables

Sea F4(A,B,C,D) con unos en los minterms 0,1,2,5,8,9,10. Mapa de 4 variables:

```text
# K-mapa de F4(A,B,C,D), unos en los minterms 0,1,2,5,8,9,10
        CD
        00 01 11 10
AB=00 |  1  1  0  1
AB=01 |  0  1  0  0
AB=11 |  0  0  0  0
AB=10 |  1  1  0  1
```

Los grupos muestran las reglas «raras» del mapa:

**Tabla 2.13. Grupos de unos del mapa de F4**

| Grupo | Casillas | Minterms | Variables que no cambian | Término |
| --- | --- | --- | --- | --- |
| 1 | 2 | 1, 5 | A=0, C=0, D=1 | ¬A·¬C·D |
| 2 | 4 | 0, 1, 8, 9 | B=0, C=0 | ¬B·¬C |
| 3 | 4 | 0, 2, 8, 10 | B=0, D=0 | ¬B·¬D |

- Grupo 1 (¬A·¬C·D): 2 casillas, filas AB = 00, 01 y columnas CD = 01.
- Grupo 2 (¬B·¬C): 4 casillas, filas AB = 00, 10 y columnas CD = 00, 01: junta la fila superior con la inferior (vecinas porque el mapa es circular).
- Grupo 3 (¬B·¬D): 4 casillas, filas AB = 00, 10 y columnas CD = 00, 10: las cuatro esquinas, que son vecinas porque el mapa es circular en vertical y en horizontal.

```text
F4(A,B,C,D) = ¬A·¬C·D + ¬B·¬C + ¬B·¬D    # SOP minima por Karnaugh
```

**Tabla 2.14. Coste en puertas de dos entradas de F4**

| Forma | Fórmula | NOT | AND | OR | Total |
| --- | --- | --- | --- | --- | --- |
| SOP canónica | ¬A·¬B·¬C·¬D + ¬A·¬B·¬C·D + ¬A·¬B·C·¬D + ¬A·B·¬C·D + A·¬B·¬C·¬D + A·¬B·¬C·D + A·¬B·C·¬D | 4 | 21 | 6 | 31 |
| SOP mínima (Karnaugh) | ¬A·¬C·D + ¬B·¬C + ¬B·¬D | 4 | 4 | 2 | 10 |

De 31 puertas de la forma canónica a 10. Y la verificación en las 16 filas:

**Tabla 2.15. F4 y su forma mínima coinciden en las 16 filas**

| A | B | C | D | F4(A,B,C,D) | mín = ¬A·¬C·D + ¬B·¬C + ¬B·¬D |
| --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 1 | 1 |
| 0 | 0 | 0 | 1 | 1 | 1 |
| 0 | 0 | 1 | 0 | 1 | 1 |
| 0 | 0 | 1 | 1 | 0 | 0 |
| 0 | 1 | 0 | 0 | 0 | 0 |
| 0 | 1 | 0 | 1 | 1 | 1 |
| 0 | 1 | 1 | 0 | 0 | 0 |
| 0 | 1 | 1 | 1 | 0 | 0 |
| 1 | 0 | 0 | 0 | 1 | 1 |
| 1 | 0 | 0 | 1 | 1 | 1 |
| 1 | 0 | 1 | 0 | 1 | 1 |
| 1 | 0 | 1 | 1 | 0 | 0 |
| 1 | 1 | 0 | 0 | 0 | 0 |
| 1 | 1 | 0 | 1 | 0 | 0 |
| 1 | 1 | 1 | 0 | 0 | 0 |
| 1 | 1 | 1 | 1 | 0 | 0 |

## Tablas

Las demás tablas están en el desarrollo. Aquí quedan las de consulta rápida.

**Tabla 2.16. Identidades del álgebra de Boole, comprobadas fila a fila**

| Identidad | Lado izquierdo | Lado derecho | Variables | Combinaciones probadas | ¿Se cumple en todas? |
| --- | --- | --- | --- | --- | --- |
| Identidad | A·1 | A | 1 | 2 | sí |
| Identidad | A+0 | A | 1 | 2 | sí |
| Anulación | A·0 | 0 | 1 | 2 | sí |
| Anulación | A+1 | 1 | 1 | 2 | sí |
| Idempotencia | A·A | A | 1 | 2 | sí |
| Idempotencia | A+A | A | 1 | 2 | sí |
| Complemento | A·¬A | 0 | 1 | 2 | sí |
| Complemento | A+¬A | 1 | 1 | 2 | sí |
| Doble negación | ¬¬A | A | 1 | 2 | sí |
| Conmutativa | A·B | B·A | 2 | 4 | sí |
| Conmutativa | A+B | B+A | 2 | 4 | sí |
| Asociativa | (A·B)·C | A·(B·C) | 3 | 8 | sí |
| Asociativa | (A+B)+C | A+(B+C) | 3 | 8 | sí |
| Absorción | A+A·B | A | 2 | 4 | sí |
| Absorción | A·(A+B) | A | 2 | 4 | sí |
| Distributiva | A·(B+C) | A·B+A·C | 3 | 8 | sí |
| Distributiva | A+B·C | (A+B)·(A+C) | 3 | 8 | sí |
| De Morgan | ¬(A·B) | ¬A+¬B | 2 | 4 | sí |
| De Morgan | ¬(A+B) | ¬A·¬B | 2 | 4 | sí |

**Tabla 2.17. Cada puerta de dos entradas como un nibble (la salida de la fila 3 es el bit más alto)**

| Puerta | Fórmula | Fila 3 (A=1, B=1) | Fila 2 (A=1, B=0) | Fila 1 (A=0, B=1) | Fila 0 (A=0, B=0) | Código |
| --- | --- | --- | --- | --- | --- | --- |
| AND | A·B | 1 | 0 | 0 | 0 | 0b1000 = 0x8 |
| OR | A+B | 1 | 1 | 1 | 0 | 0b1110 = 0xE |
| XOR | A⊕B | 0 | 1 | 1 | 0 | 0b0110 = 0x6 |
| NAND | ¬(A·B) | 0 | 1 | 1 | 1 | 0b0111 = 0x7 |
| NOR | ¬(A+B) | 0 | 0 | 0 | 1 | 0b0001 = 0x1 |
| XNOR | ¬(A⊕B) | 1 | 0 | 0 | 1 | 0b1001 = 0x9 |

## Mini-ejercicio

Un nibble ABCD (A es el bit más alto, como en el documento 01) es un dígito hexadecimal «letra» cuando vale de 0xA a 0xF. Define LETRA(A,B,C,D) = 1 solo en esos casos.

1. ¿En qué minterms vale 1?
2. Simplifícala con un mapa de Karnaugh de 4 variables.
3. ¿Se puede factorizar para usar menos puertas? ¿Cuántas?

---

**Solución**

1. En los minterms 10,11,12,13,14,15 (los números 0xA a 0xF, ver Tabla 1.5 del documento 01).
2. Mapa y grupos:

```text
# K-mapa de LETRA(A,B,C,D), unos en los minterms 10,11,12,13,14,15
        CD
        00 01 11 10
AB=00 |  0  0  0  0
AB=01 |  0  0  0  0
AB=11 |  1  1  1  1
AB=10 |  0  0  1  1
```

2 grupos de 4 casillas ({12, 13, 14, 15} con A=1, B=1 y {10, 11, 14, 15} con A=1, C=1) y la forma mínima en suma de productos es:

```text
LETRA(A,B,C,D) = A·B + A·C    # SOP minima por Karnaugh
LETRA(A,B,C,D) = A·(B+C)    # factorizada
```

3. Sí: sacando A factor común queda A·(B+C), con 2 puertas en vez de 3.

**Tabla 2.18. Coste de LETRA en puertas de dos entradas**

| Forma | Fórmula | NOT | AND | OR | Total |
| --- | --- | --- | --- | --- | --- |
| SOP mínima (Karnaugh) | A·B + A·C | 0 | 2 | 1 | 3 |
| Factorizada | A·(B+C) | 0 | 1 | 1 | 2 |

## En Redstone / en Satisfactory

- **En Redstone**, según la Minecraft Wiki, una antorcha invierte la señal que recibe en el bloque donde está colocada ([Redstone Torch](https://minecraft.wiki/w/Redstone_Torch)): hace de NOT.
- La misma wiki trata la señal como encendida cuando es mayor que 0 y describe una puerta AND como la que solo da señal si las dos entradas son mayores que 0 ([Redstone circuit](https://minecraft.wiki/w/Redstone_circuit)): lógica booleana de 1 bit.
- **En Satisfactory** qué puertas se ofrecerán y cuántas entradas tendrán es una decisión abierta: el mod aún no está diseñado. Las identidades y la minimización de este documento valen para cualquier elección.

## Resumen

- Una función de n variables tiene 2^n filas, así que, mientras n sea pequeño, cualquier igualdad entre circuitos se demuestra mirando todas las filas.
- Las 7 puertas (NOT, AND, OR, XOR, NAND, NOR, XNOR) se escriben con ¬, ·, +, ⊕ y se describen en texto con netlists.
- Idempotencia, absorción, distributiva y De Morgan permiten reescribir un circuito sin cambiarlo; NAND es un OR de entradas negadas y NOR un AND de entradas negadas.
- Toda función tiene una suma de productos y un producto de sumas canónicos, y el mapa de Karnaugh (ejes en orden Gray, grupos de potencias de 2, borde circular, casillas X que valen 0 o 1 según convenga) los simplifica.
- Minimizar la SOP no siempre da el circuito más pequeño: la mayoría de 3 pasa de 14 a 5 puertas con Karnaugh y a 4 al factorizar.
