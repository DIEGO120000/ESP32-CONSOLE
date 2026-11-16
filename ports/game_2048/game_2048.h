#pragma once
#include "console_core.h"

// =============================================================================
// GAME 9: 2048 (Classic 4x4 Sliding Puzzle with NVS Persistence & Sound)
// =============================================================================
extern const uint16_t microDigits2048[11];

inline void drawMicroChar2048(int16_t x, int16_t y, char c, uint8_t color) {
    int idx = -1;
    if (c >= '0' && c <= '9') idx = c - '0';
    else if (c == 'k' || c == 'K') idx = 10;
    if (idx < 0) return;
    uint16_t pat = microDigits2048[idx];
    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 3; col++) {
            int bitIdx = 14 - (r * 3 + col);
            if (pat & (1 << bitIdx)) {
                arduboy.drawPixel(x + col, y + r, color);
            }
        }
    }
}

inline void drawTileNumber2048(int16_t cellX, int16_t cellY, uint16_t val, uint8_t color) {
    char buf[8];
    if (val >= 1024) {
        snprintf(buf, sizeof(buf), "%uk", val / 1024);
    } else {
        snprintf(buf, sizeof(buf), "%u", val);
    }
    int len = strlen(buf);
    int width = len * 3 + (len - 1);
    int startX = cellX + 1 + (14 - width) / 2;
    int startY = cellY + 1 + (14 - 5) / 2;
    for (int i = 0; i < len; i++) {
        drawMicroChar2048(startX + i * 4, startY, buf[i], color);
    }
}

struct Game2048 {
    uint16_t board[4][4];
    uint32_t currentScore;
    uint32_t highScore;
    bool gameOver;
    bool won2048;
    uint8_t winBannerTimer;

    void init() {
        melodyPlayer.stop();
        memset(board, 0, sizeof(board));
        currentScore = 0;
        gameOver = false;
        won2048 = false;
        winBannerTimer = 0;

        Preferences prefs;
        prefs.begin("2048_data", true);
        highScore = prefs.getUInt("hi_score", 0);
        prefs.end();

        spawnTile();
        spawnTile();
    }

    void saveHighScore() {
        Preferences prefs;
        prefs.begin("2048_data", false);
        uint32_t savedHi = prefs.getUInt("hi_score", 0);
        if (highScore > savedHi) {
            prefs.putUInt("hi_score", highScore);
        }
        prefs.end();
    }

    void spawnTile() {
        struct Point { uint8_t r, c; } emptyCells[16];
        uint8_t count = 0;

        for (uint8_t r = 0; r < 4; r++) {
            for (uint8_t c = 0; c < 4; c++) {
                if (board[r][c] == 0) {
                    emptyCells[count++] = {r, c};
                }
            }
        }

        if (count > 0) {
            uint8_t idx = random(0, count);
            uint16_t val = (random(0, 100) < 90) ? 2 : 4; // 90% probabilidad de 2, 10% de 4
            board[emptyCells[idx].r][emptyCells[idx].c] = val;
        }
    }

    bool slideLine(uint16_t line[4], uint32_t &scoreGained) {
        uint16_t temp[4] = {0};
        int idx = 0;
        for (int i = 0; i < 4; i++) {
            if (line[i] != 0) {
                temp[idx++] = line[i];
            }
        }

        uint16_t merged[4] = {0};
        int mIdx = 0;
        for (int i = 0; i < idx; i++) {
            if (i < idx - 1 && temp[i] == temp[i + 1]) {
                uint16_t val = temp[i] * 2;
                merged[mIdx++] = val;
                scoreGained += val;
                i++; // Regla de una sola fusión por turno
            } else {
                merged[mIdx++] = temp[i];
            }
        }

        bool changed = false;
        for (int i = 0; i < 4; i++) {
            if (line[i] != merged[i]) {
                changed = true;
                line[i] = merged[i];
            }
        }
        return changed;
    }

    bool moveLeft() {
        bool changed = false;
        uint32_t scoreGained = 0;
        for (uint8_t r = 0; r < 4; r++) {
            if (slideLine(board[r], scoreGained)) changed = true;
        }
        if (changed) afterMove(scoreGained);
        return changed;
    }

    bool moveRight() {
        bool changed = false;
        uint32_t scoreGained = 0;
        for (uint8_t r = 0; r < 4; r++) {
            uint16_t line[4];
            for (int i = 0; i < 4; i++) line[i] = board[r][3 - i];
            if (slideLine(line, scoreGained)) {
                changed = true;
                for (int i = 0; i < 4; i++) board[r][3 - i] = line[i];
            }
        }
        if (changed) afterMove(scoreGained);
        return changed;
    }

    bool moveUp() {
        bool changed = false;
        uint32_t scoreGained = 0;
        for (uint8_t c = 0; c < 4; c++) {
            uint16_t line[4];
            for (int i = 0; i < 4; i++) line[i] = board[i][c];
            if (slideLine(line, scoreGained)) {
                changed = true;
                for (int i = 0; i < 4; i++) board[i][c] = line[i];
            }
        }
        if (changed) afterMove(scoreGained);
        return changed;
    }

    bool moveDown() {
        bool changed = false;
        uint32_t scoreGained = 0;
        for (uint8_t c = 0; c < 4; c++) {
            uint16_t line[4];
            for (int i = 0; i < 4; i++) line[i] = board[3 - i][c];
            if (slideLine(line, scoreGained)) {
                changed = true;
                for (int i = 0; i < 4; i++) board[3 - i][c] = line[i];
            }
        }
        if (changed) afterMove(scoreGained);
        return changed;
    }

