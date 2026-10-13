# Guía de lectura de la teoría

Siete documentos en español que enseñan **lógica digital desde cero**: contar en binario, combinar puertas, guardar un bit, sumar, medir el tiempo y, al final, diseñar una máquina de estados con puertas y flip-flops. Son la base conceptual de un mod de lógica «estilo Redstone» para Satisfactory (ver [Inicio](../index.md)), pero se pueden leer solos: no hace falta conocer el mod, Satisfactory ni Minecraft.

## Para quién es

Para una persona que programa (PHP, Python, SQL...) y **no ha estudiado electrónica**. Se supone que sabes programar y que has visto operadores como `&`, `|` o `^`; no se supone nada más. Cada idea nueva se ancla en algo que ya conoces: el cuentakilómetros de un coche antiguo para contar en binario, `if ($a && $b || !$c)` para el álgebra de Boole, una función pura frente a una variable que se reasigna para la memoria, la rama `default:` de un `switch` para las casillas «no importa».

Qué **no** es esta serie:

- **No es la especificación del mod.** Enseña la teoría con un modelo didáctico. Lo que hará exactamente el simulador está en la [especificación del núcleo](../spec/01-semantica.md), que tiene su propia notación (ticks, `F` para 15) y cita estos documentos por su número de Paso y de Tabla.
- **No es una guía de Satisfactory.** Las secciones «En Redstone / en Satisfactory» de cada documento citan la Minecraft Wiki con su enlace; de Satisfactory solo dejan preguntas abiertas, porque el mod aún no está diseñado.

> **Aviso: modelo didáctico.** Desde el documento 04 el tiempo se mide en **pasos** de un modelo de retardo unitario (cada puerta tarda un paso). Un paso no son segundos ni ticks del juego. Los periodos y tiempos de asentamiento que verás dependen de ese modelo y, en el 06, de tratar el flip-flop como una pieza ideal de un paso; el propio 06 compara esa idealización con el flip-flop construido con puertas del 04.

## Cómo es cada documento

Los siete tienen las mismas ocho secciones, en este orden: Objetivo, Idea intuitiva, Explicación paso a paso, Ejemplo resuelto completo, Tablas, Mini-ejercicio, En Redstone / en Satisfactory y Resumen. Las tablas se numeran `N.M` (la Tabla 4.7 es la séptima del documento 04). Intenta el mini-ejercicio antes de mirar la solución: está detrás de una línea `---`.

## Orden de lectura

El orden de los números es un orden de lectura válido: cada documento solo necesita los anteriores. Las menciones a un documento posterior («esto llega en el 04») son avisos, no requisitos.

| Documento | Qué aprendes | Necesita | Lo retoman |
| --- | --- | --- | --- |
| [01 · Numeración binaria y hexadecimal](01-numeracion-binaria-hex.md) | Bit, nibble y byte; binario, decimal y hexadecimal; operaciones bit a bit, desplazamientos y máscaras con los mismos operadores de PHP y Python; la notación de la serie; por qué la señal 0 a 15 de Redstone tiene la información de un nibble | nada | todos (la notación); 02 (el orden de los bits) |
| [02 · Álgebra de Boole y puertas](02-algebra-de-boole-y-puertas.md) | Las puertas NOT, AND, OR, XOR, NAND, NOR y XNOR; la notación de fórmulas; identidades y De Morgan por tabla de verdad; formas canónicas; mapas de Karnaugh, con casillas «no importa»; contar puertas | 01 | 03, 04, 05, 06 (netlists), 07 (mapas con X) |
| [03 · Universalidad de NAND (y NOR)](03-universalidad-nand.md) | Construir las demás puertas solo con NAND o solo con NOR, cuánto cuesta en puertas y capas, y qué es un conjunto de puertas funcionalmente completo | 02 | 05 (variante NAND del sumador); 06 (misma notación de netlists) |
| [04 · Memoria: latches y flip-flops](04-memoria-biestables.md) | Memoria como realimentación positiva; latch SR con NOR y con NAND; estado prohibido; latch D; flip-flop D maestro-esclavo; flip-flop T; el modelo temporal de retardo unitario | 02 | 05 (modelo temporal), 06 (flip-flop T, reloj y flanco), 07 (flip-flop D, biestable SR) |
| [05 · Aritmética binaria](05-aritmetica-binaria.md) | Semisumador y sumador completo; sumador de 4 bits con acarreo en cadena; complemento a dos; banderas de acarreo y desbordamiento; restar con el mismo sumador; cuánto tarda en asentarse el resultado | 01, 02 y el Paso 0 del 04 (03 para la variante NAND) | 06 (el contador ripple se compara con el sumador ripple-carry) |
| [06 · Relojes y contadores](06-relojes-y-contadores.md) | Oscilador en anillo; temporizador; divisor de frecuencia con flip-flop T; contadores ripple y síncrono; detector de flanco; alargador de pulso | 04 (02 y 03, de donde viene la notación de netlists, solo como referencia) | 07 (modelo temporal, anillo de NOT, contador) |
| [07 · Máquinas de estado](07-maquinas-de-estado.md) | Estados, entradas, transición y salida; Moore y Mealy; codificación binaria y one-hot; implementación con flip-flops D; un detector de secuencia y un controlador con histéresis | 06, 04 y 02 (Paso 6, mapas con X) | nada: es el final de la serie |

