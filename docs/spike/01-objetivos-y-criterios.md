# Spike: objetivos y criterios de aceptación

Estado: borrador del 13-oct-2026. Se escribió **sin poder compilar ni ejecutar nada de Unreal**: la toolchain solo existe en el PC del propietario. Todo lo que dice del juego sale del texto de las cabeceras del SDK en el commit `d2162c999bbaa9650594ee9ad6e6e06190306009` (CL 502094, SML 3.12.0), de la documentación de modding y de las páginas de [investigación](../research/README.md). Lo que no se pudo confirmar está en «Sin verificar» y **no** aparece en el código. El procedimiento paso a paso para el propietario es [02](02-procedimiento-propietario.md).

## 0. Qué es esto y cómo se lee

La [investigación](../research/critic.md) cerró con un riesgo que bloquea el proyecto: casi toda incógnita que decide el diseño solo se resuelve en el PC del propietario, y construir el mod completo antes de un corte vertical significaría escribir mucho código Unreal sin poder compilarlo. Este documento define ese corte en dos partes:

- **Spike 1**: código ya escrito (`mod/SlgSpike`, un plugin que no añade ninguna mecánica) que mide siete cosas (S1 a S7). Se compila, se instala y se ejecuta; devuelve líneas de log.
- **Spike 2**: solo especificación (E1 a E5). Se escribirá cuando el spike 1 haya dado luz verde, porque necesita activos del editor y decisiones que el spike 1 condiciona.

Reglas de lectura:

- Cada criterio nombra la línea de log o la observación que lo prueba. Ningún veredicto es «pasa por ausencia de error».
- Veredictos: **PASA** (se cumplen todas las condiciones de la fila), **FALLA** (se da alguna condición de la fila) e **INCONCLUSO** (la prueba no pudo distinguirse; se repite, no se interpreta). Un PASA puede llevar una nota obligatoria cuando su fila lo dice (S3: «PASA (reregistro)»); la nota se escribe siempre junto al veredicto.
- Todas las líneas del spike llevan el prefijo `[SLGSPIKE]`, el identificador del check y pares `clave=valor`. Los ejemplos de abajo muestran la forma de la línea; los valores son ilustrativos.
- Las afirmaciones de la investigación se citan como `página Cn` o `página cn` (`runtime-api C1`, `wires c4`...). Ninguna se da por cierta hasta que el check que la resuelve pase.
- Los resultados valen para el CL en que se midieron. Cada línea S1 imprime el CL del proceso. Si el servidor de prueba no está en 502094 (hoy una instalación nueva baja el CL 513885), se anota y se aplica la advertencia de `versions C14`: un mod compilado con las cabeceras de 502094 puede tener diferencias de disposición o de tabla virtual en 513885.

## 1. Preguntas go/no-go

| Id | Pregunta | Resuelve | Si falla |
|---|---|---|---|
| S1 | ¿Carga el plugin en el cliente y en el servidor dedicado Linux, con módulos raíz nativos y sin activos? | `buildables c3`, `runtime-api C14`, `versions C7` y `C10` | no hay adaptador nativo posible tal como está diseñado; se revisa el descriptor y el orden de carga antes de seguir |
| S2 | ¿Se crea el `AModSubsystem` en el servidor y sobrevive una propiedad `SaveGame` a guardar y cargar? | `runtime-api C13` y `C17`; spec 02 §4 y §8 puntos 4 y 6 | no hay dónde guardar el blob de la red lógica; hay que replantear la persistencia (D11, D17) |
| S3 | ¿Se puede registrar un `IFGFactoryTickHandlerInterface` con `AddFactoryTickHandler` desde un mod, y con qué cadencia, delta e hilo lo llama el juego? | `runtime-api C1` (decisión, parcial), `C2`, `C19`, `C22`; critic «Orphaned decision» y gap (b); D13, D14 | el diseño de spec 02 §7 no se puede ejecutar; alternativa a investigar en §5 |
| S4 | ¿Se puede leer un circuito de energía vanilla y mover un interruptor vanilla desde el manejador? | `runtime-api C3`, `C5`, `C6`, `C11`, `C23`; critic gap (f); spec 02 §8 punto 3; D15 | no hay actuadores nativos sin enganches; el MVP de sensores y actuadores se redefine |
| S5 | ¿Funciona un Remote Call Object de ida y vuelta, sin multicast? | `runtime-api C15`, `wires c7`; spec 02 §8 punto 9 | no hay forma de que el cliente edite la red lógica; solo sirven interacciones por `OnUse` |
| S6 | ¿Sigue llamándose el manejador cuando no hay jugadores (Auto Pause) y con qué delta se reanuda? | `runtime-api C18` (decisión, parcial); spec 02 §7.3 y §8 punto 2 | no falla: elige la rama de spec 02 §7.3 |
| S7 | ¿Qué estado vanilla persiste al quitar el mod? | `runtime-api C16`; spec 02 §7.6; critic gap (e) | no falla: fija el aviso de desinstalación y si hace falta «liberar actuadores» |
| E1 | Tipo de circuito propio (subclase de `UFGCircuit`) frente a grafo propio | `wires c4` (decisión, sin verificar), `c1`, `c2`, `c3`; D22; critic «Architecture source of truth» y gap (a) | se descarta la opción A y se sigue con la B (grafo propio, estilo FicsIt-Networks) |
| E2 | ¿Un edificio nativo con receta y schematic, sin activos importados, se construye, se guarda y se desmonta? | `buildables c3`, `c4`, `c13` a `c18`; critic gap (c) | se importan activos mínimos con el guion de Python de `buildables c6` |
| E3 | ¿Basta un plugin simple, o hace falta un `GameFeatureData`? | `versions C8`, `buildables c7`; critic «Contradictions» | se pasa a la plantilla de GameFeature |
| E4 | Lista definitiva de `AccessTransformers.ini` | `buildables c5`, `wires c28`; critic «Gaps» | no falla: es una decisión previa a E1 y E2 |
| E5 | ¿Empaqueta y carga en LinuxServer un edificio solo nativo? | `buildables` «Open questions», `critic` «Existence-proof scope»; critic gap (e) | el edificio exige activos o un servidor distinto; se replantea el objetivo de plataformas |

S1 a S5 son **bloqueantes**: sin los cinco en verde no se construye el adaptador. S6 y S7 no se aprueban ni se suspenden: eligen ramas del diseño. Orden de ejecución: S1, S2, S3 (mundo vacío), S4, S5, S3 (fábrica cargada), S6, S7.

## 2. Spike 1

El código está en `mod/SlgSpike` (descripción en su `README.md`). Los comandos son un comando de chat de SML (`/slgspike`, que SML ejecuta en el servidor porque el cliente lo manda por el RCO propio de SML) y mensajes de cliente con prefijo `!` (`!slg ping`). Cada check dice abajo qué hace.

### S1. El plugin carga

**Qué hace el código.** `FSlgSpikeModule::StartupModule` escribe una línea con el CL del proceso, si es servidor dedicado y si es una compilación Shipping. Dos módulos raíz nativos (`USlgSpikeWorldModule` y `USlgSpikeInstanceModule`) escriben una línea por fase del ciclo de vida de SML. SML añade las suyas: `Discovered N world modules`, `Registered subsystem class '...'`, `Registered chat command SlgSpike:slgspike`, y `Version check passed successfully! Game Changelist: N`.

```text
[SLGSPIKE] S1 event=StartupModule plugin_build=0.1.0 changelist=502094 dedicated_server=1 config=shipping
[SLGSPIKE] S1 event=ModuleLifecycle module=game_instance phase=CONSTRUCTION mod_reference=SlgSpike netmode=no_world
[SLGSPIKE] S1 event=ModuleLifecycle module=game_instance phase=INITIALIZATION mod_reference=SlgSpike netmode=no_world
[SLGSPIKE] S1 event=ModuleLifecycle module=game_instance phase=POST_INITIALIZATION mod_reference=SlgSpike netmode=no_world
[SLGSPIKE] S1 event=ModuleLifecycle module=world phase=CONSTRUCTION mod_reference=SlgSpike netmode=dedicated
[SLGSPIKE] S1 event=ModuleLifecycle module=world phase=INITIALIZATION mod_reference=SlgSpike netmode=dedicated
[SLGSPIKE] S1 event=ModuleLifecycle module=world phase=POST_INITIALIZATION mod_reference=SlgSpike netmode=dedicated
```

| Veredicto | Condición (en el servidor y en el cliente) |
|---|---|
| PASA | Hay exactamente una línea `S1 event=StartupModule` por proceso, con `changelist` igual al de la línea de SML `Game Changelist`, `dedicated_server=1` en el servidor y `0` en el cliente. Hay líneas `ModuleLifecycle module=world` con las tres fases y `mod_reference=SlgSpike`, y `module=game_instance` con las tres (el gestor de instancia las despacha seguidas, `GameInstanceModuleManager.cpp:68-70`; el `netmode` de esas líneas puede ser `no_world`). SML registra `Registered subsystem class` y `Registered chat command SlgSpike:slgspike` en el servidor. El cliente se conecta al servidor sin mensaje `missing mods`. `/slgspike status` responde con tres líneas. |
| FALLA | Falta cualquiera de las anteriores; o aparece `Found mismatched dependencies versions` o `Failed to determine owning plugin for root module`; o el proceso aborta. |
| INCONCLUSO | El servidor no llegó a cargar un mundo (nunca hay `module=world`): se repite con un guardado. |

La trampa que este check detecta es la del **GameVersion**: si el rango del descriptor no contiene el CL del proceso, SML emite un `Fatal` que **aborta todo el servidor o el cliente, no solo este plugin** (`versions C7`; SML `ModLoadingLibrary.cpp:214` y `:266`). Por eso el descriptor usa `>=502094` y nunca el botón de Alpakit que sube el suelo al CL del SDK.

Observaciones útiles aunque no decidan: el CL real del servidor y del cliente (deben ser iguales para conectar), el orden de las líneas `Init` y `BeginPlay` frente a las de S2, y la salida de la orden de consola `TestPluginModuleLoader` del cliente (SML la registra; lista los módulos raíz que encuentra y a qué plugin los asigna).

### S2. Subsistema de mod y propiedad SaveGame

