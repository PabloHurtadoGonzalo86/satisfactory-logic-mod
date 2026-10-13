# Satisfactory Logic (nombre de trabajo)

> **Estado: borrador de trabajo.** El repositorio es público desde el 13-oct-2026 para servir la web de
> documentación: **<https://pablohurtadogonzalo86.github.io/satisfactory-logic-mod/>**. El mod todavía no existe y
> no está publicado en ficsit.app. El nombre definitivo y la referencia de ficsit.app (`mod_reference`, permanente
> una vez creada) están pendientes.

Un mod de lógica digital **estilo Redstone** para Satisfactory: señales de 4 bits (0–15, un dígito
hexadecimal), cables, puertas, memoria, relojes, sensores y actuadores, con un simulador determinista de tiempo
discreto y una web que explica toda la teoría matemática y toda la implementación.

## Por qué existe aunque ya haya otro mod de lógica

Ya existe **Circuitry** (`FicsitWiremod`, [ficsit.app](https://ficsit.app/mod/FicsitWiremod)): 195 480
descargas, versión 14.1.63 del 26-sep-2026, con puertas, latches, flip-flops y temporizadores. Lo comprobamos
el 13-oct-2026 en la API de ficsit.app. Nuestro punto de partida, declarado a propósito como **inferencia por
confirmar en el juego**, es distinto:

- tiempo discreto **determinista** con retardo de propagación (en Circuitry cada puerta es un actor evaluado
  por frame);
- señales 0–15 como la fuerza de Redstone, que llevan la información de un dígito hexadecimal (4 bits) en un
  único valor por cable;
- licencia permisiva y diseño desde cero (Circuitry no tiene licencia: no se puede copiar ni adaptar nada de
  su código; FicsIt-Networks es GPL-3.0);
- teoría y documentación completas, con comprobaciones ejecutables.

## Decisiones del propietario (13-oct-2026)

| Decisión | Resultado |
| --- | --- |
| Dónde vive | Repo aparte. Privado al principio; **público desde el 13-oct-2026** para servir la web (decisión del propietario) |
| Tipo de señal | Nibble 0–15 (el 0/1 puro es el caso particular). Provisional hasta leer la investigación |
| Orden de trabajo | Núcleo del simulador y teoría primero; el adaptador de Unreal después |
| Estrategia | Mod propio diferenciado, no una copia de Circuitry |
| Web de documentación | GitHub Pages de este repo, **activa**. Con plan Free Pages solo sirve repos públicos; por eso el repo es público |
| Pruebas | Se automatizan todas las posibles; el propietario prueba en el juego al final |

El motivo y las alternativas de cada una están en [docs/decisions](docs/decisions/0001-decisiones-iniciales.md).

## Estructura

```
docs/theory/      teoría en español; cada tabla y cifra la genera y comprueba un script (python3 check_*.py)
docs/decisions/   decisiones y su motivo
docs/research/    investigación con fuentes primarias y veredictos de escépticos (generada desde data/*.json)
tools/            scripts del repo (render_research.py)
core/             (pendiente) núcleo C++20 sin Unreal, verificable en Linux
mod/              (pendiente) adaptador de Unreal, se compila en Windows
site/             (pendiente) web de documentación
```

## Comprobar la teoría

```sh
cd docs/theory && for s in check_*.py; do python3 "$s" || exit 1; done
```

La CI hace lo mismo en cada push.

## Límites que se asumen desde el principio

- El motor de Unreal con el que se compila el mod es de acceso restringido y solo existe en el PC Windows del
  propietario. Aquí solo se verifica de verdad el núcleo sin Unreal.
- "0 warnings" se refiere **solo a nuestro código**; el SDK de Satisfactory emite avisos que no controlamos.
- Se compila contra el SDK fijado por el propietario (CL 502094, SML 3.12.0). El juego pasó a CL 513885 el
  12-oct-2026; subir de versión será una decisión explícita.

## Transparencia sobre el uso de IA

Buena parte del código y de la documentación se escribe con ayuda de Claude (Anthropic) y la revisa y verifica
el propietario. ficsit.app exige declarar el uso de IA en el código al publicar, y se hará.

## Aviso legal

Proyecto de aficionados, **no afiliado ni respaldado** por Coffee Stain Studios, Mojang, Microsoft ni Wube. Licencias de
terceros, citas y atribuciones: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Licencia

[MIT](LICENSE). Decisión provisional del propietario; puede revisarse antes de hacer público el repo.
