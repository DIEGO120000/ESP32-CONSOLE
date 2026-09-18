#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"
#include "ProyectoBLE.h"
#include "braindu_bitmaps.h"
#include "hangman_words.h"

Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);

// =============================================================================
// RGB LED SYSTEM (GPIO 8 - WS2812 INTEGRATED)
// =============================================================================
#define RGB_LED_PIN 8
unsigned long rgbLedOffTime = 0;
bool rgbLedActive = false;

void triggerRgbLed(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs = 1000) {
    rgbLedWrite(RGB_LED_PIN, r, g, b);
    delay(2); // Retraso de 2ms no perceptible para prevenir caída de tensión (brownout) entre periféricos
    rgbLedOffTime = millis() + durationMs;
    rgbLedActive = true;
}

void updateRgbLed() {
    if (rgbLedActive && millis() >= rgbLedOffTime) {
        rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
        rgbLedActive = false;
    }
}

// =============================================================================
// MELODY & AUDIO SYNTHESIS SYSTEM (GPIO 2 - BUZZER)
// =============================================================================
struct Note {
    uint16_t freq;
    uint16_t duration;
    uint16_t pause;
};

// Melodía de Victoria Ping Pong (1046Hz C6, 1318Hz E6, 1568Hz G6, 2093Hz C7)
const Note pingPongVictoryNotes[4] = {
    {1046, 80, 15},
    {1318, 80, 15},
    {1568, 80, 15},
    {2093, 200, 20}
};

// Melodía de Derrota Ping Pong (330Hz E4, 293Hz D4, 261Hz C4, 196Hz G3)
const Note pingPongDefeatNotes[4] = {
    {330, 120, 15},
    {293, 120, 15},
    {261, 140, 15},
    {196, 300, 20}
};

// Melodía de Victoria Tic-Tac-Toe (Ascendente: C5 523Hz, E5 659Hz, G5 784Hz, C6 1046Hz)
const Note tttVictoryNotes[4] = {
    {523, 70, 10},
    {659, 70, 10},
    {784, 70, 10},
    {1046, 200, 20}
};

// Melodía de Derrota Tic-Tac-Toe (Descendente: G5 784Hz, E5 659Hz, C5 523Hz, G4 392Hz)
const Note tttDefeatNotes[4] = {
    {784, 90, 10},
    {659, 90, 10},
    {523, 110, 10},
    {392, 250, 20}
};

// Melodía de Victoria Hangman (Ascendente festiva: E4 330Hz, F4 349Hz, C5 523Hz)
const Note hangmanVictoryNotes[3] = {
    {330, 100, 10},
    {349, 100, 10},
    {523, 250, 20}
};

// Melodía de Derrota Hangman (Descendente fúnebre: D4 294Hz, B3 247Hz, E3 165Hz, F2 87Hz)
const Note hangmanDefeatNotes[4] = {
    {294, 100, 10},
    {247, 100, 10},
    {165, 100, 10},
    {87, 250, 20}
};


struct MelodyPlayer {
    const Note* currentNotes = nullptr;
    uint8_t totalNotes = 0;
    uint8_t noteIndex = 0;
    unsigned long nextNoteTime = 0;
    bool isPlaying = false;
    bool inPause = false;

    void play(const Note* notes, uint8_t count) {
        currentNotes = notes;
        totalNotes = count;
        noteIndex = 0;
        isPlaying = true;
        inPause = false;
        playCurrentNote();
    }

    void playCurrentNote() {
        if (!isPlaying || noteIndex >= totalNotes) {
            stop();
            return;
        }
        uint16_t freq = currentNotes[noteIndex].freq;
        uint16_t dur = currentNotes[noteIndex].duration;
        sound.tone(freq);
        nextNoteTime = millis() + dur;
        inPause = false;
    }

    void update() {
        if (!isPlaying) return;

        if (millis() >= nextNoteTime) {
            if (!inPause) {
                sound.noTone();
                uint16_t p = currentNotes[noteIndex].pause;
                if (p > 0) {
                    inPause = true;
                    nextNoteTime = millis() + p;
                } else {
                    noteIndex++;
                    playCurrentNote();
                }
            } else {
                inPause = false;
                noteIndex++;
                playCurrentNote();
            }
        }
    }

    void stop() {
        isPlaying = false;
        currentNotes = nullptr;
        noteIndex = 0;
        sound.noTone();
    }
} melodyPlayer;

// =============================================================================
// GLOBAL BITMAPS & SPRITES
// =============================================================================
// --- Space Invaders Sprites ---
const uint8_t PROGMEM alien_crab_1[] = {
    8, 8,
    0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0x81
};
const uint8_t PROGMEM alien_crab_2[] = {
    8, 8,
    0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0x24, 0x42
};
const uint8_t PROGMEM alien_squid[] = {
    8, 8,
    0x18, 0x3C, 0x7E, 0xFF, 0xBD, 0xFF, 0x24, 0x42
};
const uint8_t PROGMEM alien_octopus[] = {
    8, 8,
    0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x7E, 0x42, 0x81
};
const uint8_t PROGMEM player_ship_bmp[] = {
    11, 7,
    0x04, 0x0E, 0x0E, 0x1F, 0x7F, 0xFF, 0x7F, 0x1F, 0x0E, 0x0E, 0x04
};
const uint8_t PROGMEM ufo_bmp[] = {
    12, 6,
    0x0C, 0x1E, 0x3F, 0x7B, 0x7F, 0x7F, 0x7F, 0x7F, 0x7B, 0x3F, 0x1E, 0x0C
};

// --- 1943 Sprites ---
const uint8_t PROGMEM plane_1943_bmp[] = {
    11, 9,
    0x08, 0x1C, 0x3E, 0x7F, 0xFF, 0x3E, 0x1C, 0x3E, 0x7F, 0xFF, 0x08
};
const uint8_t PROGMEM enemy_1943_bmp[] = {
    9, 7,
    0x10, 0x38, 0x7C, 0xFE, 0x38, 0xFE, 0x7C, 0x38, 0x10
};
const uint8_t PROGMEM bomber_1943_bmp[] = {
    15, 9,
    0x38, 0x7C, 0xFE, 0xFF, 0x7C, 0xFF, 0xFE, 0x7C, 0x38, 0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38
};

// --- Jump Man Sprites (5x8 Pixel Format) ---
const uint8_t PROGMEM jm_dinoChar[8]    = { 0x04, 0x0E, 0x1F, 0x15, 0x0E, 0x04, 0x0A, 0x12 };
const uint8_t PROGMEM jm_saltoInicio[8] = { 0x04, 0x0E, 0x1F, 0x15, 0x0E, 0x04, 0x09, 0x09 };
const uint8_t PROGMEM jm_saltoPico[8]   = { 0x04, 0x0E, 0x1F, 0x15, 0x0E, 0x03, 0x02, 0x00 };
const uint8_t PROGMEM jm_saltoBajada[8] = { 0x04, 0x0E, 0x1F, 0x15, 0x0E, 0x06, 0x0A, 0x02 };

const uint8_t PROGMEM jm_obstaculo1[8]  = { 0x00, 0x04, 0x04, 0x15, 0x1F, 0x04, 0x04, 0x00 };
const uint8_t PROGMEM jm_obstaculo2[8]  = { 0x00, 0x1F, 0x15, 0x1F, 0x15, 0x1F, 0x15, 0x00 };
const uint8_t PROGMEM jm_obstaculo3[8]  = { 0x00, 0x04, 0x0E, 0x1F, 0x1F, 0x0E, 0x04, 0x00 };

// Helper to draw 5x8 bitmap
void draw5x8Sprite(int16_t x, int16_t y, const uint8_t sprite[8], uint8_t color = WHITE) {
    for (int row = 0; row < 8; row++) {
        uint8_t line = sprite[row];
        for (int col = 0; col < 5; col++) {
            if (line & (1 << (4 - col))) {
                arduboy.drawPixel(x + col, y + row, color);
            }
        }
    }
}

// =============================================================================
// =============================================================================
// GAME 1: PING PONG (Normal Match vs CPU & Inverted Survival Dodge Mode)
// =============================================================================
enum PingPongMode {
    PP_MODE_SUBMENU = 0,
    PP_MODE_NORMAL,
    PP_MODE_DODGE
};

struct PingPongGame {
    PingPongMode mode;
    int8_t submenuCursor;

    // Shared & Normal Mode Variables
    float p1Y, p2Y;
    float bx, by;
    float bvx, bvy;
    float ballSpeed;
    uint8_t s1, s2;
    uint16_t highScore; // Normal Mode High Score (EEPROM addr 14)
    uint8_t rallyCount;
    bool gameOver;
    bool victory;

    // Dodge Mode Variables
    uint16_t dodgeScore;
    uint16_t highScoreDodge; // Dodge Mode High Score (EEPROM addr 24)
    bool dodgeGameOver;
    bool isNewRecord;
    float targetCpuY;
    uint8_t launchCooldown;

    void init() {
        melodyPlayer.stop();
        mode = PP_MODE_SUBMENU;
        submenuCursor = 0;
    }

    void initNormal() {
        mode = PP_MODE_NORMAL;
        melodyPlayer.stop();
        p1Y = 24.0f;
        p2Y = 24.0f;
        s1 = 0;
        s2 = 0;
        highScore = 0;
        EEPROM.get(14, highScore);
        if (highScore == 0xFFFF) highScore = 0;
        gameOver = false;
        victory = false;
        serveNormal(1);
    }

    void serveNormal(int8_t dir) {
        bx = 64.0f;
        by = 34.0f;
        ballSpeed = 1.8f;
        rallyCount = 0;
        float angle = ((random(0, 100) / 100.0f) * 0.6f) - 0.3f;
        bvx = dir * ballSpeed * cos(angle);
        bvy = ballSpeed * sin(angle);
    }

    void initDodge() {
        mode = PP_MODE_DODGE;
        melodyPlayer.stop();
        p1Y = 24.0f;
        p2Y = 24.0f;
        targetCpuY = 11.0f + (float)random(0, 36);
        dodgeScore = 0;
        highScoreDodge = 0;
        EEPROM.get(24, highScoreDodge);
        if (highScoreDodge == 0xFFFF) highScoreDodge = 0;
        dodgeGameOver = false;
        isNewRecord = false;
        launchCooldown = 20;
        bx = 120.0f;
        by = p2Y + 6.5f;
        bvx = 0;
        bvy = 0;
    }

    void serveDodge() {
        // Difficulty curve: Base speed scales by ~8-10% every 3 dodges
        float ballSpeed = 2.2f + (float)(dodgeScore / 3) * 0.22f;
        if (ballSpeed > 5.2f) ballSpeed = 5.2f;

        // Position of the ball at CPU launcher
        bx = 118.0f;
        by = p2Y + 6.5f;

        // Algoritmo de disparo teledirigido hacia el centro exacto de la pala del jugador
        float playerCenterX = 4.0f;
        float playerCenterY = p1Y + 6.5f; // Centro de la pala del jugador (16px de alto)
        float cpuCenterX = bx;
        float cpuCenterY = by;

        float dy = playerCenterY - cpuCenterY;
        float dx = playerCenterX - cpuCenterX;
        float angle = atan2(dy, dx);

        bvx = cos(angle) * ballSpeed;
        bvy = sin(angle) * ballSpeed;

        // Garantizar velocidad horizontal activa hacia el jugador
        if (bvx > -1.2f) bvx = -1.2f;
    }

    void updateSubmenu() {
        if (arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(UP_BUTTON)) {
            sound.tone(880, 15);
            submenuCursor--;
            if (submenuCursor < 0) submenuCursor = 1;
        }
        if (arduboy.justPressed(RIGHT_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
            sound.tone(700, 15);
            submenuCursor++;
            if (submenuCursor > 1) submenuCursor = 0;
        }
        if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) {
            sound.tone(1760, 40);
            if (submenuCursor == 0) {
                initNormal();
            } else {
                initDodge();
            }
        }
    }

