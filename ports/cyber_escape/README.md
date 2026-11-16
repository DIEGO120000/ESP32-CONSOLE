# Cyber Escape

Port de **Cyber Escape** de SKRTM (2026) — rail shooter 3D con estética de homenaje a *The Lawnmower Man*.

Se conservan intactas las fórmulas de proyección de túnel/raíles, las cajas 3D con caras ocultas, las pirámides coleccionables, el cubo de salida y los fondos con dithering. Solo se adaptó la capa de plataforma (framebuffer SSD1309, `millis()` y botones).

## Contenido

| Archivo | Descripción |
|---|---|
| `cyber_escape.h` | Interfaz `CyberEscapeGame` |
| `cyber_escape.cpp` | Motor de raíles, niveles, colisiones y render |
| `cyber_escape_assets.h` | Sprites y tablas PROGMEM |
| `media/` | Portadas y capturas |

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Mover la nave / mira |
| A | Iniciar, avanzar de nivel, pausar/reanudar |
| B | Pausa universal / salir al menú de consola |

## Activación

```cpp
#define ENABLE_CYBER_ESCAPE
```

## Notas de integración

- Invierte la pantalla en los niveles nocturnos; el núcleo llama a `init()` al salir del juego para restaurar la inversión.
- El combo universal de salida A+B está desactivado: A se usa para pausar/reanudar.
