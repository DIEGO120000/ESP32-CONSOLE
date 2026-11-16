#include "console_core.h"

// Melodía de Victoria Ping Pong (1046Hz C6, 1318Hz E6, 1568Hz G6, 2093Hz C7)
const Note pingPongVictoryNotes[4] = {
    {1046, 80, 15},
    {1318, 80, 15},
    {1568, 80, 15},
    {2093, 200, 20}
};

// Melodía de Derrota Ping Pong (330Hz E4, 293Hz D4, 261Hz C4, 196Hz G3)
const Note pingPongDefeatNotes[4] = {
    {330, 120, 15},
    {293, 120, 15},
    {261, 140, 15},
    {196, 300, 20}
};

// Melodía de Victoria Tic-Tac-Toe (Ascendente: C5 523Hz, E5 659Hz, G5 784Hz, C6 1046Hz)
const Note tttVictoryNotes[4] = {
    {523, 70, 10},
    {659, 70, 10},
    {784, 70, 10},
    {1046, 200, 20}
};

// Melodía de Derrota Tic-Tac-Toe (Descendente: G5 784Hz, E5 659Hz, C5 523Hz, G4 392Hz)
const Note tttDefeatNotes[4] = {
    {784, 90, 10},
    {659, 90, 10},
    {523, 110, 10},
    {392, 250, 20}
};

// Melodía de Victoria Hangman (Ascendente festiva: E4 330Hz, F4 349Hz, C5 523Hz)
const Note hangmanVictoryNotes[3] = {
    {330, 100, 10},
    {349, 100, 10},
    {523, 250, 20}
};

// Melodía de Derrota Hangman (Descendente fúnebre: D4 294Hz, B3 247Hz, E3 165Hz, F2 87Hz)
const Note hangmanDefeatNotes[4] = {
    {294, 100, 10},
    {247, 100, 10},
    {165, 100, 10},
    {87, 250, 20}
};

MelodyPlayer melodyPlayer;
