# Catacombs of the Damned — `catacombs_3d`

Port fiel de **"Catacombs of the damned!"** para la consola ESP32-C6 Super Mini
(OLED SSD1309 128x64, I2C SDA=7 / SCL=6).

## Origen

- Repositorio oficial: **https://github.com/jhhoward/Arduboy3D**
- Autor: James Howard (@jameshhoward)
- Licencia: **MIT** (ver [`LICENSE`](LICENSE), copyright (c) 2019 James Howard)
- Versión portada: `Source/Arduboy3D/` del repositorio, copiada sin reescribir
  el motor: renderizador pseudo-3D por columnas, texturas vectoriales con
  dithering, generador aleatorio de mazmorras de 10 plantas, cuatro tipos de
  monstruos (esqueleto, mago, murciélago y araña), proyectiles mágicos, cofres,
  coronas, pergaminos, monedas, pociones, HUD y pantallas de menú/resultados.

## Estructura

```
catacombs_3d/
├── catacombs_3d.h / .cpp   interfaz del cartucho (init / update / draw / exit)
├── engine/                 código original de Arduboy3D, copia 1:1
│   ├── Platform.cpp/.h     HAL sustituida (entrada, vídeo, audio)
│   ├── Defines.h           constantes + lecturas PROGMEM seguras
│   ├── Save.cpp/.h         récord en NVS (código nuevo de integración)
│   └── Generated/          sprites y fuente en PROGMEM
├── LICENSE                 licencia MIT original
├── README.md               esta ficha
└── media/cover.png         portada oficial (banner del repositorio)
```

**Aislamiento de símbolos:** todo el motor (clases, structs, globales y
funciones) vive dentro de `namespace Catacombs3D`, de modo que puede enlazarse
junto a `ports/wolfenduino_3d/` (que también define `Menu`, `Renderer`,
`Platform`, `Random`, etc.) sin colisiones.

## Adaptaciones de plataforma

- **Entrada:** se lee el estado que `main.cpp` ya sondeó con `pollButtons()`;
  el cartucho no vuelve a sondear botones para no consumir los flancos.
- **Vídeo:** escritura directa en el framebuffer de `Arduboy2` (páginas de 8 px)
  mediante `GetScreenBuffer()`, `PutPixel()` y `DrawVLine()`.
- **PROGMEM:** en ESP32-C6 (RISC-V) y en PC las tablas se leen con funciones
  `memcpy` alineadas y seguras; no se usan registros ni macros de AVR.
- **Audio:** los patrones originales `(frecuencia, duración)` se reproducen nota
  a nota sin bloquear el render, por el buzzer pasivo de GPIO 1 o por el audio
  SDL2 del emulador, respetando el interruptor global de sonido (`MUTE/ON`).
- **Récord:** el marcador final se guarda en NVS con namespace `"catacombs"`
  (clave `best`). En el emulador se vuelca a `sim_nvs.dat`. El menú interno
  muestra `Best:` y la pantalla de resultados guarda el nuevo récord.

## Controles

| Consola | Acción |
|---|---|
| Cruceta arriba / abajo | Avanzar / retroceder |
| Cruceta izquierda / derecha | Girar la vista |
| A + izquierda / derecha | Strafe lateral (era el botón A original) |
| A | Lanzar bola de fuego / abrir cofres e interactuar (era B) |
| B | Pausa universal de la consola |
| B mantenido en menús | Volver al menú de la consola |

En el menú interno del juego, A confirma la opción seleccionada (Iniciar /
Sonido on-off); arriba/abajo cambian la selección.

## Activación

`include/games_config.h`:

```cpp
#define ENABLE_CATACOMBS_3D
```

Con la macro activa, `src/main.cpp` registra `"Catacombs 3D"` en el menú
dinámico y en la máscara NVS `games_mask` (panel web `/api/games`), y añade los
casos de pausa, submenú, salida y dormido profundo. El emulador de PC
(`sim/main_sim.cpp`) lo incluye en su menú con el mismo `#ifdef`.

## Compilación

```bash
pio run -e esp32c6_super_mini
pio run -e pc_emulator && .pio/build/pc_emulator/program
```
