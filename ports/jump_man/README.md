# Jump Man

Plataformas de un solo sentido: salta entre plataformas y sube todo lo posible.

## Contenido

| Archivo | Descripción |
|---|---|
| `jump_man.h` | Clase `JumpManGame` (header-only), incluido `draw5x8Sprite` |
| `jump_man.cpp` | Sprites `jm_*` y tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| A / B / UP / DOWN | Saltar (solo con los pies en el suelo) |
| A / B | Reiniciar tras `GAME OVER` |

## Activación

```cpp
#define ENABLE_JUMP_MAN
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- Los sprites `jm_*` y el helper `draw5x8Sprite` son propiedad de este port.