**Qué hace el código.** `ASlgSpikeSubsystem` es un `AModSubsystem` con `ReplicationPolicy = SpawnOnServer` que implementa `IFGSaveInterface` (los siete métodos a mano; `SAVE_EMPTY_IMPLEMENTATION` no se usa porque su `GatherDependencies` es `check(false)`). Guarda cuatro enteros `SaveGame`: `spike_id` (aleatorio, una vez), `begin_play_count`, `load_count` (suma en `PostLoadGame`) y `save_count` (suma en `PreSaveGame`, de modo que el valor guardado incluye el guardado que lo escribe). Cada callback escribe una línea con un contador `ord` para ver en qué orden los llama el juego.

```text
[SLGSPIKE] S2 event=Init ord=1 name=SlgSpike_SlgSpikeSubsystem class=SlgSpikeSubsystem loaded_from_save=0 spike_id=0 ...
[SLGSPIKE] S2 event=BeginPlay ord=2 name=SlgSpike_SlgSpikeSubsystem loaded_from_save=0 spike_id=1482012345 begin_play_count=1 load_count=0 save_count=0 ...
[SLGSPIKE] S2 event=PreSaveGame ord=3 ... load_count=0 save_count=1 game_save_version=NN game_version=502094 ...
```

| Veredicto | Condición |
|---|---|
| PASA | Tras la primera carga de un guardado hecho con el mod: `PostLoadGame` con `loaded_from_save=1`, `load_count=1`, el mismo `spike_id` que antes de guardar y `save_count` igual al último `PreSaveGame`. Tras un segundo ciclo de guardar y recargar: `load_count=2`. En cada arranque hay una sola línea `Init` y una sola `BeginPlay`. El nombre es `SlgSpike_SlgSpikeSubsystem` (nombre construido por SML como `<referencia>_<clase>`). |
| FALLA | Tras recargar `loaded_from_save=0`, o `load_count` no sube, o `spike_id` cambia, o hay dos instancias. |
| INCONCLUSO | El servidor guardó en una ranura distinta de la que cargó, o el guardado se hizo sin que aparezca `PreSaveGame`. |

Se anota también el orden de `PreLoadGame`, `PostLoadGame`, `Init` y `BeginPlay` en una carga (spec 02 §8 punto 4: «orden de carga y `BeginPlay`») y si el valor escrito en `PreSaveGame` es el que vuelve (la pregunta del guardado asíncrono, punto 6; un resultado afirmativo no la cierra del todo, porque no prueba que no haya carreras, pero sí que copiar el blob a la propiedad en ese momento funciona).

### S3. El manejador del tick de fábrica

**Qué hace el código.** El subsistema crea un `USlgSpikeTickHandler` (un `UObject` que implementa `IFGFactoryTickHandlerInterface`, retenido por un `UPROPERTY`) y lo registra con `AFGBuildableSubsystem::AddFactoryTickHandler` en `BeginPlay`, con reintento en el siguiente tick si el subsistema de edificios aún no existe. El objeto se crea con el mundo como `Outer` (como el único mod público que registra un manejador; un `Outer` que sea un actor guardado, como este subsistema, no tiene precedente). Lo da de baja con `RemoveFactoryTickHandler` en `EndPlay` (ese nombre, que spec 02 §8 punto 7 daba por desconocido, está en la cabecera: `FGBuildableSubsystem.h:292`). Si pasados 5 s con un jugador conectado el manejador no ha recibido ninguna llamada, escribe `HandlerSilent` y lo vuelve a registrar **una sola vez** (primero `Remove` y luego `Add`, para no duplicar la entrada si la lista seguía intacta) y abre otra ráfaga de 120 líneas: así un registro perdido se distingue de un juego que nunca llama a los manejadores de un mod. Por cada `PreFactoryTick` y `PostFactoryTick` registra:

- la hora de pared (`FDateTime::UtcNow()` menos la del primer uso), el hueco desde la llamada anterior y el `deltaTime`. Es un reloj de pared, no monotónico: un salto del reloj da un `gap_ms` negativo e invalida la ventana de resumen que lo contiene. Su resolución en la plataforma del propietario es **sin verificar** (el SDK no la documenta); el menor `gap_ms` no nulo del log es el paso real del reloj, y por debajo de él no se interpreta nada. Sirve para huecos de decenas de milisegundos y, promediada, para `factory_span_ms`;
- `GetCurrentSubStepIteration`, `GetLastSubStepIteration` e `IsLastFactorySubStep` (estáticos públicos del subsistema de edificios);
- `GetFactorySimulationTime` y el `GetTimeSeconds` del mundo: las llamadas con el mismo valor de mundo son del mismo fotograma, y así los datos distinguen «una vez por fotograma» de «una vez por subpaso de fábrica»;
- si la llamada ocurre en el hilo del juego (`IsInGameThread`).

Escribe una línea por llamada durante las primeras 120 tras el registro y bajo demanda (`/slgspike burst n`, `/slgspike ticks n`), y un resumen cada 10 s de pared:

```text
[SLGSPIKE] S3 event=HandlerRegistered result=OK attempts=0 buildable_subsystem=BP_BuildableSubsystem_C_0 netmode=dedicated
[SLGSPIKE] S3 phase=pre seq=1 t=0.000000 gap_ms=0.000 dt=0.033333 sub=0/0 last=1 sim=12.3456 world=45.6789 gt=1
[SLGSPIKE] S3 phase=post post_seq=1 t=0.004100 span_ms=4.100 dt=0.033333 sub=0/0 sim=12.3456 gt=1
[SLGSPIKE] S3 event=Summary window_s=10.002 pre_calls=300 post_calls=300 pre_per_s=29.994 frames=300 frames_per_s=29.994 max_calls_in_frame=1 dt_min=0.033000 dt_mean=0.033333 dt_max=0.034000 gap_ms_min=30.1 gap_ms_mean=33.3 gap_ms_max=41.0 last_substep_calls=300 sub_hist=0:300, non_game_thread_calls=0 factory_span_ms_mean=4.2 factory_span_ms_max=9.8 players=1 total_pre_calls=900
```

| Veredicto | Condición |
|---|---|
| PASA | `HandlerRegistered result=OK` en los 10 s siguientes a que el mundo exista, y **no** aparece `HandlerSilent`. Hay al menos 100 líneas `phase=pre` en los 30 s siguientes a que el servidor termine de cargar el mundo (la ráfaga automática da 120 si el manejador se llama al menos unas cuatro veces por segundo; si va más lento, se pide `/slgspike burst 300` antes de juzgar), todas con `gt=1`, y líneas `phase=post`. Al apagar el servidor (o volver al menú en una sesión de anfitrión) aparece `HandlerUnregistered result=OK` y no hay `SIGSEGV`, `Fatal` ni `ensure` en los 60 s siguientes. Tras cargar otro guardado en el mismo proceso, el manejador se registra de nuevo (el registro es transitorio, `runtime-api C1`). |
| PASA (reregistro) | Aparece `HandlerSilent` seguido de `HandlerReRegistered result=DONE`, y **a partir de ese momento** se dan todas las demás condiciones de PASA (al menos 100 líneas `phase=pre` en 30 s, todas con `gt=1`, líneas `phase=post` y baja limpia). Se escribe «PASA (reregistro)» y cambia el diseño (§5): el subsistema de edificios perdió el registro hecho en `BeginPlay`. |
| FALLA | `HandlerRegistration result=FAIL`; o registrado pero sin líneas `phase=pre` en 30 s con un jugador conectado y Auto Pause desactivado, incluso después de `HandlerReRegistered`; o cualquier `gt=0`; o fallo al deshacer el registro o al cerrar el mundo. |
| INCONCLUSO | La mitad del tiempo el servidor estuvo en Auto Pause (ver S6): se repite con Auto Pause apagado y un jugador quieto. Una ventana de resumen con `gap_ms_min` negativo (el reloj de pared saltó): se descarta esa ventana y se repite la medida. |

Resultados que no son de pasa o falla pero fijan el diseño (se devuelven en una tabla, en mundo vacío y en una fábrica cargada, ver 02):

| Dato | Cómo se lee | Decisión que alimenta |
|---|---|---|
| `max_calls_in_frame` y `sub_hist` | `1` y un solo índice: una llamada por fotograma. Más de uno, o varios índices: una por subpaso de fábrica. | spec 02 §7.1 y §7.2 (una muestra por llamada), D14 |
| `pre_per_s` y `frames_per_s` | La tasa real del manejador. | D13 (paso de 100 ms, máximo 4 por llamada) |
| `dt_mean`, `dt_max` y la suma de `sim` | Si `pre_per_s` por `dt_mean` queda por debajo de 1, el tiempo de fábrica se queda atrás del de pared (`runtime-api C2`: el tiempo de fábrica está acotado). | D13 y la política de descarte de spec 02 §7.3 |
| `gap_ms_*` | Regularidad del reloj de la lógica. | D13 |
| `factory_span_ms_*` | Tiempo entre `Pre` y el `Post` siguiente: una cota del coste de despachar la fábrica con la carga de la prueba. | `runtime-api C22` (coste), D20 |
| `players` y la tasa de ticks del servidor (si el propietario tiene `averageTickRate`) | Contexto de la medida. | `runtime-api C19` |
| `HandlerSilent` seguido de `HandlerReRegistered` | Si las llamadas `phase=pre` empiezan **solo después** de `HandlerReRegistered`, el subsistema de edificios reconstruyó su lista tras nuestro `BeginPlay`. No es FALLA: se anota como «PASA con reregistro» y es un hallazgo de diseño (el adaptador debe registrarse más tarde o volver a registrarse). Si siguen sin llegar llamadas, es FALLA. | spec 02 §7.1 y §8 punto 1 |

Si fallara el registro, las alternativas a investigar (**sin verificar**, no están en el spike) son un gancho nativo de SML sobre `AFGBuildableSubsystem::TickFactory` (`runtime-api` recomienda envolver `SUBSCRIBE_METHOD` en `if (!WITH_EDITOR)`) o el tick propio del `AModSubsystem`. Ambas obligan a repasar spec 02 §7.1, que se apoya en que `PreFactoryTick` es el único punto documentado como seguro para leer y escribir cualquier edificio.

