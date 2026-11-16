#pragma once
#include "console_core.h"

// =============================================================================
// GAME 11: TIC-TAC-TOE 3D (Isometric Perspective, 3 AI Levels, Pixel-Perfect HUD)
// =============================================================================
enum TTTState {
    TTT_SELECT_MODE = 0,
    TTT_SELECT_DIFFICULTY,
    TTT_PLAYING,
    TTT_GAME_OVER
};

enum TTTGameMode {
    TTT_MODE_1P = 0,
    TTT_MODE_2P
};

enum TTTDifficulty {
    DIFF_EASY = 0,
    DIFF_HARD,
    DIFF_BRAVE
};

struct TicTacToeGame {
    TTTState state;
    TTTGameMode gameMode;
    int8_t modeCursor; // 0: 1 PLAYER, 1: 2 PLAYERS
    TTTDifficulty difficulty;
    int8_t diffCursor; // 0: EASY, 1: HARD, 2: BRAVE
    uint8_t board[3][3]; // 0: Empty, 1: P1 ('X'), 2: CPU / P2 ('O')
    int8_t cursorRow, cursorCol;
    uint16_t playerScore, cpuScore;
    uint8_t winner; // 0: in progress, 1: P1, 2: CPU / P2, 3: Tie
    int8_t winLineType; // 0..2: rows, 3..5: cols, 6: diag TL-BR, 7: diag TR-BL, -1: none
    bool playerTurn; // En 1P: true = turno humano, false = CPU
    uint8_t turnPlayer; // En 2P: 1 = Turno P1 ('X'), 2 = Turno P2 ('O')
    bool cpuTurnPending;
    unsigned long turnDelayTimer;
    bool bootActionTriggered;

    // Isometric Board Geometry Definition (Trapezoidal 3D Perspective)
    static const int16_t Y_TOP = 18;
    static const int16_t Y_BOT = 54;
    static const int16_t Y_RIM = 60;

    int16_t getRayX(int8_t c, int16_t y) const {
        // c in 0..3
        static const int16_t X_TOP_COLS[4] = {40, 56, 72, 88};
        static const int16_t X_BOT_COLS[4] = {12, 47, 81, 116};
        if (c < 0) c = 0;
        if (c > 3) c = 3;
        return X_TOP_COLS[c] + (int32_t)(X_BOT_COLS[c] - X_TOP_COLS[c]) * (y - Y_TOP) / (Y_BOT - Y_TOP);
    }

    void getCellCenter(int8_t r, int8_t c, int16_t &cx, int16_t &cy) const {
        static const int16_t Y_ROWS[4] = {18, 28, 40, 54};
        cy = (Y_ROWS[r] + Y_ROWS[r + 1]) / 2;
        int16_t xl = getRayX(c, cy);
        int16_t xr = getRayX(c + 1, cy);
        cx = (xl + xr) / 2;
    }

    void init() {
        melodyPlayer.stop();
        state = TTT_SELECT_MODE;
        modeCursor = 0;
        diffCursor = 0;
        gameMode = TTT_MODE_1P;
        playerScore = 0;
        cpuScore = 0;
        cursorRow = 1;
        cursorCol = 1;
        bootActionTriggered = false;
        resetRound();
    }

    void resetRound() {
        memset(board, 0, sizeof(board));
        winner = 0;
        winLineType = -1;
        playerTurn = true;
        turnPlayer = 1; // P1 siempre inicia la ronda
        cpuTurnPending = false;
        cursorRow = 1;
        cursorCol = 1;
        bootActionTriggered = false;
    }

    void onBootShortPress() {
        bootActionTriggered = true;
    }

    void update() {
        yield();
        if (state == TTT_SELECT_MODE) {
            updateModeSelect();
        } else if (state == TTT_SELECT_DIFFICULTY) {
            updateDifficultySelect();
        } else if (state == TTT_PLAYING) {
            updatePlaying();
        } else if (state == TTT_GAME_OVER) {
            updateGameOver();
        }
        bootActionTriggered = false;
        yield();
    }

