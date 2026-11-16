#pragma once

#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"

// =============================================================================
// CYBER ESCAPE - rail shooter 3D (homenaje a The Lawnmower Man)
// Original: SKRTM 2026 - https://github.com/andreyfahrenheit/CyberEscape
//
// Port fiel del motor original: proyeccion de tunel/railes, cajas 3D con
// caras ocultas, piramides coleccionables, cubo de salida y fondos con
// dithering. Se conservan intactas las formulas, tablas PROGMEM y sprites;
// solo se adapto la capa de plataforma (buffer SSD1309, millis() y botones).
//
// Mapeo de controles en esta consola:
//   Cruceta        : mover la nave / mira
//   Boton A        : iniciar, avanzar de nivel y pausar/reanudar
//   Boton B        : pausa universal (main.cpp) / salir al menu de consola
// =============================================================================

extern Arduboy2 arduboy;
extern ArduboyTones sound;

class CyberEscapeGame {
public:
    CyberEscapeGame();

    void init();
    void update();
    void draw();

    bool isPlaying() const;

    bool exitRequested;   // el juego pide volver al menu de la consola

private:
    struct Impl;
    Impl *impl;
};
