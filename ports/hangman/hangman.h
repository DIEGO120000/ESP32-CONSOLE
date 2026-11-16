#pragma once
#include "console_core.h"
#include "hangman_words.h"

// =============================================================================
// HANGMAN! (ARDUBOY PORT BY SERISMAN)
// =============================================================================
#define HANGMAN_MODE_TITLE    0
#define HANGMAN_MODE_STATS    1
#define HANGMAN_MODE_PLAY     2
#define HANGMAN_MODE_CORRECT  4
#define HANGMAN_MODE_DEAD     5

struct HangmanGame {
    uint8_t mode;
    bool paused;
    uint32_t wins;
    uint32_t losses;
    uint8_t soundEnabled;
    uint8_t hangman;
    static const uint8_t HISTORY_CAPACITY = 20;
    uint16_t recentWordHistory[HISTORY_CAPACITY];
    uint8_t historyHead;
    uint8_t historyCount;
    char currentWord[12];
    uint8_t cursor;
    uint8_t cursorBlink;
    unsigned long lastBlinkTime;
    unsigned long lastTitleAnimTime;
    uint8_t usedLetters[26];
    char buf[32];
    bool bootActionTriggered;

    void init() {
        loadStats();
        mode = HANGMAN_MODE_TITLE;
        cursor = 0;
        paused = false;
        hangman = 0;
        bootActionTriggered = false;
        cursorBlink = 1;
        lastBlinkTime = millis();
        lastTitleAnimTime = millis();
        historyHead = 0;
        historyCount = 0;
        memset(recentWordHistory, 0xFF, sizeof(recentWordHistory));
        randomSeed(analogRead(0) ^ millis() ^ esp_random());
        if (soundEnabled) {
            sound.tone(880, 40);
        }
    }

    void onBootShortPress() {
        bootActionTriggered = true;
    }

    void loadStats() {
        Preferences prefs;
        prefs.begin("hangman_nvs", true);
        wins = prefs.getUInt("wins", 0);
        losses = prefs.getUInt("losses", 0);
        soundEnabled = prefs.getUChar("sound", 1);
        prefs.end();
        if (soundEnabled > 1) soundEnabled = 1;
    }

    void saveStats() {
        Preferences prefs;
        prefs.begin("hangman_nvs", false);
        prefs.putUInt("wins", wins);
        prefs.putUInt("losses", losses);
        prefs.putUChar("sound", soundEnabled);
        prefs.end();
    }

    void resetStats() {
        wins = 0;
        losses = 0;
        Preferences prefs;
        prefs.begin("hangman_nvs", false);
        prefs.putUInt("wins", 0);
        prefs.putUInt("losses", 0);
        prefs.putUChar("sound", soundEnabled);
        prefs.end();
    }

    void playTone(uint16_t freq, uint16_t dur) {
        if (soundEnabled) {
            sound.tone(freq, dur);
        }
    }

    void toggleSound() {
        if (soundEnabled) {
            sound.tone(220, 100);
            delay(120);
            soundEnabled = 0;
        } else {
            soundEnabled = 1;
            sound.tone(1200, 60);
            delay(80);
        }
        Preferences prefs;
        prefs.begin("hangman_nvs", false);
        prefs.putUChar("sound", soundEnabled);
        prefs.end();
    }

    void startPlaying() {
        if (!paused) {
            pickAWord();
            hangman = 0;
            memset(usedLetters, 0, sizeof(usedLetters));
        }
        paused = false;
        cursor = 0;
        mode = HANGMAN_MODE_PLAY;
    }