    void drawSubmenu() {
        // Header
        arduboy.fillRect(0, 0, 128, 9, WHITE);
        arduboy.setTextColor(BLACK);
        arduboy.setCursor(36, 1);
        arduboy.print("PING PONG");
        arduboy.setTextColor(WHITE);

        const char* ppItems[2] = {
            "1. Modo Normal",
            "2. Modo Invertido"
        };

        for (uint8_t i = 0; i < 2; i++) {
            int16_t y = 20 + (i * 14);
            if (i == submenuCursor) {
                arduboy.fillRect(8, y - 2, 112, 11, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(12, y);
                arduboy.print("> ");
                arduboy.print(ppItems[i]);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.setCursor(12, y);
                arduboy.print("  ");
                arduboy.print(ppItems[i]);
            }
        }

        // Bottom Navigation Bar
        arduboy.drawFastHLine(0, 54, 128, WHITE);
        arduboy.setCursor(2, 56);
        arduboy.print("B1/B2:Mover  B3/B4:OK");
    }

    void updateNormal() {
        if (gameOver || victory) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
                arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON) ||
                arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
                init();
            }
            return;
        }

        // --- Player Controls (B1: Up, B2: Down) ---
        if ((arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(UP_BUTTON)) && p1Y > 11.0f) p1Y -= 2.6f;
        if ((arduboy.pressed(RIGHT_BUTTON) || arduboy.pressed(DOWN_BUTTON)) && p1Y < 46.0f) p1Y += 2.6f;

        // --- CPU AI with Human-like Inertia & Error Margin ---
        if (bvx > 0.0f) {
            float targetY = by - 8.0f + (sin(millis() * 0.008f) * 3.5f);
            float maxCpuSpeed = min(2.4f, 1.2f + ballSpeed * 0.22f);
            if (p2Y < targetY - 1.5f) p2Y += min(maxCpuSpeed, targetY - p2Y);
            else if (p2Y > targetY + 1.5f) p2Y -= min(maxCpuSpeed, p2Y - targetY);
        } else {
            if (p2Y < 24.0f) p2Y += 0.6f;
            else if (p2Y > 24.0f) p2Y -= 0.6f;
        }
        if (p2Y < 11.0f) p2Y = 11.0f;
        if (p2Y > 46.0f) p2Y = 46.0f;

        // Move Ball
        bx += bvx;
        by += bvy;

        // Top / Bottom Wall Bounce
        if (by <= 10.0f) {
            by = 10.0f;
            bvy = fabs(bvy);
            sound.tone(450, 12);
        } else if (by >= 60.0f) {
            by = 60.0f;
            bvy = -fabs(bvy);
            sound.tone(450, 12);
        }

        // --- Left Paddle Segmented Collision (5 Segments) ---
        if (bx <= 6.0f && bx >= 2.0f && by + 3.0f >= p1Y && by <= p1Y + 16.0f && bvx < 0.0f) {
            bx = 7.0f;
            rallyCount++;
            ballSpeed = min(4.6f, ballSpeed * 1.05f);

            float relY = (by + 1.5f) - (p1Y + 8.0f);
            if (relY <= -4.8f) {
                bvx = ballSpeed * 0.50f;
                bvy = -ballSpeed * 0.866f;
            } else if (relY <= -1.6f) {
                bvx = ballSpeed * 0.866f;
                bvy = -ballSpeed * 0.50f;
            } else if (relY <= 1.6f) {
                bvx = ballSpeed * 0.98f;
                bvy = relY * 0.12f;
            } else if (relY <= 4.8f) {
                bvx = ballSpeed * 0.866f;
                bvy = ballSpeed * 0.50f;
            } else {
                bvx = ballSpeed * 0.50f;
                bvy = ballSpeed * 0.866f;
            }
            sound.tone(800 + min(600, rallyCount * 40), 18);
        }

        // --- Right Paddle Segmented Collision (CPU) ---
        if (bx >= 119.0f && bx <= 123.0f && by + 3.0f >= p2Y && by <= p2Y + 16.0f && bvx > 0.0f) {
            bx = 118.0f;
            rallyCount++;
            ballSpeed = min(4.6f, ballSpeed * 1.05f);

            float relY = (by + 1.5f) - (p2Y + 8.0f);
            if (relY <= -4.8f) {
                bvx = -ballSpeed * 0.50f;
                bvy = -ballSpeed * 0.866f;
            } else if (relY <= -1.6f) {
                bvx = -ballSpeed * 0.866f;
                bvy = -ballSpeed * 0.50f;
            } else if (relY <= 1.6f) {
                bvx = -ballSpeed * 0.98f;
                bvy = relY * 0.12f;
            } else if (relY <= 4.8f) {
                bvx = -ballSpeed * 0.866f;
                bvy = ballSpeed * 0.50f;
            } else {
                bvx = -ballSpeed * 0.50f;
                bvy = ballSpeed * 0.866f;
            }
            sound.tone(700 + min(600, rallyCount * 40), 18);
        }

        // Score Check (Match to 5 points)
        if (bx < -4.0f) {
            s2++;
            if (s2 >= 5) {
                gameOver = true;
                triggerRgbLed(255, 0, 0, 1000); // Exclusively RED on defeat vs CPU
                melodyPlayer.play(pingPongDefeatNotes, 4);
            } else {
                sound.tone(220, 80);
                serveNormal(1);
            }
        } else if (bx > 132.0f) {
            s1++;
            if (s1 > highScore) { highScore = s1; EEPROM.put(14, highScore); }
            if (s1 >= 5) {
                victory = true;
                triggerRgbLed(0, 255, 0, 1000); // GREEN on player victory
                melodyPlayer.play(pingPongVictoryNotes, 4);
            } else {
                sound.tone(1400, 80);
                serveNormal(-1);
            }
        }
    }

    void drawNormal() {
        // Top Scoreboard (Player vs CPU)
        arduboy.setCursor(20, 1); arduboy.print("YOU: "); arduboy.print(s1);
        arduboy.setCursor(76, 1); arduboy.print("CPU: "); arduboy.print(s2);
        arduboy.drawFastHLine(0, 9, 128, WHITE);

        // Court Net
        for (uint8_t y = 11; y < 64; y += 6) arduboy.drawFastVLine(63, y, 3, WHITE);

        // Paddles
        arduboy.fillRect(3, (int16_t)p1Y, 3, 16, WHITE);
        arduboy.fillRect(122, (int16_t)p2Y, 3, 16, WHITE);

        // Ball
        arduboy.fillRect((int16_t)bx, (int16_t)by, 3, 3, WHITE);

        if (gameOver) {
            arduboy.fillRect(18, 18, 92, 28, BLACK);
            arduboy.drawRect(18, 18, 92, 28, WHITE);
            arduboy.setCursor(38, 23); arduboy.print("CPU WINS!");
            arduboy.setCursor(24, 34); arduboy.print("Press Any Key");
        } else if (victory) {
            arduboy.fillRect(18, 18, 92, 28, BLACK);
            arduboy.drawRect(18, 18, 92, 28, WHITE);
            arduboy.setCursor(40, 23); arduboy.print("YOU WIN!");
            arduboy.setCursor(24, 34); arduboy.print("Press Any Key");
        }
    }

    void updateDodge() {
        if (dodgeGameOver) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
                arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(RIGHT_BUTTON) ||
                arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
                init(); // Return to Ping Pong submenu
            }
            return;
        }

        // Player Controls (B1: Up, B2: Down)
        if ((arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(UP_BUTTON)) && p1Y > 11.0f) p1Y -= 2.6f;
        if ((arduboy.pressed(RIGHT_BUTTON) || arduboy.pressed(DOWN_BUTTON)) && p1Y < 46.0f) p1Y += 2.6f;

        // Launch Cooldown (CPU prepares next launch)
        if (launchCooldown > 0) {
            launchCooldown--;
            if (p2Y < targetCpuY - 1.0f) p2Y += min(3.5f, targetCpuY - p2Y);
            else if (p2Y > targetCpuY + 1.0f) p2Y -= min(3.5f, p2Y - targetCpuY);
            if (launchCooldown == 0) {
                serveDodge();
            }
            return;
        }

        // Move Ball
        bx += bvx;
        by += bvy;

        // Top / Bottom Wall Bounce with Active Trajectory Correction
        if (by <= 10.0f) {
            by = 10.0f;
            bvy = fabs(bvy);
            if (bvx > -1.2f) bvx = -1.2f;
            sound.tone(450, 12);
        } else if (by >= 60.0f) {
            by = 60.0f;
            bvy = -fabs(bvy);
            if (bvx > -1.2f) bvx = -1.2f;
            sound.tone(450, 12);
        }

        // --- Colisión / Game Over: Si la pelota choca con la pala del jugador ---
        if (bx <= 6.0f && bx >= 2.0f && by + 3.0f >= p1Y && by <= p1Y + 16.0f) {
            dodgeGameOver = true;
            sound.tone(180, 120);

            if (dodgeScore > highScoreDodge) {
                highScoreDodge = dodgeScore;
                EEPROM.put(24, highScoreDodge);
                isNewRecord = true;
                triggerRgbLed(0, 255, 0, 1000); // 1s Green LED on personal record
                melodyPlayer.play(pingPongVictoryNotes, 4);
            }
            return;
        }

        // --- Punto ganado (+1 Score): Si la pelota rebasa la línea de fondo del jugador ---
        if (bx < -4.0f) {
            dodgeScore++;
            sound.tone(1300, 30); // Short bip
            launchCooldown = 18;  // Pause before next launch
            targetCpuY = 11.0f + (float)random(0, 36);
            bx = 120.0f;
            by = targetCpuY + 6.5f;
            bvx = 0;
            bvy = 0;
        }
    }

    void drawDodge() {
        // Top Scoreboard
        arduboy.setCursor(4, 1); arduboy.print("ESQUIVAS: "); arduboy.print(dodgeScore);
        arduboy.setCursor(80, 1); arduboy.print("RECORD: "); arduboy.print(highScoreDodge);
        arduboy.drawFastHLine(0, 9, 128, WHITE);

        // Court Net
        for (uint8_t y = 11; y < 64; y += 6) arduboy.drawFastVLine(63, y, 3, WHITE);

        // Player Paddle (Left)
        arduboy.fillRect(3, (int16_t)p1Y, 3, 16, WHITE);

        // CPU Launcher (Right)
        arduboy.fillRect(122, (int16_t)p2Y, 3, 16, WHITE);

        // Ball
        if (launchCooldown == 0 && !dodgeGameOver) {
            arduboy.fillRect((int16_t)bx, (int16_t)by, 3, 3, WHITE);
        } else if (launchCooldown > 0) {
            arduboy.fillRect(118, (int16_t)p2Y + 6, 3, 3, WHITE);
        }

        if (dodgeGameOver) {
            arduboy.fillRect(14, 14, 100, 36, BLACK);
            arduboy.drawRect(14, 14, 100, 36, WHITE);
            arduboy.drawRect(16, 16, 96, 32, WHITE);

            if (isNewRecord) {
                arduboy.setCursor(20, 19);
                arduboy.print("NUEVO RECORD!");
            } else {
                arduboy.setCursor(34, 19);
                arduboy.print("GAME OVER");
            }

            arduboy.setCursor(24, 29);
            arduboy.print("ESQUIVAS: ");
            arduboy.print(dodgeScore);

            arduboy.setCursor(22, 38);
            arduboy.print("Pulsa un boton");
        }
    }

    void update() {
        yield();
        switch (mode) {
            case PP_MODE_SUBMENU:
                updateSubmenu();
                break;
            case PP_MODE_NORMAL:
                updateNormal();
                break;
            case PP_MODE_DODGE:
                updateDodge();
                break;
        }
        yield();
    }

    void draw() {
        switch (mode) {
            case PP_MODE_SUBMENU:
                drawSubmenu();
                break;
            case PP_MODE_NORMAL:
                drawNormal();
                break;
            case PP_MODE_DODGE:
                drawDodge();
                break;
        }
    }
} pingPong;

