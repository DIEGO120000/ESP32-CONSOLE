#pragma once

#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"

// =============================================================================
// ROAD TRIP
// Original: Press Play on Tape (Filmote & Vampirics), inspirado en Enduro.
// Licencia BSD 3-Clause - https://github.com/filmote/RoadTrip
//
// Port a la consola ESP32-C6 / OLED SSD1309 128x64.
//
// Mapeo de controles:
//   Cruceta LEFT / RIGHT : direccion
//   Cruceta UP o Boton A : acelerar
//   Cruceta DOWN         : frenar
//   Boton B              : pausa universal / salida al menu
// =============================================================================

extern Arduboy2 arduboy;
extern ArduboyTones sound;

class RoadTripGame {
public:
    RoadTripGame();

    void init();
    void update();
    void draw();

    bool isPlaying() const;

    bool exitRequested;   // el juego pide volver al menu de la consola
    bool gameOver;        // usado por isCurrentGameActive()

private:
    struct Impl;
    Impl *impl;
};
