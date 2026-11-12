#pragma once

// =============================================================================
// Capa de plataforma para el motor WolfenduinoFX en la consola ESP32-C6.
//
// Sustituye a ArduboyPlatform.h del proyecto original (AVR + Arduboy FX):
//  - Pinta sobre el framebuffer de Arduboy2 en lugar de usar asm AVR.
//  - diskRead() lee de la seccion de datos incrustada en flash (wolf_fxdata.cpp).
//  - El guardado usa la EEPROM emulada del proyecto.
//  - El audio usa el buzzer de la consola.
// =============================================================================

#include <stdint.h>
#include <string.h>

#include "Defines.h"

#define INPUT_BIT(x) (1 << (x))

enum
{
    Input_Dpad_Up    = INPUT_BIT(0),
    Input_Dpad_Right = INPUT_BIT(1),
    Input_Dpad_Down  = INPUT_BIT(2),
    Input_Dpad_Left  = INPUT_BIT(3),
    Input_Btn_A      = INPUT_BIT(4),
    Input_Btn_B      = INPUT_BIT(5),
    Input_Btn_C      = INPUT_BIT(6),
};

class PlatformBase
{
public:
    uint8_t readInput() { return inputState; }
    bool isMuted() { return m_isMuted; }
    void setMuted(bool muted) { m_isMuted = muted; }

    uint8_t inputState;
    bool m_isMuted : 1;
};

// Buffer de video compartido con main.cpp (mismo formato de paginas).
uint8_t* GetScreenBuffer();

inline void drawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= DISPLAYWIDTH || y >= DISPLAYHEIGHT) return;
    uint8_t* buffer = GetScreenBuffer();
    uint16_t rowOffset = (uint16_t)((y >> 3) * DISPLAYWIDTH + x);
    uint8_t bit = (uint8_t)(1u << (y & 7));

    uint8_t data = buffer[rowOffset] | bit;
    if (!(color & 1)) data ^= bit;
    buffer[rowOffset] = data;
}

inline void setPixel(uint8_t x, uint8_t y)
{
    drawPixel(x, y, 0);
}

inline void clearPixel(uint8_t x, uint8_t y)
{
    drawPixel(x, y, 1);
}

inline void clearDisplay(uint8_t colour)
{
    memset(GetScreenBuffer(), colour ? 0xff : 0, DISPLAYWIDTH * DISPLAYHEIGHT / 8);
}

class ArduboyPlatform : public PlatformBase
{
public:
    void playSound(uint8_t id);
    void update();
};

extern ArduboyPlatform Platform;

void ERROR(const char* msg);

// Acceso a la seccion de datos FX incrustada (direcciones de 24 bits).
extern const uint8_t WOLF_FX_DATA[];
extern const uint32_t WOLF_FX_DATA_LEN;

inline void diskRead(uint32_t address, uint8_t* buffer, int length)
{
    if (address >= WOLF_FX_DATA_LEN) {
        memset(buffer, 0, length);
        return;
    }
    if (address + (uint32_t)length > WOLF_FX_DATA_LEN) {
        length = (int)(WOLF_FX_DATA_LEN - address);
    }
    memcpy(buffer, WOLF_FX_DATA + address, length);
}

void writeSaveFile(const uint8_t* buffer, int length);
bool readSaveFile(uint8_t* buffer, int length);
