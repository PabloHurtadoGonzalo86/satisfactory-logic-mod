# 01 · Numeración binaria y hexadecimal

## Objetivo

Entender qué son un bit, un nibble y un byte, y pasar de binario a decimal y a hexadecimal a mano.
Ver por qué un dígito hexadecimal son exactamente 4 bits y por qué la señal 0-15 de Redstone cabe justo en un dígito hex.

## Idea intuitiva

Piensa en el cuentakilómetros de un coche antiguo: cada rueda tiene 10 posiciones (0 a 9) y, cuando pasa de 9 a 0, empuja una posición a la rueda de su izquierda. Eso es contar en base 10.

- **Binario**: lo mismo, pero con ruedas de 2 posiciones (0 y 1). Cada rueda es un **bit**.
- **Hexadecimal**: ruedas de 16 posiciones (0 a 9 y después A, B, C, D, E, F).

El truco de todo este documento: una rueda hexadecimal tiene 16 = 2^4 posiciones, así que equivale exactamente a 4 ruedas binarias. Por eso el hexadecimal es la forma corta de escribir binario. Ya lo usas a diario: el color #f97316 (el naranja de acento de un dashboard web) son 3 bytes escritos con 6 dígitos hex.

## Explicación paso a paso

### Paso 1. Bit, nibble y byte

- Un **bit** es un dígito binario: vale 0 o 1.
- Un **nibble** son 4 bits (medio byte).
- Un **byte** son 8 bits, es decir, 2 nibbles.

Con n bits se pueden escribir 2^n valores distintos (Tabla 1.3): un nibble da 16 (de 0 a 15) y un byte da 256 (de 0 a 255).

