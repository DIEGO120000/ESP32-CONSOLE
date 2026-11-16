#include "sea_battle.h"
#include "sea_battle_assets.h"

// =============================================================================
// SEA BATTLE - port fiel del original de Brady Heater (DarkestSpire, 2020)
// https://github.com/DarkestSpire/SEA_BATTLE
//
// Este archivo unifica Sea_Battle.ino, Startup.ino, Main_Menu.ino, Input.ino,
// Extra_Screens.ino, GamePhase1.ino, GamePhase2.ino y EnemyAI.ino. Los tableros
// 6x6, los barcos 2/3/4, la IA de patron ajedrezado con persecucion direccional
// y todas las pantallas son exactamente las del original. Solo se adapto la
// capa de plataforma:
//   - Arduboy2Base / Sprites -> capa local ESP32-C6 (SSD1309 128x64)
//   - delay(500) -> temporizadores millis() no bloqueantes
//   - tonesInRAM -> secuenciador de melodia con millis() + sound.tone()
//   - LEDs RGB del Arduboy -> no existen en esta consola, se omiten
//   - nextFrame()/display()/clear() -> los gestiona main.cpp a 60 FPS
// =============================================================================

// -----------------------------------------------------------------------------
// Notas (ArduboyTones no define NOTE_* en esta HAL local; valores del original)
// -----------------------------------------------------------------------------
#define NOTE_REST   0
#define NOTE_CS3    139
#define NOTE_DS3    156
#define NOTE_A3     220
#define NOTE_AS3    233
#define NOTE_B3     247
#define NOTE_C4     262
#define NOTE_A4     440
#define NOTE_B4     494
#define NOTE_CS5    554
#define TONES_END   0

namespace {

// -----------------------------------------------------------------------------
// Estados globales del juego (mismos valores que el original)
// -----------------------------------------------------------------------------
uint8_t gameState;
#define STARTUP            1
#define MAIN_MENU          2
#define INSTRUCTIONS       3
#define CREDITS            4
#define GAME_PHASE_1       5
#define GAME_PHASE_1_ENEMY 6
#define GAME_PHASE_2       7
#define WIN                8
#define LOSE               9
#define MODE_SELECT        10
#define GAME_PHASE_1_P2    11
#define TRANSITION         12

// Entrada (deteccion de flanco, identica al original)
int lastButtonPressed = 0;

// Fase 2
uint8_t gamePhaseStatus;
#define PLAYER_TURN_STATE 1
#define ENEMY_TURN_STATE  2
#define PLAYER_HIT        3
#define ENEMY_HIT         4
#define PLAYER_MISS       5
#define ENEMY_MISS        6
#define PLAYER_SINK       7
#define ENEMY_SINK        8

// Cursor de disparo [0] -> Y, [1] -> X
int pointer[2] = {0, 0};

// Tableros 6x6
// 0 -> Nada, 2/3/4 -> barco de ese tamano, 5 -> agua, 6 -> impacto
int playerBoard[6][6] = {
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};
int enemyBoard[6][6] = {
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}};

// Barco: [X][0] X, [X][1] Y, [X][2] 0 vertical / 1 horizontal, [X][3] tamano
int playerShips[3][4] = {{0, 0, 0, 2}, {0, 0, 0, 3}, {0, 0, 0, 4}};
int enemyShips[3][4]  = {{0, 0, 0, 2}, {0, 0, 0, 3}, {0, 0, 0, 4}};

int currentShipPlacement = 0;
int currentShipPlacement2 = 0;
uint8_t currentPlayer = 0;

// Modo 1v1 por turnos (dos jugadores humanos, hotseat)
bool twoPlayerMode = false;
const char* transitionTitle = "CAMBIO DE TURNO";
uint8_t transitionNextState = GAME_PHASE_2;
uint8_t transitionPendingStatus = 0;

// Menu principal
int pointerLocation = 1;
uint32_t missileCounter = 0;

// IA enemiga
bool boardIsEven;
int enemyPointer[2];
int enemyShot = 0;
int enemyPointerOriginalHit[2];
bool hitShipLookingForSink = false;
int searchDirection = 0;   // 0 arriba, 1 abajo, 2 izquierda, 3 derecha
int enemyHits = 0;
int lockedNum;

// Reloj de pantallas (parpadeo del titulo y espera de WIN/LOSE)
uint32_t clockCounter = 0;

// Temporizador no bloqueante que sustituye a los delay(500) del original
uint32_t statusSince = 0;
const uint32_t TURN_DELAY_MS = 500;
const uint32_t END_SCREEN_HOLD_MS = 4000;

// Musica de intro (misma melodia que introSong del original)
struct NoteEv { uint16_t freq; uint16_t dur; };
const NoteEv introSong[] = {
    {NOTE_CS3, 500}, {NOTE_REST, 500}, {NOTE_DS3, 500}, {NOTE_AS3, 1000},
    {NOTE_DS3, 500}, {NOTE_B3, 1000},  {NOTE_REST, 500}, {NOTE_CS3, 500},
    {NOTE_REST, 500}, {NOTE_DS3, 500}, {NOTE_CS3, 1000}, {NOTE_DS3, 500},
    {NOTE_A3, 500},  {NOTE_REST, 1000}, {TONES_END, 0}
};
const uint8_t INTRO_SONG_LEN = sizeof(introSong) / sizeof(introSong[0]);
uint8_t musicIdx = 0;
uint32_t musicNext = 0;
bool musicActive = false;

// -----------------------------------------------------------------------------
// Helpers de audio (mismo timbre y duraciones que el original)
// -----------------------------------------------------------------------------
void movingCursorSound() { sound.tone(NOTE_CS5, 30); }
void placeShipSound()    { sound.tone(NOTE_C4, 30); }
void hitShipSound()      { sound.tone(NOTE_A4, 30); }
void sinkShipSound()     { sound.tone(NOTE_B4, 30); }

void musicStart() {
    musicIdx = 0;
    musicNext = millis();
    musicActive = true;
}

void musicStop() {
    musicActive = false;
    sound.noTone();
}

