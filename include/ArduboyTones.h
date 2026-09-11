#pragma once
#include <Arduino.h>

class ArduboyTones {
private:
    static int speakerPin;
    static bool audioEnabled;

public:
    ArduboyTones(bool (*)(void)) {}
    ArduboyTones() {}

    static void setSpeakerPin(int pin) {
        speakerPin = pin;
    }

    static void tone(uint16_t freq) {
        if (speakerPin >= 0 && freq > 0) {
            #if defined(ESP32)
            ::tone(speakerPin, freq);
            #endif
        }
    }

    static void tone(uint16_t freq, uint16_t dur) {
        if (speakerPin >= 0 && freq > 0) {
            #if defined(ESP32)
            ::tone(speakerPin, freq, dur);
            #endif
        }
    }

    static void tone(uint16_t freq1, uint16_t dur1, uint16_t freq2, uint16_t dur2) {
        tone(freq1, dur1);
    }

    static void tone(uint16_t freq1, uint16_t dur1, uint16_t freq2, uint16_t dur2, uint16_t freq3, uint16_t dur3) {
        tone(freq1, dur1);
    }

    static void tones(const uint16_t *tonesArray) {}
    static void tonesInRAM(uint16_t *tonesArray) {}

    static void noTone() {
        if (speakerPin >= 0) {
            #if defined(ESP32)
            ::noTone(speakerPin);
            #endif
        }
    }

    static bool playing() {
        return false;
    }

    static void volumeMode(uint8_t mode) {}
};

inline int ArduboyTones::speakerPin = 2;
inline bool ArduboyTones::audioEnabled = true;
