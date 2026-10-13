# 03 · Universalidad de NAND (y NOR)

## Objetivo

Construir NOT, AND, OR, NOR y XOR usando solo NAND (y después lo mismo con NOR), y comprobar cada construcción mirando todas las filas.  
Entender qué significa que una puerta sea «universal» y por qué, aun así, conviene tener varias puertas disponibles.

## Idea intuitiva

Imagina que solo tienes un tipo de pieza de Lego y aun así puedes montar cualquier figura. Esa pieza, en lógica, es la puerta NAND («NOT AND»). Con NAND se hacen NOT, AND, OR y todas las demás; con NOR también. Se las llama puertas **universales**.

Pero universal no significa barato: lo que con una puerta XOR es una sola pieza, con NAND son 4. La pregunta de diseño no es «¿se puede?» (sí), sino «¿cuánto cuesta y qué conviene ofrecer?».

## Explicación paso a paso

### Paso 1. Las reglas del juego y NAND como NOT

NAND(A,B) = ¬(A·B): da 0 solo cuando las dos entradas valen 1 (Tablas 2.2 y 2.17 del documento 02; el código de la 2.17 es 0b0111 = 0x7). Las reglas de construcción son las de la netlist del documento 02 (que no tiene tiempo: cada señal vale lo que dice su puerta en cuanto valen las entradas):

- Cada puerta toma sus entradas de las entradas del circuito o de la salida de una puerta anterior.
- Una señal puede alimentar a varias puertas, y las dos entradas de una puerta pueden ser la misma señal.
- No hay constantes 0 ni 1 disponibles: solo las entradas.
- El coste es el número de puertas (líneas de la netlist), como en el documento 02.

Primer truco: si unes las dos entradas de un NAND, NAND(A,A) = ¬(A·A) = ¬A, por idempotencia (documento 02). Es un NOT con 1 puerta:

**Tabla 3.1. NAND con las dos entradas unidas es un NOT**

| A | NAND(A,A) | ¬A |
| --- | --- | --- |
| 0 | 1 | 1 |
| 1 | 0 | 0 |

```text
# objetivo: NOT | base: NAND | puertas: 1 | entradas: A
Y = NAND(A, A)
```

### Paso 2. AND con NAND

AND es el NOT de un NAND: ¬¬(A·B) = A·B. Primero NAND(A,B) y después invertir ese resultado con el truco anterior: 2 puertas.

```text
# objetivo: AND | base: NAND | puertas: 2 | entradas: A, B
t = NAND(A, B)
Y = NAND(t, t)
```

### Paso 3. OR con NAND (De Morgan)

Por De Morgan (documento 02), A+B = ¬(¬A·¬B), que es exactamente un NAND de las entradas negadas. Cada entrada negada cuesta una puerta (el NOT del Paso 1): 3 puertas.

```text
# objetivo: OR | base: NAND | puertas: 3 | entradas: A, B
na = NAND(A, A)
nb = NAND(B, B)
Y = NAND(na, nb)
```

**Tabla 3.2. OR con 3 NAND (De Morgan: A+B = ¬(¬A·¬B))**

| A | B | na = NAND(A,A) | nb = NAND(B,B) | Y = NAND(na,nb) | A+B |
| --- | --- | --- | --- | --- | --- |
| 0 | 0 | 1 | 1 | 0 | 0 |
| 0 | 1 | 1 | 0 | 1 | 1 |
| 1 | 0 | 0 | 1 | 1 | 1 |
| 1 | 1 | 0 | 0 | 1 | 1 |

### Paso 4. NOR con NAND

NOR es el NOT de OR: se añade un NOT al circuito anterior, 4 puertas.

```text
# objetivo: NOR | base: NAND | puertas: 4 | entradas: A, B
na = NAND(A, A)
nb = NAND(B, B)
o = NAND(na, nb)
Y = NAND(o, o)
```

### Paso 5. XOR con NAND

