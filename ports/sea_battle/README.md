# Sea Battle

Port fiel del original de **Brady Heater (DarkestSpire), 2020** — estrategia naval por turnos sobre tableros de 6x6.

## Contenido

| Archivo | Descripción |
|---|---|
| `sea_battle.h` | Interfaz `SeaBattleGame` (init / update / draw / isPlaying / isCombat) |
| `sea_battle.cpp` | Lógica completa: menús, colocación, combate, IA y render |
| `sea_battle_assets.h` | Sprites y pantallas PROGMEM extraídos byte a byte del original |
| `media/` | Portadas y capturas para la ficha del repo |

## Modos

- **1 jugador:** colocación manual y combate contra la IA (patrón ajedrezado con persecución direccional).
- **2 jugadores (hotseat):** P1 coloca su flota, pantalla de privacidad `TURNO PLAYER 2`, P2 coloca y se combate por turnos. Los disparos no muestran pantallas negras: solo `HIT` / `MISS` / `SINK` durante 500 ms y el indicador inferior cambia a `P1 TURN` / `P2 TURN`.
- En 2P los barcos permanecen ocultos hasta hundirse; al hundir una nave se revela su sprite completo en ambos tableros.

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Mover barco (colocación) / mover cursor de disparo |
| A | Confirmar colocación / disparar |
| B | Rotar barco (colocación); pausa universal en combate |

## Activación

En `include/games_config.h`:

```cpp
#define ENABLE_SEA_BATTLE
```

## Integración

- Usa el framebuffer de `Arduboy2` y `ArduboyTones`; no toca el bus I2C ni los pines (definidos en `Arduboy2Core.h`).
- Los `delay(500)` del original son temporizadores `millis()` no bloqueantes.
- `isCombat()` distingue la fase de combate para que el núcleo gestione la pausa con B; en colocación B rota el barco.
