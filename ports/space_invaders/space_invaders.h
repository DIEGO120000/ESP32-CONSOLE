#pragma once
#include "console_core.h"

// --- Space Invaders Sprites ---
extern const uint8_t alien_crab_1[];
extern const uint8_t alien_crab_2[];
extern const uint8_t alien_squid[];
extern const uint8_t alien_octopus[];
extern const uint8_t player_ship_bmp[];
extern const uint8_t ufo_bmp[];

// =============================================================================
// GAME 3: SPACE INVADERS (Symmetric March, Accelerating Cadence & Bunkers)
// =============================================================================
struct SpaceInvadersGame {
    int16_t playerX;
    int16_t bulletX, bulletY;
    bool bulletActive;

    struct Alien {
        int16_t x, y;
        bool alive;
        uint8_t type;
    } aliens[18];

    int16_t alienDir;
    int16_t alienSpeedTimer;
    bool alienAnimFrame;
    int16_t alienBombX, alienBombY;
    bool alienBombActive;

    int16_t ufoX;
    bool ufoActive;

    uint8_t bunkers[3][8]; // 3 bunkers x 8 columns of 6-bit vertical slices
    uint16_t score;
    uint16_t highScore;
    uint8_t lives;
    bool gameOver;
    bool victory;
    bool newHighScoreNotified;

    void checkHighScore() {
        if (score > highScore) {
            if (!newHighScoreNotified) {
                newHighScoreNotified = true;
                triggerRgbLed(0, 255, 0, 1000); // GREEN on beating record
            }
            highScore = score;
            EEPROM.put(10, highScore);
        }
    }

    void init() {
        playerX = 58;
        bulletActive = false;
        alienBombActive = false;
        alienDir = 1;
        alienSpeedTimer = 0;
        alienAnimFrame = false;
        ufoActive = false;
        ufoX = -20;
        score = 0;
        lives = 3;
        gameOver = false;
        victory = false;
        newHighScoreNotified = false;

        highScore = 0;
        EEPROM.get(10, highScore);
        if (highScore == 0xFFFF) highScore = 0;

        for (uint8_t r = 0; r < 3; r++) {
            for (uint8_t c = 0; c < 6; c++) {
                uint8_t idx = r * 6 + c;
                aliens[idx].x = 10 + (c * 16);
                aliens[idx].y = 12 + (r * 10);
                aliens[idx].alive = true;
                aliens[idx].type = r; // 0: Squid, 1: Crab, 2: Octopus
            }
        }

        for (uint8_t b = 0; b < 3; b++) {
            for (uint8_t col = 0; col < 8; col++) {
                bunkers[b][col] = 0x3F; // 6 bits tall
            }
        }
    }