### S4. Un interruptor y un circuito de energía vanilla

**Qué hace el código.** `/slgspike switch list` y `/slgspike switch <nombre> read|on|off|toggle` **solo encolan** una petición. El `PreFactoryTick` siguiente la ejecuta: busca los `AFGBuildableCircuitSwitch` con `GetTypedBuildable`, y por cada uno resuelve en ese mismo instante los circuitos de sus dos lados (`GetCircuitID0` y `GetCircuitID1`, `AFGCircuitSubsystem::FindCircuit<UFGPowerCircuit>`: el puntero solo vale en ese tick, `FGCircuitSubsystem.h:81`) y escribe `HasPower`, `IsFuseTriggered` y las estadísticas (`GetStats`, que se refrescan una vez por segundo). Para `on`, `off` y `toggle` llama a `SetSwitchOn` con autoridad y después vigila el interruptor: una línea por llamada las primeras 8 llamadas y una cada 0,25 s hasta los 4 s.

**Disparo.** El enunciado pide una orden de consola; hay dos vías y las dos terminan en la misma petición encolada. La principal, y la única válida contra el servidor dedicado de prueba, es el comando de chat de SML, porque es lo único del lado de las órdenes que el SDK demuestra de principio a fin (se escribe en el chat del cliente y SML lo ejecuta en el servidor). La segunda son dos variables de consola, `slgspike.SwitchIndex` y `slgspike.SwitchAction` (1 lista, 2 lee, 3 enciende, 4 apaga, 5 conmuta), que el manejador consulta en cada llamada con `GetValueOnGameThread()`. **Una variable de consola solo cambia en el proceso donde se escribe**, y el manejador y el subsistema solo existen en el proceso del lado servidor (`SpawnOnServer` no genera el actor en un cliente remoto, `SubsystemActorManager.cpp:61`): escribirlas en la consola de un cliente remoto no hace nada contra el servidor dedicado. Valen en la consola de una sesión de anfitrión (el anfitrión es también el servidor) y, **sin verificar**, en la consola interactiva del propio servidor dedicado (la documentación de SML afirma que se pueden ejecutar órdenes de consola ahí, `SMLChatCommands.adoc:49`, sin decir que valga para variables propias de un mod). No se usa `FAutoConsoleCommand`: no hay ningún ejemplo en el SDK. Un interruptor se nombra por su nombre de actor, por una parte única de él o por `#n` (el índice que imprime `list`).

Cada petición recorre una vez todos los edificios del mundo (`GetTypedBuildable`), y durante los 4 s de vigilancia se repite en cada llamada del manejador: **no se lanzan órdenes S4 dentro de una medición de S3** (distorsionarían `factory_span_ms_*`).

```text
[SLGSPIKE] S4 event=List switches=1 handler_call=412
[SLGSPIKE] S4 phase=list switch=Build_PowerSwitch_C_2147482345 on=1 connected=1 circuit_id0=3 circuit_id1=5 loc=1200,-300,150 elapsed_s=0.000 authority=1
[SLGSPIKE] S4 circuit=side0 id=3 found=1 has_power=1 fuse_triggered=0 capacity_mw=30.00 produced_mw=30.00 consumed_mw=12.00 max_consumption_mw=12.00
[SLGSPIKE] S4 event=SetSwitchOn switch=Build_PowerSwitch_C_2147482345 action=off before=1 wanted=0 handler_call=455
[SLGSPIKE] S4 phase=after_same_call switch=Build_PowerSwitch_C_2147482345 on=0 connected=1 circuit_id0=3 circuit_id1=5 ...
[SLGSPIKE] S4 phase=watch switch=Build_PowerSwitch_C_2147482345 on=0 ... elapsed_s=0.033
```

| Veredicto | Condición |
|---|---|
| PASA | `switch list` encuentra el interruptor y sus dos `circuit_id` son `>= 0`. Los `has_power` de las líneas `read` coinciden con lo que se ve en el juego. Tras `off`: `after_same_call` muestra `on=0` y, en los 4 s de vigilancia, el lado de carga pasa a `has_power=0` (aunque cambie su `circuit_id`) y el interruptor se ve apagado en el cliente en 2 s. Tras `on` se recupera `has_power=1` en los 4 s. Lo mismo con una orden dada desde un cliente remoto que no es el anfitrión. Sin `Fatal`, `ensure` ni caída. |
| FALLA | `SetSwitchOn` no cambia `IsSwitchOn` ni en la misma llamada ni en la vigilancia; o `has_power` no cambia nunca aunque el interruptor se vea cambiado; o el cliente no ve el cambio; o hay caída. |
| INCONCLUSO | El circuito de prueba no tenía generación (`produced_mw=0` los dos lados): se rehace con una fuente activa. |

Datos que se devuelven aunque pase: cuántas llamadas y cuántos segundos tarda `has_power` en cambiar tras `SetSwitchOn` (el retardo de la vía vanilla, que decide si D03 y los pesos de spec 01 §2.5 valen para el adaptador), si `circuit_id0` y `circuit_id1` cambian tras el cambio (`runtime-api C11`: los identificadores pueden cambiar y no se cachean), y cuándo se actualizan `consumed_mw` y `produced_mw` (esperable: hasta 1 s de retraso).

### S5. Remote Call Object de ida y vuelta

**Qué hace el código.** `USlgSpikeRemoteCallObject` es un `UFGRemoteCallObject` con un `UFUNCTION(Server, Reliable, WithValidation)` y una réplica `Client, Reliable`, la propiedad replicada ficticia que el RCO necesita, y se registra en `UGameInstanceModule::RemoteCallObjects`. No hay `Multicast`. El cliente lo dispara con `!slg ping palabra` en el chat (solo se envía la primera palabra tras `ping`, recortada a 200 caracteres: una validación fallida de un RPC corta la conexión del cliente): el módulo engancha `AFGPlayerController::PlayerControllerBegunPlay` y, en cada controlador, `ChatMessageEntered` (como hace SML con `/`), con otro prefijo para no cruzarse con SML.

```text
[SLGSPIKE] S5 event=ChatHookAttached controller=BP_PlayerController_C_0 netmode=client       (cliente)
[SLGSPIKE] S5 event=Client_SendPing nonce=1 netmode=client                                    (cliente)
[SLGSPIKE] S5 event=Server_Ping nonce=1 text=hola netmode=dedicated owner_controller=BP_PlayerController_C_0 pings_received=1   (servidor)
[SLGSPIKE] S5 event=Client_Pong nonce=1 server_pings_received=1 netmode=client                (cliente)
```

| Veredicto | Condición |
|---|---|
| PASA | Tres `!slg ping` seguidos producen en el servidor `Server_Ping` con `nonce` 1, 2 y 3 en orden y en el cliente tres `Client_Pong` con el mismo `nonce`, cada ida y vuelta en menos de 2 s según las marcas de tiempo del log. `!slg status` da `rco_available=1`. `/slgspike rco` en el servidor da `rco_available=1`. Tras desconectarse y volver a conectar, el ping sigue funcionando. Sin desconexión ni caída. |
| FALLA | `rco_not_available` en el cliente que persiste 5 s después de conectar y también tras reconectar; o el servidor no registra `Server_Ping`; o el cliente no recibe `Client_Pong`; o la conexión se corta. |
| INCONCLUSO | El cliente no tiene el enganche (`ChatHookAttached` ausente): el módulo se cargó después del controlador; se repite desde el menú principal. Un `rco_not_available` justo al conectar (el propio SML reintenta en cada tick hasta que su RCO está disponible, `SessionSettingsSubsystem.cpp:82-83`): se repite a los 5 s antes de concluir. |

Este check responde a la pregunta de spec 02 §8 punto 9 («cómo devuelve el servidor el estado de una edición del cliente sin multicast»): si la respuesta `Client` por el RCO propietario llega, ese es el camino. También confirma el nombre del recuperador, `GetRemoteCallObjectOfClass` en el SDK y `...ByClass` en el texto de la documentación (`runtime-api C15`).

### S6. Auto Pause

**Qué hace el código.** El subsistema tiene un latido (`Tick` con `bTickEvenWhenPaused = true`) que escribe una línea por segundo con la hora de pared, el contador de llamadas del manejador y la edad de la última llamada. Si el actor sigue latiendo y el manejador deja de contarse, el mundo está en pausa y la fábrica no avanza.

```text
[SLGSPIKE] S6 event=Heartbeat gap_s=1.001 actor_ticks=30 tick_dt=0.033 world_time=812.4 handler_pre_calls=24001 handler_last_call_age_s=0.031 players=0
```

La hipótesis inicial de la investigación (**sin verificar**) es que la pausa congela el manejador: el constructor del subsistema de edificios fija `bTickEvenWhenPaused = false` para su tick de fábrica (`FGBuildableSubsystem.cpp:63`), y el propio juego define `IsPaused` en el componente del servidor dedicado (`FGGameModeDSComponent.h:36`), con una opción `mAutoPause` (`:57`) y un retardo de arranque `ServerAutoPauseStartupTimeDelay` (`:43`).

| Resultado | Condición | Qué fija |
|---|---|---|
| A, se congela | Sin jugadores, `handler_last_call_age_s` crece por encima de 5 s mientras el latido sigue (`gap_s` cerca de 1). Al volver un jugador, la primera llamada tiene `gap_ms` de la duración de la pausa. | spec 02 §7.3: el tiempo lógico se congela con la fábrica |
| B, sigue | Sin jugadores, `handler_pre_calls` sigue creciendo a la tasa normal. | spec 02 §7.3: hay que tratar el tiempo como continuo; D13 |
| C, todo se detiene | También se detiene el latido. | el mundo entero se congela; ningún reloj propio funciona |

Además se anota el `dt` de la primera llamada tras reanudar: si es del orden de la pausa y no del de un fotograma, el adaptador debe acotarlo antes de pasarlo a `StepPacer` (spec 02 §7.1 punto 3).

Si el servidor arranca con Auto Pause ya activado y aparece `HandlerSilent` (solo se escribe con un jugador conectado), se anota pero no se interpreta como fallo de registro hasta repetir con Auto Pause apagado (S3).

