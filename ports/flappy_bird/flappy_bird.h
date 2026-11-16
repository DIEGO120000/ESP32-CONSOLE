#pragma once
#include "console_core.h"

// =============================================================================
// GAME 5: FLAPPY BIRD (Quadratic Gravity, Crisp Flap & AABB Pipes)
// =============================================================================
struct FlappyGame {
    float birdY, velY;
    int16_t pipeX[2];
    int16_t pipeGapY[2];
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
            EEPROM.put(18, highScore);
        }
    }

    void init() {
        birdY = 28.0f;
        velY = 0.0f;
        pipeX[0] = 128; pipeGapY[0] = 22;
        pipeX[1] = 198; pipeGapY[1] = 28;
        score = 0;
        gameOver = false;
        newHighScoreNotified = false;
        highScore = 0;
        EEPROM.get(18, highScore);
        if (highScore == 0xFFFF) highScore = 0;
    }

    void update() {
        yield();
        if (gameOver) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) || arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) init();
            return;
        }

        // Instant Flap Impulse
        if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
            arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON) ||
            arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
            velY = -2.8f;
            sound.tone(950, 18);
        }

        // Quadratic Gravity
        velY += 0.22f;
        birdY += velY;

        if (birdY < 0) birdY = 0;
        if (birdY > 58.0f) {
            gameOver = true;
            sound.tone(180, 100);
            checkHighScore();
        }

        // Pipe Scrolling & Strict AABB Collision
        for (uint8_t i = 0; i < 2; i++) {
            pipeX[i] -= 2;
            if (pipeX[i] < -14) {
                pipeX[i] = 128;
                pipeGapY[i] = random(10, 36);
                score++;
                checkHighScore();
                sound.tone(1400, 30);
            }

            // Bird AABB Box at x=18..24, y=birdY-3..birdY+3
            if (pipeX[i] < 24 && pipeX[i] + 12 > 18) {
                if ((birdY - 3.0f < pipeGapY[i]) || (birdY + 3.0f > pipeGapY[i] + 22)) {
                    gameOver = true;
                    sound.tone(150, 120);
                    checkHighScore();
                }
            }
        }
    }

    void draw() {
        // Draw Pipes
        for (uint8_t i = 0; i < 2; i++) {
            arduboy.fillRect(pipeX[i], 0, 12, pipeGapY[i], WHITE);
            arduboy.fillRect(pipeX[i], pipeGapY[i] + 22, 12, 64 - (pipeGapY[i] + 22), WHITE);
            arduboy.drawRect(pipeX[i] - 1, pipeGapY[i] - 3, 14, 3, WHITE);
            arduboy.drawRect(pipeX[i] - 1, pipeGapY[i] + 22, 14, 3, WHITE);
        }

        // Bird (Circle with eye & beak)
        arduboy.fillCircle(20, (int16_t)birdY, 3, WHITE);
        arduboy.drawPixel(22, (int16_t)birdY - 1, BLACK); // Eye
        arduboy.drawPixel(24, (int16_t)birdY, WHITE);     // Beak

        arduboy.setCursor(2, 2); arduboy.print("PTS: "); arduboy.print(score);
        arduboy.setCursor(65, 2); arduboy.print("HI: "); arduboy.print(highScore);

        if (gameOver) {
            arduboy.fillRect(24, 20, 80, 24, BLACK);
            arduboy.drawRect(24, 20, 80, 24, WHITE);
            arduboy.setCursor(34, 24); arduboy.print("GAME OVER");
            arduboy.setCursor(24, 33); arduboy.print("Press B3:Flap");
        }
    }
};
