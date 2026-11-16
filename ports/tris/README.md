# TRIS

Tetris pixel-perfect (port de Arduboy de OBONO): rotación con pared kick, líneas y caída progresiva.

## Contenido

| Archivo | Descripción |
|---|---|
| `tris.h` | Clase `TrisGame` (header-only): estados `TRIS_STATE_*` |
| `tris.cpp` | Bitmap de título y dígitos PROGMEM |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| UP / A | Rotar la pieza |
| LEFT / RIGHT | Desplazar la pieza |
| DOWN | Bajar una fila (sin sumar puntos) |
| A / B / cruceta / BOOT | Empezar partida tras `GAME OVER` |

## Activación

```cpp
#define ENABLE_TRIS
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- `onBootShortPress()` lo invoca el núcleo desde la gestión del botón BOOT.