// =============================================================================
// GAME 2: ARKANOID (15 Original Rounds, 7 Power-Ups, Lasers, Multi-Ball & Warp)
// =============================================================================
const uint8_t PROGMEM arkanoid_levels[15][8][11] = {
    // Round 1: Classic Color Rows (Silver top row, Blue, Green, Red)
    {
        {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 2: Inverted Pyramid with Silver Pillars
    {
        {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4},
        {4, 0, 2, 2, 2, 2, 2, 2, 2, 0, 4},
        {4, 0, 0, 1, 1, 1, 1, 1, 0, 0, 4},
        {4, 0, 0, 0, 3, 3, 3, 0, 0, 0, 4},
        {4, 0, 0, 0, 0, 2, 0, 0, 0, 0, 4},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 3: Classic Space Invader
    {
        {0, 0, 3, 0, 0, 0, 0, 0, 3, 0, 0},
        {0, 0, 0, 3, 0, 0, 0, 3, 0, 0, 0},
        {0, 0, 3, 3, 3, 3, 3, 3, 3, 0, 0},
        {0, 3, 3, 4, 3, 3, 3, 4, 3, 3, 0},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {3, 0, 3, 3, 3, 3, 3, 3, 3, 0, 3},
        {3, 0, 3, 0, 0, 0, 0, 0, 3, 0, 3},
        {0, 0, 0, 2, 2, 0, 2, 2, 0, 0, 0}
    },
    // Round 4: Concentric Diamonds with Gold Center
    {
        {0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 3, 2, 3, 0, 0, 0, 0},
        {0, 0, 0, 3, 2, 1, 2, 3, 0, 0, 0},
        {0, 0, 3, 2, 1, 5, 1, 2, 3, 0, 0},
        {0, 0, 0, 3, 2, 1, 2, 3, 0, 0, 0},
        {0, 0, 0, 0, 3, 2, 3, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 5: Twin Pillars with Gold Cores
    {
        {3, 3, 0, 0, 4, 4, 4, 0, 0, 3, 3},
        {3, 5, 3, 0, 4, 2, 4, 0, 3, 5, 3},
        {3, 5, 3, 0, 4, 2, 4, 0, 3, 5, 3},
        {3, 5, 3, 0, 4, 1, 4, 0, 3, 5, 3},
        {3, 5, 3, 0, 4, 1, 4, 0, 3, 5, 3},
        {3, 3, 3, 0, 4, 4, 4, 0, 3, 3, 3},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 6: Arcade Shield / Heart
    {
        {0, 3, 3, 0, 0, 0, 0, 0, 3, 3, 0},
        {3, 2, 2, 3, 0, 4, 0, 3, 2, 2, 3},
        {3, 2, 1, 2, 3, 4, 3, 2, 1, 2, 3},
        {3, 2, 1, 1, 2, 4, 2, 1, 1, 2, 3},
        {0, 3, 2, 1, 1, 2, 1, 1, 2, 3, 0},
        {0, 0, 3, 2, 1, 1, 1, 2, 3, 0, 0},
        {0, 0, 0, 3, 2, 1, 2, 3, 0, 0, 0},
        {0, 0, 0, 0, 3, 2, 3, 0, 0, 0, 0}
    },
    // Round 7: Checkerboard with Silver Corners
    {
        {4, 0, 3, 0, 3, 0, 3, 0, 3, 0, 4},
        {0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0},
        {3, 0, 1, 0, 1, 0, 1, 0, 1, 0, 3},
        {0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0},
        {3, 0, 1, 0, 1, 0, 1, 0, 1, 0, 3},
        {0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0},
        {4, 0, 3, 0, 3, 0, 3, 0, 3, 0, 4},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 8: Castle Fortress Battlement
    {
        {4, 4, 0, 4, 4, 0, 4, 4, 0, 4, 4},
        {4, 4, 0, 4, 4, 0, 4, 4, 0, 4, 4},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {2, 2, 5, 2, 2, 5, 2, 2, 5, 2, 2},
        {1, 1, 5, 1, 1, 5, 1, 1, 5, 1, 1},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 9: Zig-Zag Stairs Maze
    {
        {4, 3, 2, 1, 0, 0, 0, 1, 2, 3, 4},
        {0, 4, 3, 2, 1, 0, 1, 2, 3, 4, 0},
        {0, 0, 4, 3, 2, 5, 2, 3, 4, 0, 0},
        {0, 0, 0, 4, 3, 2, 3, 4, 0, 0, 0},
        {0, 0, 4, 3, 2, 5, 2, 3, 4, 0, 0},
        {0, 4, 3, 2, 1, 0, 1, 2, 3, 4, 0},
        {4, 3, 2, 1, 0, 0, 0, 1, 2, 3, 4},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 10: Arcade Robot Face
    {
        {0, 0, 4, 4, 4, 4, 4, 4, 4, 0, 0},
        {0, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0},
        {3, 3, 5, 3, 3, 3, 3, 3, 5, 3, 3},
        {3, 3, 5, 3, 3, 4, 3, 3, 5, 3, 3},
        {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
        {0, 3, 2, 1, 1, 1, 1, 1, 2, 3, 0},
        {0, 0, 3, 2, 2, 2, 2, 2, 3, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 11: Cross of Arkanoid with Silver Edges
    {
        {4, 0, 0, 0, 3, 3, 3, 0, 0, 0, 4},
        {0, 4, 0, 0, 3, 2, 3, 0, 0, 4, 0},
        {0, 0, 4, 0, 3, 1, 3, 0, 4, 0, 0},
        {3, 3, 3, 3, 3, 5, 3, 3, 3, 3, 3},
        {0, 0, 4, 0, 3, 1, 3, 0, 4, 0, 0},
        {0, 4, 0, 0, 3, 2, 3, 0, 0, 4, 0},
        {4, 0, 0, 0, 3, 3, 3, 0, 0, 0, 4},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 12: Diagonal Ribbons
    {
        {3, 2, 1, 0, 0, 3, 2, 1, 0, 0, 3},
        {2, 1, 0, 0, 3, 2, 1, 0, 0, 3, 2},
        {1, 0, 0, 3, 2, 1, 0, 0, 3, 2, 1},
        {0, 0, 3, 2, 1, 4, 1, 2, 3, 0, 0},
        {1, 0, 0, 3, 2, 1, 0, 0, 3, 2, 1},
        {2, 1, 0, 0, 3, 2, 1, 0, 0, 3, 2},
        {3, 2, 1, 0, 0, 3, 2, 1, 0, 0, 3},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 13: Hourglass
    {
        {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4},
        {0, 4, 2, 2, 2, 2, 2, 2, 2, 4, 0},
        {0, 0, 4, 1, 1, 1, 1, 1, 4, 0, 0},
        {0, 0, 0, 4, 5, 5, 5, 4, 0, 0, 0},
        {0, 0, 4, 1, 1, 1, 1, 1, 4, 0, 0},
        {0, 4, 2, 2, 2, 2, 2, 2, 2, 4, 0},
        {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    },
    // Round 14: Concentric Fortress Cage
    {
        {5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5},
        {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4},
        {4, 3, 2, 2, 2, 2, 2, 2, 2, 3, 4},
        {4, 3, 2, 1, 0, 0, 0, 1, 2, 3, 4},
        {4, 3, 2, 1, 0, 5, 0, 1, 2, 3, 4},
        {4, 3, 2, 2, 2, 2, 2, 2, 2, 3, 4},
        {4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4},
        {5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5}
    },
    // Round 15: Final Boss Chamber - DOH Shrine
    {
        {5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5},
        {5, 3, 3, 3, 3, 3, 3, 3, 3, 3, 5},
        {4, 3, 2, 2, 5, 5, 5, 2, 2, 3, 4},
        {4, 3, 2, 1, 5, 4, 5, 1, 2, 3, 4},
        {4, 3, 2, 1, 5, 4, 5, 1, 2, 3, 4},
        {4, 3, 2, 2, 5, 5, 5, 2, 2, 3, 4},
        {5, 3, 3, 3, 3, 3, 3, 3, 3, 3, 5},
        {5, 5, 4, 4, 4, 4, 4, 4, 4, 5, 5}
    }
};

struct BreakoutGame {
    enum PowerUpType {
        PWR_NONE = 0,
        PWR_EXPAND,    // [E]
        PWR_LASER,     // [L]
        PWR_DISRUPT,   // [D]
        PWR_CATCH,     // [C]
        PWR_SLOW,      // [S]
        PWR_BREAK,     // [B]
        PWR_EXTRA_LIFE // [P]
    };

    struct Ball {
        float x, y;
        float vx, vy;
        bool active;
        bool stuck;
        float stuckOffset;
        uint32_t stuckTimer;
        uint8_t bounceCount;
    } balls[3];

    struct Capsule {
        float x, y;
        uint8_t type;
        bool active;
    } capsules[3];

    struct Laser {
        float x, y;
        bool active;
    } lasers[4];

    float paddleX;
    float paddleWidth;
    uint8_t activePowerUp;
    bool warpGateOpen;
    uint8_t laserCooldown;

    uint8_t currentLevel;
    uint8_t bricks[8][11];
    uint8_t silverHits[8][11];

    uint16_t score;
    uint16_t highScore;
    uint8_t lives;
    float ballSpeed;
    uint8_t hitsSinceSpeedUp;
    uint32_t lastLaserAutoFire;

    bool gameOver;
    bool victory;
    bool levelTransition;
    uint32_t levelTransitionTimer;
    bool newHighScoreNotified;

    void checkHighScore() {
        if (score > highScore) {
            if (!newHighScoreNotified) {
                newHighScoreNotified = true;
                triggerRgbLed(0, 255, 0, 1000); // GREEN on beating record
            }
            highScore = score;
            EEPROM.put(12, highScore);
        }
    }

    void init() {
        randomSeed(micros());
        score = 0;
        lives = 3;
        gameOver = false;
        victory = false;
        levelTransition = false;
        newHighScoreNotified = false;

        highScore = 0;
        EEPROM.get(12, highScore);
        if (highScore == 0xFFFF) highScore = 0;

        uint8_t startLvl = random(0, 15);
        loadLevel(startLvl);
    }

    void loadLevel(uint8_t lvl) {
        currentLevel = lvl;
        paddleWidth = 18.0f;
        paddleX = (128.0f - paddleWidth) / 2.0f;
        activePowerUp = PWR_NONE;
        warpGateOpen = false;
        laserCooldown = 0;
        lastLaserAutoFire = millis();
        ballSpeed = 1.4f;
        hitsSinceSpeedUp = 0;

        // Reset Lasers and Capsules
        for (uint8_t i = 0; i < 4; i++) lasers[i].active = false;
        for (uint8_t i = 0; i < 3; i++) capsules[i].active = false;

        // Load Matrix from PROGMEM
        for (uint8_t r = 0; r < 8; r++) {
            for (uint8_t c = 0; c < 11; c++) {
                bricks[r][c] = pgm_read_byte(&arkanoid_levels[lvl][r][c]);
                silverHits[r][c] = 0;
            }
        }

        // Spawn Main Ball (Ball 0 attached to paddle)
        balls[0].active = true;
        balls[0].stuck = true;
        balls[0].stuckOffset = paddleWidth / 2.0f - 1.5f;
        balls[0].stuckTimer = millis();
        balls[0].bounceCount = 0;
        balls[0].x = paddleX + balls[0].stuckOffset;
        balls[0].y = 54.0f;
        balls[0].vx = 1.0f;
        balls[0].vy = -1.3f;

        balls[1].active = false;
        balls[1].bounceCount = 0;
        balls[2].active = false;
        balls[2].bounceCount = 0;

        if (countDestructibleBricks() == 0) {
            advanceLevel();
            return;
        }

        levelTransition = true;
        levelTransitionTimer = millis();
        sound.tone(900, 40); delay(50);
        sound.tone(1400, 80);
    }

    uint8_t countDestructibleBricks() {
        uint8_t count = 0;
        for (uint8_t r = 0; r < 8; r++) {
            for (uint8_t c = 0; c < 11; c++) {
                if (bricks[r][c] >= 1 && bricks[r][c] <= 4) count++;
            }
        }
        return count;
    }

    void maybeSpawnCapsule(int16_t bx, int16_t by) {
        if (random(0, 100) < 18) {
            for (uint8_t i = 0; i < 3; i++) {
                if (!capsules[i].active) {
                    capsules[i].active = true;
                    capsules[i].x = bx + 1.0f;
                    capsules[i].y = by;
                    // Random Power-Up Type: E, L, D, C, S, B, P
                    static const uint8_t types[] = { PWR_EXPAND, PWR_LASER, PWR_DISRUPT, PWR_CATCH, PWR_SLOW, PWR_BREAK, PWR_EXTRA_LIFE };
                    capsules[i].type = types[random(0, 7)];
                    break;
                }
            }
        }
    }

    void applyPowerUp(uint8_t type) {
        if (type == PWR_EXTRA_LIFE) {
            if (lives < 5) lives++;
            sound.tone(1200, 40); delay(40); sound.tone(1800, 90);
            return;
        }

        // Exclusive switch for modifiers
        activePowerUp = type;

        // Reset previous special states
        if (type != PWR_EXPAND) paddleWidth = 18.0f;
        if (type != PWR_BREAK) warpGateOpen = false;

        if (type == PWR_EXPAND) {
            paddleWidth = 28.0f;
            if (paddleX + paddleWidth > 126.0f) paddleX = 126.0f - paddleWidth;
            sound.tone(1100, 50);
        } else if (type == PWR_LASER) {
            lastLaserAutoFire = millis();
            sound.tone(1500, 50);
        } else if (type == PWR_DISRUPT) {
            // Find first active ball to replicate
            int8_t mainBall = -1;
            for (uint8_t b = 0; b < 3; b++) {
                if (balls[b].active) { mainBall = b; break; }
            }
            if (mainBall != -1) {
                float bx = balls[mainBall].x;
                float by = balls[mainBall].y;
                balls[0].active = true; balls[0].x = bx; balls[0].y = by; balls[0].vx = -1.1f; balls[0].vy = -1.3f; balls[0].stuck = false; balls[0].bounceCount = 0;
                balls[1].active = true; balls[1].x = bx; balls[1].y = by; balls[1].vx = 0.4f;  balls[1].vy = -1.6f; balls[1].stuck = false; balls[1].bounceCount = 0;
                balls[2].active = true; balls[2].x = bx; balls[2].y = by; balls[2].vx = 1.3f;  balls[2].vy = -1.1f; balls[2].stuck = false; balls[2].bounceCount = 0;
                sound.tone(1300, 40); delay(40); sound.tone(1600, 60);
            }
        } else if (type == PWR_CATCH) {
            sound.tone(950, 50);
        } else if (type == PWR_SLOW) {
            ballSpeed = 1.4f;
            for (uint8_t b = 0; b < 3; b++) {
                if (balls[b].active && !balls[b].stuck) {
                    float currentMag = sqrt(balls[b].vx * balls[b].vx + balls[b].vy * balls[b].vy);
                    if (currentMag > 0.1f) {
                        balls[b].vx = (balls[b].vx / currentMag) * ballSpeed;
                        balls[b].vy = (balls[b].vy / currentMag) * ballSpeed;
                    }
                }
            }
            sound.tone(600, 80);
        } else if (type == PWR_BREAK) {
            warpGateOpen = true;
            sound.tone(1400, 50); delay(40); sound.tone(1800, 70);
        }
    }

    void advanceLevel() {
        uint8_t nextLvl = random(0, 15);
        if (nextLvl == currentLevel && 15 > 1) {
            nextLvl = (nextLvl + 1 + random(0, 13)) % 15;
        }
        loadLevel(nextLvl);
    }

    void fireLasers() {
        if (laserCooldown > 0) return;
        uint8_t spawned = 0;
        for (uint8_t i = 0; i < 4; i++) {
            if (!lasers[i].active) {
                lasers[i].active = true;
                lasers[i].x = (spawned == 0) ? (paddleX + 1.0f) : (paddleX + paddleWidth - 2.0f);
                lasers[i].y = 54.0f;
                spawned++;
                if (spawned >= 2) break;
            }
        }
        if (spawned > 0) {
            laserCooldown = 12; // ~0.2s cooldown
            sound.tone(1500, 15);
        }
    }

    void launchStuckBall(Ball& ball) {
        ball.stuck = false;
        ball.bounceCount = 0;
        float center = paddleX + (paddleWidth / 2.0f);
        float offset = (ball.x + 1.5f - center) / (paddleWidth / 2.0f);
        if (offset < -0.9f) offset = -0.9f;
        if (offset > 0.9f) offset = 0.9f;
        ball.vx = offset * (ballSpeed * 0.85f);
        ball.vy = -sqrt(max(0.4f, (ballSpeed * ballSpeed) - (ball.vx * ball.vx)));
        sound.tone(800, 20);
    }

    void update() {
        yield();
        if (gameOver || victory) {
            if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) init();
            return;
        }

        if (levelTransition) {
            if (millis() - levelTransitionTimer > 1500 || arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) {
                levelTransition = false;
            }
            return;
        }

        if (countDestructibleBricks() == 0) {
            advanceLevel();
            return;
        }

        if (laserCooldown > 0) laserCooldown--;

        // --- Controls: Paddle Movement ---
        if ((arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(UP_BUTTON)) && paddleX > 2.0f) paddleX -= 2.8f;
        if ((arduboy.pressed(RIGHT_BUTTON) || arduboy.pressed(DOWN_BUTTON)) && paddleX + paddleWidth < 126.0f) paddleX += 2.8f;

        // --- Automatic Laser Auto-Fire (Periodic ~380ms) ---
        if (activePowerUp == PWR_LASER) {
            uint32_t now = millis();
            if (now - lastLaserAutoFire >= 380) {
                fireLasers();
                lastLaserAutoFire = now;
            }
        }

        // --- Action Button (B3 / B4) (Manual Launch for Catch & Serve) ---
        bool actionPressed = arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON);
        if (actionPressed) {
            for (uint8_t b = 0; b < 3; b++) {
                if (balls[b].active && balls[b].stuck) {
                    launchStuckBall(balls[b]);
                }
            }
        }

        // --- Warp Gate Trigger ---
        if (warpGateOpen && (paddleX + paddleWidth >= 125.0f)) {
            score += 1000;
            advanceLevel();
            return;
        }

        // --- Update Lasers ---
        for (uint8_t i = 0; i < 4; i++) {
            if (lasers[i].active) {
                lasers[i].y -= 3.5f;
                if (lasers[i].y < 10.0f) {
                    lasers[i].active = false;
                    continue;
                }

                // Laser vs Bricks
                if (lasers[i].y <= 51.0f) {
                    for (int8_t r = 7; r >= 0; r--) {
                        for (int8_t c = 0; c < 11; c++) {
                            if (bricks[r][c] > 0) {
                                int16_t bx = 4 + c * 11;
                                int16_t by = 12 + r * 5;
                                if (lasers[i].x >= bx && lasers[i].x <= bx + 10 && lasers[i].y >= by && lasers[i].y <= by + 4) {
                                    lasers[i].active = false;
                                    if (bricks[r][c] == 5) {
                                        sound.tone(1100, 10); // Indestructible
                                    } else if (bricks[r][c] == 4) {
                                        if (silverHits[r][c] == 0) {
                                            silverHits[r][c] = 1;
                                            sound.tone(800, 15);
                                        } else {
                                            bricks[r][c] = 0;
                                            score += 100;
                                            sound.tone(1200, 25);
                                            maybeSpawnCapsule(bx, by);
                                        }
                                    } else {
                                        uint16_t pts = (bricks[r][c] == 1) ? 50 : (bricks[r][c] == 2 ? 80 : 100);
                                        score += pts;
                                        bricks[r][c] = 0;
                                        sound.tone(900 + r * 60, 20);
                                        maybeSpawnCapsule(bx, by);
                                    }

                                    if (countDestructibleBricks() == 0) {
                                        advanceLevel();
                                        return;
                                    }
                                    goto laser_done;
                                }
                            }
                        }
                    }
                }
            laser_done: ;
            }
        }

        // --- Update Falling Capsules ---
        for (uint8_t i = 0; i < 3; i++) {
            if (capsules[i].active) {
                capsules[i].y += 1.0f;
                // Check Collision with Paddle
                if (capsules[i].y >= 55.0f && capsules[i].y <= 60.0f &&
                    capsules[i].x + 7.0f >= paddleX && capsules[i].x <= paddleX + paddleWidth) {
                    applyPowerUp(capsules[i].type);
                    capsules[i].active = false;
                } else if (capsules[i].y > 64.0f) {
                    capsules[i].active = false;
                }
            }
        }

        // --- Update Active Balls ---
        uint8_t activeBallCount = 0;
        for (uint8_t b = 0; b < 3; b++) {
            if (!balls[b].active) continue;
            activeBallCount++;

            if (balls[b].stuck) {
                balls[b].x = paddleX + balls[b].stuckOffset;
                balls[b].y = 54.0f;
                balls[b].bounceCount = 0;
                if (millis() - balls[b].stuckTimer > 3500) {
                    launchStuckBall(balls[b]);
                }
                continue;
            }

            balls[b].x += balls[b].vx;
            balls[b].y += balls[b].vy;

            // Side Walls
            if (balls[b].x <= 1.0f) {
                balls[b].x = 1.0f;
                balls[b].vx = fabs(balls[b].vx);
                balls[b].bounceCount++;
                sound.tone(450, 10);
            } else if (balls[b].x >= 124.0f) {
                balls[b].x = 124.0f;
                balls[b].vx = -fabs(balls[b].vx);
                balls[b].bounceCount++;
                sound.tone(450, 10);
            }

            // Top Wall
            if (balls[b].y <= 10.0f) {
                balls[b].y = 10.0f;
                balls[b].vy = fabs(balls[b].vy);
                balls[b].bounceCount++;
                sound.tone(450, 10);
            }

            // Paddle Collision
            if (balls[b].y >= 54.0f && balls[b].y <= 59.0f &&
                balls[b].x + 3.0f >= paddleX && balls[b].x <= paddleX + paddleWidth && balls[b].vy > 0.0f) {
                balls[b].bounceCount = 0; // Reset bounce count on paddle contact
                if (activePowerUp == PWR_CATCH) {
                    balls[b].stuck = true;
                    balls[b].stuckOffset = balls[b].x - paddleX;
                    balls[b].stuckTimer = millis();
                    sound.tone(850, 15);
                } else {
                    float center = paddleX + (paddleWidth / 2.0f);
                    float offset = (balls[b].x + 1.5f - center) / (paddleWidth / 2.0f);
                    if (offset < -0.9f) offset = -0.9f;
                    if (offset > 0.9f) offset = 0.9f;
                    balls[b].vx = offset * (ballSpeed * 0.88f);
                    balls[b].vy = -sqrt(max(0.4f, (ballSpeed * ballSpeed) - (balls[b].vx * balls[b].vx)));
                    sound.tone(750, 15);
                    hitsSinceSpeedUp++;
                    if (hitsSinceSpeedUp >= 8) {
                        hitsSinceSpeedUp = 0;
                        ballSpeed = min(2.7f, ballSpeed + 0.1f);
                    }
                }
            }

            // Bricks Collision (Strict AABB)
            if (balls[b].y <= 51.0f) {
                for (int8_t r = 0; r < 8; r++) {
                    for (int8_t c = 0; c < 11; c++) {
                        if (bricks[r][c] > 0) {
                            int16_t bx = 4 + c * 11;
                            int16_t by = 12 + r * 5;
                            int16_t bw = 10;
                            int16_t bh = 4;

                            if (balls[b].x + 3.0f >= bx && balls[b].x <= bx + bw &&
                                balls[b].y + 3.0f >= by && balls[b].y <= by + bh) {
                                float overlapLeft = (balls[b].x + 3.0f) - bx;
                                float overlapRight = (bx + bw) - balls[b].x;
                                float overlapTop = (balls[b].y + 3.0f) - by;
                                float overlapBottom = (by + bh) - balls[b].y;

                                float minOverlapX = min(overlapLeft, overlapRight);
                                float minOverlapY = min(overlapTop, overlapBottom);

                                if (minOverlapX < minOverlapY) {
                                    balls[b].vx = -balls[b].vx;
                                } else {
                                    balls[b].vy = -balls[b].vy;
                                }
                                balls[b].bounceCount++;

                                if (bricks[r][c] == 5) {
                                    sound.tone(1100, 10);
                                } else if (bricks[r][c] == 4) {
                                    if (silverHits[r][c] == 0) {
                                        silverHits[r][c] = 1;
                                        sound.tone(800, 15);
                                    } else {
                                        bricks[r][c] = 0;
                                        score += 100;
                                        sound.tone(1200, 25);
                                        maybeSpawnCapsule(bx, by);
                                    }
                                } else {
                                    uint16_t pts = (bricks[r][c] == 1) ? 50 : (bricks[r][c] == 2 ? 80 : 100);
                                    score += pts;
                                    bricks[r][c] = 0;
                                    sound.tone(900 + r * 60, 20);
                                    maybeSpawnCapsule(bx, by);
                                }

                                if (countDestructibleBricks() == 0) {
                                    advanceLevel();
                                    return;
                                }
                                goto ball_collision_done;
                            }
                        }
                    }
                }
            }
        ball_collision_done:

            // Rutina anti-atrapamiento (Anti-Tunneling / Escape Vector tras 12 rebotes seguidos sin tocar pala)
            if (balls[b].bounceCount >= 12) {
                float perturb = ((float)random(-15, 16) / 20.0f); // -0.75 a +0.75
                balls[b].vx += perturb;
                if (fabs(balls[b].vx) < 0.4f) {
                    balls[b].vx = (balls[b].vx < 0.0f ? -0.6f : 0.6f);
                }
                // Si la bola está atrapada arriba o lleva demasiados rebotes en bucle, forzar escape descendente
                if (balls[b].y <= 32.0f || balls[b].bounceCount >= 16) {
                    balls[b].vy = fabs(balls[b].vy);
                    if (balls[b].vy < 0.8f) balls[b].vy = 0.8f;
                }
                float mag = sqrt(balls[b].vx * balls[b].vx + balls[b].vy * balls[b].vy);
                if (mag > 0.1f) {
                    balls[b].vx = (balls[b].vx / mag) * ballSpeed;
                    balls[b].vy = (balls[b].vy / mag) * ballSpeed;
                }
                balls[b].bounceCount = 0; // Resetear tras aplicar vector de escape
            }

            // Ball Out of Bounds
            if (balls[b].y > 64.0f) {
                balls[b].active = false;
            }
        }

        // --- Life Loss Check (When All Active Balls Fall) ---
        bool anyBallActive = false;
        for (uint8_t b = 0; b < 3; b++) {
            if (balls[b].active) { anyBallActive = true; break; }
        }

        if (!anyBallActive) {
            lives--;
            sound.tone(180, 150);
            if (lives == 0) {
                gameOver = true;
                if (score > highScore) { highScore = score; EEPROM.put(12, highScore); }
            } else {
                // Respawn single ball attached to paddle
                paddleWidth = 18.0f;
                paddleX = (128.0f - paddleWidth) / 2.0f;
                activePowerUp = PWR_NONE;
                warpGateOpen = false;
                ballSpeed = 1.4f;

                balls[0].active = true;
                balls[0].stuck = true;
                balls[0].stuckOffset = paddleWidth / 2.0f - 1.5f;
                balls[0].stuckTimer = millis();
                balls[0].bounceCount = 0;
                balls[0].x = paddleX + balls[0].stuckOffset;
                balls[0].y = 54.0f;
                balls[0].vx = 1.0f;
                balls[0].vy = -1.3f;
            }
        }
        checkHighScore();
    }

    void drawCapsule(float x, float y, uint8_t type) {
        int16_t ix = (int16_t)x;
        int16_t iy = (int16_t)y;

        arduboy.fillRect(ix, iy, 7, 5, WHITE);
        arduboy.drawPixel(ix, iy, BLACK);
        arduboy.drawPixel(ix + 6, iy, BLACK);
        arduboy.drawPixel(ix, iy + 4, BLACK);
        arduboy.drawPixel(ix + 6, iy + 4, BLACK);

        // 3x5 Mini font table: {E, L, D, C, S, B, P}
        static const uint8_t PROGMEM miniLetters[8][5] = {
            {0, 0, 0, 0, 0}, // None
            {7, 4, 6, 4, 7}, // E (Expand)
            {4, 4, 4, 4, 7}, // L (Laser)
            {6, 5, 5, 5, 6}, // D (Disrupt / Multi-ball)
            {3, 4, 4, 4, 3}, // C (Catch)
            {3, 4, 2, 1, 6}, // S (Slow)
            {6, 5, 6, 5, 6}, // B (Break / Warp)
            {6, 5, 6, 4, 4}  // P (Extra Life)
        };

        if (type >= 1 && type <= 7) {
            for (int8_t r = 0; r < 5; r++) {
                uint8_t rowBits = pgm_read_byte(&miniLetters[type][r]);
                for (int8_t c = 0; c < 3; c++) {
                    if (rowBits & (1 << (2 - c))) {
                        arduboy.drawPixel(ix + 2 + c, iy + r, BLACK);
                    }
                }
            }
        }
    }

    char getPowerUpChar(uint8_t type) {
        switch (type) {
            case PWR_EXPAND: return 'E';
            case PWR_LASER: return 'L';
            case PWR_DISRUPT: return 'D';
            case PWR_CATCH: return 'C';
            case PWR_SLOW: return 'S';
            case PWR_BREAK: return 'B';
            default: return ' ';
        }
    }

    void draw() {
        // --- Top HUD Bar ---
        arduboy.fillRect(0, 0, 128, 9, BLACK); // Clean HUD strip to prevent residual pixels
        arduboy.setCursor(2, 0); arduboy.print("P:"); arduboy.print(score);
        arduboy.setCursor(50, 0); arduboy.print("H:"); arduboy.print(highScore);
        arduboy.setCursor(102, 0); arduboy.print("L:"); arduboy.print(lives);
        arduboy.drawFastHLine(0, 9, 128, WHITE);

        // --- Warp Gate (Right Wall Opening) ---
        if (warpGateOpen) {
            // Flashing portal animation
            bool flash = (millis() / 150) % 2;
            if (flash) {
                arduboy.fillRect(125, 52, 3, 9, WHITE);
                arduboy.drawFastHLine(125, 56, 3, BLACK);
            } else {
                arduboy.drawRect(125, 52, 3, 9, WHITE);
            }
        }

        // --- Draw Bricks ---
        for (uint8_t r = 0; r < 8; r++) {
            for (uint8_t c = 0; c < 11; c++) {
                uint8_t bt = bricks[r][c];
                if (bt == 0) continue;
                int16_t bx = 4 + c * 11;
                int16_t by = 12 + r * 5;

                if (bt == 1) {
                    // Soft: Outline with center dot
                    arduboy.drawRect(bx, by, 10, 4, WHITE);
                    arduboy.drawPixel(bx + 4, by + 1, WHITE);
                    arduboy.drawPixel(bx + 5, by + 2, WHITE);
                } else if (bt == 2) {
                    // Medium: Outline with inner bar
                    arduboy.drawRect(bx, by, 10, 4, WHITE);
                    arduboy.drawFastHLine(bx + 2, by + 1, 6, WHITE);
                    arduboy.drawFastHLine(bx + 2, by + 2, 6, WHITE);
                } else if (bt == 3) {
                    // Hard: Solid filled
                    arduboy.fillRect(bx, by, 10, 4, WHITE);
                } else if (bt == 4) {
                    // Silver: Solid with diamond sheen or crack
                    arduboy.fillRect(bx, by, 10, 4, WHITE);
                    if (silverHits[r][c] == 0) {
                        arduboy.drawPixel(bx + 2, by + 1, BLACK);
                        arduboy.drawPixel(bx + 7, by + 2, BLACK);
                    } else {
                        // Cracked look
                        arduboy.drawFastVLine(bx + 4, by, 4, BLACK);
                        arduboy.drawPixel(bx + 5, by + 2, BLACK);
                    }
                } else if (bt == 5) {
                    // Gold / Indestructible: Metallic textured bars
                    arduboy.drawRect(bx, by, 10, 4, WHITE);
                    arduboy.drawFastVLine(bx + 3, by + 1, 2, WHITE);
                    arduboy.drawFastVLine(bx + 6, by + 1, 2, WHITE);
                }
            }
        }

        // --- Draw Falling Capsules ---
        for (uint8_t i = 0; i < 3; i++) {
            if (capsules[i].active) {
                drawCapsule(capsules[i].x, capsules[i].y, capsules[i].type);
            }
        }

        // --- Draw Lasers ---
        for (uint8_t i = 0; i < 4; i++) {
            if (lasers[i].active) {
                arduboy.fillRect((int16_t)lasers[i].x, (int16_t)lasers[i].y, 1, 3, WHITE);
            }
        }

        // --- Draw Paddle ---
        int16_t px = (int16_t)paddleX;
        uint8_t pw = (uint8_t)paddleWidth;

        if (activePowerUp == PWR_LASER) {
            // Twin laser turrets
            arduboy.fillRect(px, 56, 2, 2, WHITE);
            arduboy.fillRect(px + pw - 2, 56, 2, 2, WHITE);
        }

        // Main rounded paddle body
        arduboy.fillRect(px, 58, pw, 3, WHITE);
        arduboy.drawPixel(px, 58, BLACK);
        arduboy.drawPixel(px + pw - 1, 58, BLACK);

        // --- Draw Balls ---
        for (uint8_t b = 0; b < 3; b++) {
            if (balls[b].active) {
                arduboy.fillRect((int16_t)balls[b].x, (int16_t)balls[b].y, 3, 3, WHITE);
            }
        }

        // --- Intermission / Overlay Screens ---
        if (levelTransition) {
            arduboy.fillRect(28, 22, 72, 20, BLACK);
            arduboy.drawRect(28, 22, 72, 20, WHITE);
            arduboy.setCursor(38, 28);
            arduboy.print("ROUND ");
            arduboy.print(currentLevel + 1);
        } else if (gameOver) {
            arduboy.fillRect(24, 20, 80, 26, BLACK);
            arduboy.drawRect(24, 20, 80, 26, WHITE);
            arduboy.setCursor(34, 24); arduboy.print("GAME OVER");
            arduboy.setCursor(28, 33); arduboy.print("Press B3:Play");
        } else if (victory) {
            arduboy.fillRect(20, 18, 88, 30, BLACK);
            arduboy.drawRect(20, 18, 88, 30, WHITE);
            arduboy.setCursor(30, 23); arduboy.print("DOH DEFEATED!");
            arduboy.setCursor(36, 32); arduboy.print("VICTORY!!!");
        }
    }
} breakout;

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

        // Player Controls
        if ((arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(UP_BUTTON)) && playerX > 2) playerX -= 2;
        if ((arduboy.pressed(RIGHT_BUTTON) || arduboy.pressed(DOWN_BUTTON)) && playerX < 114) playerX += 2;

        if ((arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) && !bulletActive) {
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
} spaceInvaders;

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

        // Controls
        if ((arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(UP_BUTTON)) && planeX > 2) planeX -= 2;
        if ((arduboy.pressed(RIGHT_BUTTON) || arduboy.pressed(DOWN_BUTTON)) && planeX < 116) planeX += 2;

        static uint8_t shootTimer = 0;
        shootTimer++;
        if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) || shootTimer > 16) {
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
} airCombat;

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
} flappy;

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
        if ((arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(A_BUTTON)) && currentDir != DIR_DOWN) {
            nextDir = DIR_UP;
        } else if ((arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(B_BUTTON)) && currentDir != DIR_UP) {
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
} snake;

// =============================================================================
// GAME 7: JUMP MAN (Authentic Source Import, Full 128px Screen & Smooth Scrolling)
// =============================================================================
struct JumpManGame {
    float dinoY;
    float velocidadY;
    bool enSuelo;
    const float SALTO_INIT = -4.5f;
    const float GRAVEDAD = 0.25f;
    const float SUELO_Y = 47.0f;

    struct Obstaculo {
        float x;          // Pixel coordinate, starts strictly at 128.0f
        int ancho;        // columns (1..6)
        int tipo[6];      // 1..3
    } obstaculo;

    unsigned long tiempoUltimoMovimiento;
    unsigned long tiempoUltimoFrame;

    int puntaje;
    int puntajeMaximo;
    bool esNoche;

    #define NUM_NUBES 3
    int nubeX[NUM_NUBES];
    int nubeY[NUM_NUBES];

    #define NUM_PUNTOS 24
    int puntosX[NUM_PUNTOS];
    int puntosY[NUM_PUNTOS];

    #define MSG_COUNT 5
    const char* mensajes[MSG_COUNT] = {
        "GAME OVER",
        "YOU LOSE",
        "RETIRATE",
        "TU NO SILVE",
        "LONG_TWO_LINE"
    };
    int colaIndices[MSG_COUNT];
    int colaPos;
    int ultimoMensajeIdx;

    int solX;
    int lunaX;
    const int SOL_Y = 12;
    const int SOL_RADIO = 5;

    bool inGameOverScreen;
    uint32_t gameOverTimer;
    int currentMsgIdx;
    bool newHighScoreNotified;

    void init() {
        dinoY = SUELO_Y;
        velocidadY = 0.0f;
        enSuelo = true;
        puntaje = 0;
        esNoche = false;
        solX = 105;
        lunaX = 105;
        inGameOverScreen = false;
        ultimoMensajeIdx = -1;
        newHighScoreNotified = false;

        puntajeMaximo = 0;
        EEPROM.get(22, puntajeMaximo);
        if (puntajeMaximo == 0xFFFF) puntajeMaximo = 0;

        for (int i = 0; i < NUM_PUNTOS; i++) {
            puntosX[i] = random(0, 128);
            puntosY[i] = 58 + random(0, 4);
        }
        nubeX[0] = 40; nubeY[0] = 20;
        nubeX[1] = 90; nubeY[1] = 26;
        nubeX[2] = 140; nubeY[2] = 22;

        inicializarColaMensajes();
        generarObstaculo();
        tiempoUltimoMovimiento = millis();
        tiempoUltimoFrame = millis();
    }

    void inicializarColaMensajes() {
        for (int i = 0; i < MSG_COUNT; i++) colaIndices[i] = i;
        for (int i = MSG_COUNT - 1; i > 0; i--) {
            int j = random(0, i + 1);
            int tmp = colaIndices[i];
            colaIndices[i] = colaIndices[j];
            colaIndices[j] = tmp;
        }
        if (ultimoMensajeIdx >= 0 && colaIndices[0] == ultimoMensajeIdx && MSG_COUNT > 1) {
            int tmp = colaIndices[0];
            colaIndices[0] = colaIndices[1];
            colaIndices[1] = tmp;
        }
        colaPos = 0;
    }

    int obtenerSiguienteMensajeIdx() {
        if (colaPos >= MSG_COUNT) inicializarColaMensajes();
        int idx = colaIndices[colaPos++];
        if (idx == ultimoMensajeIdx) {
            if (colaPos < MSG_COUNT) idx = colaIndices[colaPos++];
            else {
                inicializarColaMensajes();
                idx = colaIndices[colaPos++];
            }
        }
        ultimoMensajeIdx = idx;
        return idx;
    }

    void generarObstaculo() {
        obstaculo.x = 128.0f; // Obligatoriamente fuera del borde derecho de la pantalla
        bool esSimple = random(0, 10) > map(puntaje, 0, 15, 3, 8);
        if (esSimple) {
            int ancho = random(1, 4);
            obstaculo.ancho = ancho;
            for (int i = 0; i < ancho; i++) obstaculo.tipo[i] = random(1, 4);
        } else {
            int patrones = random(0, 5);
            obstaculo.ancho = 6;
            for (int i = 0; i < 6; i++) obstaculo.tipo[i] = 0;
            if (patrones == 0) { obstaculo.tipo[0] = 2; obstaculo.tipo[4] = 2; }
            else if (patrones == 1) { obstaculo.tipo[0] = 1; obstaculo.tipo[4] = 3; obstaculo.tipo[5] = 3; }
            else if (patrones == 2) { obstaculo.tipo[0] = 3; obstaculo.tipo[1] = 3; obstaculo.tipo[5] = 1; }
            else if (patrones == 3) { obstaculo.tipo[0] = 2; obstaculo.tipo[4] = 2; obstaculo.tipo[5] = 2; }
            else if (patrones == 4) { obstaculo.tipo[0] = 1; obstaculo.tipo[4] = 1; }
        }
    }

    void moverObstaculo() {
        obstaculo.x -= 1.0f; // Desplazamiento progresivo pixel a pixel hacia la izquierda
        if (obstaculo.x + (obstaculo.ancho * 6) < 0) {
            puntaje++;
            if (puntaje > puntajeMaximo) {
                if (!newHighScoreNotified) {
                    newHighScoreNotified = true;
                    triggerRgbLed(0, 255, 0, 1000); // GREEN on beating record
                }
                puntajeMaximo = puntaje;
                EEPROM.put(22, puntajeMaximo);
                sound.tone(1200, 150);
            }
            generarObstaculo();
        }
    }

    void update() {
        yield();
        if (inGameOverScreen) {
            if (millis() - gameOverTimer > 1800 || arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON)) {
                init();
            }
            return;
        }

        unsigned long ahora = millis();
        // Velocidad adaptativa suave pixel a pixel
        unsigned long stepInterval = max(9UL, 20UL - (unsigned long)(min(30, puntaje) * 0.35f));

        if (ahora - tiempoUltimoMovimiento >= stepInterval) {
            moverObstaculo();
            tiempoUltimoMovimiento = ahora;
        }

        // Jump physics & Button reading (B3 / B4 / D-Pad jump)
        if ((arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) ||
             arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) && enSuelo) {
            velocidadY = SALTO_INIT;
            enSuelo = false;
            sound.tone(1000, 60);
        }

        if (!enSuelo) {
            velocidadY += GRAVEDAD;
            dinoY += velocidadY;
            if (dinoY >= SUELO_Y) {
                dinoY = SUELO_Y;
                velocidadY = 0.0f;
                enSuelo = true;
            }
        }

        // Clouds movement
        for (int i = 0; i < NUM_NUBES; i++) {
            nubeX[i] -= 1;
            if (nubeX[i] < -24) {
                nubeX[i] = 128 + random(10, 40);
                nubeY[i] = random(18, 30);
            }
        }

        // Day/Night switch
        esNoche = (puntaje >= 3);

        // Strict Collision Detection (Dino at X = 16)
        int dinoX = 16;
        int dinoAncho = 5;
        int dinoAlto = 8;
        int currentDinoY = (int)dinoY;

        for (int i = 0; i < obstaculo.ancho; i++) {
            if (obstaculo.tipo[i] != 0) {
                int obsX = (int)obstaculo.x + (i * 6);
                int obsY = 47;
                int obsAncho = 5;
                int obsAlto = 8;

                bool colisionX = (dinoX < obsX + obsAncho) && (dinoX + dinoAncho > obsX);
                bool colisionY = (currentDinoY < obsY + obsAlto) && (currentDinoY + dinoAlto > obsY);

                if (colisionX && colisionY) {
                    sound.tone(300, 200);
                    currentMsgIdx = obtenerSiguienteMensajeIdx();
                    inGameOverScreen = true;
                    gameOverTimer = millis();
                    return;
                }
            }
        }
    }

    void draw() {
        if (inGameOverScreen) {
            if (strcmp(mensajes[currentMsgIdx], "LONG_TWO_LINE") == 0) {
                arduboy.setCursor(14, 20); arduboy.print("JAJAJA, JUMPMAN?");
                arduboy.setCursor(6, 32); arduboy.print("SHITMAN SOUND BETTER");
            } else {
                arduboy.setCursor(34, 26); arduboy.print(mensajes[currentMsgIdx]);
            }
            return;
        }

        // Horizon Ground Line
        arduboy.drawFastHLine(0, 55, 128, WHITE);

        // Sand Particles
        for (int i = 0; i < NUM_PUNTOS; i++) {
            arduboy.drawPixel(puntosX[i], puntosY[i], WHITE);
        }

        // Clouds
        for (int i = 0; i < NUM_NUBES; i++) {
            int nx = nubeX[i];
            int ny = nubeY[i];
            arduboy.drawCircle(nx, ny, 3, WHITE);
            arduboy.drawCircle(nx + 4, ny + 1, 4, WHITE);
            arduboy.drawCircle(nx + 8, ny, 3, WHITE);
        }

        // Sun / Moon
        if (!esNoche) {
            arduboy.fillCircle(solX, SOL_Y, SOL_RADIO, WHITE);
        } else {
            arduboy.fillCircle(lunaX, SOL_Y, SOL_RADIO, WHITE);
            arduboy.fillCircle(lunaX + 4, SOL_Y, SOL_RADIO, BLACK);
        }

        // Dino Sprite at X = 16
        const uint8_t* spriteActual = jm_dinoChar;
        if (!enSuelo) {
            if (velocidadY < -1.5f) spriteActual = jm_saltoPico;
            else if (velocidadY > 1.5f) spriteActual = jm_saltoBajada;
            else spriteActual = jm_saltoInicio;
        }
        draw5x8Sprite(16, (int16_t)dinoY, spriteActual, WHITE);

        // Obstacles (Smooth pixel rendering across the 128px screen)
        for (int i = 0; i < obstaculo.ancho; i++) {
            if (obstaculo.tipo[i] != 0) {
                int16_t ox = (int16_t)obstaculo.x + (i * 6);
                if (ox >= -6 && ox < 128) {
                    int ot = obstaculo.tipo[i];
                    if (ot == 1) draw5x8Sprite(ox, 47, jm_obstaculo1, WHITE);
                    else if (ot == 2) draw5x8Sprite(ox, 47, jm_obstaculo2, WHITE);
                    else if (ot == 3) draw5x8Sprite(ox, 47, jm_obstaculo3, WHITE);
                }
            }
        }

        // HUD: M:000 P:000
        arduboy.setCursor(2, 2);
        arduboy.print("M:");
        if (puntajeMaximo < 10) arduboy.print("00");
        else if (puntajeMaximo < 100) arduboy.print("0");
        arduboy.print(puntajeMaximo);
        arduboy.print(" P:");
        if (puntaje < 10) arduboy.print("00");
        else if (puntaje < 100) arduboy.print("0");
        arduboy.print(puntaje);
    }
} jumpMan;

// =============================================================================
// GAME 9: 2048 (Classic 4x4 Sliding Puzzle with NVS Persistence & Sound)
// =============================================================================
const uint16_t microDigits2048[11] = {
    0b111101101101111, // 0
    0b010010010010010, // 1
    0b111001111100111, // 2
    0b111001111001111, // 3
    0b101101111001001, // 4
    0b111100111001111, // 5
    0b111100111101111, // 6
    0b111001001001001, // 7
    0b111101111101111, // 8
    0b111101111001111, // 9
    0b101110110101101  // k
};

void drawMicroChar2048(int16_t x, int16_t y, char c, uint8_t color) {
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

void drawTileNumber2048(int16_t cellX, int16_t cellY, uint16_t val, uint8_t color) {
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
        highScore = 0;
        gameOver = false;
        won2048 = false;
        winBannerTimer = 0;

        Preferences prefs;
        prefs.begin("game2048", true);
        highScore = prefs.getUInt("2048_hi", 0);
        prefs.end();

        spawnTile();
        spawnTile();
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
            Preferences prefs;
            prefs.begin("game2048", false);
            prefs.putUInt("2048_hi", highScore);
            prefs.end();
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

        // Botón 1 (GPIO 14): Deslizar ARRIBA
        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
            moveUp();
        }
        // Botón 2 (GPIO 0): Deslizar ABAJO
        else if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
            moveDown();
        }
        // Botón 3 (GPIO 7): Deslizar DERECHA (A_BUTTON)
        else if (arduboy.justPressed(A_BUTTON)) {
            moveRight();
        }
        // Botón 4 (GPIO 18): Deslizar IZQUIERDA (B_BUTTON)
        else if (arduboy.justPressed(B_BUTTON)) {
            moveLeft();
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
} game2048;

// =============================================================================
// GAME 10: T-REX RUNNER (Chrome Dino Port with AABB Physics & Pterodactyls)
// =============================================================================
void draw1BitBitmap(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint8_t* bmp, uint8_t color = WHITE) {
    uint8_t byteWidth = (w + 7) / 8;
    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t col = 0; col < w; col++) {
            uint8_t byte = bmp[row * byteWidth + (col / 8)];
            if (byte & (0x80 >> (col % 8))) {
                arduboy.drawPixel(x + col, y + row, color);
            }
        }
    }
}

// 14x14 Dino Run 1
const uint8_t PROGMEM trex_run1[28] = {
    0b00000001, 0b11110000,
    0b00000001, 0b01111100,
    0b00000001, 0b11111100,
    0b00000001, 0b11100000,
    0b00000001, 0b11111000,
    0b00010001, 0b11110000,
    0b00011011, 0b11111100,
    0b00011111, 0b11111000,
    0b00001111, 0b11110000,
    0b00000111, 0b11100000,
    0b00000011, 0b11000000,
    0b00000001, 0b11000000,
    0b00000001, 0b01000000,
    0b00000001, 0b00000000
};

// 14x14 Dino Run 2
const uint8_t PROGMEM trex_run2[28] = {
    0b00000001, 0b11110000,
    0b00000001, 0b01111100,
    0b00000001, 0b11111100,
    0b00000001, 0b11100000,
    0b00000001, 0b11111000,
    0b00010001, 0b11110000,
    0b00011011, 0b11111100,
    0b00011111, 0b11111000,
    0b00001111, 0b11110000,
    0b00000111, 0b11100000,
    0b00000011, 0b11000000,
    0b00000001, 0b11000000,
    0b00000000, 0b11000000,
    0b00000000, 0b10000000
};

// 14x14 Dino Dead (Crash Eye X)
const uint8_t PROGMEM trex_dead[28] = {
    0b00000001, 0b11110000,
    0b00000001, 0b10111100,
    0b00000001, 0b11111100,
    0b00000001, 0b11100000,
    0b00000001, 0b11111000,
    0b00010001, 0b11110000,
    0b00011011, 0b11111100,
    0b00011111, 0b11111000,
    0b00001111, 0b11110000,
    0b00000111, 0b11100000,
    0b00000011, 0b11000000,
    0b00000001, 0b11000000,
    0b00000001, 0b01000000,
    0b00000001, 0b01000000
};

// 18x9 Dino Duck 1 (Agachado)
const uint8_t PROGMEM trex_duck1[27] = {
    0b00000000, 0b01111111, 0b11000000,
    0b00000000, 0b01011111, 0b11000000,
    0b00011111, 0b11111111, 0b11000000,
    0b00111111, 0b11111111, 0b00000000,
    0b00011111, 0b11111110, 0b00000000,
    0b00000111, 0b11111100, 0b00000000,
    0b00000011, 0b11111000, 0b00000000,
    0b00000001, 0b00100000, 0b00000000,
    0b00000001, 0b00000000, 0b00000000
};

// 18x9 Dino Duck 2 (Agachado)
const uint8_t PROGMEM trex_duck2[27] = {
    0b00000000, 0b01111111, 0b11000000,
    0b00000000, 0b01011111, 0b11000000,
    0b00011111, 0b11111111, 0b11000000,
    0b00111111, 0b11111111, 0b00000000,
    0b00011111, 0b11111110, 0b00000000,
    0b00000111, 0b11111100, 0b00000000,
    0b00000011, 0b11111000, 0b00000000,
    0b00000000, 0b11000000, 0b00000000,
    0b00000000, 0b10000000, 0b00000000
};

// 6x12 Cactus Small
const uint8_t PROGMEM cactus_small[12] = {
    0b00110000,
    0b00110000,
    0b10110000,
    0b10110100,
    0b10110100,
    0b11111100,
    0b00110100,
    0b00110100,
    0b00111100,
    0b00110000,
    0b00110000,
    0b00110000
};

// 12x14 Cactus Large / Cluster
const uint8_t PROGMEM cactus_large[28] = {
    0b00011000, 0b00000000,
    0b00011000, 0b11000000,
    0b01011000, 0b11000000,
    0b01011010, 0b11000000,
    0b01011010, 0b11010000,
    0b01111110, 0b11010000,
    0b00011010, 0b11111000,
    0b00011010, 0b11010000,
    0b00011110, 0b11010000,
    0b00011000, 0b11110000,
    0b00011000, 0b11000000,
    0b00011000, 0b11000000,
    0b00011000, 0b11000000,
    0b00011000, 0b11000000
};

// 14x10 Pterodactyl Wing Up
const uint8_t PROGMEM ptero_up[20] = {
    0b00000110, 0b00000000,
    0b00001111, 0b00000000,
    0b00011111, 0b10000000,
    0b00111111, 0b11000000,
    0b11111111, 0b11110000,
    0b00001111, 0b11111100,
    0b00000011, 0b11110000,
    0b00000001, 0b11000000,
    0b00000001, 0b00000000,
    0b00000000, 0b00000000
};

// 14x10 Pterodactyl Wing Down
const uint8_t PROGMEM ptero_down[20] = {
    0b00000000, 0b00000000,
    0b00000001, 0b00000000,
    0b00000011, 0b11000000,
    0b00000011, 0b11110000,
    0b00001111, 0b11111100,
    0b11111111, 0b11110000,
    0b00111111, 0b11000000,
    0b00011111, 0b10000000,
    0b00001111, 0b00000000,
    0b00000110, 0b00000000
};

// 14x5 Cloud
const uint8_t PROGMEM trex_cloud[10] = {
    0b00001110, 0b00000000,
    0b00111111, 0b10000000,
    0b01111111, 0b11100000,
    0b11111111, 0b11110000,
    0b01111111, 0b11100000
};

struct TRexGame {
    float dinoY;
    float dinoVY;
    bool onGround;
    bool isDucking;
    bool gameOver;
    bool isNewRecord;

    uint32_t score;
    uint32_t highScore;
    uint8_t scoreCounter;
    uint16_t lastMilestoneScore;
    uint8_t milestoneBeepTimer;

    float gameSpeed;
    float groundScrollX;

    struct Cloud {
        float x, y;
        float speed;
        bool active;
    } clouds[3];

    enum ObstacleType {
        OBS_CACTUS_SMALL = 0,
        OBS_CACTUS_LARGE,
        OBS_PTERO_LOW,
        OBS_PTERO_HIGH
    };

    struct Obstacle {
        float x, y;
        uint8_t type;
        uint8_t w, h;
        bool active;
    } obstacles[3];

    float distanceToNextSpawn;
    uint8_t animFrame;
    uint8_t animTimer;

    void init() {
        melodyPlayer.stop();
        dinoY = 40.0f;
        dinoVY = 0.0f;
        onGround = true;
        isDucking = false;
        gameOver = false;
        isNewRecord = false;
        score = 0;
        scoreCounter = 0;
        lastMilestoneScore = 0;
        milestoneBeepTimer = 0;
        gameSpeed = 2.4f;
        groundScrollX = 0.0f;
        distanceToNextSpawn = 70.0f;
        animFrame = 0;
        animTimer = 0;

        // Cargar High Score desde NVS
        Preferences prefs;
        prefs.begin("trex", true);
        highScore = prefs.getUInt("trex_hi", 0);
        prefs.end();

        // Inicializar nubes
        clouds[0] = {40.0f, 12.0f, 0.5f, true};
        clouds[1] = {90.0f, 18.0f, 0.4f, true};
        clouds[2] = {140.0f, 10.0f, 0.6f, true};

        // Limpiar obstáculos
        for (int i = 0; i < 3; i++) {
            obstacles[i].active = false;
        }
    }

    void spawnObstacle() {
        for (int i = 0; i < 3; i++) {
            if (!obstacles[i].active) {
                obstacles[i].active = true;
                obstacles[i].x = 128.0f;

                uint8_t r = random(0, 100);
                if (score > 150 && r < 35) {
                    // Pterodáctilo
                    if (r < 18) {
                        obstacles[i].type = OBS_PTERO_LOW;
                        obstacles[i].y = 35.0f; // Vuelo bajo: agacharse o saltar
                    } else {
                        obstacles[i].type = OBS_PTERO_HIGH;
                        obstacles[i].y = 22.0f; // Vuelo alto: pasar por debajo
                    }
                    obstacles[i].w = 14;
                    obstacles[i].h = 10;
                } else if (r < 70) {
                    // Cactus Pequeño
                    obstacles[i].type = OBS_CACTUS_SMALL;
                    obstacles[i].y = 42.0f;
                    obstacles[i].w = 6;
                    obstacles[i].h = 12;
                } else {
                    // Cactus Grande / Doble
                    obstacles[i].type = OBS_CACTUS_LARGE;
                    obstacles[i].y = 40.0f;
                    obstacles[i].w = 12;
                    obstacles[i].h = 14;
                }
                break;
            }
        }
        distanceToNextSpawn = 65.0f + (float)random(0, 45) + (gameSpeed * 8.0f);
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

        // --- Controles ---
        // Salto: Botón 1 (UP / LEFT)
        if ((arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) && onGround) {
            dinoVY = -4.3f;
            onGround = false;
            sound.tone(800, 30);
        }

        // Agacharse: Botón 2 (DOWN / RIGHT)
        isDucking = (arduboy.pressed(DOWN_BUTTON) || arduboy.pressed(RIGHT_BUTTON));
        if (isDucking && !onGround) {
            // Caída rápida
            dinoVY += 0.4f;
        }

        // --- Físicas y Gravedad Parabólica ---
        if (!onGround) {
            dinoVY += 0.32f;
            dinoY += dinoVY;
            if (dinoY >= 40.0f) {
                dinoY = 40.0f;
                dinoVY = 0.0f;
                onGround = true;
            }
        }

        // --- Escalado Progresivo de Velocidad ---
        gameSpeed = 2.4f + min(3.4f, (float)score * 0.0025f);

        // --- Animación ---
        animTimer++;
        if (animTimer >= (uint8_t)max(3, 7 - (int)(gameSpeed))) {
            animTimer = 0;
            animFrame = !animFrame;
        }

        // --- Puntuación & Hito de 100 Puntos ---
        scoreCounter++;
        if (scoreCounter >= 5) {
            scoreCounter = 0;
            score++;

            if (score > 0 && (score % 100) == 0 && score != lastMilestoneScore) {
                lastMilestoneScore = score;
                milestoneBeepTimer = 1;
            }
        }

        if (milestoneBeepTimer == 1) {
            sound.tone(1200, 45);
            milestoneBeepTimer = 2;
        } else if (milestoneBeepTimer >= 2 && milestoneBeepTimer < 6) {
            milestoneBeepTimer++;
        } else if (milestoneBeepTimer == 6) {
            sound.tone(1600, 60);
            milestoneBeepTimer = 0;
        }

        // --- Desplazamiento del Suelo ---
        groundScrollX += gameSpeed;
        if (groundScrollX >= 128.0f) groundScrollX -= 128.0f;

        // --- Nubes de Fondo ---
        for (int i = 0; i < 3; i++) {
            if (clouds[i].active) {
                clouds[i].x -= clouds[i].speed;
                if (clouds[i].x < -20.0f) {
                    clouds[i].x = 135.0f + (float)random(0, 30);
                    clouds[i].y = 8.0f + (float)random(0, 16);
                }
            }
        }

        // --- Obstáculos ---
        distanceToNextSpawn -= gameSpeed;
        if (distanceToNextSpawn <= 0) {
            spawnObstacle();
        }

        // Hitbox del jugador
        float pBoxX, pBoxY, pBoxW, pBoxH;
        if (isDucking && onGround) {
            pBoxX = 13.0f;
            pBoxY = 46.0f;
            pBoxW = 15.0f;
            pBoxH = 7.0f;
        } else {
            pBoxX = 14.0f;
            pBoxY = dinoY + 2.0f;
            pBoxW = 10.0f;
            pBoxH = 11.0f;
        }

        for (int i = 0; i < 3; i++) {
            if (obstacles[i].active) {
                obstacles[i].x -= gameSpeed;
                if (obstacles[i].x < -16.0f) {
                    obstacles[i].active = false;
                    continue;
                }

                // Hitbox del obstáculo
                float oBoxX = obstacles[i].x + 1.0f;
                float oBoxY = obstacles[i].y + 1.0f;
                float oBoxW = obstacles[i].w - 2.0f;
                float oBoxH = obstacles[i].h - 2.0f;

                // Detección de Colisión AABB
                if (pBoxX < oBoxX + oBoxW && pBoxX + pBoxW > oBoxX &&
                    pBoxY < oBoxY + oBoxH && pBoxY + pBoxH > oBoxY) {
                    gameOver = true;
                    sound.tone(250, 100);

                    if (score > highScore) {
                        highScore = score;
                        Preferences prefs;
                        prefs.begin("trex", false);
                        prefs.putUInt("trex_hi", highScore);
                        prefs.end();
                        isNewRecord = true;
                        triggerRgbLed(0, 255, 0, 1000); // LED Verde 1s
                        melodyPlayer.play(pingPongVictoryNotes, 4);
                    }
                    return;
                }
            }
        }
    }

    void draw() {
        // --- Nubes ---
        for (int i = 0; i < 3; i++) {
            if (clouds[i].active) {
                draw1BitBitmap((int16_t)clouds[i].x, (int16_t)clouds[i].y, 14, 5, trex_cloud, WHITE);
            }
        }

        // --- Suelo y Textura Dinámica ---
        arduboy.drawFastHLine(0, 54, 128, WHITE);
        int16_t gOff = (int16_t)groundScrollX;
        for (int x = -16; x < 144; x += 16) {
            int16_t px = x - (gOff % 16);
            if (px >= 0 && px < 127) {
                arduboy.drawPixel(px, 57, WHITE);
                arduboy.drawPixel(px + 6, 59, WHITE);
                arduboy.drawPixel(px + 11, 56, WHITE);
            }
        }

        // --- Obstáculos ---
        for (int i = 0; i < 3; i++) {
            if (obstacles[i].active) {
                int16_t ox = (int16_t)obstacles[i].x;
                int16_t oy = (int16_t)obstacles[i].y;
                if (obstacles[i].type == OBS_CACTUS_SMALL) {
                    draw1BitBitmap(ox, oy, 6, 12, cactus_small, WHITE);
                } else if (obstacles[i].type == OBS_CACTUS_LARGE) {
                    draw1BitBitmap(ox, oy, 12, 14, cactus_large, WHITE);
                } else {
                    const uint8_t* ptSprite = animFrame ? ptero_up : ptero_down;
                    draw1BitBitmap(ox, oy, 14, 10, ptSprite, WHITE);
                }
            }
        }

        // --- Sprite del T-Rex ---
        if (gameOver) {
            draw1BitBitmap(12, (int16_t)dinoY, 14, 14, trex_dead, WHITE);
        } else if (isDucking && onGround) {
            const uint8_t* duckSprite = animFrame ? trex_duck1 : trex_duck2;
            draw1BitBitmap(12, 45, 18, 9, duckSprite, WHITE);
        } else if (!onGround) {
            draw1BitBitmap(12, (int16_t)dinoY, 14, 14, trex_run1, WHITE);
        } else {
            const uint8_t* runSprite = animFrame ? trex_run1 : trex_run2;
            draw1BitBitmap(12, (int16_t)dinoY, 14, 14, runSprite, WHITE);
        }

        // --- HUD (Superior Derecho: HI 00125  00042) ---
        arduboy.setCursor(38, 2);
        arduboy.print("HI ");
        printPadded5(highScore);
        arduboy.print(" ");
        printPadded5(score);

        // --- Pantalla Game Over ---
        if (gameOver) {
            arduboy.fillRect(14, 14, 100, 36, BLACK);
            arduboy.drawRect(14, 14, 100, 36, WHITE);
            arduboy.drawRect(16, 16, 96, 32, WHITE);

            if (isNewRecord) {
                arduboy.setCursor(20, 19);
                arduboy.print("NUEVO RECORD!");
            } else {
                arduboy.setCursor(34, 19);
                arduboy.print("GAME OVER");
            }

            arduboy.setCursor(26, 29);
            arduboy.print("SCORE: ");
            arduboy.print(score);

            arduboy.setCursor(22, 38);
            arduboy.print("Pulsa un boton");
        }
    }

    void printPadded5(uint32_t val) {
        if (val < 10) arduboy.print("0000");
        else if (val < 100) arduboy.print("000");
        else if (val < 1000) arduboy.print("00");
        else if (val < 10000) arduboy.print("0");
        arduboy.print(val);
    }
} trexRunner;

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
        // B1 (GPIO 14) -> ARRIBA / IZQUIERDA
        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
            modeCursor--;
            if (modeCursor < 0) modeCursor = 1;
            sound.tone(900, 15);
        }
        // B2 (GPIO 0) -> ABAJO / DERECHA
        else if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
            modeCursor++;
            if (modeCursor > 1) modeCursor = 0;
            sound.tone(900, 15);
        }

        // Confirmación con BOOT (GPIO 9) o Botón 3 (GPIO 7 - A_BUTTON)
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
        // B1 (GPIO 14) -> ARRIBA
        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
            diffCursor--;
            if (diffCursor < 0) diffCursor = 2;
            sound.tone(900, 15);
        }
        // B2 (GPIO 0) -> ABAJO
        else if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
            diffCursor++;
            if (diffCursor > 2) diffCursor = 0;
            sound.tone(900, 15);
        }

        // Confirmación con BOOT (GPIO 9) o Botón 3 (GPIO 7 - A_BUTTON)
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

            // Botón 1 (GPIO 14): Mover cursor ARRIBA
            if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
                if (cursorRow > 0) {
                    cursorRow--;
                    sound.tone(900, 15);
                }
            }
            // Botón 2 (GPIO 0): Mover cursor ABAJO
            else if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
                if (cursorRow < 2) {
                    cursorRow++;
                    sound.tone(900, 15);
                }
            }
            // Botón 3 (GPIO 7): Mover cursor DERECHA
            else if (arduboy.justPressed(A_BUTTON)) {
                if (cursorCol < 2) {
                    cursorCol++;
                    sound.tone(900, 15);
                }
            }
            // Botón 4 (GPIO 18): Mover cursor IZQUIERDA
            else if (arduboy.justPressed(B_BUTTON)) {
                if (cursorCol > 0) {
                    cursorCol--;
                    sound.tone(900, 15);
                }
            }

            // Selección / Colocación con botón BOOT (GPIO 9)
            if (bootActionTriggered) {
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
            // Botón 1 (GPIO 14): Mover cursor ARRIBA
            if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
                if (cursorRow > 0) {
                    cursorRow--;
                    sound.tone(900, 15);
                }
            }
            // Botón 2 (GPIO 0): Mover cursor ABAJO
            else if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
                if (cursorRow < 2) {
                    cursorRow++;
                    sound.tone(900, 15);
                }
            }
            // Botón 3 (GPIO 7): Mover cursor DERECHA
            else if (arduboy.justPressed(A_BUTTON)) {
                if (cursorCol < 2) {
                    cursorCol++;
                    sound.tone(900, 15);
                }
            }
            // Botón 4 (GPIO 18): Mover cursor IZQUIERDA
            else if (arduboy.justPressed(B_BUTTON)) {
                if (cursorCol > 0) {
                    cursorCol--;
                    sound.tone(900, 15);
                }
            }

            // Selección / Colocación con botón BOOT (GPIO 9)
            if (bootActionTriggered) {
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
} ticTacToe;

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
                if (arduboy.justPressed(UP_BUTTON)) { // Botón 1 (GPIO 14) -> ARRIBA exclusivamente
                    if (menuRow > 0) menuRow--;
                    else menuRow = 2;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
                } else if (arduboy.justPressed(DOWN_BUTTON)) { // Botón 2 (GPIO 0) -> ABAJO exclusivamente
                    if (menuRow < 2) menuRow++;
                    else menuRow = 0;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
                } else if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) { // Botón 3 (GPIO 7) -> IZQUIERDA exclusivamente
                    if (menuCol > 0) menuCol--;
                    else menuCol = 2;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
                } else if (arduboy.justPressed(B_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) { // Botón 4 (GPIO 18) -> DERECHA exclusivamente
                    if (menuCol < 2) menuCol++;
                    else menuCol = 0;
                    select = menuRow * 3 + menuCol;
                    sound.tone(5000, 10);
                    lastNavTime = millis();
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
                if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) { // B3 (GPIO 7 - Opción izquierda)
                    if (mathOpt1Correct) handleCorrect();
                    else handleWrong();
                } else if (arduboy.justPressed(B_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) { // B4 (GPIO 18 - Opción derecha)
                    if (!mathOpt1Correct) handleCorrect();
                    else handleWrong();
                }
            } else if (curAct == 1 || curAct == 6) { // Arrow match
                int8_t pressedDir = -1;
                if (arduboy.justPressed(UP_BUTTON)) pressedDir = 0;
                else if (arduboy.justPressed(DOWN_BUTTON)) pressedDir = 1;
                else if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) pressedDir = 2;
                else if (arduboy.justPressed(B_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) pressedDir = 3;

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
                if (arduboy.justPressed(UP_BUTTON)) { // B1: Mayor
                    if (hlNext > hlCurrent) {
                        hlCurrent = hlNext;
                        handleCorrect();
                    } else handleWrong();
                } else if (arduboy.justPressed(DOWN_BUTTON)) { // B2: Menor
                    if (hlNext < hlCurrent) {
                        hlCurrent = hlNext;
                        handleCorrect();
                    } else handleWrong();
                }
            } else if (curAct == 3) { // Odd shape out
                int8_t pressedPos = -1;
                if (arduboy.justPressed(UP_BUTTON)) pressedPos = 0;
                else if (arduboy.justPressed(DOWN_BUTTON)) pressedPos = 1;
                else if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) pressedPos = 2;
                else if (arduboy.justPressed(B_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) pressedPos = 3;

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
                    if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) { // B3 (GPIO 7)
                        if (countOpt1Correct) handleCorrect();
                        else handleWrong();
                    } else if (arduboy.justPressed(B_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) { // B4 (GPIO 18)
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
                    else if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) pressedDir = 2;
                    else if (arduboy.justPressed(B_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) pressedDir = 3;

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
} brainduTrain;

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
    uint16_t wins;
    uint16_t losses;
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
        randomSeed(analogRead(1) ^ millis() ^ esp_random());
        if (soundEnabled) {
            sound.tone(880, 40);
        }
    }

    void onBootShortPress() {
        bootActionTriggered = true;
    }

    void loadStats() {
        uint8_t h1 = EEPROM.read(50);
        uint8_t h2 = EEPROM.read(51);
        if (h1 != 'H' || h2 != 'M') {
            EEPROM.write(50, 'H');
            EEPROM.write(51, 'M');
            wins = 0;
            losses = 0;
            soundEnabled = 1;
            saveStats();
        } else {
            EEPROM.get(52, wins);
            EEPROM.get(54, losses);
            soundEnabled = EEPROM.read(56);
            if (soundEnabled > 1) soundEnabled = 1;
        }
    }

    void saveStats() {
        EEPROM.write(50, 'H');
        EEPROM.write(51, 'M');
        EEPROM.put(52, wins);
        EEPROM.put(54, losses);
        EEPROM.write(56, soundEnabled);
        EEPROM.commit();
    }

    void resetStats() {
        wins = 0;
        losses = 0;
        saveStats();
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
        saveStats();
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
        randomSeed(analogRead(1) ^ millis() ^ esp_random());
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
            saveStats();
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
                saveStats();
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
        // Botón 3 (GPIO 7): Mover cursor IZQUIERDA
        if (arduboy.justPressed(A_BUTTON)) {
            if (cursor == 0) cursor = 25;
            else cursor--;
            playTone(900, 12);
        }
        // Botón 4 (GPIO 18): Mover cursor DERECHA
        if (arduboy.justPressed(B_BUTTON)) {
            if (cursor == 25) cursor = 0;
            else cursor++;
            playTone(900, 12);
        }

        // Confirmación de Letra: Botón BOOT (GPIO 9)
        if (bootActionTriggered) {
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
} hangmanGame;

// =============================================================================
// =============================================================================
// =============================================================================
// =============================================================================
// =============================================================================
// TRIS (OBONO ARDUBOY TRIS PORT - PIXEL-PERFECT 1:1 TO SCREENSHOTS)
// =============================================================================
#define TRIS_STATE_TITLE     0
#define TRIS_STATE_PLAYING   1
#define TRIS_STATE_FLASH     2
#define TRIS_STATE_GAMEOVER  3

// Pixel-perfect Title Screen (1024 bytes, Row Format for drawBitmap)
const uint8_t tris_title_bitmap[1024] PROGMEM = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x16,0xAA,0xAB,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,0x55,0x56,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xAA,0xB8,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x03,0xFF,0x80,0x00,0x01,0x55,0x60,0x00,0x00,0x00,0x40,0x00,
0x00,0x00,0x00,0x00,0x07,0xFF,0x80,0x00,0x02,0xAB,0x80,0x00,0x00,0x00,0x43,0x00,
0x00,0x00,0x00,0x00,0x0F,0xFF,0xC0,0x00,0x01,0x56,0x00,0x00,0x00,0x01,0x44,0x80,
0x00,0x00,0x00,0x00,0x3F,0xFD,0xC0,0x00,0x00,0x60,0x1F,0x00,0x00,0x00,0x43,0x00,
0x00,0x00,0x00,0x00,0x7F,0xFA,0x60,0x00,0x00,0x00,0x28,0x80,0x00,0x00,0x40,0x00,
0x00,0x00,0x00,0x00,0xFF,0xF5,0xA0,0x00,0x00,0x00,0x44,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x01,0xFF,0xEA,0x00,0x00,0x00,0x00,0x82,0x20,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x03,0xFF,0xD5,0x7F,0xFF,0xFF,0xF1,0x01,0xF0,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x02,0x00,0x6A,0x80,0x00,0x00,0x08,0x82,0x20,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x01,0x00,0x35,0xBF,0xD7,0x2A,0xE8,0x44,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x01,0x00,0x2A,0xB6,0xD5,0xB4,0x68,0x28,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x80,0x15,0xA6,0x55,0xB3,0x28,0x1F,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x80,0x1A,0x96,0x97,0x37,0x8B,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x40,0x15,0xB6,0xD0,0xB1,0xCA,0xC0,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x40,0x0A,0xB6,0xD5,0xB6,0xE9,0x70,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x20,0x0D,0xB6,0xD6,0xB5,0x6A,0xAC,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x20,0x07,0xBA,0xD7,0x16,0xD9,0x57,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x10,0x06,0x80,0x08,0xC0,0x0B,0xAA,0xC0,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x1F,0xFC,0x7F,0xCF,0xCF,0xF3,0xD7,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2F,0x50,0x07,0xF8,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2E,0xD7,0xFF,0xC0,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2F,0x57,0xFF,0xC1,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2E,0xD7,0xFF,0x81,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0xFC,0x2D,0x57,0xFF,0x02,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x74,0x2A,0x97,0xFF,0x02,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x94,0x25,0x57,0xFE,0x04,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x12,0x2A,0x97,0xFE,0x04,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x01,0x0A,0x20,0x13,0xFC,0x18,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x1F,0xE1,0xF8,0xE0,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x00,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0xF6,0x00,0x00,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x73,0xBD,0xA3,0xDC,0xEE,0x7A,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x6B,0xB1,0xA3,0x0D,0xAD,0x33,0xC0,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x73,0xBD,0xE3,0xCD,0xEE,0x35,0x40,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x63,0x85,0xA0,0x4D,0xAD,0x32,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x23,0xAA,0xA0,0x04,0xD5,0x55,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0xF1,0x55,0x50,0x04,0xAA,0xAB,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x0F,0x11,0xAA,0xB0,0x05,0x55,0x55,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x09,0xC8,0x55,0x60,0x09,0xAA,0xAB,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x08,0x38,0xAB,0x80,0x09,0x55,0x55,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x08,0x14,0xD6,0x00,0x09,0xAA,0xAA,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x10,0x17,0xB8,0x00,0x0B,0x55,0x56,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x10,0x11,0xE0,0x00,0x13,0xAA,0xAA,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x10,0x11,0x00,0x00,0x1C,0x7D,0x56,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x10,0x21,0x00,0x00,0x1C,0x03,0xEC,0x00,0x00,0x00,0x00,0x00,
0x24,0x44,0x44,0x48,0x98,0x21,0x11,0x11,0x13,0xC0,0x1E,0x24,0x44,0x44,0x08,0x80,
0x00,0x00,0x00,0x00,0x07,0xA6,0x00,0x00,0x00,0x3F,0x18,0x00,0x00,0x00,0x00,0x00,
0x2D,0x55,0x55,0x4A,0xAA,0xFA,0x55,0x55,0x56,0xAA,0xEA,0xA5,0x55,0x55,0x2A,0xA0,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x2D,0x55,0x55,0x4A,0xAA,0xAA,0x55,0x55,0x56,0xAA,0xAA,0xA5,0x55,0x55,0x2A,0xA0,
0x12,0x22,0x22,0x24,0x44,0x44,0x88,0x88,0x89,0x11,0x11,0x12,0x22,0x22,0x44,0x40,
0x2D,0x55,0x55,0x4A,0xAA,0xAA,0x55,0x55,0x56,0xAA,0xAA,0xA5,0x55,0x55,0x2A,0xA0,
0x12,0xAA,0xAA,0xB5,0x55,0x55,0xAA,0xAA,0xA9,0x55,0x55,0x5A,0xAA,0xAA,0xD5,0x50,
};

// Pixel-perfect In-Game HUD Background (1024 bytes, Row Format for drawBitmap)
const uint8_t tris_hud_bitmap[1024] PROGMEM = {
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x3F,0xFF,0xF8,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x0F,0xFF,0xFE,0x00,
0x80,0x00,0x40,0x00,0x04,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x10,0x00,0x01,0x00,
0x80,0x00,0xBF,0xFF,0xFA,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x2F,0xFF,0xFE,0x80,
0x80,0x01,0x49,0xB3,0x1C,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5D,0x45,0x47,0x40,
0x80,0x01,0x5B,0x55,0x3C,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5C,0x4E,0xEF,0x40,
0x80,0x01,0x6B,0x53,0x7C,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5D,0x5E,0xEF,0x40,
0x80,0x01,0x49,0xB5,0x1C,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5D,0x45,0x6F,0x40,
0x80,0x00,0xBF,0xFF,0xFA,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x2F,0xFF,0xFE,0x80,
0x80,0x00,0x40,0x00,0x04,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x10,0x00,0x01,0x00,
0x80,0x00,0x3F,0xFF,0xF8,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x0F,0xFF,0xFE,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x81,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xF9,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x83,0xFF,0xFF,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0x00,0x00,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x83,0xFF,0xFF,0xFF,0xFF,0xF9,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x81,0xFF,0xFF,0xFF,0xFF,0xFD,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x80,0x03,0xFA,0xE4,0x6E,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x81,0x53,0x5A,0xB6,0xC7,0x55,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x80,0xAA,0x4A,0xB6,0x72,0xA9,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x81,0x55,0x42,0xE6,0xB9,0x55,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x80,0xAA,0x52,0x16,0x5C,0xA9,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0xFF,0xFE,0x80,0x00,
0x81,0x55,0x4A,0xA6,0xAF,0x55,0x7F,0xFF,0xFF,0xFF,0xFE,0x82,0x00,0x00,0x80,0x00,
0x80,0xAA,0x52,0xD6,0xD6,0xA9,0x7F,0xFF,0xFF,0xFF,0xFE,0x83,0xFF,0xFF,0x80,0x00,
0x80,0x00,0x02,0xE2,0xEC,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x7F,0xFF,0xF0,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x0F,0xFF,0xFE,0x00,
0x80,0x00,0x80,0x00,0x08,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x10,0x00,0x01,0x00,
0x80,0x01,0x7F,0xFF,0xF4,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x2F,0xFF,0xFE,0x80,
0x80,0x02,0xF1,0x51,0x7A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5B,0x5A,0x23,0x40,
0x80,0x02,0xF3,0x53,0x7A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5B,0x4A,0x6F,0x40,
0x80,0x02,0xF7,0x57,0x7A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5B,0x52,0xF3,0x40,
0x80,0x02,0xD1,0xB1,0x3A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x58,0x5A,0x23,0x40,
0x80,0x02,0xFF,0xFF,0xFA,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x5F,0xFF,0xFF,0x40,
0x80,0x02,0xC0,0x00,0x3A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x03,0xC3,0x40,
0x80,0x02,0xC0,0x00,0x3A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x03,0xD3,0x40,
0x80,0x02,0xC0,0x00,0x3A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x03,0xD3,0x40,
0x80,0x02,0xC0,0x00,0x3A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x03,0xD3,0x40,
0x80,0x02,0xC0,0x00,0x3A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x03,0xD3,0x40,
0x80,0x02,0xC0,0x00,0x1A,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x03,0xC3,0x40,
0x80,0x01,0x7F,0xFF,0xF4,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x2F,0xFF,0xFE,0x80,
0x80,0x00,0x80,0x00,0x08,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x10,0x00,0x01,0x00,
0x80,0x00,0x7F,0xFF,0xF0,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x0F,0xFF,0xFE,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x01,0x7F,0xFF,0xFF,0xFF,0xFE,0x80,0x00,0x00,0x00,0x00,
0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const uint8_t trisDigits4x6[10][6] PROGMEM = {
    {0b1111, 0b1001, 0b1001, 0b1001, 0b1001, 0b1111}, // 0
    {0b0110, 0b1110, 0b0110, 0b0110, 0b0110, 0b1111}, // 1
    {0b1111, 0b0001, 0b1111, 0b1000, 0b1000, 0b1111}, // 2
    {0b1111, 0b0001, 0b1111, 0b0001, 0b0001, 0b1111}, // 3
    {0b1001, 0b1001, 0b1111, 0b0001, 0b0001, 0b0001}, // 4
    {0b1111, 0b1000, 0b1111, 0b0001, 0b0001, 0b1111}, // 5
    {0b1111, 0b1000, 0b1111, 0b1001, 0b1001, 0b1111}, // 6
    {0b1111, 0b0001, 0b0010, 0b0100, 0b0100, 0b0100}, // 7
    {0b1111, 0b1001, 0b1111, 0b1001, 0b1001, 0b1111}, // 8
    {0b1111, 0b1001, 0b1111, 0b0001, 0b0001, 0b1111}  // 9
};

struct TrisGame {
    uint8_t state;
    uint8_t grid[20][10]; // 0=empty, 1..7=piece type
    uint8_t currentPiece; // 0..6
    uint8_t currentRot;   // 0..3
    int8_t currentX;
    int8_t currentY;
    uint8_t nextPiece;    // 0..6

    // 7-Bag Randomizer
    uint8_t bag[7];
    uint8_t bagIdx;

    // Game stats
    uint32_t score;
    uint32_t highScore;
    uint16_t lines;
    uint8_t level;

    // Timers & speed
    unsigned long lastFallTime;
    unsigned long fallInterval;
    unsigned long flashTimer;
    uint8_t flashCount;
    bool flashState;
    uint8_t linesToClear[4];
    uint8_t numLinesToClear;

    // Controls
    bool bootActionTriggered;

    // Piece shapes [piece][rot][block] -> (x, y)
    // 0:I, 1:O, 2:T, 3:S, 4:Z, 5:J, 6:L
    const int8_t pieceBlocks[7][4][4][2] = {
        // 0: I
        {
            {{0,1},{1,1},{2,1},{3,1}},
            {{2,0},{2,1},{2,2},{2,3}},
            {{0,2},{1,2},{2,2},{3,2}},
            {{1,0},{1,1},{1,2},{1,3}}
        },
        // 1: O
        {
            {{0,0},{1,0},{0,1},{1,1}},
            {{0,0},{1,0},{0,1},{1,1}},
            {{0,0},{1,0},{0,1},{1,1}},
            {{0,0},{1,0},{0,1},{1,1}}
        },
        // 2: T
        {
            {{1,0},{0,1},{1,1},{2,1}},
            {{1,0},{1,1},{2,1},{1,2}},
            {{0,1},{1,1},{2,1},{1,2}},
            {{1,0},{0,1},{1,1},{1,2}}
        },
        // 3: S
        {
            {{1,0},{2,0},{0,1},{1,1}},
            {{1,0},{1,1},{2,1},{2,2}},
            {{1,1},{2,1},{0,2},{1,2}},
            {{0,0},{0,1},{1,1},{1,2}}
        },
        // 4: Z
        {
            {{0,0},{1,0},{1,1},{2,1}},
            {{2,0},{1,1},{2,1},{1,2}},
            {{0,1},{1,1},{1,2},{2,2}},
            {{1,0},{0,1},{1,1},{0,2}}
        },
        // 5: J
        {
            {{0,0},{0,1},{1,1},{2,1}},
            {{1,0},{2,0},{1,1},{1,2}},
            {{0,1},{1,1},{2,1},{2,2}},
            {{1,0},{1,1},{0,2},{1,2}}
        },
        // 6: L
        {
            {{2,0},{0,1},{1,1},{2,1}},
            {{1,0},{1,1},{1,2},{2,2}},
            {{0,1},{1,1},{2,1},{0,2}},
            {{0,0},{1,0},{1,1},{1,2}}
        }
    };

    void init() {
        loadHighScore();
        state = TRIS_STATE_TITLE;
        bootActionTriggered = false;
    }

    void onBootShortPress() {
        bootActionTriggered = true;
    }

    void loadHighScore() {
        uint8_t h1 = EEPROM.read(60);
        uint8_t h2 = EEPROM.read(61);
        if (h1 != 'T' || h2 != 'R') {
            EEPROM.write(60, 'T');
            EEPROM.write(61, 'R');
            highScore = 0;
            EEPROM.put(62, highScore);
            EEPROM.commit();
        } else {
            EEPROM.get(62, highScore);
        }
    }

    void saveHighScore() {
        if (score > highScore) {
            highScore = score;
            EEPROM.write(60, 'T');
            EEPROM.write(61, 'R');
            EEPROM.put(62, highScore);
            EEPROM.commit();
        }
    }

    uint8_t getNextPieceFromBag() {
        if (bagIdx >= 7) {
            for (uint8_t i = 0; i < 7; i++) bag[i] = i;
            for (uint8_t i = 6; i > 0; i--) {
                uint8_t j = random(i + 1);
                uint8_t temp = bag[i];
                bag[i] = bag[j];
                bag[j] = temp;
            }
            bagIdx = 0;
        }
        return bag[bagIdx++];
    }

    void startNewGame() {
        memset(grid, 0, sizeof(grid));
        score = 0;
        lines = 0;
        level = 1;
        updateFallInterval();
        bagIdx = 7;
        currentPiece = getNextPieceFromBag();
        nextPiece = getNextPieceFromBag();
        spawnPiece();
        state = TRIS_STATE_PLAYING;
        sound.tone(1200, 30);
    }

    void updateFallInterval() {
        if (level >= 10) fallInterval = 90;
        else fallInterval = 720 - (level - 1) * 65;
    }

    void spawnPiece() {
        currentRot = 0;
        currentX = 3;
        currentY = 3;
        lastFallTime = millis();

        if (checkCollision(currentX, currentY, currentPiece, currentRot)) {
            state = TRIS_STATE_GAMEOVER;
            saveHighScore();
            triggerRgbLed(255, 0, 0, 1000);
            melodyPlayer.play(tttDefeatNotes, 4);
        }
    }

    bool checkCollision(int8_t px, int8_t py, uint8_t piece, uint8_t rot) {
        for (uint8_t i = 0; i < 4; i++) {
            int8_t bx = px + pieceBlocks[piece][rot][i][0];
            int8_t by = py + pieceBlocks[piece][rot][i][1];
            if (bx < 0 || bx >= 10 || by >= 20) return true;
            if (by >= 0 && grid[by][bx] != 0) return true;
        }
        return false;
    }

    void lockPiece() {
        for (uint8_t i = 0; i < 4; i++) {
            int8_t bx = currentX + pieceBlocks[currentPiece][currentRot][i][0];
            int8_t by = currentY + pieceBlocks[currentPiece][currentRot][i][1];
            if (by >= 0 && by < 20 && bx >= 0 && bx < 10) {
                grid[by][bx] = currentPiece + 1;
            }
        }
        sound.tone(600, 12);

        numLinesToClear = 0;
        for (int8_t r = 0; r < 20; r++) {
            bool full = true;
            for (int8_t c = 0; c < 10; c++) {
                if (grid[r][c] == 0) { full = false; break; }
            }
            if (full) {
                linesToClear[numLinesToClear++] = r;
            }
        }

        if (numLinesToClear > 0) {
            state = TRIS_STATE_FLASH;
            flashCount = 4;
            flashState = true;
            flashTimer = millis();
        } else {
            currentPiece = nextPiece;
            nextPiece = getNextPieceFromBag();
            spawnPiece();
        }
    }

    void collapseClearedLines() {
        for (uint8_t i = 0; i < numLinesToClear; i++) {
            uint8_t clearRow = linesToClear[i];
            for (int8_t r = clearRow; r > 0; r--) {
                for (int8_t c = 0; c < 10; c++) {
                    grid[r][c] = grid[r - 1][c];
                }
            }
            for (int8_t c = 0; c < 10; c++) grid[0][c] = 0;
        }

        // Puntuación: SOLO aumenta al eliminar líneas
        if (numLinesToClear == 1) score += 100 * level;
        else if (numLinesToClear == 2) score += 300 * level;
        else if (numLinesToClear == 3) score += 500 * level;
        else if (numLinesToClear >= 4) {
            score += 800 * level;
            triggerRgbLed(0, 255, 0, 500);
            melodyPlayer.play(pingPongVictoryNotes, 4);
        }

        if (numLinesToClear < 4) {
            sound.tone(880, 40);
        }

        lines += numLinesToClear;
        level = 1 + (lines / 10);
        updateFallInterval();
        saveHighScore();

        currentPiece = nextPiece;
        nextPiece = getNextPieceFromBag();
        spawnPiece();
        state = TRIS_STATE_PLAYING;
    }

    void update() {
        yield();

        if (state == TRIS_STATE_TITLE) {
            if (bootActionTriggered || arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) || arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
                startNewGame();
            }
            bootActionTriggered = false;
            return;
        }

        if (state == TRIS_STATE_GAMEOVER) {
            if (bootActionTriggered || arduboy.justPressed(A_BUTTON) || arduboy.justPressed(B_BUTTON) || arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
                state = TRIS_STATE_TITLE;
                sound.tone(900, 30);
            }
            bootActionTriggered = false;
            return;
        }

        if (state == TRIS_STATE_FLASH) {
            if (millis() - flashTimer >= 50) {
                flashTimer = millis();
                flashState = !flashState;
                if (flashCount > 0) {
                    flashCount--;
                } else {
                    collapseClearedLines();
                }
            }
            bootActionTriggered = false;
            return;
        }

        // --- MODO JUEGO ACTIVO ---
        // Rotar
        if (arduboy.justPressed(UP_BUTTON)) {
            uint8_t nextRot = (currentRot + 1) % 4;
            if (!checkCollision(currentX, currentY, currentPiece, nextRot)) {
                currentRot = nextRot; sound.tone(1400, 8);
            } else if (!checkCollision(currentX - 1, currentY, currentPiece, nextRot)) {
                currentX -= 1; currentRot = nextRot; sound.tone(1400, 8);
            } else if (!checkCollision(currentX + 1, currentY, currentPiece, nextRot)) {
                currentX += 1; currentRot = nextRot; sound.tone(1400, 8);
            } else if (!checkCollision(currentX - 2, currentY, currentPiece, nextRot)) {
                currentX -= 2; currentRot = nextRot; sound.tone(1400, 8);
            } else if (!checkCollision(currentX + 2, currentY, currentPiece, nextRot)) {
                currentX += 2; currentRot = nextRot; sound.tone(1400, 8);
            }
        }

        // Izquierda
        if (arduboy.justPressed(A_BUTTON)) {
            if (!checkCollision(currentX - 1, currentY, currentPiece, currentRot)) {
                currentX--; sound.tone(1100, 8);
            }
        }

        // Derecha
        if (arduboy.justPressed(B_BUTTON)) {
            if (!checkCollision(currentX + 1, currentY, currentPiece, currentRot)) {
                currentX++; sound.tone(1100, 8);
            }
        }

        // Soft drop: NO AUMENTA EL SCORE
        if (arduboy.justPressed(DOWN_BUTTON)) {
            if (!checkCollision(currentX, currentY + 1, currentPiece, currentRot)) {
                currentY++;
                lastFallTime = millis();
                sound.tone(700, 8);
            } else {
                lockPiece();
            }
        }

        // Gravedad
        if (millis() - lastFallTime >= fallInterval) {
            lastFallTime = millis();
            if (!checkCollision(currentX, currentY + 1, currentPiece, currentRot)) {
                currentY++;
            } else {
                lockPiece();
            }
        }

        bootActionTriggered = false;
    }

    void drawNumber(uint32_t val, int16_t rightX, int16_t y, uint8_t color) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u", val);
        uint8_t len = strlen(buf);
        int16_t curX = rightX - (len * 5 - 1);
        for (uint8_t i = 0; i < len; i++) {
            uint8_t d = buf[i] - '0';
            if (d <= 9) {
                for (uint8_t row = 0; row < 6; row++) {
                    uint8_t rowBits = pgm_read_byte(&trisDigits4x6[d][row]);
                    for (uint8_t col = 0; col < 4; col++) {
                        if ((rowBits >> (3 - col)) & 1) {
                            arduboy.drawPixel(curX + col, y + row, color);
                        }
                    }
                }
            }
            curX += 5;
        }
    }

    void drawCell(int16_t bx, int16_t by, uint8_t type) {
        if (type == 0) return;
        arduboy.fillRect(bx, by, 4, 4, BLACK);
        if (type == 2) { // O piece: concentric square
            arduboy.drawPixel(bx + 1, by + 1, WHITE);
            arduboy.drawPixel(bx + 2, by + 1, WHITE);
            arduboy.drawPixel(bx + 1, by + 2, WHITE);
            arduboy.drawPixel(bx + 2, by + 2, WHITE);
        } else if (type == 3 || type == 4 || type == 5) { // T, S, Z: checkerboard dither
            for (int dy = 0; dy < 4; dy++) {
                for (int dx = 0; dx < 4; dx++) {
                    if ((bx + dx + by + dy) % 2 == 0) {
                        arduboy.drawPixel(bx + dx, by + dy, WHITE);
                    }
                }
            }
        } else { // I, J, L: domino bevel dots
            arduboy.drawPixel(bx + 1, by + 1, WHITE);
            arduboy.drawPixel(bx + 2, by + 1, WHITE);
            arduboy.drawPixel(bx + 1, by + 2, WHITE);
        }
    }

    void drawWell() {
        for (int8_t r = 5; r < 20; r++) {
            bool isClearingRow = false;
            if (state == TRIS_STATE_FLASH) {
                for (uint8_t i = 0; i < numLinesToClear; i++) {
                    if (linesToClear[i] == r) { isClearingRow = true; break; }
                }
            }

            if (isClearingRow && flashState) {
                arduboy.fillRect(49, 2 + (r - 5) * 4, 38, 4, BLACK);
                continue;
            }

            for (int8_t c = 0; c < 10; c++) {
                uint8_t val = grid[r][c];
                if (val != 0) {
                    drawCell(49 + c * 4, 2 + (r - 5) * 4, val);
                }
            }
        }

        if (state == TRIS_STATE_PLAYING) {
            for (uint8_t i = 0; i < 4; i++) {
                int8_t bx = currentX + pieceBlocks[currentPiece][currentRot][i][0];
                int8_t by = currentY + pieceBlocks[currentPiece][currentRot][i][1];
                if (by >= 5 && by < 20 && bx >= 0 && bx < 10) {
                    drawCell(49 + bx * 4, 2 + (by - 5) * 4, currentPiece + 1);
                }
            }
        }
    }

    void drawNextPiece() {
        int16_t ox = 97;
        int16_t oy = 24;
        if (nextPiece == 0) { // I
            ox = 95; oy = 26;
        } else if (nextPiece == 1) { // O
            ox = 99; oy = 24;
        }
        for (uint8_t i = 0; i < 4; i++) {
            int8_t px = pieceBlocks[nextPiece][0][i][0];
            int8_t py = pieceBlocks[nextPiece][0][i][1];
            drawCell(ox + px * 4, oy + py * 4, nextPiece + 1);
        }
    }

    void drawTitleScreen() {
        arduboy.drawBitmap(0, 0, tris_title_bitmap, 128, 64, WHITE);
        if (((millis() / 400) % 2) == 1) {
            arduboy.fillRect(36, 42, 57, 5, BLACK);
        }
    }

    void draw() {
        yield();
        if (state == TRIS_STATE_TITLE) {
            drawTitleScreen();
            return;
        }

        arduboy.drawBitmap(0, 0, tris_hud_bitmap, 128, 64, WHITE);
        drawNumber(score, 39, 20, BLACK);
        drawNumber(level, 30, 51, WHITE);
        drawNumber(lines, 107, 51, WHITE);
        drawNextPiece();
        drawWell();

        if (state == TRIS_STATE_GAMEOVER) {
            arduboy.fillRect(48, 22, 40, 18, BLACK);
            arduboy.drawRect(48, 22, 40, 18, WHITE);
            arduboy.setTextColor(WHITE);
            arduboy.setTextSize(1);
            arduboy.setCursor(53, 24);
            arduboy.print("GAME");
            arduboy.setCursor(53, 31);
            arduboy.print("OVER");
        }
    }
} trisGame;



