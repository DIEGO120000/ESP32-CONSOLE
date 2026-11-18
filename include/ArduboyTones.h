#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_timer.h"
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
//   * el ciclo de trabajo (duty) fija el volumen. Resolucion de 12 bits
//     (0..4095) y curva logaritmica para que los pasos bajos sean de verdad
//     suaves: 35 (0.85 %) para uso nocturno y 1720 (42 %) como tope seguro,
//     que nunca satura la bobina de 8 ohm ni hunde el regulador de 3.3 V.
//   * el silencio es duty = 0 con el pad en bajo: el transistor queda en corte
//     total (0 mA, sin ruido de fondo).
//
// El canal LEDC se engancha UNA sola vez (setSpeakerPin) y se queda enganchado:
// los cambios de nota usan ledcChangeFrequency y el silencio es duty 0. Antes
// cada tone() hacia ledcDetach + ledcAttach, y ese reenganche por nota era lo
// que producia los golpes secos ("prap prap") al encadenar tonos cortos.
//
// SECUENCIAS: tones()/tonesInRAM() recorren el array original de Arduboy
// (pares frecuencia, duracion en ms) con un esp_timer de un disparo por nota,
// sin bloquear el render. Soporta:
//   * bit 15 (TONE_HIGH_VOLUME) en la frecuencia: se enmascara para obtener los
//     Hz reales y la nota suena con el doble de duty (acento);
//   * frecuencia 0: pausa, duty 0 durante la duracion indicada;
//   * TONES_END (0x8000): fin de la secuencia;
//   * TONES_REPEAT (0x8001): vuelve al inicio en bucle.
class ArduboyTones {
public:
    // Escala de volumen: MUTE, 5, 10, 20, 40, 70 y 100 % (este ultimo = duty
    // 1720/4095, el tope seguro de la bobina).
    static constexpr uint8_t kVolumeSteps = 6;
    static constexpr uint8_t kPwmResolution = 12; // 4096 niveles de duty
    static constexpr uint16_t kMaxDuty = 1720;    // 42 % del periodo

    // Duty en cuentas (0..4095) por nivel: curva logaritmica del oido.
    static constexpr uint16_t kDutyTable[kVolumeSteps + 1] = {
        0,    // MUTE
        35,   // 5 %   -> duty 0.85 %
        90,   // 10 %  -> duty 2.2 %
        220,  // 20 %  -> duty 5.4 %
        520,  // 40 %  -> duty 12.7 %
        1050, // 70 %  -> duty 25.6 %
        1720, // 100 % -> duty 42.0 %
    };

    // Marcadores del formato de secuencias de ArduboyTones.
    static constexpr uint16_t kToneEnd = 0x8000;     // TONES_END
    static constexpr uint16_t kToneRepeat = 0x8001;  // TONES_REPEAT
    static constexpr uint16_t kHighVolumeBit = 0x8000; // TONE_HIGH_VOLUME
    static constexpr uint16_t kFrequencyMask = 0x7FFF;

    static constexpr uint8_t kMultiBufWords = 16;

private:
    static int speakerPin;
    static bool audioEnabled;
    static uint8_t volumeLevel; // 0..kVolumeSteps
    static uint16_t currentFreq;
    static unsigned long noteEndTime; // 0 = nota infinita
    static bool isTonePlaying;
    static bool pwmReady;
    static bool noteHighVolume;

    // Secuencia en curso (flash o RAM: en ESP32 ambas son direccionables).
    static const uint16_t* seqPtr;
    static const uint16_t* seqStart;
    static bool seqActive;
    static bool timerOk;
    static uint16_t multiBuf[kMultiBufWords];
#if defined(ESP32)
    static esp_timer_handle_t noteTimer;
#endif

    static uint16_t dutyFor(uint8_t level, bool highVolume) {
        if (level == 0 || level > kVolumeSteps) return 0;
        uint32_t duty = kDutyTable[level];
        // Las notas con bit 15 del formato original son acentos: doble duty,
        // siempre con el tope seguro de la bobina.
        if (highVolume) duty *= 2;
        if (duty > kMaxDuty) duty = kMaxDuty;
        return (uint16_t)duty;
    }

    // Arranca o retuna el PWM a `freq` sin soltar el canal LEDC.
    static void pwmTone(uint16_t freq, bool highVolume) {
#if defined(ESP32)
        if (speakerPin < 0 || freq == 0) return;
        if (!pwmReady) {
            if (!ledcAttach((uint8_t)speakerPin, freq, kPwmResolution)) return;
            pwmReady = true;
        } else if (freq != currentFreq) {
            ledcChangeFrequency((uint8_t)speakerPin, freq, kPwmResolution);
        }
        currentFreq = freq;
        ledcWrite((uint8_t)speakerPin, dutyFor(volumeLevel, highVolume));
#elif defined(PC_EMULATOR)
        (void)highVolume;
        simAudioTone(freq);
#endif
    }

