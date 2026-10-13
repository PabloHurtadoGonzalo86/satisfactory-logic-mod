# Spike: procedimiento para el propietario

Estado: borrador del 13-oct-2026. Está pensado para quien ya construyó un plugin C++ con esta toolchain (Editor, Alpakit para Windows y LinuxServer, instalación por copia de ficheros), así que no repite lo básico. Qué se mide y con qué criterio de aceptación está en [01](01-objetivos-y-criterios.md); este documento solo dice qué hacer y qué devolver. Nada de lo de aquí se ha podido ejecutar: si un paso no se parece a lo que ves, **para y cuéntalo**, no improvises.

Los pasos de PowerShell van de uno en uno y sin rutas fijas: `<RAIZ_SDK>`, `<RAIZ_JUEGO>`, `<RAIZ_SERVIDOR>`, `<REPO>` y `<RESULTADOS>` son carpetas tuyas (`<RAIZ_SDK>` es la carpeta que contiene `FactoryGame.uproject`; `<REPO>`, la copia de este repositorio; `<RESULTADOS>`, una carpeta fuera de `<REPO>` donde guardas los logs).

## 0. Antes de empezar

### 0.1 Reglas

1. **Nunca en el servidor de producción**, ni en su copia, ni en un volumen compartido con él. Solo un servidor dedicado local de prueba y un cliente de prueba.
2. Siempre sobre **copias** de los guardados.
3. Cliente y servidor de prueba deben estar en el **mismo CL** o el juego no deja conectar. Anota los dos: el log del spike los imprime (`changelist=`). Una instalación nueva de servidor baja hoy el CL 513885, no el 502094; si te sale así, sigue, pero marca cada resultado con ese CL (un mod compilado con las cabeceras de 502094 puede tener diferencias de disposición en 513885, `versions C14`).
4. **No subas el plugin ni los logs al repositorio** (es público) ni a ficsit.app. Un log de FactoryGame puede llevar nombres de jugador y direcciones.
5. Un check cada vez. Cuando un paso diga «cierra el servidor», para el proceso del todo; no basta con volver a cargar el mundo.

### 0.2 Qué necesitas

| Cosa | Detalle |
|---|---|
| SDK | El Starter Project en el commit `d2162c999bbaa9650594ee9ad6e6e06190306009` (rama `master` de SML, CL 502094, Wwise 2023.1.14), el mismo con el que compilas hoy. Si tu SDK está en `dev` (CL 513885, Wwise 2025.1.6), **para**: este procedimiento no vale ahí. |
| Toolchain | La de CompanionPresence: Visual Studio 2022 con MSVC v14.38, motor 5.6.1-css-83, Alpakit en el editor. |
| SML | 3.12.0 en la carpeta `Mods` del cliente y del servidor de prueba. |
| Servidor de prueba | Un servidor dedicado **LinuxServer** local, distinto del de producción. Cómo lo montes (otra máquina, WSL2, una VM o un contenedor) es cosa tuya; solo importa saber dónde está su carpeta `FactoryGame/Mods` y su log. |
| Guardados | Uno de mundo nuevo (vacío) y una copia de una fábrica real, con al menos un interruptor de energía vanilla entre un generador y un consumidor. |
| Opcional | Un segundo cliente (para repetir S4 y S5 desde un jugador que no es el anfitrión). |

### 0.3 La trampa del GameVersion

SML compara el CL del proceso con el `GameVersion` de cada plugin. Si no coincide, **se aborta todo el servidor** (`Found mismatched dependencies versions in the environment. Loading cannot continue`), no solo el plugin. El descriptor del spike dice `>=502094`: es un suelo, y 502094 es el CL de tu SDK y de tu servidor. Alpakit enseña **dos** iconos de aviso independientes en la fila del mod: uno cuando la dependencia de SML del descriptor no es la de tu SDK, y otro cuando el rango de `GameVersion` no es el del proyecto. Al pulsar el segundo **sube el suelo al CL del SDK**. Con tu SDK en el pin los dos quedan ocultos (SML `^3.12.0` y `>=502094` ya son los valores del proyecto); con un SDK en `dev` el segundo lo subiría a 513885 y un servidor en 502094 dejaría de arrancar.

Regla: **si ves cualquiera de los dos iconos de aviso junto a `SlgSpike`, no lo pulses y para**: el SDK no está donde se cree. Antes de instalar, abre el `.uplugin` empaquetado y comprueba que dice `>=502094` (paso 3.8).