La cadena de requisitos, dibujada. La columna vertebral es la que no se puede saltar; el 03 y el 05 son ramas:

```text
 01 --> 02 --> 04 --> 06 --> 07
         |      |
         v      v
        03     05
```

Ni el 04, ni el 06 ni el 07 exigen haber leído el 03 o el 05: el 06 solo cita el 05 como comparación y el 03 como ejemplo de la misma notación. El 05, en cambio, sí usa del 03 la variante NAND del sumador.

Rutas sugeridas (son una recomendación de lectura, no una regla):

- **Primera vez:** del 01 al 07, en orden.
- **Ya sé binario y operadores bit a bit:** lee solo el Paso 1 del 01 (la notación de toda la serie y qué significa «el script») y sigue en el 02.
- **Quiero llegar a relojes y máquinas de estado:** 01 (Paso 1), 02 (hasta el Paso 6, que incluye las casillas «no importa»), 04, 06 y 07. El 03 y el 05 quedan para después.
- **Quiero la aritmética:** 01, 02, el Paso 0 del 04 (el modelo temporal) y el 05.

## Cómo se comprueban los números

Ninguna tabla, traza ni forma de onda de estos documentos se escribe a mano, y las cifras de la prosa las genera el script o son datos del enunciado que el script tiene registrados. Un programa de Python lo calcula todo y otro código comprueba que el texto dice lo mismo. Cuando un documento dice «el script», se refiere a los `check_*.py` de esta carpeta. Solo usan la biblioteca estándar de Python 3: no hay nada que instalar.

| Script | Documentos | De dónde sale el texto | Qué comprueba |
| --- | --- | --- | --- |
| `check_A.py` (con `build_A.py` y `lib_A.py`) | 01, 02 y 03 | Los `.md` son **salida**: `build_A.py` los escribe con las tablas que calcula `lib_A.py` | Regenera los tres documentos en memoria y exige que los `.md` sean idénticos, así que una edición a mano falla. Recalcula la lógica con código independiente de `lib_A.py` (evaluador de fórmulas propio, búsquedas exhaustivas, Quine-McCluskey por fuerza bruta). Exige que todo número `0b..` o `0x..` de la prosa sea una ecuación verificada o un dato del enunciado, y que cada fuente externa esté citada con su enlace |
| `check_B.py` | 04 y 05 | La prosa está escrita a mano; cada tabla, traza y forma de onda es un bloque que genera el simulador del propio script | Regenera cada bloque y exige que aparezca literalmente en el `.md`; que toda tabla y todo bloque de código del `.md` salga del generador (sin huérfanos); y que la prosa no tenga números que no vengan de un bloque. Además comprueba que su simulador rápido coincide con el de dos fases |
| `check_C.py` | 06 y 07 | Los `.md` se generan a partir de las plantillas `TPL06` y `TPL07` que hay dentro del script; las cifras de la prosa salen de marcadores | Relee los `.md`, regenera y compara. Prohíbe tablas o bloques de código escritos a mano en la plantilla y cifras tecleadas en la prosa. Comprueba la estructura (secciones, separador antes de la solución, diagramas ASCII). Carga `check_B.py` para que el 04 y el 06 digan lo mismo del flip-flop T de puertas: de él toma los mínimos de reloj, y exige que su divisor con T = 1 dé el mismo mínimo y la misma forma de onda que el del 04 |

Para comprobarlo todo (con `-W error` cualquier aviso de Python también cuenta como fallo):

```sh
cd docs/theory
for s in check_*.py; do python3 -W error "$s" || exit 1; done
```

Cada script termina con código 0 si todo coincide. La CI ejecuta el mismo bucle en cada push (sin `-W error`). Los scripts localizan los `.md` junto a sí mismos, así que también se pueden lanzar desde otro directorio. Uno por uno:

```sh
python3 -W error check_A.py    # documentos 01, 02 y 03
python3 -W error check_B.py    # documentos 04 y 05
python3 -W error check_C.py    # documentos 06 y 07
```

