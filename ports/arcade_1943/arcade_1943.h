#pragma once
#include "console_core.h"

// --- 1943 Sprites ---
extern const uint8_t plane_1943_bmp[];
extern const uint8_t enemy_1943_bmp[];
extern const uint8_t bomber_1943_bmp[];

// =============================================================================
// GAME 4: 1943 (Continuous Scrolling, Structured Formations & Accurate Hitboxes)
// =============================================================================
struct AirCombatGame {
    int16_t planeX;
    int16_t bulletX[3], bulletY[3];
    bool bulletActive[3];

    struct Enemy {
        float x, y;
        float vx, vy;
        bool active;
        uint8_t type; // 0: fighter, 1: dive curve, 2: heavy bomber
        int8_t hp;
        float phase;
    } enemies[5];

    struct Star {
        int16_t x, y;
        uint8_t speed;
    } stars[16];

    uint16_t score;
    uint16_t highScore;
    bool gameOver;
    bool newHighScoreNotified;

    void checkHighScore() {
        if (score > highScore) {
            if (!newHighScoreNotified) {
                newHighScoreNotified = true;
                triggerRgbLed(0, 255, 0, 1000); // GREEN on beating record
            }
            highScore = score;
            EEPROM.put(16, highScore);
        }
    }

    void init() {
        planeX = 58;
        score = 0;
        gameOver = false;
        newHighScoreNotified = false;
        highScore = 0;
        EEPROM.get(16, highScore);
        if (highScore == 0xFFFF) highScore = 0;

        for (uint8_t i = 0; i < 3; i++) bulletActive[i] = false;

        for (uint8_t s = 0; s < 16; s++) {
            stars[s].x = random(0, 128);
            stars[s].y = random(0, 64);
            stars[s].speed = random(1, 4);
        }

        spawnWave();
    }

    void spawnWave() {
        uint8_t waveType = random(0, 3);
        for (uint8_t i = 0; i < 5; i++) {
            enemies[i].active = (i < 4);
            if (waveType == 0) {
                // V-Formation
                int16_t offsetX = (i % 2 == 0) ? -(i * 14) : (i * 14);
                enemies[i].x = 64 + offsetX;
                enemies[i].y = -10 - (i * 12);
                enemies[i].vx = 0.0f;
                enemies[i].vy = 1.6f;
                enemies[i].type = 0;
                enemies[i].hp = 1;
                enemies[i].phase = 0;
            } else if (waveType == 1) {
                // Parabolic Sine Wave
                enemies[i].x = 10 + (i * 28);
                enemies[i].y = -15 - (i * 16);
                enemies[i].vx = (i % 2 == 0) ? 1.4f : -1.4f;
                enemies[i].vy = 1.3f;
                enemies[i].type = 1;
                enemies[i].hp = 1;
                enemies[i].phase = i * 0.8f;
            } else {
                // Heavy Bomber + escorts
                if (i == 0) {
                    enemies[i].x = 56;
                    enemies[i].y = -20;
                    enemies[i].vx = 0.0f;
                    enemies[i].vy = 0.9f;
                    enemies[i].type = 2; // Bomber
                    enemies[i].hp = 4;
                    enemies[i].phase = 0;
                } else {
                    enemies[i].x = (i == 1) ? 20 : ((i == 2) ? 96 : 64);
                    enemies[i].y = -35 - (i * 10);
                    enemies[i].vx = (i == 1) ? 0.8f : ((i == 2) ? -0.8f : 0.0f);
                    enemies[i].vy = 1.4f;
                    enemies[i].type = 0;
                    enemies[i].hp = 1;
                    enemies[i].phase = 0;
                }
            }
        }
    }

