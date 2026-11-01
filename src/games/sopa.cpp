#include "WordSearchGame.h"

// =============================================================================
// JUEGO: SOPA DE LETRAS DINÁMICA Y ALEATORIA (DICCIONARIO HANGMAN ESPAÑOL)
// Matriz de 11x6 con palabras de longitud variable (4 a 8 caracteres)
// Colocaciones en 8 direcciones: Horizontal, Vertical, Diagonal y Reversas
// =============================================================================

static const int8_t DIRECTIONS[8][2] = {
    {1, 0},   // Horizontal derecha
    {-1, 0},  // Horizontal izquierda (reversa)
    {0, 1},   // Vertical abajo
    {0, -1},  // Vertical arriba (reversa)
    {1, 1},   // Diagonal abajo-derecha
    {-1, -1}, // Diagonal arriba-izquierda (reversa)
    {1, -1},  // Diagonal arriba-derecha
    {-1, 1}   // Diagonal abajo-izquierda
};

static bool canPlaceWord(const char grid[6][14], const char* word, int8_t sc, int8_t sr, int8_t dc, int8_t dr) {
    uint8_t len = strlen(word);
    int8_t ec = sc + (len - 1) * dc;
    int8_t er = sr + (len - 1) * dr;
    if (ec < 0 || ec >= 11 || er < 0 || er >= 6) return false;
    for (uint8_t i = 0; i < len; i++) {
        char ch = grid[sr + i * dr][sc + i * dc];
        if (ch != '\0' && ch != word[i]) {
            return false;
        }
    }
    return true;
}