### 0.4 Reglas exactas del descriptor (SMR y Alpakit)

El spike no se publica, pero su descriptor sigue las mismas reglas para que, si algo falla, no sea por él. Detalle y fuentes en [publishing-and-policy](../research/publishing-and-policy.md) (c6, c7, c8, c9, c11) y [versions](../research/versions.md) (C7, C9, C10).

| Regla | Valor en `SlgSpike.uplugin` |
|---|---|
| `Version` es el primer número de `SemVersion` y `VersionName` es igual a `SemVersion` (SMR rechaza el descriptor si no). Un mayor `0` hace que el rango `^` se comporte de forma poco intuitiva y la documentación pide evitarlo; aquí se acepta porque el plugin no se publica y nada depende de él (si un plugin posterior del spike dependiera de este, se usaría un rango explícito) | `0`, `0.1.0`, `0.1.0` |
| `GameVersion` solo con rangos mínimos, nunca por encima del CL del servidor | `>=502094` |
| SML en `Plugins`, el primero de la lista, con `Enabled: true` (obligatorio) y rango `^3.12.0` | así |
| El módulo C++ declarado en `Modules` (sin él Alpakit no compila el plugin) | `SlgSpike`, `Runtime`, `PostDefault` |
| `RequiredOnRemote` y `RemoteVersionRange`: omitir si no se usan. Por defecto el mod exige la misma versión en los dos lados, y un cliente sin él es rechazado con `Client missing mods` | omitidos |
| `GameFeature`: no va en el código fuente; lo escribe Alpakit solo si el plugin está bajo `Mods/GameFeatures` | omitido; el plugin va bajo `Mods/` |
| El zip de lanzamiento solo puede tener en su raíz carpetas `Windows`, `WindowsServer` y `LinuxServer`, cada una con el `.uplugin` **idéntico byte a byte**; el archivo debe pesar menos de 1 GB | dos carpetas: `Windows` y `LinuxServer` |
| Cada subida a SMR gasta un número de versión que no se puede reutilizar (máximo 5 por 24 h), exige declarar el uso de IA y de red, y la referencia del mod es inmutable | no aplica: **SlgSpike no se sube** y su referencia no se reserva |

## 1. Comprobar que el SDK está en el pin

1.1. Mira el CL del SDK. Debe salir `502094`.

```powershell
Get-Content "<RAIZ_SDK>\Source\FactoryGame\currentVersion.txt"
```

1.2. Mira el commit del SDK. Debe salir `d2162c999bbaa9650594ee9ad6e6e06190306009`.

```powershell
git -C "<RAIZ_SDK>" rev-parse HEAD
```

1.3. Mira la versión de SML del SDK. Debe salir `3.12.0`.

```powershell
Select-String -Path "<RAIZ_SDK>\Mods\SML\SML.uplugin" -Pattern "SemVersion"
```

Si cualquiera difiere, para y dímelo (con la salida literal).

## 2. Copiar el plugin y compilarlo

2.1. Copia la carpeta del plugin dentro de la carpeta `Mods` del SDK. Debe quedar `<RAIZ_SDK>\Mods\SlgSpike\SlgSpike.uplugin`. Si `<RAIZ_SDK>\Mods\SlgSpike` ya existe, bórrala antes: con el destino existente, `Copy-Item` anida la carpeta dentro de la otra y el `.uplugin` queda un nivel más abajo.

```powershell
Copy-Item -Recurse -Path "<REPO>\mod\SlgSpike" -Destination "<RAIZ_SDK>\Mods\SlgSpike"
```

2.2. Comprueba la estructura. Deben salir, entre otros, `SlgSpike.uplugin`, `Config\AccessTransformers.ini`, `Config\Alpakit.ini` y catorce ficheros bajo `Source\SlgSpike` (el `Build.cs`, siete cabeceras y seis `.cpp`).

```powershell
Get-ChildItem "<RAIZ_SDK>\Mods\SlgSpike" -Recurse -File | Select-Object -ExpandProperty FullName
```

2.3. Regenera los ficheros de proyecto de Visual Studio como hiciste con CompanionPresence (clic derecho sobre `FactoryGame.uproject`, o la línea de comandos que ya usas).