XOR cuesta 4 NAND, tanto como el NOR construido con NAND en el Paso 4, y está desarrollado entero en el ejemplo resuelto de más abajo. La más cara con NAND es XNOR: con un NOT más sobre el XOR salen 5 puertas, y es el mini-ejercicio. Con NOR la situación se invierte (Paso 6).

### Paso 6. Lo mismo con NOR (la versión «espejo»)

Con NOR todo es simétrico, porque De Morgan intercambia AND y OR: NOR(A,A) = ¬A; OR es un NOT de NOR; y AND se hace con las entradas negadas, A·B = ¬(¬A+¬B). Las construcciones:

```text
# objetivo: NOT | base: NOR | puertas: 1 | entradas: A
Y = NOR(A, A)
```

```text
# objetivo: OR | base: NOR | puertas: 2 | entradas: A, B
t = NOR(A, B)
Y = NOR(t, t)
```

```text
# objetivo: AND | base: NOR | puertas: 3 | entradas: A, B
na = NOR(A, A)
nb = NOR(B, B)
Y = NOR(na, nb)
```

```text
# objetivo: NAND | base: NOR | puertas: 4 | entradas: A, B
na = NOR(A, A)
nb = NOR(B, B)
a = NOR(na, nb)
Y = NOR(a, a)
```

La estructura de XOR con NAND de 4 puertas, escrita con NOR, calcula **XNOR** (no XOR): así que aquí XNOR cuesta 4 puertas y XOR 5, al revés que con NAND.

```text
# objetivo: XNOR | base: NOR | puertas: 4 | entradas: A, B
t = NOR(A, B)
u = NOR(A, t)
v = NOR(B, t)
Y = NOR(u, v)
```

```text
# objetivo: XOR | base: NOR | puertas: 5 | entradas: A, B
t = NOR(A, B)
u = NOR(A, t)
v = NOR(B, t)
x = NOR(u, v)
Y = NOR(x, x)
```

### Paso 7. Contar puertas y capas

La Tabla 3.6 resume todo. **Capas** es la profundidad combinacional: el mayor número de puertas encadenadas entre una entrada y la salida (por ejemplo, en XOR con NAND la ruta A, t, u, Y pasa por 3 puertas). Es una medida de forma del circuito; aún no hablamos de tiempo.

La columna «Mínimo» no es una opinión: el script de la serie (documento 01) probó **todos** los circuitos posibles de cada base con hasta 5 puertas, reutilizando señales y repitiendo entradas, y anotó la primera cantidad de puertas con la que aparece cada función. Las construcciones de arriba coinciden con ese mínimo en los 12 casos. La Tabla 3.8 da el mismo dato para las 16 funciones de dos variables.

### Paso 8. Completitud funcional (enunciado informal)

Un conjunto de puertas es **funcionalmente completo** si, conectándolas como quieras (reutilizando señales, duplicando entradas), puedes construir **cualquier** función booleana, de cualquier número de variables. Cuántas funciones hay lo da la Tabla 3.7: 16 de 2 variables, 256 de 3, 65536 de 4.

- {AND, OR, NOT} es completo: en el documento 02 vimos que toda función tiene una suma de productos canónica, y esa forma usa solo AND, OR y NOT. (La constante 0 no tiene ningún minterm, así que su suma de productos queda vacía y se escribe aparte como A·¬A; la constante 1 sí es la suma de todos los minterms, pero A+¬A es mucho más corta. Con NAND cuestan 3 y 2 puertas, Tabla 3.8.)
- NAND construye NOT (Paso 1), AND (Paso 2) y OR (Paso 3), así que construye todo lo que se hace con {AND, OR, NOT}: **NAND es completo**. Con NOR, igual (Paso 6).
- No todo conjunto lo es. Con solo AND y OR (sin NOT) se alcanzan 4 de las 16 funciones de 2 variables y 18 de las 256 de 3 (Tabla 3.9): son *monótonas*, es decir, si subes una entrada de 0 a 1 la salida nunca baja, y un NOT hace justo lo contrario. Con solo XOR, 4 de las 16 y 8 de las 256. AND y XOR juntos, 8 y 128: les falta una constante 1 (con ella, A⊕1 = ¬A, y ya hay NOT; la Tabla 3.9 lo confirma).