    void updateModeSelect() {
        if (arduboy.justPressed(UP_BUTTON)) {
            modeCursor--;
            if (modeCursor < 0) modeCursor = 1;
            sound.tone(900, 15);
        }
        else if (arduboy.justPressed(DOWN_BUTTON)) {
            modeCursor++;
            if (modeCursor > 1) modeCursor = 0;
            sound.tone(900, 15);
        }

        // Confirmación con A_BUTTON (GPIO 20) o BOOT (GPIO 9)
        if (bootActionTriggered || arduboy.justPressed(A_BUTTON)) {
            sound.tone(1200, 40);
            if (modeCursor == 0) {
                gameMode = TTT_MODE_1P;
                diffCursor = 0;
                state = TTT_SELECT_DIFFICULTY;
            } else {
                gameMode = TTT_MODE_2P;
                resetRound();
                state = TTT_PLAYING;
            }
        }
    }

    void updateDifficultySelect() {
        if (arduboy.justPressed(UP_BUTTON)) {
            diffCursor--;
            if (diffCursor < 0) diffCursor = 2;
            sound.tone(900, 15);
        }
        else if (arduboy.justPressed(DOWN_BUTTON)) {
            diffCursor++;
            if (diffCursor > 2) diffCursor = 0;
            sound.tone(900, 15);
        }

        // Confirmación con A_BUTTON (GPIO 20) o BOOT (GPIO 9)
        if (bootActionTriggered || arduboy.justPressed(A_BUTTON)) {
            difficulty = (TTTDifficulty)diffCursor;
            resetRound();
            state = TTT_PLAYING;
            sound.tone(1200, 40);
        }
    }

    void updatePlaying() {
        if (gameMode == TTT_MODE_1P) {
            if (cpuTurnPending) {
                if (millis() >= turnDelayTimer) {
                    makeCpuMove();
                    cpuTurnPending = false;
                    int8_t line = -1;
                    uint8_t w = checkWinner(board, line);
                    if (w != 0) {
                        endGame(w, line);
                    } else {
                        playerTurn = true;
                    }
                }
                return;
            }

            if (!playerTurn) return;

            // Mover cursor ARRIBA
            if (arduboy.justPressed(UP_BUTTON)) {
                if (cursorRow > 0) {
                    cursorRow--;
                    sound.tone(900, 15);
                }
            }
            // Mover cursor ABAJO
            else if (arduboy.justPressed(DOWN_BUTTON)) {
                if (cursorRow < 2) {
                    cursorRow++;
                    sound.tone(900, 15);
                }
            }
            // Mover cursor DERECHA
            else if (arduboy.justPressed(RIGHT_BUTTON)) {
                if (cursorCol < 2) {
                    cursorCol++;
                    sound.tone(900, 15);
                }
            }
            // Mover cursor IZQUIERDA
            else if (arduboy.justPressed(LEFT_BUTTON)) {
                if (cursorCol > 0) {
                    cursorCol--;
                    sound.tone(900, 15);
                }
            }

            // Selección / Colocación con A_BUTTON (GPIO 20) o BOOT (GPIO 9)
            if (bootActionTriggered || arduboy.justPressed(A_BUTTON)) {
                if (board[cursorRow][cursorCol] == 0) {
                    board[cursorRow][cursorCol] = 1; // PLY ('X')
                    sound.tone(1200, 40);

                    int8_t line = -1;
                    uint8_t w = checkWinner(board, line);
                    if (w != 0) {
                        endGame(w, line);
                    } else {
                        playerTurn = false;
                        cpuTurnPending = true;
                        turnDelayTimer = millis() + 450;
                    }
                } else {
                    // Tono de celda ocupada
                    sound.tone(400, 30);
                }
            }
        } else {
            // =========================================================
            // MODO 2 JUGADORES (TURNOS ALTERNOS: P1 'X' / P2 'O')
            // =========================================================
            // Mover cursor ARRIBA
            if (arduboy.justPressed(UP_BUTTON)) {
                if (cursorRow > 0) {
                    cursorRow--;
                    sound.tone(900, 15);
                }
            }
            // Mover cursor ABAJO
            else if (arduboy.justPressed(DOWN_BUTTON)) {
                if (cursorRow < 2) {
                    cursorRow++;
                    sound.tone(900, 15);
                }
            }
            // Mover cursor DERECHA
            else if (arduboy.justPressed(RIGHT_BUTTON)) {
                if (cursorCol < 2) {
                    cursorCol++;
                    sound.tone(900, 15);
                }
            }
            // Mover cursor IZQUIERDA
            else if (arduboy.justPressed(LEFT_BUTTON)) {
                if (cursorCol > 0) {
                    cursorCol--;
                    sound.tone(900, 15);
                }
            }

            // Selección / Colocación con A_BUTTON (GPIO 20) o BOOT (GPIO 9)
            if (bootActionTriggered || arduboy.justPressed(A_BUTTON)) {
                if (board[cursorRow][cursorCol] == 0) {
                    board[cursorRow][cursorCol] = turnPlayer; // 1 ('X') o 2 ('O')
                    sound.tone(1200, 40);

                    int8_t line = -1;
                    uint8_t w = checkWinner(board, line);
                    if (w != 0) {
                        endGame(w, line);
                    } else {
                        // Cambiar turno al otro jugador inmediatamente
                        turnPlayer = (turnPlayer == 1) ? 2 : 1;
                    }
                } else {
                    // Tono de celda ocupada
                    sound.tone(400, 30);
                }
            }
        }
    }

