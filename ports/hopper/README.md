# Hopper

Port fiel del juego original de **Kevin McDermott** (bigkevmcd/arduboy-hopper,
rama `master`, licencia MIT): una rana que cruza carreteras y ríos esquivando
coches y saltando entre troncos.

## Origen

| Archivo | Procedencia |
|---|---|
| `Hopper.ino` | Original íntegro, conservado como referencia (no entra en el build) |
| `hopper.h` | Original íntegro (`LANESIZE`, `CAR`, `LOG`) |
| `frog.h` / `frog.cpp` | Original; solo se envolvió en `namespace HopperGame` |
| `lane.h` / `lane.cpp` | Original; idem + destructor para reiniciar sin fugas |
| `obstacle.h` / `obstacle.cpp` | Original; idem |
| `hopper_game.h` / `hopper_game.cpp` | Adaptador al HAL y a la interfaz de cartuchos |
| `LICENSE` | Licencia MIT original |

La física, los carriles, las velocidades, el área de colisión y el sprite de la
rana son los del repositorio original. `HopperGame::setup()` replica la creación
de `frog`, `lane1`…`lane5` con los mismos parámetros del `.ino`.

## Controles

| Entrada | Acción |
|---|---|
| Cruceta arriba/abajo/izquierda/derecha | Saltar un carril (8 px) |
| B | Pausa universal de la consola (Continuar / Menú Consola) |

Hopper no usa A ni B en su lógica original, por lo que B queda libre para la
pausa y la salida al menú sin interferir con el juego.

## Integración

```cpp
#define ENABLE_GAME_HOPPER 1   // include/games_config.h
```

`src/main.cpp` registra el cartucho como `STATE_HOPPER` en el carrusel, la pausa
y el bucle de dibujado. El panel web (`/config`) lo lista automáticamente a
partir de `menuGameEntries[]` y permite ocultarlo en caliente (NVS
`sys_cfg`/`games_mask`).

## Notas de fidelidad

- El original fijaba `arduboy.setFrameRate(30)`. El bucle de la consola corre a
  60 FPS, así que `HopperPort::update()` ejecuta la lógica un frame de cada dos
  para conservar la velocidad exacta de coches y troncos.
- El README upstream indica que el juego está incompleto y **no define game
  over**; el port conserva ese comportamiento y no inventa colisiones ni estados
  finales. `Obstacle::intersects()` se mantiene tal cual, disponible para la
  versión original.
- `Hopper.ino` se conserva sin modificar como referencia histórica; PlatformIO
  solo compila los `.cpp` del cartucho.
