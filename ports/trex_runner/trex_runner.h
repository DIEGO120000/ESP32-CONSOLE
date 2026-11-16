#pragma once
#include "console_core.h"

// =============================================================================
// GAME 10: T-REX RUNNER (Chrome Dino Port with AABB Physics & Pterodactyls)
// =============================================================================
inline void draw1BitBitmap(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint8_t* bmp, uint8_t color = WHITE) {
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
extern const uint8_t trex_run1[28];

// 14x14 Dino Run 2
extern const uint8_t trex_run2[28];

// 14x14 Dino Dead (Crash Eye X)
extern const uint8_t trex_dead[28];

// 18x9 Dino Duck 1 (Agachado)
extern const uint8_t trex_duck1[27];

// 18x9 Dino Duck 2 (Agachado)
extern const uint8_t trex_duck2[27];

// 6x12 Cactus Small
extern const uint8_t cactus_small[12];

// 12x14 Cactus Large / Cluster
extern const uint8_t cactus_large[28];

// 14x10 Pterodactyl Wing Up
extern const uint8_t ptero_up[20];

// 14x10 Pterodactyl Wing Down
extern const uint8_t ptero_down[20];

// 14x5 Cloud
extern const uint8_t trex_cloud[10];

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

        // Salto: UP o A_BUTTON (GPIO 15 / GPIO 20)
        if ((arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(A_BUTTON)) && onGround) {
            dinoVY = -4.3f;
            onGround = false;
            sound.tone(800, 30);
        }

        // Agacharse: DOWN (GPIO 18)
        isDucking = arduboy.pressed(DOWN_BUTTON);
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
};
