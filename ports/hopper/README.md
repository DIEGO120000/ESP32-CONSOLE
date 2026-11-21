# Hopper (OBN-Y02)

Port del juego original **Hopper** de OBONO, del repositorio oficial
[obono/ArduboyWorks](https://github.com/obono/ArduboyWorks) (carpeta `hopper/`),
licencia MIT.

Juego isometrico: salta sobre los paneles y sube; si caes hacia abajo, la
partida termina. Incluye logo, titulo animado, tabla de records, niveles y
sonido originales.

## Origen

| Archivo | Procedencia |
|---|---|
| `hopper.ino` | Original integro (referencia; PlatformIO no compila `.ino` de subcarpetas) |
| `common.h` | Original, encapsulado en `namespace HopperGame` |
| `game.cpp` | Original: fisicas, paneles, chips, niveles y puntuacion |
| `title.cpp` | Original: menu, records y creditos (solo se ajusto la lectura del puntero a 32 bits) |
| `logo.cpp` | Original |
| `MyArduboy.h/.cpp` | Capa HAL adaptada a `Arduboy2`/`ArduboyTones` |
| `hopper_eeprom.h/.cpp` | EEPROM aislada en NVS (`hopper_rec`) |
| `hopper_game.h/.cpp` | Despachador logo/titulo/juego para la consola |
| `LICENSE` | MIT del repositorio upstream |

## Adaptaciones de plataforma

- **Arduboy 1.1.1 -> HAL actual.** `MyArduboy` mantiene la API que usa el juego
  (`buttonDown/Pressed/Up`, `printEx`, fuente 6x6, `drawRect2`, `fillRect2`,
  `drawFastHLine2/VLine2`, `playScore2`). `beginNoLogo()` es no-op porque la
  consola ya inicializo pantalla y audio.
- **Sprites.** `drawBitmap` se reimplementa con el formato Arduboy por
  columnas/paginas; el HAL base usa formato horizontal y los graficos se veian
  transpuestos. Los arrays `PROGMEM` no se modifican.
- **Audio.** `playScore2` reproduce el formato de ArduboyPlaytune (0x90 nota,
  0x80 stop, esperas de 15 bits, mark/repeat/stop) sobre el buzzer PWM. Es
  monofonico, como el hardware de la consola.
- **Record.** Las funciones `eeprom_*` se emulan sobre un blob de 1 KB en la NVS
  `hopper_rec`; no comparten la EEPROM de otros cartuchos. Las escrituras se
  agrupan 250 ms para no castigar la flash.
- **Timing.** El original fija 60 FPS; la consola ya corre a 60, sin divisores.
- **Salida.** B mantenido ~0,7 s vuelve al menu de la consola. Un toque de B
  sigue siendo el control original (confirmar/pausar). La pausa universal no
  intercepta A/B en este cartucho.

## Integracion

```cpp
#define ENABLE_GAME_HOPPER 1   // include/games_config.h
```

`src/main.cpp` registra `STATE_HOPPER` en el carrusel, el panel `/config`, la
pausa y el despachador de update/draw. El cartucho vive en `ports/hopper/` y no
modifica ningun otro port ni `sim/`.
