#pragma once
#include <Arduino.h>

class BeepPin1 {
public:
    static uint8_t duration;

    static void begin() {
        pinMode(2, OUTPUT);
        ::noTone(2);
        duration = 0;
    }

    static void tone(uint16_t count) {
        if (count > 0) {
            ::tone(2, count);
        }
    }

    static void tone(uint16_t count, uint8_t dur) {
        duration = dur;
        if (count > 0) {
            ::tone(2, count);
        }
    }

    static void timer() {
        if (duration > 0) {
            duration--;
            if (duration == 0) {
                noTone();
            }
        }
    }

    static void noTone() {
        ::noTone(2);
    }

    static constexpr uint16_t freq(uint16_t hz) {
        return hz;
    }
};

inline uint8_t BeepPin1::duration = 0;
typedef BeepPin1 BeepPin2;
