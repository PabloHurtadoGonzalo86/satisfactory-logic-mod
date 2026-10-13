# Satisfactory Logic

**Lógica digital estilo Redstone para Satisfactory** (nombre de trabajo). Un mod con señales de 4 bits (un valor
0–15 por cable, la información de un dígito hexadecimal), cables, puertas, memoria, relojes, sensores y actuadores,
sobre un simulador determinista de tiempo discreto. Esta web explica la teoría matemática, el diseño y la implementación, y se actualiza con cada
avance del proyecto.

!!! warning "Estado: borrador de trabajo"
    El mod todavía no existe: hoy hay teoría verificada, una especificación completa del núcleo y la
    investigación que la respalda. El núcleo del simulador (C++ sin Unreal) está en construcción y el adaptador
    de Unreal vendrá después. Nada de esto está publicado en ficsit.app.

## Por dónde empezar

| Si quieres… | Lee… |
| --- | --- |
| Saber qué está hecho y qué falta | [Estado del proyecto](estado.md) |
| Aprender la teoría desde cero (binario, puertas, memoria, relojes, máquinas de estado) | [Teoría](theory/README.md) |
| Ver cómo funcionará exactamente el simulador | [Especificación](spec/01-semantica.md) |
| Saber por qué se decidió cada cosa | [Decisiones](decisions/0001-decisiones-iniciales.md) |
| Comprobar de dónde sale cada afirmación | [Investigación](research/README.md) |
| Ver qué errores encontró la revisión independiente de la teoría | [Revisión](review/theory-review-20261013.md) |

## Por qué otro mod de lógica

Ya existe **Circuitry** (195 480 descargas a 13-oct-2026), con puertas, latches y temporizadores. Este proyecto
apuesta por lo que no hemos encontrado en él, y que habrá que confirmar en el juego: un modelo de **tiempo discreto
determinista con retardo de propagación**, señales 0–15 como la fuerza de Redstone, licencia permisiva y una
documentación que explica la teoría completa con ejemplos comprobados por scripts. Los detalles y las fuentes están
en [Decisiones](decisions/0001-decisiones-iniciales.md) e [Investigación](research/prior-art.md).

## Cómo se comprueba lo que aquí se afirma

- Cada tabla, cifra y forma de onda de la teoría la **genera y re-comprueba un script** de Python (`check_*.py`).
- Los ejemplos y los valores de hash de la especificación se recalcularon de forma independiente a mano y por script.
- Las páginas de investigación se **generan** a partir de los datos originales (con veredictos de escépticos que
  intentaron refutarlas), y la CI falla si alguien las edita a mano.
- Lo que no está verificado se marca como **propuesta** o **sin verificar**; no se afirma nada del juego sin fuente.

## Aviso

Proyecto de aficionados, **no afiliado ni respaldado** por Coffee Stain Studios, Mojang, Microsoft ni Wube. Más
detalles, licencias y atribuciones en los [avisos de terceros](https://github.com/PabloHurtadoGonzalo86/satisfactory-logic-mod/blob/main/THIRD_PARTY_NOTICES.md).
Parte del código y la documentación se ha escrito con ayuda de Claude (Anthropic).
