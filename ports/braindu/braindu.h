#pragma once
#include "console_core.h"
#include "braindu_bitmaps.h"

// =============================================================================
// =============================================================================
// GAME 12: BRAINDU-TRAIN (Native Arduboy 1.1 Port)
// =============================================================================
enum BrainduState {
    BT_STATE_TITLE = 0,
    BT_STATE_MENU,
    BT_STATE_ACTIVITY,
    BT_STATE_RESULTS
};

struct BrainduTrainGame {
    BrainduState state;
    uint8_t select;          // Selected exercise 0..8
    uint8_t menuRow;         // 0..2 (strictly orthogonal row)
    uint8_t menuCol;         // 0..2 (strictly orthogonal column)
    uint32_t lastNavTime;    // 150ms debounce for one-shot cell movement
    uint8_t timelife;        // Time bar (max 86, decreases at 8 FPS)
    uint8_t currentScore;    // Score 0..99
    uint8_t highScore;       // High score stored in EEPROM addr 26
    int16_t countdown;       // 3-2-1 countdown frames (23..0)
    
    // Timing & pacing
    uint32_t lastFrameTick;
    uint32_t stateTimer;
    
    // Mini-game 0: Math
    int16_t mathA, mathB, mathAns, mathOpt1, mathOpt2;
    char mathOp;
    bool mathOpt1Correct;
    
    // Mini-game 1 & 6: Arrows
    uint8_t arrowDir;        // 0:UP, 1:DOWN, 2:LEFT, 3:RIGHT
    bool arrowInverted;
    
    // Mini-game 2: Mayor o Menor
    int16_t hlCurrent, hlNext;
    
    // Mini-game 3: Odd symbol
    uint8_t oddPos;          // 0:UP, 1:DOWN, 2:LEFT, 3:RIGHT
    uint8_t oddType;
    
    // Mini-game 4: Fast reflex
    uint32_t reflexTriggerTime;
    bool reflexArmed;
    bool reflexFired;
    
    // Mini-game 5: Count dots (Flash Dots 800ms)
    uint8_t dotCount;
    uint8_t countOpt1, countOpt2;
    bool countOpt1Correct;
    struct DotPoint { int8_t x, y; } dots[10];
    uint32_t dotShowStartTime;
    
    // Mini-game 7: Simon Sequence
    uint8_t seqLength;
    uint8_t seqPattern[5];
    uint8_t seqStep;
    uint8_t seqPlayerIdx;
    bool seqPlaying;
    uint32_t seqNextTime;
    
    // Mini-game 8: Mix (Brain Sprint)
    uint8_t activeMixType;

    const unsigned char* getTileBmp(uint8_t idx) {
        switch (idx) {
            case 0: return menu_tile_0;
            case 1: return menu_tile_1;
            case 2: return menu_tile_2;
            case 3: return menu_tile_3;
            case 4: return menu_tile_4;
            case 5: return menu_tile_5;
            case 6: return menu_tile_6;
            case 7: return menu_tile_7;
            case 8: return menu_tile_8;
            default: return menu_tile_0;
        }
    }

    void init() {
        melodyPlayer.stop();
        state = BT_STATE_TITLE;
        menuRow = 0;
        menuCol = 0;
        select = 0;
        lastNavTime = 0;
        currentScore = 0;
        highScore = 0;
        EEPROM.get(26, highScore);
        if (highScore == 0xFF) highScore = 0;

        pinMode(7, INPUT_PULLUP);
        pinMode(18, INPUT_PULLUP);
        pinMode(14, INPUT_PULLUP);
        pinMode(0, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_BOOT, INPUT_PULLUP);
        
        stateTimer = millis();
        // Boot sound jingle (original Arduboy-BrainduTrain)
        sound.tone(987, 120);
        delay(128);
        sound.tone(1318, 250);
    }

    void onBootShortPress() {
        if (state == BT_STATE_TITLE) {
            sound.tone(1760, 40);
            state = BT_STATE_MENU;
            menuRow = 0;
            menuCol = 0;
            select = 0;
            lastNavTime = millis();
        } else if (state == BT_STATE_MENU) {
            sound.tone(1760, 40);
            startExercise(select);
        } else if (state == BT_STATE_RESULTS) {
            sound.tone(1760, 40);
            state = BT_STATE_MENU;
            menuRow = 0;
            menuCol = 0;
            select = 0;
            lastNavTime = millis();
        }
    }

