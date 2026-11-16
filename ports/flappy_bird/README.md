# Flappy Bird

El pájaro y las tuberías: un botón, gravedad constante y récord en pantalla.

## Contenido

| Archivo | Descripción |
|---|---|
| `flappy_bird.h` | Clase `FlappyGame` (header-only) |
| `flappy_bird.cpp` | Sprites y tablas PROGMEM |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| A / B / UP / DOWN / LEFT / RIGHT | Aletear |
| A / B / cruceta | Reiniciar tras `GAME OVER` |

## Activación

```cpp
#define ENABLE_FLAPPY_BIRD
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
