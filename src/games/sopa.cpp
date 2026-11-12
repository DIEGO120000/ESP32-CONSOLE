#include "WordSearchGame.h"

// =============================================================================
// JUEGO: SOPA DE LETRAS PANTALLA COMPLETA CON VISTAS DESLIZABLES (ESP32-C6)
// Cuadrícula simétrica y espaciosa: 15 columnas x 7 filas (celda 8x8 px)
// Espaciado ampliado: 3 píxeles limpios de separación entre letras (A-Z)
// =============================================================================

static const uint8_t GRID_COLS = 15;
static const uint8_t GRID_ROWS = 7;
static const int16_t GRID_X0 = 4;
static const int16_t GRID_Y0 = 4;

// Búferes estáticos dedicados para transición de desplazamiento horizontal (slide)
static uint8_t slideBufA[1024];
static uint8_t slideBufB[1024];

static const int8_t DIRECTIONS[8][2] = {
    {1, 0},   // Horizontal derecha ->
    {-1, 0},  // Horizontal izquierda <- (reversa)
    {0, 1},   // Vertical abajo v
    {0, -1},  // Vertical arriba ^ (reversa)
    {1, 1},   // Diagonal abajo-derecha \
    {-1, -1}, // Diagonal arriba-izquierda (reversa)
    {1, -1},  // Diagonal arriba-derecha
    {-1, 1}   // Diagonal abajo-izquierda
};

static bool canPlaceWord(const char grid[7][16], const char* word, int8_t sc, int8_t sr, int8_t dc, int8_t dr) {
    uint8_t len = strlen(word);
    int8_t ec = sc + (len - 1) * dc;
    int8_t er = sr + (len - 1) * dr;
    if (ec < 0 || ec >= GRID_COLS || er < 0 || er >= GRID_ROWS) return false;
    for (uint8_t i = 0; i < len; i++) {
        char ch = grid[sr + i * dr][sc + i * dc];
        if (ch != '\0' && ch != word[i]) {
            return false;
        }
    }
    return true;
}

static void placeWord(char grid[7][16], const char* word, int8_t sc, int8_t sr, int8_t dc, int8_t dr) {
    uint8_t len = strlen(word);
    for (uint8_t i = 0; i < len; i++) {
        grid[sr + i * dr][sc + i * dc] = word[i];
    }
}

void WordSearchGame::init() {
    loadHighScore();
    score = 0;
    exitRequested = false;
    gameOver = false;
    victory = false;
    difficultyCursor = 0;
    currentDifficulty = SOPA_DIFF_FACIL;
    state = SOPA_STATE_DIFFICULTY;
    vistaActual = 0;
    scrollIndex = 0;
    heldDirection = 0;
    dirHoldStartMs = 0;
    dirLastRepeatMs = 0;
}