// =============================================================================
// SYSTEM STATE & SELECTOR MENU
// =============================================================================
enum SystemState {
    STATE_MENU = 0,
    STATE_PING_PONG,
    STATE_BREAKOUT,
    STATE_SPACE_INVADERS,
    STATE_1943,
    STATE_FLAPPY_BIRD,
    STATE_SNAKE,
    STATE_JUMP_MAN,
    STATE_PROYECTO_BLE,
    STATE_2048,
    STATE_TREX_RUNNER,
    STATE_TIC_TAC_TOE,
    STATE_BRAINDU_TRAIN,
    STATE_HANGMAN,
    STATE_TRIS
};

SystemState currentState = STATE_MENU;
int8_t menuCursor = 0;
const char* menuItems[] = {
    "1. Ping Pong",
    "2. Breakout",
    "3. Space Invaders",
    "4. 1943",
    "5. Flappy Bird",
    "6. Snake",
    "7. Jump Man",
    "8. Proyecto BLE",
    "9. 2048",
    "10. T-Rex Runner",
    "11. Tic-Tac-Toe",
    "12. BrainduTrain",
    "13. Hangman",
    "14. TRIS"
};
const uint8_t MENU_COUNT = 14;


// System Pause & Power Management State
bool gamePaused = false;
bool isScreenSleeping = false;
unsigned long bootRawLowStart = 0;
bool bootDebouncedDown = false;
unsigned long bootDebouncedPressStart = 0;
unsigned long exitComboStart = 0;
const unsigned long INACTIVITY_TIMEOUT_MS = 90000UL; // 90s timeout de inactividad
unsigned long lastActivityTime = 0;

