# SlgSpike

Plugin de medición, descartable, del proyecto Satisfactory Logic (no oficial). Es el **spike 1**: contesta con datos
reales, en el PC del propietario, a las preguntas que no se pueden responder leyendo cabeceras. No añade ningún edificio,
receta ni mecánica, **no se publica en ficsit.app** y su referencia (`SlgSpike`) no se reserva allí.

- Qué se mide y con qué criterio de aceptación: [docs/spike/01-objetivos-y-criterios.md](../../docs/spike/01-objetivos-y-criterios.md).
- Cómo compilarlo, instalarlo y qué log devolver: [docs/spike/02-procedimiento-propietario.md](../../docs/spike/02-procedimiento-propietario.md).

Está escrito sin poder compilar (la toolchain de Unreal solo existe en el PC del propietario). Cada símbolo de
FactoryGame y de SML que usa está comprobado contra el texto de las cabeceras del SDK en el commit
`d2162c999bbaa9650594ee9ad6e6e06190306009` (CL 502094, SML 3.12.0); la tabla está en la sección «Evidencia de símbolos»
del documento 01. Lo que no se pudo confirmar está en su lista «Sin verificar» y no aparece en el código.

## Contenido

```
mod/SlgSpike/
  SlgSpike.uplugin                        descriptor nuevo (no copia el de ningún otro plugin)
  Config/AccessTransformers.ini           vacío a propósito: el spike 1 no necesita ninguno
  Config/Alpakit.ini                      objetivos de la versión de lanzamiento: Windows y LinuxServer
  Source/SlgSpike/SlgSpike.Build.cs       reglas del módulo, copiadas de la plantilla de Alpakit (C++20)
  Source/SlgSpike/Public, Private/
    SlgSpike.*                            módulo; línea S1; engancha el chat del cliente (solo no servidor)
    SlgSpikeLog.h                         categoría de log y ayudas (reloj, modo de red, nº de jugadores)
    SlgSpikeModules.*                     dos módulos raíz nativos: mundo (subsistema, comando) e instancia (RCO)
    SlgSpikeSubsystem.*                   AModSubsystem + IFGSaveInterface: S2, registro del manejador, latido S6
    SlgSpikeTickHandler.*                 IFGFactoryTickHandlerInterface: S3 (estadísticas) y S4 (interruptor)
    SlgSpikeRemoteCall.*                  UFGRemoteCallObject: S5, y el enganche de chat del cliente
    SlgSpikeChatCommand.*                 comando de chat /slgspike (se ejecuta en el servidor)
```

No hay `Content/`, ni cabeceras de FactoryGame, ni activos del juego, ni dependencia del núcleo `core/`.

## Qué hace cada pieza

| Pieza | Check | Qué deja en el log |
|---|---|---|
| `FSlgSpikeModule::StartupModule` y los dos módulos raíz | S1 | CL del proceso, si es servidor dedicado, fases del ciclo de vida de SML |
| `ASlgSpikeSubsystem` (guardado) | S2 | contadores `SaveGame` (`load_count`, `save_count`, `begin_play_count`, `spike_id`) y el orden de las llamadas |
| `USlgSpikeTickHandler` (estadísticas) | S3 | por llamada (las primeras 120 y bajo demanda) y un resumen cada 10 s; si con un jugador conectado no llega ninguna llamada en 5 s, `HandlerSilent` y un único reregistro (`Remove` y `Add`) |
| `USlgSpikeTickHandler` (interruptor) | S4 | estado de un interruptor de energía vanilla y de sus dos circuitos antes y después de `SetSwitchOn` |
| `USlgSpikeRemoteCallObject` | S5 | petición del cliente, línea del servidor y respuesta al cliente |
| `ASlgSpikeSubsystem::Tick` | S6 | latido de 1 s que sigue contando con el mundo en pausa |

S7 (qué estado vanilla persiste al retirar el mod) no necesita código: es un procedimiento manual.

## Controles

Comando de chat (se escribe en el chat del juego; SML lo ejecuta en el servidor):

