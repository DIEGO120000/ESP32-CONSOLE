# 1943

Shooter vertical inspirado en el arcade 1943: avión, disparo continuo y oleadas.

## Contenido

| Archivo | Descripción |
|---|---|
| `arcade_1943.h` | Clase `AirCombatGame` (header-only) |
| `arcade_1943.cpp` | Sprites de aviones y tablas PROGMEM |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| LEFT / RIGHT | Mover el avión |
| A | Disparar |
| A / B | Reintentar tras `GAME OVER` |

## Activación

```cpp
#define ENABLE_1943
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- La carpeta conserva el nombre `arcade_1943` aunque la clase sea `AirCombatGame`.
