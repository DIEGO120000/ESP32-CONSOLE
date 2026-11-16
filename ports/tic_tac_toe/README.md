# Tic-Tac-Toe

Tres en raya con 1 o 2 jugadores, tres niveles de dificultad y menú interno.

## Contenido

| Archivo | Descripción |
|---|---|
| `tic_tac_toe.h` | Clase `TicTacToeGame` (header-only): `TTTState`, `TTTGameMode`, `TTTDifficulty` |
| `tic_tac_toe.cpp` | Tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Mover el cursor / elegir modo y dificultad |
| A | Colocar ficha / confirmar |
| B | Volver atrás |
| BOOT (pulsación corta) | Aceptar (atajo de la pantalla de selección) |

## Activación

```cpp
#define ENABLE_TIC_TAC_TOE
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- `onBootShortPress()` lo invoca el núcleo desde la gestión del botón BOOT.