Qué ha comprobado el script y qué no: para NAND y NOR ha construido por cierre todas las funciones de 2 y 3 variables (Tabla 3.9). Para un número cualquiera de variables, lo que lo garantiza es el argumento de la suma de productos de arriba, no una prueba por fuerza bruta.

### Paso 9. Por qué importa para un mod (o cualquier sistema de puertas)

- **Un solo primitivo basta.** Si un sistema ofrece NAND (o NOR), ya es completo: cualquier circuito lógico se puede construir, y la definición mínima del sistema es pequeña.
- **Pero tiene un coste.** Con solo NAND, OR cuesta 3 puertas en vez de 1, NOR 4, XOR 4 y XNOR 5, y esta última llega a 4 capas (Tabla 3.6). Más piezas, más cableado, circuitos más difíciles de leer.
- **Por eso conviene ofrecer varios.** Con NOT, AND, OR, XOR (y NAND o NOR) los circuitos salen más pequeños y legibles, y la universalidad de NAND/NOR sigue garantizando que no hay nada que no se pueda construir. Cuál sea el conjunto exacto es una decisión de diseño que aún no está tomada.

## Ejemplo resuelto completo: XOR con 4 NAND, fila a fila

**1. Objetivo.** A⊕B vale 1 si las entradas son distintas. Solo tenemos NAND.

**2. Idea.** Sea t = NAND(A,B) = ¬(A·B) = ¬A + ¬B. Usamos t para comparar cada entrada con la otra:

- u = NAND(A,t) = ¬(A·t) = ¬A + ¬t = ¬A + A·B = ¬A + B (la última igualdad es la distributiva (2) del documento 02: ¬A + A·B = (¬A + A)·(¬A + B) = ¬A + B).
- v = NAND(B,t) = ¬B + A, por simetría.
- Y = NAND(u,v) = ¬(u·v) = ¬u + ¬v (De Morgan) = A·¬B + ¬A·B = A⊕B.

**3. Netlist.**

```text
# objetivo: XOR | base: NAND | puertas: 4 | entradas: A, B
t = NAND(A, B)
u = NAND(A, t)
v = NAND(B, t)
Y = NAND(u, v)
```

**4. Fila a fila.** Calculado con la netlist:

- A=0, B=0: t=1, u=1, v=1, Y=0
- A=0, B=1: t=1, u=1, v=0, Y=1
- A=1, B=0: t=1, u=0, v=1, Y=1
- A=1, B=1: t=0, u=1, v=1, Y=0

**5. Todas las señales.** Las columnas u y ¬A+B coinciden, y v y A+¬B también, como dice el punto 2 de este ejemplo:

**Tabla 3.3. Traza de XOR con 4 NAND (todas las señales internas)**

| A | B | t = NAND(A,B) | u = NAND(A,t) | ¬A+B | v = NAND(B,t) | A+¬B | Y = NAND(u,v) | A⊕B |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 0 |
| 0 | 1 | 1 | 1 | 1 | 0 | 0 | 1 | 1 |
| 1 | 0 | 1 | 0 | 0 | 1 | 1 | 1 | 1 |
| 1 | 1 | 0 | 1 | 1 | 1 | 1 | 0 | 0 |

**6. Coste.** 4 puertas y 3 capas, frente a 1 puerta con un XOR nativo.

## Tablas

Salidas de todas las construcciones. Cada columna lleva el número de puertas entre paréntesis y debe coincidir con la puerta del mismo nombre (Tabla 2.2 del documento 02).

**Tabla 3.4. Salidas de las construcciones solo con NAND**