PASA aquí significa solo «obtenido A, B o C con Auto Pause confirmado en el servidor y el servidor activo desde hace al menos 5 minutos» (el retardo de arranque existe pero su valor no está en el SDK). Sin esa confirmación el resultado es INCONCLUSO. No hay FALLA salvo caída.

### S7. Qué persiste al quitar el mod

Es un procedimiento manual y no tiene código propio: reutiliza las líneas `S2` (contadores) y la acción `off` de S4 (el interruptor). Los pasos para el propietario están en [02](02-procedimiento-propietario.md), §5 «S7». Se trabaja **siempre sobre una copia del guardado**.

1. Guardado A: hecho con el mod, con el interruptor de S4 apagado por el spike y contadores de S2 mayores que cero.
2. Quitar `SlgSpike` del servidor **y del cliente** (si no, el servidor cierra la conexión con `Client missing mods: ...`, `SMLNetworkManager.cpp:191`, y esa es la parte de control negativo: si se conecta un cliente sin el mod a un servidor con el mod, debe verse ese rechazo).
3. Cargar una copia de A sin el mod.
4. Volver a guardar sin el mod (copia B).
5. Reinstalar el mod y cargar B.

| Veredicto | Condición |
|---|---|
| PASA | El paso 3 carga como juego base, sin caída ni pantalla de carga bloqueada. El interruptor sigue apagado (el estado vanilla escrito persiste, `runtime-api C16`). El log no contiene líneas `S2` en el paso 3. En el paso 5, `load_count` vuelve a empezar (el contenido del mod se perdió al guardar sin él, como dice la FAQ). |
| FALLA | Caída, bloqueo en la carga, o el interruptor vuelve a encendido (entonces el mod estaba reaplicando estado y la tesis de `runtime-api C16` se corrige). |
| INCONCLUSO | El servidor rechazó al cliente por el desajuste de mods antes de llegar al paso 3: se repite quitando el mod en los dos lados. |

Se copian las líneas del log del paso 3 que mencionen clases desconocidas o activos que no cargan: sirven de lista de limpieza para el aviso de desinstalación y para el comando «liberar actuadores» de spec 02 §7.6.

## 3. Spike 2 (solo especificación)

No se escribe código hasta cerrar el spike 1. Todo vive en un plugin aparte (`SlgSpike2`) para que un fallo no contamine las medidas del spike 1. Cada check dice qué cabecera lo respalda; los miembros privados que exigen `AccessTransformers.ini` están en E4.

### E1. Tipo de circuito propio frente a grafo propio

**Pregunta.** ¿Una subclase directa de `UFGCircuit` (no de `UFGPowerCircuit`) se crea, se actualiza, se replica, se guarda y sobrevive a quitar el mod? Es la opción A de [wires-and-connections](../research/wires-and-connections.md); la B es un grafo propio estilo FicsIt-Networks.

**Qué se escribiría (mínimo).**

- `USlgCircuit : UFGCircuit` (la base es abstracta, `FGCircuit.h:21`) con sobrescrituras que solo cuentan y registran: `TickCircuit` (`:97`), `OnCircuitChanged` (`:100`), `IsTrivial` (`:82`), `OnRemoved` (`:87`). `SplitCircuit` y `CreateCircuitGroup` devuelven `nullptr` por defecto (`:103`, `:109`): se mide qué pasa tal cual y, en una segunda variante, con un `UFGCircuitGroup` propio (`PushCircuit`, `PreTickCircuitGroup`, `TickCircuitGroup`, `:151-159`).
- `USlgCircuitConnection : UFGCircuitConnectionComponent`, con `mCircuitType` (protegido, `FGCircuitConnectionComponent.h:163`) fijado en el constructor, `SetIsHidden(true)` (`:35`) y enlace por código con `AddHiddenConnection` (`:94`), sin cable ni holograma. Un edificio nativo con un conector y una orden de chat para enlazar dos.
- Segunda variante, con cable: `AFGBuildableWire` es abstracta (`FGBuildableWire.h:32`), así que hace falta una subclase concreta con `mCircuitType` (`:138`) y `Connect(first, second)` (público, `:87`). El holograma `AFGWireHologram` guarda en privado su tipo de circuito y las recetas de postes automáticos (`FGWireHologram.h:133-141`); ver E4.

**Criterios.**

| Veredicto | Condición |
|---|---|
| PASA (la opción A es viable) | En un servidor dedicado: (1) `FindCircuit` devuelve un `USlgCircuit` con los dos componentes tras el enlace; (2) `TickCircuit` se invoca de forma periódica (sin grupo propio, o con él, y se anota cuál); (3) el cliente lee el mismo `GetCircuitID()` en sus componentes (`mCircuitID` replicado, `FGCircuitConnectionComponent.h:203`); (4) guardar y cargar conservan la pertenencia (no los identificadores, que pueden cambiar); (5) romper el enlace deja los dos circuitos, o uno vacío, sin caída; (6) con el mod quitado, el guardado carga como juego base (como S7). |
| FALLA (se pasa a la opción B) | Alguna de (1) a (6) no se cumple y no hay arreglo con el grupo propio. |
| INCONCLUSO | No hay una fuente de datos para (3): se repite con el registro de réplica de un segundo cliente. |

Medida extra para la decisión del crítico («Architecture source of truth»): el orden de `OnCircuitChanged` respecto a `PreFactoryTick` dentro de un mismo fotograma (la variante híbrida, circuito vanilla solo para conectividad y valores en el subsistema propio). Si el orden es inestable, la opción A inyecta no determinismo que las pruebas T1 y T2 de spec 03 no cubren.

**Resuelve.** `wires c4` (decisión, sin verificar), `c1`, `c2`, `c3` (hace falta un tipo de grupo propio), `c8`; D22; spec 02 §5.

### E2. Edificio nativo, receta y schematic sin activos importados

**Pregunta.** ¿Se puede construir, guardar, cargar y desmontar un edificio cuyo único contenido son clases nativas?

**Qué se escribiría (mínimo).** Una categoría y una subcategoría de construcción (`UFGBuildCategory`, `UFGBuildSubCategory`), un `UFGBuildingDescriptor` con `mBuildableClass` (protegido), una `UFGRecipe` (`mIngredients`, `mProduct`, `mProducedIn` con la ruta blanda al arma de construcción), un `UFGUnlockRecipe` concreto nativo (la clase base es abstracta), un `UFGSchematic` (`mType`, `mTechTier`, `mCost`, `mUnlocks`, protegidos), registrado en `UGameWorldModule::mSchematics`; y un `AFGBuildable` con `mHologramClass`, `mDisplayName` y `mDescription` (públicos) y una malla de `/Engine/BasicShapes` en un `UStaticMeshComponent`. Dos variantes: **A** sin interacción (no necesita transformadores) y **B** con `mIsUseable` y un widget de Slate (necesita E4).

| Veredicto | Condición |
|---|---|
| PASA | El edificio aparece en su categoría de la construcción tras desbloquear el schematic; se construye desde un cliente en un servidor dedicado; sobrevive a guardar, reiniciar y cargar; se desmonta y reembolsa; en el cliente se ve (no es invisible ni una forma de error); la recompensa del schematic se muestra en el HUB o en la MAM, o, si no, se documenta cómo falla (`buildables c13`). No se importó ningún `.uasset`. |
| FALLA | El edificio no se construye, no se guarda, o es invisible en cliente o servidor. |
| INCONCLUSO | Falla solo la presentación del desbloqueo o el icono: se anota y se aplica la solución de `buildables c13` (sustituir por el `BP_UnlockRecipe` de serie) en otra iteración. |

**Resuelve.** `buildables c3`, `c4`, `c13` (interfaz de desbloqueo), `c14` (componente de malla para pintura), `c15` (iconos), `c16` (carga de activos en el constructor), `c17` (holograma), `c18` (primitivas del motor empaquetadas); critic gap (c).

### E3. Plugin simple o GameFeature

**Pregunta.** ¿Hace falta un `FGGameFeatureData`? La documentación dice que «el 99 % de los mods debería» llevarlo (`versions C8`); la investigación de edificios recomienda lo contrario y los dos sustentos no son definitivos (`buildables c7`, critic «Contradictions»).

**Qué se prueba.** El mismo edificio de E2 empaquetado de dos maneras: **P**, plugin simple bajo `Mods/` sin GFDA; **G**, plantilla «C++ & Blueprint» (GameFeature) bajo `Mods/GameFeatures/` con el activo creado en el editor.

| Veredicto | Condición |
|---|---|
| PASA para P | P cumple todo lo de E2 y E5 sin GFDA, y el RCO y el schematic se registran por módulos (como en S5 y E2). |
| FALLA para P | P no registra algo que G sí: se anota qué (iconos, mensajes, ajustes de usuario o contextos de entrada son los casos documentados). |
| Dato de instalación | G exige ficsit-cli desde la 0.7.0 y la carpeta `Mods/GameFeatures` (`versions C17`, `publishing c9`): el propietario instala por SFTP, así que se anota si el montaje manual funciona. |

**Resuelve.** `versions C8`, `buildables c7`, critic «Contradictions» y gap (d).

### E4. Lista de `AccessTransformers.ini`

Cada cambio del fichero fuerza una reconstrucción larga del SDK (`buildables c5`). Por eso la lista se decide **antes** de E1 y E2 y se aplica entera, no a retazos. Los miembros con acceso protegido se asignan desde el constructor de una subclase y no necesitan nada.