void enterDeepSleep(const char* message = "APAGANDO...") {
    if (currentState == STATE_PROYECTO_BLE) {
        proyectoBLE.stop();
    }
    melodyPlayer.stop();
    arduboy.clear();
    arduboy.fillRect(10, 18, 108, 28, BLACK);
    arduboy.drawRect(10, 18, 108, 28, WHITE);
    arduboy.drawRect(12, 20, 104, 24, WHITE);
    arduboy.setCursor(24, 28);
    arduboy.print(message);
    arduboy.display();

    // Power-off sound sequence on GPIO 2
    sound.tone(880, 50); delay(70);
    sound.tone(659, 70); delay(90);
    sound.tone(440, 100); delay(120);
    sound.tone(220, 160); delay(200);
    sound.noTone();

    // Turn off display to conserve power
    arduboy.clear();
    arduboy.display();
    Arduboy2Core::sendCommand(SSD1306_DISPLAYOFF);

    // Turn off buzzer and RGB LED (Full detachment & GND clamp)
    sound.noTone();
    #if defined(ESP32)
    ledcDetach(ARDUBOY_PIN_BUZZER);
    #endif
    pinMode(ARDUBOY_PIN_BUZZER, OUTPUT);
    digitalWrite(ARDUBOY_PIN_BUZZER, LOW);
    rgbLedWrite(RGB_LED_PIN, 0, 0, 0);

    // Configure wake-up on GPIO 9 (BOOT button pressed LOW)
    esp_deep_sleep_enable_gpio_wakeup((1ULL << ARDUBOY_PIN_BOOT), ESP_GPIO_WAKEUP_GPIO_LOW);

    // Wait until button is released to prevent immediate wakeup
    while (digitalRead(ARDUBOY_PIN_BOOT) == LOW) {
        delay(20);
    }
    delay(100);
    esp_deep_sleep_start();
}