    void pickAWord() {
        randomSeed(analogRead(0) ^ millis() ^ esp_random());
        uint16_t newWordIndex = 0;
        uint8_t attempts = 0;
        bool isRecent = false;

        do {
            newWordIndex = random(WORD_COUNT);
            isRecent = false;
            for (uint8_t i = 0; i < historyCount; i++) {
                if (recentWordHistory[i] == newWordIndex) {
                    isRecent = true;
                    break;
                }
            }
            attempts++;
        } while (isRecent && attempts < 100);

        // Registrar en buffer circular de exclusión histórica
        recentWordHistory[historyHead] = newWordIndex;
        historyHead = (historyHead + 1) % HISTORY_CAPACITY;
        if (historyCount < HISTORY_CAPACITY) {
            historyCount++;
        }

        // Cargar palabra en español desde Flash (PROGMEM)
        memset(currentWord, 0, sizeof(currentWord));
        for (uint8_t i = 0; i < 9; i++) {
            currentWord[i] = pgm_read_byte(&words_es[newWordIndex][i]);
            if (currentWord[i] == '\0') break;
        }
        currentWord[sizeof(currentWord) - 1] = '\0';
    }

    void scoreResponse(char letter) {
        bool allDone = true;
        bool letterOk = false;
        uint8_t wordLen = strlen(currentWord);

        for (uint8_t chr = 0; chr < wordLen; chr++) {
            char wordLetter = currentWord[chr];
            if (usedLetters[wordLetter - 'A'] == 0) {
                allDone = false;
            }
            if (wordLetter == letter) {
                letterOk = true;
            }
        }

        if (allDone) {
            mode = HANGMAN_MODE_CORRECT;
            wins++;
            Preferences prefs;
            prefs.begin("hangman_nvs", false);
            prefs.putUInt("wins", wins);
            prefs.end();
            triggerRgbLed(0, 255, 0, 1000); // LED Verde 1s
            if (soundEnabled) {
                melodyPlayer.play(hangmanVictoryNotes, 3);
            }
        } else if (letterOk) {
            playTone(1200, 60); // Beep agudo corto
        } else {
            hangman++;
            if (hangman >= 6) {
                hangman = 6;
                mode = HANGMAN_MODE_DEAD;
                losses++;
                Preferences prefs;
                prefs.begin("hangman_nvs", false);
                prefs.putUInt("losses", losses);
                prefs.end();
                triggerRgbLed(255, 0, 0, 1000); // LED Rojo 1s
                if (soundEnabled) {
                    melodyPlayer.play(hangmanDefeatNotes, 4);
                }
            } else {
                playTone(200, 120); // Tono grave
            }
        }
    }

    void update() {
        yield();

        // Blink timer para el cursor
        if (millis() - lastBlinkTime >= 180) {
            cursorBlink = 1 - cursorBlink;
            lastBlinkTime = millis();
        }

        switch (mode) {
            case HANGMAN_MODE_TITLE:
                updateTitle();
                break;
            case HANGMAN_MODE_STATS:
                updateStats();
                break;
            case HANGMAN_MODE_PLAY:
                updatePlay();
                break;
            case HANGMAN_MODE_CORRECT:
            case HANGMAN_MODE_DEAD:
                updateGameOver();
                break;
        }

        bootActionTriggered = false; // Reset one-shot
    }