static void placeWord(char grid[6][14], const char* word, int8_t sc, int8_t sr, int8_t dc, int8_t dr) {
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
    randomSeed(analogRead(1) ^ millis() ^ esp_random());
    state = SOPA_STATE_PLAYING;
    cursorCol = 0;
    cursorRow = 0;
    isSelecting = false;
    foundCount = 0;
    victory = false;
    activeWordCount = 5;

    // Parámetros por nivel de dificultad según requisitos:
    // FÁCIL:
    //   - Direcciones: Solo Horizontal (->) y Vertical (v) [dx >= 0, dy >= 0]
    //   - Banco de palabras: Términos de 3 a 5 letras
    //   - Letras invertidas: Desactivadas
    // MEDIO:
    //   - Direcciones: Horizontal (->), Vertical (v) y Diagonal descendente (\)
    //   - Banco de palabras: Términos de 4 a 6 letras
    //   - Letras invertidas: Desactivadas
    // DIFÍCIL:
    //   - Direcciones: 8 direcciones posibles (permitiendo palabras en reversa)
    //   - Banco de palabras: Términos de 5 a 7 letras
    uint8_t minLen = 3;
    uint8_t maxLen = 5;
    uint8_t numDirs = 2;
    int8_t activeDirs[8][2];

    if (currentDifficulty == SOPA_DIFF_FACIL) {
        minLen = 3;
        maxLen = 5;
        numDirs = 2;
        activeDirs[0][0] = 1; activeDirs[0][1] = 0; // Horizontal ->
        activeDirs[1][0] = 0; activeDirs[1][1] = 1; // Vertical v
    } else if (currentDifficulty == SOPA_DIFF_MEDIO) {
        minLen = 4;
        maxLen = 6;
        numDirs = 3;
        activeDirs[0][0] = 1; activeDirs[0][1] = 0; // Horizontal ->
        activeDirs[1][0] = 0; activeDirs[1][1] = 1; // Vertical v
        activeDirs[2][0] = 1; activeDirs[2][1] = 1; // Diagonal descendente \ (dx=1, dy=1)
    } else { // SOPA_DIFF_DIFICIL
        minLen = 5;
        maxLen = 7;
        numDirs = 8;
        for (uint8_t d = 0; d < 8; d++) {
            activeDirs[d][0] = DIRECTIONS[d][0];
            activeDirs[d][1] = DIRECTIONS[d][1];
        }
    }

    char candidatePool[5][10];

    for (uint8_t masterAttempt = 0; masterAttempt < 30; masterAttempt++) {
        for (uint8_t wIdx = 0; wIdx < 5; wIdx++) {
            bool unique = false;
            for (uint16_t tr = 0; tr < 300; tr++) {
                uint16_t randIdx = random(WORD_COUNT);
                char temp[10];
                for (uint8_t c = 0; c < 9; c++) {
                    temp[c] = pgm_read_byte(&words_es[randIdx][c]);
                    if (temp[c] == '\0') break;
                }
                temp[9] = '\0';
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
                strcpy(candidatePool[wIdx], "GATO");
            }
        }

        // Ordenar palabras de mayor a menor longitud para maximizar colocación
        for (uint8_t i = 0; i < 4; i++) {
            for (uint8_t j = i + 1; j < 5; j++) {
                if (strlen(candidatePool[j]) > strlen(candidatePool[i])) {
                    char swapBuf[10];
                    strcpy(swapBuf, candidatePool[i]);
                    strcpy(candidatePool[i], candidatePool[j]);
                    strcpy(candidatePool[j], swapBuf);
                }
            }
        }

        char testGrid[6][14];
        for (uint8_t r = 0; r < 6; r++) {
            memset(testGrid[r], 0, sizeof(testGrid[r]));
        }

        struct Placement {
            int8_t sc, sr, dc, dr;
        } placements[5];

        bool allPlaced = true;

        for (uint8_t w = 0; w < 5; w++) {
            const char* word = candidatePool[w];

            // Pase 1: Contar opciones válidas
            uint16_t validCount = 0;
            for (int8_t sr = 0; sr < 6; sr++) {
                for (int8_t sc = 0; sc < 11; sc++) {
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
            for (int8_t sr = 0; sr < 6; sr++) {
                for (int8_t sc = 0; sc < 11; sc++) {
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
            for (uint8_t r = 0; r < 6; r++) {
                for (uint8_t c = 0; c < 11; c++) {
                    if (testGrid[r][c] != '\0') {
                        currentGrid[r][c] = testGrid[r][c];
                    } else {
                        currentGrid[r][c] = 'A' + random(26);
                    }
                }
                currentGrid[r][11] = '\0';
            }

            for (uint8_t w = 0; w < 5; w++) {
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

    // Navegación con cruceta por las 6 filas y 11 columnas
    if (arduboy.justPressed(UP_BUTTON)) {
        if (cursorRow > 0) {
            cursorRow--;
            sound.tone(880, 15);
        }
    }
    if (arduboy.justPressed(DOWN_BUTTON)) {
        if (cursorRow < 5) {
            cursorRow++;
            sound.tone(700, 15);
        }
    }
    if (arduboy.justPressed(LEFT_BUTTON)) {
        if (cursorCol > 0) {
            cursorCol--;
            sound.tone(800, 15);
        }
    }
    if (arduboy.justPressed(RIGHT_BUTTON)) {
        if (cursorCol < 10) {
            cursorCol++;
            sound.tone(750, 15);
        }
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

void WordSearchGame::draw() {
    if (state == SOPA_STATE_DIFFICULTY) {
        drawDifficultyScreen();
        return;
    }

    arduboy.clear();

    // -------------------------------------------------------------------------
    // 1. MARCO DOBLE RETRO EN LA CUADRÍCULA (1:1 FOTO)
    // -------------------------------------------------------------------------
    arduboy.drawRect(0, 0, 92, 53, WHITE);
    arduboy.drawRect(2, 2, 88, 49, WHITE);

    // -------------------------------------------------------------------------
    // 2. MATRIZ DE LETRAS CON ESPACIADO GRANDE Y SIMÉTRICO (3 PX DE SEPARACIÓN)
    // -------------------------------------------------------------------------
    // Celda de 8x8 px: x = 3 + c*8, y = 4 + r*8
    // Letra retro 5x5 px centrada: deja 3 píxeles limpios en todos los costados
    for (uint8_t r = 0; r < 6; r++) {
        for (uint8_t c = 0; c < 11; c++) {
            int16_t x = 3 + c * 8;
            int16_t y = 4 + r * 8;
            drawLetter5x5(x, y, currentGrid[r][c], WHITE);
        }
    }

    // -------------------------------------------------------------------------
    // 3. PALABRAS ENCONTRADAS: RECUADRO LIMPIO ALREDEDOR DE LA PALABRA
    // -------------------------------------------------------------------------
    for (uint8_t i = 0; i < activeWordCount; i++) {
        if (activeWords[i].found) {
            drawFoundBox(activeWords[i].startC, activeWords[i].startR,
                         activeWords[i].endC, activeWords[i].endR);
        }
    }

    // -------------------------------------------------------------------------
    // 4. RETÍCULA Y SELECCIÓN ACTIVA
    // -------------------------------------------------------------------------
    if (isSelecting) {
        // Píldora / nodo redondeado sólido en la letra inicial (1:1 foto)
        int16_t sx = 3 + startCol * 8;
        int16_t sy = 4 + startRow * 8;
        arduboy.fillRoundRect(sx - 1, sy - 1, 7, 7, 2, WHITE);
        drawLetter5x5(sx, sy, currentGrid[startRow][startCol], BLACK);

        // Bounding box en tiempo real del camino
        int8_t dc = cursorCol - startCol;
        int8_t dr = cursorRow - startRow;
        if (dr == 0) {
            int8_t minC = min(startCol, cursorCol);
            int8_t maxC = max(startCol, cursorCol);
            arduboy.drawRect((3 + minC * 8) - 1, (4 + startRow * 8) - 1, (maxC - minC) * 8 + 7, 7, WHITE);
        } else if (dc == 0) {
            int8_t minR = min(startRow, cursorRow);
            int8_t maxR = max(startRow, cursorRow);
            arduboy.drawRect((3 + startCol * 8) - 1, (4 + minR * 8) - 1, 7, (maxR - minR) * 8 + 7, WHITE);
        } else if (abs(dc) == abs(dr)) {
            int8_t steps = abs(dc);
            int8_t stepC = (dc > 0) ? 1 : -1;
            int8_t stepR = (dr > 0) ? 1 : -1;
            for (int8_t s = 0; s <= steps; s++) {
                int16_t cx = 3 + (startCol + s * stepC) * 8;
                int16_t cy = 4 + (startRow + s * stepR) * 8;
                arduboy.drawRect(cx - 1, cy - 1, 7, 7, WHITE);
            }
        }

        // Cursor de corchetes en la posición actual
        drawCornerBrackets(3 + cursorCol * 8, 4 + cursorRow * 8);
    } else {
        // Cursor de corchetes parpadeante en modo reposo
        if ((arduboy.frameCount % 24) < 16) {
            drawCornerBrackets(3 + cursorCol * 8, 4 + cursorRow * 8);
        }
    }

    // -------------------------------------------------------------------------
    // 5. PANEL LATERAL DERECHO (X: 94 a 127) - ESTÉTICA OPTIMIZADA Y ESPACIADA
    // -------------------------------------------------------------------------
    // Línea divisoria vertical entre cuadrícula y panel
    arduboy.drawFastVLine(94, 0, 50, WHITE);

    // Cabecera: SOPA y contador %d/%d integrados (se elimina el texto redundante "PALABRAS:")
    drawMiniText(96, 1, "SOPA");
    char countStr[8];
    snprintf(countStr, sizeof(countStr), "%d/%d", foundCount, activeWordCount);
    drawMiniText(114, 1, countStr);

    // Línea separadora horizontal bajo la cabecera
    arduboy.drawFastHLine(94, 7, 34, WHITE);

    // Lista de palabras con tachado continuo conectando a [X] (Y: 9, 16, 23, 30, 37)
    for (uint8_t i = 0; i < activeWordCount; i++) {
        int16_t wy = 9 + i * 7;
        drawMiniText(96, wy, activeWords[i].text);
        if (activeWords[i].found) {
            uint8_t wlen = strlen(activeWords[i].text);
            if (wlen <= 5) {
                arduboy.drawFastHLine(96, wy + 2, 21, WHITE);
                drawMiniText(118, wy, "[X]");
            } else {
                int16_t lineW = (wlen * 4) - 1;
                if (lineW > 31) lineW = 31;
                arduboy.drawFastHLine(96, wy + 2, lineW, WHITE);
            }
        }
    }

    // Puntuación P1: 0045 en Y: 44 (2 px libres de separación respecto a la palabra 5)
    char scoreStr[16];
    snprintf(scoreStr, sizeof(scoreStr), "P1: %04u", (unsigned int)score);
    drawMiniText(96, 44, scoreStr);

    // Ayuda de controles en la esquina inferior derecha (100% visible sin recorte)
    drawMiniText(96, 51, "A: Selec");
    drawMiniText(96, 57, "B: Salir");

    // -------------------------------------------------------------------------
    // 6. VENTANA MODAL DE VICTORIA
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
    int16_t x0 = cx - 2;
    int16_t y0 = cy - 2;
    int16_t x1 = cx + 6;
    int16_t y1 = cy + 6;
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
        arduboy.drawRect((3 + minC * 8) - 1, (4 + sr * 8) - 1, (maxC - minC) * 8 + 7, 7, WHITE);
    } else if (sc == ec) {
        // Vertical
        int8_t minR = min(sr, er);
        int8_t maxR = max(sr, er);
        arduboy.drawRect((3 + sc * 8) - 1, (4 + minR * 8) - 1, 7, (maxR - minR) * 8 + 7, WHITE);
    } else {
        // Diagonal
        int8_t steps = max(abs(ec - sc), abs(er - sr));
        int8_t stepC = (ec > sc) ? 1 : -1;
        int8_t stepR = (er > sr) ? 1 : -1;
        for (int8_t s = 0; s <= steps; s++) {
            int16_t x = 3 + (sc + s * stepC) * 8;
            int16_t y = 4 + (sr + s * stepR) * 8;
            arduboy.drawRect(x - 1, y - 1, 7, 7, WHITE);
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

    char candidate[16];
    uint8_t len = 0;
    int8_t steps = max(abs(dc), abs(dr));
    int8_t stepC = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);
    int8_t stepR = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);

    for (int8_t s = 0; s <= steps && len < 15; s++) {
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

                // Verificar victoria del tema
                if (foundCount >= activeWordCount) {
                    state = SOPA_STATE_VICTORY;
                    victory = true;
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
