# Hangman

El ahorcado, port de Arduboy de serisman, con lista de palabras propia y estadísticas persistentes.

## Contenido

| Archivo | Descripción |
|---|---|
| `hangman.h` | Clase `HangmanGame` (header-only) |
| `hangman_words.h` | Diccionario PROGMEM de palabras |
| `hangman.cpp` | Tablas PROGMEM del port |
| `media/` | Portada (`cover.png`) y capturas |

## Controles

| Entrada | Acción |
|---|---|
| UP / LEFT, DOWN / RIGHT | Recorrer letras |
| A | Elegir letra / confirmar |
| B | Volver / pausa del menú interno |
| BOOT (pulsación corta) | Jugar / confirmar |

## Activación

```cpp
#define ENABLE_HANGMAN
```

## Dependencias

- `console_core.h`: `arduboy`, `sound`, `melodyPlayer`, `triggerRgbLed`.
- `Preferences` (NVS `hangman_nvs`) para victorias y derrotas.
- `hangman_words.h` se incluye desde el propio port.
