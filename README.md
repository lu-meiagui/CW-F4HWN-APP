# CW-OVERLAY-APP

## Requisitos
Este programa se ejecuta como una aplicación externa (Overlay App) basada en la app ["Beacon"](https://github.com/armel/uv-k1-k5v3-firmware-custom/tree/main/App/apps/beacon). 
Requiere tener instalado el firmware [UV-K1-K5v3-firmware-custom de armel](https://github.com/armel/uv-k1-k5v3-firmware-custom) en su edición **Labs** para poder usarse.

**La app fue creada y testeada para la radio quansheng UV K5 (8) con el filmware F4HWN V6.0.0, no garantizo que funcione en otros modelos de quansheng**

## Como instalar?
Hay dos maneras:
1) Descargar el zip que contiene CWTX.app, luego colocarla en el filmware usando la herramienta https://armel.github.io/uvstudio/#apps 
(para usar la herrramienta de forma correcta tiene que conectar su radio con el cable de programacion mientras la radio está encendida en su modo normal,
elegir el archivo de CWTX.app en "App file (.app)/ Chose file", elegir el slot en que lo quiere instalar y presionar "install".

2) Installar el codigo .zip, renombrarlo a cwtx, colocarlo en .../uv-k1-k5v3-firmware-custom-6.0.0/App/apps/cwtx, darle permisos a build.sh y ejecutarlo, aso generará a "CWTX.app"
luego de eso es solo hacer el paso 1.
