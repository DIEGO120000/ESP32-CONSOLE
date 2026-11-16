# ports/ — catálogo de cartuchos

Cada juego es un paquete autocontenido. El núcleo de la consola (HAL, menú, pausa,
ajustes, NVS, Wi-Fi 2P) vive en `src/` e `include/`; aquí solo hay juegos.

```
ports/<juego>/
├── <juego>.h        interfaz pública (init / update / draw / exitRequested)
├── <juego>.cpp      lógica del juego
├── <juego>_assets.h sprites y pantallas PROGMEM (si aplica)
├── README.md        ficha del juego: origen, controles, integración
└── media/            portadas y capturas (se versiona con .gitkeep)
```

Las tablas grandes (`PROGMEM`) viven en el `.cpp` y el `.h` las declara `extern`,
para que el cartucho se pueda incluir desde más de una unidad sin duplicar flash.

## Activación

`include/games_config.h` es el interruptor maestro. Comentar una macro excluye el
port del binario: su include, instancia, estado del sistema, entrada del menú,
pausa y dibujado están bajo el mismo `#ifdef`. La numeración del menú se calcula
en tiempo de render, así que no quedan huecos.

```cpp
#define ENABLE_SEA_BATTLE
// #define ENABLE_CYBER_ESCAPE   // fuera del build
```

## Ports disponibles

| Port | Carpeta | Macro | Portada | Origen / licencia |
|---|---|---|---|---|
| Ping Pong | `ping_pong/` | `ENABLE_PING_PONG` | `media/cover.png` | Port interno (incluye 2P Wi-Fi) |
| Breakout | `breakout/` | `ENABLE_BREAKOUT` | `media/cover.png` | Port interno |
| Space Invaders | `space_invaders/` | `ENABLE_SPACE_INVADERS` | `media/cover.png` | Port interno |
| 1943 | `arcade_1943/` | `ENABLE_1943` | `media/cover.png` | Port interno |
| Flappy Bird | `flappy_bird/` | `ENABLE_FLAPPY_BIRD` | `media/cover.png` | Port interno |
| Snake | `snake/` | `ENABLE_SNAKE` | `media/cover.png` | Port interno |
| Jump Man | `jump_man/` | `ENABLE_JUMP_MAN` | `media/cover.png` | Port interno |
| 2048 | `game_2048/` | `ENABLE_2048` | `media/cover.png` | Port interno |
| T-Rex Runner | `trex_runner/` | `ENABLE_TREX_RUNNER` | `media/cover.png` | Port interno |
| Tic-Tac-Toe | `tic_tac_toe/` | `ENABLE_TIC_TAC_TOE` | `media/cover.png` | Port interno |
| BrainduTrain | `braindu/` | `ENABLE_BRAINDU` | `media/cover.png` | Port interno |
| Hangman | `hangman/` | `ENABLE_HANGMAN` | `media/cover.png` | serisman (port Arduboy) |
| TRIS | `tris/` | `ENABLE_TRIS` | `media/cover.png` | OBONO (port Arduboy) |
| Sea Battle | `sea_battle/` | `ENABLE_SEA_BATTLE` | `media/cover.png` | Brady Heater (DarkestSpire), 2020 |
| Trench Run | `trench_run/` | `ENABLE_TRENCH_RUN` | `media/cover.png` | Lucas Cardinali — MIT |
| Road Trip | `road_trip/` | `ENABLE_ROAD_TRIP` | `media/cover.png` | Press Play on Tape — BSD 3-Clause |
| Wolfenduino 3D | `wolfenduino_3d/` | `ENABLE_WOLFENDUINO_3D` | `media/cover.png` | James Howard — MIT |
| Cyber Escape | `cyber_escape/` | `ENABLE_CYBER_ESCAPE` | `media/cover.png` | SKRTM, 2026 |
| Sopa de Letras | `sopa_letras/` | `ENABLE_SOPA_LETRAS` | `media/cover.png` | Desarrollo propio |
| Math Challenge | `math_challenge/` | `ENABLE_MATH_CHALLENGE` | `media/cover.png` | Desarrollo propio |

`math_challenge` y `sopa_letras` (clase `WordSearchGame`) conservan sus nombres de
clase originales: solo cambiaron de archivo. Igual ocurre con `arcade_1943`
(clase `AirCombatGame`) y `braindu` (clase `BrainduTrainGame`).

Proyecto BLE no es un cartucho: su lógica sigue en el núcleo.

## Añadir un port nuevo

1. Crear `ports/<juego>/` con `.h`, `.cpp`, `README.md` y `media/.gitkeep`.
2. Añadir `#define ENABLE_<JUEGO>` en `include/games_config.h`.
3. En `src/main.cpp`: include guardado, instancia, valor de `SystemState`,
   entrada en `menuEntries[]` y casos en `isCurrentGameActive`,
   `currentGameHasSubmenu`, `returnToCurrentGameSubmenu`,
   `exitCurrentGameToMainMenu`, `enterDeepSleep` y los `switch` de tick/draw.
4. Compilar: SCons recoge la carpeta automáticamente vía
   `build_src_filter = +<*> +<../ports/>`.