```text
/slgspike status                         contadores S2 y estado del manejador
/slgspike ticks [n]                      vuelca las últimas n llamadas del manejador al log (por defecto 40, máximo 512)
/slgspike burst [n]                      registra cada llamada Pre y Post durante las siguientes n (por defecto 60, máximo 2000)
/slgspike reset                          pone a cero las estadísticas S3
/slgspike hb on|off                      activa o apaga el latido S6
/slgspike rco                            comprueba en el servidor que el RCO del jugador existe (solo si lo escribe un jugador)
/slgspike switch list                    lista los interruptores de energía y el estado de sus circuitos
/slgspike switch <nombre o #n> read|on|off|toggle     #n es el índice que imprime `switch list`
```

Variables de consola (misma petición que el comando `switch`; se leen en cada llamada del manejador):

```text
slgspike.SwitchIndex <n>                 qué interruptor (el idx de la lista)
slgspike.SwitchAction <1..5>             1 lista, 2 lee, 3 enciende, 4 apaga, 5 conmuta; vuelve a 0 antes de repetir
```

Una variable de consola solo cambia en el proceso donde se escribe y el manejador solo existe en el proceso del lado
servidor, así que **no hacen nada si se escriben en la consola de un cliente conectado a un servidor dedicado**. Valen en
la consola de una sesión de anfitrión y, sin verificar, en la consola interactiva del propio servidor dedicado. Para el
servidor dedicado de prueba la vía es el comando de chat.

Mensajes del cliente (prefijo `!`, porque SML consume todo lo que empieza por `/`; no se envían al resto de jugadores):

```text
!slg ping [palabra]                      S5: petición al servidor y respuesta (solo la primera palabra, recortada a 200 caracteres)
!slg status                              S5: dice si el RCO está disponible en este cliente
```

## Formato del log

Toda línea del spike empieza por `[SLGSPIKE]`, sigue el identificador del check (`S1` a `S7`, o `CHAT`) y pares
`clave=valor` sin espacios dentro del valor. Se escribe con nivel `Display`, así que también sale por la consola del
servidor dedicado. Un filtro de una línea basta para recogerlas:

```text
grep -F "[SLGSPIKE]" FactoryGame.log
```

## Reglas del descriptor

| Campo | Valor | Por qué |
|---|---|---|
| `Version` y `SemVersion` | `0` y `0.1.0` | SMR exige que `Version` sea el mayor de `SemVersion`; este plugin nunca se sube |
| `VersionName` | `0.1.0` | debe coincidir con `SemVersion` |
| `GameVersion` | `>=502094` | el suelo no puede superar el CL del servidor; un suelo mayor es un error **fatal** para todo el servidor |
| `Plugins` | SML `^3.12.0`, `Enabled: true`, el primero | rango recomendado por la documentación; `Enabled` es obligatorio |
| `Modules` | `SlgSpike`, `Runtime`, `PostDefault` | igual que SML; sin esta entrada Alpakit no compila el plugin |
| `RequiredOnRemote` | ausente (vale `true`) | el RCO y el enganche del chat existen en el cliente, así que el plugin debe estar en los dos lados |
| `RemoteVersionRange` | ausente | el valor por defecto exige la misma versión en los dos lados |
| `GameFeature` | ausente | lo escribe Alpakit solo si el plugin está bajo `Mods/GameFeatures`; este va bajo `Mods/` |

## Cómo se añadiría el núcleo (no en el spike 1)

El núcleo (`core/include/slg` y `core/src`, C++20 sin Unreal) **no** forma parte de este plugin. Si un spike posterior
lo necesita, se copiaría al empaquetar, sin duplicarlo en el repositorio: un script copia `core/include/slg` a
`Source/SlgSpike/Public/slg` y `core/src` a `Source/SlgSpike/Private/slg_core`, y el módulo ya compila todo `.cpp` bajo
`Source/SlgSpike/`. Antes hay que resolver el punto 8 de la especificación 02 §8 (`<span>`, excepciones y RTTI
con la configuración de Unreal) y volver a pasar `make forbidden` sobre lo copiado. El spike 1 se mantiene libre de él
para que un fallo de compilación apunte al adaptador y no al núcleo.

## Licencia

Como el resto del repositorio (MIT, ver `LICENSE`). El plugin enlaza con SML (GPL-3.0); mientras no se distribuya, la
cuestión de licencias combinadas que recoge la investigación de publicación no se plantea.
