#pragma once

#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"

// =============================================================================
// CATACOMBS OF THE DAMNED - port fiel para la consola ESP32-C6 (128x64)
//
// Motor, mapas generados, texturas vectoriales con dithering, enemigos, bolas de
// fuego, objetos, HUD y menus originales de James Howard
// (jhhoward/Arduboy3D, licencia MIT), compilados sin reescribir el motor.
//
// Adaptaciones de plataforma:
//   - Entrada desde el estado ya sondeado por main.cpp (sin doble pollButtons).
//   - Framebuffer del OLED SSD1309 via Arduboy2.
//   - Tonos originales traducidos al buzzer GPIO 1 o al audio SDL2 del emulador.
//   - Record final en NVS, namespace "catacombs" (en PC: sim_nvs.dat).
//
// Controles:
//   Cruceta arriba/abajo : avanzar / retroceder
//   Cruceta izq./der.    : girar la vista
//   A + izq./der.        : strafe lateral (como A en el original)
//   A                    : lanzar bola de fuego / interactuar (era B)
//   B                    : pausa universal de la consola
//   B mantenido en menus : volver al menu de la consola
// =============================================================================

class Catacombs3DGame
{
public:
    Catacombs3DGame();

    void init();
    void update();
    void draw();

    bool isPlaying() const;

    bool exitRequested; // el juego pide volver al menu de la consola

private:
    struct Impl;
    Impl *impl;
};
