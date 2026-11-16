# BrainduTrain

Entrenamiento mental por rondas: compara estímulos y responde con los botones.

## Contenido

| Archivo | Descripción |
|---|---|
| `braindu.h` | Clase `BrainduTrainGame` (header-only): estados `BT_STATE_*` |
| `braindu_bitmaps.h` | Bitmaps PROGMEM del port |
| `braindu.cpp` | Tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| LEFT / RIGHT | Responder la opción izquierda / derecha |
| UP / DOWN | Responder las opciones superiores |
| A / B | Navegar el menú y confirmar |
| BOOT corto / 1 s | Atajos de las pantallas del juego |

## Activación

```cpp
#define ENABLE_BRAINDU
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- `onBootShortPress()` y `onBootHold1s()` los invoca el núcleo.
