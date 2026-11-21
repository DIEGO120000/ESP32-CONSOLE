#pragma once

#include <Arduboy2.h>
#include <ArduboyTones.h>
#include <EEPROM.h>
#include "hopper_eeprom.h"

// =============================================================================
// CAPA DE COMPATIBILIDAD ARDUBOY 1.1.1 -> HAL DE LA CONSOLA
//
// MyArduboy conserva la superficie que usa el juego de obono (fuente 6x6,
// printEx, rectangulos con altura signed, botones con flanco, playScore2) pero
// se apoya en Arduboy2/ArduboyTones. No contiene logica de Hopper.
// =============================================================================
namespace HopperGame {

typedef uint8_t buffer_t;

// Reproductor del formato de partituras de ArduboyPlaytune (0x90 nota,
// 0x80 stop, espera de 15 bits en ms, 0xD0 mark, 0xE0 repeat, 0xF0 stop).
// Monofonico sobre el buzzer PWM de la consola.
class HopperTune {
public:
    void playScore(const uint8_t *score, uint8_t priority);
    void stopScore();
    void update();

private:
    void step();

    const uint8_t *scoreStart = nullptr;
    const uint8_t *cursor = nullptr;
    bool playing = false;
    uint8_t priority = 0;
    uint16_t noteFreq = 0;
    bool noteOn = false;
    uint32_t waitUntil = 0;
    uint8_t repeat = 0;
};

class MyArduboy : public Arduboy2 {
public:
    HopperTune tunes;

    void beginNoLogo(void);
    bool nextFrame(void);

    // El bucle de la consola ya llama a pollButtons() una vez por frame; el
    // juego necesita su propio latch para buttonDown/buttonUp.
    void latchButtons(void);
    bool buttonDown(uint8_t buttons);
    bool buttonPressed(uint8_t buttons);
    bool buttonUp(uint8_t buttons);

    void setTextColor(uint8_t color);
    void setTextColor(uint8_t color, uint8_t bg);

    size_t printEx(int16_t x, int16_t y, const char *p);
    size_t printEx(int16_t x, int16_t y, const __FlashStringHelper *p);
    size_t write(uint8_t c) override;

    // Arduboy 1.1.1 dibuja los bitmaps por columnas/paginas; el HAL base usa
    // formato horizontal. Se restaura el formato original para los sprites.
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                    int16_t w, int16_t h, uint8_t color = WHITE);

    void drawRect2(int16_t x, int16_t y, uint8_t w, int8_t h, uint8_t color);
    void drawFastVLine2(int16_t x, int16_t y, int8_t h, uint8_t color);
    void drawFastHLine2(int16_t x, int16_t y, uint8_t w, uint8_t color);
    void fillRect2(int16_t x, int16_t y, uint8_t w, int8_t h, uint8_t color);

    bool isAudioEnabled(void);
    void setAudioEnabled(bool on);
    void saveAudioOnOff(void);
    void playScore2(const byte *score, uint8_t priority);
    void stopScore2(void);

private:
    void myDrawChar(int16_t x, int16_t y, unsigned char c,
                    uint8_t color, uint8_t bg, uint8_t size);
    void fillBeltBlack(buffer_t *p, uint8_t d, uint8_t w);
    void fillBeltWhite(buffer_t *p, uint8_t d, uint8_t w);

    uint8_t textColor = WHITE;
    uint8_t textBg = BLACK;
    uint8_t lastButtonState = 0;
    uint8_t currentButtonState = 0;
};

} // namespace HopperGame
