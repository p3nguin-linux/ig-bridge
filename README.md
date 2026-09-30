# IG Bridge — Instagram y Facebook para Ubuntu Touch (C++ / Qt / QML)

App nativa (ejecutable C++ + interfaz QML de Lomiri) que muestra Instagram web
y Facebook web con QtWebEngine y un "puente" C++ que controla lo que la web puede hacer.

## Instagram + Facebook en una sola app (v0.2.0)
- Selector **Instagram | Facebook** en la barra superior.
- Cada servicio tiene su propio perfil de navegador (cookies, sesión y caché separadas),
  por eso no se mezclan ni entran en conflicto. Facebook se carga solo la primera vez que lo abres.
- Un **botón único de menú** (☰) despliega las opciones del servicio activo:
  - Instagram: Inicio, Explorar, Reels, Mensajes, Notificaciones, Crear publicación.
  - Facebook: Inicio, Amigos, Videos, Mensajes, Notificaciones, Crear publicación, Mi perfil.
- Los enlaces a Instagram dentro de Facebook se abren en la pestaña de Instagram (y al revés no se mezclan sesiones).
- La app recuerda el último servicio y la última página de cada uno.
- "Crear publicación" no tiene URL directa en la web: el puente intenta pulsar el botón
  de la propia página (si no está visible, vuelve al inicio y lo reintenta). Es de mejor esfuerzo.

## Qué hace el puente C++ (`src/bridge.cpp`)
- User-Agent móvil y URL de arranque (retoma la última página o un enlace externo de Instagram).
- Lista blanca de dominios (instagram, cdninstagram, fbcdn, facebook…); el resto se abre fuera de la app.
- Ruta de descargas segura (sin `../`, sin sobrescribir).
- Script inyectado con ajustes ligeros (viewport, sin resaltado al tocar, sin rebote de scroll).
- `main.cpp`: sandbox desactivado (AppArmor ya confina), límite de procesos de render (menos RAM).

## Estructura
```
CMakeLists.txt  clickable.yaml  manifest.json.in  apparmor.json
instagrambridge.desktop.in
src/  main.cpp  bridge.h  bridge.cpp
qml/  Main.qml
assets/icon.png
```

## Qué instalar en tu PC (Ubuntu 22.04/24.04)
```bash
sudo apt update
sudo apt install -y docker.io adb git pipx
pipx install clickable-ut
pipx ensurepath          # y reabre la terminal
sudo usermod -aG docker $USER   # cierra sesión y vuelve a entrar
```
Verifica con `clickable doctor`. La primera vez Clickable descarga un contenedor
Docker con el SDK de Ubuntu Touch 20.04 (varios GB) y ahí instala las
dependencias de `clickable.yaml` (`qtbase5-dev`, `qtdeclarative5-dev`, `qtwebengine5-dev`).
No necesitas instalar Qt en tu PC.

## Compilar y probar
```bash
cd ig-bridge
clickable desktop                 # probar en el PC (ventana)
clickable                         # compila, instala y lanza en el teléfono por USB
clickable build --arch arm64      # solo generar el .click (usa armhf en teléfonos de 32 bits)
```
Para el teléfono: Ajustes → Acerca de → pulsa 7 veces "Número de compilación"
(o Ajustes → Modo desarrollador) y activa ADB. Comprueba con `adb devices`.

Antes de publicar: cambia `maintainer` en `manifest.json.in` y el nombre
`instagrambridge.aprilpixelrain` si quieres otro identificador.

## Permisos (`apparmor.json`)
`networking`, `webview` (QtWebEngine), `camera`/`microphone`/`audio`/`video`
(historias, reels), `content_exchange`/`content_exchange_source` (subir/descargar archivos).

## Limitaciones honestas
- Es Instagram/Facebook **web**: sin notificaciones push nativas, y algunas funciones (crear
  reels, ciertos filtros) no existen en la versión web.
- Subir fotos depende del selector de archivos de Morph.Web/Content Hub: pruébalo en tu dispositivo.
- Los enlaces externos se abren con `QDesktopServices`; si tu política AppArmor lo bloquea, no se abrirán.
- El User-Agent (`kUserAgent` en `bridge.cpp`) debe coincidir con el Chromium de tu UT.
- Instagram cambia su web a menudo: por eso el script inyectado evita depender de clases CSS.
