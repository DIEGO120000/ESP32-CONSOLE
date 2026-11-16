# Sopa de Letras

Sopa de letras a pantalla completa con vistas deslizables, desarrollada para esta consola (ESP32-C6 / SSD1309 128x64).

Cuadrícula de 15x7 celdas de 8x8 px, tres dificultades, selección de palabras por extremos y fuente retro 5x5 propia.

## Contenido

| Archivo | Descripción |
|---|---|
| `sopa_letras.h` | Interfaz `WordSearchGame`, enums de estado/dificultad y fuente 5x5 |
| `sopa_letras.cpp` | Generación del tablero, validación de palabras, dibujado y audio |
| `media/` | Portadas y capturas |

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Mover el cursor por la cuadrícula / navegar menús |
| A | Seleccionar / marcar palabra |
| B | Pausa universal y salida al menú |

## Activación

```cpp
#define ENABLE_SOPA_LETRAS
```

## Dependencias

- `include/hangman_words.h` (diccionario compartido con Hangman, que todavía vive en `main.cpp`).
- `Arduboy2`, `ArduboyTones` y `Preferences` (récord persistente).