// Secuenciador no bloqueante: cada nota se programa en su instante exacto y el
// render continua; nunca se espera dentro de un delay().
void musicUpdate() {
    if (!musicActive) return;
    uint32_t now = millis();
    if ((int32_t)(now - musicNext) < 0) return;

    const NoteEv &n = introSong[musicIdx];
    if (n.freq == TONES_END) {
        musicIdx = 0;
        musicNext = now + 500;
        return;
    }
    if (sound.getVolume() > 0) {
        sound.tone(n.freq, n.dur);
    }
    musicNext = now + n.dur;
    musicIdx = (musicIdx + 1 < INTRO_SONG_LEN) ? (musicIdx + 1) : 0;
}

// -----------------------------------------------------------------------------
// Entrada (Input.ino) - flanco descendente, igual que el original
// -----------------------------------------------------------------------------
int getUserInput() {
    if (arduboy.pressed(LEFT_BUTTON)) {
        if (lastButtonPressed == 1) return 0;
        lastButtonPressed = 1;
        return 1;
    } else if (arduboy.pressed(RIGHT_BUTTON)) {
        if (lastButtonPressed == 2) return 0;
        lastButtonPressed = 2;
        return 2;
    } else if (arduboy.pressed(DOWN_BUTTON)) {
        if (lastButtonPressed == 3) return 0;
        lastButtonPressed = 3;
        return 3;
    } else if (arduboy.pressed(UP_BUTTON)) {
        if (lastButtonPressed == 4) return 0;
        lastButtonPressed = 4;
        return 4;
    } else if (arduboy.pressed(A_BUTTON)) {
        if (lastButtonPressed == 5) return 0;
        lastButtonPressed = 5;
        return 5;
    } else if (arduboy.pressed(B_BUTTON)) {
        if (lastButtonPressed == 6) return 0;
        lastButtonPressed = 6;
        return 6;
    }
    lastButtonPressed = 0;
    return 0;
}

// -----------------------------------------------------------------------------
// Tableros (GamePhase1.ino - helpers)
// -----------------------------------------------------------------------------
void placeShip(int board[6][6], int ship[3][4]) {
    for (int i = 0; i < 3; i++) {
        if (ship[i][2] == 0) { // vertical
            for (int j = 0; j < ship[i][3]; j++) {
                board[ship[i][1] + j][ship[i][0]] = ship[i][3];
            }
        } else { // horizontal
            for (int j = 0; j < ship[i][3]; j++) {
                board[ship[i][1]][ship[i][0] + j] = ship[i][3];
            }
        }
    }
}

bool checkBoard(int board[6][6]) {
    int shipTwo = 0, shipThree = 0, shipFour = 0;
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            if (board[i][j] == 2) shipTwo++;
            else if (board[i][j] == 3) shipThree++;
            else if (board[i][j] == 4) shipFour++;
        }
    }
    return (shipTwo == 2) && (shipThree == 3) && (shipFour == 4);
}

