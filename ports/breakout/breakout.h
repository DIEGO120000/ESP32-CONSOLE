#pragma once
#include "console_core.h"

// =============================================================================
// GAME 2: ARKANOID (15 Original Rounds, 7 Power-Ups, Lasers, Multi-Ball & Warp)
// =============================================================================
extern const uint8_t arkanoid_levels[15][8][11];

struct BreakoutGame {
    // Mapa unico y fijo tipo <-> letra. Cada poder real tiene UNA sola letra y
    // esa letra nunca otorga otro poder durante la partida:
    //   E = Expand (pala ancha)   L = Laser       M = Multi-ball (disrupt)
    //   C = Catch  (pegajosa)     S = Slow        B = Break (portal warp)
    //   P = Extra life (o puntos) 
    enum PowerUpType {
        PWR_NONE = 0,
        PWR_EXPAND,    // [E]
        PWR_LASER,     // [L]
        PWR_DISRUPT,   // [M]
        PWR_CATCH,     // [C]
        PWR_SLOW,      // [S]
        PWR_BREAK,     // [B]
        PWR_EXTRA_LIFE,// [P]
        PWR_COUNT      // Centinela: PWR_NONE + 7 poderes reales
    };

    struct Ball {
        float x, y;
        float vx, vy;
        bool active;
        bool stuck;
        float stuckOffset;
        uint32_t stuckTimer;
        uint8_t bounceCount;
    } balls[3];

    struct Capsule {
        float x, y;
        uint8_t type;
        bool active;
    } capsules[3];

    struct Laser {
        float x, y;
        bool active;
    } lasers[4];

    float paddleX;
    float paddleWidth;
    uint8_t activePowerUp;
    bool warpGateOpen;
    uint8_t laserCooldown;

    uint8_t currentLevel;
    uint8_t bricks[8][11];
    uint8_t silverHits[8][11];

    uint16_t score;
    uint16_t highScore;
    uint8_t lives;
    float ballSpeed;
    uint8_t hitsSinceSpeedUp;
    uint32_t lastLaserAutoFire;

    bool gameOver;
    bool victory;
    bool levelTransition;
    uint32_t levelTransitionTimer;
    bool newHighScoreNotified;

    void checkHighScore() {
        if (score > highScore) {
            if (!newHighScoreNotified) {
                newHighScoreNotified = true;
                triggerRgbLed(0, 255, 0, 1000); // GREEN on beating record
            }
            highScore = score;
            EEPROM.put(12, highScore);
        }
    }

    void init() {
        randomSeed(micros());
        score = 0;
        lives = 3;
        gameOver = false;
        victory = false;
        levelTransition = false;
        newHighScoreNotified = false;

        highScore = 0;
        EEPROM.get(12, highScore);
        if (highScore == 0xFFFF) highScore = 0;

        uint8_t startLvl = random(0, 15);
        loadLevel(startLvl);
    }

    void loadLevel(uint8_t lvl) {
        currentLevel = lvl;
        paddleWidth = 18.0f;
        paddleX = (128.0f - paddleWidth) / 2.0f;
        activePowerUp = PWR_NONE;
        warpGateOpen = false;
        laserCooldown = 0;
        lastLaserAutoFire = millis();
        ballSpeed = 1.4f;
        hitsSinceSpeedUp = 0;

        // Reset Lasers and Capsules
        for (uint8_t i = 0; i < 4; i++) lasers[i].active = false;
        for (uint8_t i = 0; i < 3; i++) capsules[i].active = false;

        // Load Matrix from PROGMEM
        for (uint8_t r = 0; r < 8; r++) {
            for (uint8_t c = 0; c < 11; c++) {
                bricks[r][c] = pgm_read_byte(&arkanoid_levels[lvl][r][c]);
                silverHits[r][c] = 0;
            }
        }

        // Spawn Main Ball (Ball 0 attached to paddle)
        balls[0].active = true;
        balls[0].stuck = true;
        balls[0].stuckOffset = paddleWidth / 2.0f - 1.5f;
        balls[0].stuckTimer = millis();
        balls[0].bounceCount = 0;
        balls[0].x = paddleX + balls[0].stuckOffset;
        balls[0].y = 54.0f;
        balls[0].vx = 1.0f;
        balls[0].vy = -1.3f;

        balls[1].active = false;
        balls[1].bounceCount = 0;
        balls[2].active = false;
        balls[2].bounceCount = 0;

        if (countDestructibleBricks() == 0) {
            advanceLevel();
            return;
        }

        levelTransition = true;
        levelTransitionTimer = millis();
        sound.tone(900, 40); delay(50);
        sound.tone(1400, 80);
    }