| Miembro | Clase y cabecera | Lo necesita | Forma | Estado |
|---|---|---|---|---|
| `mIsUseable` (campo de bits) | `AFGBuildable`, `Buildables/FGBuildable.h:1050` (privado) | E2 variante B | `Friend` hacia la clase del edificio; la forma `Accessor` sobre campos de bits la usa otro mod publicado (`buildables c5`) pero no sobre este campo | `Friend` demostrado en un mod publicado; `Accessor` sin probar en este campo |
| `mInteractWidgetSoftClass` | `AFGBuildable`, `FGBuildable.h:1039` (privado) | E2 variante B | igual que arriba | igual |
| `mPowerConsumption` | `AFGBuildableFactory`, `Buildables/FGBuildableFactory.h:661` (privado) | E2 si el edificio parte de `AFGBuildableFactory` y fija su consumo | `Friend` | Mod publicado lo hace (`buildables c4`, secundaria); **sin verificar** si hace falta en nuestro edificio |
| `mDefaultPowerPoleRecipe`, `mDefaultPowerPoleWallRecipe` y `mCircuitType` | `AFGWireHologram`, `Hologram/FGWireHologram.h:133-141` (privados) | E1 variante con cable | `Friend` hacia la subclase del holograma | **Sin verificar** cuáles hacen falta (`wires c9`, pregunta abierta) |
| `mConsumption`, `mBaseProduction` | `UFGPowerCircuitGroup`, `FGPowerCircuit.h:427` y `:430` (privados) | sensores de potencia por grupo | `Friend` (como hace FRM) | Fuera de los spikes 1 y 2; se anota para el MVP de sensores |
| `mDoorConfiguration` | `AFGBuildableDoor`, `FGBuildableDoor.h:143` (privado) | puertas | `Accessor` más gestión manual de la réplica | Fuera de alcance (`runtime-api C4`: «difícil, más tarde») |

La lista aplicada al spike 1 está **vacía** (`mod/SlgSpike/Config/AccessTransformers.ini`). Regla: no se añade una entrada sin un check que la necesite.

### E5. Empaquetado de un edificio solo nativo en LinuxServer

**Pregunta.** El único precedente de un edificio casi solo nativo no tiene objetivo LinuxServer y su propia lista de comprobaciones está marcada como pendiente (critic «Existence-proof scope»). ¿Funciona aquí?

**Qué se comprueba.** Con el edificio de E2: Alpakit Release con `Windows` y `LinuxServer`; el zip tiene las dos carpetas de objetivo, con el mismo `.uplugin` byte a byte (`publishing c11`) y un `.so` para Linux; el servidor Linux carga el guardado con el edificio sin líneas de activo que no carga; un cliente lo construye y lo ve; reiniciar y recargar lo conserva.

| Veredicto | Condición |
|---|---|
| PASA | Se cumple todo lo anterior, sin `.uasset` en el paquete y sin `Fatal`, `ensure` ni `LoadObject failed` relacionados con el mod en el log del servidor. |
| FALLA | El `.so` no se genera, el servidor no carga el guardado, o el edificio no existe en el servidor. |
| INCONCLUSO | La primitiva `/Engine/BasicShapes` no está en el paquete del servidor pero el cliente la ve: se anota (`buildables c18`). |

**Resuelve.** `buildables` «Open questions» (empaquetado LinuxServer), critic «Existence-proof scope» y gap (e), `publishing c9` y `c11`, `versions C9` y `C10`.

## 4. Orden y dependencias

1. Spike 1 completo (S1 a S7). Sin verde en S1 a S5 no se sigue.
2. E4 (decidir la lista) con los resultados de S3 y S4 delante.
3. E2 y E5 en el mismo corte (se compila una vez), luego E3 como variante del mismo código.
4. E1 en paralelo, en un plugin aparte; no depende de E2.
5. Con todo ello se actualizan spec 02 §8 y las decisiones D11, D13, D14, D15, D16 y D22 en [04](../spec/04-decisiones.md); los resultados de medida entran en la «Resolución» de ese documento, no en este.

## 5. Qué cambia en el diseño según el resultado

| Resultado | Efecto |
|---|---|
| S3 falla (no se puede registrar) | spec 02 §7.1 no se puede ejecutar; investigar el gancho nativo o el tick propio y volver a escribir §7.1 y §8 |
| S3 pasa por subpaso | cambia el contrato de ráfaga de spec 02 §7.2 (hay varias muestras por fotograma) y D14 |
| S3 pasa solo con reregistro (`HandlerSilent` y después llamadas) | el adaptador no puede registrarse una vez en `BeginPlay`: debe registrarse más tarde o volver a registrarse, y se repasa spec 02 §7.1 y §8 |
| S4 pasa con retardo `r` | `r` entra como retardo del actuador en D03 y en los pesos de spec 01 §2.5 |
| S6 resultado B | spec 02 §7.3: el tiempo lógico no se congela con la pausa; revisar `paused` en `StepPacer::advance` |
| S7 con estado reaplicado | se corrige `runtime-api C16` y D15 |
| E1 falla | opción B (grafo propio), 02 §5 deja de mencionar la entrega de lotes por reconstrucción del motor |
| E3 favorece G | se pasa a la plantilla de GameFeature y se repasa el procedimiento de instalación |

## 6. Lo que este spike no cubre

- Rendimiento por puerta en el motor (solo hay una cota del despacho de fábrica en `factory_span_ms`); el núcleo se mide aparte con `make bench`.
- Replicación del estado visual por nodo (`critic`, «Replication cost»).
- Comportamiento frente a Blueprint Designer, copia y pegado, zoop y mejoras de cable (`critic`, «Vanilla interaction tests»).
- Compatibilidad con Circuitry o FicsIt-Networks.
- Los otros actuadores de spec 02 §8 punto 3 (`SetIsProductionPaused`, `SetSortRules`, `SetLightEnabled`): el spike solo ejerce `SetSwitchOn`. Se resuelven en el spike 2 o en el primer corte del adaptador.
- El punto 8 de spec 02 §8 (`<span>`, excepciones y RTTI en UE 5.6.1): el spike no incluye el núcleo (`mod/SlgSpike/README.md`); se resuelve al compilar el núcleo dentro de Unreal, antes de cualquier adaptador.
- De spec 02 §8 punto 1, el orden de `PreFactoryTick` respecto al circuito de potencia dentro de un fotograma: S4 solo mide el retardo de `has_power` tras `SetSwitchOn`, no el orden. Del punto 4, el evento de construcción, el pegado de blueprints y la clave estable del edificio: solo se mide el orden de carga y `BeginPlay` (S2).
- Licencia y publicación: el plugin no se sube a ficsit.app y su referencia no se reserva.

## 7. Evidencia de símbolos

Rutas relativas a la raíz del SDK en el pin `d2162c999bbaa9650594ee9ad6e6e06190306009` salvo que se diga otra cosa. `Doc` es el repositorio Documentation en `d35cc911d6849cb39740cb884735ec5d9ffb2f67` (carpeta `modules/ROOT/pages`). «Secundaria» es el código de un mod publicado, no del SDK.

### Manejador del tick de fábrica (S3, S4, S6)

| Símbolo o hecho | Evidencia |
|---|---|
| `UFGFactoryTickHandlerInterface` es `UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))` | `Source/FactoryGame/Public/FGBuildableSubsystem.h:220` |
| `IFGFactoryTickHandlerInterface` exportada, con `PreFactoryTick(AFGBuildableSubsystem*, float)` y `PostFactoryTick` virtuales | `FGBuildableSubsystem.h:231`, `:236`, `:238` |
| El comentario de cabecera dice «RegisterFactoryTickHandler» pero el método es `AddFactoryTickHandler` | `FGBuildableSubsystem.h:227` (comentario) y `:290` (declaración) |
| `AddFactoryTickHandler` y `RemoveFactoryTickHandler(IFGFactoryTickHandlerInterface*)` | `FGBuildableSubsystem.h:290`, `:292` |
| `static AFGBuildableSubsystem* Get(UWorld*)` | `FGBuildableSubsystem.h:272` |
| Los manejadores se guardan en un `UPROPERTY(Transient)` de `TScriptInterface`, así que el manejador es un `UObject` y el registro se repite por mundo | `FGBuildableSubsystem.h:835-836` |
| «Safe to read and write data for any buildable» solo en `PreFactoryTick` | `FGBuildableSubsystem.h:235` |
| `GetCurrentSubStepIteration`, `GetLastSubStepIteration`, `IsLastFactorySubStep` públicos y estáticos | `FGBuildableSubsystem.h:480`, `:485`, `:490` |
| `GetFactorySimulationTime()` público | `FGBuildableSubsystem.h:496` |
| `GetTypedBuildable<T>(TArray<T*>&)` público | `FGBuildableSubsystem.h:371`, definición `:840` |
| El tick de fábrica del subsistema se declara con `bTickEvenWhenPaused = false` (constructor del esqueleto, indicativo) | `Source/FactoryGame/Private/FGBuildableSubsystem.cpp:63` |
| `AFGBuildableSubsystem` implementa `IFGSaveInterface` con los siete `_Implementation` (precedente de firma) | `FGBuildableSubsystem.h:253-259` |
| `bTickEvenWhenPaused`, `bCanEverTick`, `bAllowTickOnDedicatedServer` en el `PrimaryActorTick` de un `AFGSubsystem`; `bStartWithTickEnabled` | `Source/FactoryGame/Private/FGWorldGridSubsystem.cpp:17-20`; `Private/FGTimeSubsystem.cpp:28` |
| Doc: el tiempo de fábrica está acotado; algunos ticks ocurren en el hilo principal | `Doc: Development/Satisfactory/FactoryTick.adoc:52`, `:14` |

### Subsistema, guardado y módulos (S1, S2)