bool shipOverlapsPlaced(int ships[3][4], int idx) {
    bool horiz = ships[idx][2] == 1;
    int len = ships[idx][3];
    for (int a = 0; a < len; a++) {
        int ax = ships[idx][0] + (horiz ? a : 0);
        int ay = ships[idx][1] + (horiz ? 0 : a);
        for (int k = 0; k < idx; k++) {
            bool horiz2 = ships[k][2] == 1;
            int len2 = ships[k][3];
            for (int b = 0; b < len2; b++) {
                int bx = ships[k][0] + (horiz2 ? b : 0);
                int by = ships[k][1] + (horiz2 ? 0 : b);
                if (ax == bx && ay == by) return true;
            }
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Disparos (GamePhase2.ino - helpers)
// 1 -> agua, 2 -> impacto, 3 -> hundido, 0 -> casilla no valida
// -----------------------------------------------------------------------------
int shootHere(int board[6][6], int ship[3][4], int location[2]) {
    if (board[location[0]][location[1]] == 2) {
        board[location[0]][location[1]] = 6;
        ship[0][3]--;
        return (ship[0][3] == 0) ? 3 : 2;
    } else if (board[location[0]][location[1]] == 3) {
        board[location[0]][location[1]] = 6;
        ship[1][3]--;
        return (ship[1][3] == 0) ? 3 : 2;
    } else if (board[location[0]][location[1]] == 4) {
        board[location[0]][location[1]] = 6;
        ship[2][3]--;
        return (ship[2][3] == 0) ? 3 : 2;
    } else if (board[location[0]][location[1]] == 0) {
        board[location[0]][location[1]] = 5;
        return 1;
    }
    return 0;
}

bool checkForAllSunk(int board[6][6]) {
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            if (board[i][j] == 2 || board[i][j] == 3 || board[i][j] == 4) {
                return false;
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// IA enemiga (EnemyAI.ino) - identica al original
// -----------------------------------------------------------------------------
void enemyAI() {
    enemyShot = 0;

    while (hitShipLookingForSink) {
        if (searchDirection == 0 && enemyPointerOriginalHit[0] - enemyHits >= 0) {
            enemyPointer[0] = enemyPointerOriginalHit[0] - enemyHits;
            enemyPointer[1] = enemyPointerOriginalHit[1];
            enemyShot = shootHere(playerBoard, playerShips, enemyPointer);
        } else if (searchDirection == 1 && enemyPointerOriginalHit[0] + enemyHits <= 5) {
            enemyPointer[0] = enemyPointerOriginalHit[0] + enemyHits;
            enemyPointer[1] = enemyPointerOriginalHit[1];
            enemyShot = shootHere(playerBoard, playerShips, enemyPointer);
        } else if (searchDirection == 2 && enemyPointerOriginalHit[1] - enemyHits >= 0) {
            enemyPointer[0] = enemyPointerOriginalHit[0];
            enemyPointer[1] = enemyPointerOriginalHit[1] - enemyHits;
            enemyShot = shootHere(playerBoard, playerShips, enemyPointer);
        } else if (searchDirection == 3 && enemyPointerOriginalHit[1] + enemyHits <= 5) {
            enemyPointer[0] = enemyPointerOriginalHit[0];
            enemyPointer[1] = enemyPointerOriginalHit[1] + enemyHits;
            enemyShot = shootHere(playerBoard, playerShips, enemyPointer);
        } else if (searchDirection > 3) {
            hitShipLookingForSink = false;
            searchDirection = 0;
        }

        if (enemyShot == 1) {
            enemyHits = 1;
            searchDirection++;
            break;
        } else if (enemyShot == 0) {
            enemyHits = 1;
            searchDirection++;
        } else if (enemyShot == 2 || enemyShot == 3) {
            break;
        }
    }

    lockedNum = 0;
    while (enemyShot == 0) {
        if (boardIsEven && lockedNum < 100) {
            enemyPointer[0] = random(0, 6);
            enemyPointer[1] = (enemyPointer[0] % 2) + random(0, 3) * 2;
            enemyShot = shootHere(playerBoard, playerShips, enemyPointer);
            lockedNum++;
        } else if (!boardIsEven && lockedNum < 100) {
            enemyPointer[0] = random(0, 6);
            enemyPointer[1] = ((random(0, 3) * 2) + 1) - (enemyPointer[0] % 2);
            enemyShot = shootHere(playerBoard, playerShips, enemyPointer);
            lockedNum++;
        } else {
            enemyPointer[0] = random(0, 6);
            enemyPointer[1] = random(0, 6);
            enemyShot = shootHere(playerBoard, playerShips, enemyPointer);
        }

        if (enemyShot == 2) {
            enemyPointerOriginalHit[0] = enemyPointer[0];
            enemyPointerOriginalHit[1] = enemyPointer[1];
        }
    }

    if (enemyShot == 1) {
        gamePhaseStatus = ENEMY_MISS;
    } else if (enemyShot == 2) {
        hitShipSound();
        gamePhaseStatus = ENEMY_HIT;
        hitShipLookingForSink = true;
        enemyHits++;
    } else if (enemyShot == 3) {
        sinkShipSound();
        gamePhaseStatus = ENEMY_SINK;
        hitShipLookingForSink = false;
        enemyHits = 0;
        searchDirection = 0;
        if (checkForAllSunk(playerBoard)) {
            clockCounter = millis();
            gameState = LOSE;
        }
    }
}

// -----------------------------------------------------------------------------
// Pantallas extra (Extra_Screens.ino)
// -----------------------------------------------------------------------------
void creditsTick() {
    if (getUserInput() != 0) gameState = MAIN_MENU;
}

void creditsRender() {
    arduboy.drawArduboyBitmap(0, 0, CREDITS_SCREEN, 128, 64, 1);
}

void instructionTick() {
    if (getUserInput() != 0) gameState = MAIN_MENU;
}

void instructionRender() {
    arduboy.drawArduboyBitmap(0, 0, HOW_TO_PLAY_SCREEN, 128, 64, 1);
}

void resetGameData();

// -----------------------------------------------------------------------------
// Seleccion de modo (1 jugador vs 2 jugadores)
// -----------------------------------------------------------------------------
void modeSelectTick() {
    int input = getUserInput();
    if (input == 5) { // A -> 1 jugador
        twoPlayerMode = false;
        resetGameData();
        gamePhaseStatus = (random(0, 2) == 0) ? PLAYER_TURN_STATE : ENEMY_TURN_STATE;
        boardIsEven = (random(0, 2) == 0);
        gameState = GAME_PHASE_1_ENEMY;
    } else if (input == 6) { // B -> 2 jugadores (hotseat)
        twoPlayerMode = true;
        resetGameData();
        gamePhaseStatus = PLAYER_TURN_STATE;
        boardIsEven = (random(0, 2) == 0);
        currentShipPlacement = 0;
        currentShipPlacement2 = 0;
        gameState = GAME_PHASE_1;
    }
}

void modeSelectRender() {
    arduboy.setCursor(16, 18);
    arduboy.print("MODO DE JUEGO");
    arduboy.setCursor(16, 32);
    arduboy.print("A: 1 JUGADOR");
    arduboy.setCursor(16, 44);
    arduboy.print("B: 2 JUGADORES");
}

void transitionTick() {
    if (getUserInput() == 5) {
        gameState = transitionNextState;
        if (transitionNextState == GAME_PHASE_2 && transitionPendingStatus != 0) {
            gamePhaseStatus = transitionPendingStatus;
            transitionPendingStatus = 0;
        }
        statusSince = millis();
    }
}

void transitionRender() {
    arduboy.setCursor(18, 24);
    arduboy.print(transitionTitle);
    arduboy.setCursor(18, 44);
    arduboy.print("PULSA A");
}

// -----------------------------------------------------------------------------
// Fase 1 jugador 2: colocacion de la flota del segundo jugador (tablero derecho)
// -----------------------------------------------------------------------------
void gamePhaseOnePlayer2Tick() {
    int input = getUserInput();

    if (input == 2) { // derecha
        if (enemyShips[currentShipPlacement2][2] == 0 &&
            enemyShips[currentShipPlacement2][0] < 5) {
            enemyShips[currentShipPlacement2][0]++;
            movingCursorSound();
        } else if (enemyShips[currentShipPlacement2][2] == 1 &&
                   enemyShips[currentShipPlacement2][0] <
                       6 - enemyShips[currentShipPlacement2][3]) {
            enemyShips[currentShipPlacement2][0]++;
            movingCursorSound();
        }
    }

    if (input == 1) { // izquierda
        if (enemyShips[currentShipPlacement2][0] > 0) {
            enemyShips[currentShipPlacement2][0]--;
            movingCursorSound();
        }
    }

    if (input == 3) { // abajo
        if (enemyShips[currentShipPlacement2][2] == 0 &&
            enemyShips[currentShipPlacement2][1] <
                6 - enemyShips[currentShipPlacement2][3]) {
            enemyShips[currentShipPlacement2][1]++;
            movingCursorSound();
        } else if (enemyShips[currentShipPlacement2][2] == 1 &&
                   enemyShips[currentShipPlacement2][1] < 5) {
            enemyShips[currentShipPlacement2][1]++;
            movingCursorSound();
        }
    }

    if (input == 4) { // arriba
        if (enemyShips[currentShipPlacement2][1] > 0) {
            enemyShips[currentShipPlacement2][1]--;
            movingCursorSound();
        }
    }

    if (input == 6) { // rotar (Boton B)
        if (enemyShips[currentShipPlacement2][2] == 0 &&
            enemyShips[currentShipPlacement2][0] <
                7 - enemyShips[currentShipPlacement2][3]) {
            enemyShips[currentShipPlacement2][2] = 1;
            movingCursorSound();
        } else if (enemyShips[currentShipPlacement2][1] <
                   7 - enemyShips[currentShipPlacement2][3]) {
            enemyShips[currentShipPlacement2][2] = 0;
            movingCursorSound();
        }
    }

    if (input == 5) { // confirmar barco
        if (shipOverlapsPlaced(enemyShips, currentShipPlacement2)) {
            sound.tone(NOTE_CS3, 40);
            return;
        }
        currentShipPlacement2++;
        placeShipSound();
        if (currentShipPlacement2 > 2) {
            placeShip(enemyBoard, enemyShips);
            if (checkBoard(enemyBoard)) {
                gameState = GAME_PHASE_2;
                statusSince = millis();
                currentPlayer = 0;
            } else {
                for (int i = 0; i < 6; i++) {
                    for (int j = 0; j < 6; j++) enemyBoard[i][j] = 0;
                }
                currentShipPlacement2 = 0;
                for (int i = 0; i < 3; i++) enemyShips[i][3] = i + 2;
            }
        }
    }
}

void gamePhaseOnePlayer2Render() {
    // Barra divisoria central
    arduboy.fillRect(62, 0, 4, 64, WHITE);

    // Cuadricula de despliegue en el lado derecho: marco doble + puntos interiores
    arduboy.fillRect(70, 2, 53, 2, WHITE);   // borde superior
    arduboy.fillRect(70, 53, 53, 2, WHITE);  // borde inferior
    arduboy.fillRect(70, 2, 2, 53, WHITE);   // borde izquierdo
    arduboy.fillRect(121, 2, 2, 53, WHITE);  // borde derecho
    for (int iy = 0; iy < 7; iy++)
        for (int ix = 0; ix < 7; ix++)
            arduboy.drawPixel(72 + ix * 8, 4 + iy * 8, WHITE);

    // Panel izquierdo: encabezado, iconos de referencia y leyenda
    arduboy.setCursor(4, 4);
    arduboy.print("PLACE SHIPS");
    arduboy.drawArduboyBitmap(4, 16, SHIP_SIZE_2_HORIZONTIAL, 16, 8);
    arduboy.drawArduboyBitmap(4, 28, SHIP_SIZE_3_HORIZONTIAL, 24, 8);
    arduboy.drawArduboyBitmap(4, 40, SHIP_SIZE_4_HORIZONTIAL, 32, 8);
    arduboy.setCursor(4, 48);
    arduboy.print("A: PLACE");
    arduboy.setCursor(4, 56);
    arduboy.print("B: ROTATE");

    if (currentShipPlacement2 == 0) {
        arduboy.drawArduboyBitmap(4, 8, POINTER, 8, 8, 1);
    } else if (currentShipPlacement2 == 1) {
        arduboy.drawArduboyBitmap(4, 20, POINTER, 8, 8, 1);
    } else if (currentShipPlacement2 == 2) {
        arduboy.drawArduboyBitmap(4, 32, POINTER, 8, 8, 1);
    }

    if (enemyShips[0][2] == 1) {
        arduboy.drawArduboyBitmap(72 + (8 * enemyShips[0][0]), 8 + (8 * enemyShips[0][1]),
                           SHIP_SIZE_2_HORIZONTIAL, 16, 8);
    } else {
        arduboy.drawArduboyBitmap(72 + (8 * enemyShips[0][0]), 8 + (8 * enemyShips[0][1]),
                           SHIP_SIZE_2_VERTICAL, 8, 16);
    }

    if (currentShipPlacement2 > 0) {
        if (enemyShips[1][2] == 1) {
            arduboy.drawArduboyBitmap(72 + (8 * enemyShips[1][0]), 8 + (8 * enemyShips[1][1]),
                               SHIP_SIZE_3_HORIZONTIAL, 24, 8);
        } else {
            arduboy.drawArduboyBitmap(72 + (8 * enemyShips[1][0]), 8 + (8 * enemyShips[1][1]),
                               SHIP_SIZE_3_VERTICAL, 8, 24);
        }
    }

    if (currentShipPlacement2 > 1) {
        if (enemyShips[2][2] == 1) {
            arduboy.drawArduboyBitmap(72 + (8 * enemyShips[2][0]), 8 + (8 * enemyShips[2][1]),
                               SHIP_SIZE_4_HORIZONTIAL, 32, 8);
        } else {
            arduboy.drawArduboyBitmap(72 + (8 * enemyShips[2][0]), 8 + (8 * enemyShips[2][1]),
                               SHIP_SIZE_4_VERTICAL, 8, 32);
        }
    }
}

// -----------------------------------------------------------------------------
// Arranque (Startup.ino)
// -----------------------------------------------------------------------------
void resetGameData() {
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            enemyBoard[i][j] = 0;
            playerBoard[i][j] = 0;
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            playerShips[i][j] = 0;
            enemyShips[i][j] = 0;
        }
    }
    for (int i = 0; i < 3; i++) {
        playerShips[i][3] = i + 2;
        enemyShips[i][3] = i + 2;
    }
    searchDirection = 0;
    enemyHits = 0;
    hitShipLookingForSink = false;
    enemyShot = 0;
    currentShipPlacement = 0;
    currentShipPlacement2 = 0;
    pointer[0] = 0;
    pointer[1] = 0;
    pointerLocation = 1;
}

void startupTick() {
    if (getUserInput() == 5) {
        resetGameData();

        // Quien empieza (imagen GamePhase2)
        gamePhaseStatus = (random(0, 2) == 0) ? PLAYER_TURN_STATE : ENEMY_TURN_STATE;
        // Patron ajedrezado de la IA
        boardIsEven = (random(0, 2) == 0);

        musicStop();
        gameState = MAIN_MENU;
    }
}

void startupRender() {
    arduboy.drawArduboyBitmap(0, 0, TITLE_SCREEN, 128, 64, 1);

    if (millis() - clockCounter < 400) {
        // sin parpadeo
    } else if (millis() - clockCounter < 800) {
        arduboy.drawArduboyBitmap(9, 23, PRESS_A, 32, 8, 1);
    } else {
        clockCounter = millis();
    }
}

// -----------------------------------------------------------------------------
// Menu principal (Main_Menu.ino)
// -----------------------------------------------------------------------------
void mainMenuTick() {
    int input = getUserInput();
    if (input == 3 && pointerLocation == 1) {
        pointerLocation = 2; movingCursorSound();
    } else if (input == 3 && pointerLocation == 2) {
        pointerLocation = 3; movingCursorSound();
    } else if (input == 3 && pointerLocation == 3) {
        pointerLocation = 4; movingCursorSound();
    } else if (input == 4 && pointerLocation == 4) {
        pointerLocation = 3; movingCursorSound();
    } else if (input == 4 && pointerLocation == 3) {
        pointerLocation = 2; movingCursorSound();
    } else if (input == 4 && pointerLocation == 2) {
        pointerLocation = 1; movingCursorSound();
    } else if (input == 5 && pointerLocation == 1) {
        musicStop();
        gameState = MODE_SELECT;
    } else if (input == 5 && pointerLocation == 2) {
        // Musica: en esta consola el audio global lo gestiona Ajustes; aqui se
        // silencia/reactiva solo la melodia del titulo.
        musicActive = !musicActive;
        if (!musicActive) sound.noTone();
        else musicStart();
    } else if (input == 5 && pointerLocation == 3) {
        gameState = CREDITS;
    } else if (input == 5 && pointerLocation == 4) {
        gameState = INSTRUCTIONS;
    }
}

void mainMenuRender() {
    arduboy.drawArduboyBitmap(0, 0, MENU_SCREEN, 128, 64, 1);

    if (pointerLocation == 1) {
        arduboy.drawArduboyBitmap(3, 6, POINTER, 8, 8, 1);
        arduboy.drawArduboyBitmap(76, 8, ANCHOR, 48, 48, 1);
    } else if (pointerLocation == 2) {
        arduboy.drawArduboyBitmap(3, 14, POINTER, 8, 8, 1);
        arduboy.drawArduboyBitmap(76, 8, MUSIC_NOTE, 48, 48, 1);
    } else if (pointerLocation == 3) {
        arduboy.drawArduboyBitmap(3, 22, POINTER, 8, 8, 1);
        arduboy.drawArduboyBitmap(76, 8, LIGHTHOUSE, 48, 48, 1);
    } else {
        arduboy.drawArduboyBitmap(3, 30, POINTER, 8, 8, 1);
        arduboy.drawArduboyBitmap(76, 8, COMPASS_ROSE, 48, 48, 1);
    }

    if (arduboy.audio.enabled() && musicActive) {
        arduboy.drawArduboyBitmap(28, 14, ON, 16, 8, 1);
    } else {
        arduboy.drawArduboyBitmap(28, 14, OFF, 16, 8, 1);
    }

    // Misil animado (igual que el original)
    if (millis() - missileCounter < 100) {
        arduboy.drawArduboyBitmap(2, 46, MISSILE_1_1, 64, 16, 1);
    } else if (millis() - missileCounter < 200) {
        arduboy.drawArduboyBitmap(2, 46, MISSILE_1_2, 64, 16, 1);
    } else if (millis() - missileCounter < 300) {
        arduboy.drawArduboyBitmap(2, 46, MISSILE_1_3, 64, 16, 1);
    } else if (millis() - missileCounter < 400) {
        arduboy.drawArduboyBitmap(2, 46, MISSILE_2_1, 64, 16, 1);
    } else if (millis() - missileCounter < 500) {
        arduboy.drawArduboyBitmap(2, 46, MISSILE_2_2, 64, 16, 1);
    } else {
        arduboy.drawArduboyBitmap(2, 46, MISSILE_2_3, 64, 16, 1);
        missileCounter = millis();
    }
}

// -----------------------------------------------------------------------------
// Fase 1: colocacion (GamePhase1.ino)
// -----------------------------------------------------------------------------
void gamePhaseOneEnemyTick() {
    uint16_t attempts = 0;
    while (true) {
        for (int i = 0; i < 3; i++) {
            enemyShips[i][2] = random(0, 2);
            if (enemyShips[i][2] == 0) {
                enemyShips[i][0] = random(0, 6);
                enemyShips[i][1] = random(0, 7 - enemyShips[i][3]);
            } else {
                enemyShips[i][0] = random(0, 7 - enemyShips[i][3]);
                enemyShips[i][1] = random(0, 6);
            }
        }

        placeShip(enemyBoard, enemyShips);
        if (checkBoard(enemyBoard)) break;

        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 6; j++) enemyBoard[i][j] = 0;
        }
        // Guarda de seguridad: el original no la necesita porque siempre
        // converge, pero evita un bloqueo del firmware en el caso patologico.
        if (++attempts > 1000) {
            break;
        }
    }

    gameState = GAME_PHASE_1;
}

void gamePhaseOneTick() {
    int input = getUserInput();

    if (input == 2) { // derecha
        if (playerShips[currentShipPlacement][2] == 0 &&
            playerShips[currentShipPlacement][0] < 5) {
            playerShips[currentShipPlacement][0]++;
            movingCursorSound();
        } else if (playerShips[currentShipPlacement][2] == 1 &&
                   playerShips[currentShipPlacement][0] <
                       6 - playerShips[currentShipPlacement][3]) {
            playerShips[currentShipPlacement][0]++;
            movingCursorSound();
        }
    }

    if (input == 1) { // izquierda
        if (playerShips[currentShipPlacement][0] > 0) {
            playerShips[currentShipPlacement][0]--;
            movingCursorSound();
        }
    }

    if (input == 3) { // abajo
        if (playerShips[currentShipPlacement][2] == 0 &&
            playerShips[currentShipPlacement][1] <
                6 - playerShips[currentShipPlacement][3]) {
            playerShips[currentShipPlacement][1]++;
            movingCursorSound();
        } else if (playerShips[currentShipPlacement][2] == 1 &&
                   playerShips[currentShipPlacement][1] < 5) {
            playerShips[currentShipPlacement][1]++;
            movingCursorSound();
        }
    }

    if (input == 4) { // arriba
        if (playerShips[currentShipPlacement][1] > 0) {
            playerShips[currentShipPlacement][1]--;
            movingCursorSound();
        }
    }

    if (input == 6) { // rotar (Boton B)
        if (playerShips[currentShipPlacement][2] == 0 &&
            playerShips[currentShipPlacement][0] <
                7 - playerShips[currentShipPlacement][3]) {
            playerShips[currentShipPlacement][2] = 1;
            movingCursorSound();
        } else if (playerShips[currentShipPlacement][1] <
                   7 - playerShips[currentShipPlacement][3]) {
            playerShips[currentShipPlacement][2] = 0;
            movingCursorSound();
        }
    }

    if (input == 5) { // confirmar barco
        if (shipOverlapsPlaced(playerShips, currentShipPlacement)) {
            sound.tone(NOTE_CS3, 40);
            return;
        }
        currentShipPlacement++;
        placeShipSound();
        if (currentShipPlacement > 2) {
            placeShip(playerBoard, playerShips);
            if (checkBoard(playerBoard)) {
                if (twoPlayerMode) {
                    for (int i = 0; i < 6; i++)
                        for (int j = 0; j < 6; j++) enemyBoard[i][j] = 0;
                    currentShipPlacement2 = 0;
                    for (int i = 0; i < 3; i++) enemyShips[i][3] = i + 2;
                    transitionTitle = "TURNO PLAYER 2";
                    transitionNextState = GAME_PHASE_1_P2;
                    transitionPendingStatus = 0;
                    gameState = TRANSITION;
                } else {
                    gameState = GAME_PHASE_2;
                    statusSince = millis();
                }
            } else {
                for (int i = 0; i < 6; i++) {
                    for (int j = 0; j < 6; j++) playerBoard[i][j] = 0;
                }
                currentShipPlacement = 0;
            }
        }
    }
}

void gamePhaseOneRender() {
    arduboy.drawArduboyBitmap(0, 0, GAME_PHASE_1_SCREEN, 128, 64, 1);

    if (currentShipPlacement == 0) {
        arduboy.drawArduboyBitmap(68, 19, POINTER, 8, 8, 1);
    } else if (currentShipPlacement == 1) {
        arduboy.drawArduboyBitmap(86, 19, POINTER, 8, 8, 1);
    } else if (currentShipPlacement == 2) {
        arduboy.drawArduboyBitmap(102, 19, POINTER, 8, 8, 1);
    }

    if (playerShips[0][2] == 1) {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[0][0]), 8 + (8 * playerShips[0][1]),
                           SHIP_SIZE_2_HORIZONTIAL, 16, 8);
    } else {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[0][0]), 8 + (8 * playerShips[0][1]),
                           SHIP_SIZE_2_VERTICAL, 8, 16);
    }

    if (currentShipPlacement > 0) {
        if (playerShips[1][2] == 1) {
            arduboy.drawArduboyBitmap(7 + (8 * playerShips[1][0]), 8 + (8 * playerShips[1][1]),
                               SHIP_SIZE_3_HORIZONTIAL, 24, 8);
        } else {
            arduboy.drawArduboyBitmap(7 + (8 * playerShips[1][0]), 8 + (8 * playerShips[1][1]),
                               SHIP_SIZE_3_VERTICAL, 8, 24);
        }
    }

    if (currentShipPlacement > 1) {
        if (playerShips[2][2] == 1) {
            arduboy.drawArduboyBitmap(7 + (8 * playerShips[2][0]), 8 + (8 * playerShips[2][1]),
                               SHIP_SIZE_4_HORIZONTIAL, 32, 8);
        } else {
            arduboy.drawArduboyBitmap(7 + (8 * playerShips[2][0]), 8 + (8 * playerShips[2][1]),
                               SHIP_SIZE_4_VERTICAL, 8, 32);
        }
    }
}

