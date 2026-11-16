#pragma once
#include "console_core.h"

// =============================================================================
// GAME 6: SNAKE (Grid-Locked 4x4, Circular Buffer O(1) & 180° Turn Lock)
// =============================================================================
struct SnakeGame {
    enum Direction { DIR_UP = 0, DIR_RIGHT, DIR_DOWN, DIR_LEFT };

    struct Point {
        int8_t x, y;
    };

    Point body[128];
    uint8_t headIdx;
    uint8_t tailIdx;
    uint8_t length;

    Direction currentDir;
    Direction nextDir;
    Point food;
    uint16_t score;
    uint16_t highScore;
    uint8_t tickCount;
    uint8_t speedTicks;
    bool gameOver;
    bool newHighScoreNotified;

    void checkHighScore() {
        if (score > highScore) {
            if (!newHighScoreNotified) {
                newHighScoreNotified = true;
                triggerRgbLed(0, 255, 0, 1000); // GREEN on beating record
            }
            highScore = score;
            EEPROM.put(20, highScore);
        }
    }

    void init() {
        length = 4;
        headIdx = 3;
        tailIdx = 0;
        currentDir = DIR_RIGHT;
        nextDir = DIR_RIGHT;
        score = 0;
        tickCount = 0;
        speedTicks = 7;
        gameOver = false;
        newHighScoreNotified = false;

        highScore = 0;
        EEPROM.get(20, highScore);
        if (highScore == 0xFFFF) highScore = 0;

        for (uint8_t i = 0; i < length; i++) {
            body[i].x = 10 + i;
            body[i].y = 8;
        }

        spawnFood();
    }

    void spawnFood() {
        bool valid = false;
        while (!valid) {
            food.x = random(1, 31);
            food.y = random(2, 15);
            valid = true;
            for (uint8_t i = 0; i < length; i++) {
                uint8_t idx = (tailIdx + i) % 128;
                if (body[idx].x == food.x && body[idx].y == food.y) {
                    valid = false;
                    break;
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

        // Direction Input with Strict 180° Reverse Lock
        if (arduboy.justPressed(UP_BUTTON) && currentDir != DIR_DOWN) {
            nextDir = DIR_UP;
        } else if (arduboy.justPressed(DOWN_BUTTON) && currentDir != DIR_UP) {
            nextDir = DIR_DOWN;
        } else if (arduboy.justPressed(LEFT_BUTTON) && currentDir != DIR_RIGHT) {
            nextDir = DIR_LEFT;
        } else if (arduboy.justPressed(RIGHT_BUTTON) && currentDir != DIR_LEFT) {
            nextDir = DIR_RIGHT;
        }

        tickCount++;
        if (tickCount >= speedTicks) {
            tickCount = 0;
            currentDir = nextDir;

            Point newHead = body[headIdx];
            if (currentDir == DIR_UP) newHead.y--;
            else if (currentDir == DIR_DOWN) newHead.y++;
            else if (currentDir == DIR_LEFT) newHead.x--;
            else if (currentDir == DIR_RIGHT) newHead.x++;

            // Wall Collision
            if (newHead.x < 0 || newHead.x >= 32 || newHead.y < 2 || newHead.y >= 16) {
                gameOver = true;
                sound.tone(140, 120);
                checkHighScore();
                return;
            }

            // Self Collision
            for (uint8_t i = 0; i < length; i++) {
                uint8_t idx = (tailIdx + i) % 128;
                if (body[idx].x == newHead.x && body[idx].y == newHead.y) {
                    gameOver = true;
                    sound.tone(140, 120);
                    checkHighScore();
                    return;
                }
            }

            // Move Head in Circular Buffer O(1)
            headIdx = (headIdx + 1) % 128;
            body[headIdx] = newHead;

            // Food Check
            if (newHead.x == food.x && newHead.y == food.y) {
                score += 10;
                length++;
                if (length % 4 == 0 && speedTicks > 2) speedTicks--;
                checkHighScore();
                sound.tone(1200, 20);
                spawnFood();
            } else {
                tailIdx = (tailIdx + 1) % 128; // Pop Tail O(1)
            }
        }
    }

    void draw() {
        arduboy.setCursor(2, 0); arduboy.print("PTS:"); arduboy.print(score);
        arduboy.setCursor(65, 0); arduboy.print("HI:"); arduboy.print(highScore);
        arduboy.drawFastHLine(0, 8, 128, WHITE);

        // Draw Food
        arduboy.fillRect(food.x * 4 + 1, food.y * 4 + 1, 2, 2, WHITE);

        // Draw Snake Body
        for (uint8_t i = 0; i < length; i++) {
            uint8_t idx = (tailIdx + i) % 128;
            arduboy.fillRect(body[idx].x * 4, body[idx].y * 4, 3, 3, WHITE);
        }

        if (gameOver) {
            arduboy.fillRect(24, 20, 80, 24, BLACK);
            arduboy.drawRect(24, 20, 80, 24, WHITE);
            arduboy.setCursor(34, 24); arduboy.print("GAME OVER");
            arduboy.setCursor(24, 33); arduboy.print("Press B3:Retry");
        }
    }
};