    void onBootHold1s() {
        if (state == BT_STATE_ACTIVITY) {
            sound.tone(600, 40);
            state = BT_STATE_MENU;
            menuRow = 0;
            menuCol = 0;
            select = 0;
            lastNavTime = millis();
        }
    }

    void startExercise(uint8_t idx) {
        select = idx;
        state = BT_STATE_ACTIVITY;
        countdown = 23;      // (3 * 8) - 1 = 23 frames
        timelife = 86;       // _TIMELIFE = 86
        currentScore = 0;
        lastFrameTick = millis();
        initExercise(idx);
    }

    void initExercise(uint8_t idx) {
        if (idx == 0) generateMath();
        else if (idx == 1) generateArrow(false);
        else if (idx == 2) generateHighLow();
        else if (idx == 3) generateOdd();
        else if (idx == 4) generateReflex();
        else if (idx == 5) generateCount();
        else if (idx == 6) generateArrow(true);
        else if (idx == 7) generateSeq();
        else if (idx == 8) {
            activeMixType = random(0, 8);
            initExercise(activeMixType);
        }
    }

    void generateMath() {
        mathA = random(4, 25);
        mathB = random(2, 15);
        bool isAdd = random(0, 2) == 0;
        mathOp = isAdd ? '+' : '-';
        mathAns = isAdd ? (mathA + mathB) : (mathA - mathB);
        int8_t diff = (random(0, 2) == 0 ? 1 : -1) * random(1, 4);
        int16_t wrong = mathAns + diff;
        mathOpt1Correct = random(0, 2) == 0;
        mathOpt1 = mathOpt1Correct ? mathAns : wrong;
        mathOpt2 = mathOpt1Correct ? wrong : mathAns;
    }

    void generateArrow(bool inverted) {
        arrowDir = random(0, 4); // 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT
        arrowInverted = inverted;
    }

    void generateHighLow() {
        hlCurrent = random(20, 80);
        do {
            hlNext = random(10, 90);
        } while (hlNext == hlCurrent);
    }

    void generateOdd() {
        oddPos = random(0, 4);
        oddType = random(0, 3);
    }

    void generateReflex() {
        reflexArmed = true;
        reflexFired = false;
        reflexTriggerTime = millis() + random(1000, 2400);
    }

    void generateCount() {
        dotCount = random(3, 8);
        for (uint8_t i = 0; i < dotCount; i++) {
            dots[i].x = random(10, 92);
            dots[i].y = random(14, 46);
        }
        countOpt1Correct = random(0, 2) == 0;
        int8_t diff = (random(0, 2) == 0 ? 1 : -1) * random(1, 3);
        int8_t wrong = dotCount + diff;
        if (wrong < 1) wrong = dotCount + 2;
        countOpt1 = countOpt1Correct ? dotCount : wrong;
        countOpt2 = countOpt1Correct ? wrong : dotCount;
        dotShowStartTime = millis();
    }

    void generateSeq() {
        seqLength = 3;
        for (uint8_t i = 0; i < seqLength; i++) {
            seqPattern[i] = random(0, 4);
        }
        seqPlaying = true;
        seqStep = 0;
        seqPlayerIdx = 0;
        seqNextTime = millis() + 350;
    }

    void handleCorrect() {
        sound.tone(2200, 30);
        if (currentScore < 99) currentScore += 10;
        if (select == 8) {
            activeMixType = random(0, 8);
            initExercise(activeMixType);
        } else {
            initExercise(select);
        }
    }

    void handleWrong() {
        sound.tone(250, 60);
        if (select == 8) {
            activeMixType = random(0, 8);
            initExercise(activeMixType);
        } else {
            initExercise(select);
        }
    }

