#pragma once
#include "console_core.h"

// =============================================================================
// =============================================================================
// GAME 1: PING PONG (Normal Match vs CPU & Inverted Survival Dodge Mode)
// =============================================================================
enum PingPongMode {
    PP_MODE_SUBMENU = 0,
    PP_MODE_NORMAL,
    PP_MODE_DODGE,
    PP_MODE_WIFI_2P
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
    // Record nuevo en RAM pendiente de volcar a EEPROM. El commit de 1 KB
    // (Preferences::putBytes) bloquea la flash y es un pico de corriente que
    // puede tumbar el regulador: nunca se escribe en mitad de partida.
    bool scoreDirty;
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
    unsigned long submenuEnterTime;
    bool exitRequested;

    // 2P WiFi Gamepads Variables
    bool wifiMatchStarted;
    uint8_t winnerP;
    unsigned long lobbyWarnUntil; // Aviso breve: falta un jugador en el lobby

    void init() {
        flushScores(); // record pendiente a flash con el juego ya parado
        melodyPlayer.stop();
        if (wifiGamepads.isRunning) {
            wifiGamepads.stop();
        }
        mode = PP_MODE_SUBMENU;
        submenuCursor = 0;
        submenuEnterTime = millis();
        exitRequested = false;
        scoreDirty = false;
    }