    void update() {
        yield();
        if (gameOver || victory) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) init();
            return;
        }

        // Player Controls (LEFT: GPIO 13, RIGHT: GPIO 19)
        if (arduboy.pressed(LEFT_BUTTON) && playerX > 2) playerX -= 2;
        if (arduboy.pressed(RIGHT_BUTTON) && playerX < 114) playerX += 2;

        if (arduboy.justPressed(A_BUTTON) && !bulletActive) {
            bulletX = playerX + 5;
            bulletY = 54;
            bulletActive = true;
            sound.tone(1200, 20);
        }

        // Bullet Movement & Collision
        if (bulletActive) {
            bulletY -= 3;
            if (bulletY < 8) bulletActive = false;

            // UFO Collision
            if (ufoActive && bulletX >= ufoX && bulletX <= ufoX + 12 && bulletY <= 12) {
                ufoActive = false;
                bulletActive = false;
                score += 150;
                checkHighScore();
                sound.tone(1800, 80);
            }

            // Alien Collision
            for (uint8_t i = 0; i < 18; i++) {
                if (aliens[i].alive && bulletActive) {
                    if (bulletX >= aliens[i].x && bulletX <= aliens[i].x + 8 &&
                        bulletY >= aliens[i].y && bulletY <= aliens[i].y + 8) {
                        aliens[i].alive = false;
                        bulletActive = false;
                        score += (3 - aliens[i].type) * 10;
                        checkHighScore();
                        sound.tone(450, 40);
                    }
                }
            }

            // Bunker Collision
            for (uint8_t b = 0; b < 3; b++) {
                int16_t bx = 20 + (b * 38);
                if (bulletX >= bx && bulletX < bx + 8 && bulletY >= 46 && bulletY <= 52) {
                    uint8_t col = bulletX - bx;
                    if (bunkers[b][col] != 0) {
                        bunkers[b][col] &= (bunkers[b][col] << 2);
                        bulletActive = false;
                        sound.tone(200, 15);
                    }
                }
            }
        }

        // Inversely Proportional Step Cadence
        uint8_t aliveCount = 0;
        for (uint8_t i = 0; i < 18; i++) if (aliens[i].alive) aliveCount++;

        if (aliveCount == 0) {
            victory = true;
            triggerRgbLed(0, 255, 0, 1000); // GREEN on victory
            checkHighScore();
            sound.tone(1500, 150);
            return;
        }

        alienSpeedTimer++;
        uint8_t stepRate = (aliveCount > 12) ? 7 : ((aliveCount > 6) ? 4 : ((aliveCount > 2) ? 2 : 1));
        if (alienSpeedTimer >= stepRate) {
            alienSpeedTimer = 0;
            alienAnimFrame = !alienAnimFrame;
            sound.tone(90 + (aliveCount * 12), 12);

            bool hitEdge = false;
            for (uint8_t i = 0; i < 18; i++) {
                if (aliens[i].alive) {
                    if ((alienDir == 1 && aliens[i].x >= 118) || (alienDir == -1 && aliens[i].x <= 2)) {
                        hitEdge = true;
                        break;
                    }
                }
            }

            if (hitEdge) {
                alienDir = -alienDir;
                for (uint8_t i = 0; i < 18; i++) {
                    aliens[i].y += 3;
                    if (aliens[i].alive && aliens[i].y >= 48) {
                        gameOver = true;
                        sound.tone(120, 200);
                    }
                }
            } else {
                for (uint8_t i = 0; i < 18; i++) {
                    aliens[i].x += (alienDir * 2);
                }
            }
        }

        // Alien Bomb Drops
        if (!alienBombActive && random(0, 25) == 0) {
            uint8_t lucky = random(0, 18);
            if (aliens[lucky].alive) {
                alienBombX = aliens[lucky].x + 4;
                alienBombY = aliens[lucky].y + 8;
                alienBombActive = true;
            }
        }

        if (alienBombActive) {
            alienBombY += 2;
            if (alienBombY > 64) alienBombActive = false;

            if (alienBombX >= playerX && alienBombX <= playerX + 11 && alienBombY >= 56 && alienBombY <= 62) {
                alienBombActive = false;
                lives--;
                sound.tone(150, 100);
                if (lives == 0) {
                    gameOver = true;
                    if (score > highScore) { highScore = score; EEPROM.put(10, highScore); }
                }
            }

            for (uint8_t b = 0; b < 3; b++) {
                int16_t bx = 20 + (b * 38);
                if (alienBombX >= bx && alienBombX < bx + 8 && alienBombY >= 46 && alienBombY <= 52) {
                    uint8_t col = alienBombX - bx;
                    if (bunkers[b][col] != 0) {
                        bunkers[b][col] &= (bunkers[b][col] >> 2);
                        alienBombActive = false;
                    }
                }
            }
        }

        // UFO Movement
        if (!ufoActive && random(0, 250) == 0) {
            ufoActive = true;
            ufoX = -15;
        }
        if (ufoActive) {
            ufoX += 2;
            if (ufoX > 130) ufoActive = false;
        }
    }

    void draw() {
        arduboy.setCursor(2, 0); arduboy.print("PTS:"); arduboy.print(score);
        arduboy.setCursor(55, 0); arduboy.print("HI:"); arduboy.print(highScore);
        arduboy.setCursor(102, 0); arduboy.print("L:"); arduboy.print(lives);
        arduboy.drawFastHLine(0, 9, 128, WHITE);

        for (uint8_t i = 0; i < 18; i++) {
            if (aliens[i].alive) {
                if (aliens[i].type == 0) {
                    arduboy.drawBitmap(aliens[i].x, aliens[i].y, alien_squid, 8, 8, WHITE);
                } else if (aliens[i].type == 1) {
                    arduboy.drawBitmap(aliens[i].x, aliens[i].y, alienAnimFrame ? alien_crab_1 : alien_crab_2, 8, 8, WHITE);
                } else {
                    arduboy.drawBitmap(aliens[i].x, aliens[i].y, alien_octopus, 8, 8, WHITE);
                }
            }
        }

        if (ufoActive) arduboy.drawBitmap(ufoX, 1, ufo_bmp, 12, 6, WHITE);

        // Bunkers
        for (uint8_t b = 0; b < 3; b++) {
            int16_t bx = 20 + (b * 38);
            for (uint8_t col = 0; col < 8; col++) {
                uint8_t mask = bunkers[b][col];
                for (uint8_t row = 0; row < 6; row++) {
                    if (mask & (1 << row)) {
                        arduboy.drawPixel(bx + col, 46 + row, WHITE);
                    }
                }
            }
        }

        arduboy.drawBitmap(playerX, 56, player_ship_bmp, 11, 7, WHITE);

        if (bulletActive) arduboy.drawFastVLine(bulletX, bulletY, 3, WHITE);
        if (alienBombActive) {
            arduboy.drawPixel(alienBombX, alienBombY, WHITE);
            arduboy.drawPixel(alienBombX, alienBombY + 1, WHITE);
        }

        if (gameOver) {
            arduboy.fillRect(20, 20, 88, 24, BLACK);
            arduboy.drawRect(20, 20, 88, 24, WHITE);
            arduboy.setCursor(34, 24); arduboy.print("GAME OVER");
            arduboy.setCursor(24, 33); arduboy.print("Press B3:Retry");
        } else if (victory) {
            arduboy.fillRect(20, 20, 88, 24, BLACK);
            arduboy.drawRect(20, 20, 88, 24, WHITE);
            arduboy.setCursor(32, 24); arduboy.print("VICTORY!!!");
            arduboy.setCursor(24, 33); arduboy.print("Press B3:Retry");
        }
    }
};
