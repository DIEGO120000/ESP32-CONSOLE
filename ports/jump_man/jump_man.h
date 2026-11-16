#pragma once
#include "console_core.h"

// --- Jump Man Sprites (5x8 Pixel Format) ---
extern const uint8_t jm_dinoChar[8];
extern const uint8_t jm_saltoInicio[8];
extern const uint8_t jm_saltoPico[8];
extern const uint8_t jm_saltoBajada[8];

extern const uint8_t jm_obstaculo1[8];
extern const uint8_t jm_obstaculo2[8];
extern const uint8_t jm_obstaculo3[8];

// Helper to draw 5x8 bitmap
inline void draw5x8Sprite(int16_t x, int16_t y, const uint8_t sprite[8], uint8_t color = WHITE) {
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
};
