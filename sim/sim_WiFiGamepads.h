#pragma once
// =============================================================================
// STUB DEL AP DE MANDOS 2P (solo [env:pc_emulator])
// =============================================================================
// Ping Pong compila su modo "2P WiFi Gamepads" en el build nativo. En PC no hay
// Wi-Fi: begin() arranca un AP simulado sin clientes, de modo que el juego entra
// en su submenu y se puede salir con B sin bloquear el emulador.
// =============================================================================

#include <Arduino.h>

class WiFiGamepadManager {
public:
    volatile bool p1Up = false;
    volatile bool p1Down = false;
    volatile bool p2Up = false;
    volatile bool p2Down = false;
    volatile uint8_t pausedBy = 0;

    bool p1Connected = false;
    bool p2Connected = false;
    bool isRunning = false;
    volatile unsigned long lastInputMs = 0;

    void begin() { isRunning = true; }
    void stop() {
        isRunning = false;
        p1Connected = p2Connected = false;
        p1Up = p1Down = p2Up = p2Down = false;
        pausedBy = 0;
    }
    void update() {}
};

inline WiFiGamepadManager wifiGamepads;