Si vas a **editar** un documento, no lo corrijas a mano donde es salida de un generador:

```sh
python3 build_A.py                    # reescribe 01, 02 y 03 desde build_A.py y lib_A.py
python3 -W error check_C.py --write   # reescribe 06 y 07 desde sus plantillas
python3 check_B.py --dump 04          # imprime los bloques generados del 04 (igual con 05)
python3 check_B.py --fill 04          # sustituye los marcadores {{bloque}} del 04 por su bloque generado
```

En 01 a 03 y en 06 y 07 se edita el generador y se vuelve a generar. En 04 y 05 se puede editar la prosa, pero cada tabla y cada bloque de código tiene que venir del generador. En todos los casos, el cambio no está terminado hasta que el script de ese documento sale con 0, y conviene lanzar los tres antes de darlo por bueno. Como la especificación del núcleo cita los Pasos y las Tablas de estos documentos por su número, no los renumeres sin revisarla.

Lo que estas comprobaciones **no** garantizan:

- **Que la explicación sea buena.** Comprueban cifras, tablas y estructura, no si una frase se entiende ni si el español es correcto. Una revisión independiente de la prosa, hecha después de escribir los documentos, está en la [revisión de la teoría](../review/theory-review-20261013.md).
- **Que las citas externas digan hoy lo que dicen aquí.** `check_A.py` exige que cada fuente de los documentos 01 a 03 (Minecraft Wiki, manual de PHP) esté citada con su enlace, pero ningún script descarga esas páginas. La revisión independiente contrastó las citas de la Minecraft Wiki con sus páginas.
- **Que el modelo didáctico sea el del mod.** Eso lo fija la [especificación del núcleo](../spec/01-semantica.md).

## Convenciones de toda la serie

- **Números.** Binario con prefijo `0b` y hexadecimal con prefijo `0x`. Un literal lleva tantos dígitos como bits tenga lo que representa: un nibble son 4 dígitos binarios, y una señal de un solo bit es `0b0` o `0b1`. Dentro de un literal `0x...`, las letras A a F son dígitos hexadecimales, no variables: `0xB` es un número y `B` suelta es una variable o una señal.
- **Operaciones.** El 01 las nombra con palabras (AND, OR, XOR, NOT) y con los operadores de código (`&`, `|`, `^`, `~`). Desde el 02, en las fórmulas, `·` es AND, `+` es OR, `¬` es NOT y `⊕` es XOR; la precedencia es primero `¬`, luego `·`, luego `+`.
- **Orden de los bits.** El bit más alto va primero: la primera variable de una tabla de verdad es el bit más alto de la fila (02), `Q1` en `Q1 Q0` (07).
- **Netlists.** Una puerta por línea, `señal = PUERTA(entradas)`, y la última línea es la salida. En el 02 y el 03 no tienen tiempo. En los documentos con memoria y relojes cada puerta o elemento tarda un paso: el 04 y el 05 lo escriben con `(t)` y `(t+1)`, y el 06 usa netlists con un aviso explícito.
- **Reloj.** El **periodo** de un reloj es el número de pasos de un ciclo completo (CLK a 0 y luego a 1). En el 04 el reloj es simétrico, con tantos pasos a 0 como a 1, y se habla de **semiperiodo**: los pasos que dura cada nivel, la mitad del periodo. En las tablas de periodo mínimo del 06 el reloj está en alto la mitad del periodo, redondeando hacia abajo: un periodo impar tiene un paso más a 0 que a 1 y no es simétrico. Los mínimos del flip-flop T construido con puertas, en el 04 y en el 06, salen del mismo circuito, pero cada documento mide con su criterio (el 04 lee Q un paso antes del flanco siguiente; el 06 lee los contadores en el instante del flanco siguiente y comprueba los periodos de las salidas del divisor). Cada mínimo vale solo para las condiciones con que se mide (T=1 fija, T que cambia en instantes arbitrarios, o el T que calcula un contador), y cada documento las enuncia.
- **Casillas «no importa».** `X` en los mapas de Karnaugh y `-` en las tablas de verdad (con las excepciones de abajo).
- **Satisfactory.** Los documentos no afirman nada del mod: cuántos bits tendrá una señal, qué puertas se ofrecerán o qué retardo tendrá cada pieza aparecen como preguntas abiertas. Lo que el proyecto va decidiendo está en las [decisiones](../decisions/0001-decisiones-iniciales.md) y en la [especificación del núcleo](../spec/01-semantica.md).

Excepciones conocidas a esas convenciones, para que no te sorprendan:

