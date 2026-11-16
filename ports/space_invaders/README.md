# Space Invaders

Invasores en oleadas con formación que desciende, disparo único y puntuación.

## Contenido

| Archivo | Descripción |
|---|---|
| `space_invaders.h` | Clase `SpaceInvadersGame` (header-only) |
| `space_invaders.cpp` | Sprites de aliens y tablas PROGMEM |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| LEFT / RIGHT | Mover la nave |
| A | Disparar |
| A / B | Reintentar tras `GAME OVER` o `VICTORY` |

## Activación

```cpp
#define ENABLE_SPACE_INVADERS
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