    // Apaga la salida sin tocar el estado de la secuencia (permite mutear y
    // desmutear a mitad de una melodia).
    static void silencePwm() {
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
#elif defined(PC_EMULATOR)
        simAudioOff();
#endif
        isTonePlaying = false;
    }

    static void stopTimer() {
#if defined(ESP32)
        if (noteTimer != nullptr) esp_timer_stop(noteTimer);
#endif
    }

    // Un disparo por nota. Si esp_timer no esta disponible, update() hace de
    // respaldo con millis() (y en PC_EMULATOR es el unico camino).
    static void armTimer(uint32_t ms) {
#if defined(ESP32)
        if (noteTimer == nullptr) {
            esp_timer_create_args_t args = {};
            args.callback = &ArduboyTones::timerCallback;
            args.name = "arduboy_tone";
            args.dispatch_method = ESP_TIMER_TASK;
            if (esp_timer_create(&args, &noteTimer) != ESP_OK) {
                noteTimer = nullptr;
                timerOk = false;
                return;
            }
        }
        timerOk = true;
        esp_timer_stop(noteTimer);
        esp_timer_start_once(noteTimer, (uint64_t)ms * 1000ULL);
#else
        (void)ms;
#endif
    }

    static void startNote(uint16_t freq, uint16_t dur, bool highVolume) {
        noteHighVolume = highVolume;
        if (freq == 0 || !audioEnabled || volumeLevel == 0) {
            silencePwm();
        } else {
            pwmTone(freq, highVolume);
            isTonePlaying = true;
        }
        if (dur == 0) {
            // tone(freq) sin duracion: suena hasta noTone().
            noteEndTime = 0;
            stopTimer();
            return;
        }
        noteEndTime = millis() + dur;
        armTimer(dur);
    }

    static void stopSequence() {
        seqActive = false;
        seqPtr = nullptr;
        seqStart = nullptr;
        stopTimer();
        silencePwm();
        noteEndTime = 0;
    }

    // Lee el siguiente par (frecuencia, duracion) del array original.
    static void playNextNote() {
        if (!seqActive || seqPtr == nullptr) {
            silencePwm();
            return;
        }
        const uint16_t word = *seqPtr++;
        if (word == kToneEnd) {
            stopSequence();
            return;
        }
        if (word == kToneRepeat) {
            seqPtr = seqStart;
            playNextNote();
            return;
        }
        const uint16_t dur = *seqPtr++;
        // El bit 15 solo marca volumen alto: los Hz reales son los 15 bits bajos.
        const bool highVolume = (word & kHighVolumeBit) != 0;
        startNote(word & kFrequencyMask, dur, highVolume);
    }

    static void startSequence(const uint16_t* array) {
        stopSequence();
        if (array == nullptr) return;
        seqStart = array;
        seqPtr = array;
        seqActive = true;
        playNextNote();
    }

    static void timerCallback(void*) {
        if (seqActive) {
            playNextNote();
        } else {
            silencePwm();
            noteEndTime = 0;
        }
    }

    // Rellena multiBuf con los tonos sueltos y lo reproduce como secuencia.
    static void startMultiTone(uint8_t pairs, const uint16_t* flat) {
        uint8_t n = 0;
        for (uint8_t i = 0; i < pairs && (n + 2) <= (kMultiBufWords - 1); i++) {
            multiBuf[n++] = flat[i * 2];
            multiBuf[n++] = flat[i * 2 + 1];
        }
        multiBuf[n] = kToneEnd;
        startSequence(multiBuf);
    }

public:
    static uint8_t indexForPercent(uint8_t percent) {
        // Nivel mas cercano de la escala logaritmica (migracion y panel web).
        static const uint8_t kPercent[kVolumeSteps] = {5, 10, 20, 40, 70, 100};
        if (percent == 0) return 0;
        uint8_t best = 1;
        uint8_t bestDiff = 255;
        for (uint8_t i = 1; i <= kVolumeSteps; i++) {
            const uint8_t p = kPercent[i - 1];
            const uint8_t diff = (p > percent) ? (p - percent) : (percent - p);
            if (diff < bestDiff) {
                bestDiff = diff;
                best = i;
            }
        }
        return best;
    }

    static uint8_t percentOfLevel(uint8_t level) {
        static const uint8_t kPercent[kVolumeSteps] = {5, 10, 20, 40, 70, 100};
        if (level == 0) return 0;
        if (level > kVolumeSteps) level = kVolumeSteps;
        return kPercent[level - 1];
    }

    static uint16_t dutyOfLevel(uint8_t level) {
        return dutyFor(level, false);
    }

