# Snake

Serpiente clásica: come, crece y evita muros y tu propia cola.

## Contenido

| Archivo | Descripción |
|---|---|
| `snake.h` | Clase `SnakeGame` (header-only) |
| `snake.cpp` | Tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Cambiar de dirección (no se permite giro de 180°) |
| A / B | Reiniciar tras `GAME OVER` |

## Activación

```cpp
#define ENABLE_SNAKE
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