    void updateTitle() {
        if (!paused && (millis() - lastTitleAnimTime >= 350)) {
            lastTitleAnimTime = millis();
            if (hangman < 6) hangman++;
            else hangman = 0;
        }

        // BTN1 (GPIO 14 - UP): cursor arriba
        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
            if (cursor > 0) {
                cursor--;
                playTone(880, 15);
            }
        }
        // BTN2 (GPIO 0 - DOWN): cursor abajo
        if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
            if (cursor < 2) {
                cursor++;
                playTone(700, 15);
            }
        }

        // Confirmación con botón BOOT (GPIO 9) o BTN3 (GPIO 7 / A_BUTTON)
        if (bootActionTriggered || arduboy.justPressed(A_BUTTON)) {
            if (cursor == 0) {
                playTone(1400, 30);
                startPlaying();
            } else if (cursor == 1) {
                toggleSound();
            } else if (cursor == 2) {
                playTone(1000, 25);
                mode = HANGMAN_MODE_STATS;
                cursor = 1; // Default en "Back"
            }
        }
    }

    void updateStats() {
        if (!paused && (millis() - lastTitleAnimTime >= 350)) {
            lastTitleAnimTime = millis();
            if (hangman < 6) hangman++;
            else hangman = 0;
        }

        // BTN1 (GPIO 14 - UP) / BTN2 (GPIO 0 - DOWN)
        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
            if (cursor > 0) {
                cursor--;
                playTone(880, 15);
            }
        }
        if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
            if (cursor < 1) {
                cursor++;
                playTone(700, 15);
            }
        }

        // Confirmar
        if (bootActionTriggered || arduboy.justPressed(A_BUTTON)) {
            if (cursor == 0) {
                resetStats();
                playTone(400, 60);
            } else if (cursor == 1) {
                playTone(900, 25);
                mode = HANGMAN_MODE_TITLE;
                cursor = 2; // Apuntar a Stats
            }
        }

        if (arduboy.justPressed(B_BUTTON)) {
            playTone(900, 25);
            mode = HANGMAN_MODE_TITLE;
            cursor = 2;
        }
    }

    void updatePlay() {
        // Mapeo Físico de Botones (INPUT_PULLUP):
        // Botón 1 (GPIO 14): Mover cursor ARRIBA en el abecedario
        if (arduboy.justPressed(UP_BUTTON)) {
            if (cursor >= 9) {
                cursor -= 9;
                playTone(900, 12);
            }
        }
        // Botón 2 (GPIO 0): Mover cursor ABAJO en el abecedario
        if (arduboy.justPressed(DOWN_BUTTON)) {
            if (cursor < 17) {
                cursor += 9;
                if (cursor > 25) cursor = 25;
                playTone(900, 12);
            }
        }
        // Mover cursor IZQUIERDA
        if (arduboy.justPressed(LEFT_BUTTON)) {
            if (cursor == 0) cursor = 25;
            else cursor--;
            playTone(900, 12);
        }
        // Mover cursor DERECHA
        if (arduboy.justPressed(RIGHT_BUTTON)) {
            if (cursor == 25) cursor = 0;
            else cursor++;
            playTone(900, 12);
        }

        // Confirmación de Letra: A_BUTTON (GPIO 20) o Botón BOOT (GPIO 9)
        if (bootActionTriggered || arduboy.justPressed(A_BUTTON)) {
            if (usedLetters[cursor] == 0) {
                usedLetters[cursor] = 1;
                scoreResponse(cursor + 'A');
            } else {
                playTone(300, 30); // Letra ya usada
            }
        }
    }

    void updateGameOver() {
        if (bootActionTriggered || arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
            arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
            playTone(1000, 30);
            startPlaying();
        }
    }

    void draw() {
        yield();
        switch (mode) {
            case HANGMAN_MODE_TITLE:
                drawLogo();
                drawTitleMenu();
                drawHangman();
                break;
            case HANGMAN_MODE_STATS:
                drawStats();
                drawStatsMenu();
                drawHangman();
                break;
            case HANGMAN_MODE_PLAY:
                drawScore();
                drawHangman();
                drawWord();
                drawKeyboard();
                break;
            case HANGMAN_MODE_CORRECT:
                drawScore();
                drawHangman();
                drawWord();
                drawCorrect();
                break;
            case HANGMAN_MODE_DEAD:
                drawScore();
                drawHangman();
                drawWord();
                drawDead();
                break;
        }
    }

    void drawLogo() {
        arduboy.setCursor(0, 6);
        arduboy.print("HANGMAN!");
        arduboy.drawFastHLine(0, 15, 45, WHITE);
        arduboy.setCursor(0, 18);
        arduboy.print(" by serisman");
    }

    void drawTitleMenu() {
        arduboy.setCursor(10, 40); // 64 - (8 * 3)
        if (paused) arduboy.print("Resume");
        else arduboy.print("Start");

        arduboy.setCursor(10, 48); // 64 - (8 * 2)
        arduboy.print("Sound:");
        if (soundEnabled) arduboy.print("ON");
        else arduboy.print("OFF");

        arduboy.setCursor(10, 56); // 64 - (8 * 1)
        arduboy.print("Stats");

        // Cursor '>'
        arduboy.setCursor(0, 40 + (cursor * 8));
        arduboy.print(">");
    }

    void drawStats() {
        snprintf(buf, sizeof(buf), " %u Wins\n %u Losses", wins, losses);
        arduboy.setCursor(0, 10);
        arduboy.print(buf);
    }

    void drawStatsMenu() {
        arduboy.setCursor(10, 48);
        arduboy.print("Reset");

        arduboy.setCursor(10, 56);
        arduboy.print("Back");

        // Cursor '>'
        arduboy.setCursor(0, 48 + (cursor * 8));
        arduboy.print(">");
    }

    void drawScore() {
        snprintf(buf, sizeof(buf), "%uW-%uL", wins, losses);
        arduboy.setCursor((128 - 50) - (strlen(buf) * 6), 0);
        arduboy.print(buf);
    }

    void drawHangman() {
        const uint8_t LEFT = 128 - 45; // 83

        arduboy.fillRect(LEFT + 5, 61, 40, 3, WHITE);  // ground
        arduboy.fillRect(LEFT + 30, 0, 3, 64, WHITE);  // post
        arduboy.fillRect(LEFT + 10, 0, 20, 3, WHITE);  // bar
        arduboy.fillRect(LEFT + 10, 0, 3, 10, WHITE);  // noose

        if (hangman > 0) // head
            arduboy.drawCircle(LEFT + 11, 15, 5, WHITE);
        if (hangman > 1) // body
            arduboy.drawFastVLine(LEFT + 11, 20, 20, WHITE);
        if (hangman > 2) // left arm
            arduboy.drawLine(LEFT + 0, 22, LEFT + 11, 25, WHITE);
        if (hangman > 3) // right arm
            arduboy.drawLine(LEFT + 22, 22, LEFT + 11, 25, WHITE);
        if (hangman > 4) // left leg
            arduboy.drawLine(LEFT + 0, 52, LEFT + 11, 40, WHITE);
        if (hangman > 5) // right leg
            arduboy.drawLine(LEFT + 22, 52, LEFT + 11, 40, WHITE);

        if (paused) {
            arduboy.setCursor(LEFT + 7, 28);
            arduboy.print("Paused");
        }
    }

    void drawWord() {
        uint8_t x = 0;
        const uint8_t y = 12;
        uint8_t wordLen = strlen(currentWord);
        for (uint8_t chr = 0; chr < wordLen; chr++) {
            uint8_t letter = currentWord[chr];
            if (usedLetters[letter - 'A'] == 1) {
                arduboy.setCursor(x, y);
                arduboy.write(letter);
            } else {
                if (mode == HANGMAN_MODE_DEAD) {
                    arduboy.setCursor(x, y);
                    arduboy.write(letter + 32); // Mostrar respuesta correcta en minúscula
                }
                arduboy.drawFastHLine(x - 1, y + 9, 7, WHITE);
            }
            x += 9;
        }
    }

    void drawKeyboard() {
        uint8_t x = 0;
        uint8_t y = 19;
        for (uint8_t chr = 0; chr < 26; chr++) {
            if (chr % 9 == 0) {
                x = 0;
                y += 12;
            } else {
                x += 9;
            }

            // Letra activa resaltada con subrayado parpadeante
            if (cursor == chr && cursorBlink) {
                arduboy.drawFastHLine(x - 1, y + 8, 7, WHITE);
            }

            arduboy.setCursor(x, y);
            if (usedLetters[chr] == 0) {
                arduboy.write(chr + 'A');
            } else {
                arduboy.write(chr + 'a'); // Letras ya probadas en minúscula
            }
        }
    }

    void drawCorrect() {
        arduboy.setCursor(0, 35);
        arduboy.print("YOU GOT IT!");
        arduboy.setCursor(0, 48);
        arduboy.print("BOOT: Jugar");
    }

    void drawDead() {
        arduboy.setCursor(0, 35);
        arduboy.print("YOU'RE DEAD!");
        arduboy.setCursor(0, 48);
        arduboy.print("BOOT: Jugar");
    }
};
