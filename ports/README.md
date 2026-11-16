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

| Port | Carpeta | Macro | Origen / licencia |
|---|---|---|---|
| Sea Battle | `sea_battle/` | `ENABLE_SEA_BATTLE` | Brady Heater (DarkestSpire), 2020 |
| Trench Run | `trench_run/` | `ENABLE_TRENCH_RUN` | Lucas Cardinali — MIT |
| Road Trip | `road_trip/` | `ENABLE_ROAD_TRIP` | Press Play on Tape — BSD 3-Clause |
| Wolfenduino 3D | `wolfenduino_3d/` | `ENABLE_WOLFENDUINO_3D` | James Howard — MIT |
| Cyber Escape | `cyber_escape/` | `ENABLE_CYBER_ESCAPE` | SKRTM, 2026 |
| Sopa de Letras | `sopa_letras/` | `ENABLE_SOPA_LETRAS` | Desarrollo propio |
| Math Challenge | `math_challenge/` | `ENABLE_MATH_CHALLENGE` | Desarrollo propio |

`math_challenge` y `sopa_letras` (clase `WordSearchGame`) conservan sus nombres de
clase originales: solo cambiaron de archivo.

## Pendiente de extracción

Estos juegos siguen dentro de `src/main.cpp` y **no** se pueden desactivar aún:
Ping Pong (incluido el modo Wi-Fi 2P), Breakout/Arkanoid, Space Invaders, 1943,
Flappy Bird, Snake, Jump Man, 2048, T-Rex Runner, Tic-Tac-Toe, BrainduTrain,
Hangman y TRIS. Sus entradas del menú son fijas y su lógica está acoplada al
núcleo (pausa, sonido, NVS, selector). Al extraerlos, cada uno recibirá su
carpeta aquí y su macro en `games_config.h`.

## Añadir un port nuevo

1. Crear `ports/<juego>/` con `.h`, `.cpp`, `README.md` y `media/.gitkeep`.
2. Añadir `#define ENABLE_<JUEGO>` en `include/games_config.h`.
3. En `src/main.cpp`: include guardado, instancia, valor de `SystemState`,
   entrada en `menuEntries[]` y casos en `isCurrentGameActive`,
   `currentGameHasSubmenu`, `returnToCurrentGameSubmenu`,
   `exitCurrentGameToMainMenu`, `enterDeepSleep` y los `switch` de tick/draw.
4. Compilar: SCons recoge la carpeta automáticamente vía
   `build_src_filter = +<*> +<../ports/>`.
