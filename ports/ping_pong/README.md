# Ping Pong

El clásico Pong con cuatro modos: partida normal, modo invertido, dodge y **2P por Wi-Fi** con mandos móviles.

## Contenido

| Archivo | Descripción |
|---|---|
| `ping_pong.h` | Clase `PingPongGame` (header-only): menú interno, físicas, IA, modos y lobby |
| `ping_pong.cpp` | Tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| UP / DOWN | Mover la pala del jugador 1 |
| A / B | Mover la pala del jugador 2 |
| A | Sacar / revancha |
| B | Menú del juego / salir en el lobby |

En **2P Wi-Fi Gamepads** la consola levanta el AP `AP RetroArcade-ESP32` y cada
jugador controla su pala desde el navegador del móvil. Durante una partida LAN
la pantalla no entra en standby.

## Activación

```cpp
#define ENABLE_PING_PONG
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- `wifiGamepads` (`WiFiGamepadServer.h`) para el modo 2P.
- La pausa del núcleo consulta `mode`, `wifiMatchStarted` y `gameOver` para el
  keep-awake de la partida LAN.
