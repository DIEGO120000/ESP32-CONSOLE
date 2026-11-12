#include "games/trench_run.h"
#include "games/trench_run_assets.h"

// =============================================================================
// TRENCH RUN - restauracion del original de Lucas Cardinali
// Logica de juego, vuelo por la trinchera, cazas TIE y ducto termico
// portados desde lscardinali/TrenchRun-Arduboy (licencia MIT).
// =============================================================================

static char trenchTextBuffer[32];

// -----------------------------------------------------------------------------
// Dibujo de la trinchera
// -----------------------------------------------------------------------------
void TrenchRunGame::drawTrench() {
    if (currentDistanceFromExaustPort == 0) {
        Sprites::drawOverwrite(-16 - xDisplacement / 3,
                               -14 - yDisplacement / 3,
                               TrenchBackgroundSprites,
                               3);
    } else {
        Sprites::drawOverwrite(-16 - xDisplacement / 3,
                               -14 - yDisplacement / 3,
                               TrenchBackgroundSprites,
                               trenchBackgroundFrame);
    }
}

void TrenchRunGame::drawCockpit() {
    int8_t extraDisplacement = 0;

    if (shipShakeCounter > 0) {
        extraDisplacement = (shipShakeCounter % 4 == 0) ? 2 : -2;
    }

    Sprites::drawPlusMask((-16 + xDisplacement / 2) + extraDisplacement,
                          (-14 + yDisplacement / 2) + extraDisplacement * -1,
                          cockpit, 0);

    arduboy.setCursor((50 + xDisplacement / 2) + extraDisplacement,
                      (50 + yDisplacement / 2) + extraDisplacement * -1);
    arduboy.print("0");
    if (currentDistanceFromExaustPort < 1000) arduboy.print("0");
    if (currentDistanceFromExaustPort < 100) arduboy.print("0");
    if (currentDistanceFromExaustPort < 10) arduboy.print("0");

    arduboy.print(currentDistanceFromExaustPort);
}

void TrenchRunGame::drawCrosshair() {
    Sprites::drawPlusMask(crosshairX, crosshairY, crosshair, 0);
}

void TrenchRunGame::drawTies() {
    for (uint8_t i = 0; i < TRENCH_TIE_MAX_COUNT; i++) {
        if (!tieAlive[i] && tieExplosionCooldown[i] > 0) {
            Sprites::drawPlusMask(tieX[i] - xDisplacement / 4,
                                  tieY[i] - yDisplacement / 4,
                                  tie, 8);
        } else if (tieAlive[i]) {
            Sprites::drawPlusMask(tieX[i] - xDisplacement / 4,
                                  tieY[i] - yDisplacement / 4,
                                  tie, tieFrame[i]);
        }
    }
}

void TrenchRunGame::drawBlink() {
    if (blinkCounter > 0 && (blinkCounter % 2)) {
        arduboy.fillRect(0, 0, 128, 64);
    }
}

void TrenchRunGame::drawHull() {
    arduboy.setCursor(36, 0);
    arduboy.print("hull:");
    arduboy.print((hull * 100) / 3);
    arduboy.print("%");
}

// -----------------------------------------------------------------------------
// Logica de la trinchera
// -----------------------------------------------------------------------------
void TrenchRunGame::decreaseDistance() {
    if (arduboy.everyXFrames(1) && currentDistanceFromExaustPort > 0) {
        currentDistanceFromExaustPort = currentDistanceFromExaustPort - 1;
    }
}

void TrenchRunGame::calculateDisplacement() {
    xDisplacement = crosshairX - TRENCH_DEFAULT_CROSSHAIR_X;
    yDisplacement = crosshairY - TRENCH_DEFAULT_CROSSHAIR_Y;
}

void TrenchRunGame::checkForTieRespawn() {
    if (arduboy.everyXFrames(60) && currentDistanceFromExaustPort > 0) {
        for (uint8_t i = 0; i < TRENCH_TIE_MAX_COUNT; i++) {
            if (!tieEnabled[i]) break;

            if (!tieAlive[i]) {
                if (tieRespawnTime[i] == 0) {
                    tieRespawnTime[i] = random(1, 4);
                    tieAlive[i] = true;
                    tieX[i] = random(0, 100);
                    tieY[i] = random(0, 36);
                    tieFrame[i] = 0;
                } else {
                    tieRespawnTime[i] -= 1;
                }
            }
        }
    }
}

