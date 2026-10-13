# 0001. Decisiones iniciales (13 de octubre de 2026)

Estado: **aceptadas por el propietario**, salvo donde se indica "provisional".

## 1. Repositorio aparte y privado hasta la v0.1

- **Motivo:** la idea nació en un repositorio privado de otro proyecto, con su propia infraestructura, despliegues
  y reglas; un mod público no debe convivir con eso ni con su CI.
- **Alternativas descartadas:** repo público desde el principio (más visibilidad, pero el contenido todavía no
  estaba auditado); rama independiente en el repo de ese otro proyecto. El 13-oct-2026 el propietario autorizó
  hacerlo público cuando haga falta (véase §5).

## 2. Señal de 4 bits (0–15) — provisional

- **Motivo:** coincide con la fuerza de señal de Redstone y con un dígito hexadecimal; el 0/1 puro es el caso
  particular y un bus de 4 bits cabe en un cable.
- **Alternativas:** binario puro de 1 bit; enteros grandes estilo Factorio (más potentes para fábricas, menos
  "puertas lógicas" clásicas).
- **Pendiente:** contrastar con la investigación de Redstone y Factorio antes de fijar el modelo formal.

## 3. Núcleo y teoría primero, Unreal después

- **Motivo:** el núcleo sin Unreal es lo único que se puede compilar y probar de verdad en Linux
  (`g++ -Wall -Wextra -Werror` con sanitizers); el adaptador necesita el motor restringido de Windows.

## 4. Mod propio diferenciado pese a Circuitry

- **Hecho comprobado el 13-oct-2026** en la API de ficsit.app: Circuitry (`FicsitWiremod`) tiene 195 480
  descargas, 3 568 673 vistas, creado el 15-jul-2022, última versión 14.1.63 del 26-sep-2026 para
  `>=502094`, con objetivos Windows, WindowsServer y LinuxServer.
- **Diferenciación prevista (inferencia por confirmar en el juego):** modelo determinista de tiempo discreto con
  retardo, señales 0–15, licencia permisiva y teoría completa.
- **Restricción de licencias:** Circuitry no tiene licencia (todos los derechos reservados por defecto) y
  FicsIt-Networks es GPL-3.0; el diseño es desde cero y no se copia ni se enlaza código suyo.

## 5. Web en GitHub Pages del propio repo

- **Restricción verificada en la documentación de GitHub:** con el plan Free, Pages solo funciona en repos
  públicos; con Pro, Team o Enterprise también en privados, pero el sitio publicado es público en internet aunque
  el repo sea privado ([qué es Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages),
  [visibilidad](https://docs.github.com/en/pages/getting-started-with-github-pages/changing-the-visibility-of-your-github-pages-site)).
- **Decisión:** la web se desarrolla desde el primer día en este repo. El propietario dijo el 13-oct-2026 que, si
  Pages necesita que el repo sea público, **se puede hacer público sin problema**. Es un permiso condicional, no
  una orden: antes de cambiar la visibilidad se hace una auditoría de publicación (secretos, datos personales o
  de infraestructura, rutas locales, licencias de terceros, aviso de "no afiliado", declaración de uso de IA) y se
  pide la confirmación expresa del cambio. Mientras tanto el repo sigue privado.

## 6. Pruebas

- Se automatiza todo lo automatizable: el núcleo con pruebas unitarias y de propiedades en CI.
- El empaquetado y la carga en el juego requieren el PC Windows del propietario. No se arranca un servidor de
  juego de pruebas en el VPS de desarrollo, que es compartido, no tiene swap y tuvo OOM del kernel el 9 y el 13
  de octubre de 2026: esas pruebas van al PC del propietario o a CI.
- El propietario hace la pasada final dentro del juego (colocar cables, interfaz, aspecto).

## 7. Transparencia sobre IA

- ficsit.app exige declarar el uso de IA en el código y no permite deshacer esa declaración una vez hecha; se
  declarará de forma veraz al publicar.
