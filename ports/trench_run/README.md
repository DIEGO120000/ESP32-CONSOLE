# Trench Run

Port de **Trench Run** de Lucas Cardinali (`lscardinali/TrenchRun-Arduboy`), licencia **MIT** (ver `trench_run_LICENSE.txt`).

Vuelo por la trinchera espacial: esquivar obstáculos, derribar cazas TIE y disparar al ducto térmico (Exhaust Port) para completar la carrera.

## Contenido

| Archivo | Descripción |
|---|---|
| `trench_run.h` | Interfaz `TrenchRunGame` y constantes de juego |
| `trench_run.cpp` | Lógica, scroll, colisiones, enemigos y render |
| `trench_run_assets.h` | Sprites y pantallas PROGMEM |
| `trench_run_LICENSE.txt` | Licencia MIT del original |
| `media/` | Portadas y capturas |

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Mover la mira |
| A | Disparo láser |
| B | Pausa universal (gestionada por `main.cpp`) |

## Activación

```cpp
#define ENABLE_TRENCH_RUN
```

## Notas de integración

- El combo universal de salida A+B está desactivado en este juego: A forma parte del control de disparo.
- Render a 60 FPS con temporización por `millis()`.
