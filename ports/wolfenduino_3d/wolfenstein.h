#pragma once

#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"

// =============================================================================
// WOLFENSTEIN 3D - Port de WolfenduinoFX para la consola ESP32-C6 (128x64)
//
// Motor, mapas, texturas, sprites y menus originales de James Howard
// (jhhoward/WolfenduinoFX, MIT): 10 niveles del episodio shareware, enemigos,
// armas, puertas secretas, 4 dificultades y tabla de records.
//
// Solo se adapto la capa de plataforma: los 261 KB de datos FX se incrustan en
// flash y se leen con diskRead(), el dibujado va al framebuffer de Arduboy2 y
// el guardado usa la EEPROM emulada. No toca el bus I2C ni main.cpp.
//
// Mapeo de controles:
//   Cruceta        : mover / girar (como el original)
//   Boton A        : disparar / usar
//   A + LEFT/RIGHT : strafe lateral
//   Doble toque A  : cambiar de arma
//   Boton B        : pausa universal (main.cpp); mantenido en menus sale
// =============================================================================

extern Arduboy2 arduboy;
extern ArduboyTones sound;

class WolfensteinGame {
public:
    WolfensteinGame();

    void init();
    void update();
    void draw();

    bool isPlaying() const;

    bool exitRequested;   // el juego pide volver al menu de la consola

private:
    struct Impl;
    Impl *impl;
};