// -----------------------------------------------------------------------------
// Fase 2: combate (GamePhase2.ino)
// -----------------------------------------------------------------------------
void setPhaseStatus(uint8_t s) {
    gamePhaseStatus = s;
    statusSince = millis();
}

void gamePhaseTwoTick() {
    // En modo 2 jugadores, el segundo jugador controla el tablero izquierdo
    // cuando le toca el turno (estados pares).
    if (twoPlayerMode && gamePhaseStatus == ENEMY_TURN_STATE) {
        int input = getUserInput();
        if (input == 2 && pointer[1] < 5) {
            pointer[1]++; movingCursorSound();
        } else if (input == 1 && pointer[1] > 0) {
            pointer[1]--; movingCursorSound();
        } else if (input == 3 && pointer[0] < 5) {
            pointer[0]++; movingCursorSound();
        } else if (input == 4 && pointer[0] > 0) {
            pointer[0]--; movingCursorSound();
        } else if (input == 5) {
            int shoot = shootHere(playerBoard, playerShips, pointer);
            if (shoot == 2) {
                hitShipSound();
                setPhaseStatus(ENEMY_HIT);
            } else if (shoot == 3) {
                sinkShipSound();
                setPhaseStatus(ENEMY_SINK);
                if (checkForAllSunk(playerBoard)) {
                    clockCounter = millis();
                    gameState = LOSE;
                }
            } else if (shoot == 1) {
                setPhaseStatus(ENEMY_MISS);
            }
        }
        return;
    }

    // Turno del jugador: cursor y disparo
    if (gamePhaseStatus == PLAYER_TURN_STATE) {
        int input = getUserInput();
        if (input == 2 && pointer[1] < 5) {
            pointer[1]++; movingCursorSound();
        } else if (input == 1 && pointer[1] > 0) {
            pointer[1]--; movingCursorSound();
        } else if (input == 3 && pointer[0] < 5) {
            pointer[0]++; movingCursorSound();
        } else if (input == 4 && pointer[0] > 0) {
            pointer[0]--; movingCursorSound();
        } else if (input == 5) {
            int shoot = shootHere(enemyBoard, enemyShips, pointer);
            if (shoot == 2) {
                hitShipSound();
                setPhaseStatus(PLAYER_HIT);
            } else if (shoot == 3) {
                sinkShipSound();
                setPhaseStatus(PLAYER_SINK);
                if (checkForAllSunk(enemyBoard)) {
                    clockCounter = millis();
                    gameState = WIN;
                }
            } else if (shoot == 1) {
                setPhaseStatus(PLAYER_MISS);
            }
        }
        return;
    }

    // Sustituto no bloqueante del delay(500) del original
    if (millis() - statusSince < TURN_DELAY_MS) return;

    if (gamePhaseStatus == ENEMY_TURN_STATE) {
        enemyAI();
        statusSince = millis();
    } else if (gamePhaseStatus == PLAYER_MISS) {
        // 2P: los barcos permanecen ocultos, asi que no hay nada que tapar.
        // El turno pasa directo a P2 sin pantalla negra ni "PULSA A".
        if (twoPlayerMode) pointer[0] = pointer[1] = 0;
        setPhaseStatus(ENEMY_TURN_STATE);
    } else if (gamePhaseStatus == PLAYER_HIT || gamePhaseStatus == PLAYER_SINK) {
        setPhaseStatus(PLAYER_TURN_STATE);
    } else if (gamePhaseStatus == ENEMY_MISS) {
        // 2P: regreso inmediato a P1, sin pantalla intermedia.
        if (twoPlayerMode) pointer[0] = pointer[1] = 0;
        setPhaseStatus(PLAYER_TURN_STATE);
    } else if (gamePhaseStatus == ENEMY_HIT || gamePhaseStatus == ENEMY_SINK) {
        setPhaseStatus(ENEMY_TURN_STATE);
    }
}