void setup() {
    sound.noTone();
    pinMode(ARDUBOY_PIN_BOOT, INPUT_PULLUP);

    // -------------------------------------------------------------------------
    // POWER-ON: REQUERIR PULSACIÓN SOSTENIDA DE 3 SEGUNDOS TRAS DEEP SLEEP
    // -------------------------------------------------------------------------
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        uint32_t pressStart = millis();
        bool powerOnConfirmed = false;

        while (digitalRead(ARDUBOY_PIN_BOOT) == LOW) {
            if (millis() - pressStart >= 3000) {
                powerOnConfirmed = true;
                break; // Cumplió los 3 segundos continuos
            }
            delay(10);
        }

        if (!powerOnConfirmed) {
            // Si el usuario soltó antes de 3s: abortar arranque y volver de inmediato a Deep Sleep
            esp_deep_sleep_enable_gpio_wakeup((1ULL << ARDUBOY_PIN_BOOT), ESP_GPIO_WAKEUP_GPIO_LOW);
            esp_deep_sleep_start();
        }

        // Pitido breve de confirmación de encendido en el Buzzer (GPIO 2)
        sound.tone(1400, 60);
        delay(80);
        sound.noTone();

        // Esperar a que se suelte el botón BOOT para no disparar pausas inmediatas
        while (digitalRead(ARDUBOY_PIN_BOOT) == LOW) {
            delay(10);
        }
        delay(50);
    }

    Serial.begin(115200);
    delay(50);

    // Ensure RGB LED starts OFF
    rgbLedWrite(RGB_LED_PIN, 0, 0, 0);

    // Boot Arduboy2 Core (OLED on SDA=19, SCL=20 | Buttons: B1=14, B2=0, B3=7, B4=18 | BOOT=9 | Buzzer=2)
    arduboy.begin();
    arduboy.setFrameRate(60);

    lastActivityTime = millis();

    sound.tone(880, 40); delay(50);
    sound.tone(1760, 60); delay(70);
    sound.noTone();

    Serial.println("[ESP32-C6] Retro Console Ready (Buzzer=GPIO2, B1=GPIO14, B2=GPIO0, B3=GPIO7, B4=GPIO18, BOOT=GPIO9, Standby=90s)!");
}

