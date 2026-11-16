#pragma once

// =============================================================================
// GAMES CONFIG - interruptor maestro del catalogo de ports
//
// Comenta la macro de un port para excluirlo del binario. Su entrada del menu,
// su estado del sistema, su instancia, la pausa y el dibujado se compilan bajo
// el mismo #ifdef, y la numeracion del menu se recalcula sola: no quedan
// huecos ni referencias rotas.
//
// Uso:
//   #define ENABLE_SEA_BATTLE      // port incluido
//   // #define ENABLE_SEA_BATTLE   // port excluido
//
// Los ports viven en ports/<juego>/ como paquetes autocontenidos
// (.h + .cpp + README.md + media/). Ver ports/README.md.
//
// NOTA: los juegos que aun viven dentro de src/main.cpp (Ping Pong, Breakout,
// Space Invaders, 1943, Flappy Bird, Snake, Jump Man, 2048, T-Rex, Tic-Tac-Toe,
// BrainduTrain, Hangman y TRIS) NO se pueden desactivar todavia: su logica esta
// acoplada al nucleo. Se activaran aqui cuando se extraigan a ports/.
// =============================================================================

#define ENABLE_SEA_BATTLE
#define ENABLE_TRENCH_RUN
#define ENABLE_ROAD_TRIP
#define ENABLE_WOLFENDUINO_3D
#define ENABLE_CYBER_ESCAPE
#define ENABLE_SOPA_LETRAS
#define ENABLE_MATH_CHALLENGE

// Juegos extraidos a ports/ en la Fase 2:
#define ENABLE_PING_PONG
#define ENABLE_BREAKOUT
#define ENABLE_SPACE_INVADERS
#define ENABLE_1943
#define ENABLE_FLAPPY_BIRD
#define ENABLE_SNAKE
#define ENABLE_JUMP_MAN
#define ENABLE_2048
#define ENABLE_TREX_RUNNER
#define ENABLE_TIC_TAC_TOE
#define ENABLE_BRAINDU
#define ENABLE_HANGMAN
#define ENABLE_TRIS
