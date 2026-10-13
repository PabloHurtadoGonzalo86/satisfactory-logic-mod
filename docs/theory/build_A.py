#!/usr/bin/env python3
"""Ensambla los documentos 01, 02 y 03 (clave A).

Los numeros, tablas, ecuaciones y mapas salen de lib_A.py; la prosa esta aqui.
Uso:  python3 build_A.py          (escribe los .md junto a este script)
"""
from __future__ import annotations

import pathlib
import sys

sys.dont_write_bytecode = True

from lib_A import (
    Ctx, Net, TARGETS, bits, hexs, chain, weights_sum, op_eq, not_eq, shift_eq,
    calc_block_text, truth_table, rows_of, func_code, kmap_text, prime_implicants,
    min_cover, min_cover_dc, cube_term, cube_minterms, cube_literals, sop_cost, min_gate_counts,
    int_gates, var_codes, closure_size, net_signals, maj3, gray,
    b_not, b_and, b_or, b_xor, b_nand, b_nor, b_xnor,
)

HERE = pathlib.Path(__file__).resolve().parent

URL_TRANSMISSION = "https://minecraft.wiki/w/Redstone_circuits/Transmission"
URL_MECHANICS = "https://minecraft.wiki/w/Redstone_mechanics"
URL_TORCH = "https://minecraft.wiki/w/Redstone_Torch"
URL_PHP_INT = "https://www.php.net/manual/en/language.types.integer.php"
URL_PHP_BIT = "https://www.php.net/manual/en/language.operators.bitwise.php"

NB = 4                 # bits de un nibble
BB = 8                 # bits de un byte
NV = 2**NB             # valores de un nibble
BV = 2**BB             # valores de un byte
MAXN = NV - 1          # mayor valor de un nibble
MAXB = BV - 1          # mayor valor de un byte