2.4. Compila el mismo objetivo que usas para el editor (Development Editor, Win64). La primera compilación tras copiar el plugin **puede** ser larga aunque `Config\AccessTransformers.ini` esté vacío: el generador de cabeceras decide reconstruir por la fecha de cambio de ese fichero y la copia le da una fecha nueva (inferencia de la documentación, no medida). No lo edites para acortarla.

Lo que no se pudo comprobar aquí es la compilación. Si falla, **copia los primeros 30 errores literales** (de UHT o de UBT) sin interpretarlos. Los sospechosos esperables, por orden de probabilidad:

| Síntoma | Causa probable |
|---|---|
| UHT se queja de la interfaz de `USlgSpikeTickHandler` o de `ASlgSpikeSubsystem` | primera vez que un plugin implementa `IFGFactoryTickHandlerInterface` (interfaz `MinimalAPI`); pasa el error tal cual |
| Falta una cabecera (`UnrealEngine.h`, `TimerManager.h`, `Misc/DateTime.h`) | el módulo `Engine` o `Core` no está en la lista de dependencias; la plantilla de Alpakit ya los incluye, así que revisa que `SlgSpike.Build.cs` es el copiado |
| Un símbolo sin resolver en el enlace (`AddFactoryTickHandler`, `GetCurrentSubStepIteration`...) | el juego no exporta ese símbolo en este CL; es dato valioso para S3 y se cuenta en «sin verificar» |
| `TObjectPtr` o punteros sin procesar | el SDK ya exige `TObjectPtr` en miembros UPROPERTY; el código lo cumple, así que repórtalo |

2.5. **No añadas nada a `Config\AccessTransformers.ini`** para hacer que compile: el fichero está vacío a propósito y cualquier cambio fuerza reconstruir el SDK entero. Si crees que hace falta una entrada, para y dímelo.

## 3. Empaquetar con Alpakit (versión de lanzamiento)

Se usa la versión de lanzamiento porque es la que produce el zip multiobjetivo que SMR validaría y porque compila en Shipping, igual que el juego.

3.1. En el editor abre `File > Alpakit Release` (o el botón de la alpaca).

3.2. En la lista, busca `SlgSpike`. **Si hay cualquiera de los dos iconos de aviso junto a él, para** (sección 0.3).

3.3. En «Release Targets» deja marcados **Windows** y **Linux Server**, y sin marcar **Windows Server** (es lo que dice `Config\Alpakit.ini`, y el que tu instalación de Alpakit construye).

3.4. Pulsa «Alpakit!» en la fila de `SlgSpike`. La versión de lanzamiento compila en Shipping y, al fusionar los objetivos, cocina los dos clientes de Windows (`FactoryGameEGS` y `FactoryGameSteam`) además de `LinuxServer`; la primera vez tarda bastante más de lo que sugiere «dos objetivos». Después pulsa «Open Archive Folder». El zip es `<RAIZ_SDK>\Saved\ArchivedPlugins\SlgSpike\SlgSpike.zip`.

3.5. Comprueba el zip. Descomprímelo en una carpeta temporal tuya.

```powershell
Expand-Archive -Path "<RAIZ_SDK>\Saved\ArchivedPlugins\SlgSpike\SlgSpike.zip" -DestinationPath "<RESULTADOS>\SlgSpike-zip"
```

3.6. Mira las carpetas de la raíz. Deben ser exactamente `Windows` y `LinuxServer`.

```powershell
Get-ChildItem "<RESULTADOS>\SlgSpike-zip" | Select-Object -ExpandProperty Name
```

3.7. Compara los dos descriptores. Los dos hashes deben ser **iguales**.

```powershell
(Get-FileHash "<RESULTADOS>\SlgSpike-zip\Windows\SlgSpike.uplugin").Hash
```

```powershell
(Get-FileHash "<RESULTADOS>\SlgSpike-zip\LinuxServer\SlgSpike.uplugin").Hash
```

3.8. Mira el suelo de versión del descriptor empaquetado. Debe decir `>=502094`.

```powershell
Select-String -Path "<RESULTADOS>\SlgSpike-zip\LinuxServer\SlgSpike.uplugin" -Pattern "GameVersion"
```

3.9. Comprueba que hay una biblioteca por objetivo (un `.dll` en `Windows`, un `.so` en `LinuxServer`). Anota las rutas exactas que salgan.

```powershell
Get-ChildItem "<RESULTADOS>\SlgSpike-zip" -Recurse -Include *.dll,*.so | Select-Object -ExpandProperty FullName
```

