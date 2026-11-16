#pragma once

#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"

// =============================================================================
// SEA BATTLE - estrategia naval por turnos (port fiel del original Arduboy)
// Original: Brady Heater (DarkestSpire) 2020
// https://github.com/DarkestSpire/SEA_BATTLE
//
// Se unifican en este modulo los archivos originales Sea_Battle.ino,
// Startup.ino, Main_Menu.ino, Input.ino, Extra_Screens.ino, GamePhase1.ino,
// GamePhase2.ino y EnemyAI.ino. Toda la logica (tableros 6x6, colocacion de
// barcos 2/3/4, fase de disparo, IA de patron ajedrezado con persecucion
// direccional y pantallas) se conserva identica al original. Los bitmaps
// PROGMEM viven en games/sea_battle_assets.h, extraidos byte a byte.
//
// Adaptaciones de plataforma (unicas):
//   - Arduboy2Base/ArduboyTones -> capa local ESP32-C6 / SSD1309 128x64
//   - Los delay(500) de la fase de combate se sustituyen por temporizadores
//     millis() no bloqueantes (el render sigue a 60 FPS)
//   - La melodia de intro (tonesInRAM) se secuencia con millis() y sound.tone()
//   - Los LEDs RGB del Arduboy original no existen aqui: se omiten
//
// Mapeo de controles en esta consola:
//   Cruceta : mover barco/cursor por la cuadricula
//   Boton A : confirmar colocacion / disparar torpedo / avanzar menus
//   Boton B : rotar barco en la fase de colocacion
//             (la pausa universal y la salida al menu las gestiona main.cpp)
// =============================================================================

extern Arduboy2 arduboy;
extern ArduboyTones sound;

class SeaBattleGame {
public:
    SeaBattleGame();

    void init();
    void update();
    void draw();

    bool isPlaying() const;
    // true solo durante la fase de combate: alli B es la pausa universal.
    // En la colocacion B rota el barco y no debe ser interceptado por main.cpp.
    bool isCombat() const;

    bool exitRequested;   // el juego pide volver al menu de la consola
};

extern SeaBattleGame seaBattle;