# ==========================================================================
# DOCUMENTO 01
# ==========================================================================
def build_doc01() -> tuple[str, Ctx]:
    c = Ctx()
    C = c.claim

    # ---- datos del ejemplo (color de acento del dashboard) -------------
    R, G, B = 0xF9, 0x73, 0x16
    CHANNELS = (R, G, B)
    rgb = (R << 16) | (G << 8) | B
    color = f"#{R:02x}{G:02x}{B:02x}"
    assert color == "#f97316"

    # ---- tablas del ejemplo ------------------------------------------------
    div_rows = []
    n, step = R, 1
    rem_list = []
    while n > 0:
        div_rows.append([step, n, n // 2, n % 2])
        rem_list.append(n % 2)
        n //= 2
        step += 1
    t_div = c.table(
        "1.2", f"Divisiones sucesivas entre 2 de {R} (el canal rojo)",
        ["Paso", "Número", "Cociente (número ÷ 2)", "Resto"], div_rows,
    )
    rem_read = "".join(str(r) for r in reversed(rem_list))
    assert int(rem_read, 2) == R

    chan_rows = []
    for name, v in (("Rojo (R)", R), ("Verde (G)", G), ("Azul (B)", B)):
        chan_rows.append([name, hexs(v, 8), v, bits(v, 8)])
    t_chan = c.table("1.1", f"Los {len(CHANNELS)} canales de {color}", ["Canal", "Hex", "Decimal", "Binario"], chan_rows)

    # ---- tablas de referencia ------------------------------------------------
    t_pow = c.table(
        "1.3", "Cuántos valores caben en n bits",
        ["Bits (n)", "Valores distintos (2^n)", "Valor máximo (2^n - 1)", "Nombre"],
        [[k, 2**k, 2**k - 1, {1: "bit", NB: "nibble", BB: "byte"}.get(k, "")] for k in range(1, BB + 1)],
    )
    sample = 0b10011100
    t_w = c.table(
        "1.4", "Peso de cada posición de un byte (de derecha a izquierda)",
        ["Posición"] + [str(k) for k in range(BB - 1, -1, -1)],
        [
            ["Peso"] + [2**k for k in range(BB - 1, -1, -1)],
            [f"Bits de {bits(sample, 8)}"] + [(sample >> k) & 1 for k in range(BB - 1, -1, -1)],
        ],
    )
    t_hex = c.table(
        "1.5", "Los 16 dígitos hexadecimales, su valor y su nibble (= nivel de señal de Redstone)",
        ["Hex", "Decimal", "Binario"],
        [[hexs(v, 4), v, bits(v, 4)] for v in range(NV)],
    )
    t_ops = truth_table(
        c, "1.6", "AND, OR y XOR sobre un bit (NOT simplemente invierte el bit)", ["A", "B"],
        [("A AND B", b_and), ("A OR B", b_or), ("A XOR B", b_xor)],
    )
    x6 = 0b0110
    t_shift = c.table(
        "1.7", f"Desplazamientos del nibble x = {bits(x6, 4)} (valor {x6})",
        ["n", "x << n", "Valor (<<)", "x >> n", "Valor (>>)"],
        [[k, bits((x6 << k) & (NV - 1), 4), (x6 << k) & (NV - 1), bits(x6 >> k, 4), x6 >> k] for k in range(0, NB + 1)],
    )

    # ---- bloques ASCII -----------------------------------------------------
    g_hi, g_lo = (sample >> 4) & 0xF, sample & 0xF
    # el bloque de agrupacion es ASCII puro (sin tildes)
    grp_text = f"{g_hi:04b} {g_lo:04b}   (grupos de 4 bits)\n   {g_hi:X}    {g_lo:X}   (un digito hex por grupo)"
    grp_block = c.block("group", grp_text)

    a_n, b_n = 0b1100, 0b1010
    calc_and = c.block("calc", calc_block_text(a_n, "AND", b_n, 4))
    ops_block = c.block(
        "ascii",
        "AND  ->  a & b\nOR   ->  a | b\nXOR  ->  a ^ b\nNOT  ->  ~a   (en un ancho fijo: hay que enmascarar)\n"
        "<<   ->  a << n\n>>   ->  a >> n\nPOW  ->  a ** n   (ojo: a ^ n es XOR, no una potencia)",
    )
    # la potencia del texto (2^n) no es el `^` del codigo: el ejemplo sale del propio calculo
    assert (2 ^ NB, 2 ** NB) == (6, 16)
    # los literales 0b0 y 0b1 (una senal de un bit) son de ancho libre
    bit_off, bit_on = c.free_tok("0b0"), c.free_tok("0b1")

    # R08: 'hay senal' es x > 0, que es el OR de los cuatro bits, no un bit concreto
    assert all(bool(x & (NV - 1)) == (x > 0) for x in range(NV))
    LEVEL2 = 2
    assert LEVEL2 > 0 and (LEVEL2 & 1) == 0  # verdadero aunque su bit mas bajo valga 0

    # ---- prosa -----------------------------------------------------------------
    md = f"""# 01 · Numeración binaria y hexadecimal

## Objetivo

Entender qué son un bit, un nibble y un byte, y pasar de binario a decimal y a hexadecimal a mano.
Ver por qué un dígito hexadecimal son exactamente {NB} bits y por qué la señal 0-{MAXN} de Redstone cabe justo en un dígito hex.

## Idea intuitiva

Piensa en el cuentakilómetros de un coche antiguo: cada rueda tiene 10 posiciones (0 a 9) y, cuando pasa de 9 a 0, empuja una posición a la rueda de su izquierda. Eso es contar en base 10.

- **Binario**: lo mismo, pero con ruedas de 2 posiciones (0 y 1). Cada rueda es un **bit**.
- **Hexadecimal**: ruedas de {NV} posiciones (0 a 9 y después A, B, C, D, E, F).

El truco de todo este documento: una rueda hexadecimal tiene {NV} = 2^{NB} posiciones, así que equivale exactamente a {NB} ruedas binarias. Por eso el hexadecimal es la forma corta de escribir binario. Ya lo usas a diario: el color {color} (el naranja de acento de un dashboard web) son {len(CHANNELS)} bytes escritos con {len(CHANNELS) * BB // NB} dígitos hex.

## Explicación paso a paso

### Paso 1. Bit, nibble y byte

- Un **bit** es un dígito binario: vale 0 o 1.
- Un **nibble** son {NB} bits (medio byte).
- Un **byte** son {BB} bits, es decir, {BB // NB} nibbles.

Con n bits se pueden escribir 2^n valores distintos (Tabla 1.3): un nibble da {NV} (de 0 a {MAXN}) y un byte da {BV} (de 0 a {MAXB}).

**Notación de toda la serie**: binario con prefijo `0b`, hexadecimal con prefijo `0x`, y cada literal con tantos dígitos como bits tenga lo que representa: un nibble son {NB} dígitos binarios (1 hexadecimal) y un byte son {BB} (2 hexadecimales), con los ceros a la izquierda que hagan falta, y una señal de un solo bit es simplemente {bit_off} o {bit_on}. Por ejemplo, {C(chain(6, 4))} es el mismo nibble escrito de tres formas. En el texto, 2^n es la potencia «2 elevado a n» (en código no se escribe así: ver el Paso 6). Python acepta esos prefijos como literales y PHP también: según el manual de PHP, el binario se escribe con el prefijo 0b y el hexadecimal con 0x ([manual de PHP]({URL_PHP_INT})).

**De dónde salen las cifras.** Las tablas, ecuaciones y cifras de esta serie las calcula y las comprueba un script de Python del repositorio (los ficheros `check_*.py` de `docs/theory/`). Cuando un documento dice «el script», se refiere a ese programa: si una cifra del texto estuviera mal, el script fallaría.

### Paso 2. De binario a decimal

Cada posición pesa una potencia de 2: contando desde la derecha, 1, 2, 4, 8, 16... (Tabla 1.4). Para convertir, suma los pesos de las posiciones que tienen un 1:

- {C(f"{bits(11, 4)} = {weights_sum(11, 4)} = 11 = {hexs(11, 4)}")}
- {C(f"{bits(sample, 8)} = {weights_sum(sample, 8)} = {sample} = {hexs(sample, 8)}")}

### Paso 3. De decimal a binario

Divide entre 2 una y otra vez y apunta el resto de cada división (siempre 0 o 1). Cuando el cociente llega a 0, el número binario son los restos leídos **de abajo arriba**: el último resto es el bit más alto. El ejemplo resuelto lo hace paso a paso con {R}.

Alternativa mental: recorre los pesos de mayor a menor; si el peso cabe en lo que queda, pon un 1 y réstalo; si no, pon un 0.

### Paso 4. Por qué un dígito hex son exactamente {NB} bits

1. {NB} bits dan 2^{NB} = {NV} patrones distintos (Tabla 1.3).
2. Un dígito hexadecimal tiene {NV} símbolos distintos: 0-9 y A-F.
3. Hay {NV} patrones y {NV} símbolos, así que se puede asignar un símbolo a cada patrón sin repetir ni dejar ninguno fuera (Tabla 1.5). Un dígito hex es *exactamente* un nibble.

Consecuencia práctica: {BB // NB} dígitos hex son un byte, y para pasar de binario a hex no hay que calcular nada: se agrupa de {NB} en {NB} desde la derecha y se traduce cada grupo con la Tabla 1.5.

{grp_block}

Resultado: {C(chain(sample, 8))}.

### Paso 5. Entre hexadecimal y decimal

Igual que en binario, pero con pesos de 16 (1, 16, 256...). La letra C vale {0xC} (Tabla 1.5), así que:

- {C(f"{hexs(sample, 8)} = {g_hi} × 16 + {g_lo} = {sample}")}
- Al revés, dividiendo entre 16, el cociente y el resto son los dos dígitos: {C(f"{sample} = {g_hi} × 16 + {g_lo} = {hexs(sample, 8)}")}

### Paso 6. Operaciones bit a bit

Una operación bit a bit aplica la misma regla en cada columna de bits, de forma independiente. Aquí las escribo con palabras (AND, OR, XOR, NOT); en el documento 02 se escriben con símbolos de fórmula: AND como ·, OR como +, XOR como ⊕ y NOT como ¬. En este documento, + y × son la suma y el producto aritméticos de siempre.

- **AND**: 1 solo si los dos bits son 1.
- **OR**: 1 si al menos uno de los bits es 1.
- **XOR**: 1 si los dos bits son distintos.
- **NOT**: invierte el bit (0 pasa a 1 y 1 pasa a 0); solo tiene un operando.

Con los nibbles A = {c.given_tok(bits(a_n, 4))} y B = {c.given_tok(bits(b_n, 4))}, columna a columna:

{calc_and}

- {C(op_eq(a_n, "AND", b_n, 4))}
- {C(op_eq(a_n, "OR", b_n, 4))}
- {C(op_eq(a_n, "XOR", b_n, 4))}

**NOT depende del ancho.** Invierte todos los bits que existen, así que hay que decir cuántos son: como nibble, {C(not_eq(6, 4))}; como byte, {C(not_eq(6, 8))}.

**Desplazamientos.** `x << n` mueve los bits n posiciones a la izquierda y rellena con ceros por la derecha; cada posición multiplica por 2 *mientras no se pierdan bits*. `x >> n` los mueve a la derecha y los que salen se pierden: equivale a la división entera entre 2^n (aquí solo usamos valores sin signo). En un ancho fijo, los bits que salen por la izquierda desaparecen: {C(shift_eq(0b1100, "<<", 1, 4))} (en un nibble, {0b1100} × 2 = {0b1100 * 2} no cabe, porque el máximo es {MAXN}, y queda {0b1100 * 2} - {NV} = {(0b1100 * 2) % NV}). Más casos en la Tabla 1.7.

**Máscaras** (el uso más habitual en programación):

- Quedarse con unos bits: AND con una máscara que tenga 1 donde quieres conservar. Nibble bajo de un byte: {C(op_eq(0xB6, "AND", 0x0F, 8, True))}.
- Obtener el nibble alto: desplazar a la derecha. {C(shift_eq(0xB6, ">>", 4, 8, True))}.
- Encender bits: OR con la máscara. {C(op_eq(0b0110, "OR", 0b0001, 4))}.
- Invertir bits concretos: XOR con la máscara. {C(op_eq(0b0110, "XOR", 0b0011, 4))}.
- Apagar bits: AND con el NOT de la máscara. {C(not_eq(0b0010, 4))}, y después {C(op_eq(0b0110, "AND", 0b1101, 4))}.

**Mismos operadores en PHP y Python:**

{ops_block}

Ojo con `^`: en código es XOR, no la potencia. `2 ^ {NB}` vale {2 ^ NB} y no {2 ** NB}; la potencia se escribe `2 ** {NB}`. Por eso, en el texto de la serie, 2^n es siempre «2 elevado a n».

Ojo con `~`: en Python, {C(f"~{6} vale {~6}")}, porque el entero tiene signo y `~` invierte todos sus bits. Según el manual de PHP, PHP no admite enteros sin signo ([enteros]({URL_PHP_INT})) y `~` invierte los bits del operando ([operadores bit a bit]({URL_PHP_BIT})), así que hay que enmascarar igual: {C(f"~{6} & {15} vale {(~6) & 15}")}, que es el {not_eq(6, 4)} de arriba.

### Paso 7. La señal 0-{MAXN} de Redstone cabe justo en un dígito hexadecimal

Según la Minecraft Wiki, una señal de Redstone activa tiene una intensidad entera entre 1 y {MAXN} ([Redstone mechanics]({URL_MECHANICS})), y los niveles van de 0 a {MAXN} ([Redstone circuits/Transmission]({URL_TRANSMISSION})), así que una transmisión analógica puede transportar {NV} estados por un solo cable ([Redstone circuits/Transmission]({URL_TRANSMISSION})).

{NV} estados = 2^{NB} = un nibble = un dígito hexadecimal. No es un parecido: es la misma cuenta de información. La Tabla 1.5 es exactamente la correspondencia nivel ↔ dígito hex ↔ nibble. Por ejemplo, el nivel 11 es {C(chain(11, 4))}.

**Ojo: la equivalencia es de cantidad de información, no de cableado.** En Redstone el nivel es un único valor que viaja por un solo cable, no cuatro hilos con un bit cada uno: para obtener los {NB} bits de un nivel hacen falta circuitos que lo decodifiquen. La propia wiki distingue la codificación analógica ({NV} estados en un cable) de la binaria de {NB} líneas, que contiene la misma cantidad de información ([Redstone circuits/Transmission]({URL_TRANSMISSION})).

## Ejemplo resuelto completo: descomponer y recomponer {color}

Usamos como ejemplo el naranja de acento de un dashboard web, {color}. En CSS, `#RRGGBB` son {len(CHANNELS)} bytes (rojo, verde, azul) escritos en hexadecimal: {BB // NB} dígitos hex por canal.

**1. Separar en pares de dígitos hex:** F9 | 73 | 16, es decir, {c.given_tok(hexs(R, 8))}, {c.given_tok(hexs(G, 8))} y {c.given_tok(hexs(B, 8))}.

**2. Cada byte a decimal** (cada dígito hex vale lo que dice la Tabla 1.5; la F vale {0xF}):

- {C(f"{hexs(R, 8)} = {R >> 4} × 16 + {R & 15} = {R}")}
- {C(f"{hexs(G, 8)} = {G >> 4} × 16 + {G & 15} = {G}")}
- {C(f"{hexs(B, 8)} = {B >> 4} × 16 + {B & 15} = {B}")}

**3. Cada byte a binario** (un nibble por dígito hex, Tabla 1.5): ver la Tabla 1.1.

{t_chan}

**4. Comprobación del rojo por divisiones sucesivas.** Dividimos {R} entre 2 hasta llegar a 0 y leemos los restos de abajo arriba:

{t_div}

Restos de abajo arriba: {rem_read}, que es {C(f"{bits(R, 8)} = {R} = {hexs(R, 8)}")}. Coincide con el punto 3 de este ejemplo.

**5. Los {len(CHANNELS)} bytes como un único entero de {len(CHANNELS) * BB} bits** (rojo en los bits altos):

- {C(f"{hexs(rgb, 24)} = {rgb} = {bits(rgb, 24)}")}

**6. Extraer los canales con desplazamientos y máscaras.** Todo con ancho 24 bits:

- Rojo: {C(shift_eq(rgb, ">>", 16, 24, True))}
- Verde, primero desplazar: {C(shift_eq(rgb, ">>", 8, 24, True))}
- Verde, después enmascarar: {C(op_eq(rgb >> 8, "AND", 0xFF, 24, True))}
- Azul, solo máscara: {C(op_eq(rgb, "AND", 0xFF, 24, True))}

**7. Recomponer.** Cada canal se coloca en su sitio con `<<` y se juntan con OR:

- {C(shift_eq(R, "<<", 16, 24, True))}
- {C(shift_eq(G, "<<", 8, 24, True))}
- {C(op_eq(R << 16, "OR", G << 8, 24, True))}
- {C(op_eq((R << 16) | (G << 8), "OR", B, 24, True))}

Hemos vuelto a {c.given_tok(hexs(rgb, 24))}, el valor del principio. Lo que has hecho a mano es lo que hace cualquier librería de colores por dentro.

## Tablas

Tablas de referencia (las del ejemplo están arriba, Tablas 1.1 y 1.2).

{t_pow}

{t_w}

{t_hex}

{t_ops}

{t_shift}

## Mini-ejercicio

Tienes el byte {c.given_tok(bits(0b10110110, 8))}.

1. Pásalo a decimal y a hexadecimal.
2. Sepáralo en nibble alto y nibble bajo con `>>` y con AND con una máscara.
3. ¿Qué nivel de señal de Redstone (de 0 a {MAXN}) representa cada nibble?
4. Calcula el NOT del byte completo (8 bits).

---

**Solución**

1. {C(f"{bits(0b10110110, 8)} = {weights_sum(0b10110110, 8)} = {0b10110110} = {hexs(0b10110110, 8)}")}
2. {C(shift_eq(0xB6, ">>", 4, 8, True))} y {C(op_eq(0xB6, "AND", 0x0F, 8, True))}.
3. {C(f"{hexs(0xB6 >> 4, 8)} = {0xB6 >> 4}")} y {C(f"{hexs(0xB6 & 15, 8)} = {0xB6 & 15}")}: nivel {0xB6 >> 4} el alto y nivel {0xB6 & 15} el bajo.
4. {C(not_eq(0b10110110, 8))}.

## En Redstone / en Satisfactory

- **En Redstone**: la Minecraft Wiki da los niveles de 0 a {MAXN} y habla de {NV} estados por cable, justo la información de un nibble (de {c.given_tok(hexs(0, 4))} a {c.given_tok(hexs(MAXN, 4))}), aunque el nivel viaja como un único valor y no como {NB} bits sueltos.
- **Bits sueltos**: muchos circuitos de Redstone solo distinguen «hay señal» (mayor que 0) de «no hay señal», así que aprovechan 1 bit de información de los {NB} que cabrían ([Redstone circuit]({URL_CIRCUIT})). Ojo: eso es «x > 0», el OR de los {NB} bits, y no mirar un bit concreto: el nivel {C(chain(LEVEL2, 4))} cuenta como «hay señal» aunque su bit más bajo valga 0.
- **En Satisfactory**: qué ancho tendrán las señales de un mod (1 bit, {NB} bits u otro) es una decisión abierta, porque el mod aún no está diseñado; aquí solo tienes el vocabulario para decidirla.

## Resumen

- Un bit vale 0 o 1; un nibble son {NB} bits ({NV} valores) y un byte son {BB} bits ({BV} valores).
- Binario a decimal: suma los pesos 1, 2, 4, 8... de los bits a 1; decimal a binario: divide entre 2 y lee los restos de abajo arriba.
- Un dígito hex equivale exactamente a un nibble porque {NV} = 2^{NB}; para convertir basta agrupar de {NB} en {NB} bits.
- AND, OR, XOR, NOT y los desplazamientos operan bit a bit; con máscaras sirven para extraer, encender, apagar e invertir bits (y `~` necesita máscara).
- La señal de Redstone, de 0 a {MAXN}, tiene {NV} niveles: lleva exactamente la información de un dígito hexadecimal (aunque viaja por un solo cable, no como {NB} bits sueltos).
"""
    return md, c


# ==========================================================================
# DOCUMENTO 02
# ==========================================================================
URL_CIRCUIT = "https://minecraft.wiki/w/Redstone_circuit"


def minterm_str(k: int, names: list[str]) -> str:
    n = len(names)
    return "·".join(names[i] if (k >> (n - 1 - i)) & 1 else "¬" + names[i] for i in range(n))


def maxterm_str(k: int, names: list[str]) -> str:
    n = len(names)
    return "(" + "+".join("¬" + names[i] if (k >> (n - 1 - i)) & 1 else names[i] for i in range(n)) + ")"


def cube_group_rows(cubes: list[str], names: list[str]) -> list[list]:
    rows = []
    for i, cb in enumerate(cubes, 1):
        ms = cube_minterms(cb)
        fixed = ", ".join(f"{names[j]}={ch}" for j, ch in enumerate(cb) if ch != "-")
        rows.append([i, len(ms), ", ".join(str(m) for m in ms), fixed, cube_term(cb, names)])
    return rows


GROUP_HEAD = ["Grupo", "Casillas", "Minterms", "Variables que no cambian", "Término"]


def build_doc02() -> tuple[str, Ctx]:
    c = Ctx()
    C = c.claim
    V3 = ["A", "B", "C"]
    V4 = ["A", "B", "C", "D"]
    ROW5 = 5
    GATE_LIST = ["NOT", "AND", "OR", "XOR", "NAND", "NOR", "XNOR"]
    N_GATES = len(GATE_LIST)
    N_GATES2 = N_GATES - 1  # las de dos entradas
    MAJ_N, MAJ_K = 3, 2  # MAJ vale 1 si al menos MAJ_K de las MAJ_N entradas valen 1
    assert all(maj3(*r) == int(sum(r) >= MAJ_K) for r in rows_of(MAJ_N))
    FORMS = ["SOP", "POS", "mín", "fact"]
    N_FORMS = len(FORMS)

    # ---------------- tablas de las puertas ----------------
    t_not = truth_table(c, "2.1", "Tabla de verdad de NOT", ["A"], [("¬A", b_not)])
    t_gates = truth_table(
        c, "2.2", f"Las {N_GATES2} puertas de dos entradas", ["A", "B"],
        [
            ("AND = A·B", b_and), ("OR = A+B", b_or), ("XOR = A⊕B", b_xor),
            ("NAND = ¬(A·B)", b_nand), ("NOR = ¬(A+B)", b_nor), ("XNOR = ¬(A⊕B)", b_xnor),
        ],
    )
    # ---------------- identidades ----------------
    t_idem = truth_table(
        c, "2.3", "Idempotencia: las tres columnas coinciden en todas las filas", ["A"],
        [("A·A", lambda a: a & a), ("A+A", lambda a: a | a)],
    )
    t_abs = truth_table(
        c, "2.4", "Absorción: A, A+A·B y A·(A+B) coinciden", ["A", "B"],
        [("A·B", b_and), ("A+A·B", lambda a, b: a | (a & b)), ("A·(A+B)", lambda a, b: a & (a | b))],
    )
    t_dist1 = truth_table(
        c, "2.5", "Distributiva (1): A·(B+C) = A·B + A·C", ["A", "B", "C"],
        [
            ("B+C", lambda a, b, c_: b | c_), ("A·(B+C)", lambda a, b, c_: a & (b | c_)),
            ("A·B", lambda a, b, c_: a & b), ("A·C", lambda a, b, c_: a & c_),
            ("A·B+A·C", lambda a, b, c_: (a & b) | (a & c_)),
        ],
    )
    t_dist2 = truth_table(
        c, "2.6", "Distributiva (2): A + B·C = (A+B)·(A+C)", ["A", "B", "C"],
        [
            ("B·C", lambda a, b, c_: b & c_), ("A+B·C", lambda a, b, c_: a | (b & c_)),
            ("A+B", lambda a, b, c_: a | b), ("A+C", lambda a, b, c_: a | c_),
            ("(A+B)·(A+C)", lambda a, b, c_: (a | b) & (a | c_)),
        ],
    )
    t_dm1 = truth_table(
        c, "2.7", "De Morgan (1): ¬(A·B) = ¬A + ¬B", ["A", "B"],
        [
            ("A·B", b_and), ("¬(A·B)", b_nand), ("¬A", lambda a, b: 1 - a),
            ("¬B", lambda a, b: 1 - b), ("¬A+¬B", lambda a, b: (1 - a) | (1 - b)),
        ],
    )
    t_dm2 = truth_table(
        c, "2.8", "De Morgan (2): ¬(A+B) = ¬A · ¬B", ["A", "B"],
        [
            ("A+B", b_or), ("¬(A+B)", b_nor), ("¬A", lambda a, b: 1 - a),
            ("¬B", lambda a, b: 1 - b), ("¬A·¬B", lambda a, b: (1 - a) & (1 - b)),
        ],
    )

    # ---------------- mapas de Karnaugh de apoyo ----------------
    kidx2 = c.block("kmap-index", kmap_text(2, ["A", "B"], "Numero de minterm de cada casilla (2 variables)", lambda m: str(m)))
    kidx3 = c.block("kmap-index", kmap_text(3, V3, "Numero de minterm de cada casilla (3 variables)", lambda m: str(m)))
    kidx4 = c.block("kmap-index", kmap_text(4, V4, "Numero de minterm de cada casilla (4 variables)", lambda m: str(m)))
    or_ons = [m for m in range(4) if b_or(m >> 1, m & 1)]
    xor_ons = [m for m in range(4) if b_xor(m >> 1, m & 1)]
    ml = lambda ons: ",".join(str(m) for m in ons)  # noqa: E731
    k_or = c.block("kmap", kmap_text(2, ["A", "B"], f"K-mapa de OR(A,B), unos en los minterms {ml(or_ons)}", lambda m: str(int(m in or_ons))))
    k_xor = c.block("kmap", kmap_text(2, ["A", "B"], f"K-mapa de XOR(A,B), unos en los minterms {ml(xor_ons)}", lambda m: str(int(m in xor_ons))))

    # ---------------- ejemplo A: mayoria de 3 ----------------
    maj_rows = []
    for m, r in enumerate(rows_of(3)):
        maj_rows.append([m, *r, maj3(*r)])
    t_maj = c.table("2.9", f"Mayoría de {MAJ_N} (1 si al menos {MAJ_K} entradas valen 1)", ["m", "A", "B", "C", "f = MAJ(A,B,C)"], maj_rows)
    maj_ons = [m for m in range(8) if maj3(*rows_of(3)[m])]
    maj_off = [m for m in range(8) if m not in maj_ons]
    maj_code = func_code(maj3, 3)
    sop_maj = " + ".join(minterm_str(m, V3) for m in maj_ons)
    pos_maj = "·".join(maxterm_str(m, V3) for m in maj_off)
    pr = prime_implicants(maj_ons, 3)
    cov = min_cover(pr, maj_ons)
    min_maj = " + ".join(cube_term(cb, V3) for cb in cov)
    assert min_maj == "A·B + A·C + B·C", min_maj
    fact_maj = "A·B + C·(A+B)"
    ms = lambda ons: ",".join(str(m) for m in ons)  # noqa: E731
    k_maj = c.block("kmap", kmap_text(3, V3, f"K-mapa de MAJ(A,B,C), unos en los minterms {ms(maj_ons)}", lambda m: str(int(m in maj_ons))))
    t_majg = c.table("2.10", "Grupos de unos del mapa de la mayoría", GROUP_HEAD, cube_group_rows(cov, V3))
    maj_net_sop = Net("MAJ", "AND/OR", V3, [("p", "AND", ["A", "B"]), ("q", "AND", ["A", "C"]), ("r", "AND", ["B", "C"]), ("s", "OR", ["p", "q"]), ("Y", "OR", ["s", "r"])])
    maj_net_fact = Net("MAJ", "AND/OR", V3, [("p", "AND", ["A", "B"]), ("q", "OR", ["A", "B"]), ("r", "AND", ["C", "q"]), ("Y", "OR", ["p", "r"])])
    maj_net_sop.verify()
    maj_net_fact.verify()
    b_net_sop = c.block("netlist", maj_net_sop.text())
    b_net_fact = c.block("netlist", maj_net_fact.text())

    cost_can = sop_cost([format(m, "03b") for m in maj_ons])
    cost_min = sop_cost(cov)
    assert cost_can["total"] == 14 and cost_min["total"] == 5
    # minimo por busqueda exhaustiva (AND, OR, NOT) y (AND, OR, XOR, NOT)
    gts, mask = int_gates(["AND", "OR", "NOT"], 3)
    mc = min_gate_counts(var_codes(3), gts, mask, 4)
    min_andor = mc[maj_code]
    assert min_andor == 4
    gts2, mask2 = int_gates(["AND", "OR", "XOR", "NOT"], 3)
    mc2 = min_gate_counts(var_codes(3), gts2, mask2, 4)
    min_xor = mc2[maj_code]
    assert min_xor == 4
    t_majc = c.table(
        "2.11", "Coste en puertas de dos entradas (los NOT de una misma variable se comparten)",
        ["Forma", "Fórmula", "NOT", "AND", "OR", "Total"],
        [
            ["SOP canónica", sop_maj, cost_can["NOT"], cost_can["AND"], cost_can["OR"], cost_can["total"]],
            ["SOP mínima (Karnaugh)", min_maj, cost_min["NOT"], cost_min["AND"], cost_min["OR"], cost_min["total"]],
            ["Factorizada", fact_maj, 0, 2, 2, 4],
        ],
    )
    t_majv = c.table(
        "2.12", f"Las {N_FORMS} formas (más la definición) dan la misma salida en las {2**MAJ_N} filas",
        ["A", "B", "C", "MAJ(A,B,C)", f"{FORMS[0]} = {sop_maj}", f"{FORMS[1]} = {pos_maj}", f"{FORMS[2]} = {min_maj}", f"{FORMS[3]} = {fact_maj}"],
        [[*r, maj3(*r), maj3(*r), maj3(*r), maj3(*r), maj3(*r)] for r in rows_of(3)],
    )
    f_sop = c.block("formulas", f"MAJ(A,B,C) = {sop_maj}    # SOP canonica = suma de los minterms {ms(maj_ons)}")
    f_pos = c.block("formulas", f"MAJ(A,B,C) = {pos_maj}    # POS canonica = producto de los maxterms {ms(maj_off)}")
    f_min = c.block("formulas", f"MAJ(A,B,C) = {min_maj}    # SOP minima por Karnaugh")
    f_fact = c.block("formulas", f"MAJ(A,B,C) = {fact_maj}    # factorizada")

    # ---------------- ejemplo B: 4 variables ----------------
    f4_ons = [0, 1, 2, 5, 8, 9, 10]
    f4_code_fn = lambda a, b, c_, d: int((a << 3 | b << 2 | c_ << 1 | d) in f4_ons)  # noqa: E731
    pr4 = prime_implicants(f4_ons, 4)
    cov4 = min_cover(pr4, f4_ons)
    min_f4 = " + ".join(cube_term(cb, V4) for cb in cov4)
    k_f4 = c.block("kmap", kmap_text(4, V4, f"K-mapa de F4(A,B,C,D), unos en los minterms {ms(f4_ons)}", lambda m: str(int(m in f4_ons))))
    sop_f4 = " + ".join(minterm_str(m, V4) for m in f4_ons)
    t_f4g = c.table("2.13", "Grupos de unos del mapa de F4", GROUP_HEAD, cube_group_rows(cov4, V4))
    cost_can4 = sop_cost([format(m, "04b") for m in f4_ons])
    cost_min4 = sop_cost(cov4)
    t_f4c = c.table(
        "2.14", "Coste en puertas de dos entradas de F4",
        ["Forma", "Fórmula", "NOT", "AND", "OR", "Total"],
        [
            ["SOP canónica", sop_f4, cost_can4["NOT"], cost_can4["AND"], cost_can4["OR"], cost_can4["total"]],
            ["SOP mínima (Karnaugh)", min_f4, cost_min4["NOT"], cost_min4["AND"], cost_min4["OR"], cost_min4["total"]],
        ],
    )
    f4_val = lambda r: f4_code_fn(*r)  # noqa: E731
    t_f4v = c.table(
        "2.15", f"F4 y su forma mínima coinciden en las {2**4} filas",
        ["A", "B", "C", "D", "F4(A,B,C,D)", f"mín = {min_f4}"],
        [[*r, f4_val(r), f4_val(r)] for r in rows_of(4)],
    )
    f_f4 = c.block("formulas", f"F4(A,B,C,D) = {min_f4}    # SOP minima por Karnaugh")

    # ---------------- tablas de referencia ----------------
    ident = [
        ("Identidad", "A·1", "A"), ("Identidad", "A+0", "A"),
        ("Anulación", "A·0", "0"), ("Anulación", "A+1", "1"),
        ("Idempotencia", "A·A", "A"), ("Idempotencia", "A+A", "A"),
        ("Complemento", "A·¬A", "0"), ("Complemento", "A+¬A", "1"),
        ("Doble negación", "¬¬A", "A"),
        ("Conmutativa", "A·B", "B·A"), ("Conmutativa", "A+B", "B+A"),
        ("Asociativa", "(A·B)·C", "A·(B·C)"), ("Asociativa", "(A+B)+C", "A+(B+C)"),
        ("Absorción", "A+A·B", "A"), ("Absorción", "A·(A+B)", "A"),
        ("Distributiva", "A·(B+C)", "A·B+A·C"), ("Distributiva", "A+B·C", "(A+B)·(A+C)"),
        ("De Morgan", "¬(A·B)", "¬A+¬B"), ("De Morgan", "¬(A+B)", "¬A·¬B"),
    ]

    def nvars(*fs: str) -> int:
        return len({ch for f in fs for ch in f if ch in "ABC"})

    def evaluate(f: str, env: dict) -> int:
        # evaluador local (solo para generar la tabla); el check usa otro propio
        e = f.replace("¬", " not ").replace("·", " and ").replace("+", " or ")
        return int(bool(eval(e, {}, {k: bool(v) for k, v in env.items()})))

    ident_rows = []
    for name, l, r in ident:
        k = nvars(l, r)
        allok = True
        for row in rows_of(k):
            env = dict(zip("ABC"[:k], row))
            allok &= evaluate(l, env) == evaluate(r, env)
        assert allok, (l, r)
        ident_rows.append([name, l, r, k, 2**k, "sí"])
    t_ident = c.table(
        "2.16", "Identidades del álgebra de Boole, comprobadas fila a fila",
        ["Identidad", "Lado izquierdo", "Lado derecho", "Variables", "Combinaciones probadas", "¿Se cumple en todas?"],
        ident_rows,
    )
    code_rows = []
    for gname, gfn, formula in (
        ("AND", b_and, "A·B"), ("OR", b_or, "A+B"), ("XOR", b_xor, "A⊕B"),
        ("NAND", b_nand, "¬(A·B)"), ("NOR", b_nor, "¬(A+B)"), ("XNOR", b_xnor, "¬(A⊕B)"),
    ):
        code = func_code(gfn, 2)
        code_rows.append([gname, formula, gfn(1, 1), gfn(1, 0), gfn(0, 1), gfn(0, 0), f"{bits(code, 4)} = {hexs(code, 4)}"])
    t_codes = c.table(
        "2.17", "Cada puerta de dos entradas como un nibble (la salida de la fila 3 es el bit más alto)",
        ["Puerta", "Fórmula", "Fila 3 (A=1, B=1)", "Fila 2 (A=1, B=0)", "Fila 1 (A=0, B=1)", "Fila 0 (A=0, B=0)", "Código"],
        code_rows,
    )

    # ---------------- ejercicio ----------------
    let_ons = list(range(10, 16))
    k_let = c.block("kmap", kmap_text(4, V4, f"K-mapa de LETRA(A,B,C,D), unos en los minterms {ms(let_ons)}", lambda m: str(int(m in let_ons))))
    cov_l = min_cover(prime_implicants(let_ons, 4), let_ons)
    min_let = " + ".join(cube_term(cb, V4) for cb in cov_l)
    assert min_let == "A·B + A·C", min_let
    cost_l = sop_cost(cov_l)
    f_let = c.block("formulas", f"LETRA(A,B,C,D) = {min_let}    # SOP minima por Karnaugh\nLETRA(A,B,C,D) = A·(B+C)    # factorizada")
    t_letc = c.table(
        "2.18", "Coste de LETRA en puertas de dos entradas",
        ["Forma", "Fórmula", "NOT", "AND", "OR", "Total"],
        [
            ["SOP mínima (Karnaugh)", min_let, cost_l["NOT"], cost_l["AND"], cost_l["OR"], cost_l["total"]],
            ["Factorizada", "A·(B+C)", 0, 1, 1, 2],
        ],
    )
    let_hex_range = f"{c.given_tok(hexs(0xA, 4))} a {c.given_tok(hexs(0xF, 4))}"


    # ---------------- descripciones calculadas para la prosa ----------------
    def grp(cb: str) -> str:
        return "{" + ", ".join(str(m) for m in cube_minterms(cb)) + "}"

    def fixed_of(cb: str, names: list[str]) -> str:
        return ", ".join(f"{names[j]}={ch}" for j, ch in enumerate(cb) if ch != "-")

    cov_or = min_cover(prime_implicants(or_ons, 2), or_ons)
    cov_xor = min_cover(prime_implicants(xor_ons, 2), xor_ons)
    assert len(cov_or) == 2 and all(cube_literals(x) == 1 for x in cov_or)
    assert all(cube_literals(x) == 2 for x in prime_implicants(xor_ons, 2))
    or_groups = " y ".join(f"el grupo {grp(cb)} ({fixed_of(cb, ['A', 'B'])}) da {cube_term(cb, ['A', 'B'])}" for cb in cov_or)
    or_formula = " + ".join(cube_term(cb, ["A", "B"]) for cb in cov_or)
    xor_formula = " + ".join(cube_term(cb, ["A", "B"]) for cb in cov_xor)
    maj_sizes = {len(cube_minterms(cb)) for cb in cov}
    assert len(maj_sizes) == 1
    maj_size = maj_sizes.pop()
    maj_shared = set.intersection(*[set(cube_minterms(cb)) for cb in cov])
    assert len(maj_shared) == 1
    maj_shared_cell = maj_shared.pop()
    g2 = gray(2)

    def f4_group_line(i: int, cb: str) -> str:
        ms_ = cube_minterms(cb)
        rpos = sorted({g2.index(m >> 2) for m in ms_})
        cpos = sorted({g2.index(m & 3) for m in ms_})
        rows_txt = ", ".join(f"{g2[x]:02b}" for x in rpos)
        cols_txt = ", ".join(f"{g2[x]:02b}" for x in cpos)
        wr = 0 in rpos and 3 in rpos and len(rpos) < 4
        wc = 0 in cpos and 3 in cpos and len(cpos) < 4
        extra = ""
        if wr and wc:
            extra = ": las cuatro esquinas, que son vecinas porque el mapa es circular en vertical y en horizontal"
        elif wr:
            extra = ": junta la fila superior con la inferior (vecinas porque el mapa es circular)"
        elif wc:
            extra = ": junta la columna izquierda con la derecha (vecinas porque el mapa es circular)"
        return f"- Grupo {i} ({cube_term(cb, V4)}): {len(ms_)} casillas, filas AB = {rows_txt} y columnas CD = {cols_txt}{extra}."

    f4_group_lines = "\n".join(f4_group_line(i, cb) for i, cb in enumerate(cov4, 1))
    let_groups = " y ".join(f"{grp(cb)} con {fixed_of(cb, V4)}" for cb in cov_l)
    let_sizes = {len(cube_minterms(cb)) for cb in cov_l}
    assert len(let_sizes) == 1
    let_size = let_sizes.pop()

    # ---------------- casillas «no importa»: un contador de 0 a 5 con 3 bits ----------------
    FIN_BITS, FIN_LAST = 3, 5
    fin_ons = [FIN_LAST]
    fin_dc = list(range(FIN_LAST + 1, 2**FIN_BITS))  # los valores que el contador nunca alcanza
    fin_all = set(fin_ons) | set(fin_dc)
    cov_fin0 = min_cover(prime_implicants(fin_ons, FIN_BITS), fin_ons)       # sin usar las X
    cov_fin1 = min_cover_dc(fin_ons, fin_dc, FIN_BITS)                       # usando las X
    f_fin0 = " + ".join(cube_term(cb, V3) for cb in cov_fin0)
    f_fin1 = " + ".join(cube_term(cb, V3) for cb in cov_fin1)
    fin_cost0, fin_cost1 = sop_cost(cov_fin0), sop_cost(cov_fin1)
    fin_covered = sorted({m for cb in cov_fin1 for m in cube_minterms(cb)})
    fin_x_as_1 = [m for m in fin_dc if m in fin_covered]
    fin_x_as_0 = [m for m in fin_dc if m not in fin_covered]
    fin_group = cube_minterms(cov_fin1[0])
    # lo que el texto cuenta: una X se usa como 1 y la otra se queda en 0; con X sale mas barato
    assert len(cov_fin0) == len(cov_fin1) == 1 and f_fin0 == "A·¬B·C" and f_fin1 == "A·C"
    assert (fin_x_as_1, fin_x_as_0) == ([7], [6]) and fin_group == [5, 7]
    assert fin_cost0 == {"NOT": 1, "AND": 2, "OR": 0, "total": 3} and fin_cost1 == {"NOT": 0, "AND": 1, "OR": 0, "total": 1}
    # el 6 no se puede unir al 5: el unico rectangulo que los contiene es la fila A=1 y esa tiene un 0 (el 4)
    assert (FIN_LAST - 1) not in fin_all and set(cube_minterms("1--")) == {4, 5, 6, 7}
    fin_changing = [V3[i] for i, ch in enumerate(cov_fin1[0]) if ch == "-"]
    assert fin_changing == ["B"]
    fin_out = {m: int(m in fin_covered) for m in fin_dc}  # salida del circuito en las casillas X
    k_fin = c.block(
        "kmap",
        kmap_text(
            3, V3,
            f"K-mapa de FIN(A,B,C), unos en los minterms {ms(fin_ons)} y X en los minterms {ms(fin_dc)}",
            lambda m: "1" if m in fin_ons else ("X" if m in fin_dc else "0"),
        ),
    )
    f_fin = c.block(
        "formulas",
        f"FIN(A,B,C) = {f_fin0}    # SOP minima sin usar las X\nFIN(A,B,C) = {f_fin1}    # SOP minima usando las X",
    )
    BIG_N = 32  # numero de variables con el que la fuerza bruta deja de ser razonable

    md = f"""# 02 · Álgebra de Boole y puertas lógicas

## Objetivo

Aprender a describir un circuito lógico como una expresión con AND, OR y NOT, y a simplificarla sin cambiar lo que hace.  
Al terminar sabrás leer una tabla de verdad, demostrar identidades por fuerza bruta y minimizar con mapas de Karnaugh.

## Idea intuitiva

Un `if ($a && $b || !$c)` ya es álgebra de Boole. Cada variable vale 0 (falso) o 1 (verdadero) y un circuito lógico es una expresión así, hecha con puertas.

Como solo hay dos valores, una función de n variables tiene 2^n combinaciones de entrada: {2**1} con 1 variable, {2**2} con 2, {2**3} con 3 y {2**4} con 4. Mientras n sea pequeño, es tan poco que se puede **probar todo**: una tabla de verdad es un test exhaustivo. (2^n crece muy deprisa: con {BIG_N} variables ya son {2**BIG_N} filas, así que con circuitos grandes la fuerza bruta se combina con el álgebra de este documento.) Eso te da dos herramientas:

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

Una **puerta** es la versión en circuito de un operador. Las {N_GATES} que usaremos:

- **NOT** (1 entrada): invierte (Tabla 2.1).
- **AND**: 1 solo si todas las entradas valen 1.
- **OR**: 1 si al menos una entrada vale 1.
- **XOR**: 1 si las dos entradas son distintas.
- **NAND**: NOT de AND, es decir, 0 solo si las dos entradas valen 1.
- **NOR**: NOT de OR, es decir, 1 solo si las dos entradas valen 0.
- **XNOR**: NOT de XOR, es decir, 1 si las dos entradas son iguales.

{t_not}

{t_gates}

Para escribir circuitos en texto uso una **netlist**: una puerta por línea, `señal = PUERTA(entradas)`, y la última línea es la salida. Ejemplo: `p = AND(A, B)` y después `Y = NOT(p)` es lo mismo que NAND. En este documento y en el 03 una netlist no tiene tiempo: cada señal vale lo que dice su puerta en el mismo instante en que valen las entradas. Más adelante, en los documentos con memoria y relojes, la misma notación se usará con tiempo: cada puerta tardará un paso.

Cada puerta de dos entradas es una tabla de {2**2} filas, es decir, {2**2} bits de salida: un nibble (documento 01). La Tabla 2.17 los muestra. Una función de 3 entradas tiene {2**3} filas y su salida es un byte.

### Paso 3. Identidades

Una **identidad** es una igualdad que se cumple para todos los valores de las variables. Con 2 o 3 variables se demuestra por fuerza bruta: se calcula la tabla de verdad de cada lado y se comprueba que coinciden en todas las filas. Las que más vas a usar:

- **Idempotencia**: repetir una condición no cambia nada. A·A = A y A+A = A (Tabla 2.3).
- **Absorción**: si A ya basta, la rama «A y B» sobra. A + A·B = A y A·(A+B) = A (Tabla 2.4).
- **Distributiva**: A·(B+C) = A·B + A·C, como en el álgebra de siempre (Tabla 2.5). Y la segunda, que **no** existe con números: A + B·C = (A+B)·(A+C) (Tabla 2.6).

{t_idem}

{t_abs}

{t_dist1}

{t_dist2}

La Tabla 2.16 recoge estas y otras identidades (identidad, anulación, complemento, doble negación, conmutativa, asociativa, De Morgan) con el número de combinaciones comprobadas en cada una.

### Paso 4. Leyes de De Morgan

Negar una condición intercambia AND por OR y niega cada parte:

- ¬(A·B) = ¬A + ¬B
- ¬(A+B) = ¬A·¬B

En código: `!($a && $b)` equivale a `!$a || !$b`. Las dos leyes se demuestran por tabla exhaustiva (Tablas 2.7 y 2.8: la columna de la izquierda y la de la derecha de cada ley coinciden en las {2**2} filas).

{t_dm1}

{t_dm2}

Consecuencia que usaremos en el documento 03: un NAND es un OR de entradas negadas, y un NOR es un AND de entradas negadas.

### Paso 5. Formas canónicas (suma de productos y producto de sumas)

Toda función se puede escribir siempre de forma «mecánica», como suma de productos y como producto de sumas. Cada fila de la tabla de verdad es un número: la fila {ROW5} de una función de 3 variables es A={rows_of(3)[ROW5][0]}, B={rows_of(3)[ROW5][1]}, C={rows_of(3)[ROW5][2]} ({C(f"{ROW5} = {4 * rows_of(3)[ROW5][0]} + {2 * rows_of(3)[ROW5][1]} + {rows_of(3)[ROW5][2]}")}: esta es una suma aritmética de números, no un OR), con la primera variable como bit más alto, igual que en el documento 01.

- **Minterm** de una fila: un AND de todas las variables, cada una directa si vale 1 en esa fila y negada si vale 0. Vale 1 solo en esa fila. Se escribe mₖ, con k el número de fila.
- **Suma de productos (SOP)**: el OR de los minterms de las filas donde la función vale 1. Se abrevia Σm(...).
- **Maxterm** de una fila: un OR de todas las variables, cada una directa si vale 0 en esa fila y negada si vale 1. Vale 0 solo en esa fila. Se escribe Mₖ.
- **Producto de sumas (POS)**: el AND de los maxterms de las filas donde la función vale 0. Se abrevia ΠM(...).

Siempre existen y siempre son correctas, pero suelen ser grandes: por eso se simplifican. El ejemplo resuelto las calcula para la mayoría de 3.

### Paso 6. Mapas de Karnaugh

La simplificación se apoya en una identidad: dos productos que difieren en una sola variable se funden y esa variable desaparece. A·B + A·¬B = A·(B+¬B) = A·1 = A (distributiva, complemento e identidad).

Un **mapa de Karnaugh** coloca las filas de la tabla de modo que **casillas vecinas difieran exactamente en una variable**. Para eso los ejes no van en orden binario (00, 01, 10, 11) sino en **orden Gray** (00, 01, 11, 10): entre una etiqueta y la siguiente cambia un solo bit. Estos son los números de minterm en cada casilla para 2, 3 y 4 variables:

{kidx2}

{kidx3}

{kidx4}

Reglas para agrupar unos:

1. Los grupos son rectángulos de {2**0}, {2**1}, {2**2}, {2**3} o {2**4} casillas (potencias de 2) y todas sus casillas valen 1 (o X, ver más abajo las casillas «no importa»).
2. El mapa es circular: el borde izquierdo es vecino del derecho, el superior del inferior, y las cuatro esquinas son vecinas entre sí.
3. Los grupos pueden solaparse y conviene hacerlos tan grandes como se pueda.
4. Hay que cubrir todos los unos con el menor número de grupos (las casillas X no hace falta cubrirlas).
5. Cada grupo da un producto con las variables que **no cambian** dentro del grupo: directa si valen 1, negada si valen 0. La función es la suma de esos productos.

Con 2 variables, un caso en que se agrupa y otro en que no. OR: sus {len(or_ons)} unos se cubren con {len(cov_or)} grupos de {2**1} casillas.

{k_or}

Aquí {or_groups}, así que OR(A,B) = {or_formula}. XOR, en cambio, tiene los unos en diagonal, sin vecinos: no se agrupa nada y la forma mínima sigue siendo {xor_formula}.

{k_xor}

**Casillas «no importa» (X).** A veces hay combinaciones de entradas que no pueden ocurrir (un código que ningún estado usa) o en las que da igual qué salida se produzca. En el mapa se marcan con una **X** (en otras tablas se escriben con un guion, «-»). Una X es una libertad que te das al diseñar, parecida a la rama `default:` de un `switch` que sabes que nunca se ejecuta (ahí escribes lo que más te convenga): cada X se puede contar como 1, si así un grupo sale más grande, o como 0, si no hace falta, y nunca hay que cubrirla. Un grupo puede mezclar unos y X, pero nunca contener un 0.

Ejemplo con {FIN_BITS} variables. Un contador de {FIN_BITS} bits (A es el bit más alto) cuenta de 0 a {FIN_LAST} y vuelve a 0, así que sus valores {fin_dc[0]} y {fin_dc[1]} no aparecen nunca. FIN(A,B,C) vale 1 cuando el contador está en su último valor, {FIN_LAST}; en {fin_dc[0]} y {fin_dc[1]} da igual:

{k_fin}

Con los números de casilla de los mapas de arriba: sin usar las X, el único 1 está solo y la forma mínima es {f_fin0} ({fin_cost0['total']} puertas: {fin_cost0['NOT']} NOT y {fin_cost0['AND']} AND). Contando la casilla {fin_x_as_1[0]} como 1 se forma el grupo {grp(cov_fin1[0])} ({fixed_of(cov_fin1[0], V3)}): dentro de él cambia {fin_changing[0]}, que desaparece, y queda {f_fin1} (una sola puerta, un AND). La casilla {fin_x_as_0[0]} se cuenta como 0: para unirla al {FIN_LAST} habría que tomar toda la fila A=1, que incluye la casilla {FIN_LAST - 1}, un 0.

{f_fin}

Ojo con la decisión que has tomado: el circuito {f_fin1} da {fin_out[fin_x_as_1[0]]} en la casilla {fin_x_as_1[0]} y {fin_out[fin_x_as_0[0]]} en la {fin_x_as_0[0]}. Mientras el contador no llegue a esos valores, nadie lo nota; si llegara, el circuito daría eso. Por eso solo se marca X lo que de verdad no importa. Volverás a usar estas casillas en el documento 07.

### Paso 7. Contar puertas

Para comparar circuitos hace falta una unidad. En estos documentos: una puerta es un AND, OR o XOR de 2 entradas, o un NOT; un AND de 3 entradas cuenta como 2 puertas de 2 entradas; los NOT de una misma variable se comparten; las entradas no cuestan. Es una convención de conteo, no una ley.

## Ejemplo resuelto completo: la mayoría de 3 y una función de 4 variables

### Parte A. Mayoría de 3

La función MAJ(A,B,C) vale 1 cuando al menos {MAJ_K} de las {MAJ_N} entradas valen 1 (es una votación). Su tabla de verdad:

{t_maj}

Si lees la columna de salida de abajo arriba (la fila 0 es el bit más bajo) sale un byte: {C(chain(maj_code, 8))}.

**1. Formas canónicas.** Vale 1 en las filas {ms(maj_ons)} y 0 en las filas {ms(maj_off)}:

{f_sop}

{f_pos}

**2. Karnaugh.** Mapa de 3 variables y sus grupos:

{k_maj}

{t_majg}

{len(cov)} grupos de {maj_size} casillas que comparten la casilla {maj_shared_cell} (se solapan, y está permitido). La función mínima en suma de productos:

{f_min}

**3. Circuitos.** La SOP mínima necesita {maj_net_sop.n_gates()} puertas; si además sacas factor común (A·B + C·(A+B)), bastan {maj_net_fact.n_gates()}:

{b_net_sop}

{b_net_fact}

{f_fact}

**4. Contar puertas.**

{t_majc}

La forma canónica cuesta {cost_can["total"]} puertas y la mínima de Karnaugh {cost_min["total"]}. La factorizada baja a {maj_net_fact.n_gates()}: Karnaugh minimiza la suma de productos, que no siempre es el circuito más pequeño. ¿Se puede bajar de {maj_net_fact.n_gates()}? El script de la serie (documento 01) probó **todos** los circuitos de AND, OR y NOT con {min_andor - 1} puertas o menos: ninguno calcula MAJ. Con {min_andor} sí (los de arriba), así que {min_andor} es el mínimo con esas puertas. Añadiendo XOR a las puertas permitidas el mínimo sigue siendo {min_xor}.

**5. Verificación.** Las {N_FORMS} formas dan la misma salida que la definición en todas las filas:

{t_majv}

### Parte B. Una función de 4 variables

Sea F4(A,B,C,D) con unos en los minterms {ms(f4_ons)}. Mapa de 4 variables:

{k_f4}

Los grupos muestran las reglas «raras» del mapa:

{t_f4g}

{f4_group_lines}

{f_f4}

{t_f4c}

De {cost_can4["total"]} puertas de la forma canónica a {cost_min4["total"]}. Y la verificación en las {2**4} filas:

{t_f4v}

## Tablas

Las demás tablas están en el desarrollo. Aquí quedan las de consulta rápida.

{t_ident}

{t_codes}

## Mini-ejercicio

Un nibble ABCD (A es el bit más alto, como en el documento 01) es un dígito hexadecimal «letra» cuando vale de {let_hex_range}. Define LETRA(A,B,C,D) = 1 solo en esos casos.

1. ¿En qué minterms vale 1?
2. Simplifícala con un mapa de Karnaugh de 4 variables.
3. ¿Se puede factorizar para usar menos puertas? ¿Cuántas?

---

**Solución**

1. En los minterms {ms(let_ons)} (los números {let_hex_range}, ver Tabla 1.5 del documento 01).
2. Mapa y grupos:

{k_let}

{len(cov_l)} grupos de {let_size} casillas ({let_groups}) y la forma mínima en suma de productos es:

{f_let}

3. Sí: sacando A factor común queda A·(B+C), con 2 puertas en vez de {cost_l["total"]}.

{t_letc}

## En Redstone / en Satisfactory

- **En Redstone**, según la Minecraft Wiki, una antorcha invierte la señal que recibe en el bloque donde está colocada ([Redstone Torch]({URL_TORCH})): hace de NOT.
- La misma wiki trata la señal como encendida cuando es mayor que 0 y describe una puerta AND como la que solo da señal si las dos entradas son mayores que 0 ([Redstone circuit]({URL_CIRCUIT})): lógica booleana de 1 bit.
- **En Satisfactory** qué puertas se ofrecerán y cuántas entradas tendrán es una decisión abierta: el mod aún no está diseñado. Las identidades y la minimización de este documento valen para cualquier elección.

## Resumen

- Una función de n variables tiene 2^n filas, así que, mientras n sea pequeño, cualquier igualdad entre circuitos se demuestra mirando todas las filas.
- Las {N_GATES} puertas (NOT, AND, OR, XOR, NAND, NOR, XNOR) se escriben con ¬, ·, +, ⊕ y se describen en texto con netlists.
- Idempotencia, absorción, distributiva y De Morgan permiten reescribir un circuito sin cambiarlo; NAND es un OR de entradas negadas y NOR un AND de entradas negadas.
- Toda función tiene una suma de productos y un producto de sumas canónicos, y el mapa de Karnaugh (ejes en orden Gray, grupos de potencias de 2, borde circular, casillas X que valen 0 o 1 según convenga) los simplifica.
- Minimizar la SOP no siempre da el circuito más pequeño: la mayoría de 3 pasa de {cost_can["total"]} a {cost_min["total"]} puertas con Karnaugh y a {maj_net_fact.n_gates()} al factorizar.
"""
    return md, c


# ==========================================================================
# DOCUMENTO 03
# ==========================================================================
URL_LOGIC = "https://minecraft.wiki/w/Redstone_circuits/Logic"

F2_NAMES = {
    0b0000: ("constante 0", "0"), 0b0001: ("NOR", "¬(A+B)"), 0b0010: ("solo B sin A", "¬A·B"),
    0b0011: ("NOT A", "¬A"), 0b0100: ("solo A sin B", "A·¬B"), 0b0101: ("NOT B", "¬B"),
    0b0110: ("XOR", "A⊕B"), 0b0111: ("NAND", "¬(A·B)"), 0b1000: ("AND", "A·B"),
    0b1001: ("XNOR", "¬(A⊕B)"), 0b1010: ("B", "B"), 0b1011: ("A implica B", "¬A+B"),
    0b1100: ("A", "A"), 0b1101: ("B implica A", "A+¬B"), 0b1110: ("OR", "A+B"),
    0b1111: ("constante 1", "1"),
}


def py_formula_code(f: str) -> int:
    """Codigo (2 variables) de una formula; solo para validar F2_NAMES al generar."""
    e = f.replace("¬", " not ").replace("·", " and ").replace("+", " or ")
    e = e.replace("⊕", " ^ ")
    code = 0
    for k, (a, b) in enumerate(rows_of(2)):
        code |= int(bool(eval(e, {}, {"A": bool(a), "B": bool(b)}))) << k
    return code


def build_nets() -> dict[tuple[str, str], Net]:
    AB, A1 = ["A", "B"], ["A"]
    n: dict[tuple[str, str], Net] = {}
    # ---- solo NAND ----
    n["NAND", "NOT"] = Net("NOT", "NAND", A1, [("Y", "NAND", ["A", "A"])])
    n["NAND", "AND"] = Net("AND", "NAND", AB, [("t", "NAND", ["A", "B"]), ("Y", "NAND", ["t", "t"])])
    n["NAND", "OR"] = Net("OR", "NAND", AB, [("na", "NAND", ["A", "A"]), ("nb", "NAND", ["B", "B"]), ("Y", "NAND", ["na", "nb"])])
    n["NAND", "NOR"] = Net("NOR", "NAND", AB, [("na", "NAND", ["A", "A"]), ("nb", "NAND", ["B", "B"]), ("o", "NAND", ["na", "nb"]), ("Y", "NAND", ["o", "o"])])
    n["NAND", "XOR"] = Net("XOR", "NAND", AB, [("t", "NAND", ["A", "B"]), ("u", "NAND", ["A", "t"]), ("v", "NAND", ["B", "t"]), ("Y", "NAND", ["u", "v"])])
    n["NAND", "XNOR"] = Net("XNOR", "NAND", AB, [("t", "NAND", ["A", "B"]), ("u", "NAND", ["A", "t"]), ("v", "NAND", ["B", "t"]), ("x", "NAND", ["u", "v"]), ("Y", "NAND", ["x", "x"])])
    # ---- solo NOR ----
    n["NOR", "NOT"] = Net("NOT", "NOR", A1, [("Y", "NOR", ["A", "A"])])
    n["NOR", "OR"] = Net("OR", "NOR", AB, [("t", "NOR", ["A", "B"]), ("Y", "NOR", ["t", "t"])])
    n["NOR", "AND"] = Net("AND", "NOR", AB, [("na", "NOR", ["A", "A"]), ("nb", "NOR", ["B", "B"]), ("Y", "NOR", ["na", "nb"])])
    n["NOR", "NAND"] = Net("NAND", "NOR", AB, [("na", "NOR", ["A", "A"]), ("nb", "NOR", ["B", "B"]), ("a", "NOR", ["na", "nb"]), ("Y", "NOR", ["a", "a"])])
    n["NOR", "XNOR"] = Net("XNOR", "NOR", AB, [("t", "NOR", ["A", "B"]), ("u", "NOR", ["A", "t"]), ("v", "NOR", ["B", "t"]), ("Y", "NOR", ["u", "v"])])
    n["NOR", "XOR"] = Net("XOR", "NOR", AB, [("t", "NOR", ["A", "B"]), ("u", "NOR", ["A", "t"]), ("v", "NOR", ["B", "t"]), ("x", "NOR", ["u", "v"]), ("Y", "NOR", ["x", "x"])])
    for net in n.values():
        net.verify()
    return n


def build_doc03() -> tuple[str, Ctx]:
    c = Ctx()
    C = c.claim
    nets = build_nets()
    NAND_ORDER = ["NOT", "AND", "OR", "NOR", "XOR", "XNOR"]
    NOR_ORDER = ["NOT", "OR", "AND", "NAND", "XNOR", "XOR"]

    # ---- busquedas exhaustivas (BFS) y cierres ----
    A2, B2 = var_codes(2)
    gN, m2 = int_gates(["NAND"], 2)
    gR, _ = int_gates(["NOR"], 2)
    min_nand = min_gate_counts([A2, B2], gN, m2, 5)
    min_nor = min_gate_counts([A2, B2], gR, m2, 5)
    assert len(min_nand) == 16 and len(min_nor) == 16
    for tgt in NAND_ORDER:
        k, fn = TARGETS[tgt]
        assert min_nand[func_code(fn, k) if k == 2 else 0b0011] == nets["NAND", tgt].n_gates(), tgt
    for tgt in NOR_ORDER:
        k, fn = TARGETS[tgt]
        assert min_nor[func_code(fn, k) if k == 2 else 0b0011] == nets["NOR", tgt].n_gates(), tgt
    max_search = max(max(min_nand.values()), max(min_nor.values()))

    # lo mas caro de cada base es una sola puerta (el texto no puede decir otra cosa)
    cost_n = {t: nets["NAND", t].n_gates() for t in NAND_ORDER}
    cost_r = {t: nets["NOR", t].n_gates() for t in NOR_ORDER}
    depth_n = {t: nets["NAND", t].depth() for t in NAND_ORDER}
    for table_, top in ((cost_n, "XNOR"), (cost_r, "XOR"), (depth_n, "XNOR")):
        assert max(table_, key=table_.get) == top and list(table_.values()).count(max(table_.values())) == 1, table_
    assert cost_n["XOR"] == cost_n["NOR"] and cost_r["XNOR"] == cost_r["NAND"]

    # ---- tablas de verdad ----
    t_not = truth_table(c, "3.1", "NAND con las dos entradas unidas es un NOT", ["A"], [("NAND(A,A)", lambda a: b_nand(a, a)), ("¬A", b_not)])
    t_or = truth_table(
        c, "3.2", f"OR con {nets['NAND', 'OR'].n_gates()} NAND (De Morgan: A+B = ¬(¬A·¬B))", ["A", "B"],
        [
            ("na = NAND(A,A)", lambda a, b: b_nand(a, a)), ("nb = NAND(B,B)", lambda a, b: b_nand(b, b)),
            ("Y = NAND(na,nb)", lambda a, b: b_nand(b_nand(a, a), b_nand(b, b))), ("A+B", b_or),
        ],
    )
    t_xor = truth_table(
        c, "3.3", f"Traza de XOR con {nets['NAND', 'XOR'].n_gates()} NAND (todas las señales internas)", ["A", "B"],
        [
            ("t = NAND(A,B)", b_nand), ("u = NAND(A,t)", lambda a, b: b_nand(a, b_nand(a, b))), ("¬A+B", lambda a, b: (1 - a) | b),
            ("v = NAND(B,t)", lambda a, b: b_nand(b, b_nand(a, b))), ("A+¬B", lambda a, b: a | (1 - b)),
            ("Y = NAND(u,v)", lambda a, b: b_nand(b_nand(a, b_nand(a, b)), b_nand(b, b_nand(a, b)))), ("A⊕B", b_xor),
        ],
    )

    def built_table(tid: str, caption: str, base: str, order: list[str]) -> str:
        cols = []
        for tgt in order:
            net = nets[base, tgt]
            hdr = f"{tgt} ({net.n_gates()} {base})"
            if tgt == "NOT":
                cols.append((hdr, lambda a, b, net=net: net.value({"A": a})))
            else:
                cols.append((hdr, lambda a, b, net=net: net.value({"A": a, "B": b})))
        return truth_table(c, tid, caption, ["A", "B"], cols)

    t_built_nand = built_table("3.4", "Salidas de las construcciones solo con NAND", "NAND", NAND_ORDER)
    t_built_nor = built_table("3.5", "Salidas de las construcciones solo con NOR", "NOR", NOR_ORDER)
    native = {"NOT": 1, "AND": 2, "OR": 3, "NOR": 4, "XOR": 5, "XNOR": 6}  # solo para claridad de orden
    del native

    summary_rows = []
    for base, order, mins in (("NAND", NAND_ORDER, min_nand), ("NOR", NOR_ORDER, min_nor)):
        for tgt in order:
            net = nets[base, tgt]
            k, fn = TARGETS[tgt]
            code = func_code(fn, k)
            tt_code = 0b0011 if tgt == "NOT" else code
            summary_rows.append([base, tgt, net.n_gates(), net.depth(), 2**k, "sí", mins[tt_code]])
    t_summary = c.table(
        "3.6", "Resumen de construcciones: puertas, capas y comprobación exhaustiva",
        ["Base", "Objetivo", "Puertas", "Capas", "Filas comprobadas", "Coincide con la puerta nativa", "Mínimo (búsqueda exhaustiva)"],
        summary_rows,
    )

    t_nfun = c.table(
        "3.7", "Cuántas funciones distintas hay de n variables",
        ["Variables (n)", "Filas (2^n)", "Funciones distintas (2^(2^n))"],
        [[n_, 2**n_, 2 ** (2**n_)] for n_ in range(1, 5)],
    )

    for code, (_, f) in F2_NAMES.items():
        assert py_formula_code(f) == code, (code, f)
    t_16 = c.table(
        "3.8", "Las 16 funciones de dos variables: código, fórmula y mínimo de puertas con NAND y con NOR",
        ["Código", "Función", "Fórmula", "Mín. NAND", "Mín. NOR"],
        [[f"{bits(code, 4)} = {hexs(code, 4)}", F2_NAMES[code][0], F2_NAMES[code][1], min_nand[code], min_nor[code]] for code in range(16)],
    )

    clo_sets = [["AND", "OR"], ["XOR"], ["AND", "XOR"], ["AND", "OR", "NOT"], ["NAND"], ["NOR"]]
    clo_rows = []
    for gs in clo_sets:
        c2, c3 = closure_size(gs, 2), closure_size(gs, 3)
        clo_rows.append([", ".join(gs), c2, 2 ** (2**2), c3, 2 ** (2**3), "sí" if (c2, c3) == (16, 256) else "no"])
    c2, c3 = closure_size(["AND", "XOR"], 2, (1,)), closure_size(["AND", "XOR"], 3, (1,))
    clo_rows.insert(3, ["AND, XOR y la constante 1", c2, 16, c3, 256, "sí" if (c2, c3) == (16, 256) else "no"])
    t_clo = c.table(
        "3.9", "Funciones que se pueden construir con cada conjunto de puertas (sin constantes salvo que se indique)",
        ["Puertas disponibles", "Alcanzables con 2 variables", "Total (2 variables)", "Alcanzables con 3 variables", "Total (3 variables)", "¿Completo?"],
        clo_rows,
    )
    clo = {tuple(r[0].split(", ")): (r[1], r[3]) for r in clo_rows}
    and_or2, and_or3 = clo[("AND", "OR")]
    xor_only2, xor_only3 = clo[("XOR",)]
    and_xor2, and_xor3 = clo[("AND", "XOR")]

    # ---- bloques netlist ----
    def nl(base: str, tgt: str) -> str:
        return c.block("netlist", nets[base, tgt].text())

    blk_nand = {t: nl("NAND", t) for t in NAND_ORDER}
    blk_nor = {t: nl("NOR", t) for t in NOR_ORDER}

    xor_rows = net_signals(nets["NAND", "XOR"])
    xor_lines = "\n".join(
        f"- A={a}, B={b}: t={xor_rows['t'][k]}, u={xor_rows['u'][k]}, v={xor_rows['v'][k]}, Y={xor_rows['Y'][k]}"
        for k, (a, b) in enumerate(rows_of(2))
    )

    # ---- ejercicio: XNOR ----
    t_xnor = truth_table(
        c, "3.10", f"Traza de XNOR con {nets['NAND', 'XNOR'].n_gates()} NAND", ["A", "B"],
        [
            ("t = NAND(A,B)", b_nand), ("u = NAND(A,t)", lambda a, b: b_nand(a, b_nand(a, b))),
            ("v = NAND(B,t)", lambda a, b: b_nand(b, b_nand(a, b))),
            ("x = NAND(u,v)", lambda a, b: b_xor(a, b)), ("Y = NAND(x,x)", b_xnor), ("¬(A⊕B)", b_xnor),
        ],
    )

    x_nand, x_nor = nets["NAND", "XOR"], nets["NOR", "XOR"]
    xn_nand, xn_nor = nets["NAND", "XNOR"], nets["NOR", "XNOR"]
    gates_not = nets["NAND", "NOT"].n_gates()
    ratio_xor = x_nand.n_gates()

    md = f"""# 03 · Universalidad de NAND (y NOR)

## Objetivo

Construir NOT, AND, OR, NOR y XOR usando solo NAND (y después lo mismo con NOR), y comprobar cada construcción mirando todas las filas.  
Entender qué significa que una puerta sea «universal» y por qué, aun así, conviene tener varias puertas disponibles.

## Idea intuitiva

Imagina que solo tienes un tipo de pieza de Lego y aun así puedes montar cualquier figura. Esa pieza, en lógica, es la puerta NAND («NOT AND»). Con NAND se hacen NOT, AND, OR y todas las demás; con NOR también. Se las llama puertas **universales**.

Pero universal no significa barato: lo que con una puerta XOR es una sola pieza, con NAND son {x_nand.n_gates()}. La pregunta de diseño no es «¿se puede?» (sí), sino «¿cuánto cuesta y qué conviene ofrecer?».

## Explicación paso a paso

### Paso 1. Las reglas del juego y NAND como NOT

NAND(A,B) = ¬(A·B): da 0 solo cuando las dos entradas valen 1 (Tablas 2.2 y 2.17 del documento 02; el código de la 2.17 es {C(f"{bits(func_code(b_nand, 2), 4)} = {hexs(func_code(b_nand, 2), 4)}")}). Las reglas de construcción son las de la netlist del documento 02 (que no tiene tiempo: cada señal vale lo que dice su puerta en cuanto valen las entradas):

- Cada puerta toma sus entradas de las entradas del circuito o de la salida de una puerta anterior.
- Una señal puede alimentar a varias puertas, y las dos entradas de una puerta pueden ser la misma señal.
- No hay constantes 0 ni 1 disponibles: solo las entradas.
- El coste es el número de puertas (líneas de la netlist), como en el documento 02.

Primer truco: si unes las dos entradas de un NAND, NAND(A,A) = ¬(A·A) = ¬A, por idempotencia (documento 02). Es un NOT con {gates_not} puerta:

{t_not}

{blk_nand["NOT"]}

### Paso 2. AND con NAND

AND es el NOT de un NAND: ¬¬(A·B) = A·B. Primero NAND(A,B) y después invertir ese resultado con el truco anterior: {nets["NAND", "AND"].n_gates()} puertas.

{blk_nand["AND"]}

### Paso 3. OR con NAND (De Morgan)

Por De Morgan (documento 02), A+B = ¬(¬A·¬B), que es exactamente un NAND de las entradas negadas. Cada entrada negada cuesta una puerta (el NOT del Paso 1): {nets["NAND", "OR"].n_gates()} puertas.

{blk_nand["OR"]}

{t_or}

### Paso 4. NOR con NAND

NOR es el NOT de OR: se añade un NOT al circuito anterior, {nets["NAND", "NOR"].n_gates()} puertas.

{blk_nand["NOR"]}

### Paso 5. XOR con NAND

XOR cuesta {x_nand.n_gates()} NAND, tanto como el NOR construido con NAND en el Paso 4, y está desarrollado entero en el ejemplo resuelto de más abajo. La más cara con NAND es XNOR: con un NOT más sobre el XOR salen {xn_nand.n_gates()} puertas, y es el mini-ejercicio. Con NOR la situación se invierte (Paso 6).

### Paso 6. Lo mismo con NOR (la versión «espejo»)

Con NOR todo es simétrico, porque De Morgan intercambia AND y OR: NOR(A,A) = ¬A; OR es un NOT de NOR; y AND se hace con las entradas negadas, A·B = ¬(¬A+¬B). Las construcciones:

{blk_nor["NOT"]}

{blk_nor["OR"]}

{blk_nor["AND"]}

{blk_nor["NAND"]}

La estructura de XOR con NAND de {x_nand.n_gates()} puertas, escrita con NOR, calcula **XNOR** (no XOR): así que aquí XNOR cuesta {nets["NOR", "XNOR"].n_gates()} puertas y XOR {nets["NOR", "XOR"].n_gates()}, al revés que con NAND.

{blk_nor["XNOR"]}

{blk_nor["XOR"]}

### Paso 7. Contar puertas y capas

La Tabla 3.6 resume todo. **Capas** es la profundidad combinacional: el mayor número de puertas encadenadas entre una entrada y la salida (por ejemplo, en XOR con NAND la ruta A, t, u, Y pasa por {x_nand.depth()} puertas). Es una medida de forma del circuito; aún no hablamos de tiempo.

La columna «Mínimo» no es una opinión: el script de la serie (documento 01) probó **todos** los circuitos posibles de cada base con hasta {max_search} puertas, reutilizando señales y repitiendo entradas, y anotó la primera cantidad de puertas con la que aparece cada función. Las construcciones de arriba coinciden con ese mínimo en los {len(summary_rows)} casos. La Tabla 3.8 da el mismo dato para las {2**(2**2)} funciones de dos variables.

### Paso 8. Completitud funcional (enunciado informal)

Un conjunto de puertas es **funcionalmente completo** si, conectándolas como quieras (reutilizando señales, duplicando entradas), puedes construir **cualquier** función booleana, de cualquier número de variables. Cuántas funciones hay lo da la Tabla 3.7: {2**(2**2)} de 2 variables, {2**(2**3)} de 3, {2**(2**4)} de 4.

- {{AND, OR, NOT}} es completo: en el documento 02 vimos que toda función tiene una suma de productos canónica, y esa forma usa solo AND, OR y NOT. (La constante 0 no tiene ningún minterm, así que su suma de productos queda vacía y se escribe aparte como A·¬A; la constante 1 sí es la suma de todos los minterms, pero A+¬A es mucho más corta. Con NAND cuestan {min_nand[0b0000]} y {min_nand[0b1111]} puertas, Tabla 3.8.)
- NAND construye NOT (Paso 1), AND (Paso 2) y OR (Paso 3), así que construye todo lo que se hace con {{AND, OR, NOT}}: **NAND es completo**. Con NOR, igual (Paso 6).
- No todo conjunto lo es. Con solo AND y OR (sin NOT) se alcanzan {and_or2} de las {2**(2**2)} funciones de 2 variables y {and_or3} de las {2**(2**3)} de 3 (Tabla 3.9): son *monótonas*, es decir, si subes una entrada de 0 a 1 la salida nunca baja, y un NOT hace justo lo contrario. Con solo XOR, {xor_only2} de las {2**(2**2)} y {xor_only3} de las {2**(2**3)}. AND y XOR juntos, {and_xor2} y {and_xor3}: les falta una constante 1 (con ella, A⊕1 = ¬A, y ya hay NOT; la Tabla 3.9 lo confirma).

Qué ha comprobado el script y qué no: para NAND y NOR ha construido por cierre todas las funciones de 2 y 3 variables (Tabla 3.9). Para un número cualquiera de variables, lo que lo garantiza es el argumento de la suma de productos de arriba, no una prueba por fuerza bruta.

### Paso 9. Por qué importa para un mod (o cualquier sistema de puertas)

- **Un solo primitivo basta.** Si un sistema ofrece NAND (o NOR), ya es completo: cualquier circuito lógico se puede construir, y la definición mínima del sistema es pequeña.
- **Pero tiene un coste.** Con solo NAND, OR cuesta {nets["NAND", "OR"].n_gates()} puertas en vez de 1, NOR {nets["NAND", "NOR"].n_gates()}, XOR {x_nand.n_gates()} y XNOR {xn_nand.n_gates()}, y esta última llega a {xn_nand.depth()} capas (Tabla 3.6). Más piezas, más cableado, circuitos más difíciles de leer.
- **Por eso conviene ofrecer varios.** Con NOT, AND, OR, XOR (y NAND o NOR) los circuitos salen más pequeños y legibles, y la universalidad de NAND/NOR sigue garantizando que no hay nada que no se pueda construir. Cuál sea el conjunto exacto es una decisión de diseño que aún no está tomada.

## Ejemplo resuelto completo: XOR con {x_nand.n_gates()} NAND, fila a fila

**1. Objetivo.** A⊕B vale 1 si las entradas son distintas. Solo tenemos NAND.

**2. Idea.** Sea t = NAND(A,B) = ¬(A·B) = ¬A + ¬B. Usamos t para comparar cada entrada con la otra:

- u = NAND(A,t) = ¬(A·t) = ¬A + ¬t = ¬A + A·B = ¬A + B (la última igualdad es la distributiva (2) del documento 02: ¬A + A·B = (¬A + A)·(¬A + B) = ¬A + B).
- v = NAND(B,t) = ¬B + A, por simetría.
- Y = NAND(u,v) = ¬(u·v) = ¬u + ¬v (De Morgan) = A·¬B + ¬A·B = A⊕B.

**3. Netlist.**

{blk_nand["XOR"]}

**4. Fila a fila.** Calculado con la netlist:

{xor_lines}

**5. Todas las señales.** Las columnas u y ¬A+B coinciden, y v y A+¬B también, como dice el punto 2 de este ejemplo:

{t_xor}

**6. Coste.** {x_nand.n_gates()} puertas y {x_nand.depth()} capas, frente a 1 puerta con un XOR nativo.

## Tablas

Salidas de todas las construcciones. Cada columna lleva el número de puertas entre paréntesis y debe coincidir con la puerta del mismo nombre (Tabla 2.2 del documento 02).

{t_built_nand}

{t_built_nor}

{t_summary}

{t_nfun}

{t_16}

{t_clo}

## Mini-ejercicio

Construye XNOR(A,B) usando solo NAND.

1. ¿Cuántas puertas y cuántas capas necesitas?
2. Compruébalo con una tabla de verdad.
3. ¿Y usando solo NOR?

---

**Solución**

1. XNOR es el NOT de XOR: partes de las {x_nand.n_gates()} puertas del XOR y añades un NAND con las dos entradas unidas. Total: {xn_nand.n_gates()} puertas y {xn_nand.depth()} capas. La búsqueda exhaustiva confirma que con {xn_nand.n_gates() - 1} NAND o menos no existe (Tabla 3.8).

{blk_nand["XNOR"]}

2. Traza (las columnas Y y ¬(A⊕B) coinciden en las {2**2} filas):

{t_xnor}

3. Con NOR es más barato: la misma estructura de {xn_nor.n_gates()} puertas ya da XNOR, sin NOT final (Paso 6). Con NAND el XOR es la versión corta; con NOR lo es el XNOR.

## En Redstone / en Satisfactory

- **En Redstone**, según la Minecraft Wiki, un NOR se implementa con una antorcha con dos o más entradas, una antorcha con una sola entrada es un NOT, y la propia wiki afirma que todas las puertas lógicas se pueden construir con combinaciones de NOR (y lo mismo con NAND) ([Redstone circuits/Logic]({URL_LOGIC})).
- **En Satisfactory** cuántos tipos de puerta ofrecer (uno solo, o varios) es una decisión abierta: el mod aún no está diseñado. Lo que sí queda establecido es el precio: con solo NAND, XOR cuesta {x_nand.n_gates()} piezas y XNOR {xn_nand.n_gates()}; con solo NOR (la puerta nativa de Redstone: la antorcha con dos entradas), XOR cuesta {x_nor.n_gates()}, XNOR {xn_nor.n_gates()}, OR {nets["NOR", "OR"].n_gates()} y AND {nets["NOR", "AND"].n_gates()}.

## Resumen

- Con NAND se construyen NOT ({nets["NAND", "NOT"].n_gates()} puerta), AND ({nets["NAND", "AND"].n_gates()}), OR ({nets["NAND", "OR"].n_gates()}), NOR ({nets["NAND", "NOR"].n_gates()}) y XOR ({x_nand.n_gates()}); con NOR, NOT ({nets["NOR", "NOT"].n_gates()}), OR ({nets["NOR", "OR"].n_gates()}), AND ({nets["NOR", "AND"].n_gates()}), NAND ({nets["NOR", "NAND"].n_gates()}) y XOR ({x_nor.n_gates()}).
- Cada construcción se ha comprobado en todas las filas, y los conteos son mínimos (búsqueda exhaustiva sobre todas las funciones de dos variables).
- Un conjunto de puertas es funcionalmente completo si construye cualquier función: NAND y NOR lo son; AND+OR sin NOT, no.
- La universalidad viene de la suma de productos del documento 02: AND, OR y NOT bastan, y NAND construye los tres.
- Un solo primitivo basta, pero conviene ofrecer varios porque cada puerta que falta se paga en piezas y en capas.
"""
    return md, c


# ==========================================================================
# Escritura
# ==========================================================================
DOCS = {
    "01-numeracion-binaria-hex.md": build_doc01,
    "02-algebra-de-boole-y-puertas.md": build_doc02,
    "03-universalidad-nand.md": build_doc03,
}


def main() -> None:
    for name, fn in DOCS.items():
        md, _ = fn()
        (HERE / name).write_text(md, encoding="utf-8")
        print(f"escrito {name}: {len(md.splitlines())} lineas")


if __name__ == "__main__":
    main()