    void drawBigArrow(int16_t cx, int16_t cy, uint8_t dir) {
        if (dir == 0) { // UP
            arduboy.fillTriangle(cx, cy - 10, cx - 8, cy - 1, cx + 8, cy - 1, WHITE);
            arduboy.fillRect(cx - 3, cy - 1, 7, 10, WHITE);
        } else if (dir == 1) { // DOWN
            arduboy.fillTriangle(cx, cy + 10, cx - 8, cy + 1, cx + 8, cy + 1, WHITE);
            arduboy.fillRect(cx - 3, cy - 9, 7, 10, WHITE);
        } else if (dir == 2) { // LEFT
            arduboy.fillTriangle(cx - 10, cy, cx - 1, cy - 8, cx - 1, cy + 8, WHITE);
            arduboy.fillRect(cx - 1, cy - 3, 10, 7, WHITE);
        } else if (dir == 3) { // RIGHT
            arduboy.fillTriangle(cx + 10, cy, cx + 1, cy - 8, cx + 1, cy + 8, WHITE);
            arduboy.fillRect(cx - 9, cy - 3, 10, 7, WHITE);
        }
    }

    void update() {
        yield();

        if (state == BT_STATE_TITLE) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
                arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON) ||
                arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
                sound.tone(1760, 40);
                state = BT_STATE_MENU;
                menuRow = 0;
                menuCol = 0;
                select = 0;
                lastNavTime = millis();
            }
            return;
        }

        if (state == BT_STATE_MENU) {
            // Strictly orthogonal, one-shot movement (150ms debounce lockout)
            if (millis() - lastNavTime >= 150) {
                if (arduboy.justPressed(UP_BUTTON)) {
                    if (menuRow > 0) menuRow--;
                    else menuRow = 2;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
                } else if (arduboy.justPressed(DOWN_BUTTON)) {
                    if (menuRow < 2) menuRow++;
                    else menuRow = 0;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
                } else if (arduboy.justPressed(LEFT_BUTTON)) {
                    if (menuCol > 0) menuCol--;
                    else menuCol = 2;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
                } else if (arduboy.justPressed(RIGHT_BUTTON)) {
                    if (menuCol < 2) menuCol++;
                    else menuCol = 0;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
                } else if (arduboy.justPressed(A_BUTTON)) {
                    sound.tone(1760, 40);
                    startExercise(select);
                }
            }
            return;
        }

        if (state == BT_STATE_ACTIVITY) {
            // Frame rate pacing: 125ms = 8 FPS
            if (millis() - lastFrameTick >= 125) {
                lastFrameTick = millis();
                if (countdown > 0) {
                    if (countdown == 23 || countdown == 15 || countdown == 7) {
                        sound.tone(5000, 20);
                    }
                    countdown--;
                } else {
                    if (timelife > 0) {
                        timelife--;
                        if (timelife == 0) {
                            // Test complete!
                            triggerRgbLed(0, 255, 0, 1000);
                            if (currentScore > highScore) {
                                highScore = currentScore;
                                EEPROM.put(26, highScore);
                            }
                            state = BT_STATE_RESULTS;
                            sound.tone(1046, 80); delay(80);
                            sound.tone(1318, 80); delay(80);
                            sound.tone(1568, 80); delay(80);
                            sound.tone(2093, 200);
                            return;
                        }
                    }
                }
            }

            if (countdown > 0) return; // Wait for countdown before accepting game inputs

            uint8_t curAct = (select == 8) ? activeMixType : select;

            if (curAct == 0) { // Math
                if (arduboy.justPressed(LEFT_BUTTON)) { // Opción izquierda
                    if (mathOpt1Correct) handleCorrect();
                    else handleWrong();
                } else if (arduboy.justPressed(RIGHT_BUTTON)) { // Opción derecha
                    if (!mathOpt1Correct) handleCorrect();
                    else handleWrong();
                }
            } else if (curAct == 1 || curAct == 6) { // Arrow match
                int8_t pressedDir = -1;
                if (arduboy.justPressed(UP_BUTTON)) pressedDir = 0;
                else if (arduboy.justPressed(DOWN_BUTTON)) pressedDir = 1;
                else if (arduboy.justPressed(LEFT_BUTTON)) pressedDir = 2;
                else if (arduboy.justPressed(RIGHT_BUTTON)) pressedDir = 3;

                if (pressedDir >= 0) {
                    uint8_t target = arrowDir;
                    if (arrowInverted) {
                        if (arrowDir == 0) target = 1;
                        else if (arrowDir == 1) target = 0;
                        else if (arrowDir == 2) target = 3;
                        else if (arrowDir == 3) target = 2;
                    }
                    if (pressedDir == target) handleCorrect();
                    else handleWrong();
                }
            } else if (curAct == 2) { // Mayor / Menor
                if (arduboy.justPressed(UP_BUTTON)) { // Mayor
                    if (hlNext > hlCurrent) {
                        hlCurrent = hlNext;
                        handleCorrect();
                    } else handleWrong();
                } else if (arduboy.justPressed(DOWN_BUTTON)) { // Menor
                    if (hlNext < hlCurrent) {
                        hlCurrent = hlNext;
                        handleCorrect();
                    } else handleWrong();
                }
            } else if (curAct == 3) { // Odd shape out
                int8_t pressedPos = -1;
                if (arduboy.justPressed(UP_BUTTON)) pressedPos = 0;
                else if (arduboy.justPressed(DOWN_BUTTON)) pressedPos = 1;
                else if (arduboy.justPressed(LEFT_BUTTON)) pressedPos = 2;
                else if (arduboy.justPressed(RIGHT_BUTTON)) pressedPos = 3;

                if (pressedPos >= 0) {
                    if (pressedPos == oddPos) handleCorrect();
                    else handleWrong();
                }
            } else if (curAct == 4) { // Fast reflex
                if (reflexArmed && !reflexFired && millis() >= reflexTriggerTime) {
                    reflexFired = true;
                    sound.tone(2800, 20);
                }
                if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
                    arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON) ||
                    arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
                    if (!reflexFired) {
                        // Too early!
                        handleWrong();
                    } else {
                        uint32_t delta = millis() - reflexTriggerTime;
                        if (delta < 300) { if (currentScore < 99) currentScore += 15; }
                        else { if (currentScore < 99) currentScore += 10; }
                        handleCorrect();
                    }
                }
            } else if (curAct == 5) { // Count dots (Flash Dots 800ms)
                if (millis() - dotShowStartTime >= 800) {
                    if (arduboy.justPressed(LEFT_BUTTON)) {
                        if (countOpt1Correct) handleCorrect();
                        else handleWrong();
                    } else if (arduboy.justPressed(RIGHT_BUTTON)) {
                        if (!countOpt1Correct) handleCorrect();
                        else handleWrong();
                    }
                }
            } else if (curAct == 7) { // Simon Sequence
                if (seqPlaying) {
                    if (millis() >= seqNextTime) {
                        seqStep++;
                        if (seqStep >= seqLength) {
                            seqPlaying = false;
                        } else {
                            uint16_t freqs[4] = {880, 1175, 1397, 1760};
                            sound.tone(freqs[seqPattern[seqStep]], 40);
                            seqNextTime = millis() + 450;
                        }
                    }
                } else {
                    int8_t pressedDir = -1;
                    if (arduboy.justPressed(UP_BUTTON)) pressedDir = 0;
                    else if (arduboy.justPressed(DOWN_BUTTON)) pressedDir = 1;
                    else if (arduboy.justPressed(LEFT_BUTTON)) pressedDir = 2;
                    else if (arduboy.justPressed(RIGHT_BUTTON)) pressedDir = 3;

                    if (pressedDir >= 0) {
                        if (pressedDir == seqPattern[seqPlayerIdx]) {
                            uint16_t freqs[4] = {880, 1175, 1397, 1760};
                            sound.tone(freqs[pressedDir], 30);
                            seqPlayerIdx++;
                            if (seqPlayerIdx >= seqLength) {
                                handleCorrect();
                            }
                        } else {
                            handleWrong();
                        }
                    }
                }
            }
            return;
        }

        if (state == BT_STATE_RESULTS) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
                arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON) ||
                arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
                sound.tone(1760, 40);
                state = BT_STATE_MENU;
                menuRow = 0;
                menuCol = 0;
                select = 0;
                lastNavTime = millis();
            }
            return;
        }
    }

    void draw() {
        if (state == BT_STATE_TITLE) {
            arduboy.setCursor(5, 2);
            arduboy.setTextSize(2);
            arduboy.print("Braindu-");
            arduboy.setCursor(60, 25);
            arduboy.print("Train");
            arduboy.setTextSize(1);
            arduboy.drawArduboyBitmap(5, 25, title_brain, 32, 24, WHITE);
            if ((millis() / 400) % 2 == 0) {
                arduboy.setCursor(2, 55);
                arduboy.print("PRESS A or B to BEGIN");
            }
            return;
        }

        if (state == BT_STATE_MENU) {
            // Draw 3x3 menu grid tiles
            arduboy.drawArduboyBitmap(20, 5, menu_tile_0, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(55, 5, menu_tile_1, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(90, 5, menu_tile_2, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(20, 25, menu_tile_3, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(55, 25, menu_tile_4, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(90, 25, menu_tile_5, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(20, 45, menu_tile_6, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(55, 45, menu_tile_7, 17, 16, WHITE);
            arduboy.drawArduboyBitmap(90, 45, menu_tile_8, 17, 16, WHITE);

            // Left & right selection arrows (Strict orthogonal row/col placement)
            uint8_t selX = 14 + (35 * menuCol);
            uint8_t selY = 9 + (20 * menuRow);
            arduboy.drawArduboyBitmap(selX, selY, LH_arrow, 4, 7, WHITE);
            arduboy.drawArduboyBitmap(selX + 25, selY, RH_arrow, 4, 7, WHITE);

            // High Score badge
            if (highScore > 0) {
                arduboy.setCursor(2, 57);
                arduboy.print("HI:");
                arduboy.print(highScore);
            }
            return;
        }

        if (state == BT_STATE_ACTIVITY) {
            // Split layout frame
            arduboy.drawRect(0, 0, 105, 64, WHITE);
            arduboy.drawRect(107, 0, 21, 64, WHITE);

            // Side icon
            arduboy.drawArduboyBitmap(109, 46, getTileBmp(select), 17, 16, WHITE);

            // Side time bar
            uint8_t tall = timelife >> 1;
            uint8_t ystart = 2 + (43 - tall);
            if (tall > 0) arduboy.fillRect(109, ystart, 8, tall, WHITE);

            // Side 3-digit score
            uint8_t sc[3] = {0, 0, 0};
            uint8_t dispScore = (currentScore > 99) ? 99 : currentScore;
            if (currentScore >= 100) sc[0] = 1;
            sc[1] = (dispScore / 10) % 10;
            sc[2] = dispScore % 10;
            arduboy.setTextSize(1);
            arduboy.setCursor(120, 13); arduboy.print(sc[0]);
            arduboy.setCursor(120, 21); arduboy.print(sc[1]);
            arduboy.setCursor(120, 29); arduboy.print(sc[2]);

            // Main box content
            if (countdown > 0) {
                arduboy.setCursor(45, 17);
                arduboy.setTextSize(4);
                arduboy.print((countdown / 8) + 1);
                arduboy.setTextSize(1);
                return;
            }

            uint8_t curAct = (select == 8) ? activeMixType : select;

            if (curAct == 0) { // Math
                arduboy.setCursor(12, 10); arduboy.print("CALCULO RAPIDO");
                arduboy.setCursor(22, 26); arduboy.setTextSize(2);
                arduboy.print(mathA); arduboy.print(mathOp); arduboy.print(mathB); arduboy.print("=?");
                arduboy.setTextSize(1);
                arduboy.setCursor(8, 48); arduboy.print("B3:["); arduboy.print(mathOpt1); arduboy.print("]");
                arduboy.setCursor(56, 48); arduboy.print("B4:["); arduboy.print(mathOpt2); arduboy.print("]");
            } else if (curAct == 1) { // Arrow reflex
                arduboy.setCursor(14, 8); arduboy.print("¡PULSA FLECHA!");
                drawBigArrow(52, 34, arrowDir);
            } else if (curAct == 2) { // Mayor / Menor
                arduboy.setCursor(14, 8); arduboy.print("MAYOR O MENOR");
                arduboy.setCursor(18, 22); arduboy.print("NUMERO: ");
                arduboy.setTextSize(2); arduboy.print(hlCurrent); arduboy.setTextSize(1);
                arduboy.setCursor(8, 42); arduboy.print("B1: MAYOR (^)");
                arduboy.setCursor(8, 52); arduboy.print("B2: MENOR (v)");
            } else if (curAct == 3) { // Odd shape
                arduboy.setCursor(8, 6); arduboy.print("BUSCA DIFERENTE");
                // Diamond positions: 0:UP(52, 20), 1:DOWN(52, 48), 2:LEFT(26, 34), 3:RIGHT(78, 34)
                int16_t px[4] = {52, 52, 26, 78};
                int16_t py[4] = {20, 48, 34, 34};
                for (uint8_t p = 0; p < 4; p++) {
                    bool isOdd = (p == oddPos);
                    if (isOdd) {
                        arduboy.drawCircle(px[p], py[p], 5, WHITE);
                        arduboy.drawPixel(px[p], py[p], WHITE);
                    } else {
                        arduboy.drawRect(px[p] - 4, py[p] - 4, 9, 9, WHITE);
                    }
                }
            } else if (curAct == 4) { // Fast reflex
                arduboy.setCursor(14, 10); arduboy.print("TEST REFLEJOS");
                if (!reflexFired) {
                    arduboy.setCursor(24, 30); arduboy.print("¡ATENTO...!");
                } else {
                    arduboy.fillRect(16, 26, 72, 18, WHITE);
                    arduboy.setTextColor(BLACK);
                    arduboy.setCursor(22, 31); arduboy.setTextSize(2);
                    arduboy.print("¡PULSA!");
                    arduboy.setTextSize(1);
                    arduboy.setTextColor(WHITE);
                }
            } else if (curAct == 5) { // Count dots (Flash Dots: visible for strictly 800ms)
                if (millis() - dotShowStartTime < 800) {
                    arduboy.setCursor(14, 6); arduboy.print("¡CUENTA YA!");
                    for (uint8_t i = 0; i < dotCount; i++) {
                        arduboy.fillRect(dots[i].x, dots[i].y, 3, 3, WHITE);
                    }
                } else {
                    arduboy.setCursor(10, 20); arduboy.print("¿CUANTOS HABIA?");
                    arduboy.setCursor(8, 46); arduboy.print("B3:[ "); arduboy.print(countOpt1); arduboy.print(" ]");
                    arduboy.setCursor(56, 46); arduboy.print("B4:[ "); arduboy.print(countOpt2); arduboy.print(" ]");
                }
            } else if (curAct == 6) { // Inverted arrow
                if (!arrowInverted) {
                    arduboy.fillRect(24, 6, 56, 11, WHITE);
                    arduboy.setTextColor(BLACK);
                    arduboy.setCursor(30, 8); arduboy.print("NORMAL");
                    arduboy.setTextColor(WHITE);
                } else {
                    arduboy.fillRect(14, 6, 76, 11, WHITE);
                    arduboy.setTextColor(BLACK);
                    arduboy.setCursor(18, 8); arduboy.print("¡INVERTIDO!");
                    arduboy.setTextColor(WHITE);
                }
                drawBigArrow(52, 36, arrowDir);
            } else if (curAct == 7) { // Simon Sequence
                if (seqPlaying) {
                    arduboy.setCursor(18, 8); arduboy.print("MEMORIZA:");
                    drawBigArrow(52, 34, seqPattern[seqStep]);
                } else {
                    arduboy.setCursor(22, 8); arduboy.print("¡TU TURNO!");
                    arduboy.setCursor(20, 26); arduboy.print("PASO ");
                    arduboy.print(seqPlayerIdx + 1);
                    arduboy.print("/");
                    arduboy.print(seqLength);
                    arduboy.setCursor(12, 46); arduboy.print("USA: B1/B2/B3/B4");
                }
            }
            return;
        }

        if (state == BT_STATE_RESULTS) {
            arduboy.drawRect(4, 4, 120, 56, WHITE);
            arduboy.drawRect(6, 6, 116, 52, WHITE);
            arduboy.fillRect(10, 9, 108, 11, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(16, 11); arduboy.print("TEST FINALIZADO");
            arduboy.setTextColor(WHITE);

            arduboy.setCursor(14, 24); arduboy.print("PUNTOS: "); arduboy.print(currentScore);
            arduboy.setCursor(14, 34); arduboy.print("RECORD: "); arduboy.print(highScore);

            arduboy.setCursor(14, 46);
            if (currentScore >= 60) arduboy.print("¡CEREBRO GENIAL!");
            else if (currentScore >= 40) arduboy.print("¡NIVEL EXPERTO!");
            else if (currentScore >= 20) arduboy.print("¡BUEN TRABAJO!");
            else arduboy.print("¡A PRACTICAR!");
            return;
        }
    }
};