void TrenchRunGame::advanceTies() {
    if (arduboy.everyXFrames(tieFrameSpeed)) {
        for (uint8_t i = 0; i < TRENCH_TIE_MAX_COUNT; i++) {
            if (tieAlive[i]) {
                if (tieFrame[i] == TRENCH_TIE_FRAMES) {
                    sound.tone(323, 5);
                    tieAlive[i] = false;
                    shipShakeCounter = TRENCH_SHIP_SHAKE_COOLDOWN;
                    hull -= 1;
                } else {
                    tieFrame[i] += 1;
                }
            }
        }
    }
}

void TrenchRunGame::checkDeath() {
    if (hull == 0) {
        lives--;
        explosionBlinkCounter = TRENCH_EXPLOSION_BLINK_FRAMES;
        state = TR_STATE_DEATH;
    }
}

void TrenchRunGame::checkFinalShot() {
    if (currentDistanceFromExaustPort == 0) {
        if (lastShotCooldown == 0) {
            hull = 0;
        }
        lastShotCooldown--;
    }
}

void TrenchRunGame::checkShoot() {
    if (shootCooldownCounter > 0) {
        shootCooldownCounter--;
    }

    if (shooting) {
        sound.tone(587, 5);

        uint8_t crosshairXCenter = crosshairX + 5;
        uint8_t crosshairYCenter = crosshairY + 5;

        if (currentDistanceFromExaustPort == 0) {
            if (crosshairXCenter >= 58 &&
                crosshairXCenter <= 70 &&
                crosshairYCenter >= 26 &&
                crosshairYCenter <= 38) {
                state = TR_STATE_WIN;
            }
        }

        for (uint8_t i = 0; i < TRENCH_TIE_MAX_COUNT; i++) {
            if (tieAlive[i]) {
                uint8_t tieXCollisionStart = tieX[i] - (xDisplacement / 4) + 2;
                uint8_t tieYCollisionStart = tieY[i] - (yDisplacement / 4) + 2;
                uint8_t tieXCollisionEnd = (tieX[i] - (xDisplacement / 4)) + 26;
                uint8_t tieYCollisionEnd = (tieY[i] - (yDisplacement / 4)) + 30;

                if (crosshairXCenter >= tieXCollisionStart &&
                    crosshairXCenter <= tieXCollisionEnd &&
                    crosshairYCenter >= tieYCollisionStart &&
                    crosshairYCenter <= tieYCollisionEnd) {
                    tieAlive[i] = false;
                    defeatedTies++;
                    tieExplosionCooldown[i] = TRENCH_EXPLOSION_COOLDOWN;
                }
            }
        }

        blinkCounter = TRENCH_BLINK_FRAMES;
        shooting = false;
    }
}

void TrenchRunGame::checkTieBehavior() {
    if (currentDistanceFromExaustPort == TRENCH_DISTANCE_ENABLE_TIE2 &&
        !tieEnabled[1]) {
        tieEnabled[1] = true;
    } else if (currentDistanceFromExaustPort == TRENCH_DISTANCE_ENABLE_SPEED) {
        tieFrameSpeed = TRENCH_TIE_FRAME_SPEED_FAST;
    } else if (currentDistanceFromExaustPort == TRENCH_DISTANCE_ENABLE_MAX_SPEED) {
        tieFrameSpeed = TRENCH_TIE_FRAME_SPEED_FASTEST;
    }
}

void TrenchRunGame::resetLevel() {
    tieAlive[0] = false;
    tieAlive[1] = false;
    tieRespawnTime[0] = random(2, 4);
    tieRespawnTime[1] = random(2, 4);
    tieEnabled[0] = true;
    tieEnabled[1] = false;
    tieEnabled[2] = false;
    tieFrameSpeed = TRENCH_TIE_FRAME_SPEED_SLOW;
    hull = 3;
    shipShakeCounter = 0;
    crosshairX = TRENCH_DEFAULT_CROSSHAIR_X;
    crosshairY = TRENCH_DEFAULT_CROSSHAIR_Y;
    currentDistanceFromExaustPort = TRENCH_DISTANCE_FROM_EXAUST_PORT;
    defeatedTies = 0;
    lastShotCooldown = TRENCH_LAST_SHOT_FRAMES;
    blinkCounter = 0;
    shooting = false;
    shootCooldownCounter = 0;
    explosionBlinkCounter = 0;
    xDisplacement = 0;
    yDisplacement = 0;
    trenchBackgroundFrame = 0;

    for (uint8_t i = 0; i < 3; i++) {
        tieFrame[i] = 0;
        tieExplosionCooldown[i] = 0;
        tieX[i] = 0;
        tieY[i] = 0;
    }
}

