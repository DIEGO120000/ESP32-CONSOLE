#pragma once
// =============================================================================
// HAL DE PANTALLA/BOTONES PARA EL EMULADOR DE PC (solo [env:pc_emulator])
// =============================================================================
// Sustituye a include/Arduboy2Core.h en el build nativo manteniendo exactamente
// la misma interfaz que usa Arduboy2.h y los ports. El framebuffer real de
// 1024 bytes se comparte con SDL2 desde sim/main_sim.cpp.
// =============================================================================

#include <Arduino.h>
#include "EEPROM.h"

#define WIDTH  128
#define HEIGHT 64

#define INVERT 2
#undef WHITE
#undef BLACK
#define WHITE 1
#define BLACK 0

#define SSD1306_I2C_ADDR   0x3C
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON  0xAF

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

#define UP_BUTTON    _BV(7)
#define RIGHT_BUTTON _BV(6)
#define LEFT_BUTTON  _BV(5)
#define DOWN_BUTTON  _BV(4)
#define A_BUTTON     _BV(3)
#define B_BUTTON     _BV(2)
#define CLICK_BUTTON _BV(1)
#define SYSTEM_MENU_BUTTON CLICK_BUTTON
#define SYS_BUTTON CLICK_BUTTON

// Implementadas en sim/main_sim.cpp.
uint8_t simButtonsState();
void simPresentFrame(const uint8_t *buffer, bool inverted);

class Arduboy2Core {
public:
    static uint8_t sBuffer[WIDTH * HEIGHT / 8];
    static uint8_t currentButtonState;
    static uint8_t previousButtonState;

    static void init() { boot(); }

    static void boot() {
        clear();
        display();
    }

    static void sendCommand(uint8_t) {}
    static void setContrast(uint8_t) {}

    static void invert(bool inverse) { invertActive = inverse; }

    static void display() { simPresentFrame(sBuffer, invertActive); }

    static void clear() { memset(sBuffer, 0, sizeof(sBuffer)); }

    static void fillScreen(uint8_t color) {
        memset(sBuffer, color ? 0xFF : 0x00, sizeof(sBuffer));
    }

    static uint8_t *getBuffer() { return sBuffer; }

    // Los botones llegan del teclado (sim/main_sim.cpp).
    static uint8_t buttonsState() { return simButtonsState(); }

    static void setRGBled(uint8_t, uint8_t, uint8_t) {}
    static void setRGBled(uint8_t, uint8_t) {}
    static void digitalWriteRGB(uint8_t, uint8_t, uint8_t) {}

    // Bateria fija: 4.10 V (~92 %) para que la UI no parpadee en el emulador.
    static void updateBatteryTelemetry(bool = false) {}
    static void initBattery() {}
    static uint32_t getBatteryRawMilliVolts() { return 2050; }
    static float getBatteryVoltage() { return 4.10f; }
    static int getBatteryPercentage() { return 92; }

private:
    static bool invertActive;
};