    uint8_t countDestructibleBricks() {
        uint8_t count = 0;
        for (uint8_t r = 0; r < 8; r++) {
            for (uint8_t c = 0; c < 11; c++) {
                if (bricks[r][c] >= 1 && bricks[r][c] <= 4) count++;
            }
        }
        return count;
    }

    void maybeSpawnCapsule(int16_t bx, int16_t by) {
        if (random(0, 100) < 18) {
            for (uint8_t i = 0; i < 3; i++) {
                if (!capsules[i].active) {
                    capsules[i].active = true;
                    capsules[i].x = bx + 1.0f;
                    capsules[i].y = by;
                    // Tabla unica de poderes REALES: sin PWR_NONE ni huecos.
                    // sizeof(types) == PWR_COUNT - 1 y random(0, N) devuelve
                    // [0, N), asi que el 100% de los indices caen dentro de la
                    // tabla y cada valor es un poder que aplica efecto.
                    static const uint8_t types[PWR_COUNT - 1] = {
                        PWR_EXPAND, PWR_LASER, PWR_DISRUPT, PWR_CATCH,
                        PWR_SLOW, PWR_BREAK, PWR_EXTRA_LIFE
                    };
                    capsules[i].type = types[random(0, (long)(sizeof(types)))];
                    break;
                }
            }
        }
    }

    void applyPowerUp(uint8_t type) {
        // Un tipo vacio (PWR_NONE) o fuera de rango no puede borrar el poder
        // activo ni consumirse como capsula fantasma: se ignora por completo.
        if (type <= PWR_NONE || type >= PWR_COUNT) return;

        // Extra life es acumulativa: no reemplaza el modificador activo y, si
        // la partida ya esta al tope de vidas, nunca queda como capsula vacia.
        if (type == PWR_EXTRA_LIFE) {
            if (lives < 5) lives++;
            else score += 500;
            sound.tone(1200, 40); delay(40); sound.tone(1800, 90);
            return;
        }

        // Poderes exclusivos: uno reemplaza al anterior.
        activePowerUp = type;

        // La pala vuelve a su ancho base salvo EXPAND; el portal se cierra
        // salvo BREAK.
        paddleWidth = 18.0f;
        warpGateOpen = false;

        switch (type) {
            case PWR_EXPAND:
                paddleWidth = 28.0f;
                if (paddleX + paddleWidth > 126.0f) paddleX = 126.0f - paddleWidth;
                sound.tone(1100, 50);
                break;

            case PWR_LASER:
                lastLaserAutoFire = millis();
                sound.tone(1500, 50);
                break;

            case PWR_DISRUPT: {
                // Multi-ball: replica la primera bola activa (o la pala si no
                // hubiera ninguna) para que el poder nunca quede sin efecto.
                int8_t mainBall = -1;
                for (uint8_t b = 0; b < 3; b++) {
                    if (balls[b].active) { mainBall = b; break; }
                }
                float bx = (mainBall >= 0) ? balls[mainBall].x : (paddleX + paddleWidth / 2.0f);
                float by = (mainBall >= 0) ? balls[mainBall].y : 54.0f;
                balls[0].active = true; balls[0].x = bx; balls[0].y = by; balls[0].vx = -1.1f; balls[0].vy = -1.3f; balls[0].stuck = false; balls[0].bounceCount = 0;
                balls[1].active = true; balls[1].x = bx; balls[1].y = by; balls[1].vx = 0.4f;  balls[1].vy = -1.6f; balls[1].stuck = false; balls[1].bounceCount = 0;
                balls[2].active = true; balls[2].x = bx; balls[2].y = by; balls[2].vx = 1.3f;  balls[2].vy = -1.1f; balls[2].stuck = false; balls[2].bounceCount = 0;
                sound.tone(1300, 40); delay(40); sound.tone(1600, 60);
                break;
            }

            case PWR_CATCH:
                sound.tone(950, 50);
                break;

            case PWR_SLOW:
                // Siempre frena de verdad: con ballSpeed ya en 1.4 (su base) el
                // reset anterior no hacia nada y borraba el poder anterior.
                ballSpeed = min(1.4f, ballSpeed * 0.75f);
                if (ballSpeed < 0.9f) ballSpeed = 0.9f;
                for (uint8_t b = 0; b < 3; b++) {
                    if (balls[b].active && !balls[b].stuck) {
                        float currentMag = sqrt(balls[b].vx * balls[b].vx + balls[b].vy * balls[b].vy);
                        if (currentMag > 0.1f) {
                            balls[b].vx = (balls[b].vx / currentMag) * ballSpeed;
                            balls[b].vy = (balls[b].vy / currentMag) * ballSpeed;
                        }
                    }
                }
                sound.tone(600, 80);
                break;

            case PWR_BREAK:
                warpGateOpen = true;
                sound.tone(1400, 50); delay(40); sound.tone(1800, 70);
                break;

            default:
                // Inalcanzable: el rango ya se valido arriba.
                activePowerUp = PWR_NONE;
                break;
        }
    }