void TrenchRunGame::resetGame() {
    lives = 3;
    logoFrameCount = 0;
    crawlFrameCount = TRENCH_CRAWL_START;
}

// -----------------------------------------------------------------------------
// Estados del juego original
// -----------------------------------------------------------------------------
void TrenchRunGame::stateIntroText() {
    arduboy.setCursor(0, 24);
    for (uint8_t i = 0; i < TEXT_INTRO_LINES; i++) {
        strcpy(trenchTextBuffer, textIntro[i]);
        arduboy.println(trenchTextBuffer);
    }

}

void TrenchRunGame::stateIntroCrawl() {
    Sprites::drawOverwrite(24, crawlFrameCount, StarWarsLogo, 0);
    arduboy.setCursor(24, crawlFrameCount + 50);

    for (uint8_t i = 0; i < CRAWL_LINES; i++) {
        strcpy(trenchTextBuffer, crawlText[i]);
        arduboy.println(trenchTextBuffer);
    }
}

void TrenchRunGame::stateMenu() {
    Sprites::drawOverwrite(24, 0, StarWarsLogo, 0);
    arduboy.setCursor(34, 42);
    strcpy(trenchTextBuffer, menuText[0]);
    arduboy.print(trenchTextBuffer);
    arduboy.setCursor(20, 56);
    strcpy(trenchTextBuffer, menuText[1]);
    arduboy.print(trenchTextBuffer);
}

void TrenchRunGame::stateWarmup() {
    arduboy.setCursor(8, 10);
    arduboy.setTextSize(2);
    arduboy.print("GET READY");
    arduboy.setTextSize(1);
    arduboy.setCursor(10, 40);
    arduboy.print("Remaining Lifes:");
    arduboy.print(lives);
}

void TrenchRunGame::stateGame() {
    drawTrench();
    drawTies();
    drawBlink();
    drawCockpit();
    drawCrosshair();
    drawHull();
}

void TrenchRunGame::statePause() {
    arduboy.setCursor(44, 32);
    arduboy.println("-PAUSE-");
}

void TrenchRunGame::stateDeath() {
    if (explosionBlinkCounter % 8) {
        arduboy.fillRect(0, 0, 128, 64);
    }
}

int TrenchRunGame::calculateScore(int starting) {
    int score = starting;
    score += lives * 500;
    score += hull * 100;
    score += defeatedTies * 10;
    return score;
}

void TrenchRunGame::stateWin() {
    arduboy.setCursor(0, 0);
    arduboy.println("Congratulations!");
    arduboy.println("");
    arduboy.println("You've dealt a");
    arduboy.println("crippling blow to the ");
    arduboy.println("empire!");
    arduboy.println("");
    arduboy.print("SCORE: ");
    arduboy.print(calculateScore(1000));

    arduboy.setCursor(18, 56);
    arduboy.println("-Press a Button-");
}

void TrenchRunGame::stateLose() {
    arduboy.setCursor(0, 0);
    arduboy.println("Hope is lost!");
    arduboy.println("");
    arduboy.println("With you dies the");
    arduboy.println("last hope for a ");
    arduboy.println("liberated galaxy...");
    arduboy.println("");
    arduboy.print("SCORE: ");
    arduboy.print(calculateScore(0));
    arduboy.setCursor(18, 56);
    arduboy.println("-Press a Button-");
}