| Símbolo o hecho | Evidencia |
|---|---|
| `AModSubsystem`, `ReplicationPolicy`, `SpawnOnServer`, `Init()` protegido, aviso de llamar a `Super` en `BeginPlay` y `EndPlay` | `Mods/SML/Source/SML/Public/Subsystem/ModSubsystem.h:35`, `:10`, `:44`, `:24` |
| El nombre del actor es `<referencia>_<clase>` y un actor guardado con ese nombre se reutiliza | `Mods/SML/Source/SML/Private/Subsystem/SubsystemActorManager.cpp:14-15`, `:22` |
| `SpawnOnServer` no genera el actor en clientes remotos (`NM_Client`) | `SubsystemActorManager.cpp:61` |
| `GetSubsystemActor<T>()` y `World->GetSubsystem<USubsystemActorManager>()` | `Mods/SML/Source/SML/Public/Subsystem/SubsystemActorManager.h:47`, `Mods/SML/Source/SML/Private/Module/WorldModuleManager.cpp:48` |
| `IFGSaveInterface`: `PreSaveGame`, `PostLoadGame`, `ShouldSave`, `NeedTransform` como `BlueprintNativeEvent` | `Source/FactoryGame/Public/FGSaveInterface.h:66`, `:93`, `:111`, `:118` |
| `SAVE_EMPTY_IMPLEMENTATION` hace `check(false)` en `GatherDependencies` (por eso no se usa) | `FGSaveInterface.h:41` |
| `gameVersion` es el CL del juego, no una versión del mod | `FGSaveInterface.h:63` |
| `UGameWorldModule::ModSubsystems` y `mChatCommands`; registro en CONSTRUCTION e INITIALIZATION | `Mods/SML/Source/SML/Public/Module/GameWorldModule.h:63`, `:33`; `Mods/SML/Source/SML/Private/Module/GameWorldModule.cpp:54`, `:98` |
| `UGameInstanceModule::RemoteCallObjects` y su registro | `Mods/SML/Source/SML/Public/Module/GameInstanceModule.h:70`; `Private/Module/GameInstanceModule.cpp:99` |
| `bRootModule`, un módulo raíz por tipo, `GetOwnerModReference`, `LifecyclePhaseToString` | `Mods/SML/Source/SML/Public/Module/ModModule.h:47`, `:42`, `:51`, `:65` |
| Descubrimiento de módulos nativos por CDO y error si hay dos | `Mods/SML/Source/SML/Private/ModLoading/PluginModuleLoader.cpp:32`; `Private/Module/WorldModuleManager.cpp:81` |
| Órdenes de consola de SML `TestPluginModuleLoader` y `TestContentOwnerResolution` | `PluginModuleLoader.cpp:12`, `:23` |
| Líneas de log de SML usadas como evidencia | `WorldModuleManager.cpp:92` (`Discovered %d world modules`), `SubsystemActorManager.cpp:89` (`Registered subsystem class`), `Private/Command/ChatCommandLibrary.cpp:107` (`Registered chat command`), `Private/SatisfactoryModLoader.cpp:117` (`Game Changelist`) |
| `Fatal` si el `GameVersion` no contiene el CL o una dependencia no cumple el rango, solo fuera del editor | `Mods/SML/Source/SML/Private/ModLoading/ModLoadingLibrary.cpp:15`, `:214`, `:226`, `:265-266` |
| `FEngineVersion::Current().GetChangelist()` e `#include "Misc/EngineVersion.h"` | `Mods/SML/Source/SML/Private/SatisfactoryModLoader.cpp:110`, `:16` |
| `IsRunningDedicatedServer()` | `Mods/SML/Source/SML/Private/Module/GameInstanceModule.cpp:56` |
| `UE_BUILD_SHIPPING` | `Mods/SML/Source/SML/Private/SatisfactoryModLoader.cpp:135` |

### RCO y enganches de cliente (S5)

| Símbolo o hecho | Evidencia |
|---|---|
| `UFGRemoteCallObject` y `GetOwnerPlayerController()` | `Source/FactoryGame/Public/FGRemoteCallObject.h:14`, `:35` |
| `GetRemoteCallObjectOfClass` (no `...ByClass`) y la versión plantilla | `Source/FactoryGame/Public/FGPlayerController.h:132`, `:416` |
| RCO nativo con `Server, Reliable, WithValidation`, `Client, Reliable`, propiedad replicada ficticia, `DOREPLIFETIME`, `#include "Net/UnrealNetwork.h"` y `_Validate` | `Mods/SML/Source/SML/Public/SessionSettings/SessionSettingsSubsystem.h:42-59`; `Private/SessionSettings/SessionSettingsSubsystem.cpp:8`, `:93`, `:103`, `:133` |
| Doc: se registra en el módulo de instancia de juego; propiedad replicada obligatoria; sin multicast; `GetRemoteCallObjectByClass` en el texto | `Doc: Development/Satisfactory/Multiplayer.adoc:125`, `:157`, `:135`, `:196` |
| `ChatMessageEntered` (dos parámetros, el segundo por referencia) y `PlayerControllerBegunPlay` estático | `FGPlayerController.h:32-33`, `:318`, `:321` |
| SML engancha ambos y consume lo que empieza por `/` | `Mods/SML/Source/SML/Private/Player/SMLRemoteCallObject.cpp:54`, `:56`, `:58`, `:64` |
| `SetTimerForNextTick(FTimerDelegate::CreateUObject(...))` | `Private/SessionSettings/SessionSettingsSubsystem.cpp:83` |

### Comando de chat (S4, control)

| Símbolo o hecho | Evidencia |
|---|---|
| `AChatCommandInstance` y sus campos `bOnlyUsableByPlayer`, `MinNumberOfArguments`, `CommandName`, `Usage` y `ExecuteCommand`; `UCLASS` y `GENERATED_BODY` | `Mods/SML/Source/SML/Public/Command/ChatCommandInstance.h:20`, `:42`, `:50`, `:57`, `:71`, `:84`, `:19`, `:21` |
| `bOnlyUsableByPlayer` vale `false` por defecto y SML lo comprueba antes de ejecutar; los valores `COMPLETED`, `UNCOMPLETED` y `BAD_ARGUMENTS` de `EExecutionStatus` | `Private/Command/ChatCommandInstance.cpp:6`; `Private/Command/ChatCommandLibrary.cpp:149`; `Public/Command/ChatCommandInstance.h:8-17` |
| El registro de un comando no detecta colisiones de nombre (el último gana en silencio), por eso el spike no declara alias | `Private/Command/ChatCommandLibrary.cpp:96-103` |
| Un comando nativo sobrescribe `ExecuteCommand_Implementation` | `Public/Command/SMLCommands/HelpCommandInstance.h:10` |
| El nombre del comando se quita de `Arguments` antes de la llamada (y la ayuda de SML indexa `[1]`, por eso el spike registra los argumentos en bruto) | `Private/Command/ChatCommandLibrary.cpp:132`, `:161`; `Private/Command/SMLCommands/HelpCommandInstance.cpp:14-15` |
| El comando llega al servidor por un RCO de SML y se ejecuta allí | `Private/Player/SMLRemoteCallObject.cpp:22-26` |
| `UCommandSender`: `GetSenderName`, `IsPlayerSender` (falso por defecto), `GetPlayer` (hace `checkf(false)` si el emisor no es un jugador, por eso el spike pregunta antes), `SendChatMessage` | `Public/Command/CommandSender.h:16`, `:22`, `:28`, `:34`; `Private/Command/CommandSender.cpp:3-6`, `:12-14` |
| `TPlayerControllerIterator<T>::ServerAll` e `#include "UnrealEngine.h"` | `Private/Command/ChatCommandLibrary.cpp:60`; `Private/Command/SMLCommands/PlayerListCommandInstance.cpp:5`, `:15` |
| Doc: los comandos de chat se registran en `M Chat Commands` del módulo de mundo y corren en el chat con `/` | `Doc: Development/ModLoader/ChatCommands.adoc:46`; `Doc: SMLChatCommands.adoc:11` |

### Circuitos e interruptor (S4, E1)

| Símbolo o hecho | Evidencia |
|---|---|
| `AFGBuildableCircuitSwitch::SetSwitchOn(bool)`, `IsSwitchOn()`, `IsSwitchConnected()`, propiedad `mIsSwitchOn` `SaveGame` | `Source/FactoryGame/Public/Buildables/FGBuildableCircuitSwitch.h:47`, `:53`, `:59`, `:126` |
| `GetCircuitID0()` y `GetCircuitID1()` | `Source/FactoryGame/Public/Buildables/FGBuildableCircuitBridge.h:39`, `:43` |
| `AFGCircuitSubsystem::Get(UWorld*)`, `FindCircuit<T>(int32)` y la nota de validez de un tick | `Source/FactoryGame/Public/FGCircuitSubsystem.h:45`, `:88`, `:81` |
| `UFGPowerCircuit::HasPower()`, `IsFuseTriggered()` (no const), `GetStats(FPowerCircuitStats&)` | `Source/FactoryGame/Public/FGPowerCircuit.h:256`, `:163`, `:171` |
| `FPowerCircuitStats`: `PowerProductionCapacity`, `PowerProduced`, `PowerConsumed`, `MaximumPowerConsumption`, refresco de 1 s | `FGPowerCircuit.h:101-110`, `:54` |
| `UFGCircuit` abstracta con `TickCircuit`, `OnCircuitChanged`, `IsTrivial`, `OnRemoved`, `SplitCircuit` y `CreateCircuitGroup` por defecto a `nullptr` | `Source/FactoryGame/Public/FGCircuit.h:21`, `:97`, `:100`, `:82`, `:87`, `:103`, `:109` |
| `UFGCircuitGroup`: `PushCircuit`, `PreTickCircuitGroup`, `TickCircuitGroup` | `FGCircuit.h:151`, `:158`, `:159` |
| `UFGCircuitConnectionComponent`: `mCircuitType` protegido, `SetIsHidden`, `AddHiddenConnection`, `GetCircuitID` y `mCircuitID` replicado | `Source/FactoryGame/Public/FGCircuitConnectionComponent.h:163`, `:35`, `:94`, `:122`, `:203` |
| `AFGBuildableWire` abstracta, `mCircuitType` protegido, `Connect` público | `Source/FactoryGame/Public/Buildables/FGBuildableWire.h:32`, `:138`, `:87` |
| Miembros privados del holograma de cable | `Source/FactoryGame/Public/Hologram/FGWireHologram.h:134`, `:138`, `:141` |

### Edificio nativo (E2, E4)

| Símbolo o hecho | Evidencia |
|---|---|
| `AFGBuildable::mHologramClass` público; `mInteractWidgetSoftClass` y `mIsUseable` privados | `Source/FactoryGame/Public/Buildables/FGBuildable.h:820`, `:1039`, `:1050` |
| `AFGBuildableFactory::mPowerConsumption` privado | `Source/FactoryGame/Public/Buildables/FGBuildableFactory.h:661` |
| `UFGSchematic` con `mType`, `mTechTier`, `mCost`, `mUnlocks` protegidos; `UFGUnlockRecipe` abstracta con `AddRecipe` público | `Source/FactoryGame/Public/FGSchematic.h:202`, `:230`, `:234`, `:251`; `Unlocks/FGUnlockRecipe.h:14`, `:29` |
| `UFGBuildingDescriptor::mBuildableClass` protegido | `Source/FactoryGame/Public/Resources/FGBuildingDescriptor.h:74` |
| `Friend` hacia `AFGBuildable` y `AFGBuildableFactory`; `mIsUseable = true` y `mPowerConsumption = 0` en el constructor (secundaria: Teleport Logistics `47343693b5175d3684c709ce3838789e92916047`) | `TeleportLogistics/Config/AccessTransformers.ini:3`; `TeleportLogistics/Source/TeleportLogistics/Private/TeleportLogisticsBuilding.cpp:31`, `:34` |
| Formas `Friend`, `Accessor` y `BlueprintReadWrite` y la reconstrucción | `Doc: Development/ModLoader/AccessTransformers.adoc:28-31`, `:149` |

