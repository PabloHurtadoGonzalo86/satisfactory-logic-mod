# Estado del proyecto

Última actualización: 13-oct-2026. Esta página se actualiza con cada hito; los detalles y las fuentes están en
las demás secciones de la web.

## Dónde estamos

| Fase | Qué es | Estado |
| --- | --- | --- |
| 0. Investigación | 7 temas con fuentes primarias, 78 veredictos de escépticos independientes (46 confirmados, 31 parcialmente correctos, 0 refutados, 1 no verificable) y un crítico de huecos | **Hecha**: [Investigación](research/README.md) |
| 1. Teoría | 7 documentos en español, de binario y hexadecimal a máquinas de estado; cada cifra la genera y comprueba un script | **Hecha y revisada**: una revisión independiente halló 22 problemas de prosa y se corrigieron ([revisión](review/theory-review-20261013.md)) |
| 2. Especificación del núcleo | Semántica, API, plan de pruebas y decisiones; tres arquitectos propusieron, dos jueces puntuaron | **Hecha**: [especificación](spec/01-semantica.md) |
| 3. Núcleo del simulador (C++ sin Unreal) | Implementación, oráculo independiente en Python, tests, mutantes y revisiones | **Hecho y en revisión**: núcleo, capa del anfitrión, pruebas de propiedades, trazas doradas, mutantes y oráculo coinciden; las 29 ambigüedades que halló el oráculo están arbitradas ([ambigüedades](spec/ambiguities.md)) |
| 4. Prueba mínima en el PC del propietario | Qué puede y qué no puede hacer un mod en el juego (tipo de cable, gancho de tick, edificio nativo en servidor Linux) | **Preparada, sin ejecutar**: criterios, procedimiento y un plugin cuyos 300 símbolos se auditaron contra el SDK ([prueba mínima](spike/01-objetivos-y-criterios.md)). Falta que el propietario la ejecute; es el mayor riesgo del proyecto |
| 5. Adaptador de Unreal | Subsistema de servidor, sensores y actuadores, edificios | Pendiente de la fase 4 |
| 6. Publicación | Mod en ficsit.app con declaración de uso de IA | Pendiente; requiere la cuenta del propietario. La **web y el repositorio ya son públicos** desde el 13-oct-2026 |

## Qué se puede verificar aquí y qué no

- **Aquí, de forma automática:** el núcleo sin Unreal (tests, propiedades, mutantes, hashes dorados, comparación
  con un oráculo escrito aparte) y la teoría (scripts). Se ejecutan en CI en cada cambio.
- **Solo en el PC del propietario:** compilar y empaquetar con el motor de Unreal (acceso restringido) y probar
  dentro del juego. "0 avisos" se refiere a nuestro código: el SDK de Satisfactory emite avisos que no controlamos.

## Decisiones tomadas y abiertas

Las decisiones del propietario están en [Decisiones del proyecto](decisions/0001-decisiones-iniciales.md). El diseño
del núcleo ya aplica valores por defecto a cinco puntos que conviene que el propietario confirme
([Decisiones del diseño](spec/04-decisiones.md)); el arbitraje de las ambigüedades añadió D28 a D35 (clasificación de
estados, orden de `decode`, ids de un lote y política del `FrameDriver`), con D31 a D35 también por confirmar:

| Id | Punto | Valor por defecto aplicado |
| --- | --- | --- |
| D04 | Qué domina en el latch nativo si S y R valen a la vez | Gana R (reinicio) |
| D09 | Valor canónico de "verdadero" | 15 |
| D13 | Duración del paso lógico y tope de pasos por llamada | 100 ms y 4 pasos (a medir en un servidor real) |
| D16 | Guarda contra el transitorio tras editar un circuito | Activa |
| D23 | Cómo llamar al diseño en público | "Diseño desde cero", sin afirmar "clean-room" |

## Qué sigue

1. Revisar y fusionar el núcleo, y publicar sus resultados en esta web.
2. Preparar la prueba mínima del PC: un script de un comando y una lista de comprobaciones.
3. Una demostración interactiva del simulador en la propia web (el mismo núcleo compilado a WebAssembly).
4. Elegir el nombre definitivo y la referencia de ficsit.app, que es permanente.
