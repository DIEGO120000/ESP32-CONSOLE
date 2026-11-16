#pragma once
// =============================================================================
// 18 HOLES - ArduGolf (minigolf de 18 hoyos) para ESP32-C6 / emulador PC
// =============================================================================
// Cartucho autocontenido. Todo el juego original (logica, fisica, camara 3D,
// rasterizador y los 18 hoyos) vive en ports/eighteen_holes/core/ dentro del
// namespace `eighteen_holes`, sin reescribir mecanicas ni niveles.
//
// Origen: https://github.com/tiberiusbrown/arduboy_minigolf (ArduGolf, brow1067)
// Licencia: MPL-2.0 (ver README.md y core/LICENSE.txt).
// =============================================================================

#include <stdint.h>

class EighteenHolesGame {
public:
    // Carga el estado inicial y muestra la pantalla de titulo del juego.
    void init();

    // Ejecuta un tick de juego (logica + render en el framebuffer).
    // Se autolimita a ~30 fps, la cadencia original del Arduboy.
    void update();

    // Vuelca el ultimo frame dibujado. Permite que el menu de pausa de la
    // consola se superponga a la imagen congelada del juego.
    void draw();

    // true mientras haya una partida en curso (habilita la pausa universal).
    bool isPlaying() const;

    // Vuelve a la pantalla de titulo del juego (opcion "Menu Juego").
    void returnToTitle();

    // Compatibilidad con el patron de ports/: la salida al menu de la consola
    // la gestiona main.cpp (pausa universal + combo A+B).
    bool exitRequested = false;

private:
    uint32_t lastTickMs = 0;
    bool initialized = false;
};