**Notación de toda la serie**: binario con prefijo `0b`, hexadecimal con prefijo `0x`, y cada literal con tantos dígitos como bits tenga lo que representa: un nibble son 4 dígitos binarios (1 hexadecimal) y un byte son 8 (2 hexadecimales), con los ceros a la izquierda que hagan falta, y una señal de un solo bit es simplemente 0b0 o 0b1. Por ejemplo, 0b0110 = 6 = 0x6 es el mismo nibble escrito de tres formas. En el texto, 2^n es la potencia «2 elevado a n» (en código no se escribe así: ver el Paso 6). Python acepta esos prefijos como literales y PHP también: según el manual de PHP, el binario se escribe con el prefijo 0b y el hexadecimal con 0x ([manual de PHP](https://www.php.net/manual/en/language.types.integer.php)).

**De dónde salen las cifras.** Las tablas, ecuaciones y cifras de esta serie las calcula y las comprueba un script de Python del repositorio (los ficheros `check_*.py` de `docs/theory/`). Cuando un documento dice «el script», se refiere a ese programa: si una cifra del texto estuviera mal, el script fallaría.

### Paso 2. De binario a decimal

Cada posición pesa una potencia de 2: contando desde la derecha, 1, 2, 4, 8, 16... (Tabla 1.4). Para convertir, suma los pesos de las posiciones que tienen un 1:

- 0b1011 = 8 + 2 + 1 = 11 = 0xB
- 0b10011100 = 128 + 16 + 8 + 4 = 156 = 0x9C

### Paso 3. De decimal a binario

Divide entre 2 una y otra vez y apunta el resto de cada división (siempre 0 o 1). Cuando el cociente llega a 0, el número binario son los restos leídos **de abajo arriba**: el último resto es el bit más alto. El ejemplo resuelto lo hace paso a paso con 249.

Alternativa mental: recorre los pesos de mayor a menor; si el peso cabe en lo que queda, pon un 1 y réstalo; si no, pon un 0.

### Paso 4. Por qué un dígito hex son exactamente 4 bits

1. 4 bits dan 2^4 = 16 patrones distintos (Tabla 1.3).
2. Un dígito hexadecimal tiene 16 símbolos distintos: 0-9 y A-F.
3. Hay 16 patrones y 16 símbolos, así que se puede asignar un símbolo a cada patrón sin repetir ni dejar ninguno fuera (Tabla 1.5). Un dígito hex es *exactamente* un nibble.

Consecuencia práctica: 2 dígitos hex son un byte, y para pasar de binario a hex no hay que calcular nada: se agrupa de 4 en 4 desde la derecha y se traduce cada grupo con la Tabla 1.5.

```text
1001 1100   (grupos de 4 bits)
   9    C   (un digito hex por grupo)
```

Resultado: 0b10011100 = 156 = 0x9C.

### Paso 5. Entre hexadecimal y decimal

Igual que en binario, pero con pesos de 16 (1, 16, 256...). La letra C vale 12 (Tabla 1.5), así que:

- 0x9C = 9 × 16 + 12 = 156
- Al revés, dividiendo entre 16, el cociente y el resto son los dos dígitos: 156 = 9 × 16 + 12 = 0x9C

### Paso 6. Operaciones bit a bit

Una operación bit a bit aplica la misma regla en cada columna de bits, de forma independiente. Aquí las escribo con palabras (AND, OR, XOR, NOT); en el documento 02 se escriben con símbolos de fórmula: AND como ·, OR como +, XOR como ⊕ y NOT como ¬. En este documento, + y × son la suma y el producto aritméticos de siempre.

- **AND**: 1 solo si los dos bits son 1.
- **OR**: 1 si al menos uno de los bits es 1.
- **XOR**: 1 si los dos bits son distintos.
- **NOT**: invierte el bit (0 pasa a 1 y 1 pasa a 0); solo tiene un operando.

Con los nibbles A = 0b1100 y B = 0b1010, columna a columna:

```text
    0b1100
AND 0b1010
    ------
    0b1000
```

- 0b1100 AND 0b1010 = 0b1000
- 0b1100 OR 0b1010 = 0b1110
- 0b1100 XOR 0b1010 = 0b0110

**NOT depende del ancho.** Invierte todos los bits que existen, así que hay que decir cuántos son: como nibble, NOT 0b0110 = 0b1001; como byte, NOT 0b00000110 = 0b11111001.

**Desplazamientos.** `x << n` mueve los bits n posiciones a la izquierda y rellena con ceros por la derecha; cada posición multiplica por 2 *mientras no se pierdan bits*. `x >> n` los mueve a la derecha y los que salen se pierden: equivale a la división entera entre 2^n (aquí solo usamos valores sin signo). En un ancho fijo, los bits que salen por la izquierda desaparecen: 0b1100 << 1 = 0b1000 (en un nibble, 12 × 2 = 24 no cabe, porque el máximo es 15, y queda 24 - 16 = 8). Más casos en la Tabla 1.7.

**Máscaras** (el uso más habitual en programación):

- Quedarse con unos bits: AND con una máscara que tenga 1 donde quieres conservar. Nibble bajo de un byte: 0xB6 AND 0x0F = 0x06.
- Obtener el nibble alto: desplazar a la derecha. 0xB6 >> 4 = 0x0B.
- Encender bits: OR con la máscara. 0b0110 OR 0b0001 = 0b0111.
- Invertir bits concretos: XOR con la máscara. 0b0110 XOR 0b0011 = 0b0101.
- Apagar bits: AND con el NOT de la máscara. NOT 0b0010 = 0b1101, y después 0b0110 AND 0b1101 = 0b0100.

**Mismos operadores en PHP y Python:**

```text
AND  ->  a & b
OR   ->  a | b
XOR  ->  a ^ b
NOT  ->  ~a   (en un ancho fijo: hay que enmascarar)
<<   ->  a << n
>>   ->  a >> n
POW  ->  a ** n   (ojo: a ^ n es XOR, no una potencia)
```

Ojo con `^`: en código es XOR, no la potencia. `2 ^ 4` vale 6 y no 16; la potencia se escribe `2 ** 4`. Por eso, en el texto de la serie, 2^n es siempre «2 elevado a n».

Ojo con `~`: en Python, ~6 vale -7, porque el entero tiene signo y `~` invierte todos sus bits. Según el manual de PHP, PHP no admite enteros sin signo ([enteros](https://www.php.net/manual/en/language.types.integer.php)) y `~` invierte los bits del operando ([operadores bit a bit](https://www.php.net/manual/en/language.operators.bitwise.php)), así que hay que enmascarar igual: ~6 & 15 vale 9, que es el NOT 0b0110 = 0b1001 de arriba.

### Paso 7. La señal 0-15 de Redstone cabe justo en un dígito hexadecimal

Según la Minecraft Wiki, una señal de Redstone activa tiene una intensidad entera entre 1 y 15 ([Redstone mechanics](https://minecraft.wiki/w/Redstone_mechanics)), y los niveles van de 0 a 15 ([Redstone circuits/Transmission](https://minecraft.wiki/w/Redstone_circuits/Transmission)), así que una transmisión analógica puede transportar 16 estados por un solo cable ([Redstone circuits/Transmission](https://minecraft.wiki/w/Redstone_circuits/Transmission)).

16 estados = 2^4 = un nibble = un dígito hexadecimal. No es un parecido: es la misma cuenta de información. La Tabla 1.5 es exactamente la correspondencia nivel ↔ dígito hex ↔ nibble. Por ejemplo, el nivel 11 es 0b1011 = 11 = 0xB.

**Ojo: la equivalencia es de cantidad de información, no de cableado.** En Redstone el nivel es un único valor que viaja por un solo cable, no cuatro hilos con un bit cada uno: para obtener los 4 bits de un nivel hacen falta circuitos que lo decodifiquen. La propia wiki distingue la codificación analógica (16 estados en un cable) de la binaria de 4 líneas, que contiene la misma cantidad de información ([Redstone circuits/Transmission](https://minecraft.wiki/w/Redstone_circuits/Transmission)).

## Ejemplo resuelto completo: descomponer y recomponer #f97316

Usamos como ejemplo el naranja de acento de un dashboard web, #f97316. En CSS, `#RRGGBB` son 3 bytes (rojo, verde, azul) escritos en hexadecimal: 2 dígitos hex por canal.

**1. Separar en pares de dígitos hex:** F9 | 73 | 16, es decir, 0xF9, 0x73 y 0x16.

**2. Cada byte a decimal** (cada dígito hex vale lo que dice la Tabla 1.5; la F vale 15):

- 0xF9 = 15 × 16 + 9 = 249
- 0x73 = 7 × 16 + 3 = 115
- 0x16 = 1 × 16 + 6 = 22

**3. Cada byte a binario** (un nibble por dígito hex, Tabla 1.5): ver la Tabla 1.1.

**Tabla 1.1. Los 3 canales de #f97316**

| Canal | Hex | Decimal | Binario |
| --- | --- | --- | --- |
| Rojo (R) | 0xF9 | 249 | 0b11111001 |
| Verde (G) | 0x73 | 115 | 0b01110011 |
| Azul (B) | 0x16 | 22 | 0b00010110 |

**4. Comprobación del rojo por divisiones sucesivas.** Dividimos 249 entre 2 hasta llegar a 0 y leemos los restos de abajo arriba:

**Tabla 1.2. Divisiones sucesivas entre 2 de 249 (el canal rojo)**

| Paso | Número | Cociente (número ÷ 2) | Resto |
| --- | --- | --- | --- |
| 1 | 249 | 124 | 1 |
| 2 | 124 | 62 | 0 |
| 3 | 62 | 31 | 0 |
| 4 | 31 | 15 | 1 |
| 5 | 15 | 7 | 1 |
| 6 | 7 | 3 | 1 |
| 7 | 3 | 1 | 1 |
| 8 | 1 | 0 | 1 |

Restos de abajo arriba: 11111001, que es 0b11111001 = 249 = 0xF9. Coincide con el punto 3 de este ejemplo.

**5. Los 3 bytes como un único entero de 24 bits** (rojo en los bits altos):

- 0xF97316 = 16347926 = 0b111110010111001100010110

**6. Extraer los canales con desplazamientos y máscaras.** Todo con ancho 24 bits:

- Rojo: 0xF97316 >> 16 = 0x0000F9
- Verde, primero desplazar: 0xF97316 >> 8 = 0x00F973
- Verde, después enmascarar: 0x00F973 AND 0x0000FF = 0x000073
- Azul, solo máscara: 0xF97316 AND 0x0000FF = 0x000016

**7. Recomponer.** Cada canal se coloca en su sitio con `<<` y se juntan con OR:

- 0x0000F9 << 16 = 0xF90000
- 0x000073 << 8 = 0x007300
- 0xF90000 OR 0x007300 = 0xF97300
- 0xF97300 OR 0x000016 = 0xF97316

Hemos vuelto a 0xF97316, el valor del principio. Lo que has hecho a mano es lo que hace cualquier librería de colores por dentro.

## Tablas

Tablas de referencia (las del ejemplo están arriba, Tablas 1.1 y 1.2).

**Tabla 1.3. Cuántos valores caben en n bits**

| Bits (n) | Valores distintos (2^n) | Valor máximo (2^n - 1) | Nombre |
| --- | --- | --- | --- |
| 1 | 2 | 1 | bit |
| 2 | 4 | 3 |  |
| 3 | 8 | 7 |  |
| 4 | 16 | 15 | nibble |
| 5 | 32 | 31 |  |
| 6 | 64 | 63 |  |
| 7 | 128 | 127 |  |
| 8 | 256 | 255 | byte |

**Tabla 1.4. Peso de cada posición de un byte (de derecha a izquierda)**

| Posición | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Peso | 128 | 64 | 32 | 16 | 8 | 4 | 2 | 1 |
| Bits de 0b10011100 | 1 | 0 | 0 | 1 | 1 | 1 | 0 | 0 |

**Tabla 1.5. Los 16 dígitos hexadecimales, su valor y su nibble (= nivel de señal de Redstone)**

| Hex | Decimal | Binario |
| --- | --- | --- |
| 0x0 | 0 | 0b0000 |
| 0x1 | 1 | 0b0001 |
| 0x2 | 2 | 0b0010 |
| 0x3 | 3 | 0b0011 |
| 0x4 | 4 | 0b0100 |
| 0x5 | 5 | 0b0101 |
| 0x6 | 6 | 0b0110 |
| 0x7 | 7 | 0b0111 |
| 0x8 | 8 | 0b1000 |
| 0x9 | 9 | 0b1001 |
| 0xA | 10 | 0b1010 |
| 0xB | 11 | 0b1011 |
| 0xC | 12 | 0b1100 |
| 0xD | 13 | 0b1101 |
| 0xE | 14 | 0b1110 |
| 0xF | 15 | 0b1111 |

**Tabla 1.6. AND, OR y XOR sobre un bit (NOT simplemente invierte el bit)**

| A | B | A AND B | A OR B | A XOR B |
| --- | --- | --- | --- | --- |
| 0 | 0 | 0 | 0 | 0 |
| 0 | 1 | 0 | 1 | 1 |
| 1 | 0 | 0 | 1 | 1 |
| 1 | 1 | 1 | 1 | 0 |

**Tabla 1.7. Desplazamientos del nibble x = 0b0110 (valor 6)**

| n | x << n | Valor (<<) | x >> n | Valor (>>) |
| --- | --- | --- | --- | --- |
| 0 | 0b0110 | 6 | 0b0110 | 6 |
| 1 | 0b1100 | 12 | 0b0011 | 3 |
| 2 | 0b1000 | 8 | 0b0001 | 1 |
| 3 | 0b0000 | 0 | 0b0000 | 0 |
| 4 | 0b0000 | 0 | 0b0000 | 0 |

## Mini-ejercicio

Tienes el byte 0b10110110.

1. Pásalo a decimal y a hexadecimal.
2. Sepáralo en nibble alto y nibble bajo con `>>` y con AND con una máscara.
3. ¿Qué nivel de señal de Redstone (de 0 a 15) representa cada nibble?
4. Calcula el NOT del byte completo (8 bits).

---

**Solución**

1. 0b10110110 = 128 + 32 + 16 + 4 + 2 = 182 = 0xB6
2. 0xB6 >> 4 = 0x0B y 0xB6 AND 0x0F = 0x06.
3. 0x0B = 11 y 0x06 = 6: nivel 11 el alto y nivel 6 el bajo.
4. NOT 0b10110110 = 0b01001001.

## En Redstone / en Satisfactory

- **En Redstone**: la Minecraft Wiki da los niveles de 0 a 15 y habla de 16 estados por cable, justo la información de un nibble (de 0x0 a 0xF), aunque el nivel viaja como un único valor y no como 4 bits sueltos.
- **Bits sueltos**: muchos circuitos de Redstone solo distinguen «hay señal» (mayor que 0) de «no hay señal», así que aprovechan 1 bit de información de los 4 que cabrían ([Redstone circuit](https://minecraft.wiki/w/Redstone_circuit)). Ojo: eso es «x > 0», el OR de los 4 bits, y no mirar un bit concreto: el nivel 0b0010 = 2 = 0x2 cuenta como «hay señal» aunque su bit más bajo valga 0.
- **En Satisfactory**: qué ancho tendrán las señales de un mod (1 bit, 4 bits u otro) es una decisión abierta, porque el mod aún no está diseñado; aquí solo tienes el vocabulario para decidirla.

## Resumen

- Un bit vale 0 o 1; un nibble son 4 bits (16 valores) y un byte son 8 bits (256 valores).
- Binario a decimal: suma los pesos 1, 2, 4, 8... de los bits a 1; decimal a binario: divide entre 2 y lee los restos de abajo arriba.
- Un dígito hex equivale exactamente a un nibble porque 16 = 2^4; para convertir basta agrupar de 4 en 4 bits.
- AND, OR, XOR, NOT y los desplazamientos operan bit a bit; con máscaras sirven para extraer, encender, apagar e invertir bits (y `~` necesita máscara).
- La señal de Redstone, de 0 a 15, tiene 16 niveles: lleva exactamente la información de un dígito hexadecimal (aunque viaja por un solo cable, no como 4 bits sueltos).
