# ESP32-CONSOLE

Consola de videojuegos retro y emulador Arduboy / mini juegos para microcontrolador ESP32 / ESP32-C6 Super Mini.

## 🚀 Características
- **Plataforma:** ESP32-C6 (PlatformIO / Arduino Framework)
- **Pantalla:** OLED SSD1306 128x64 (I2C)
- **Audio & Melodías:** Sintetizador de tonos y buzzer (GPIO 1) / ArduboyTones
- **LED RGB:** WS2812 integrado (GPIO 8)
- **Conectividad:** BLE (Bluetooth Low Energy)
- **Juegos y Roms:** Emulación y juegos retro integrados (Ping Pong, Breakout, Space Invaders, etc.)

## 🛠️ Requisitos y Compilación
Este proyecto está configurado para [PlatformIO](https://platformio.org/).

### Compilar y subir firmware:
```bash
# Compilar
pio run

# Flashear en ESP32-C6
pio run --target upload
```

### Scripts de Flasheo:
Incluye scripts de utilidad para flasheo automático y configuración rápida en ESP32-C6 / Super Mini:
- `./flash_esp32c6.sh`
- `./flash_auto.sh`
- `./flash_direct.sh`

## 🧩 Arquitectura modular (cartuchos)

El proyecto separa el **núcleo de la consola** del **catálogo de juegos**:

```
src/                    núcleo: main.cpp, HAL (Arduboy2Core), audio, Wi-Fi 2P, BLE
include/                cabeceras del núcleo + games_config.h (interruptor maestro)
ports/<juego>/          un paquete autocontenido por juego
  <juego>.h  <juego>.cpp  <juego>_assets.h  README.md  media/
```

`platformio.ini` compila ambas zonas con:

```ini
build_src_filter = +<*> +<../ports/>
```

### Activar o desactivar juegos

Edita `include/games_config.h` y comenta la macro del port que no quieras en el
binario. Su include, instancia, estado, entrada del menú, pausa y dibujado se
compilan bajo el mismo `#ifdef`, y la numeración del menú se recalcula sola:

```cpp
#define ENABLE_SEA_BATTLE
// #define ENABLE_WOLFENDUINO_3D   // excluido: libera ~800 KB de flash
```

### Ports incluidos

| Port | Macro | Origen / licencia |
|---|---|---|
| Sea Battle | `ENABLE_SEA_BATTLE` | Brady Heater (DarkestSpire), 2020 |
| Trench Run | `ENABLE_TRENCH_RUN` | Lucas Cardinali — MIT |
| Road Trip | `ENABLE_ROAD_TRIP` | Press Play on Tape — BSD 3-Clause |
| Wolfenduino 3D | `ENABLE_WOLFENDUINO_3D` | James Howard — MIT |
| Cyber Escape | `ENABLE_CYBER_ESCAPE` | SKRTM, 2026 |
| Sopa de Letras | `ENABLE_SOPA_LETRAS` | Desarrollo propio |
| Math Challenge | `ENABLE_MATH_CHALLENGE` | Desarrollo propio |

Detalles de cada cartucho, controles y guía para añadir uno nuevo: [`ports/README.md`](ports/README.md).

### Pendiente de extracción

Ping Pong (con su modo Wi-Fi 2P), Breakout, Space Invaders, 1943, Flappy Bird,
Snake, Jump Man, 2048, T-Rex Runner, Tic-Tac-Toe, BrainduTrain, Hangman y TRIS
siguen dentro de `src/main.cpp` y todavía no se pueden desactivar desde
`games_config.h`.