void drawPlayerShips() {
    // En modo 2 jugadores se ocultan los barcos intactos (Pass & Play)
    if (twoPlayerMode) return;
    if (playerShips[0][2] == 1) {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[0][0]), 4 + (8 * playerShips[0][1]),
                           SHIP_SIZE_2_HORIZONTIAL, 16, 8);
    } else {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[0][0]), 4 + (8 * playerShips[0][1]),
                           SHIP_SIZE_2_VERTICAL, 8, 16);
    }
    if (playerShips[1][2] == 1) {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[1][0]), 4 + (8 * playerShips[1][1]),
                           SHIP_SIZE_3_HORIZONTIAL, 24, 8);
    } else {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[1][0]), 4 + (8 * playerShips[1][1]),
                           SHIP_SIZE_3_VERTICAL, 8, 24);
    }
    if (playerShips[2][2] == 1) {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[2][0]), 4 + (8 * playerShips[2][1]),
                           SHIP_SIZE_4_HORIZONTIAL, 32, 8);
    } else {
        arduboy.drawArduboyBitmap(7 + (8 * playerShips[2][0]), 4 + (8 * playerShips[2][1]),
                           SHIP_SIZE_4_VERTICAL, 8, 32);
    }
}

void gamePhaseTwoRender() {
    arduboy.drawArduboyBitmap(0, 0, GAME_PHASE_2_SCREEN, 128, 64, 1);

    // Marcador del cursor de disparo: en modo 2 jugadores se dibuja en el
    // tablero activo (derecho en turnos impares, izquierdo en turnos pares).
    if (twoPlayerMode && (gamePhaseStatus % 2 == 0)) {
        arduboy.drawArduboyBitmap(7 + (8 * pointer[1]), 4 + (8 * pointer[0]), MARKER, 8, 8);
    } else {
        arduboy.drawArduboyBitmap(72 + (8 * pointer[1]), 4 + (8 * pointer[0]), MARKER, 8, 8);
    }

    if (!twoPlayerMode) drawPlayerShips();

    // En 2P, revelar un barco solo cuando ha sido hundido por completo
    if (twoPlayerMode) {
        for (int i = 0; i < 3; i++) {
            if (playerShips[i][3] == 0) {
                if (playerShips[i][2] == 1) {
                    arduboy.drawArduboyBitmap(7 + (8 * playerShips[i][0]), 4 + (8 * playerShips[i][1]),
                                       (i == 0) ? SHIP_SIZE_2_HORIZONTIAL : (i == 1) ? SHIP_SIZE_3_HORIZONTIAL : SHIP_SIZE_4_HORIZONTIAL,
                                       (i == 0) ? 16 : (i == 1) ? 24 : 32, 8);
                } else {
                    arduboy.drawArduboyBitmap(7 + (8 * playerShips[i][0]), 4 + (8 * playerShips[i][1]),
                                       (i == 0) ? SHIP_SIZE_2_VERTICAL : (i == 1) ? SHIP_SIZE_3_VERTICAL : SHIP_SIZE_4_VERTICAL,
                                       8, (i == 0) ? 16 : (i == 1) ? 24 : 32);
                }
            }
        }
        for (int i = 0; i < 3; i++) {
            if (enemyShips[i][3] == 0) {
                if (enemyShips[i][2] == 1) {
                    arduboy.drawArduboyBitmap(72 + (8 * enemyShips[i][0]), 4 + (8 * enemyShips[i][1]),
                                       (i == 0) ? SHIP_SIZE_2_HORIZONTIAL : (i == 1) ? SHIP_SIZE_3_HORIZONTIAL : SHIP_SIZE_4_HORIZONTIAL,
                                       (i == 0) ? 16 : (i == 1) ? 24 : 32, 8);
                } else {
                    arduboy.drawArduboyBitmap(72 + (8 * enemyShips[i][0]), 4 + (8 * enemyShips[i][1]),
                                       (i == 0) ? SHIP_SIZE_2_VERTICAL : (i == 1) ? SHIP_SIZE_3_VERTICAL : SHIP_SIZE_4_VERTICAL,
                                       8, (i == 0) ? 16 : (i == 1) ? 24 : 32);
                }
            }
        }
    }

    // Marcas de agua/impacto en ambos tableros
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            if (playerBoard[i][j] == 5) {
                arduboy.drawArduboyBitmap(7 + (8 * j), 4 + (8 * i), MISS_MARKER, 8, 8);
            } else if (playerBoard[i][j] == 6) {
                // 1P: el barco propio se dibuja siempre, asi que hay que borrar
                // la celda antes de pintar el impacto. 2P: en el tablero
                // izquierdo el barco solo aparece cuando esta hundido; borrar la
                // celda eliminaba el sprite revelado y rompia la simetria con el
                // tablero derecho, donde el HIT_MARKER se pinta transparente.
                if (!twoPlayerMode) {
                    arduboy.fillRect(7 + (8 * j), 4 + (8 * i), 8, 8, 0);
                }
                arduboy.drawArduboyBitmap(7 + (8 * j), 4 + (8 * i), HIT_MARKER, 8, 8);
            }
            if (enemyBoard[i][j] == 5) {
                arduboy.drawArduboyBitmap(72 + (8 * j), 4 + (8 * i), MISS_MARKER, 8, 8);
            } else if (enemyBoard[i][j] == 6) {
                arduboy.drawArduboyBitmap(72 + (8 * j), 4 + (8 * i), HIT_MARKER, 8, 8);
            }
        }
    }

    switch (gamePhaseStatus) {
        case PLAYER_TURN_STATE:
            if (twoPlayerMode) { arduboy.setCursor(73, 56); arduboy.print("P1 TURN"); }
            else arduboy.drawArduboyBitmap(73, 56, YOUR_TURN, 48, 8);
            break;
        case ENEMY_TURN_STATE:
            if (twoPlayerMode) { arduboy.setCursor(73, 56); arduboy.print("P2 TURN"); }
            else arduboy.drawArduboyBitmap(8, 56, ENEMY_TURN, 48, 8);
            break;
        case PLAYER_HIT:        arduboy.drawArduboyBitmap(81, 56, HIT, 32, 8); break;
        case ENEMY_HIT:         arduboy.drawArduboyBitmap(16, 56, HIT, 32, 8); break;
        case PLAYER_MISS:       arduboy.drawArduboyBitmap(81, 56, MISS, 32, 8); break;
        case ENEMY_MISS:        arduboy.drawArduboyBitmap(16, 56, MISS, 32, 8); break;
        case PLAYER_SINK:       arduboy.drawArduboyBitmap(81, 56, SINK, 32, 8); break;
        case ENEMY_SINK:        arduboy.drawArduboyBitmap(16, 56, SINK, 32, 8); break;
        default: break;
    }
}