Si en algún paso difiere, para y cuéntalo con la salida literal.

## 4. Instalar

4.1. **Cliente.** Copia el **contenido** de `<RESULTADOS>\SlgSpike-zip\Windows` dentro de `<RAIZ_JUEGO>\FactoryGame\Mods\SlgSpike\` (la carpeta se llama exactamente como el `.uplugin`; si ya existe, bórrala antes por la misma razón que en 2.1). Junto a ella debe haber `<RAIZ_JUEGO>\FactoryGame\Mods\SML` con SML 3.12.0.

```powershell
Copy-Item -Recurse -Path "<RESULTADOS>\SlgSpike-zip\Windows" -Destination "<RAIZ_JUEGO>\FactoryGame\Mods\SlgSpike"
```

4.2. **Servidor de prueba.** Con el servidor **parado**, copia el contenido de `<RESULTADOS>\SlgSpike-zip\LinuxServer` en `<RAIZ_SERVIDOR>/FactoryGame/Mods/SlgSpike/`. No copies la carpeta de mods del cliente (no sirve: sus binarios son de Windows). SML 3.12.0 también tiene que estar en `<RAIZ_SERVIDOR>/FactoryGame/Mods/SML`.

4.3. Desactiva **Auto Pause** en el servidor de prueba hasta el check S6.

4.4. Arranca el servidor. Cuando termine de cargar el mundo, comprueba que el log tiene la línea de arranque del spike. Si no está, o hay un `Fatal`, ve a la sección 7.

```bash
grep -F "[SLGSPIKE] S1 event=StartupModule" "<RAIZ_SERVIDOR>/FactoryGame/Saved/Logs/FactoryGame.log"
```

El servidor rota el log al arrancar (según la wiki oficial de servidores dedicados, el vigente se llama `FactoryGame.log`): **copia el log antes de cada reinicio** que vayas a necesitar después.

## 5. Ejecutar los checks

Orden: S1, S2, S3 (mundo vacío), S4, S5, S3 (fábrica cargada), S6, S7. Para cada uno: pasos, líneas esperadas y qué guardar. En el chat del juego los comandos empiezan por `/slgspike`; los del cliente, por `!slg`. Los criterios exactos de pasa y falla están en [01](01-objetivos-y-criterios.md); aquí solo lo que tienes que ver.

### S1. El plugin carga

1. Cliente: arranca el juego hasta el menú principal. En `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log` debe haber una línea `[SLGSPIKE] S1 event=StartupModule ... dedicated_server=0`.
2. Cliente: en la consola del juego (tecla `~`) escribe `TestPluginModuleLoader`. SML lista los módulos raíz que encuentra, con el plugin al que los asigna; deben salir los dos de `SlgSpike` (la lista sale en la propia consola; si no la ves, busca `Found` y `plugin root modules` en el log).
3. Conéctate al servidor de prueba (carga el guardado de mundo vacío). Debe entrar sin mensaje `missing mods`.
4. En el chat escribe `/slgspike status`. Deben llegar tres líneas (S1, S2 y S3).
5. En el servidor debe haber `S1 event=StartupModule ... dedicated_server=1`, tres líneas `ModuleLifecycle module=world`, tres `module=game_instance` (el `netmode` de estas puede ser `no_world`) y las de SML `Registered subsystem class` y `Registered chat command SlgSpike:slgspike`.

Guarda: las líneas `[SLGSPIKE] S1` de los dos lados, las de SML `Game Changelist`, `Discovered`, `Registered subsystem class` y `Registered chat command`, y la salida de `/slgspike status`.

### S2. Guardado

1. En el servidor con el mundo nuevo, `/slgspike status`. Apunta `begin_play_count`, `load_count`, `save_count` y `spike_id`.
2. Guarda la partida (el autoguardado o el guardado desde el gestor del servidor). Debe verse `S2 event=PreSaveGame` y `PostSaveGame` en el log.
3. **Copia el log**, cierra el servidor del todo, arráncalo y carga ese guardado. Conéctate y haz `/slgspike status`: `load_count` debe valer 1, `spike_id` lo mismo de antes.
4. Repite 2 y 3 una vez más: `load_count` debe valer 2.

Guarda: las líneas `[SLGSPIKE] S2` completas (llevan `ord` y los contadores) de los tres arranques.

### S3. El manejador del tick de fábrica

Con **Auto Pause apagado**, un solo jugador conectado y quieto. Haz dos pasadas: con el mundo vacío y con la copia de tu fábrica real.

1. Tras entrar, espera hasta 30 s: debe haber `S3 event=HandlerRegistered result=OK` y unas 120 líneas `S3 phase=pre` y `phase=post` (el registro automático inicial; con una fábrica cargada el manejador puede llamarse más despacio y la ráfaga tardar más). **No** debe aparecer `S3 event=HandlerSilent`: si aparece, copia el log entero a ese punto, es un resultado y no un error tuyo.
2. `/slgspike reset`. Espera 60 s sin tocar nada. Salen unos seis resúmenes (`S3 event=Summary`).
3. `/slgspike burst 300`. Espera 15 s. Salen unas 300 líneas `pre` y otras tantas `post`.
4. `/slgspike ticks 200`. Vuelca al log las últimas 200 llamadas.
5. No guardes durante la medida (un guardado detiene el tick): si el autoguardado cae en la ventana, anota la hora y repite. Tampoco lances órdenes `/slgspike switch ...` dentro de la medida: recorren todos los edificios y distorsionarían los tiempos.
6. Si puedes, apunta a la vez el `averageTickRate` que ve tu panel o tu consulta `QueryServerState` (contexto de la medida).
7. Al final, cierra el servidor con normalidad. Debe salir `S3 event=HandlerUnregistered result=OK` y ningún `SIGSEGV`, `Fatal` ni `ensure`. (Opcional) en una sesión de anfitrión, volver al menú principal debería dar la misma línea.
8. Carga otro guardado en el mismo proceso (opcional, solo en una sesión de anfitrión) y comprueba que el manejador se registra de nuevo.

Guarda: de cada pasada, **todas** las líneas `[SLGSPIKE] S3` (resúmenes, ráfaga y volcado). Si un resumen trae `gap_ms_min` negativo, el reloj de pared saltó en esa ventana: se descarta y se repite.

### S4. Interruptor y circuito de energía

Carga la copia de tu fábrica, con un generador activo, un interruptor de energía vanilla y un consumidor al otro lado.

1. `/slgspike switch list`. Deben salir una línea `S4 event=List switches=N` y una `S4 phase=list` por interruptor, con `circuit_id0` y `circuit_id1` mayores o iguales que cero. Apunta el nombre de tu interruptor (algo como `Build_PowerSwitch_C_...`).
2. `/slgspike switch <nombre> read`. Compara `has_power` de los dos lados con lo que ves.
3. `/slgspike switch <nombre> off`. En el juego el interruptor debe verse apagado y el consumidor perder la energía. En el log, 4 s de líneas `phase=watch`.
4. `/slgspike switch <nombre> on`. Recupera la energía.
5. Repite 3 y 4 desde un segundo cliente que no sea el anfitrión (si lo tienes).
6. En lugar del nombre vale `#n`, el índice que imprime `switch list` (`/slgspike switch #0 off`), o una parte única del nombre. Si no es única o no existe, el log dice `ambiguous` o `not_found` y no se toca nada.
7. Vía de consola (opcional, **no contra el servidor dedicado desde un cliente**): una variable de consola solo cambia en el proceso donde la escribes, y el manejador solo existe en el proceso del lado servidor. Escribirla en la consola (`~`) de un cliente que se ha conectado al servidor de prueba **no hace nada**. Solo vale en la consola de una sesión de anfitrión (donde el anfitrión es el servidor) y, sin verificar, en la consola interactiva del propio servidor dedicado (la documentación de SML dice que admite órdenes de consola, sin hablar de variables de mods). Ahí escribe `slgspike.SwitchIndex 0` y luego `slgspike.SwitchAction 4` (apaga) o `5` (conmuta): la acción se ejecuta en la siguiente llamada del manejador y deja una línea `S4 event=ConsoleTrigger`. Pon `slgspike.SwitchAction 0` antes de repetir la misma acción. Si la consola del servidor no admite la variable, dilo: la vía del chat es la que vale para el servidor dedicado.

