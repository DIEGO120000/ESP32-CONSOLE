#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#endif

class ArduboyTones {
private:
    static int speakerPin;
    static bool audioEnabled;
    static uint8_t volumeLevel;
    static unsigned long toneEndTime;
    static bool isTonePlaying;

public:
    ArduboyTones(bool (*)(void)) {}
    ArduboyTones() {}

    static void setSpeakerPin(int pin) {
        speakerPin = pin;
        silenceHardware();
    }

    static void setVolume(uint8_t vol) {
        if (vol > 3) vol = 3;
        volumeLevel = vol;
        if (volumeLevel == 0) {
            silenceHardware();
        }
    }

    static uint8_t getVolume() {
        return volumeLevel;
    }

    static void silenceHardware() {
        #if defined(ESP32)
        if (speakerPin >= 0) {
            ledcWrite(speakerPin, 0);
            ledcDetach(speakerPin);
            esp_rom_gpio_pad_select_gpio((uint32_t)speakerPin);
            gpio_reset_pin((gpio_num_t)speakerPin);
            gpio_set_direction((gpio_num_t)speakerPin, GPIO_MODE_OUTPUT);
            gpio_pullup_dis((gpio_num_t)speakerPin);
            gpio_pulldown_en((gpio_num_t)speakerPin);
            gpio_set_level((gpio_num_t)speakerPin, 0);
        }
        #endif
        isTonePlaying = false;
        toneEndTime = 0;
    }

    static void tone(uint16_t freq) {
        if (speakerPin >= 0) {
            if (freq > 0 && audioEnabled && volumeLevel > 0) {
                #if defined(ESP32)
                ledcDetach(speakerPin);
                esp_rom_gpio_pad_select_gpio((uint32_t)speakerPin);
                ledcAttach(speakerPin, freq, 8);
                const uint8_t dutyTable[4] = {0, 20, 64, 128};
                ledcWrite(speakerPin, dutyTable[volumeLevel]);
                #endif
                isTonePlaying = true;
                toneEndTime = 0;
            } else {
                noTone();
            }
        }
    }

    static void tone(uint16_t freq, uint16_t dur) {
        if (speakerPin >= 0) {
            if (freq > 0 && dur > 0 && audioEnabled && volumeLevel > 0) {
                #if defined(ESP32)
                ledcDetach(speakerPin);
                esp_rom_gpio_pad_select_gpio((uint32_t)speakerPin);
                ledcAttach(speakerPin, freq, 8);
                const uint8_t dutyTable[4] = {0, 20, 64, 128};
                ledcWrite(speakerPin, dutyTable[volumeLevel]);
                #endif
                isTonePlaying = true;
                toneEndTime = millis() + dur;
            } else {
                noTone();
            }
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
        silenceHardware();
    }

    static void update() {
        if (isTonePlaying && toneEndTime > 0) {
            if (millis() >= toneEndTime) {
                silenceHardware();
            }
        }
    }

    static bool playing() {
        return isTonePlaying;
    }

    static void volumeMode(uint8_t mode) {}
};

inline int ArduboyTones::speakerPin = -1;
inline bool ArduboyTones::audioEnabled = false;
inline uint8_t ArduboyTones::volumeLevel = 0;
inline unsigned long ArduboyTones::toneEndTime = 0;
inline bool ArduboyTones::isTonePlaying = false;


