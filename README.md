# 🕹️ Consola Retro ESP32-C6 · OLED 2.42"

Consola portátil de videojuegos retro basada en **ESP32-C6 Super Mini** con pantalla
**OLED SSD1309 de 2.42" (128×64, I2C)**, pensada como plataforma de cartuchos
intercambiables: el mismo firmware aloja emulación estilo **Arduboy**, juegos propios y
**multijugador local por Wi-Fi** con mandos web servidos desde la propia consola
(portal cautivo compatible con iOS y Android).

```
┌─────────────────────────────┐
│  ESP32-C6 Super Mini        │
│  ┌───────────────────────┐  │   OLED SSD1309 2.42" 128x64 (I2C 400 kHz)
│  │   MENU                │  │   Cruceta 5 vías + A/B
│  │ 1. Sea Battle         │  │   Buzzer activo + LED RGB WS2812
│  │ 2. Trench Run         │  │   Batería Li-Ion con telemetría ADC
│  │ ...                   │  │   Wi-Fi AP + portal cautivo
│  └───────────────────────┘  │
└─────────────────────────────┘
```

## ✨ Características

- **Arquitectura modular por cartuchos**: cada juego es un paquete independiente en
  `ports/<juego>/` con su `.h`, `.cpp`, assets, README y `media/`. El núcleo (HAL,
  menú, pausa, ajustes, NVS, audio, Wi-Fi) vive en `src/` e `include/`.
- **Interruptor maestro de cartuchos**: `include/games_config.h` permite compilar el
  firmware sin los juegos que no quieras. Los `#ifdef` cubren include, instancia,
  estado, menú, pausa y dibujado, y la numeración del menú se calcula en el render,
  así que desactivar un cartucho no deja huecos ni rompe la navegación.
- **Compatible con la API de Arduboy**: `Arduboy2`, `Arduboy2Core` y `ArduboyTones`
  sobre el framebuffer de 1 bit y el hardware real (`drawBitmap`, `Sprites`, `tone`).
- **Multijugador local Wi-Fi 2P**: la consola levanta un AP (`RetroArcade-ESP32`) con
  portal cautivo; cada jugador abre un mando en su móvil y juega desde el navegador.
- **Menú dinámico, pausa universal y ajustes persistentes** (brillo, sonido, batería)
  guardados en NVS con `Preferences`.
- **Telemetría de batería Li-Ion** por ADC con divisor 100k/100k y aviso de carga baja.
- **BLE** disponible para gamepad/experimentos (`ProyectoBLE.h`).

## 🔌 Hardware y pinout

Placa: **ESP32-C6 Super Mini**. Pantalla: **SSD1309/SSD1306 2.42" 128×64**, I2C a
400 kHz, dirección autodetectada (`0x3C` o `0x3D`).

| Función | GPIO | Definición en `include/Arduboy2Core.h` | Notas |
|---|---:|---|---|
| OLED SDA | **7** | `ARDUBOY_PIN_SDA` | Bus I2C exclusivo de la pantalla |
| OLED SCL | **6** | `ARDUBOY_PIN_SCL` | 400 kHz |
| Cruceta arriba | **14** | `ARDUBOY_PIN_UP` | `INPUT_PULLUP`, activo a LOW |
| Cruceta abajo | **18** | `ARDUBOY_PIN_DOWN` | `INPUT_PULLUP`, activo a LOW |
| Cruceta derecha | **19** | `ARDUBOY_PIN_RIGHT` | `INPUT_PULLUP`, activo a LOW |
| Cruceta izquierda | **5** | `ARDUBOY_PIN_LEFT` | `INPUT_PULLUP`, activo a LOW |
| Cruceta centro (click) | **20** | `ARDUBOY_PIN_SELECT` / `PIN_NAV_CENTER` | Abre ajustes / confirma |
| Botón A | **3** | `PIN_BUTTON_A` | Seleccionar / OK / acción |
| Botón B | **4** | `PIN_BUTTON_B` | Volver / pausa universal / salir |
| BOOT integrado | **9** | `ARDUBOY_PIN_BOOT` | Activo a LOW, atajo contextual |
| Buzzer | **1** | `ARDUBOY_PIN_BUZZER` | LEDC / `ArduboyTones` |
| LED RGB WS2812 | **8** | — | Estado, notificaciones y feedback |
| ADC batería | **2** | `ARDUBOY_PIN_BATTERY` | ADC1_CH2, divisor 1:2 (100k/100k) |

> ⚠️ Los pines del bus I2C y de la cruceta son fijos: la HAL los configura al arrancar
> y ningún cartucho debe reconfigurarlos.

## 🧱 Estructura del repositorio

