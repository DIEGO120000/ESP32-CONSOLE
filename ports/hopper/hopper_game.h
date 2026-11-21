#pragma once

#include <Arduino.h>

// =============================================================================
// HOPPER - port fiel del cartucho original de bigkevmcd/arduboy-hopper (MIT)
//
// Fuente upstream (rama master): Hopper.ino, hopper.h, frog.*, lane.*,
// obstacle.*. La logica, los carriles, las velocidades, el tamano de los
// obstaculos y el sprite de la rana son los originales, y el cartucho entero
// vive dentro de namespace HopperGame para no colisionar con otros ports.
//
// Adaptaciones de plataforma (sin cambios de mecanica):
//   - HAL propio: Arduboy2 / ArduboyTones de la consola (I2C 7/6, buzzer GPIO 1).
//   - El original fijaba 30 FPS en setup(); aqui HopperPort::update() ejecuta la
//     logica un frame de cada dos del bucle de 60 FPS, mismo ritmo efectivo.
//   - reset en cada entrada al cartucho (init()), como el resto de ports.
//   - B abre la pausa universal de la consola (Hopper no usa A ni B).
// =============================================================================

namespace HopperGame {
    // Entrada y ciclo del cartucho, equivalentes al setup()/loop() del .ino.
    void setup();
    void update();
    void draw();
    void loop();
}

// Interfaz unificada que usa src/main.cpp para todos los cartuchos.
class HopperPort {
public:
    void init();
    void update();
    void draw();

    // El original no tiene game over (README upstream: "not yet complete"),
    // asi que la partida siempre admite pausa.
    bool isPlaying() const { return true; }

private:
    uint8_t frameDivider = 0;
};
