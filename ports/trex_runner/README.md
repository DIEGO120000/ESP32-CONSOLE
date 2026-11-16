# T-Rex Runner

El dinosaurio de Chrome: salta cactus, agáchate ante pájaros y persigue el récord.

## Contenido

| Archivo | Descripción |
|---|---|
| `trex_runner.h` | Clase `TRexGame` (header-only) |
| `trex_runner.cpp` | Sprites del T-Rex y tablas PROGMEM |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| UP / A | Saltar |
| DOWN (mantener) | Agacharse |
| A / B / cruceta | Reiniciar tras `GAME OVER` |

## Activación

```cpp
#define ENABLE_TREX_RUNNER
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- `Preferences` (NVS `trex`, clave `trex_hi`) para el récord persistente.