| A | B | NOT (1 NAND) | AND (2 NAND) | OR (3 NAND) | NOR (4 NAND) | XOR (4 NAND) | XNOR (5 NAND) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 1 | 0 | 0 | 1 | 0 | 1 |
| 0 | 1 | 1 | 0 | 1 | 0 | 1 | 0 |
| 1 | 0 | 0 | 0 | 1 | 0 | 1 | 0 |
| 1 | 1 | 0 | 1 | 1 | 0 | 0 | 1 |

**Tabla 3.5. Salidas de las construcciones solo con NOR**

| A | B | NOT (1 NOR) | OR (2 NOR) | AND (3 NOR) | NAND (4 NOR) | XNOR (4 NOR) | XOR (5 NOR) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 1 | 0 | 0 | 1 | 1 | 0 |
| 0 | 1 | 1 | 1 | 0 | 1 | 0 | 1 |
| 1 | 0 | 0 | 1 | 0 | 1 | 0 | 1 |
| 1 | 1 | 0 | 1 | 1 | 0 | 1 | 0 |

**Tabla 3.6. Resumen de construcciones: puertas, capas y comprobación exhaustiva**

| Base | Objetivo | Puertas | Capas | Filas comprobadas | Coincide con la puerta nativa | Mínimo (búsqueda exhaustiva) |
| --- | --- | --- | --- | --- | --- | --- |
| NAND | NOT | 1 | 1 | 2 | sí | 1 |
| NAND | AND | 2 | 2 | 4 | sí | 2 |
| NAND | OR | 3 | 2 | 4 | sí | 3 |
| NAND | NOR | 4 | 3 | 4 | sí | 4 |
| NAND | XOR | 4 | 3 | 4 | sí | 4 |
| NAND | XNOR | 5 | 4 | 4 | sí | 5 |
| NOR | NOT | 1 | 1 | 2 | sí | 1 |
| NOR | OR | 2 | 2 | 4 | sí | 2 |
| NOR | AND | 3 | 2 | 4 | sí | 3 |
| NOR | NAND | 4 | 3 | 4 | sí | 4 |
| NOR | XNOR | 4 | 3 | 4 | sí | 4 |
| NOR | XOR | 5 | 4 | 4 | sí | 5 |

**Tabla 3.7. Cuántas funciones distintas hay de n variables**

| Variables (n) | Filas (2^n) | Funciones distintas (2^(2^n)) |
| --- | --- | --- |
| 1 | 2 | 4 |
| 2 | 4 | 16 |
| 3 | 8 | 256 |
| 4 | 16 | 65536 |

**Tabla 3.8. Las 16 funciones de dos variables: código, fórmula y mínimo de puertas con NAND y con NOR**

| Código | Función | Fórmula | Mín. NAND | Mín. NOR |
| --- | --- | --- | --- | --- |
| 0b0000 = 0x0 | constante 0 | 0 | 3 | 2 |
| 0b0001 = 0x1 | NOR | ¬(A+B) | 4 | 1 |
| 0b0010 = 0x2 | solo B sin A | ¬A·B | 3 | 2 |
| 0b0011 = 0x3 | NOT A | ¬A | 1 | 1 |
| 0b0100 = 0x4 | solo A sin B | A·¬B | 3 | 2 |
| 0b0101 = 0x5 | NOT B | ¬B | 1 | 1 |
| 0b0110 = 0x6 | XOR | A⊕B | 4 | 5 |
| 0b0111 = 0x7 | NAND | ¬(A·B) | 1 | 4 |
| 0b1000 = 0x8 | AND | A·B | 2 | 3 |
| 0b1001 = 0x9 | XNOR | ¬(A⊕B) | 5 | 4 |
| 0b1010 = 0xA | B | B | 0 | 0 |
| 0b1011 = 0xB | A implica B | ¬A+B | 2 | 3 |
| 0b1100 = 0xC | A | A | 0 | 0 |
| 0b1101 = 0xD | B implica A | A+¬B | 2 | 3 |
| 0b1110 = 0xE | OR | A+B | 3 | 2 |
| 0b1111 = 0xF | constante 1 | 1 | 2 | 3 |

