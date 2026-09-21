# CW-OVERLAY-APP

## Requisitos
Este programa se ejecuta como una aplicación externa (Overlay App) basada en la app ["Beacon"](https://github.com/armel/uv-k1-k5v3-firmware-custom/tree/main/App/apps/beacon) y permite enviar y recibir CW. 
Requiere tener instalado el firmware [UV-K1-K5v3-firmware-custom de armel](https://github.com/armel/uv-k1-k5v3-firmware-custom) en su edición **Labs** para poder usarse.

**La app fue creada y testeada para la radio quansheng UV K5 (8) con el filmware F4HWN V6.0.0, no garantizo que funcione en otros modelos de quansheng**

## Como instalar?
Hay dos maneras:
1) Archivo precompilado:

1. Descarga el archivo ".zip" que contiene "CWTX.app" y extráelo.
2. Ingresa a la herramienta web de instalación: [UVStudio Apps](https://armel.github.io/uvstudio/#apps).
3. Conecta tu radio al PC con el cable de programación. **La radio debe estar encendida en su modo normal.**
4. En la página, ve a **"App file (.app) / Choose file"** y busca y selecciona tu archivo "CWTX.app".
5. Elige el *slot* de memoria donde deseas instalar la app.
6. Presiona el botón **Install**.

2) Compilación desde el código fuente:

Si deseas modificar el código o compilar la aplicación por tu cuenta, necesitarás instalar las herramientas de compilación para ARM (`arm-none-eabi-gcc`). 

1. Preparar el entorno según tu Sistema Operativo:

**Linux (Debian / Ubuntu / Zorin OS / Mint):**
```Bash
sudo apt update
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
```
**Linux (Arch / Manjaro):**

```Bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-binutils
```

**Linux (Fedora):**

```Bash
sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-binutils
```
**macOS:**
Requiere Homebrew.

```Bash
brew install arm-none-eabi-gcc
```
**Windows:**

Dado que la compilación utiliza un script de bash (build.sh), la forma más sencilla en Windows es usar WSL (Windows Subsystem for Linux) instalando Ubuntu y luego ejecutando los comandos de Debian/Ubuntu mencionados arriba.

2. Compilar la aplicación
Una vez instaladas las dependencias [https://github.com/armel/uv-k1-k5v3-firmware-custom/archive/refs/tags/v6.0.0.zip](https://github.com/armel/uv-k1-k5v3-firmware-custom/archive/refs/tags/v6.0.0.zip), sigue estos pasos en tu terminal:

Descarga el código fuente y renombra la carpeta a "cwtx".

Mueve la carpeta a la ruta de las apps del firmware base:
.../uv-k1-k5v3-firmware-custom-6.0.0/App/apps/cwtx

Abre una terminal en esa carpeta y dale permisos de ejecución al script:

```Bash
chmod +x build.sh
```
Ejecuta el script de compilación:

```Bash
./build.sh
```
Si todo sale bien, se generará el archivo CWTX.app. Ahora puedes instalarlo siguiendo los pasos del Método 1.

## Manual y Características de CW-TX-APP

1. Características Principales:

Modo Transmisión (TX): Permite redactar mensajes usando el teclado de la radio y transmitirlos en Morse con tono sidetone (TONE) o como portadora pura (CARRIER).
Modo Recepción (RX): Decodifica automáticamente las señales Morse entrantes por la antena, mostrando los caracteres en pantalla en tiempo real.
Velocidad Ajustable (WPM): Control en tiempo real de la velocidad de transmisión y recepción mediante los botones de navegación (UP/DOWN key).
Prosignos Rápidos: Acceso directo a señales especiales (CQ, SK, AR, BT, KN, etc.) combinando la tecla F + numero.
Modo Automático (Auto-repeat): Envío en bucle del mensaje guardado cada ciertos segundos para llamadas generales (CQ loop) (presionando durante 3 segundos PTT para activar o desactivar el modo).
Persistencia de Configuración: Guarda automáticamente en la EEPROM de la radio tus preferencias (velocidad, tono, modo de portadora, etc.).

2. Mapa de Controles y Atajos

Botón MENU: Alterna entre el Modo Transmisión (TX) y el Modo Recepción (RX).
Doble toque de la tecla F: Limpia la pantalla y borra el buffer de texto actual (funciona tanto en modo TX como en RX).
Botón EXIT: Sale de la aplicación y regresa al firmware principal de la radio o cuando se transmite corta la transmision y vuelve al "menu de escritura".

Controles de Velocidad y Navegación
Con las flechas (UP/DOWN key): Subir o bajar la velocidad de transmisión/recepción (WPM) en tiempo real (ajustando los milisegundos de la unidad del punto Morse, desde 15 ms hasta 300 ms).

Funciones Especiales con la tecla F (Atajos):

Al presionar una vez la tecla F y luego presionar:
Tecla 0: Disminuye la frecuencia del tono local en 100 Hz (rango de 500 Hz a 1200 Hz).
Tecla *: Aumenta la frecuencia del tono local en 100 Hz.

Controles en Modo Transmisión (TX):

Teclado numérico (1 al 9): Escribe letras y números mediante el sistema Multitap (pulsar varias veces la misma tecla cambia de letra, estilo antiguo de teléfono móvil).

![Esquema del teclado Multitap](img/teclado.png)

Tecla 0: Funciona como Backspace (borra el último carácter escrito).
Tecla * (Asterisco): Inserta un espacio en blanco.
Botón PTT (Toque corto): Inicia la transmisión del mensaje redactado por el aire.
Botón PTT (Mantener presionado 1.5s): Activa o desactiva el Modo Automático (AUTO) para repetir el mensaje cíclicamente.
Mantener tecla F (1.5s): Activa/Desactiva el modo de Portadora Pura (CARR) sin tono de audio interno.
Prosignos: se usa F + numero, puedes configurar eso a tu gusto desde "cwtx_app.c".

Lista de Prosignos (F + 1 al 9):

F + 1:   (Espacio / Vacío — acá puedes agregar tu ID/indicativo)
F + 2: SK (End of Work — Fin de contacto / Cierre)
F + 3: AR (End of Message — Fin de mensaje)
F + 4: BT (Break — Separador o pausa larga entre párrafos)
F + 5: KN (Invite to specific station — Transmisión dirigida a una estación específica)
F + 6: K (Invite to Transmit — Adelante / Responda)
F + 7: CQ (Calling Any Station — Llamada general a cualquier estación)
F + 8: DE (From — De parte de)
F + 9: 73 (Saludos cordiales / Mejores deseos)

Controles en Modo Recepción (RX):

Visualización en vivo: Muestra una barra de estado con el RSSI (intensidad de señal) y los puntos/rayas (. y -) que la radio va detectando antes de convertirlos en letras.
Alertas automáticas: La radio emite un pitido o destella el LED si detecta prosignos comunes como CQ o tu ID puesto en F + 1 en una transmisión.