    ArduboyTones(bool (*)(void)) {}
    ArduboyTones() {}

    static void setSpeakerPin(int pin) {
        speakerPin = pin;
#if defined(ESP32)
        if (speakerPin >= 0) {
            // Pad en bajo ANTES de enganchar el LEDC: el transistor arranca en
            // corte y no hay pulso parasito al inicializar el periferico.
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
        noteEndTime = 0;
        seqActive = false;
        seqPtr = nullptr;
        seqStart = nullptr;
    }

    static void enableAudio(bool en) {
        audioEnabled = en;
        if (!audioEnabled) {
            noTone();
        }
    }

    // Usado por Arduboy2Audio (arduboy.audio.enabled()).
    static bool isAudioEnabled() {
        return audioEnabled;
    }

    // Nivel 0..6 (MUTE, 5 %, 10 %, 20 %, 40 %, 70 %, 100 %). Si hay una nota
    // sonando, el duty se recalcula al instante.
    static void setVolume(uint8_t vol) {
        if (vol > kVolumeSteps) vol = kVolumeSteps;
        volumeLevel = vol;
        if (volumeLevel == 0) {
            silencePwm();
            return;
        }
        if (isTonePlaying && currentFreq > 0) {
            pwmTone(currentFreq, noteHighVolume);
        }
    }

    static uint8_t getVolume() {
        return volumeLevel;
    }

    // Duty efectivo en % del periodo (para diagnostico / panel web).
    static uint8_t getDutyPercent() {
        if (volumeLevel == 0) return 0;
        const uint32_t duty = dutyFor(volumeLevel, false);
        return (uint8_t)((duty * 100UL + (1UL << kPwmResolution) / 2) >> kPwmResolution);
    }

    static void tone(uint16_t freq) {
        if (speakerPin < 0) return;
        stopSequence();
        startNote(freq, 0, false);
    }

    static void tone(uint16_t freq, uint16_t dur) {
        if (speakerPin < 0) return;
        if (dur == 0) {
            tone(freq);
            return;
        }
        stopSequence();
        startNote(freq, dur, false);
    }

    // Dos y tres tonos encadenados (API original de ArduboyTones).
    static void tone(uint16_t freq1, uint16_t dur1, uint16_t freq2, uint16_t dur2) {
        const uint16_t flat[4] = {freq1, dur1, freq2, dur2};
        startMultiTone(2, flat);
    }

    static void tone(uint16_t freq1, uint16_t dur1, uint16_t freq2, uint16_t dur2,
                     uint16_t freq3, uint16_t dur3) {
        const uint16_t flat[6] = {freq1, dur1, freq2, dur2, freq3, dur3};
        startMultiTone(3, flat);
    }

    // Secuencia original (frecuencia, duracion) terminada en TONES_END /
    // TONES_REPEAT. Sirve para arrays en flash (PROGMEM) o const en RAM.
    static void tones(const uint16_t* tonesArray) {
        startSequence(tonesArray);
    }

    static void tonesInRAM(uint16_t* tonesArray) {
        startSequence((const uint16_t*)tonesArray);
    }

    // duty = 0 y pad en bajo: 2N2222 en corte, 0 mA en reposo.
    static void silenceHardware() {
        stopSequence();
        silencePwm();
    }

    static void noTone() {
        silenceHardware();
    }

    // Respaldo de temporizacion cuando esp_timer no esta disponible.
    static void update() {
        if (timerOk) return;
        if (noteEndTime == 0) return;
        if ((int32_t)(millis() - noteEndTime) < 0) return;
        if (seqActive) {
            playNextNote();
        } else {
            silencePwm();
            noteEndTime = 0;
        }
    }

    static bool playing() {
        return isTonePlaying || seqActive;
    }

    static void volumeMode(uint8_t mode) {}
};

inline int ArduboyTones::speakerPin = -1;
inline bool ArduboyTones::audioEnabled = false;
inline uint8_t ArduboyTones::volumeLevel = 0;
inline uint16_t ArduboyTones::currentFreq = 0;
inline unsigned long ArduboyTones::noteEndTime = 0;
inline bool ArduboyTones::isTonePlaying = false;
inline bool ArduboyTones::pwmReady = false;
inline bool ArduboyTones::noteHighVolume = false;
inline const uint16_t* ArduboyTones::seqPtr = nullptr;
inline const uint16_t* ArduboyTones::seqStart = nullptr;
inline bool ArduboyTones::seqActive = false;
inline bool ArduboyTones::timerOk = false;
inline uint16_t ArduboyTones::multiBuf[ArduboyTones::kMultiBufWords] = {0};
#if defined(ESP32)
inline esp_timer_handle_t ArduboyTones::noteTimer = nullptr;
#endif
