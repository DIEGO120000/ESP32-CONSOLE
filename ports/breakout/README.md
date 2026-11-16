# Breakout

Arkanoid clásico: pala, rebotes, ladrillos por rondas y victoria final.

## Contenido

| Archivo | Descripción |
|---|---|
| `breakout.h` | Clase `BreakoutGame` (header-only) |
| `breakout.cpp` | Tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| LEFT / RIGHT | Mover la pala |
| A | Lanzar la bola |
| A / B | Reintentar tras `GAME OVER` o `VICTORY` |

## Activación

```cpp
#define ENABLE_BREAKOUT
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