Guarda: todas las líneas `[SLGSPIKE] S4` de las dos operaciones y, aparte, qué viste en pantalla y a los cuántos segundos. Los milisegundos del log salen de un reloj de pared (`FDateTime::UtcNow()`) cuya resolución no está verificada: el menor `gap_ms` no nulo del log es su paso real, y no se lee nada por debajo.

### S5. Remote Call Object

Desde el cliente, con el servidor de prueba.

1. En el chat: `!slg status`. Debe decir `rco_available=1` en el log del cliente. (El mensaje no se envía a los demás jugadores.) Si dice `rco_available=0` justo después de conectar, espera 5 s y repite antes de dar el resultado por malo.
2. `!slg ping hola`, tres veces seguidas (solo se envía la primera palabra tras `ping`).
3. En el servidor, `/slgspike rco`: debe decir `rco_available=1` y `pings_received=3`.
4. Desconéctate, vuelve a conectarte y repite el ping.

Guarda: las líneas `[SLGSPIKE] S5` del cliente y del servidor, con las marcas de tiempo del log (sirven para medir la ida y vuelta).

### S6. Auto Pause

1. Activa **Auto Pause** en el servidor de prueba. Reinicia. Espera con un jugador conectado al menos 5 minutos (el servidor tiene un retardo de arranque antes de poder pausarse).
2. Anota la hora. `/slgspike hb on` (el latido está encendido por defecto, pero se confirma).
3. Quédate 60 s. Desconéctate del todo y espera 180 s sin ningún jugador.
4. Vuelve a conectarte. En cuanto estés dentro, `/slgspike ticks 20`.
5. Copia el log.

