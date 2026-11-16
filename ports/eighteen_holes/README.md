# 18 Holes — minigolf 3D de 18 hoyos (ArduGolf)

Cartucho de la consola ESP32-C6 Super Mini. Es un **port fiel** de *ArduGolf*: el
código original del juego (lógica, física, cámara, rasterizador y los 18 hoyos)
se compila tal cual; solo se sustituyó la capa de plataforma.

- Menú: **18 Holes**
- Macro: `ENABLE_EIGHTEEN_HOLES` (`include/games_config.h`)
- Estado del sistema: `STATE_EIGHTEEN_HOLES`
- Portada reservada: `media/cover.png` (128×64)

## Origen y licencia

| | |
|---|---|
| Juego | **ArduGolf / arduboy_minigolf** — minigolf en 3D con 18 hoyos |
| Autor | **brow1067** |
| Repositorio | <https://github.com/tiberiusbrown/arduboy_minigolf> |
| Licencia | **Mozilla Public License 2.0** — texto completo en [`core/LICENSE.txt`](core/LICENSE.txt) |

> **Nota sobre el encargo original.** El pedido hablaba de un juego oficial de
> Arduboy llamado «18 Holes», de *Press Play on Tape* (Filmote & Vampirics), con
> licencia BSD-3-Clause. Ese juego **no existe**: `Press-Play-On-Tape/18Holes` y
> `filmote/18Holes` devuelven «Repository not found», y el catálogo real de Press
> Play on Tape (Filmote + Vampirics) solo incluye `GolfCompanion`, que es una
> **tarjeta de puntuación** de golf, no un juego. El único minigolf de 18 hoyos
> publicado para Arduboy es **ArduGolf**, que es el que se ha portado aquí. Por
> eso la licencia es MPL-2.0 y no BSD-3-Clause.

## Qué se ha conservado del original

`core/` contiene **copias literales** de las fuentes del autor (renombradas a
`.inc` para que SCons no las compile por separado):

| Archivo | Contenido |
|---|---|
| `game.hpp` | tipos, constantes, `int24`/`uint24`, HAL mínima |
| `game.inc` | máquina de estados (`TITLE`, `LEVEL`, `AIM`, `ROLLING`, `HOLE`, `SCORE`, `MENU`, `OVERVIEW`, `PITCH`, `HISCORES`), puntuación, menús |
| `physics.inc` | trayectoria, gravedad, pendientes, rebotes, colisiones, agua/arena |
| `camera.inc`, `mat.inc`, `mul.inc`, `div.inc`, `sincos.inc` | cámara y matemáticas de coma fija |
| `render_scene.inc`, `draw.inc`, `graphics.inc`, `font.inc` | rasterizador 3D y gráficos |
| `save.inc` | tarjeta de puntuaciones (`best_game`, `best_holes`, partidas) |
| `levels/levels_default.hpp` | **los 18 hoyos originales en PROGMEM** |

No se ha reescrito ninguna mecánica, física ni nivel.

## Adaptación de plataforma (solo el pegamento)

`eighteen_holes.cpp` incluye todo el núcleo dentro de `namespace eighteen_holes` y
únicamente implementa lo que `game.hpp` declara como dependencia del sistema:

| Punto de la HAL | Implementación en la consola |
|---|---|
| `uint8_t* buf` | `Arduboy2Core::getBuffer()` — framebuffer 1bpp vertical por páginas (mismo formato que Arduboy: **nunca** se usa `drawBitmap` row-major de Adafruit GFX) |
| `time_ms()` | `millis()` |
| `poll_btns()` | `Arduboy2Core::currentButtonState` (los mismos bits que ve el menú de la consola) |
| `play_tone(...)` | `::sound.tone(...)` de `console_core.h` |
| `audio_enabled()` / `toggle_audio()` | `sound.getVolume()` / `setSoundEnabled()` — respeta el **mute global** |
| `save_audio_on_off()` | `saveSystemSettings()` (NVS `sys_cfg`) |
| EEPROM de puntuaciones | **NVS con `Preferences`**, namespace `18holes`, clave `save` |

Cambios mínimos en el núcleo, todos por plataforma y sin tocar la jugabilidad:

1. `ARDUGOLF_FX = 0`: no hay chip FX; se usa la ruta no-FX del propio autor con
   los 18 hoyos en flash interna.