    // Vuelca a EEPROM el record pendiente. Solo se llama en Game Over o al
    // salir al menu, nunca dentro del bucle activo: cada EEPROM.put() arrastra
    // un commit sincrono de los 1024 B completos de la NVS a la flash.
    void flushScores() {
        if (!scoreDirty) return;
        if (mode == PP_MODE_NORMAL) {
            EEPROM.put(14, highScore);
        } else if (mode == PP_MODE_DODGE) {
            EEPROM.put(24, highScoreDodge);
        }
        scoreDirty = false;
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
        // Navegación bidireccional accesible (3 modos):
        if (arduboy.justPressed(UP_BUTTON) || arduboy.justPressed(LEFT_BUTTON)) {
            if (submenuCursor > 0) {
                sound.tone(880, 15);
                submenuCursor--;
            }
        }
        if (arduboy.justPressed(DOWN_BUTTON) || arduboy.justPressed(RIGHT_BUTTON)) {
            if (submenuCursor < 2) {
                sound.tone(700, 15);
                submenuCursor++;
            }
        }

        // Botón B: Regresar al Menú Principal
        if (arduboy.justPressed(B_BUTTON)) {
            sound.tone(1400, 30);
            exitRequested = true;
            return;
        }

        // Botón A o Click Central: Confirmar y arrancar el modo seleccionado
        if (arduboy.justPressed(A_BUTTON)) {
            sound.tone(1760, 40);
            if (submenuCursor == 0) {
                initNormal();
            } else if (submenuCursor == 1) {
                initDodge();
            } else {
                initWifi2P();
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

        const char* ppItems[3] = {
            "1. Modo Normal",
            "2. Modo Invertido",
            "3. 2P WiFi Gamepads"
        };

        for (uint8_t i = 0; i < 3; i++) {
            int16_t y = 16 + (i * 12);
            if (i == submenuCursor) {
                arduboy.fillRect(4, y - 2, 120, 11, WHITE);
                arduboy.setTextColor(BLACK);
                arduboy.setCursor(8, y);
                arduboy.print("> ");
                arduboy.print(ppItems[i]);
                arduboy.setTextColor(WHITE);
            } else {
                arduboy.setCursor(8, y);
                arduboy.print("  ");
                arduboy.print(ppItems[i]);
            }
        }

        // Bottom Navigation Bar
        arduboy.drawFastHLine(0, 54, 128, WHITE);
        arduboy.setCursor(2, 56);
        arduboy.print("D-PAD:Mover  A:OK  B:Menu");
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

        // --- Player Controls (UP: GPIO 15, DOWN: GPIO 18) ---
        if (arduboy.pressed(UP_BUTTON) && p1Y > 11.0f) p1Y -= 2.6f;
        if (arduboy.pressed(DOWN_BUTTON) && p1Y < 46.0f) p1Y += 2.6f;

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
                flushScores(); // fin de partida: unico punto de escritura
                triggerRgbLed(255, 0, 0, 1000); // Exclusively RED on defeat vs CPU
                melodyPlayer.play(pingPongDefeatNotes, 4);
            } else {
                sound.tone(220, 80);
                serveNormal(1);
            }
        } else if (bx > 132.0f) {
            s1++;
            if (s1 > highScore) { highScore = s1; scoreDirty = true; } // RAM; flush al final
            if (s1 >= 5) {
                victory = true;
                flushScores(); // fin de partida: unico punto de escritura
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
            const int16_t w = 96;
            const int16_t h = 30;
            const int16_t x = (128 - w) / 2;
            const int16_t y = 17;
            arduboy.fillRect(x, y, w, h, BLACK);
            arduboy.drawRoundRect(x, y, w, h, 3, WHITE);
            arduboy.setCursor((128 - (9 * 6)) / 2, y + 5); arduboy.print("CPU WINS!");
            arduboy.setCursor((128 - (14 * 6)) / 2, y + 16); arduboy.print("Pulsa un boton");
        } else if (victory) {
            const int16_t w = 96;
            const int16_t h = 30;
            const int16_t x = (128 - w) / 2;
            const int16_t y = 17;
            arduboy.fillRect(x, y, w, h, BLACK);
            arduboy.drawRoundRect(x, y, w, h, 3, WHITE);
            arduboy.setCursor((128 - (8 * 6)) / 2, y + 5); arduboy.print("YOU WIN!");
            arduboy.setCursor((128 - (14 * 6)) / 2, y + 16); arduboy.print("Pulsa un boton");
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

        // Player Controls (UP: GPIO 15, DOWN: GPIO 18)
        if (arduboy.pressed(UP_BUTTON) && p1Y > 11.0f) p1Y -= 2.6f;
        if (arduboy.pressed(DOWN_BUTTON) && p1Y < 46.0f) p1Y += 2.6f;

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
                scoreDirty = true;
                isNewRecord = true;
                triggerRgbLed(0, 255, 0, 1000); // 1s Green LED on personal record
                melodyPlayer.play(pingPongVictoryNotes, 4);
                flushScores(); // Game Over: se persiste aqui, no durante la partida
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
            const int16_t w = 104;
            const int16_t h = 36;
            const int16_t x = (128 - w) / 2;
            const int16_t y = 14;
            arduboy.fillRect(x, y, w, h, BLACK);
            arduboy.drawRoundRect(x, y, w, h, 3, WHITE);

            if (isNewRecord) {
                arduboy.setCursor((128 - (13 * 6)) / 2, y + 4);
                arduboy.print("NUEVO RECORD!");
            } else {
                arduboy.setCursor((128 - (9 * 6)) / 2, y + 4);
                arduboy.print("GAME OVER");
            }

            arduboy.setCursor(24, y + 14);
            arduboy.print("ESQUIVAS: ");
            arduboy.print(dodgeScore);

            arduboy.setCursor((128 - (14 * 6)) / 2, y + 24);
            arduboy.print("Pulsa un boton");
        }
    }

    void initWifi2P() {
        mode = PP_MODE_WIFI_2P;
        melodyPlayer.stop();
        wifiGamepads.begin();
        p1Y = 24.0f;
        p2Y = 24.0f;
        s1 = 0;
        s2 = 0;
        gameOver = false;
        victory = false;
        winnerP = 0;
        wifiMatchStarted = false;
        lobbyWarnUntil = 0;
        serveNormal(1);
    }

    void updateWifi2P() {
        wifiGamepads.update();

        // Pantalla de Lobby previo al saque
        if (!wifiMatchStarted) {
            // Botón B: Regresar al menú de Ping Pong y apagar SoftAP
            if (arduboy.justPressed(B_BUTTON)) {
                sound.tone(1400, 30);
                init();
                return;
            }

            // El host puede pulsar A para arrancar, pero solo con LOS DOS
            // mandos web conectados (P1 y P2). Sin ambos, la pulsacion se
            // ignora y el estado ausente parpadea como aviso.
            if (arduboy.justPressed(A_BUTTON)) {
                if (wifiGamepads.p1Connected && wifiGamepads.p2Connected) {
                    sound.tone(1760, 40);
                    wifiMatchStarted = true;
                    serveNormal(1);
                } else {
                    sound.tone(220, 70);
                    lobbyWarnUntil = millis() + 1500;
                }
            }
            return;
        }

        // Si la partida está pausada por alguno de los mandos web, congelar la física
        if (wifiGamepads.pausedBy != 0) {
            return;
        }

        // Fin de partida (victoria P1 o P2)
        if (gameOver) {
            if (arduboy.justPressed(A_BUTTON) || wifiGamepads.p1Up || wifiGamepads.p1Down || wifiGamepads.p2Up || wifiGamepads.p2Down) {
                sound.tone(1760, 40);
                s1 = 0;
                s2 = 0;
                gameOver = false;
                winnerP = 0;
                serveNormal(1);
            }
            return;
        }

        // --- Movimiento Paleta 1 (Izquierda) ---
        // Controlable por Mando Web A (WebSocket) O Cruceta Física local
        if ((wifiGamepads.p1Up || arduboy.pressed(UP_BUTTON)) && p1Y > 11.0f) p1Y -= 2.6f;
        if ((wifiGamepads.p1Down || arduboy.pressed(DOWN_BUTTON)) && p1Y < 46.0f) p1Y += 2.6f;

        // --- Movimiento Paleta 2 (Derecha) ---
        // Controlable exclusivamente por Mando Web B (WebSocket)
        if (wifiGamepads.p2Up && p2Y > 11.0f) p2Y -= 2.6f;
        if (wifiGamepads.p2Down && p2Y < 46.0f) p2Y += 2.6f;

        // Movimiento de la pelota
        bx += bvx;
        by += bvy;

        // Rebotes en paredes superior e inferior
        if (by <= 10.0f) {
            by = 10.0f;
            bvy = fabs(bvy);
            sound.tone(450, 12);
        } else if (by >= 60.0f) {
            by = 60.0f;
            bvy = -fabs(bvy);
            sound.tone(450, 12);
        }

        // Colisión segmentada Paleta 1 (Izquierda)
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

        // Colisión segmentada Paleta 2 (Derecha)
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

        // Comprobación de puntos (Partida a 5 tantos)
        if (bx < -4.0f) {
            s2++;
            if (s2 >= 5) {
                gameOver = true;
                winnerP = 2;
                triggerRgbLed(255, 140, 0, 1500); // Feedback Naranja Neón P2
                melodyPlayer.play(pingPongVictoryNotes, 4);
            } else {
                sound.tone(220, 80);
                serveNormal(1);
            }
        } else if (bx > 132.0f) {
            s1++;
            if (s1 >= 5) {
                gameOver = true;
                winnerP = 1;
                triggerRgbLed(0, 240, 255, 1500); // Feedback Cian Neón P1
                melodyPlayer.play(pingPongVictoryNotes, 4);
            } else {
                sound.tone(1400, 80);
                serveNormal(-1);
            }
        }
    }

    void drawWifi2P() {
        if (!wifiMatchStarted) {
            // Pantalla limpia: borra cualquier residuo del frame anterior.
            arduboy.fillRect(0, 0, 128, 64, BLACK);

            // Barra de titulo
            arduboy.fillRect(0, 0, 128, 9, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(4, 1);
            arduboy.print("2P WIFI GAMEPADS");
            arduboy.setTextColor(WHITE);

            // Margen izquierdo seguro X = 4 y solo ASCII estandar.
            arduboy.setCursor(4, 12);
            arduboy.print("AP RetroArcade-ESP32");
            arduboy.setCursor(4, 22);
            arduboy.print("Web:192.168.4.1");

            arduboy.drawFastHLine(4, 32, 120, WHITE);

            const bool twoPlayers = wifiGamepads.p1Connected && wifiGamepads.p2Connected;
            // Parpadeo del jugador que falta durante 1.5 s tras pulsar A.
            const bool warnActive = (long)(millis() - lobbyWarnUntil) < 0;
            const bool blinkOn = !warnActive || ((millis() / 250) % 2 == 0);

            arduboy.setCursor(4, 36);
            arduboy.print("P1(Izq):");
            if (wifiGamepads.p1Connected) arduboy.print("[CONECTADO]");
            else if (blinkOn) arduboy.print("[ESPERANDO]");

            arduboy.setCursor(4, 46);
            arduboy.print("P2(Der):");
            if (wifiGamepads.p2Connected) arduboy.print("[CONECTADO]");
            else if (blinkOn) arduboy.print("[ESPERANDO]");

            arduboy.drawFastHLine(4, 55, 120, WHITE);

            arduboy.setCursor(4, 57);
            if (twoPlayers) arduboy.print("A:Iniciar");
            else arduboy.print("Esperando 2P");

            arduboy.setCursor(82, 57);
            arduboy.print("B:Salir");

            // Garantiza un borde izquierdo limpio (X = 0..3 sin basura).
            arduboy.fillRect(0, 12, 4, 52, BLACK);
            return;
        }

        // Marcador superior en pantalla OLED de 2.42"
        arduboy.setCursor(14, 1); arduboy.print("P1: "); arduboy.print(s1);
        if (wifiGamepads.p1Connected) arduboy.print("*");
        arduboy.setCursor(80, 1); arduboy.print("P2: "); arduboy.print(s2);
        if (wifiGamepads.p2Connected) arduboy.print("*");
        arduboy.drawFastHLine(0, 9, 128, WHITE);

        // Red central
        for (uint8_t y = 11; y < 64; y += 6) arduboy.drawFastVLine(63, y, 3, WHITE);

        // Paleta Jugador 1 (Izquierda)
        arduboy.fillRect(3, (int16_t)p1Y, 3, 16, WHITE);

        // Paleta Jugador 2 (Derecha)
        arduboy.fillRect(122, (int16_t)p2Y, 3, 16, WHITE);

        // Pelota
        arduboy.fillRect((int16_t)bx, (int16_t)by, 3, 3, WHITE);

        // Banner de Victoria Centrado
        if (gameOver) {
            const int16_t bw = 104;
            const int16_t bh = 34;
            const int16_t bxPos = (128 - bw) / 2;
            const int16_t byPos = 16;
            arduboy.fillRect(bxPos, byPos, bw, bh, BLACK);
            arduboy.drawRoundRect(bxPos, byPos, bw, bh, 3, WHITE);

            arduboy.setCursor((128 - (14 * 6)) / 2, byPos + 5);
            if (winnerP == 1) arduboy.print("GANA JUGADOR 1");
            else arduboy.print("GANA JUGADOR 2");

            arduboy.setCursor((128 - (17 * 6)) / 2, byPos + 18);
            arduboy.print("A:Revancha B:Menu");
        } else if (wifiGamepads.pausedBy != 0) {
            // Cartel de Pausa Web Centrado
            const int16_t pw = 108;
            const int16_t ph = 36;
            const int16_t pxPos = (128 - pw) / 2;
            const int16_t pyPos = 14;
            arduboy.fillRect(pxPos, pyPos, pw, ph, BLACK);
            arduboy.drawRoundRect(pxPos, pyPos, pw, ph, 3, WHITE);

            arduboy.setCursor((128 - (5 * 6)) / 2, pyPos + 4);
            arduboy.print("PAUSA");

            arduboy.setCursor((128 - (14 * 6)) / 2, pyPos + 14);
            if (wifiGamepads.pausedBy == 1) arduboy.print("PAUSADO POR P1");
            else arduboy.print("PAUSADO POR P2");

            arduboy.setCursor((128 - (16 * 6)) / 2, pyPos + 24);
            arduboy.print("Solo P");
            arduboy.print(wifiGamepads.pausedBy);
            arduboy.print(" despausa");
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
            case PP_MODE_WIFI_2P:
                updateWifi2P();
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
            case PP_MODE_WIFI_2P:
                drawWifi2P();
                break;
        }
    }
};