Si aparece `S3 event=HandlerSilent` en este check (solo se escribe con un jugador conectado), no lo cuentes como fallo de registro: anótalo y repite S3 con Auto Pause apagado.

Guarda: las líneas `S6 event=Heartbeat` y `S3` de todo el intervalo, y la hora de cada paso. Si el servidor no hace pausa aunque la opción esté activa, dilo: es un resultado válido (caso B).

### S7. Qué persiste al quitar el mod

Trabaja con **copias**. Necesitas el guardado hecho en S4 con el interruptor apagado por el spike.

1. Guarda la partida con el interruptor apagado y contadores de S2 mayores que cero. Copia ese fichero como `A`.
2. Cierra servidor y cliente. **Quita `SlgSpike` del servidor y del cliente.** (Si lo quitas solo de uno, el otro cierra la conexión con `Client missing mods` o `Server missing mods`: eso es el control negativo; copia esa línea y sigue quitándolo del otro lado.)
3. Arranca el servidor con una copia de `A` y entra. Debe cargar como juego base. El interruptor debe seguir **apagado**. En el log del servidor no debe haber ninguna línea `[SLGSPIKE]`.
4. Guarda ahí (copia `B`), cierra, **reinstala** el mod en los dos lados (sección 4) y carga `B`. `/slgspike status`: el contador debe haber vuelto a empezar (`load_count` pequeño).

Guarda: las líneas del log del paso 3 que hablen de clases o activos desconocidos, y el estado final del interruptor.

## 6. Recoger y devolver los logs

Dónde están: el cliente, en `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`; el servidor, en `<RAIZ_SERVIDOR>/FactoryGame/Saved/Logs/FactoryGame.log`.

6.1. Copia el log del cliente y filtra la copia (el log vivo puede estar bloqueado mientras el juego corre, y se rota al arrancar). Un fichero por check; el nombre te dice de qué es:

```powershell
Copy-Item "$env:LOCALAPPDATA\FactoryGame\Saved\Logs\FactoryGame.log" "<RESULTADOS>\copia-cliente.log"
```

```powershell
Select-String -Path "<RESULTADOS>\copia-cliente.log" -SimpleMatch "[SLGSPIKE]" | ForEach-Object { $_.Line } | Set-Content -Path "<RESULTADOS>\s3-vacio-cliente.log"
```

6.2. Lo mismo en el servidor:

```bash
grep -F "[SLGSPIKE]" "<RAIZ_SERVIDOR>/FactoryGame/Saved/Logs/FactoryGame.log" > "<RESULTADOS>/s3-vacio-servidor.log"
```

6.3. Las líneas de SML que sirven de contexto (CL, módulos, subsistemas y comandos) y cualquier error de carga:

```bash
grep -E "Game Changelist|Discovered [0-9]+ (world|game instance) modules|Registered subsystem class|Registered chat command|mismatched dependencies|requires game version|root module|missing mods|SIGSEGV|Fatal" "<RAIZ_SERVIDOR>/FactoryGame/Saved/Logs/FactoryGame.log" > "<RESULTADOS>/contexto-servidor.log"
```