2. `int24_t`/`uint24_t` y `__builtin_avr_fmuls`: solo en `__AVR__`; en el ESP32-C6
   se usan la emulación en 32 bits y `fmuls`/`fmuls8` portables del autor.
3. `#pragma GCC optimize(...)` de AVR: solo en `__AVR__`.
4. `extern "C"` de EEPROM → backend `Preferences` con caché de 1 KB en RAM y
   escritura diferida (`save()` vuelca una sola vez).

El guardado se lee de NVS una sola vez y se mantiene cacheado, porque el juego
llama a `load()` en cada frame de las vistas de hoyo.

## Controles

| Botón (GPIO) | En el juego | En la consola |
|---|---|---|
| LEFT (5) / RIGHT (19) | girar el ángulo de tiro · navegar menús · elegir hoyo en práctica | — |
| UP (14) / DOWN (18) | subir/bajar potencia · navegar menús | — |
| A (3) | **golpear** · confirmar · empezar partida | — |
| B (4) | menú interno del juego (vista general / inclinación / salir al título) · cancelar apuntado · saltar animaciones · alternar audio en el título | — |
| Click central (20) | — | **pausa universal**: Continuar / Menu Juego / Menu Consola |

> **Por qué la pausa no es B.** En ArduGolf B es una tecla de juego (abre el menú
> interno, cancela el apuntado y salta animaciones). Para no romper el original,
> la pausa universal de la consola se dispara con el **click central de la
> cruceta** en este cartucho; el resto de juegos siguen usando B. Por la misma
> razón 18 Holes queda excluido del combo A+B de salida rápida: A golpea y B abre
> el menú.

El juego conserva su propio menú interno (B durante el apuntado): *ver hoyo*,
*ajustar inclinación de cámara* y *volver al título*, más la selección de hoyo
1–18 en modo práctica (LEFT/RIGHT en la vista previa inicial).

## Guardado

- Namespace NVS `18holes`, clave `save` (blob de 1024 B equivalente a la EEPROM
  original; los records viven a partir del offset 700, igual que en el AVR).
- En `[env:pc_emulator]` se usa el mock `sim/Preferences.h` (archivo `sim_nvs.dat`).
- El formato y el checksum son los del autor, así que la estructura
  `ARDUGOLF` + `best_game[18]` + `best_holes[18]` + partidas jugadas se mantiene.

## Audio

Los tres tonos originales (golpe, penalización y hoyo) se reproducen con el
sintetizador de la consola. Si el volumen global está en mute (`sysVolume`), el
juego no suena y su icono de audio aparece silenciado; el título permite
alternar audio como en el original.

## Integración en el núcleo

1. `include/games_config.h`: `#define ENABLE_EIGHTEEN_HOLES`.
2. `src/main.cpp`: include, instancia `eighteenHoles`, `STATE_EIGHTEEN_HOLES`,
   entrada `{ "18 Holes", STATE_EIGHTEEN_HOLES }` en `menuEntries[]`, casos en
   `launchSelectedGame`, `isCurrentGameActive`, `currentGameHasSubmenu`,
   `returnToCurrentGameSubmenu` (→ título), `exitCurrentGameToMainMenu` y en los
   `switch` de tick/dibujado, más `universalPausePressed()` para mapear el click
   central en este cartucho.
3. `sim/main_sim.cpp`: entrada `"18 Holes"` en el menú del emulador.
4. Visibilidad: la máscara NVS `games_mask` del panel web lo incluye como una
   fila más; se puede ocultar/mostrar sin recompilar.

## Compilar y probar

```bash
pio run -e esp32c6_super_mini      # firmware ESP32-C6
pio run -e pc_emulator             # emulador SDL2
.pio/build/pc_emulator/program     # jugar en PC
pio run -e esp32c6_super_mini -t upload --upload-port /dev/ttyACM0
```

El juego corre a **~30 fps**, la cadencia del original (el `.ino` hacía
`game_loop()` + `delay(30)`), mantenida con un throttle interno para que las
velocidades y la física no cambien.

## Rendimiento y memoria

- Framebuffer compartido con la consola; además se guarda un **shadow buffer de
  1 KB** para congelar el último frame bajo el menú de pausa.
- Núcleo + 18 niveles: ~192 KB de fuentes, con los niveles en `PROGMEM`.
- El cartucho se puede desactivar con su macro sin tocar ningún otro juego.