    void advanceLevel() {
        uint8_t nextLvl = random(0, 15);
        if (nextLvl == currentLevel && 15 > 1) {
            nextLvl = (nextLvl + 1 + random(0, 13)) % 15;
        }
        loadLevel(nextLvl);
    }

    void fireLasers() {
        if (laserCooldown > 0) return;
        uint8_t spawned = 0;
        for (uint8_t i = 0; i < 4; i++) {
            if (!lasers[i].active) {
                lasers[i].active = true;
                lasers[i].x = (spawned == 0) ? (paddleX + 1.0f) : (paddleX + paddleWidth - 2.0f);
                lasers[i].y = 54.0f;
                spawned++;
                if (spawned >= 2) break;
            }
        }
        if (spawned > 0) {
            laserCooldown = 12; // ~0.2s cooldown
            sound.tone(1500, 15);
        }
    }

    void launchStuckBall(Ball& ball) {
        ball.stuck = false;
        ball.bounceCount = 0;
        float center = paddleX + (paddleWidth / 2.0f);
        float offset = (ball.x + 1.5f - center) / (paddleWidth / 2.0f);
        if (offset < -0.9f) offset = -0.9f;
        if (offset > 0.9f) offset = 0.9f;
        ball.vx = offset * (ballSpeed * 0.85f);
        ball.vy = -sqrt(max(0.4f, (ballSpeed * ballSpeed) - (ball.vx * ball.vx)));
        sound.tone(800, 20);
    }