6.4. Nombra los ficheros `s1-...`, `s2-...`, `s3-vacio-...`, `s3-fabrica-...`, `s4-...`, `s5-...`, `s6-...`, `s7-...` con el sufijo `cliente` o `servidor`. Antes de compartirlos, mira que no llevan nada que no quieras publicar (nombres de jugador, direcciones). **No los subas al repositorio.**

6.5. Envíalos por el mismo canal por el que recibiste este procedimiento. Si un fichero pesa más de unos 300 KB, manda solo las líneas `S3 event=Summary`, las `S3 phase=...` de la ráfaga y las de los otros checks, no el log entero.

6.6. Junto a los ficheros, rellena esta tabla (una fila por check; los criterios están en [01](01-objetivos-y-criterios.md)):

```text
Check | Veredicto (PASA / FALLA / INCONCLUSO) | CL cliente | CL servidor | Qué viste en pantalla | Observaciones
S1    |                                       |            |             |                       |
S2    |
S3    |
S4    |
S5    |
S6    |
S7    |
```

Para S3, si el manejador solo recibió llamadas después de `HandlerSilent` y `HandlerReRegistered`, escribe «PASA (reregistro)» en la columna del veredicto (criterio en [01](01-objetivos-y-criterios.md), S3).

Además: la versión exacta de SML en cada lado, si el servidor de prueba es físico, WSL2, VM o contenedor, y si usaste Alpakit Release o Development.

## 7. Si algo sale mal

| Síntoma | Qué mirar |
|---|---|
| El servidor aborta al arrancar con `Found mismatched dependencies versions in the environment. Loading cannot continue` | Mira el texto que sigue: `Plugin SlgSpike requires game version ... (current: N)` es el suelo (sección 0.3); `Plugin SlgSpike requires SML version matching ...` es SML. Revisa el descriptor empaquetado |
| No hay ninguna línea `[SLGSPIKE]` | El plugin no se cargó: ¿la carpeta es `Mods/SlgSpike` y contiene `SlgSpike.uplugin`?, ¿existe el binario del objetivo?, ¿es SML 3.12.0? Busca `LogPluginManager` y `SlgSpike` en el log |
| Hay `StartupModule` pero no `ModuleLifecycle` | SML no descubrió el módulo raíz: busca `Failed to determine owning plugin for root module` y `Only one root module is allowed` |
| `/slgspike` dice «Unknown command» | El comando no se registró: busca `Registered chat command SlgSpike`; si falta, mira si el mundo cargó antes de que SML registrase comandos |
| `S3 event=HandlerRegistration result=WAITING` que no termina | `AFGBuildableSubsystem::Get` no devuelve nada en el mundo de la prueba; manda el log |
| `S3 event=HandlerSilent` seguido de `HandlerReRegistered` | El manejador se registró pero no recibió llamadas con un jugador conectado; el spike lo registró de nuevo una vez. Mira si las líneas `phase=pre` empiezan después de `HandlerReRegistered`: es un resultado de diseño, no tu fallo. Manda el log |
| `rco_not_available` en el cliente | El RCO no se registró o la réplica aún no llegó: espera 5 s y repite; si persiste, vuelve al menú y reconecta; si sigue, manda el log |
| S1 falla por orden de carga (el módulo se carga antes de que existan los subsistemas de SML) | Sin verificar: el descriptor usa `LoadingPhase` `PostDefault` (como SML); prueba `Default` (lo que escribe el asistente de Alpakit) y repite S1 |
| El cocinado o el empaquetado se queja de contenido que falta | Sin verificar: el descriptor lleva `CanContainContent: true` y el plugin no tiene carpeta `Content`; prueba `false` (el módulo C++ no depende de ello) y manda el mensaje literal |
| El cliente es rechazado al conectar | `Client missing mods: ...` o `Server missing mods: ...`: el mod está en un solo lado o las versiones difieren |
| Cliente y servidor no conectan por versión del juego | CL distinto; anota los dos, es información del spike |
| Caída del servidor al apagar | Guarda el log completo (no solo las líneas del spike) y la parte de la pila; es justo lo que S3 quiere saber |

## 8. Cuando termines

Quita `SlgSpike` del cliente y del servidor de prueba y, en el SDK, la carpeta `<RAIZ_SDK>\Mods\SlgSpike` si ya no la necesitas. No dejes el plugin instalado en ningún servidor compartido: escribe una línea por segundo en el log y registra un manejador en cada mundo.