**Tabla 3.9. Funciones que se pueden construir con cada conjunto de puertas (sin constantes salvo que se indique)**

| Puertas disponibles | Alcanzables con 2 variables | Total (2 variables) | Alcanzables con 3 variables | Total (3 variables) | ¿Completo? |
| --- | --- | --- | --- | --- | --- |
| AND, OR | 4 | 16 | 18 | 256 | no |
| XOR | 4 | 16 | 8 | 256 | no |
| AND, XOR | 8 | 16 | 128 | 256 | no |
| AND, XOR y la constante 1 | 16 | 16 | 256 | 256 | sí |
| AND, OR, NOT | 16 | 16 | 256 | 256 | sí |
| NAND | 16 | 16 | 256 | 256 | sí |
| NOR | 16 | 16 | 256 | 256 | sí |

## Mini-ejercicio

Construye XNOR(A,B) usando solo NAND.

1. ¿Cuántas puertas y cuántas capas necesitas?
2. Compruébalo con una tabla de verdad.
3. ¿Y usando solo NOR?

---

**Solución**

1. XNOR es el NOT de XOR: partes de las 4 puertas del XOR y añades un NAND con las dos entradas unidas. Total: 5 puertas y 4 capas. La búsqueda exhaustiva confirma que con 4 NAND o menos no existe (Tabla 3.8).

```text
# objetivo: XNOR | base: NAND | puertas: 5 | entradas: A, B
t = NAND(A, B)
u = NAND(A, t)
v = NAND(B, t)
x = NAND(u, v)
Y = NAND(x, x)
```

2. Traza (las columnas Y y ¬(A⊕B) coinciden en las 4 filas):

**Tabla 3.10. Traza de XNOR con 5 NAND**

| A | B | t = NAND(A,B) | u = NAND(A,t) | v = NAND(B,t) | x = NAND(u,v) | Y = NAND(x,x) | ¬(A⊕B) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 1 | 1 | 1 | 0 | 1 | 1 |
| 0 | 1 | 1 | 1 | 0 | 1 | 0 | 0 |
| 1 | 0 | 1 | 0 | 1 | 1 | 0 | 0 |
| 1 | 1 | 0 | 1 | 1 | 0 | 1 | 1 |

3. Con NOR es más barato: la misma estructura de 4 puertas ya da XNOR, sin NOT final (Paso 6). Con NAND el XOR es la versión corta; con NOR lo es el XNOR.

## En Redstone / en Satisfactory

- **En Redstone**, según la Minecraft Wiki, un NOR se implementa con una antorcha con dos o más entradas, una antorcha con una sola entrada es un NOT, y la propia wiki afirma que todas las puertas lógicas se pueden construir con combinaciones de NOR (y lo mismo con NAND) ([Redstone circuits/Logic](https://minecraft.wiki/w/Redstone_circuits/Logic)).
- **En Satisfactory** cuántos tipos de puerta ofrecer (uno solo, o varios) es una decisión abierta: el mod aún no está diseñado. Lo que sí queda establecido es el precio: con solo NAND, XOR cuesta 4 piezas y XNOR 5; con solo NOR (la puerta nativa de Redstone: la antorcha con dos entradas), XOR cuesta 5, XNOR 4, OR 2 y AND 3.

## Resumen

- Con NAND se construyen NOT (1 puerta), AND (2), OR (3), NOR (4) y XOR (4); con NOR, NOT (1), OR (2), AND (3), NAND (4) y XOR (5).
- Cada construcción se ha comprobado en todas las filas, y los conteos son mínimos (búsqueda exhaustiva sobre todas las funciones de dos variables).
- Un conjunto de puertas es funcionalmente completo si construye cualquier función: NAND y NOR lo son; AND+OR sin NOT, no.
- La universalidad viene de la suma de productos del documento 02: AND, OR y NOT bastan, y NAND construye los tres.
- Un solo primitivo basta, pero conviene ofrecer varios porque cada puerta que falta se paga en piezas y en capas.