### Ayudas del motor

| Símbolo o hecho | Evidencia |
|---|---|
| `IsInGameThread()` | `Mods/SML/Source/SML/Private/Patching/BlueprintHookManager.cpp:163` |
| `UWorld::GetTimeSeconds()` | `Source/FactoryGame/Public/FGVertexAnimatedMeshComponent.h:194` |
| `GetNetMode()` y los valores `NM_DedicatedServer` y `NM_Client` | `SubsystemActorManager.cpp:46`, `:55`, `:61` |
| `AActor::GetActorLocation()` y `HasAuthority()` | `Source/FactoryGame/Private/FGGasPillarCloud.cpp:32`; `Private/SessionSettings/SessionSettingsSubsystem.cpp:18` |
| `FMath::RandRange` | `Source/FactoryGame/Public/FGElevatorTypes.h:143` |
| `FString::Join`, `ParseIntoArray`, `FCString::Atoi`, `ToLower`, `StartsWith`, `TrimStartAndEnd` | `Mods/SML/Source/SML/Private/Util/SemVersion.cpp:570`, `:608`, `:735`; `ChatCommandLibrary.cpp:102`; `SMLRemoteCallObject.cpp:58`, `:60` |
| `FDateTime::UtcNow()`, la resta de dos `FDateTime` (da un `FTimespan`) y `GetTotalSeconds()`; cabecera `Misc/DateTime.h` | `Mods/Alpakit/Source/Alpakit/Private/AlpakitInstance.cpp:116`, `Private/SAlpakitLogTabContent.cpp:303`, `Private/AlpakitInstance.cpp:83`, `Public/AlpakitInstance.h:7` |
| `TAutoConsoleVariable<T>` con plantilla explícita (`<bool>` y `<int32>`, este último con definición y valor por defecto), `ECVF_Default` y `GetValueOnGameThread()` | `Mods/SML/Source/SML/Private/Patching/BlueprintHookManager.cpp:184`, `Private/Player/PlayerCheatManagerHandler.cpp:6`, `:10`, `:14`; `Source/FactoryGame/Private/FGWorldEventSubsystem.cpp:8` |
| `#include "HAL/IConsoleManager.h"` | `Source/FactoryGame/Public/FGNoise.h:8`, `FGWorldEventSubsystem.h:9`, `Resources/FGItemDescriptor.h:8` |
| `FString::Left` | `Mods/SML/Source/SML/Private/SessionSettings/SessionSettingsManager.cpp:141`; `Mods/Alpakit/Source/Alpakit/Private/ModMetadataObject.cpp:128` |
| Macros y tipos genéricos del motor que usa el código: `NSLOCTEXT`, `UE_LOG`, `FString::Printf`, `NewObject<T>(Outer)`, `EEndPlayReason::Type`, `ENetMode`, `TObjectPtr<T>`, `FLifetimeProperty`, `FVector`, `TArray::Add/Num/RemoveAt`, `FString::IsEmpty`, `GetClass()->GetName()`, `GetOwnerModReference().ToString()` | `Source/FactoryGame/Private/FGSchematic.cpp:72`; `Public/FGWorldSettings.h:118`; `Private/FGGasPillarCloud.cpp:38`; `Private/FGAtmosphereVolume.cpp:21`; `Public/FGTrain.h:403`; `Mods/SML/Source/SML/Private/Subsystem/SubsystemActorManager.cpp:46`; `Source/FactoryGame/Public/FGWorldPartitionContentBundle.h:38`; `Mods/SML/Source/SML/Private/SessionSettings/SessionSettingsSubsystem.cpp:55`; `Source/FactoryGame/Public/FGVertexAnimatedMeshComponent.h:113`; `Private/Command/ChatCommandLibrary.cpp:97`, `:132`, `:139`, `:155`; `Mods/Alpakit/Source/Alpakit/Private/AlpakitReleaseWidget.cpp:120`; `Private/Command/CommandSender.cpp:4` |
| `GetWorld()`, `GetTimerManager()`, `UFUNCTION`, `UModModule::LifecyclePhaseToString`, `TCHAR`; en `Build.cs`: `PCHUsage`, `PCHUsageMode`, `AddRange` y `using System.IO` | `Mods/SML/Source/SML/Private/Subsystem/SubsystemActorManager.cpp:46`; `Private/SessionSettings/SessionSettingsSubsystem.cpp:83`; `Public/SessionSettings/SessionSettingsSubsystem.h:42-59`; `Public/Module/ModModule.h:65`; `Private/Util/SemVersion.cpp:28`; `Mods/Alpakit/Templates/CPPAndBlueprintBlank/Source/PLUGIN_NAME/PLUGIN_NAME.Build.cs:2`, `:9`, `:16` |
| `UCLASS(NotBlueprintable)`, `UPROPERTY(Replicated)`, `FString::Len`, los miembros `X`, `Y` y `Z` de un `FVector`, y los niveles `Display`, `Warning` y `Error` de `UE_LOG` | `Mods/SML/Source/SML/Public/SessionSettings/SessionSettingsSubsystem.h:41`, `:58`; `Private/Util/SemVersion.cpp:361`; `Source/FactoryGame/Private/FGWorldSettings.cpp:104`; `Mods/SML/Source/SML/Private/Command/ChatCommandLibrary.cpp:107`, `Private/SatisfactoryModLoader.cpp:78`, `Source/FactoryGame/Public/FGWorldSettings.h:118` |
| Módulo y registro: `IMPLEMENT_MODULE`, `IModuleInterface`, `DECLARE_LOG_CATEGORY_EXTERN`, `DEFINE_LOG_CATEGORY`, `ESubsystemReplicationPolicy`, `ELifecyclePhase`, `StaticClass()`, `AddLambda` sobre un delegado | `Mods/Alpakit/Templates/CPPAndBlueprintBlank/Source/PLUGIN_NAME/Private/PLUGIN_NAME.cpp:20`, `Public/PLUGIN_NAME.h:8`; `Source/FactoryGame/Public/FGScannableSubsystem.h:12`; `Private/FactoryGame.cpp:5`; `Mods/SML/Source/SML/Public/Subsystem/ModSubsystem.h:8`; `Public/Module/ModModule.h:10`; `Private/Module/GameInstanceModuleManager.cpp:45`; `Private/SatisfactoryModLoader.cpp:164` |
| Doc: las órdenes de consola se pueden ejecutar desde la consola interactiva de un servidor dedicado | `Doc: SMLChatCommands.adoc:49` |
| `FMath::Clamp`, `Max` y `Min` | `Source/FactoryGame/Public/FGSplineMeshGenerationLibrary.h:281`, `:282`; `Mods/SML/Source/SML/Private/SessionSettings/SessionSettingsSubsystem.cpp:115` |
| `FString::Contains(FString)` y `RightChop` | `Mods/Alpakit/Source/Alpakit/Private/AlpakitModEntryList.cpp:293`; `SMLRemoteCallObject.cpp:60` |
| `TWeakObjectPtr::Get()` y la conversión implícita de `TObjectPtr<T>` a `T*` | `Mods/SML/Source/SML/Private/Module/WorldModuleManager.cpp:121`; `Private/Subsystem/SubsystemActorManager.cpp:108` |
| `FDelegateHandle::IsValid()` y `Reset()` y `Remove(handle)` sobre un delegado estático | `Mods/Alpakit/Source/Alpakit/Private/SAlpakitLogTabContent.cpp:13-16` |
| Cada `#include` del plugin aparece en texto del SDK (barrido mecánico sobre `Source/FactoryGame`, `Mods` y, aparte, `Source/FactoryDedicatedServer`): `Misc/EngineVersion.h` (`FGSaveSession.h:11`), `TimerManager.h` (`Private/Configuration/ConfigManager.cpp:5`), `Net/UnrealNetwork.h` (`FGProjectAssembly.h:9`), `UnrealEngine.h` (`PlayerListCommandInstance.cpp:5`), `Engine/World.h` (`FGGameMode.h:5`), `Modules/ModuleManager.h` (`FactoryGameModule.h:6`), `GameFramework/PlayerController.h` (`UI/FGJoinableGamesViewModel.h:9`), `UObject/Object.h` (`UI/Message/FGMessageSender.h:7`) | las rutas son de `Source/FactoryGame/Public` salvo que se indique otra |

### Descriptor y empaquetado (S1, E5)