// -----------------------------------------------------------------------------
// Pantallas finales
// -----------------------------------------------------------------------------
void gameWinRender() {
    arduboy.drawArduboyBitmap(0, 0, WIN_SCREEN, 128, 64, 1);
}

void gameLoseRender() {
    arduboy.drawArduboyBitmap(0, 0, LOSE_SCREEN, 128, 64, 1);
}

void endScreenTick() {
    if (millis() - clockCounter >= END_SCREEN_HOLD_MS) {
        if (getUserInput() == 5) {
            gameState = STARTUP;
            clockCounter = millis();
            musicStart();
        }
    }
}

} // namespace

// =============================================================================
// Interfaz publica
// =============================================================================
SeaBattleGame::SeaBattleGame() {
    exitRequested = false;
}

void SeaBattleGame::init() {
    exitRequested = false;
    gameState = STARTUP;
    clockCounter = millis();
    missileCounter = millis();
    resetGameData();
    gamePhaseStatus = PLAYER_TURN_STATE;
    boardIsEven = true;
    statusSince = millis();
    musicStart();
}

void SeaBattleGame::update() {
    switch (gameState) {
        case STARTUP:
            startupTick();
            break;
        case MAIN_MENU:
            mainMenuTick();
            break;
        case INSTRUCTIONS:
            instructionTick();
            break;
        case CREDITS:
            creditsTick();
            break;
        case MODE_SELECT:
            modeSelectTick();
            break;
        case GAME_PHASE_1_P2:
            gamePhaseOnePlayer2Tick();
            break;
        case TRANSITION:
            transitionTick();
            break;
        case GAME_PHASE_1_ENEMY:
            gamePhaseOneEnemyTick();
            break;
        case GAME_PHASE_1:
            gamePhaseOneTick();
            break;
        case GAME_PHASE_2:
            gamePhaseTwoTick();
            break;
        case WIN:
        case LOSE:
            endScreenTick();
            break;
        default:
            break;
    }

    // Secuenciador de la melodia del titulo (no bloqueante)
    musicUpdate();

    // Salida limpia al menu de la consola con B en pantallas no jugables.
    // Durante la partida, main.cpp intercepta B como pausa universal.
    if (!isPlaying() && arduboy.justPressed(B_BUTTON)) {
        exitRequested = true;
    }
}