    void updateGameOver() {
        if (bootActionTriggered || arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
            arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON) ||
            arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
            resetRound();
            state = TTT_PLAYING;
            sound.tone(1200, 40);
        }
    }

    void endGame(uint8_t w, int8_t line) {
        winner = w;
        winLineType = line;
        state = TTT_GAME_OVER;

        if (gameMode == TTT_MODE_1P) {
            if (winner == 1) {
                // Victoria del Jugador
                playerScore++;
                triggerRgbLed(0, 255, 0, 1000); // LED Verde 1s
                melodyPlayer.play(tttVictoryNotes, 4);
            } else if (winner == 2) {
                // Victoria de la CPU
                cpuScore++;
                triggerRgbLed(255, 0, 0, 1000); // LED Rojo 1s
                melodyPlayer.play(tttDefeatNotes, 4);
            } else {
                // Empate
                rgbLedWrite(RGB_LED_PIN, 0, 0, 0); // Apagado
                sound.tone(440, 80);
            }
        } else {
            // MODO 2 JUGADORES
            if (winner == 1) {
                // Victoria Jugador 1
                playerScore++;
                triggerRgbLed(0, 255, 0, 1000); // LED Verde 1s
                melodyPlayer.play(tttVictoryNotes, 4);
            } else if (winner == 2) {
                // Victoria Jugador 2
                cpuScore++;
                triggerRgbLed(0, 255, 255, 1000); // LED Azul/Verde (Cyan) 1s
                melodyPlayer.play(tttVictoryNotes, 4);
            } else {
                // Empate
                rgbLedWrite(RGB_LED_PIN, 0, 0, 0); // Apagado
                sound.tone(440, 80);
            }
        }
    }

    uint8_t checkWinner(const uint8_t b[3][3], int8_t &line) const {
        // Filas
        for (int8_t r = 0; r < 3; r++) {
            if (b[r][0] != 0 && b[r][0] == b[r][1] && b[r][1] == b[r][2]) {
                line = r;
                return b[r][0];
            }
        }
        // Columnas
        for (int8_t c = 0; c < 3; c++) {
            if (b[0][c] != 0 && b[0][c] == b[1][c] && b[1][c] == b[2][c]) {
                line = 3 + c;
                return b[0][c];
            }
        }
        // Diagonal TL-BR
        if (b[0][0] != 0 && b[0][0] == b[1][1] && b[1][1] == b[2][2]) {
            line = 6;
            return b[0][0];
        }
        // Diagonal TR-BL
        if (b[0][2] != 0 && b[0][2] == b[1][1] && b[1][1] == b[2][0]) {
            line = 7;
            return b[0][2];
        }
        // Comprobar celdas vacías
        for (int8_t r = 0; r < 3; r++) {
            for (int8_t c = 0; c < 3; c++) {
                if (b[r][c] == 0) {
                    line = -1;
                    return 0; // Partida en curso
                }
            }
        }
        line = -1;
        return 3; // Empate
    }

    void makeCpuMove() {
        if (difficulty == DIFF_EASY) {
            makeEasyMove();
        } else if (difficulty == DIFF_HARD) {
            makeHardMove();
        } else {
            makeBraveMove();
        }
        sound.tone(1200, 40);
    }

    void makeEasyMove() {
        // Bloquear victoria inminente del jugador solo el 30% de las veces
        if (random(0, 100) < 30) {
            for (int8_t r = 0; r < 3; r++) {
                for (int8_t c = 0; c < 3; c++) {
                    if (board[r][c] == 0) {
                        board[r][c] = 1; // Probar si el jugador ganaría aquí
                        int8_t line = -1;
                        if (checkWinner(board, line) == 1) {
                            board[r][c] = 2; // CPU bloquea la celda
                            return;
                        }
                        board[r][c] = 0;
                    }
                }
            }
        }

        // Movimiento aleatorio entre celdas vacías
        struct Pos { int8_t r, c; } emptyPos[9];
        uint8_t count = 0;
        for (int8_t r = 0; r < 3; r++) {
            for (int8_t c = 0; c < 3; c++) {
                if (board[r][c] == 0) emptyPos[count++] = {r, c};
            }
        }
        if (count > 0) {
            uint8_t idx = random(0, count);
            board[emptyPos[idx].r][emptyPos[idx].c] = 2;
        }
    }

    void makeHardMove() {
        // 1. Completar victoria de CPU al 100%
        for (int8_t r = 0; r < 3; r++) {
            for (int8_t c = 0; c < 3; c++) {
                if (board[r][c] == 0) {
                    board[r][c] = 2;
                    int8_t line = -1;
                    if (checkWinner(board, line) == 2) return;
                    board[r][c] = 0;
                }
            }
        }

        // 2. Bloquear victoria inminente del jugador al 100%
        for (int8_t r = 0; r < 3; r++) {
            for (int8_t c = 0; c < 3; c++) {
                if (board[r][c] == 0) {
                    board[r][c] = 1;
                    int8_t line = -1;
                    if (checkWinner(board, line) == 1) {
                        board[r][c] = 2;
                        return;
                    }
                    board[r][c] = 0;
                }
            }
        }

        // 3. Tomar el centro prioritariamente
        if (board[1][1] == 0) {
            board[1][1] = 2;
            return;
        }

        // 4. Tomar esquinas prioritariamente
        const struct Pos { int8_t r, c; } corners[4] = {{0, 0}, {0, 2}, {2, 0}, {2, 2}};
        Pos availCorners[4];
        uint8_t cCount = 0;
        for (uint8_t i = 0; i < 4; i++) {
            if (board[corners[i].r][corners[i].c] == 0) {
                availCorners[cCount++] = corners[i];
            }
        }
        if (cCount > 0) {
            uint8_t idx = random(0, cCount);
            board[availCorners[idx].r][availCorners[idx].c] = 2;
            return;
        }

        // 5. Tomar bordes
        const Pos edges[4] = {{0, 1}, {1, 0}, {1, 2}, {2, 1}};
        Pos availEdges[4];
        uint8_t eCount = 0;
        for (uint8_t i = 0; i < 4; i++) {
            if (board[edges[i].r][edges[i].c] == 0) {
                availEdges[eCount++] = edges[i];
            }
        }
        if (eCount > 0) {
            uint8_t idx = random(0, eCount);
            board[availEdges[idx].r][availEdges[idx].c] = 2;
        }
    }

    void makeBraveMove() {
        int bestScore = -1000;
        int8_t bestR = -1, bestC = -1;

        for (int8_t r = 0; r < 3; r++) {
            for (int8_t c = 0; c < 3; c++) {
                yield();
                if (board[r][c] == 0) {
                    board[r][c] = 2;
                    int score = minimax(board, 0, false);
                    board[r][c] = 0;
                    if (score > bestScore) {
                        bestScore = score;
                        bestR = r;
                        bestC = c;
                    }
                }
            }
        }

        if (bestR != -1 && bestC != -1) {
            board[bestR][bestC] = 2;
        }
    }

    int minimax(uint8_t b[3][3], int depth, bool isCpu) {
        int8_t line = -1;
        uint8_t w = checkWinner(b, line);
        if (w == 2) return 10 - depth;
        if (w == 1) return depth - 10;
        if (w == 3) return 0;

        if (isCpu) {
            int best = -1000;
            for (int8_t r = 0; r < 3; r++) {
                for (int8_t c = 0; c < 3; c++) {
                    if (b[r][c] == 0) {
                        b[r][c] = 2;
                        int val = minimax(b, depth + 1, false);
                        b[r][c] = 0;
                        if (val > best) best = val;
                    }
                }
            }
            return best;
        } else {
            int best = 1000;
            for (int8_t r = 0; r < 3; r++) {
                for (int8_t c = 0; c < 3; c++) {
                    if (b[r][c] == 0) {
                        b[r][c] = 1;
                        int val = minimax(b, depth + 1, true);
                        b[r][c] = 0;
                        if (val < best) best = val;
                    }
                }
            }
            return best;
        }
    }

    // =========================================================================
    // RENDERING PIPELINE (PIXEL-PERFECT GRAPHICS & ISOMETRIC 3D)
    // =========================================================================
    void draw() {
        if (state == TTT_SELECT_MODE) {
            drawModeSelect();
        } else if (state == TTT_SELECT_DIFFICULTY) {
            drawHUD();
            drawDifficultySelect();
        } else {
            drawHUD();
            drawGameBoard();
        }
    }

    void drawHUD() {
        if (gameMode == TTT_MODE_1P) {
            // Caja Izquierda: PLY : [score]
            if (state == TTT_PLAYING && playerTurn && !cpuTurnPending) {
                arduboy.fillRoundRect(2, 1, 60, 11, 2, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(6, 3);
                arduboy.print("PLY : ");
                arduboy.print(playerScore);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.drawRoundRect(2, 1, 60, 11, 2, WHITE);
                arduboy.setCursor(6, 3);
                arduboy.print("PLY : ");
                arduboy.print(playerScore);
            }
            for (int16_t x = 4; x <= 60; x += 2) {
                arduboy.drawPixel(x, 13, WHITE);
            }

            // Caja Derecha: CPU : [score]
            if (state == TTT_PLAYING && cpuTurnPending) {
                arduboy.fillRoundRect(66, 1, 60, 11, 2, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(70, 3);
                arduboy.print("CPU : ");
                arduboy.print(cpuScore);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.drawRoundRect(66, 1, 60, 11, 2, WHITE);
                arduboy.setCursor(70, 3);
                arduboy.print("CPU : ");
                arduboy.print(cpuScore);
            }
            for (int16_t x = 68; x <= 124; x += 2) {
                arduboy.drawPixel(x, 13, WHITE);
            }
        } else {
            // MODO 2 JUGADORES
            // Caja Izquierda: P1 : [score]
            if (state == TTT_PLAYING && turnPlayer == 1) {
                arduboy.fillRoundRect(2, 1, 60, 11, 2, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(6, 3);
                arduboy.print("P1  : ");
                arduboy.print(playerScore);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.drawRoundRect(2, 1, 60, 11, 2, WHITE);
                arduboy.setCursor(6, 3);
                arduboy.print("P1  : ");
                arduboy.print(playerScore);
            }
            for (int16_t x = 4; x <= 60; x += 2) {
                arduboy.drawPixel(x, 13, WHITE);
            }

            // Caja Derecha: P2 : [score]
            if (state == TTT_PLAYING && turnPlayer == 2) {
                arduboy.fillRoundRect(66, 1, 60, 11, 2, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(70, 3);
                arduboy.print("P2  : ");
                arduboy.print(cpuScore);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.drawRoundRect(66, 1, 60, 11, 2, WHITE);
                arduboy.setCursor(70, 3);
                arduboy.print("P2  : ");
                arduboy.print(cpuScore);
            }
            for (int16_t x = 68; x <= 124; x += 2) {
                arduboy.drawPixel(x, 13, WHITE);
            }
        }
    }

    void drawModeSelect() {
        // Cabecera estilizada
        arduboy.fillRect(0, 0, 128, 11, WHITE);
        arduboy.setTextColor(BLACK);
        arduboy.setCursor(20, 2);
        arduboy.print("TIC-TAC-TOE 3D");
        arduboy.setTextColor(WHITE);

        // Cuadrículas decorativas a los lados
        drawDecorativeGrid(6, 18, true);
        drawDecorativeGrid(98, 18, false);

        // Opciones centrales: "1 PLAYER" y "2 PLAYERS"
        const char* modeLabels[2] = {"1 PLAYER", "2 PLAYERS"};
        static const int16_t optionY[2] = {20, 36};

        for (uint8_t i = 0; i < 2; i++) {
            int16_t y = optionY[i];
            if (i == modeCursor) {
                // Sombra sólida 3D
                arduboy.fillRect(35, y + 2, 58, 12, WHITE);
                // Bloque invertido (Fondo blanco, texto negro)
                arduboy.fillRect(33, y, 58, 12, WHITE);
                arduboy.setTextColor(BLACK);
                int16_t textX = 33 + (58 - strlen(modeLabels[i]) * 6) / 2;
                arduboy.setCursor(textX, y + 2);
                arduboy.print(modeLabels[i]);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.drawRect(33, y, 58, 12, WHITE);
                int16_t textX = 33 + (58 - strlen(modeLabels[i]) * 6) / 2;
                arduboy.setCursor(textX, y + 2);
                arduboy.print(modeLabels[i]);
            }
        }

        // Barra inferior de navegación
        arduboy.setCursor(16, 56);
        arduboy.print("B1/B2: Sel  BOOT: Ok");
    }

    void drawDifficultySelect() {
        // Cuadrícula decorativa izquierda (3x3 clásica con tres 'O' tachadas abajo)
        drawDecorativeGrid(6, 18, true);

        // Cuadrícula decorativa derecha (3x3 clásica decorativa)
        drawDecorativeGrid(98, 18, false);

        // Selector Central Vertical ("EASY", "HARD", "BRAVE")
        const char* diffLabels[3] = {"EASY", "HARD", "BRAVE"};
        static const int16_t optionY[3] = {18, 32, 46};

        for (uint8_t i = 0; i < 3; i++) {
            int16_t y = optionY[i];
            if (i == diffCursor) {
                // Sombra sólida 3D
                arduboy.fillRect(39, y + 2, 50, 11, WHITE);
                // Bloque invertido (Fondo blanco, texto negro)
                arduboy.fillRect(37, y, 50, 11, WHITE);
                arduboy.setTextColor(BLACK);
                int16_t textX = 37 + (50 - strlen(diffLabels[i]) * 6) / 2;
                arduboy.setCursor(textX, y + 2);
                arduboy.print(diffLabels[i]);
                arduboy.setTextColor(WHITE);
            } else {
                int16_t textX = 37 + (50 - strlen(diffLabels[i]) * 6) / 2;
                arduboy.setCursor(textX, y + 2);
                arduboy.print(diffLabels[i]);
            }
        }

        arduboy.setCursor(16, 57);
        arduboy.print("B1/B2: Sel  BOOT: Ok");
    }

    void drawDecorativeGrid(int16_t gx, int16_t gy, bool crossedCircles) {
        // Cuadrícula 24x24 px
        arduboy.drawFastVLine(gx + 8, gy, 24, WHITE);
        arduboy.drawFastVLine(gx + 16, gy, 24, WHITE);
        arduboy.drawFastHLine(gx, gy + 8, 24, WHITE);
        arduboy.drawFastHLine(gx, gy + 16, 24, WHITE);

        // Cruces arriba
        drawMiniCross(gx + 4, gy + 4);
        drawMiniCross(gx + 12, gy + 4);
        drawMiniCross(gx + 20, gy + 4);

        if (crossedCircles) {
            // Tres 'O' abajo tachadas
            for (uint8_t c = 0; c < 3; c++) {
                int16_t ox = gx + 4 + c * 8;
                int16_t oy = gy + 20;
                drawMiniCircle(ox, oy);
            }
            // Línea de tachado a través de las 3 'O'
            arduboy.drawLine(gx + 1, gy + 22, gx + 23, gy + 18, WHITE);
        } else {
            drawMiniCircle(gx + 4, gy + 12);
            drawMiniCross(gx + 12, gy + 12);
            drawMiniCircle(gx + 20, gy + 20);
        }
    }

    void drawMiniCross(int16_t x, int16_t y) {
        arduboy.drawLine(x - 2, y - 2, x + 2, y + 2, WHITE);
        arduboy.drawLine(x - 2, y + 2, x + 2, y - 2, WHITE);
    }

    void drawMiniCircle(int16_t x, int16_t y) {
        arduboy.drawCircle(x, y, 2, WHITE);
    }

    void drawGameBoard() {
        static const int16_t Y_ROWS[4] = {18, 28, 40, 54};

        // 1. Resaltado de celda activa con dither de tablero de ajedrez
        bool showCursor = false;
        if (state == TTT_PLAYING) {
            if (gameMode == TTT_MODE_1P && playerTurn && !cpuTurnPending) showCursor = true;
            else if (gameMode == TTT_MODE_2P) showCursor = true;
        }

        if (showCursor) {
            int16_t yStart = Y_ROWS[cursorRow] + 1;
            int16_t yEnd = Y_ROWS[cursorRow + 1] - 1;
            for (int16_t y = yStart; y <= yEnd; y++) {
                int16_t xl = getRayX(cursorCol, y) + 1;
                int16_t xr = getRayX(cursorCol + 1, y) - 1;
                for (int16_t x = xl; x <= xr; x++) {
                    if (((x + y) & 1) == 0) {
                        arduboy.drawPixel(x, y, WHITE);
                    }
                }
            }
        }

        // 2. Líneas horizontales del tablero trapezoidal
        for (int8_t r = 0; r < 4; r++) {
            int16_t y = Y_ROWS[r];
            int16_t xl = getRayX(0, y);
            int16_t xr = getRayX(3, y);
            arduboy.drawFastHLine(xl, y, xr - xl + 1, WHITE);
        }

        // 3. Líneas convergentes de columnas (Rayos de fuga)
        for (int8_t c = 0; c < 4; c++) {
            arduboy.drawLine(getRayX(c, Y_TOP), Y_TOP, getRayX(c, Y_BOT), Y_BOT, WHITE);
        }

        // 4. Grosor frontal 3D (Bisel y aristas)
        arduboy.drawLine(getRayX(0, Y_BOT), Y_BOT, getRayX(0, Y_BOT), Y_RIM, WHITE);
        arduboy.drawLine(getRayX(3, Y_BOT), Y_BOT, getRayX(3, Y_BOT), Y_RIM, WHITE);
        arduboy.drawFastHLine(getRayX(0, Y_BOT), Y_RIM, getRayX(3, Y_BOT) - getRayX(0, Y_BOT) + 1, WHITE);

        // Divisores verticales de columnas en la cara frontal 3D
        arduboy.drawLine(getRayX(1, Y_BOT), Y_BOT, getRayX(1, Y_BOT), Y_RIM, WHITE);
        arduboy.drawLine(getRayX(2, Y_BOT), Y_BOT, getRayX(2, Y_BOT), Y_RIM, WHITE);

        // Puntos de textura/sombra en bisel frontal
        for (int16_t x = getRayX(0, Y_BOT) + 2; x < getRayX(3, Y_BOT) - 1; x += 3) {
            arduboy.drawPixel(x, Y_BOT + 3, WHITE);
        }

        // 5. Renderizado de fichas 'X' y 'O' en perspectiva 3D
        for (int8_t r = 0; r < 3; r++) {
            for (int8_t c = 0; c < 3; c++) {
                uint8_t val = board[r][c];
                if (val == 1) {
                    drawPerspectiveX(r, c);
                } else if (val == 2) {
                    drawPerspectiveO(r, c);
                }
            }
        }

        // 6. Línea de victoria 3D si hay ganador
        if (state == TTT_GAME_OVER && winner != 3 && winLineType != -1) {
            drawWinningLine3D(winLineType);
        }

        // 7. Cartel de Game Over
        if (state == TTT_GAME_OVER) {
            arduboy.fillRect(20, 22, 88, 18, BLACK);
            arduboy.drawRect(20, 22, 88, 18, WHITE);
            arduboy.drawRect(22, 24, 84, 14, WHITE);
            
            const char* msg = "";
            if (gameMode == TTT_MODE_1P) {
                if (winner == 1) msg = "VICTORIA!";
                else if (winner == 2) msg = "DERROTA!";
                else msg = "EMPATE!";
            } else {
                if (winner == 1) msg = "GANA P1!";
                else if (winner == 2) msg = "GANA P2!";
                else msg = "EMPATE!";
            }
            int16_t textX = 22 + (84 - strlen(msg) * 6) / 2;
            arduboy.setCursor(textX, 27);
            arduboy.print(msg);
        }
    }

    void drawPerspectiveX(int8_t r, int8_t c) {
        int16_t cx, cy;
        getCellCenter(r, c, cx, cy);

        int16_t dx = (r == 0) ? 5 : (r == 1 ? 7 : 10);
        int16_t dy = (r == 0) ? 2 : (r == 1 ? 3 : 4);

        // Trazos dobles de la cruz
        arduboy.drawLine(cx - dx, cy - dy, cx + dx, cy + dy, WHITE);
        arduboy.drawLine(cx - dx, cy + dy, cx + dx, cy - dy, WHITE);

        arduboy.drawLine(cx - dx + 1, cy - dy, cx + dx + 1, cy + dy, WHITE);
        arduboy.drawLine(cx - dx - 1, cy + dy, cx + dx - 1, cy - dy, WHITE);

        // Relieve 3D inferior
        arduboy.drawLine(cx - dx, cy - dy + 1, cx + dx, cy + dy + 1, WHITE);
        arduboy.drawLine(cx - dx, cy + dy + 1, cx + dx, cy - dy + 1, WHITE);
    }

    void drawPerspectiveO(int8_t r, int8_t c) {
        int16_t cx, cy;
        getCellCenter(r, c, cx, cy);

        int16_t rx = (r == 0) ? 6 : (r == 1 ? 8 : 11);
        int16_t ry = (r == 0) ? 3 : (r == 1 ? 4 : 5);
        int16_t depthH = (r == 0) ? 1 : 2;

        // Anillo elíptico inferior y bordes laterales (Profundidad 3D)
        for (int16_t h = 1; h <= depthH; h++) {
            drawEllipseContinuous(cx, cy + h, rx, ry);
        }
        arduboy.drawFastVLine(cx - rx, cy, depthH + 1, WHITE);
        arduboy.drawFastVLine(cx + rx, cy, depthH + 1, WHITE);

        // Anillo elíptico superior completo
        drawEllipseContinuous(cx, cy, rx, ry);

        // Agujero interior (Forma toroidal isométrica)
        if (r == 0) {
            drawEllipseContinuous(cx, cy, rx - 3, ry - 1);
        } else if (r == 1) {
            drawEllipseContinuous(cx, cy, rx - 4, ry - 2);
        } else {
            drawEllipseContinuous(cx, cy, rx - 5, ry - 2);
        }
    }

    void drawEllipseContinuous(int16_t cx, int16_t cy, int16_t rx, int16_t ry) {
        if (rx <= 0 || ry <= 0) return;
        const int NUM_PTS = 24;
        int16_t prevX = cx + rx;
        int16_t prevY = cy;
        for (int i = 1; i <= NUM_PTS; i++) {
            float angle = (i * 2.0f * 3.14159265f) / NUM_PTS;
            int16_t curX = cx + (int16_t)roundf(cosf(angle) * rx);
            int16_t curY = cy + (int16_t)roundf(sinf(angle) * ry);
            arduboy.drawLine(prevX, prevY, curX, curY, WHITE);
            prevX = curX;
            prevY = curY;
        }
    }

    void drawWinningLine3D(int8_t line) {
        int16_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        if (line >= 0 && line <= 2) {
            // Filas
            getCellCenter(line, 0, x1, y1);
            getCellCenter(line, 2, x2, y2);
        } else if (line >= 3 && line <= 5) {
            // Columnas
            int8_t col = line - 3;
            getCellCenter(0, col, x1, y1);
            getCellCenter(2, col, x2, y2);
        } else if (line == 6) {
            // Diagonal TL-BR
            getCellCenter(0, 0, x1, y1);
            getCellCenter(2, 2, x2, y2);
        } else if (line == 7) {
            // Diagonal TR-BL
            getCellCenter(0, 2, x1, y1);
            getCellCenter(2, 0, x2, y2);
        }

        arduboy.drawLine(x1, y1, x2, y2, WHITE);
        arduboy.drawLine(x1, y1 - 1, x2, y2 - 1, WHITE);
        arduboy.drawLine(x1, y1 + 1, x2, y2 + 1, WHITE);
    }
};