```
├── platformio.ini            entorno esp32c6_super_mini
├── include/
│   ├── Arduboy2Core.h        HAL: GPIO, I2C, botones, framebuffer
│   ├── Arduboy2.h/.cpp       API estilo Arduboy (texto, bitmaps, sprites)
│   ├── ArduboyTones.h/.cpp   motor de audio
│   ├── WiFiGamepadServer.h   AP + portal cautivo + WebSocket :81
│   └── games_config.h        ⭐ interruptor maestro de cartuchos
├── src/
│   ├── main.cpp              núcleo: menú, estados, pausa, ajustes, juegos inline
│   ├── Arduboy2.cpp ...
├── ports/                    catálogo de cartuchos (ver ports/README.md)
│   └── <juego>/  <juego>.h  <juego>.cpp  *_assets.h  README.md  media/
└── flash_*.sh                scripts de flasheo
```

## 🛠️ Compilación y flasheo

Requiere [PlatformIO](https://platformio.org/) (CLI o extensión de VS Code) y el
entorno `esp32c6_super_mini` del `platformio.ini`.

```bash
# Compilar
pio run -e esp32c6_super_mini

# Compilar y flashear (puerto USB de la consola)
pio run -e esp32c6_super_mini -t upload --upload-port /dev/ttyACM0

# Monitor serie a 115200 baudios
pio device monitor -e esp32c6_super_mini --port /dev/ttyACM0
```

En Windows/macOS el puerto cambia (`COM5`, `/dev/cu.usbmodem*`). La placa expone un
único puerto USB-CDC/JTAG; no hay `/dev/ttyACM1` salvo que conectes otra placa. Si el
puerto está ocupado por el monitor serie, ciérralo antes de flashear. Tras el flasheo
el firmware arranca solo (*hard reset* por RTS).

Diagnóstico de compilación de referencia con todos los cartuchos activos: **RAM 27 %**,
**Flash 88 %** de 1,25 MB.

## 🎛️ Activar y desactivar cartuchos

Edita `include/games_config.h`:

```cpp
#define ENABLE_SEA_BATTLE
#define ENABLE_TRENCH_RUN
// #define ENABLE_WOLFENDUINO_3D   // comentado: fuera del binario
```

Comentar una macro elimina del build su código, su entrada de menú y su estado; los
números del menú se recalculan solos. Es la forma recomendada de liberar flash (el
motor de Wolfenstein 3D con sus assets ocupa la mayor parte del binario) sin tocar
ningún juego.

## 🎮 Catálogo de cartuchos

| Cartucho | Carpeta | Macro | Portada | Origen / licencia |
|---|---|---|---|---|
| **Sea Battle** | [`ports/sea_battle/`](ports/sea_battle/) | `ENABLE_SEA_BATTLE` | `ports/sea_battle/media/cover.png` | Brady Heater (DarkestSpire), 2020 |
| **Trench Run** | [`ports/trench_run/`](ports/trench_run/) | `ENABLE_TRENCH_RUN` | `ports/trench_run/media/cover.png` | Lucas Cardinali — MIT |
| **Road Trip** | [`ports/road_trip/`](ports/road_trip/) | `ENABLE_ROAD_TRIP` | `ports/road_trip/media/cover.png` | Press Play on Tape — BSD 3-Clause |
| **Wolfenduino 3D** | [`ports/wolfenduino_3d/`](ports/wolfenduino_3d/) | `ENABLE_WOLFENDUINO_3D` | `ports/wolfenduino_3d/media/cover.png` | James Howard — MIT |
| **Cyber Escape** | [`ports/cyber_escape/`](ports/cyber_escape/) | `ENABLE_CYBER_ESCAPE` | `ports/cyber_escape/media/cover.png` | SKRTM, 2026 |
| **Sopa de Letras** | [`ports/sopa_letras/`](ports/sopa_letras/) | `ENABLE_SOPA_LETRAS` | `ports/sopa_letras/media/cover.png` | Desarrollo propio |
| **Math Challenge** | [`ports/math_challenge/`](ports/math_challenge/) | `ENABLE_MATH_CHALLENGE` | `ports/math_challenge/media/cover.png` | Desarrollo propio |
| **Ping Pong** | [`ports/ping_pong/`](ports/ping_pong/) | `ENABLE_PING_PONG` | `ports/ping_pong/media/cover.png` | Port interno (incluye 2P Wi-Fi) |
| **Breakout** | [`ports/breakout/`](ports/breakout/) | `ENABLE_BREAKOUT` | `ports/breakout/media/cover.png` | Port interno |
| **Space Invaders** | [`ports/space_invaders/`](ports/space_invaders/) | `ENABLE_SPACE_INVADERS` | `ports/space_invaders/media/cover.png` | Port interno |
| **1943** | [`ports/arcade_1943/`](ports/arcade_1943/) | `ENABLE_1943` | `ports/arcade_1943/media/cover.png` | Port interno |
| **Flappy Bird** | [`ports/flappy_bird/`](ports/flappy_bird/) | `ENABLE_FLAPPY_BIRD` | `ports/flappy_bird/media/cover.png` | Port interno |
| **Snake** | [`ports/snake/`](ports/snake/) | `ENABLE_SNAKE` | `ports/snake/media/cover.png` | Port interno |
| **Jump Man** | [`ports/jump_man/`](ports/jump_man/) | `ENABLE_JUMP_MAN` | `ports/jump_man/media/cover.png` | Port interno |
| **2048** | [`ports/game_2048/`](ports/game_2048/) | `ENABLE_2048` | `ports/game_2048/media/cover.png` | Port interno |
| **T-Rex Runner** | [`ports/trex_runner/`](ports/trex_runner/) | `ENABLE_TREX_RUNNER` | `ports/trex_runner/media/cover.png` | Port interno |
| **Tic-Tac-Toe** | [`ports/tic_tac_toe/`](ports/tic_tac_toe/) | `ENABLE_TIC_TAC_TOE` | `ports/tic_tac_toe/media/cover.png` | Port interno |
| **BrainduTrain** | [`ports/braindu/`](ports/braindu/) | `ENABLE_BRAINDU` | `ports/braindu/media/cover.png` | Port interno |
| **Hangman** | [`ports/hangman/`](ports/hangman/) | `ENABLE_HANGMAN` | `ports/hangman/media/cover.png` | serisman (port Arduboy) |
| **TRIS** | [`ports/tris/`](ports/tris/) | `ENABLE_TRIS` | `ports/tris/media/cover.png` | OBONO (port Arduboy) |

Cada cartucho documenta sus controles e integración en su propio `README.md`; las
convenciones, la plantilla y el índice están en **[`ports/README.md`](ports/README.md)**.

Convención de portadas: imagen PNG de 128×64 (1 bit o escala de grises) guardada como
`media/cover.png` dentro del cartucho. Las carpetas ya existen con su `.gitkeep`; las
portadas se añaden en cuanto se capture la pantalla de título de cada juego.

Todos los juegos viven ya en `ports/`; `src/main.cpp` solo conserva el núcleo
(menú, pausa, ajustes, NVS, Wi-Fi 2P) y el diagnóstico de pines. Proyecto BLE sigue
en el núcleo porque no es un cartucho de juego.

## 📶 Multijugador Wi-Fi 2P

1. En el menú abre **Ping Pong → 3. 2P WiFi Gamepads**.
2. La consola crea el AP **`RetroArcade-ESP32`** (sin contraseña) y el portal cautivo.
3. Cada jugador conecta su móvil y abre el mando: iOS y Android muestran la página
   automáticamente; si no, entra en **`http://192.168.4.1`**.
4. Jugador 1 usa el mando web **o** la cruceta física; el jugador 2, solo el mando web.
5. El WebSocket escucha en el puerto **81** y resuelve el host dinámicamente para
   funcionar tanto por IP como por el nombre del portal.

Las rutas de sondeo de portal (`/hotspot-detect.html`, `/generate_204`, `/ncsi.txt`,
`/connecttest.txt`, `/success.txt`, …) sirven el gamepad con cabeceras `no-store`, y
cualquier ruta desconocida redirige a `192.168.4.1`, de modo que la página carga a la
primera en Safari, Chrome y Windows.

## 🕹️ Controles globales

| Entrada | Acción |
|---|---|
| B | Pausa universal / volver / salir al menú (según contexto) |
| Click central | Menú de ajustes de la consola |
| A + B | Salir del juego al menú de la consola |
| BOOT (corto) | Atajo contextual (salir, confirmar, despertar) |
| BOOT (largo) | Apagado / deep sleep |

Cada juego puede sobrescribir estos atajos en su propio `README.md`.

## 📄 Créditos y licencias

El núcleo y los juegos propios son de este repositorio. Los ports conservan la
licencia de sus autores originales, incluida en cada carpeta
(`trench_run_LICENSE.txt`, cabeceras MIT/BSD en los fuentes). Arduboy2, Arduboy2Core y
ArduboyTones derivan del proyecto Arduboy (MIT).

## 🗺️ Roadmap

- [x] Fase 1 — arquitectura `ports/` + `games_config.h` + documentación.
- [ ] Portadas reales en `ports/<juego>/media/cover.png`.
- [ ] Extraer los juegos inline de `main.cpp` a cartuchos independientes.
- [ ] Añadir captura de pantalla desde la propia consola (volcado por serie).