| Símbolo o hecho | Evidencia |
|---|---|
| `Version` es el primer dígito de `SemVersion`; `VersionName` igual; `GameVersion` solo con rangos mínimos; SML comprueba el rango en ejecución | `Doc: Development/BeginnersGuide/ReleaseMod.adoc:82`, `:86`, `:161`, `:168` |
| La dependencia de SML con `Enabled: true` obligatorio; `^` recomendado | `ReleaseMod.adoc:204`, `:236` |
| `RemoteVersionRange` y `RequiredOnRemote` (por defecto `true`; omitir si no se usan) | `ReleaseMod.adoc:244-263` |
| `GameFeature` lo escribe Alpakit, no va en el origen | `ReleaseMod.adoc:271` |
| Descriptores de referencia del SDK | `Mods/SML/SML.uplugin` (módulo `PostDefault`, `GameVersion >=491125`); `Mods/GameFeatures/ExampleMod/ExampleMod.uplugin` (dependencia `^3.12.0`) |
| La versión de lanzamiento usa `-merge`, y con `-merge` se cocinan los dos clientes de Windows (`FactoryGameEGS` y `FactoryGameSteam`) además de la plataforma `LinuxServer` de `Config/Alpakit.ini` (el nombre del objetivo de compilación del servidor Linux no está en el SDK leído: sin verificar) | `Mods/Alpakit/Source/Alpakit/Private/AlpakitProfile.cpp:89`; `Mods/Alpakit/Source/Alpakit.Automation/PackagePlugin.cs:126-159`; `Mods/Alpakit/Source/Alpakit/Private/ModTargetsConfig.cpp:12-14`, `:28-29` |
| Cada fila de Alpakit Release tiene **dos** iconos de aviso independientes: dependencia de SML distinta de `^<SML del SDK>` (al pulsarlo la iguala) y rango de `GameVersion` distinto del del proyecto (al pulsarlo sube el suelo al CL del SDK); los dos se ocultan si coinciden | `Mods/Alpakit/Source/Alpakit/Private/AlpakitReleaseWidget.cpp:14`, `:117-122`, `:129`, `:137`; `:159`, `:172`, `:179` |
| Alpakit añade el módulo con `Runtime` y `Default` al crear los ficheros C++ | `Mods/Alpakit/Source/Alpakit/Private/AlpakitEditModDialog.cpp:319` |
| El rango objetivo de Alpakit es el del mod más `>= currentVersion.txt`; el botón lo aplica | `Mods/Alpakit/Source/Alpakit/Private/AlpakitReleaseWidget.cpp:303-323`, `:172`; `AlpakitEditModDialog.cpp:96`; `Alpakit.cpp:164` |
| `RequiredOnRemote` verdadero se elimina del descriptor | `Mods/Alpakit/Source/Alpakit/Private/ModMetadataObject.cpp:109` |
| Objetivos en `Config/Alpakit.ini` y lista vacía igual a los tres | `Mods/Alpakit/Source/Alpakit/Private/ModTargetsConfig.cpp:12-14`; `Mods/GameFeatures/ExampleMod/Config/Alpakit.ini` |
| La versión de lanzamiento es Shipping, fusiona los objetivos en un zip y lo deja en `Saved/ArchivedPlugins/<mod>/<mod>.zip` con una carpeta por objetivo | `Alpakit.cpp:228-230`; `Mods/Alpakit/Source/Alpakit.Automation/PackagePlugin.cs:192`, `:204`, `:218` |
| `BuildId` de los `.modules` a `SML` | `PackagePlugin.cs:281` |
| Plantilla de módulo de Alpakit (`ModuleRules`, `ReadOnlyTargetRules`, `TargetRules.TargetType`, `CppStandard` C++20, `PublicDependencyModuleNames`, `DummyHeaders`, `FactoryGame`, `SML`); el `Build.cs` del spike es la plantilla sin los arreglos vacíos de rutas y dependencias | `Mods/Alpakit/Templates/CPPAndBlueprintBlank/Source/PLUGIN_NAME/PLUGIN_NAME.Build.cs:10`, `:56`, `:62` |
| Instalación manual: carpeta `<juego>/FactoryGame/Mods/<nombre>` igual al `.uplugin`; dependencias a mano | `Doc: ManualInstallDirections.adoc:80`, `:88`, `:93` |
| Servidor parado antes de instalar; no copiar la carpeta de mods del cliente | `Doc: ForUsers/DedicatedServerSetup.adoc:317`, `:386` |
| Rutas de los logs | `Doc: faq.adoc:356-358` |
| Rechazo por mods ausentes («Client missing mods» o «Server missing mods») | `Mods/SML/Source/SML/Private/Network/SMLNetworkManager.cpp:191`, `:170` |
| Servidor dedicado: `IsPaused`, `mAutoPause`, `ServerAutoPauseStartupTimeDelay` y `RunCommand` | `Source/FactoryDedicatedServer/Public/FGGameModeDSComponent.h:36`, `:57`, `:43`; `Public/Controller/FGServerManagementController.h:52` |

## 8. Sin verificar

Dos clases de filas. Un **símbolo** que no se pudo confirmar contra el SDK o la documentación (SIN VERIFICAR) no aparece en el código. Un **comportamiento** que solo se puede medir en el PC del propietario sí puede estar en el código, pero siempre como medida o como diagnóstico, nunca como supuesto; la fila dice cuál de los dos casos es y qué línea de log lo resuelve.

| Punto | Estado |
|---|---|
| `FPlatformTime::Seconds()`, `#include "HAL/PlatformTime.h"` | No se usan: no aparecen en el SDK (0 coincidencias en `Source/FactoryGame`, `Mods` y `Source/FactoryDedicatedServer`) ni en la documentación de SML. Una auditoría propuso cambiar a ese reloj monotónico porque lo usa un mod de terceros; se rechaza porque el criterio del spike es no usar nada que no esté en el SDK o en la documentación. El reloj sigue siendo `FDateTime::UtcNow()`, con los límites de S3 |
| Monotonía y resolución de `FDateTime::UtcNow()` | Sin verificar: el SDK no las documenta. Un salto del reloj da un `gap_ms` negativo y se descarta esa ventana; el menor `gap_ms` no nulo del log es el paso real. Afecta a `gap_ms_*` y `factory_span_ms_*`, no a los recuentos por fotograma ni por subpaso |
| `TWeakObjectPtr::IsValid()` y `Reset()` | No se usan (no aparecen en el SDK); se usan `Get()`, el constructor desde puntero y la asignación de un objeto vacío |
| `FAutoConsoleCommand` y la API de registro de órdenes de consola | No se usan: sin ejemplo en el SDK |
| Que las dos variables `slgspike.*` se puedan teclear en la consola interactiva del servidor dedicado del propietario | Sin verificar: la documentación lo afirma de las órdenes de consola en general. Una consola de cliente remoto no puede cambiarlas (el manejador solo existe en el servidor); el chat es la vía principal |
| `GFrameCounter` y `FPlatformTLS::GetCurrentThreadId` | No se usan: no aparecen en el SDK ni en las fuentes consultadas. El hilo se mide con `IsInGameThread()`, y los fotogramas con `GetTimeSeconds` |
| Si `AFGBuildableSubsystem::Get` ya devuelve algo en `BeginPlay` del subsistema | Desconocido; hay reintento cada tick durante 600 intentos |
| Qué valen `mCurrentSubStep` y `mCurrentSubStepMax` dentro de `PreFactoryTick` | Desconocido; se registran en bruto |
| Que un `UCLASS` de un mod pueda implementar `IFGFactoryTickHandlerInterface` (UHT y enlace) | Sin precedente en el SDK; hay uno de código fuente en un mod de terceros (`runtime-api C1`) |
| Que `AFGBuildableSubsystem` conserve nuestro registro: que no vacíe ni reconstruya `mFactoryTickHandlers` después de nuestro `BeginPlay` | Desconocido (el miembro es `UPROPERTY(Transient)`, `FGBuildableSubsystem.h:835-836`, y los `.cpp` del SDK están vacíos). `HandlerSilent` y `HandlerReRegistered` lo distinguen |
| `RemoveFactoryTickHandler` con un puntero que ya no está en la lista | Desconocido; el reregistro de `HandlerSilent` hace `Remove` y después `Add` para no duplicar la entrada. Si `Remove` fallara, el log lo mostraría justo después de `HandlerSilent` |
| `NewObject<USlgSpikeTickHandler>(Mundo)`: el mundo como `Outer` | Precedente solo de código fuente de un mod de terceros (`runtime-api C1`); un actor guardado como `Outer` no tiene precedente en el SDK, y por eso el código usa el mundo |
| Que el binario real exporte `AddFactoryTickHandler` y `RemoveFactoryTickHandler` (los `.cpp` del SDK son esqueletos vacíos) | Desconocido. Un símbolo importado que el juego no exporte puede impedir la carga de la biblioteca (**inferencia**), lo que se vería en S1 |
| El orden de `PreLoadGame`, `PostLoadGame`, `Init` y `BeginPlay`, y que `IFGSaveInterface` se llame para un `AModSubsystem` | Lo mide S2 |
| Qué efecto tiene `SetSwitchOn` fuera de `OnUse`, y su autoridad | Lo mide S4 (`runtime-api C3`, `C5`, `C23`) |
| Que `ChatMessageEntered` se dispare para un prefijo distinto de `/`, que el enganche llegue antes que el primer controlador, que el RCO esté ya en el cliente en el primer mensaje y que `ShouldRegisterRemoteCallObject` (`FGRemoteCallObject.h:42`, cuerpo oculto en el binario) deje registrarlo | Lo mide S5; SML solo demuestra `/`, y registra los RCO en `GameModeInitialized` (`RemoteCallObjectRegistry.cpp:18-31`) |
| Que un `AModSubsystem` con `Tick` y `bTickEvenWhenPaused` siga latiendo en un servidor dedicado en pausa | Lo mide S6 |
| Empaquetar un plugin con `CanContainContent` verdadero y sin carpeta `Content` | Desconocido; la plantilla C++ de Alpakit tampoco trae `Content/`, y el plugin de presencia del propietario es solo C++ y empaqueta, pero no se pudo leer. Si el cocinado se queja de contenido, probar `CanContainContent: false` |
| `LoadingPhase` `PostDefault` frente a `Default` (el asistente de Alpakit escribe `Default`; SML y un mod publicado usan `PostDefault`) | Se eligió `PostDefault`; si S1 falla por orden de carga, probar `Default` |
| Que la primera compilación tras copiar el plugin sea larga aunque `AccessTransformers.ini` esté vacío | Inferencia de `AccessTransformers.adoc:154` (el generador decide reconstruir por la fecha de cambio del fichero, y `Copy-Item` le da una fecha nueva); no está medido |
| `RunCommand` de la API HTTPS del servidor | La cabecera lo declara («runs a console command on the server», `FGServerManagementController.h:52`) con el cuerpo vacío; el spike no lo usa |
| Que el servidor local use el CL 502094 | Hoy una instalación nueva baja 513885; S1 imprime el CL real |
| Compilación del código con MSVC 14.38 y la configuración de UE 5.6.1 | No se pudo compilar aquí; solo se comprobó, con un barrido mecánico, que cada formato de log tiene tantos argumentos como especificadores, que cada `#include` aparece en texto del SDK y que cada identificador del SDK que usa el código está en la tabla de la sección 7 |