    void afterMove(uint32_t scoreGained) {
        currentScore += scoreGained;
        if (currentScore > highScore) {
            highScore = currentScore;
            saveHighScore();
        }

        // Bip de confirmación de movimiento en buzzer
        sound.tone(1400, 15);

        // Comprobación de hito 2048
        if (!won2048) {
            for (uint8_t r = 0; r < 4; r++) {
                for (uint8_t c = 0; c < 4; c++) {
                    if (board[r][c] >= 2048) {
                        won2048 = true;
                        winBannerTimer = 90; // Mostrar notificación durante 1.5s
                        triggerRgbLed(0, 255, 0, 1000); // LED Verde 1s
                        melodyPlayer.play(pingPongVictoryNotes, 4);
                        break;
                    }
                }
                if (won2048) break;
            }
        }

        spawnTile();
        checkGameOver();
    }

    void checkGameOver() {
        // Celdas vacías
        for (uint8_t r = 0; r < 4; r++) {
            for (uint8_t c = 0; c < 4; c++) {
                if (board[r][c] == 0) return;
            }
        }

        // Fusiones horizontales
        for (uint8_t r = 0; r < 4; r++) {
            for (uint8_t c = 0; c < 3; c++) {
                if (board[r][c] == board[r][c + 1]) return;
            }
        }

        // Fusiones verticales
        for (uint8_t c = 0; c < 4; c++) {
            for (uint8_t r = 0; r < 3; r++) {
                if (board[r][c] == board[r + 1][c]) return;
            }
        }

        // Fin de partida
        gameOver = true;
        sound.tone(220, 150);
        saveHighScore();
    }

    void update() {
        yield();
        if (gameOver) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
                arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON) ||
                arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
                init();
            }
            return;
        }

        // Deslizar ARRIBA (UP_BUTTON - GPIO 15)
        if (arduboy.justPressed(UP_BUTTON)) {
            moveUp();
        }
        // Deslizar ABAJO (DOWN_BUTTON - GPIO 18)
        else if (arduboy.justPressed(DOWN_BUTTON)) {
            moveDown();
        }
        // Deslizar IZQUIERDA (LEFT_BUTTON - GPIO 13)
        else if (arduboy.justPressed(LEFT_BUTTON)) {
            moveLeft();
        }
        // Deslizar DERECHA (RIGHT_BUTTON - GPIO 19)
        else if (arduboy.justPressed(RIGHT_BUTTON)) {
            moveRight();
        }
    }

    void draw() {
        // --- LADO IZQUIERDO: Grid 4x4 (60x60 píxeles, X=2..62, Y=2..62) ---
        arduboy.drawRect(2, 2, 60, 60, WHITE);

        // Líneas internas del tablero
        arduboy.drawFastVLine(17, 2, 60, WHITE);
        arduboy.drawFastVLine(32, 2, 60, WHITE);
        arduboy.drawFastVLine(47, 2, 60, WHITE);

        arduboy.drawFastHLine(2, 17, 60, WHITE);
        arduboy.drawFastHLine(2, 32, 60, WHITE);
        arduboy.drawFastHLine(2, 47, 60, WHITE);

        // Renderizado de Fichas
        for (uint8_t r = 0; r < 4; r++) {
            for (uint8_t c = 0; c < 4; c++) {
                uint16_t val = board[r][c];
                int16_t cellX = 2 + c * 15;
                int16_t cellY = 2 + r * 15;

                if (val > 0) {
                    if (val >= 128) {
                        // Ficha invertida de alto nivel
                        arduboy.fillRect(cellX + 1, cellY + 1, 14, 14, WHITE);
                        drawTileNumber2048(cellX, cellY, val, BLACK);
                    } else {
                        // Ficha normal
                        drawTileNumber2048(cellX, cellY, val, WHITE);
                    }
                }
            }
        }

        // --- LADO DERECHO: Panel de Información (X=66..126) ---
        if (winBannerTimer > 0) {
            winBannerTimer--;
            arduboy.fillRect(66, 2, 60, 11, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(72, 4);
            arduboy.print("¡2048!");
            arduboy.setTextColor(WHITE);
        } else {
            arduboy.fillRect(66, 2, 60, 11, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(82, 4);
            arduboy.print("2048");
            arduboy.setTextColor(WHITE);
        }

        // Caja de Score
        arduboy.drawRect(66, 15, 60, 22, WHITE);
        arduboy.setCursor(70, 18);
        arduboy.print("SCORE");
        arduboy.setCursor(70, 27);
        arduboy.print(currentScore);

        // Caja de High Score
        arduboy.drawRect(66, 39, 60, 23, WHITE);
        arduboy.setCursor(70, 42);
        arduboy.print("HI-SCORE");
        arduboy.setCursor(70, 52);
        arduboy.print(highScore);

        // Pantalla de Game Over
        if (gameOver) {
            arduboy.fillRect(10, 14, 108, 36, BLACK);
            arduboy.drawRect(10, 14, 108, 36, WHITE);
            arduboy.drawRect(12, 16, 104, 32, WHITE);
            arduboy.setCursor(38, 19);
            arduboy.print("GAME OVER");
            arduboy.setCursor(24, 29);
            arduboy.print("SCORE: ");
            arduboy.print(currentScore);
            arduboy.setCursor(22, 39);
            arduboy.print("Pulsa un boton");
        }
    }
};
