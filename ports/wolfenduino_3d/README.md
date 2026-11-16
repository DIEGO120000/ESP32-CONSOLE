# Wolfenduino 3D

Port de **WolfenduinoFX** de James Howard (`jhhoward/WolfenduinoFX`), licencia **MIT**.

Motor de raycasting con los 10 niveles del episodio shareware, enemigos, armas, puertas secretas, 4 dificultades y tabla de récords. Los ~261 KB de datos FX se incrustan en flash y se leen con `diskRead()`.

## Contenido

| Archivo | Descripción |
|---|---|
| `wolfenstein.h` | Interfaz `WolfensteinGame` (patrón pimpl, sin exponer el motor) |
| `wolfenstein.cpp` | Adaptación de la capa de plataforma (framebuffer, EEPROM, entrada) |
| `wolf3d/` | Motor original completo: renderer, mapas, actores, menús, datos FX |
| `media/` | Portadas y capturas |

## Controles

| Entrada | Acción |
|---|---|
| Cruceta | Mover / girar |
| A | Disparar / usar |
| A + LEFT / RIGHT | Strafe lateral |
| Doble toque A | Cambiar de arma |
| B | Pausa universal (en menús, mantenido, sale) |

## Activación

```cpp
#define ENABLE_WOLFENDUINO_3D
```

## Notas de integración

- Es el port más pesado del catálogo (~800 KB de flash). Desactivarlo deja el binario en torno al 65 %.
- `wolf3d/` se compila gracias a `build_src_filter = +<*> +<../ports/>` en `platformio.ini`.
- No toca el bus I2C ni los pines.
