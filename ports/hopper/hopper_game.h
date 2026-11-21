#pragma once

#include <Arduino.h>

// =============================================================================
// HOPPER - OBN-Y02 (obono/ArduboyWorks), MIT
//
// Port fiel del original para Arduboy 1.1.1. Se conservan game.cpp, title.cpp,
// logo.cpp y todos sus bitmaps, fisicas isometricas, niveles y record. La
// unica capa adaptada es MyArduboy (HAL) y este despachador de modos.
//
// Controles: cruceta mueve, A/B confirman/pausan. B mantenido ~0,7 s sale al
// menu de la consola (lo gestiona main.cpp, fuera del juego).
// =============================================================================

namespace HopperGame {
    void setup();
    void update();
    void draw();
    void loop();
}

class HopperPort {
public:
    void init();
    void update();
    void draw();

    bool isPlaying() const { return true; }
};