void SeaBattleGame::draw() {
    switch (gameState) {
        case STARTUP:            startupRender(); break;
        case MAIN_MENU:          mainMenuRender(); break;
        case INSTRUCTIONS:       instructionRender(); break;
        case CREDITS:            creditsRender(); break;
        case MODE_SELECT:        modeSelectRender(); break;
        case GAME_PHASE_1_P2:    gamePhaseOnePlayer2Render(); break;
        case TRANSITION:         transitionRender(); break;
        case GAME_PHASE_1_ENEMY: gamePhaseOneRender(); break;
        case GAME_PHASE_1:       gamePhaseOneRender(); break;
        case GAME_PHASE_2:       gamePhaseTwoRender(); break;
        case WIN:
            if (millis() - clockCounter < END_SCREEN_HOLD_MS) gamePhaseTwoRender();
            else gameWinRender();
            break;
        case LOSE:
            if (millis() - clockCounter < END_SCREEN_HOLD_MS) gamePhaseTwoRender();
            else gameLoseRender();
            break;
        default: break;
    }
}

bool SeaBattleGame::isPlaying() const {
    return gameState == GAME_PHASE_1 || gameState == GAME_PHASE_1_ENEMY ||
           gameState == GAME_PHASE_1_P2 || gameState == GAME_PHASE_2;
}

bool SeaBattleGame::isCombat() const {
    return gameState == GAME_PHASE_2;
}
