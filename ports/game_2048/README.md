# 2048

Puzle de deslizamiento 4×4 con récord persistente en NVS.

## Contenido

| Archivo | Descripción |
|---|---|
| `game_2048.h` | Clase `Game2048` (header-only) |
| `game_2048.cpp` | Tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Deslizar el tablero en esa dirección |
| A / B / cruceta | Empezar tras `GAME OVER` |

## Activación

```cpp
#define ENABLE_2048
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- `Preferences` (NVS `2048_data`, clave `hi_score`) para el récord. El núcleo
  llama a `saveHighScore()` al salir o al apagar.
