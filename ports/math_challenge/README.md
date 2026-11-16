# Math Challenge

Juego de cálculo mental con modo contrarreloj y modo relajado, cuatro categorías y teclado físico de botones.

## Contenido

| Archivo | Descripción |
|---|---|
| `math_challenge.h` | Implementación completa: clase `MathChallengeGame` (header-only, ~1.400 líneas) |
| `media/` | Portadas y capturas |

> No hay `.cpp`: la clase es completamente inline y se compila en la única unidad que la incluye (`main.cpp`). Si en el futuro se incluye desde más de un `.cpp`, habrá que separar las definiciones para evitar símbolos duplicados.

## Controles

| Entrada | Acción |
|---|---|
| Botones del teclado | Elegir respuesta (pines definidos en el propio port) |
| BOOT | Atajo de la pantalla de juego (`onBootShortPress`) |

## Activación

```cpp
#define ENABLE_MATH_CHALLENGE
```

## Dependencias

- `triggerRgbLed()` del núcleo (`main.cpp`) para el feedback del LED RGB.
- `Preferences` para el récord persistente y `ArduboyTones` para el audio.