// -----------------------------------------------------------------------------
// Entrada
// -----------------------------------------------------------------------------
void TrenchRunGame::checkInput() {
    switch (state) {
        case TR_STATE_INTRO_TEXT:
            if (arduboy.justPressed(A_BUTTON)) {
                state = TR_STATE_INTRO_CRAWL;
            } else if (arduboy.justPressed(B_BUTTON)) {
                exitRequested = true;
            }
            break;

        case TR_STATE_INTRO_CRAWL:
            if (arduboy.justPressed(A_BUTTON)) {
                state = TR_STATE_MENU;
            } else if (arduboy.justPressed(B_BUTTON)) {
                exitRequested = true;
            }
            break;

        case TR_STATE_MENU:
            if (arduboy.justPressed(A_BUTTON)) {
                state = TR_STATE_WARMUP;
                sound.tone(587, 5);
            } else if (arduboy.justPressed(B_BUTTON)) {
                exitRequested = true;
            }
            break;

        case TR_STATE_GAME:
            if (arduboy.pressed(LEFT_BUTTON) &&
                crosshairX - 2 >= TRENCH_CROSSHAIR_LIMIT_LEFT) {
                crosshairX -= 2;
            }

            if (arduboy.pressed(RIGHT_BUTTON) &&
                crosshairX + 2 <= TRENCH_CROSSHAIR_LIMIT_RIGHT) {
                crosshairX += 2;
            }

            if (arduboy.pressed(UP_BUTTON) &&
                crosshairY - 2 >= TRENCH_CROSSHAIR_LIMIT_TOP) {
                crosshairY -= 2;
            }

            if (arduboy.pressed(DOWN_BUTTON) &&
                crosshairY + 2 <= TRENCH_CROSSHAIR_LIMIT_BOTTOM) {
                crosshairY += 2;
            }

            if (arduboy.justPressed(A_BUTTON)) {
                if (shootCooldownCounter == 0) {
                    shooting = true;
                    shootCooldownCounter = TRENCH_SHOOT_COOLDOWN;
                }
            }
            break;

        case TR_STATE_WIN:
        case TR_STATE_LOSE:
            if (arduboy.justPressed(A_BUTTON | B_BUTTON)) {
                resetGame();
                resetLevel();
                state = TR_STATE_INTRO_TEXT;
            }
            break;

        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// Interfaz publica
// -----------------------------------------------------------------------------
void TrenchRunGame::init() {
    exitRequested = false;
    gameOver = false;
    resetGame();
    resetLevel();
    state = TR_STATE_INTRO_TEXT;
}

void TrenchRunGame::update() {
    checkInput();

    switch (state) {
        case TR_STATE_INTRO_TEXT:
            if (arduboy.everyXFrames(TRENCH_INTRO_TEXT_FRAME_LIMIT)) {
                state = TR_STATE_INTRO_CRAWL;
            }
            break;

        case TR_STATE_INTRO_CRAWL:
            if (crawlFrameCount == TRENCH_INTRO_CRAWL_FRAME_LIMIT) {
                state = TR_STATE_MENU;
            } else if (arduboy.everyXFrames(TRENCH_INTRO_CRAWL_FRAME_UPDATE)) {
                crawlFrameCount--;
            }
            break;

        case TR_STATE_WARMUP:
            if (arduboy.everyXFrames(TRENCH_WARMUP_FRAME_COUNT)) {
                resetLevel();
                state = TR_STATE_GAME;
            }
            break;

        case TR_STATE_GAME:
            checkTieBehavior();
            checkShoot();
            checkFinalShot();
            checkDeath();
            calculateDisplacement();
            advanceTies();
            checkForTieRespawn();
            decreaseDistance();

            if (arduboy.everyXFrames(5)) {
                trenchBackgroundFrame = (trenchBackgroundFrame == 2)
                                            ? 0
                                            : trenchBackgroundFrame + 1;
            }
            if (shipShakeCounter > 0) shipShakeCounter--;
            if (blinkCounter > 0) blinkCounter--;
            for (uint8_t i = 0; i < TRENCH_TIE_MAX_COUNT; i++) {
                if (!tieAlive[i] && tieExplosionCooldown[i] > 0) {
                    tieExplosionCooldown[i]--;
                }
            }
            break;

        case TR_STATE_DEATH:
            if (explosionBlinkCounter == 0) {
                state = (lives == 0) ? TR_STATE_LOSE : TR_STATE_WARMUP;
            } else {
                explosionBlinkCounter--;
            }
            break;

        default:
            break;
    }
}

void TrenchRunGame::draw() {
    switch (state) {
        case TR_STATE_INTRO_TEXT:  stateIntroText(); break;
        case TR_STATE_INTRO_CRAWL: stateIntroCrawl(); break;
        case TR_STATE_MENU:        stateMenu(); break;
        case TR_STATE_WARMUP:      stateWarmup(); break;
        case TR_STATE_GAME:        stateGame(); break;
        case TR_STATE_PAUSE:       statePause(); break;
        case TR_STATE_DEATH:       stateDeath(); break;
        case TR_STATE_WIN:         stateWin(); break;
        case TR_STATE_LOSE:        stateLose(); break;
        default: break;
    }
}