- El 04 escribe los estados de dos bits sin prefijo (`AB=01`, Tabla 4.1); el 06 y el 07 escriben `0b01`. Los ejes de los mapas de Karnaugh del 02 (`00 01 11 10`) también van sin prefijo: son etiquetas.
- El 04 usa `X` en su tabla de excitación (Tabla 4.13), que es una tabla y no un mapa.

## Tabla de símbolos

Dentro de un documento cada letra significa una sola cosa y se define donde se usa, pero la serie **reutiliza las mismas letras** en documentos distintos. Esta tabla junta todas las definiciones: busca la letra y lee la columna del documento que estás leyendo. «—» quiere decir que esa letra no se usa como símbolo en ese documento (aunque aparezca en un nombre como «flip-flop T»).

| Símbolo | 01 | 02 | 03 | 04 | 05 | 06 | 07 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `A`, `B` | los operandos de las operaciones bit a bit (nibbles en el Paso 6, un bit en la Tabla 1.6); en la Tabla 1.1, `B` es el canal azul | variables booleanas de entrada | entradas de las netlists | las dos puertas NOT del anillo (Tabla 4.1) | operandos del sumador; `A_i`, `B_i` es su bit i | — (`A2` y `A3` son puertas AND del contador síncrono, no bits de un operando) | — |
| `C` | — | variable booleana | — | — | acarreo: `C` del semisumador, `C1` a `C4` de la cadena, `Cin`, `Cout` y la bandera `C = C4` | — | — |
| `D` | — | variable booleana (`F4`, `LETRA`) | — | dato (latch D, flip-flop D); `nD` es su negada | — | retardo total del bucle (Paso 2) | `D0`, `D1`: entradas D de los flip-flops; `D` en la histéresis |
| `E` | — | — | — | habilitación (enable) del latch D | — | — | `E0` a `E2`: bits de la codificación one-hot |
| `e` | — | — | — | instante del flanco: el primer instante con CLK=1 (`e-1`, `e+1`) | — | lo mismo: el paso en que el flip-flop 0 ve el flanco | — |
| `Q` | — | — | — | salida del latch o del flip-flop; `Qn` es ¬Q | — | salida del flip-flop T; `Q0` a `Q3`: bits del contador (`Q0` el bajo) | `Q1`, `Q0`: bits del estado (`Q1` el alto); `Q` en la histéresis |
| `R` | `R`, `G`, `B`: canales rojo, verde y azul de #f97316 | — | — | reset del latch SR (`Rn`: activa a nivel bajo) | — | `R0` a `R3`: etapas del contador ripple | — |
| `S` | — | — | — | set del latch SR (`Sn`: activa a nivel bajo) | suma: `S` del semisumador, `S0` a `S3` bits del resultado | `S0` a `S3`: flip-flops del contador síncrono (son los bits `Q0` a `Q3`) | `S`: conjunto de estados; `S0` a `S3`: estados; `S(t)`: estado en el ciclo t; `s0`: estado inicial |
| `T` | — | — | — | entrada del flip-flop T (toggle) | — | lo mismo; `T_k`: condición de cambio de la etapa k | — |
| `X` | — | casilla «no importa» de los mapas | — | «da igual» en la tabla de excitación (Tabla 4.13) | — | — | `X`: conjunto de entradas de la máquina; también casilla «no importa» en los mapas |
| `x` | un nibble cualquiera (`x << n`, Tabla 1.7) | — | señal interna (`x = NAND(u, v)`) | — | un patrón de 4 bits cualquiera (`x` y `NOT x`, Paso 4) | señal de entrada (`x = ENTRADA`) | `x(t)`: bit de entrada de la máquina |
| `Y` | — | última línea de una netlist: la salida (`Y = ...`) | igual que en el 02 | — | — | — | `Y(t)`: salida de la máquina; `Y` también es el conjunto de salidas |
| `M`, `m` | — | `mₖ`: minterm de la fila k; `Mₖ`: maxterm | — | `M`, `Mn`: salida del latch maestro del flip-flop D | — | — | `M`: la máquina completa `(S, X, Y, δ, λ, s0)` |
| `g` | — | — | — | — | `g`: generate, `g = A · B` (`g0` a `g3`) | `g1` a `g3`: las puertas NOT del anillo | `g1 g2 g3`: esas mismas puertas vistas como bits de estado (`g1` el alto) |
| `p` | — | `p`: señal interna (`p = AND(A, B)`); igual `q`, `r`, `s` | — | — | `p`: propagate, `p = A ⊕ B` (`p0` a `p3`); `pc = p · Cin`: acarreo propagado; en la variante NAND, `q` es otra puerta | — | — |
| `N` | — | — | — | — | — | número de copias retrasadas del alargador de pulso | `N`: número de estados; `N0` a `N2`: siguiente valor de cada flip-flop one-hot |
| `n` | número de bits (2^n valores) | número de variables (2^n filas) | número de variables | — | número de bits del sumador | número de NOT del anillo y de bits del contador | número de bits de memoria (2^n estados) |
| `k` | — | número de fila (`mₖ`) | — | — | valor sin signo de un patrón | número de etapas o de BUF extra; `T_k`: etapa k | número de bits de entrada o de estado; `S_k`: k bits coinciden |
| `t` | — | — | señal interna (`t = NAND(A, B)`) | instante, en pasos del modelo (`t = 0, 1, 2...`) | instante, en pasos del modelo | instante, en pasos del modelo | ciclo de reloj completo |

