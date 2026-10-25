#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#endif

class BeepPin1 {
public:
    static uint8_t duration;

    static void begin() {}
    static void tone(uint16_t count) {}
    static void tone(uint16_t count, uint8_t dur) {}
    static void timer() {}
    static void silence() {}
    static void noTone() {}
    static constexpr uint16_t freq(uint16_t hz) {
        return hz;
    }
};

inline uint8_t BeepPin1::duration = 0;
typedef BeepPin1 BeepPin2;


