#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#endif
#if defined(PC_EMULATOR)
// Salida de audio del emulador (onda cuadrada en sim/main_sim.cpp).
void simAudioTone(uint16_t freq);
void simAudioOff();
#endif

// =============================================================================
// DRIVER DE AUDIO - ALTAVOZ MAGNETICO 8 ohm via NPN 2N2222 (GPIO 1)
// =============================================================================
// El pin excita la base del 2N2222 con una ONDA CUADRADA del periferico LEDC,
// nunca con niveles DC fijos:
//   * la frecuencia del PWM fija la nota (Hz);
//   * el ciclo de trabajo (duty) fija el volumen, repartido entre 5 % y 45 %
//     del periodo. Nunca 50 % fijo: la bobina de 8 ohm no satura y el
//     regulador de 3.3 V no se hunde en cada semiciclo.
//   * el silencio es duty = 0 con el pad en bajo: el transistor queda en corte
//     total (0 mA, sin ruido de fondo).
//
// El canal LEDC se engancha UNA sola vez (setSpeakerPin) y se queda enganchado:
// los cambios de nota usan ledcChangeFrequency y el silencio es duty 0. Antes
// cada tone() hacia ledcDetach + ledcAttach, y ese reenganche por nota era lo
// que producia los golpes secos ("prap prap") al encadenar tonos cortos.
class ArduboyTones {
public:
    static constexpr uint8_t kVolumeSteps = 5;    // 0 = MUTE, 1..5 = 20 %..100 %
    static constexpr uint8_t kPwmResolution = 10; // 1024 niveles de duty

private:
    static int speakerPin;
    static bool audioEnabled;
    static uint8_t volumeLevel; // 0..kVolumeSteps
    static uint16_t currentFreq;
    static unsigned long toneEndTime;
    static bool isTonePlaying;
    static bool pwmReady;

    // Duty en cuentas (0..1023) para un nivel de volumen:
    // MUTE = 0; 20/40/60/80/100 % -> 5/15/25/35/45 % del periodo.
    static uint32_t dutyFor(uint8_t level) {
        if (level == 0) return 0;
        if (level > kVolumeSteps) level = kVolumeSteps;
        const uint32_t maxDuty = (1UL << kPwmResolution) - 1UL;
        const uint32_t pct = 5UL + (uint32_t)(level - 1) * 10UL;
        return (maxDuty * pct) / 100UL;
    }

    // Arranca o retuna el PWM a `freq` sin soltar el canal LEDC.
    static void pwmTone(uint16_t freq) {
#if defined(ESP32)
        if (speakerPin < 0) return;
        if (!pwmReady) {
            if (!ledcAttach((uint8_t)speakerPin, freq, kPwmResolution)) return;
            pwmReady = true;
        } else if (freq != currentFreq) {
            ledcChangeFrequency((uint8_t)speakerPin, freq, kPwmResolution);
        }
        currentFreq = freq;
        ledcWrite((uint8_t)speakerPin, dutyFor(volumeLevel));
#endif
    }

    static void startTone(uint16_t freq, uint16_t dur) {
        if (freq == 0 || !audioEnabled || volumeLevel == 0) {
            silenceHardware();
            return;
        }
#if defined(ESP32)
        pwmTone(freq);
#elif defined(PC_EMULATOR)
        simAudioTone(freq);
#endif
        isTonePlaying = true;
        toneEndTime = (dur > 0) ? (millis() + dur) : 0;
    }

public:
    ArduboyTones(bool (*)(void)) {}
    ArduboyTones() {}

    static void setSpeakerPin(int pin) {
        speakerPin = pin;
#if defined(ESP32)
        if (speakerPin >= 0) {
            // Pad en bajo ANTES de enganchar el LEDC: el transistor arranca en
            // corte y no hay pulso parásito al inicializar el periférico.
            gpio_reset_pin((gpio_num_t)speakerPin);
            gpio_set_direction((gpio_num_t)speakerPin, GPIO_MODE_OUTPUT);
            gpio_pullup_dis((gpio_num_t)speakerPin);
            gpio_pulldown_en((gpio_num_t)speakerPin);
            gpio_set_level((gpio_num_t)speakerPin, 0);
        }
#endif
        pwmReady = false;
        currentFreq = 0;
        isTonePlaying = false;
        toneEndTime = 0;
    }

    static void enableAudio(bool en) {
        audioEnabled = en;
        if (!audioEnabled) {
            silenceHardware();
        }
    }

    // Nivel 0..5 (MUTE, 20 %, 40 %, 60 %, 80 %, 100 %). Si hay una nota
    // sonando, el duty se recalcula al instante.
    static void setVolume(uint8_t vol) {
        if (vol > kVolumeSteps) vol = kVolumeSteps;
        volumeLevel = vol;
#if defined(ESP32)
        if (volumeLevel > 0 && isTonePlaying && speakerPin >= 0) {
            ledcWrite((uint8_t)speakerPin, dutyFor(volumeLevel));
            return;
        }
#endif
        if (volumeLevel == 0) {
            silenceHardware();
        }
    }

    static uint8_t getVolume() {
        return volumeLevel;
    }

    // Duty efectivo en % del periodo (para diagnostico / panel web).
    static uint8_t getDutyPercent() {
        return (volumeLevel == 0) ? 0 : (uint8_t)(5 + (volumeLevel - 1) * 10);
    }

    // duty = 0 y pad en bajo: 2N2222 en corte, 0 mA en reposo.
    static void silenceHardware() {
#if defined(ESP32)
        if (speakerPin >= 0) {
            if (pwmReady) {
                // duty 0: la onda queda plana en bajo sin soltar el canal, asi
                // la siguiente nota no paga un ledcAttach (que hacia "prap").
                ledcWrite((uint8_t)speakerPin, 0);
                gpio_set_level((gpio_num_t)speakerPin, 0);
            } else {
                pinMode(speakerPin, OUTPUT);
                digitalWrite(speakerPin, LOW);
            }
        }
#endif
#if defined(PC_EMULATOR)
        simAudioOff();
#endif
        isTonePlaying = false;
        toneEndTime = 0;
    }

    static void tone(uint16_t freq) {
        if (speakerPin < 0) return;
        startTone(freq, 0);
    }

    static void tone(uint16_t freq, uint16_t dur) {
        if (speakerPin < 0) return;
        if (dur == 0) {
            tone(freq);
            return;
        }
        startTone(freq, dur);
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
inline uint16_t ArduboyTones::currentFreq = 0;
inline unsigned long ArduboyTones::toneEndTime = 0;
inline bool ArduboyTones::isTonePlaying = false;
inline bool ArduboyTones::pwmReady = false;
