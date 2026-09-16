#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#endif

class BeepPin1 {
public:
    static uint8_t duration;

    static void begin() {
        silence();
        duration = 0;
    }

    static void tone(uint16_t count) {
        if (count > 0) {
            #if defined(ESP32)
            ledcDetach(2);
            esp_rom_gpio_pad_select_gpio(2);
            ledcAttach(2, count, 8);
            ledcWrite(2, 128);
            #else
            ::tone(2, count);
            #endif
        } else {
            noTone();
        }
    }

    static void tone(uint16_t count, uint8_t dur) {
        duration = dur;
        if (count > 0) {
            #if defined(ESP32)
            ledcDetach(2);
            esp_rom_gpio_pad_select_gpio(2);
            ledcAttach(2, count, 8);
            ledcWrite(2, 128);
            #else
            ::tone(2, count);
            #endif
        } else {
            noTone();
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

    static void silence() {
        #if defined(ESP32)
        ledcWrite(2, 0);
        ledcDetach(2);
        esp_rom_gpio_pad_select_gpio(2);
        gpio_reset_pin(GPIO_NUM_2);
        gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
        gpio_pullup_dis(GPIO_NUM_2);
        gpio_pulldown_en(GPIO_NUM_2);
        gpio_set_level(GPIO_NUM_2, 0);
        #else
        pinMode(2, OUTPUT);
        digitalWrite(2, LOW);
        #endif
    }

    static void noTone() {
        silence();
    }

    static constexpr uint16_t freq(uint16_t hz) {
        return hz;
    }
};

inline uint8_t BeepPin1::duration = 0;
typedef BeepPin1 BeepPin2;