    void update() {
        yield();
        if (gameOver || victory) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) init();
            return;
        }

        if (levelTransition) {
            if (millis() - levelTransitionTimer > 1500 || arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) {
                levelTransition = false;
            }
            return;
        }

        if (countDestructibleBricks() == 0) {
            advanceLevel();
            return;
        }

        if (laserCooldown > 0) laserCooldown--;

        // --- Controls: Paddle Movement (LEFT: GPIO 13, RIGHT: GPIO 19) ---
        if (arduboy.pressed(LEFT_BUTTON) && paddleX > 2.0f) paddleX -= 2.8f;
        if (arduboy.pressed(RIGHT_BUTTON) && paddleX + paddleWidth < 126.0f) paddleX += 2.8f;

        // --- Automatic Laser Auto-Fire (Periodic ~380ms) ---
        if (activePowerUp == PWR_LASER) {
            uint32_t now = millis();
            if (now - lastLaserAutoFire >= 380) {
                fireLasers();
                lastLaserAutoFire = now;
            }
        }

        // --- Action Button (A_BUTTON: GPIO 20) (Manual Launch for Catch & Serve) ---
        bool actionPressed = arduboy.justPressed(A_BUTTON);
        if (actionPressed) {
            for (uint8_t b = 0; b < 3; b++) {
                if (balls[b].active && balls[b].stuck) {
                    launchStuckBall(balls[b]);
                }
            }
        }

        // --- Warp Gate Trigger ---
        if (warpGateOpen && (paddleX + paddleWidth >= 125.0f)) {
            score += 1000;
            advanceLevel();
            return;
        }

        // --- Update Lasers ---
        for (uint8_t i = 0; i < 4; i++) {
            if (lasers[i].active) {
                lasers[i].y -= 3.5f;
                if (lasers[i].y < 10.0f) {
                    lasers[i].active = false;
                    continue;
                }

                // Laser vs Bricks
                if (lasers[i].y <= 51.0f) {
                    for (int8_t r = 7; r >= 0; r--) {
                        for (int8_t c = 0; c < 11; c++) {
                            if (bricks[r][c] > 0) {
                                int16_t bx = 4 + c * 11;
                                int16_t by = 12 + r * 5;
                                if (lasers[i].x >= bx && lasers[i].x <= bx + 10 && lasers[i].y >= by && lasers[i].y <= by + 4) {
                                    lasers[i].active = false;
                                    if (bricks[r][c] == 5) {
                                        sound.tone(1100, 10); // Indestructible
                                    } else if (bricks[r][c] == 4) {
                                        if (silverHits[r][c] == 0) {
                                            silverHits[r][c] = 1;
                                            sound.tone(800, 15);
                                        } else {
                                            bricks[r][c] = 0;
                                            score += 100;
                                            sound.tone(1200, 25);
                                            maybeSpawnCapsule(bx, by);
                                        }
                                    } else {
                                        uint16_t pts = (bricks[r][c] == 1) ? 50 : (bricks[r][c] == 2 ? 80 : 100);
                                        score += pts;
                                        bricks[r][c] = 0;
                                        sound.tone(900 + r * 60, 20);
                                        maybeSpawnCapsule(bx, by);
                                    }

                                    if (countDestructibleBricks() == 0) {
                                        advanceLevel();
                                        return;
                                    }
                                    goto laser_done;
                                }
                            }
                        }
                    }
                }
            laser_done: ;
            }
        }

        // --- Update Falling Capsules ---
        for (uint8_t i = 0; i < 3; i++) {
            if (capsules[i].active) {
                capsules[i].y += 1.0f;
                // Check Collision with Paddle
                if (capsules[i].y >= 55.0f && capsules[i].y <= 60.0f &&
                    capsules[i].x + 7.0f >= paddleX && capsules[i].x <= paddleX + paddleWidth) {
                    applyPowerUp(capsules[i].type);
                    capsules[i].active = false;
                } else if (capsules[i].y > 64.0f) {
                    capsules[i].active = false;
                }
            }
        }

        // --- Update Active Balls ---
        uint8_t activeBallCount = 0;
        for (uint8_t b = 0; b < 3; b++) {
            if (!balls[b].active) continue;
            activeBallCount++;

            if (balls[b].stuck) {
                balls[b].x = paddleX + balls[b].stuckOffset;
                balls[b].y = 54.0f;
                balls[b].bounceCount = 0;
                if (millis() - balls[b].stuckTimer > 3500) {
                    launchStuckBall(balls[b]);
                }
                continue;
            }

            balls[b].x += balls[b].vx;
            balls[b].y += balls[b].vy;

            // Side Walls
            if (balls[b].x <= 1.0f) {
                balls[b].x = 1.0f;
                balls[b].vx = fabs(balls[b].vx);
                balls[b].bounceCount++;
                sound.tone(450, 10);
            } else if (balls[b].x >= 124.0f) {
                balls[b].x = 124.0f;
                balls[b].vx = -fabs(balls[b].vx);
                balls[b].bounceCount++;
                sound.tone(450, 10);
            }

            // Top Wall
            if (balls[b].y <= 10.0f) {
                balls[b].y = 10.0f;
                balls[b].vy = fabs(balls[b].vy);
                balls[b].bounceCount++;
                sound.tone(450, 10);
            }

            // Paddle Collision
            if (balls[b].y >= 54.0f && balls[b].y <= 59.0f &&
                balls[b].x + 3.0f >= paddleX && balls[b].x <= paddleX + paddleWidth && balls[b].vy > 0.0f) {
                balls[b].bounceCount = 0; // Reset bounce count on paddle contact
                if (activePowerUp == PWR_CATCH) {
                    balls[b].stuck = true;
                    balls[b].stuckOffset = balls[b].x - paddleX;
                    balls[b].stuckTimer = millis();
                    sound.tone(850, 15);
                } else {
                    float center = paddleX + (paddleWidth / 2.0f);
                    float offset = (balls[b].x + 1.5f - center) / (paddleWidth / 2.0f);
                    if (offset < -0.9f) offset = -0.9f;
                    if (offset > 0.9f) offset = 0.9f;
                    balls[b].vx = offset * (ballSpeed * 0.88f);
                    balls[b].vy = -sqrt(max(0.4f, (ballSpeed * ballSpeed) - (balls[b].vx * balls[b].vx)));
                    sound.tone(750, 15);
                    hitsSinceSpeedUp++;
                    if (hitsSinceSpeedUp >= 8) {
                        hitsSinceSpeedUp = 0;
                        ballSpeed = min(2.7f, ballSpeed + 0.1f);
                    }
                }
            }

            // Bricks Collision (Strict AABB)
            if (balls[b].y <= 51.0f) {
                for (int8_t r = 0; r < 8; r++) {
                    for (int8_t c = 0; c < 11; c++) {
                        if (bricks[r][c] > 0) {
                            int16_t bx = 4 + c * 11;
                            int16_t by = 12 + r * 5;
                            int16_t bw = 10;
                            int16_t bh = 4;

                            if (balls[b].x + 3.0f >= bx && balls[b].x <= bx + bw &&
                                balls[b].y + 3.0f >= by && balls[b].y <= by + bh) {
                                float overlapLeft = (balls[b].x + 3.0f) - bx;
                                float overlapRight = (bx + bw) - balls[b].x;
                                float overlapTop = (balls[b].y + 3.0f) - by;
                                float overlapBottom = (by + bh) - balls[b].y;

                                float minOverlapX = min(overlapLeft, overlapRight);
                                float minOverlapY = min(overlapTop, overlapBottom);

                                if (minOverlapX < minOverlapY) {
                                    balls[b].vx = -balls[b].vx;
                                } else {
                                    balls[b].vy = -balls[b].vy;
                                }
                                balls[b].bounceCount++;

                                if (bricks[r][c] == 5) {
                                    sound.tone(1100, 10);
                                } else if (bricks[r][c] == 4) {
                                    if (silverHits[r][c] == 0) {
                                        silverHits[r][c] = 1;
                                        sound.tone(800, 15);
                                    } else {
                                        bricks[r][c] = 0;
                                        score += 100;
                                        sound.tone(1200, 25);
                                        maybeSpawnCapsule(bx, by);
                                    }
                                } else {
                                    uint16_t pts = (bricks[r][c] == 1) ? 50 : (bricks[r][c] == 2 ? 80 : 100);
                                    score += pts;
                                    bricks[r][c] = 0;
                                    sound.tone(900 + r * 60, 20);
                                    maybeSpawnCapsule(bx, by);
                                }

                                if (countDestructibleBricks() == 0) {
                                    advanceLevel();
                                    return;
                                }
                                goto ball_collision_done;
                            }
                        }
                    }
                }
            }
        ball_collision_done:

            // Rutina anti-atrapamiento (Anti-Tunneling / Escape Vector tras 12 rebotes seguidos sin tocar pala)
            if (balls[b].bounceCount >= 12) {
                float perturb = ((float)random(-15, 16) / 20.0f); // -0.75 a +0.75
                balls[b].vx += perturb;
                if (fabs(balls[b].vx) < 0.4f) {
                    balls[b].vx = (balls[b].vx < 0.0f ? -0.6f : 0.6f);
                }
                // Si la bola está atrapada arriba o lleva demasiados rebotes en bucle, forzar escape descendente
                if (balls[b].y <= 32.0f || balls[b].bounceCount >= 16) {
                    balls[b].vy = fabs(balls[b].vy);
                    if (balls[b].vy < 0.8f) balls[b].vy = 0.8f;
                }
                float mag = sqrt(balls[b].vx * balls[b].vx + balls[b].vy * balls[b].vy);
                if (mag > 0.1f) {
                    balls[b].vx = (balls[b].vx / mag) * ballSpeed;
                    balls[b].vy = (balls[b].vy / mag) * ballSpeed;
                }
                balls[b].bounceCount = 0; // Resetear tras aplicar vector de escape
            }

            // Ball Out of Bounds
            if (balls[b].y > 64.0f) {
                balls[b].active = false;
            }
        }

        // --- Life Loss Check (When All Active Balls Fall) ---
        bool anyBallActive = false;
        for (uint8_t b = 0; b < 3; b++) {
            if (balls[b].active) { anyBallActive = true; break; }
        }

        if (!anyBallActive) {
            lives--;
            sound.tone(180, 150);
            if (lives == 0) {
                gameOver = true;
                if (score > highScore) { highScore = score; EEPROM.put(12, highScore); }
            } else {
                // Respawn single ball attached to paddle
                paddleWidth = 18.0f;
                paddleX = (128.0f - paddleWidth) / 2.0f;
                activePowerUp = PWR_NONE;
                warpGateOpen = false;
                ballSpeed = 1.4f;

                balls[0].active = true;
                balls[0].stuck = true;
                balls[0].stuckOffset = paddleWidth / 2.0f - 1.5f;
                balls[0].stuckTimer = millis();
                balls[0].bounceCount = 0;
                balls[0].x = paddleX + balls[0].stuckOffset;
                balls[0].y = 54.0f;
                balls[0].vx = 1.0f;
                balls[0].vy = -1.3f;
            }
        }
        checkHighScore();
    }

    void drawCapsule(float x, float y, uint8_t type) {
        int16_t ix = (int16_t)x;
        int16_t iy = (int16_t)y;

        arduboy.fillRect(ix, iy, 7, 5, WHITE);
        arduboy.drawPixel(ix, iy, BLACK);
        arduboy.drawPixel(ix + 6, iy, BLACK);
        arduboy.drawPixel(ix, iy + 4, BLACK);
        arduboy.drawPixel(ix + 6, iy + 4, BLACK);

        // Glifos 3x5 indexados DIRECTAMENTE por PowerUpType: la letra dibujada
        // sale del tipo real de la capsula, sin tablas paralelas que se puedan
        // desincronizar. Una letra = un poder, siempre.
        static const uint8_t PROGMEM miniLetters[PWR_COUNT][5] = {
            {0, 0, 0, 0, 0}, // PWR_NONE (sin capsula)
            {7, 4, 6, 4, 7}, // [E] Expand    - pala ancha
            {4, 4, 4, 4, 7}, // [L] Laser     - disparos
            {5, 7, 7, 5, 5}, // [M] Multi-ball - disrupt
            {7, 4, 4, 4, 7}, // [C] Catch     - pegajosa
            {7, 4, 7, 1, 7}, // [S] Slow      - lenta
            {6, 5, 6, 5, 6}, // [B] Break     - portal warp
            {6, 5, 6, 4, 4}  // [P] Extra life - o puntos
        };

        if (type > PWR_NONE && type < PWR_COUNT) {
            for (int8_t r = 0; r < 5; r++) {
                uint8_t rowBits = pgm_read_byte(&miniLetters[type][r]);
                for (int8_t c = 0; c < 3; c++) {
                    if (rowBits & (1 << (2 - c))) {
                        arduboy.drawPixel(ix + 2 + c, iy + r, BLACK);
                    }
                }
            }
        }
    }

    // Misma tabla 1 a 1 que drawCapsule(): E/L/M/C/S/B/P.
    char getPowerUpChar(uint8_t type) {
        switch (type) {
            case PWR_EXPAND: return 'E';
            case PWR_LASER: return 'L';
            case PWR_DISRUPT: return 'M';
            case PWR_CATCH: return 'C';
            case PWR_SLOW: return 'S';
            case PWR_BREAK: return 'B';
            case PWR_EXTRA_LIFE: return 'P';
            default: return ' ';
        }
    }

    void draw() {
        // --- Top HUD Bar ---
        arduboy.fillRect(0, 0, 128, 9, BLACK); // Clean HUD strip to prevent residual pixels
        arduboy.setCursor(2, 0); arduboy.print("P:"); arduboy.print(score);
        arduboy.setCursor(50, 0); arduboy.print("H:"); arduboy.print(highScore);
        arduboy.setCursor(102, 0); arduboy.print("L:"); arduboy.print(lives);
        arduboy.drawFastHLine(0, 9, 128, WHITE);

        // --- Warp Gate (Right Wall Opening) ---
        if (warpGateOpen) {
            // Flashing portal animation
            bool flash = (millis() / 150) % 2;
            if (flash) {
                arduboy.fillRect(125, 52, 3, 9, WHITE);
                arduboy.drawFastHLine(125, 56, 3, BLACK);
            } else {
                arduboy.drawRect(125, 52, 3, 9, WHITE);
            }
        }

        // --- Draw Bricks ---
        for (uint8_t r = 0; r < 8; r++) {
            for (uint8_t c = 0; c < 11; c++) {
                uint8_t bt = bricks[r][c];
                if (bt == 0) continue;
                int16_t bx = 4 + c * 11;
                int16_t by = 12 + r * 5;

                if (bt == 1) {
                    // Soft: Outline with center dot
                    arduboy.drawRect(bx, by, 10, 4, WHITE);
                    arduboy.drawPixel(bx + 4, by + 1, WHITE);
                    arduboy.drawPixel(bx + 5, by + 2, WHITE);
                } else if (bt == 2) {
                    // Medium: Outline with inner bar
                    arduboy.drawRect(bx, by, 10, 4, WHITE);
                    arduboy.drawFastHLine(bx + 2, by + 1, 6, WHITE);
                    arduboy.drawFastHLine(bx + 2, by + 2, 6, WHITE);
                } else if (bt == 3) {
                    // Hard: Solid filled
                    arduboy.fillRect(bx, by, 10, 4, WHITE);
                } else if (bt == 4) {
                    // Silver: Solid with diamond sheen or crack
                    arduboy.fillRect(bx, by, 10, 4, WHITE);
                    if (silverHits[r][c] == 0) {
                        arduboy.drawPixel(bx + 2, by + 1, BLACK);
                        arduboy.drawPixel(bx + 7, by + 2, BLACK);
                    } else {
                        // Cracked look
                        arduboy.drawFastVLine(bx + 4, by, 4, BLACK);
                        arduboy.drawPixel(bx + 5, by + 2, BLACK);
                    }
                } else if (bt == 5) {
                    // Gold / Indestructible: Metallic textured bars
                    arduboy.drawRect(bx, by, 10, 4, WHITE);
                    arduboy.drawFastVLine(bx + 3, by + 1, 2, WHITE);
                    arduboy.drawFastVLine(bx + 6, by + 1, 2, WHITE);
                }
            }
        }

        // --- Draw Falling Capsules ---
        for (uint8_t i = 0; i < 3; i++) {
            if (capsules[i].active) {
                drawCapsule(capsules[i].x, capsules[i].y, capsules[i].type);
            }
        }

        // --- Draw Lasers ---
        for (uint8_t i = 0; i < 4; i++) {
            if (lasers[i].active) {
                arduboy.fillRect((int16_t)lasers[i].x, (int16_t)lasers[i].y, 1, 3, WHITE);
            }
        }

        // --- Draw Paddle ---
        int16_t px = (int16_t)paddleX;
        uint8_t pw = (uint8_t)paddleWidth;

        if (activePowerUp == PWR_LASER) {
            // Twin laser turrets
            arduboy.fillRect(px, 56, 2, 2, WHITE);
            arduboy.fillRect(px + pw - 2, 56, 2, 2, WHITE);
        }

        // Main rounded paddle body
        arduboy.fillRect(px, 58, pw, 3, WHITE);
        arduboy.drawPixel(px, 58, BLACK);
        arduboy.drawPixel(px + pw - 1, 58, BLACK);

        // --- Draw Balls ---
        for (uint8_t b = 0; b < 3; b++) {
            if (balls[b].active) {
                arduboy.fillRect((int16_t)balls[b].x, (int16_t)balls[b].y, 3, 3, WHITE);
            }
        }

        // --- Intermission / Overlay Screens ---
        if (levelTransition) {
            arduboy.fillRect(28, 22, 72, 20, BLACK);
            arduboy.drawRect(28, 22, 72, 20, WHITE);
            arduboy.setCursor(38, 28);
            arduboy.print("ROUND ");
            arduboy.print(currentLevel + 1);
        } else if (gameOver) {
            arduboy.fillRect(24, 20, 80, 26, BLACK);
            arduboy.drawRect(24, 20, 80, 26, WHITE);
            arduboy.setCursor(34, 24); arduboy.print("GAME OVER");
            arduboy.setCursor(28, 33); arduboy.print("Press B3:Play");
        } else if (victory) {
            arduboy.fillRect(20, 18, 88, 30, BLACK);
            arduboy.drawRect(20, 18, 88, 30, WHITE);
            arduboy.setCursor(30, 23); arduboy.print("DOH DEFEATED!");
            arduboy.setCursor(36, 32); arduboy.print("VICTORY!!!");
        }
    }
};