Qué mirar al leer la tabla:

- **Mismo significado en todos los documentos donde aparecen:** `n` es siempre «cuántos»; `T` es la entrada toggle en el 04 y el 06; `e` es el instante del flanco en el 04 y el 06; `Q` es siempre la salida de un latch o flip-flop; `Qn`, `Cin` y `Cout` tienen un solo sentido.
- **Los nombres de las señales internas de una netlist** (`a`, `b`, `o`, `u`, `v`, `w`, `y`, `z`, `na`, `nb`, `a1`, `b2`, `ANCHO`, `PULSO`...) valen solo para el ejemplo donde aparecen. Lo mismo ocurre con `c(t)` (el contador interno del temporizador, 06), `i` (el bit i, 05), `j` (el flanco j, 06) y `f` (la columna de salida de la Tabla 2.9, 02).
- **Letras de un solo documento:** `V` (05, desbordamiento); `P`, `H` y `W` (06, periodo, pasos en alto y anchura de entrada); `L` (07, nivel del depósito); `Z1` a `Z3` (06, las etapas del divisor, que no se llaman `Q` para no chocar con los bits del contador); `δ` y `λ` (07); `Σ` y `Π` (02).
- **El contador del 06 tiene tres juegos de nombres para los mismos cuatro bits:** `Q0` a `Q3` en las tablas y las ondas, `R0` a `R3` en el montaje ripple y `S0` a `S3` en el síncrono. El 06 lo avisa donde lo usa.
- **Un prefijo o un sufijo `n` significa «negado»:** `Qn`, `Mn`, `Sn`, `Rn`, `Z1n` por detrás; `nD`, `nCLK`, `nx`, `nZ3` por delante. `Sn` y `Rn` del 04 son las entradas propias del latch con NAND (activas a nivel bajo), no el resultado de negar `S` y `R`.

Signos que también cambian de significado:

| Signo | Qué significa y dónde |
| --- | --- |
| `+` | Con números es la suma de siempre (01, y las cuentas de los demás: `2×C + S`, `W + N − 1`, `(s + 1) mod 16`). En una fórmula lógica es OR (desde el 02). En el 05, el 06 y el 07 conviven los dos usos y cada documento lo avisa |
| `·` y `×` | `·` es AND en las fórmulas lógicas; la multiplicación aritmética se escribe `×` |
| `^` | En PHP y Python es XOR (`2 ^ 4` es 6, no 16); la potencia se escribe `2 ** 4`. En el texto de la serie, 2^n es siempre «2 elevado a n» |
| `¬`, `⊕` | NOT y XOR en las fórmulas; en el código de PHP y Python son `~` y `^` |
| `-` | Casilla «no importa» en las tablas de verdad (02 y 07); en un mapa de Karnaugh la misma idea es la `X`. También es el signo menos de siempre |
| `(t+1)` | En las ecuaciones de puertas del 04, el 05 y el 06: el valor un paso después. En las tablas características del 04: el valor en que se asienta `Q`, que puede tardar varios pasos. En el 07: el estado del ciclo siguiente |
| «Paso N» y «paso» | «Paso 3», con mayúscula y número, es un apartado del documento; «paso», en minúscula, es la unidad de tiempo del modelo (04 a 06). En el 07 la unidad es el ciclo de reloj |

## Dónde seguir

- [Especificación del núcleo](../spec/01-semantica.md): cómo funcionará exactamente el simulador, con su propia notación.
- [Redstone y teoría lógica](../research/redstone-and-logic-theory.md): la investigación con fuentes primarias detrás de las secciones de Redstone.
- [Revisión de la teoría](../review/theory-review-20261013.md): informe de la revisión independiente de la prosa.
- [Decisiones del proyecto](../decisions/0001-decisiones-iniciales.md): qué se decidió y por qué.