void loop() {
    yield();
    // Non-blocking 1s RGB LED auto-off timer & tone auto-detach
    updateRgbLed();
    sound.update();
    melodyPlayer.update();

    // -------------------------------------------------------------------------
    // STANDBY POR SOFTWARE / SCREEN BLANKING (WAKEUP INMEDIATO SIN BUG I2C)
    // -------------------------------------------------------------------------
    if (isScreenSleeping) {
        melodyPlayer.stop();
        sound.noTone();
        // En reposo: omitir renderizado y físicas; monitorizar los 5 botones (14, 0, 7, 18, 9)
        bool btn1 = (digitalRead(ARDUBOY_PIN_BTN1) == LOW); // GPIO 14
        bool btn2 = (digitalRead(ARDUBOY_PIN_BTN2) == LOW); // GPIO 0
        bool btn3 = (digitalRead(ARDUBOY_PIN_BTN3) == LOW); // GPIO 7
        bool btn4 = (digitalRead(ARDUBOY_PIN_BTN4) == LOW); // GPIO 18
        bool boot = (digitalRead(ARDUBOY_PIN_BOOT) == LOW); // GPIO 9

        if (btn1 || btn2 || btn3 || btn4 || boot) {
            // Despertar inmediato: Encender panel OLED y refrescar pantalla
            Arduboy2Core::sendCommand(SSD1306_DISPLAYON);
            arduboy.display();
            sound.tone(1200, 25);
            isScreenSleeping = false;
            lastActivityTime = millis();
            bootRawLowStart = 0;
            bootDebouncedDown = false;
            delay(150); // Debounce de despertar
        } else {
            delay(10);
        }
        return;
    }

    // Strict software debounce for BOOT button (GPIO 9): minimum 50ms continuous LOW
    bool bootPinLow = (digitalRead(ARDUBOY_PIN_BOOT) == LOW);
    if (bootPinLow) {
        if (bootRawLowStart == 0) {
            bootRawLowStart = millis();
        } else if (!bootDebouncedDown && (millis() - bootRawLowStart >= 50)) {
            // Valid continuous 50ms LOW confirmed
            bootDebouncedDown = true;
            bootDebouncedPressStart = millis();
            lastActivityTime = millis();
        }
    } else {
        bootRawLowStart = 0;
        if (bootDebouncedDown) {
            unsigned long duration = millis() - bootDebouncedPressStart;
            bootDebouncedDown = false;
            if (duration >= 50 && duration < 1000) {
                // Short press:
                if (currentState == STATE_TIC_TAC_TOE) {
                    ticTacToe.onBootShortPress();
                } else if (currentState == STATE_BRAINDU_TRAIN) {
                    brainduTrain.onBootShortPress();
                } else if (currentState == STATE_HANGMAN) {
                    hangmanGame.onBootShortPress();
                } else if (currentState == STATE_TRIS) {
                    trisGame.onBootShortPress();
                } else if (currentState != STATE_MENU) {
                    gamePaused = !gamePaused;
                    if (gamePaused) {
                        sound.tone(600, 30);
                    } else {
                        sound.tone(900, 30);
                    }
                }
            } else if (duration >= 1000 && duration < 3000) {
                // Medium press (1s - 3s hold):
                if (currentState == STATE_BRAINDU_TRAIN) {
                    brainduTrain.onBootHold1s();
                }
            }
        }
    }

    // Check for sustained hold (>= 3000ms) to trigger Deep Sleep
    if (bootDebouncedDown && (millis() - bootDebouncedPressStart >= 3000)) {
        bootDebouncedDown = false;
        bootRawLowStart = 0;
        enterDeepSleep("APAGANDO...");
    }

    // Reset inactivity timer whenever any button is pressed
    if (arduboy.buttonsState() != 0 || bootDebouncedDown) {
        lastActivityTime = millis();
    }

    // Auto Standby tras 90s de inactividad (Screen Blanking)
    if (millis() - lastActivityTime >= INACTIVITY_TIMEOUT_MS) {
        melodyPlayer.stop();
        sound.noTone();
        Arduboy2Core::sendCommand(SSD1306_DISPLAYOFF);
        isScreenSleeping = true;
        return;
    }

    if (!arduboy.nextFrame()) return;

    arduboy.clear();

    // Universal Exit Combo: Hold B1 (GPIO 14) + B4 (GPIO 18) or B3 (GPIO 7) + B4 (GPIO 18) >= 450ms returns to menu
    if (currentState != STATE_MENU) {
        bool comboHeld = (arduboy.pressed(LEFT_BUTTON) && arduboy.pressed(B_BUTTON)) ||
                         (arduboy.pressed(A_BUTTON) && arduboy.pressed(B_BUTTON));
        if (comboHeld) {
            if (exitComboStart == 0) {
                exitComboStart = millis();
            } else if (millis() - exitComboStart >= 450) {
                if (currentState == STATE_PROYECTO_BLE) {
                    proyectoBLE.stop();
                }
                melodyPlayer.stop();
                sound.tone(600, 30);
                gamePaused = false;
                currentState = STATE_MENU;
                exitComboStart = 0;
            }
        } else {
            exitComboStart = 0;
        }
    }

    switch (currentState) {
        case STATE_MENU: {
            gamePaused = false;

            // Header Bar
            arduboy.fillRect(0, 0, 128, 9, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(8, 1);
            arduboy.print("ESP32-C6 RETRO ARCADE");
            arduboy.setTextColor(WHITE);

            // Menu Viewport (Displays 5 items with smooth scrolling)
            uint8_t startIdx = 0;
            if (menuCursor > 3) startIdx = menuCursor - 3;
            if (startIdx + 5 > MENU_COUNT) startIdx = MENU_COUNT - 5;

            for (uint8_t i = 0; i < 5; i++) {
                uint8_t itemIdx = startIdx + i;
                int16_t y = 11 + (i * 9);
                if (itemIdx == menuCursor) {
                    arduboy.fillRect(2, y - 1, 124, 9, WHITE);
                    arduboy.setTextColor(BLACK);
                    arduboy.setCursor(6, y);
                    arduboy.print("> ");
                    arduboy.print(menuItems[itemIdx]);
                    arduboy.setTextColor(WHITE);
                } else {
                    arduboy.setCursor(6, y);
                    arduboy.print("  ");
                    arduboy.print(menuItems[itemIdx]);
                }
            }

            // Bottom Navigation Bar
            arduboy.drawFastHLine(0, 56, 128, WHITE);
            arduboy.setCursor(2, 57);
            arduboy.print("B1/B2:Mover  B3:Jugar");

            if (arduboy.justPressed(LEFT_BUTTON) || arduboy.justPressed(UP_BUTTON)) {
                sound.tone(880, 15);
                menuCursor--;
                if (menuCursor < 0) menuCursor = MENU_COUNT - 1;
            }
            if (arduboy.justPressed(RIGHT_BUTTON) || arduboy.justPressed(DOWN_BUTTON)) {
                sound.tone(700, 15);
                menuCursor++;
                if (menuCursor >= MENU_COUNT) menuCursor = 0;
            }

            if (arduboy.justPressed(A_BUTTON)) {
                sound.tone(1760, 50);
                gamePaused = false;
                if (menuCursor == 0) { pingPong.init(); currentState = STATE_PING_PONG; }
                else if (menuCursor == 1) { breakout.init(); currentState = STATE_BREAKOUT; }
                else if (menuCursor == 2) { spaceInvaders.init(); currentState = STATE_SPACE_INVADERS; }
                else if (menuCursor == 3) { airCombat.init(); currentState = STATE_1943; }
                else if (menuCursor == 4) { flappy.init(); currentState = STATE_FLAPPY_BIRD; }
                else if (menuCursor == 5) { snake.init(); currentState = STATE_SNAKE; }
                else if (menuCursor == 6) { jumpMan.init(); currentState = STATE_JUMP_MAN; }
                else if (menuCursor == 7) { proyectoBLE.init(); currentState = STATE_PROYECTO_BLE; }
                else if (menuCursor == 8) { game2048.init(); currentState = STATE_2048; }
                else if (menuCursor == 9) { trexRunner.init(); currentState = STATE_TREX_RUNNER; }
                else if (menuCursor == 10) { ticTacToe.init(); currentState = STATE_TIC_TAC_TOE; }
                else if (menuCursor == 11) { brainduTrain.init(); currentState = STATE_BRAINDU_TRAIN; }
                else if (menuCursor == 12) { hangmanGame.init(); currentState = STATE_HANGMAN; }
                else if (menuCursor == 13) { trisGame.init(); currentState = STATE_TRIS; }
            }
            break;
        }

        case STATE_PING_PONG:
            if (!gamePaused) pingPong.update();
            pingPong.draw();
            break;

        case STATE_BREAKOUT:
            if (!gamePaused) breakout.update();
            breakout.draw();
            break;

        case STATE_SPACE_INVADERS:
            if (!gamePaused) spaceInvaders.update();
            spaceInvaders.draw();
            break;

        case STATE_1943:
            if (!gamePaused) airCombat.update();
            airCombat.draw();
            break;

        case STATE_FLAPPY_BIRD:
            if (!gamePaused) flappy.update();
            flappy.draw();
            break;

        case STATE_SNAKE:
            if (!gamePaused) snake.update();
            snake.draw();
            break;

        case STATE_JUMP_MAN:
            if (!gamePaused) jumpMan.update();
            jumpMan.draw();
            break;

        case STATE_PROYECTO_BLE:
            if (!gamePaused) proyectoBLE.update();
            proyectoBLE.draw();
            break;

        case STATE_2048:
            if (!gamePaused) game2048.update();
            game2048.draw();
            break;

        case STATE_TREX_RUNNER:
            if (!gamePaused) trexRunner.update();
            trexRunner.draw();
            break;

        case STATE_TIC_TAC_TOE:
            if (!gamePaused) ticTacToe.update();
            ticTacToe.draw();
            break;

        case STATE_BRAINDU_TRAIN:
            if (!gamePaused) brainduTrain.update();
            brainduTrain.draw();
            break;

        case STATE_HANGMAN:
            if (!gamePaused) hangmanGame.update();
            hangmanGame.draw();
            break;

        case STATE_TRIS:
            if (!gamePaused) trisGame.update();
            trisGame.draw();
            break;
    }

    // Pause Overlay Window
    if (currentState != STATE_MENU && gamePaused) {
        arduboy.fillRect(12, 16, 104, 32, BLACK);
        arduboy.drawRect(12, 16, 104, 32, WHITE);
        arduboy.drawRect(14, 18, 100, 28, WHITE);
        arduboy.setCursor(49, 22);
        arduboy.print("PAUSA");
        arduboy.setCursor(22, 33);
        arduboy.print("BOOT: REANUDAR");
    }

    arduboy.display();
}
