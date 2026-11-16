#pragma once

#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"

// =============================================================================
// TRENCH RUN
// Original: Lucas Cardinali (lscardinali) - licencia MIT
// https://github.com/lscardinali/TrenchRun-Arduboy
//
// Restauracion del juego original: vuelo por la trinchera espacial, esquiva y
// derribo de cazas TIE y disparo final al ducto termico (Exhaust Port).
//
// Mapeo de controles en esta consola:
//   Cruceta: mueve la mira
//   Boton A: disparo laser
//   Boton B: pausa universal (gestionada por main.cpp)
// =============================================================================

extern Arduboy2 arduboy;
extern ArduboyTones sound;

#define TRENCH_FPS                        60

#define TRENCH_DISTANCE_FROM_EXAUST_PORT  7350
#define TRENCH_DISTANCE_ENABLE_TIE2       5510
#define TRENCH_DISTANCE_ENABLE_SPEED      3670
#define TRENCH_DISTANCE_ENABLE_MAX_SPEED  1835

#define TRENCH_DEFAULT_CROSSHAIR_X        58
#define TRENCH_DEFAULT_CROSSHAIR_Y        24
#define TRENCH_CROSSHAIR_LIMIT_LEFT       8
#define TRENCH_CROSSHAIR_LIMIT_TOP        0
#define TRENCH_CROSSHAIR_LIMIT_RIGHT      110
#define TRENCH_CROSSHAIR_LIMIT_BOTTOM     46

#define TRENCH_MAXIMUM_TIES               2
#define TRENCH_TIE_MAX_COUNT              2

#define TRENCH_SHIP_SHAKE_COOLDOWN        30

#define TRENCH_TIE_FRAMES                 7
#define TRENCH_TIE_FRAME_SPEED_SLOW       30
#define TRENCH_TIE_FRAME_SPEED_FAST       15
#define TRENCH_TIE_FRAME_SPEED_FASTEST    10

#define TRENCH_BLINK_FRAMES               6
#define TRENCH_SHOOT_COOLDOWN             20
#define TRENCH_EXPLOSION_COOLDOWN         30
#define TRENCH_EXPLOSION_BLINK_FRAMES     60
#define TRENCH_LAST_SHOT_FRAMES           90

#define TRENCH_INTRO_TEXT_FRAME_LIMIT     180
#define TRENCH_INTRO_CRAWL_FRAME_UPDATE   3
#define TRENCH_INTRO_CRAWL_FRAME_LIMIT    (-180)
#define TRENCH_WARMUP_FRAME_COUNT         255
#define TRENCH_CRAWL_START                64

enum TrenchRunState : uint8_t {
    TR_STATE_INTRO_TEXT = 0,
    TR_STATE_INTRO_CRAWL,
    TR_STATE_MENU,
    TR_STATE_WARMUP,
    TR_STATE_GAME,
    TR_STATE_PAUSE,
    TR_STATE_DEATH,
    TR_STATE_WIN,
    TR_STATE_LOSE
};

class TrenchRunGame {
public:
    uint8_t state;
    bool exitRequested;   // el juego pide volver al menu de la consola
    bool gameOver;        // usado por isCurrentGameActive()

    void init();
    void update();
    void draw();

    bool isPlaying() const {
        return (state == TR_STATE_WARMUP || state == TR_STATE_GAME ||
                state == TR_STATE_DEATH);
    }

private:
    void checkInput();

    void stateIntroText();
    void stateIntroCrawl();
    void stateMenu();
    void stateWarmup();
    void stateGame();
    void statePause();
    void stateDeath();
    void stateWin();
    void stateLose();

    void drawTrench();
    void drawCockpit();
    void drawCrosshair();
    void drawTies();
    void drawBlink();
    void drawHull();

    void resetLevel();
    void resetGame();
    void calculateDisplacement();
    void decreaseDistance();
    void checkForTieRespawn();
    void advanceTies();
    void checkDeath();
    void checkFinalShot();
    void checkShoot();
    void checkTieBehavior();
    int calculateScore(int starting);

    uint8_t logoFrameCount;
    int16_t crawlFrameCount;

    uint8_t shipShakeCounter;
    uint8_t defeatedTies;
    uint8_t lastShotCooldown;
    uint8_t tieFrameSpeed;

    bool shooting;

    uint8_t blinkCounter;
    uint8_t explosionBlinkCounter;
    uint8_t shootCooldownCounter;

    int8_t xDisplacement;
    int8_t yDisplacement;

    uint8_t crosshairX;
    uint8_t crosshairY;
    uint16_t currentDistanceFromExaustPort;

    uint8_t lives;
    uint8_t hull;
    uint8_t trenchBackgroundFrame;

    bool tieEnabled[3];
    bool tieAlive[3];
    uint8_t tieRespawnTime[3];
    uint8_t tieX[3];
    uint8_t tieY[3];
    uint8_t tieFrame[3];
    uint8_t tieExplosionCooldown[3];
};