void WordSearchGame::drawDifficultyScreen() {
    arduboy.clear();
    // Marco exterior con esquinas redondeadas
    arduboy.drawRoundRect(14, 6, 100, 52, 3, WHITE);

    // Título [ DIFICULTAD ]
    arduboy.setTextColor(WHITE);
    arduboy.setCursor(26, 10);
    arduboy.print("[ DIFICULTAD ]");
    arduboy.drawFastHLine(20, 20, 88, WHITE);

    // Opciones FÁCIL, MEDIO, DIFÍCIL
    const char* diffNames[3] = {"FACIL", "MEDIO", "DIFICIL"};
    for (uint8_t i = 0; i < 3; i++) {
        int16_t y = 24 + i * 9;
        if (difficultyCursor == i) {
            arduboy.fillRect(22, y - 1, 84, 9, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(28, y);
            arduboy.print("> ");
            arduboy.print(diffNames[i]);
            arduboy.setTextColor(WHITE);
        } else {
            arduboy.setTextColor(WHITE);
            arduboy.setCursor(34, y);
            arduboy.print(diffNames[i]);
        }
    }

    // Pie de controles
    arduboy.setCursor(22, 51);
    arduboy.print("A:Selec  B:Salir");
}

void WordSearchGame::generateRandomPuzzle() {
    randomSeed(analogRead(0) ^ millis() ^ esp_random());
    state = SOPA_STATE_PLAYING;
    cursorCol = 0;
    cursorRow = 0;
    isSelecting = false;
    foundCount = 0;
    victory = false;
    vistaActual = 0;
    scrollIndex = 0;
    heldDirection = 0;
    dirHoldStartMs = 0;
    dirLastRepeatMs = 0;

    // Configuración por dificultad:
    // FÁCIL:
    //   - 7 palabras (longitud 4 a 5 letras)
    //   - Direcciones: Horizontal (->) y Vertical (v)
    // MEDIO:
    //   - 8 palabras (longitud 4 a 6 letras)
    //   - Direcciones: Horizontal (->), Vertical (v) y Diagonal descendente (\)
    // DIFÍCIL:
    //   - 8 palabras (longitud 5 a 7 letras)
    //   - Direcciones: 8 direcciones posibles (incluyendo reversas)
    uint8_t minLen = 4;
    uint8_t maxLen = 5;
    uint8_t numDirs = 2;
    int8_t activeDirs[8][2];

    if (currentDifficulty == SOPA_DIFF_FACIL) {
        activeWordCount = 7;
        minLen = 4;
        maxLen = 5;
        numDirs = 2;
        activeDirs[0][0] = 1; activeDirs[0][1] = 0; // Horizontal ->
        activeDirs[1][0] = 0; activeDirs[1][1] = 1; // Vertical v
    } else if (currentDifficulty == SOPA_DIFF_MEDIO) {
        activeWordCount = 8;
        minLen = 4;
        maxLen = 6;
        numDirs = 3;
        activeDirs[0][0] = 1; activeDirs[0][1] = 0; // Horizontal ->
        activeDirs[1][0] = 0; activeDirs[1][1] = 1; // Vertical v
        activeDirs[2][0] = 1; activeDirs[2][1] = 1; // Diagonal descendente \ (dx=1, dy=1)
    } else { // SOPA_DIFF_DIFICIL
        activeWordCount = 8;
        minLen = 5;
        maxLen = 7;
        numDirs = 8;
        for (uint8_t d = 0; d < 8; d++) {
            activeDirs[d][0] = DIRECTIONS[d][0];
            activeDirs[d][1] = DIRECTIONS[d][1];
        }
    }

    char candidatePool[10][12];

    for (uint8_t masterAttempt = 0; masterAttempt < 50; masterAttempt++) {
        // En casos extremos de colocación tras 35 intentos, adaptar a 7 palabras
        if (masterAttempt >= 35 && activeWordCount > 7) {
            activeWordCount = 7;
        }

        for (uint8_t wIdx = 0; wIdx < activeWordCount; wIdx++) {
            bool unique = false;
            for (uint16_t tr = 0; tr < 300; tr++) {
                uint16_t randIdx = random(WORD_COUNT);
                char temp[12];
                for (uint8_t c = 0; c < 11; c++) {
                    temp[c] = pgm_read_byte(&words_es[randIdx][c]);
                    if (temp[c] == '\0') break;
                }
                temp[11] = '\0';
                uint8_t len = strlen(temp);
                if (len >= minLen && len <= maxLen) {
                    bool duplicate = false;
                    for (uint8_t prev = 0; prev < wIdx; prev++) {
                        if (strcmp(candidatePool[prev], temp) == 0) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        strcpy(candidatePool[wIdx], temp);
                        unique = true;
                        break;
                    }
                }
            }
            if (!unique) {
                const char* fallbackWords[8] = {"PLAYA", "BARCO", "CIELO", "MONTE", "SELVA", "VIENTO", "FUEGO", "ARENA"};
                strcpy(candidatePool[wIdx], fallbackWords[wIdx % 8]);
            }
        }

        // Ordenar palabras de mayor a menor longitud para maximizar colocación en el grid
        for (uint8_t i = 0; i < activeWordCount - 1; i++) {
            for (uint8_t j = i + 1; j < activeWordCount; j++) {
                if (strlen(candidatePool[j]) > strlen(candidatePool[i])) {
                    char swapBuf[12];
                    strcpy(swapBuf, candidatePool[i]);
                    strcpy(candidatePool[i], candidatePool[j]);
                    strcpy(candidatePool[j], swapBuf);
                }
            }
        }

        char testGrid[7][16];
        for (uint8_t r = 0; r < GRID_ROWS; r++) {
            memset(testGrid[r], 0, sizeof(testGrid[r]));
        }

        struct Placement {
            int8_t sc, sr, dc, dr;
        } placements[10];

        bool allPlaced = true;

        for (uint8_t w = 0; w < activeWordCount; w++) {
            const char* word = candidatePool[w];

            // Pase 1: Contar opciones válidas
            uint16_t validCount = 0;
            for (int8_t sr = 0; sr < GRID_ROWS; sr++) {
                for (int8_t sc = 0; sc < GRID_COLS; sc++) {
                    for (uint8_t d = 0; d < numDirs; d++) {
                        if (canPlaceWord(testGrid, word, sc, sr, activeDirs[d][0], activeDirs[d][1])) {
                            validCount++;
                        }
                    }
                }
            }

            if (validCount == 0) {
                allPlaced = false;
                break;
            }

            // Pase 2: Seleccionar opción aleatoria uniforme
            uint16_t chosenIdx = random(validCount);
            uint16_t cur = 0;
            bool placedCur = false;
            for (int8_t sr = 0; sr < GRID_ROWS; sr++) {
                for (int8_t sc = 0; sc < GRID_COLS; sc++) {
                    for (uint8_t d = 0; d < numDirs; d++) {
                        if (canPlaceWord(testGrid, word, sc, sr, activeDirs[d][0], activeDirs[d][1])) {
                            if (cur == chosenIdx) {
                                placements[w] = {sc, sr, activeDirs[d][0], activeDirs[d][1]};
                                placedCur = true;
                                break;
                            }
                            cur++;
                        }
                    }
                    if (placedCur) break;
                }
                if (placedCur) break;
            }

            placeWord(testGrid, word, placements[w].sc, placements[w].sr,
                      placements[w].dc, placements[w].dr);
        }

        if (allPlaced) {
            // Copiar matriz y rellenar celdas libres con letras aleatorias
            for (uint8_t r = 0; r < GRID_ROWS; r++) {
                for (uint8_t c = 0; c < GRID_COLS; c++) {
                    if (testGrid[r][c] != '\0') {
                        currentGrid[r][c] = testGrid[r][c];
                    } else {
                        currentGrid[r][c] = 'A' + random(26);
                    }
                }
                currentGrid[r][GRID_COLS] = '\0';
            }

            for (uint8_t w = 0; w < activeWordCount; w++) {
                strcpy(activeWords[w].text, candidatePool[w]);
                uint8_t len = strlen(candidatePool[w]);
                activeWords[w].startC = placements[w].sc;
                activeWords[w].startR = placements[w].sr;
                activeWords[w].endC = placements[w].sc + (len - 1) * placements[w].dc;
                activeWords[w].endR = placements[w].sr + (len - 1) * placements[w].dr;
                activeWords[w].found = false;
            }

            return; // Cuadrícula completada exitosamente
        }
    }
}

void WordSearchGame::transitionToView(uint8_t targetView) {
    if (vistaActual == targetView) return;
    sound.tone(900, 20);

    // 1. Capturar pantalla de vista actual
    arduboy.clear();
    if (vistaActual == 0) drawBoardView();
    else drawWordListView();
    memcpy(slideBufA, arduboy.getBuffer(), 1024);

    // 2. Preparar búfer de la vista destino
    vistaActual = targetView;
    arduboy.clear();
    if (vistaActual == 0) drawBoardView();
    else drawWordListView();
    memcpy(slideBufB, arduboy.getBuffer(), 1024);

    // 3. Animación de desplazamiento horizontal fluido
    if (targetView == 1) {
        // Desplazamiento a la izquierda: Tablero sale por la izq, Info entra por la derecha
        for (int16_t offset = 16; offset <= 128; offset += 16) {
            uint8_t* dst = arduboy.getBuffer();
            for (uint8_t page = 0; page < 8; page++) {
                uint8_t* pageDst = dst + (page * 128);
                uint8_t* src0 = slideBufA + (page * 128);
                uint8_t* src1 = slideBufB + (page * 128);
                memcpy(pageDst, src0 + offset, 128 - offset);
                memcpy(pageDst + (128 - offset), src1, offset);
            }
            arduboy.display();
            delay(10);
        }
    } else {
        // Desplazamiento a la derecha: Info sale por la derecha, Tablero entra por la izq
        for (int16_t offset = 16; offset <= 128; offset += 16) {
            uint8_t* dst = arduboy.getBuffer();
            for (uint8_t page = 0; page < 8; page++) {
                uint8_t* pageDst = dst + (page * 128);
                uint8_t* src0 = slideBufA + (page * 128); // Info
                uint8_t* src1 = slideBufB + (page * 128); // Tablero
                memcpy(pageDst, src1 + (128 - offset), offset);
                memcpy(pageDst + offset, src0, 128 - offset);
            }
            arduboy.display();
            delay(10);
        }
    }
}

void WordSearchGame::update() {
    if (state == SOPA_STATE_DIFFICULTY) {
        if (arduboy.justPressed(UP_BUTTON)) {
            if (difficultyCursor > 0) {
                difficultyCursor--;
                sound.tone(880, 15);
            }
        }
        if (arduboy.justPressed(DOWN_BUTTON)) {
            if (difficultyCursor < 2) {
                difficultyCursor++;
                sound.tone(700, 15);
            }
        }
        if (arduboy.justPressed(A_BUTTON)) {
            currentDifficulty = (SopaDifficulty)difficultyCursor;
            sound.tone(1200, 30);
            generateRandomPuzzle();
            state = SOPA_STATE_PLAYING;
        }
        if (arduboy.justPressed(B_BUTTON)) {
            sound.tone(450, 25);
            exitRequested = true;
        }
        return;
    }

    if (state == SOPA_STATE_VICTORY) {
        if (arduboy.justPressed(A_BUTTON)) {
            sound.tone(1400, 30);
            generateRandomPuzzle();
        } else if (arduboy.justPressed(B_BUTTON)) {
            sound.tone(450, 25);
            exitRequested = true;
        }
        return;
    }

    // -------------------------------------------------------------------------
    // SISTEMA DE DESLIZAMIENTO AUTOMÁTICO (AUTO-REPEAT / DAS)
    // -------------------------------------------------------------------------
    static const uint16_t DAS_INITIAL_DELAY_MS = 240;  // Retardo inicial al mantener pulsado
    static const uint16_t DAS_REPEAT_INTERVAL_MS = 85; // Velocidad de repetición continua fluida

    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;

    uint32_t nowMs = millis();

    uint8_t currentPressedDir = 0;
    if (arduboy.pressed(UP_BUTTON)) currentPressedDir = 1;
    else if (arduboy.pressed(DOWN_BUTTON)) currentPressedDir = 2;
    else if (arduboy.pressed(LEFT_BUTTON)) currentPressedDir = 3;
    else if (arduboy.pressed(RIGHT_BUTTON)) currentPressedDir = 4;

    if (currentPressedDir != 0) {
        if (heldDirection != currentPressedDir) {
            // Primer toque de la dirección
            heldDirection = currentPressedDir;
            dirHoldStartMs = nowMs;
            dirLastRepeatMs = nowMs;
            if (heldDirection == 1) moveUp = true;
            else if (heldDirection == 2) moveDown = true;
            else if (heldDirection == 3) moveLeft = true;
            else if (heldDirection == 4) moveRight = true;
        } else {
            // Dirección sostenida en el tiempo
            if ((nowMs - dirHoldStartMs >= DAS_INITIAL_DELAY_MS) &&
                (nowMs - dirLastRepeatMs >= DAS_REPEAT_INTERVAL_MS)) {
                dirLastRepeatMs = nowMs;
                if (heldDirection == 1) moveUp = true;
                else if (heldDirection == 2) moveDown = true;
                else if (heldDirection == 3) moveLeft = true;
                else if (heldDirection == 4) moveRight = true;
            }
        }
    } else {
        heldDirection = 0;
    }

    // -------------------------------------------------------------------------
    // VISTA 1: PANEL DE INFORMACIÓN / PALABRAS
    // -------------------------------------------------------------------------
    if (vistaActual == 1) {
        // Regresar de inmediato al Tablero con cruceta LEFT
        if (arduboy.justPressed(LEFT_BUTTON)) {
            cursorCol = GRID_COLS - 1; // 14
            heldDirection = 0;
            transitionToView(0);
            return;
        }

        // Regresar también con Botón A
        if (arduboy.justPressed(A_BUTTON)) {
            heldDirection = 0;
            transitionToView(0);
            return;
        }

        // Salir al menú con Botón B
        if (arduboy.justPressed(B_BUTTON)) {
            sound.tone(450, 25);
            exitRequested = true;
            return;
        }

        // Desplazamiento vertical (scroll) continuo con UP / DOWN
        uint8_t maxScroll = (activeWordCount > 5) ? (activeWordCount - 5) : 0;
        if (moveUp) {
            if (scrollIndex > 0) {
                scrollIndex--;
                sound.tone(880, 15);
            }
        }
        if (moveDown) {
            if (scrollIndex < maxScroll) {
                scrollIndex++;
                sound.tone(700, 15);
            }
        }
        return;
    }

    // -------------------------------------------------------------------------
    // VISTA 0: TABLERO A PANTALLA COMPLETA
    // -------------------------------------------------------------------------
    // Desplazamiento por las 7 filas y 15 columnas con deslizamiento continuo al mantener presionado
    if (moveUp) {
        if (cursorRow > 0) {
            cursorRow--;
            sound.tone(880, 15);
        }
    }
    if (moveDown) {
        if (cursorRow < GRID_ROWS - 1) {
            cursorRow++;
            sound.tone(700, 15);
        }
    }
    if (moveLeft) {
        if (cursorCol > 0) {
            cursorCol--;
            sound.tone(800, 15);
        }
    }
    if (moveRight) {
        if (cursorCol < GRID_COLS - 1) {
            cursorCol++;
            sound.tone(750, 15);
        }
    }

    // Transición deliberada a VISTA 1: solo al pulsar RIGHT cuando ya se está en el borde derecho
    if (arduboy.justPressed(RIGHT_BUTTON) && (cursorCol == GRID_COLS - 1) && !isSelecting) {
        heldDirection = 0;
        transitionToView(1);
        return;
    }

    // Botón A: Fijar inicio de palabra o validar selección
    if (arduboy.justPressed(A_BUTTON)) {
        if (!isSelecting) {
            isSelecting = true;
            startCol = cursorCol;
            startRow = cursorRow;
            sound.tone(1100, 25);
        } else {
            validateSelection();
        }
    }

    // Botón B: Cancelar selección activa o salir al menú principal
    if (arduboy.justPressed(B_BUTTON)) {
        if (isSelecting) {
            isSelecting = false;
            sound.tone(450, 25);
        } else {
            exitRequested = true;
        }
    }
}

void WordSearchGame::drawBoardView() {
    arduboy.clear();

    // 1. Marco contenedor general perfectamente ajustado al panel
    arduboy.drawRoundRect(2, 2, 124, 60, 2, WHITE);

    // 2. Matriz de letras a pantalla completa (15 columnas x 7 filas, 8x8 px por celda)
    // Fuente retro 5x5 px con 3 píxeles de espacio libre entre cada letra horizontal y verticalmente
    for (uint8_t r = 0; r < GRID_ROWS; r++) {
        for (uint8_t c = 0; c < GRID_COLS; c++) {
            int16_t x = GRID_X0 + c * 8;
            int16_t y = GRID_Y0 + r * 8;
            drawLetter5x5(x + 1, y + 1, currentGrid[r][c], WHITE);
        }
    }

    // 3. Palabras encontradas: recuadros redondeados limpios y amplios
    for (uint8_t i = 0; i < activeWordCount; i++) {
        if (activeWords[i].found) {
            drawFoundBox(activeWords[i].startC, activeWords[i].startR,
                         activeWords[i].endC, activeWords[i].endR);
        }
    }

    // 4. Retícula y selección activa
    if (isSelecting) {
        // Píldora sólida 8x8 con letra en negro
        int16_t sx = GRID_X0 + startCol * 8;
        int16_t sy = GRID_Y0 + startRow * 8;
        arduboy.fillRoundRect(sx, sy, 8, 8, 2, WHITE);
        drawLetter5x5(sx + 1, sy + 1, currentGrid[startRow][startCol], BLACK);

        // Bounding box en tiempo real del camino
        int8_t dc = cursorCol - startCol;
        int8_t dr = cursorRow - startRow;
        if (dr == 0) {
            int8_t minC = min(startCol, cursorCol);
            int8_t maxC = max(startCol, cursorCol);
            arduboy.drawRoundRect(GRID_X0 + minC * 8, GRID_Y0 + startRow * 8, (maxC - minC + 1) * 8, 8, 2, WHITE);
        } else if (dc == 0) {
            int8_t minR = min(startRow, cursorRow);
            int8_t maxR = max(startRow, cursorRow);
            arduboy.drawRoundRect(GRID_X0 + startCol * 8, GRID_Y0 + minR * 8, 8, (maxR - minR + 1) * 8, 2, WHITE);
        } else if (abs(dc) == abs(dr)) {
            int8_t steps = abs(dc);
            int8_t stepC = (dc > 0) ? 1 : -1;
            int8_t stepR = (dr > 0) ? 1 : -1;
            for (int8_t s = 0; s <= steps; s++) {
                int16_t x = GRID_X0 + (startCol + s * stepC) * 8;
                int16_t y = GRID_Y0 + (startRow + s * stepR) * 8;
                arduboy.drawRoundRect(x, y, 8, 8, 2, WHITE);
            }
        }

        // Cursor de corchetes en la posición actual
        drawCornerBrackets(GRID_X0 + cursorCol * 8, GRID_Y0 + cursorRow * 8);
    } else {
        // Cursor de corchetes parpadeante en modo reposo
        if ((arduboy.frameCount % 24) < 16) {
            drawCornerBrackets(GRID_X0 + cursorCol * 8, GRID_Y0 + cursorRow * 8);
        }
    }

    // 5. Indicador visual discreto ("INFO >") en la esquina inferior derecha
    int16_t bx = 93;
    int16_t by = 58;
    bool hilite = (cursorCol == GRID_COLS - 1) && ((arduboy.frameCount % 16) < 8);
    if (hilite) {
        arduboy.fillRect(bx - 2, by - 1, 31, 7, WHITE);
        drawMiniText(bx, by, "INFO >", BLACK);
    } else {
        arduboy.fillRect(bx - 2, by - 1, 31, 7, BLACK);
        drawMiniText(bx, by, "INFO >", WHITE);
    }
}

void WordSearchGame::drawWordListView() {
    arduboy.clear();

    // 1. Encabezado con título centrado: "SOPA - PALABRAS (X/Y)"
    char titleStr[32];
    snprintf(titleStr, sizeof(titleStr), "SOPA - PALABRAS (%d/%d)", foundCount, activeWordCount);
    int16_t titleLen = strlen(titleStr);
    int16_t xTitle = (128 - (titleLen * 6 - 1)) / 2;
    if (xTitle < 1) xTitle = 1;

    arduboy.setTextColor(WHITE);
    arduboy.setCursor(xTitle, 1);
    arduboy.print(titleStr);

    // Línea separadora bajo el encabezado
    arduboy.drawLine(0, 10, 127, 10, WHITE);

    // 2. Lista de palabras (fuente nativa 5x7 px)
    uint8_t visibleRows = 5;
    for (uint8_t r = 0; r < visibleRows; r++) {
        uint8_t idx = scrollIndex + r;
        if (idx >= activeWordCount) break;

        int16_t y = 13 + r * 8;
        arduboy.setCursor(6, y);

        if (currentDifficulty == SOPA_DIFF_FACIL) {
            // MODO FÁCIL: Lista de palabras en texto plano
            arduboy.printf("%d: %s", idx + 1, activeWords[idx].text);
            if (activeWords[idx].found) {
                arduboy.print("  [X]");
                // Tachado limpio sobre la palabra encontrada
                int16_t prefixW = (idx >= 9) ? 24 : 18;
                int16_t textStartX = 6 + prefixW;
                int16_t wordLen = strlen(activeWords[idx].text);
                arduboy.drawFastHLine(textStartX, y + 3, wordLen * 6 - 1, WHITE);
            }
        } else {
            // MODO MEDIO / DIFÍCIL:
            // Modo pista tipo Ahorcado con guiones bajos según longitud
            if (activeWords[idx].found) {
                // Al descubrirla: reemplazada por el texto real con [X] y tachado
                arduboy.printf("%d: %s  [X]", idx + 1, activeWords[idx].text);
                int16_t prefixW = (idx >= 9) ? 24 : 18;
                int16_t textStartX = 6 + prefixW;
                int16_t wordLen = strlen(activeWords[idx].text);
                arduboy.drawFastHLine(textStartX, y + 3, wordLen * 6 - 1, WHITE);
            } else {
                // Oculta: muestra únicamente los guiones bajos por letra
                char clue[24];
                uint8_t wlen = strlen(activeWords[idx].text);
                uint8_t pos = 0;
                for (uint8_t k = 0; k < wlen && pos < 22; k++) {
                    clue[pos++] = '_';
                    if (k < wlen - 1) clue[pos++] = ' ';
                }
                clue[pos] = '\0';
                arduboy.printf("%d: %s", idx + 1, clue);
            }
        }
    }

    // Indicadores discretos de scroll vertical
    uint8_t maxScroll = (activeWordCount > 5) ? (activeWordCount - 5) : 0;
    if (scrollIndex > 0) {
        arduboy.drawPixel(123, 13, WHITE);
        arduboy.drawPixel(122, 14, WHITE);
        arduboy.drawPixel(124, 14, WHITE);
    }
    if (scrollIndex < maxScroll) {
        arduboy.drawPixel(122, 49, WHITE);
        arduboy.drawPixel(124, 49, WHITE);
        arduboy.drawPixel(123, 50, WHITE);
    }

    // 3. Pie de página
    arduboy.drawLine(0, 54, 127, 54, WHITE);
    drawMiniText(8, 57, "< VOLVER (LEFT)    B: SALIR", WHITE);
}

void WordSearchGame::draw() {
    if (state == SOPA_STATE_DIFFICULTY) {
        drawDifficultyScreen();
        return;
    }

    if (vistaActual == 1) {
        drawWordListView();
    } else {
        drawBoardView();
    }

    // -------------------------------------------------------------------------
    // VENTANA MODAL DE VICTORIA
    // -------------------------------------------------------------------------
    if (state == SOPA_STATE_VICTORY) {
        arduboy.fillRect(8, 12, 112, 38, BLACK);
        arduboy.drawRoundRect(8, 12, 112, 38, 3, WHITE);
        arduboy.setTextColor(WHITE);
        arduboy.setCursor(14, 16);
        arduboy.print("!SOPA COMPLETADA!");
        arduboy.setCursor(24, 26);
        arduboy.printf("PUNTOS: %u", (unsigned int)score);
        arduboy.setCursor(16, 36);
        arduboy.print("A:Nueva B:Salir");
    }
}

void WordSearchGame::drawCornerBrackets(int16_t cx, int16_t cy) {
    int16_t x0 = cx;
    int16_t y0 = cy;
    int16_t x1 = cx + 7;
    int16_t y1 = cy + 7;
    // Top-left
    arduboy.drawPixel(x0, y0, WHITE);
    arduboy.drawPixel(x0 + 1, y0, WHITE);
    arduboy.drawPixel(x0, y0 + 1, WHITE);
    // Top-right
    arduboy.drawPixel(x1, y0, WHITE);
    arduboy.drawPixel(x1 - 1, y0, WHITE);
    arduboy.drawPixel(x1, y0 + 1, WHITE);
    // Bottom-left
    arduboy.drawPixel(x0, y1, WHITE);
    arduboy.drawPixel(x0 + 1, y1, WHITE);
    arduboy.drawPixel(x0, y1 - 1, WHITE);
    // Bottom-right
    arduboy.drawPixel(x1, y1, WHITE);
    arduboy.drawPixel(x1 - 1, y1, WHITE);
    arduboy.drawPixel(x1, y1 - 1, WHITE);
}

void WordSearchGame::drawFoundBox(int8_t sc, int8_t sr, int8_t ec, int8_t er) {
    if (sr == er) {
        // Horizontal
        int8_t minC = min(sc, ec);
        int8_t maxC = max(sc, ec);
        arduboy.drawRoundRect(GRID_X0 + minC * 8, GRID_Y0 + sr * 8, (maxC - minC + 1) * 8, 8, 2, WHITE);
    } else if (sc == ec) {
        // Vertical
        int8_t minR = min(sr, er);
        int8_t maxR = max(sr, er);
        arduboy.drawRoundRect(GRID_X0 + sc * 8, GRID_Y0 + minR * 8, 8, (maxR - minR + 1) * 8, 2, WHITE);
    } else {
        // Diagonal
        int8_t steps = max(abs(ec - sc), abs(er - sr));
        int8_t stepC = (ec > sc) ? 1 : -1;
        int8_t stepR = (er > sr) ? 1 : -1;
        for (int8_t s = 0; s <= steps; s++) {
            int16_t x = GRID_X0 + (sc + s * stepC) * 8;
            int16_t y = GRID_Y0 + (sr + s * stepR) * 8;
            arduboy.drawRoundRect(x, y, 8, 8, 2, WHITE);
        }
    }
}

void WordSearchGame::validateSelection() {
    int8_t dc = cursorCol - startCol;
    int8_t dr = cursorRow - startRow;
    bool collinear = (dc == 0 || dr == 0 || abs(dc) == abs(dr));

    if (!collinear || (dc == 0 && dr == 0)) {
        sound.tone(250, 60);
        isSelecting = false;
        return;
    }

    char candidate[24];
    uint8_t len = 0;
    int8_t steps = max(abs(dc), abs(dr));
    int8_t stepC = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);
    int8_t stepR = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);

    for (int8_t s = 0; s <= steps && len < 22; s++) {
        int8_t c = startCol + s * stepC;
        int8_t r = startRow + s * stepR;
        candidate[len++] = currentGrid[r][c];
    }
    candidate[len] = '\0';

    bool matched = false;
    for (uint8_t i = 0; i < activeWordCount; i++) {
        if (!activeWords[i].found) {
            if (strcasecmp(candidate, activeWords[i].text) == 0 ||
                isReverseMatch(candidate, activeWords[i].text)) {
                activeWords[i].found = true;
                activeWords[i].startC = startCol;
                activeWords[i].startR = startRow;
                activeWords[i].endC = cursorCol;
                activeWords[i].endR = cursorRow;

                foundCount++;
                score += 100 + len * 25;
                matched = true;

                // Sonido de acierto
                sound.tone(1320, 40);
                delay(45);
                sound.tone(1760, 80);

                // Verificar victoria de la sopa
                if (foundCount >= activeWordCount) {
                    state = SOPA_STATE_VICTORY;
                    victory = true;
                    vistaActual = 0;
                    score += 300;
                    saveHighScore();
                    // Fanfarria
                    sound.tone(1046, 50); delay(55);
                    sound.tone(1318, 50); delay(55);
                    sound.tone(1568, 50); delay(55);
                    sound.tone(2093, 100);
                }
                break;
            }
        }
    }

    if (!matched) {
        sound.tone(250, 60);
    }

    isSelecting = false;
}

bool WordSearchGame::isReverseMatch(const char* s1, const char* s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    if (len1 != len2) return false;
    for (size_t i = 0; i < len1; i++) {
        if (toupper(s1[i]) != toupper(s2[len1 - 1 - i])) return false;
    }
    return true;
}

void WordSearchGame::saveHighScore() {
    Preferences prefs;
    if (prefs.begin("sopa_stats", false)) {
        if (score > highScore) {
            highScore = score;
            prefs.putUInt("hi_score", highScore);
        }
        prefs.end();
    }
}

void WordSearchGame::loadHighScore() {
    Preferences prefs;
    if (prefs.begin("sopa_stats", true)) {
        highScore = prefs.getUInt("hi_score", 0);
        prefs.end();
    } else {
        highScore = 0;
    }
}