    void update() {
        yield();
        if (gameOver) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) init();
            return;
        }

        // Starfield Scrolling
        for (uint8_t s = 0; s < 16; s++) {
            stars[s].y += stars[s].speed;
            if (stars[s].y >= 64) {
                stars[s].y = 0;
                stars[s].x = random(0, 128);
            }
        }

        // Controls (LEFT: GPIO 13, RIGHT: GPIO 19)
        if (arduboy.pressed(LEFT_BUTTON) && planeX > 2) planeX -= 2;
        if (arduboy.pressed(RIGHT_BUTTON) && planeX < 116) planeX += 2;

        static uint8_t shootTimer = 0;
        shootTimer++;
        if (arduboy.justPressed(A_BUTTON) || shootTimer > 16) {
            shootTimer = 0;
            for (uint8_t i = 0; i < 3; i++) {
                if (!bulletActive[i]) {
                    bulletActive[i] = true;
                    bulletX[i] = planeX + 5;
                    bulletY[i] = 50;
                    sound.tone(1100, 15);
                    break;
                }
            }
        }

        // Bullets
        for (uint8_t i = 0; i < 3; i++) {
            if (bulletActive[i]) {
                bulletY[i] -= 4;
                if (bulletY[i] < 0) bulletActive[i] = false;

                for (uint8_t e = 0; e < 5; e++) {
                    if (enemies[e].active && bulletActive[i]) {
                        int16_t ew = (enemies[e].type == 2) ? 15 : 9;
                        int16_t eh = (enemies[e].type == 2) ? 9 : 7;
                        if (bulletX[i] >= enemies[e].x && bulletX[i] <= enemies[e].x + ew &&
                            bulletY[i] >= enemies[e].y && bulletY[i] <= enemies[e].y + eh) {
                            bulletActive[i] = false;
                            enemies[e].hp--;
                            if (enemies[e].hp <= 0) {
                                enemies[e].active = false;
                                score += (enemies[e].type == 2) ? 150 : 50;
                                checkHighScore();
                                sound.tone(350, 40);
                            } else {
                                sound.tone(700, 15);
                            }
                        }
                    }
                }
            }
        }

        // Enemy Update
        bool anyActive = false;
        for (uint8_t e = 0; e < 5; e++) {
            if (enemies[e].active) {
                anyActive = true;
                enemies[e].y += enemies[e].vy;
                if (enemies[e].type == 1) {
                    enemies[e].phase += 0.08f;
                    enemies[e].x += sin(enemies[e].phase) * 2.2f;
                } else {
                    enemies[e].x += enemies[e].vx;
                    if (enemies[e].x <= 2 || enemies[e].x >= 118) enemies[e].vx = -enemies[e].vx;
                }

                if (enemies[e].y > 64) enemies[e].active = false;

                // Accurate Centered Player Collision
                if (planeX + 8 >= enemies[e].x && planeX + 2 <= enemies[e].x + 8 &&
                    enemies[e].y >= 48 && enemies[e].y <= 58) {
                    gameOver = true;
                    sound.tone(120, 150);
                }
            }
        }

        if (!anyActive) spawnWave();
    }

    void draw() {
        // Starfield
        for (uint8_t s = 0; s < 16; s++) {
            arduboy.drawPixel(stars[s].x, stars[s].y, WHITE);
        }

        // Player
        arduboy.drawBitmap(planeX, 52, plane_1943_bmp, 11, 9, WHITE);

        // Bullets
        for (uint8_t i = 0; i < 3; i++) {
            if (bulletActive[i]) arduboy.drawFastVLine(bulletX[i], bulletY[i], 3, WHITE);
        }

        // Enemies
        for (uint8_t e = 0; e < 5; e++) {
            if (enemies[e].active) {
                if (enemies[e].type == 2) {
                    arduboy.drawBitmap((int16_t)enemies[e].x, (int16_t)enemies[e].y, bomber_1943_bmp, 15, 9, WHITE);
                } else {
                    arduboy.drawBitmap((int16_t)enemies[e].x, (int16_t)enemies[e].y, enemy_1943_bmp, 9, 7, WHITE);
                }
            }
        }

        arduboy.setCursor(2, 2); arduboy.print("PTS:"); arduboy.print(score);
        arduboy.setCursor(65, 2); arduboy.print("HI:"); arduboy.print(highScore);

        if (gameOver) {
            arduboy.fillRect(24, 20, 80, 24, BLACK);
            arduboy.drawRect(24, 20, 80, 24, WHITE);
            arduboy.setCursor(34, 24); arduboy.print("GAME OVER");
            arduboy.setCursor(24, 33); arduboy.print("Press B3:Retry");
        }
    }
};
