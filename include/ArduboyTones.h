#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_timer.h"
#endif
#if defined(PC_EMULATOR)
// Salida de audio del emulador (onda cuadrada / PCM en sim/main_sim.cpp).
void simAudioTone(uint16_t freq);
void simAudioOff();
void simAudioPcm(const uint8_t* data, uint32_t len, bool loop);
void simAudioPcmStop();
void simAudioPcmGain(uint8_t gain);
bool simAudioPcmPlaying();
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

    // --- Motor PCM (muestras unsigned 8-bit @ 8000 Hz) ---------------------
    // La portadora LEDC sube a 31.25 kHz con 8 bits de resolucion: muy por
    // encima del audio audible, de modo que el duty cycle reproduce la muestra
    // y el altavoz solo deja pasar la envolvente acustica. Un timer de
    // hardware periodico de 125 us (8 kHz) entrega el byte siguiente desde la
    // ISR sin bloquear el juego. Al agotar la muestra el duty queda en 0: el
    // 2N2222 corta.
    //
    // Nota: el core Arduino-ESP32 de este proyecto compila esp_timer sin
    // CONFIG_ESP_TIMER_SUPPORTS_ISR_DISPATCH_METHOD, y el despacho por tarea
    // agrupa los 8 avisos de cada milisegundo (audio a 1 kHz). Por eso el PCM
    // usa el timer de hardware del SoC (1 MHz / 125 cuentas); esp_timer sigue
    // encargandose de las notas de tone().
    static constexpr uint32_t kPcmSampleRate = 8000;
    static constexpr uint32_t kPcmPeriodUs = 125;
    static constexpr uint32_t kPcmCarrierHz = 31250;
    static constexpr uint8_t kPcmResolution = 8;
    // 42 % de 255: mismo tope de seguridad que la tabla de tonos (1720/4095).
    static constexpr uint8_t kPcmMaxDuty = 107;
    static constexpr uint8_t kPcmDefaultGain = 255;

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

    // Estado compartido con la ISR del timer de hardware (o con SDL2 en el PC).
    static const uint8_t* volatile pcmData;
    static volatile uint32_t pcmLen;
    static volatile uint32_t pcmPos;
    static volatile bool pcmActive;
    static volatile bool pcmLoop;
    static volatile bool pcmPwmReady;
    static volatile bool pcmTimerRunning;
    static volatile uint8_t pcmGain;
#if defined(ESP32)
    static esp_timer_handle_t noteTimer;
    static hw_timer_t* pcmHwTimer;
    static bool pcmHwAttached;
#endif

    // Amplitud efectiva 0..255 = volumen global (0..100 %) x ganancia PCM.
    // La lee el callback en cada muestra, asi una bajada de volumen o la
    // modulacion de la bola de golf se aplica sin reiniciar la muestra.
    static uint8_t pcmAmplitude() {
        const uint32_t pct = percentOfLevel(volumeLevel);
        const uint32_t amp = (pct * 255UL) / 100UL;
        return (uint8_t)((amp * pcmGain) / 255UL);
    }

    static void stopPcmTimer() {
#if defined(ESP32)
        if (pcmHwTimer != nullptr) {
            timerStop(pcmHwTimer);
        }
        pcmTimerRunning = false;
#endif
    }

    // Prepara el canal LEDC para PCM: 31.25 kHz / 8 bits y duty 0 (corte).
    static void startPcmHardware() {
#if defined(ESP32)
        stopPcmTimer();
        if (pcmPwmReady) {
            ledcDetach((uint8_t)speakerPin);
            pcmPwmReady = false;
        }
        pwmReady = false;
        currentFreq = 0;
        gpio_reset_pin((gpio_num_t)speakerPin);
        gpio_set_direction((gpio_num_t)speakerPin, GPIO_MODE_OUTPUT);
        gpio_pullup_dis((gpio_num_t)speakerPin);
        gpio_pulldown_en((gpio_num_t)speakerPin);
        gpio_set_level((gpio_num_t)speakerPin, 0);

        if (!ledcAttach((uint8_t)speakerPin, kPcmCarrierHz, kPcmResolution)) {
            pcmActive = false;
            return;
        }
        pcmPwmReady = true;
        ledcWrite((uint8_t)speakerPin, 0);

        if (pcmHwTimer == nullptr) {
            // 1 MHz -> 125 cuentas = 125 us exactos (8 kHz).
            pcmHwTimer = timerBegin(1000000);
        }
        if (pcmHwTimer == nullptr) {
            pcmActive = false;
            pcmPwmReady = false;
            ledcDetach((uint8_t)speakerPin);
            gpio_set_level((gpio_num_t)speakerPin, 0);
            return;
        }
        if (!pcmHwAttached) {
            timerAttachInterrupt(pcmHwTimer, &ArduboyTones::pcmTimerCallback);
            pcmHwAttached = true;
        }
        timerWrite(pcmHwTimer, 0);
        timerAlarm(pcmHwTimer, kPcmPeriodUs, true, 0);
        timerStart(pcmHwTimer);
        pcmTimerRunning = true;
#elif defined(PC_EMULATOR)
        simAudioPcm(pcmData, pcmLen, pcmLoop);
        simAudioPcmGain(pcmAmplitude());
#endif
    }

#if defined(ESP32)
    // 8 kHz exactos: un byte PCM -> un valor de duty. IRAM para que el
    // streaming no dependa de la cache de Flash.
    static void IRAM_ATTR pcmTimerCallback() {
        if (!pcmActive) return;
        if (!audioEnabled || volumeLevel == 0) {
            ledcWrite((uint8_t)speakerPin, 0);
            return;
        }
        if (pcmPos >= pcmLen) {
            if (pcmLoop) {
                pcmPos = 0;
            } else {
                pcmActive = false;
                ledcWrite((uint8_t)speakerPin, 0);
                gpio_set_level((gpio_num_t)speakerPin, 0);
                return;
            }
        }
        const uint32_t pos = pcmPos;
        pcmPos = pos + 1;
        const uint8_t sample = pcmData[pos];
        // duty = muestra * tope_seguro * amplitud / (255 * 255)
        const uint32_t duty =
            ((uint32_t)sample * kPcmMaxDuty * pcmAmplitude()) / 65025UL;
        ledcWrite((uint8_t)speakerPin, (uint8_t)duty);
    }
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
            stopPcm();
            silencePwm();
            return;
        }
        if (pcmActive) {
#if defined(PC_EMULATOR)
            simAudioPcmGain(pcmAmplitude());
#endif
            return; // el callback ESP32 lee el volumen en cada muestra
        }
        if (isTonePlaying && currentFreq > 0) {
            pwmTone(currentFreq, noteHighVolume);
        }
    }

    // --- API publica del motor PCM ---------------------------------------
    // Reproduce `len` bytes unsigned 8-bit @ 8000 Hz. `loop` reinicia la
    // muestra al llegar al final (para el rodar de la bola de golf).
    static void playPcm(const uint8_t* data, uint32_t len, bool loop = false) {
        if (speakerPin < 0 || data == nullptr || len == 0) return;
        stopSequence(); // corta tonos y deja el pin en bajo
        if (!audioEnabled || volumeLevel == 0) {
            silencePwm();
            return;
        }
        pcmData = data;
        pcmLen = len;
        pcmPos = 0;
        pcmLoop = loop;
        pcmGain = kPcmDefaultGain;
        pcmActive = true;
        startPcmHardware();
    }

    // Modula la amplitud (0..255) sin reiniciar: velocidad de la bola.
    static void setPcmGain(uint8_t gain) {
        pcmGain = gain;
#if defined(PC_EMULATOR)
        if (pcmActive) simAudioPcmGain(pcmAmplitude());
#endif
    }

    static bool isPcmPlaying() { return pcmActive; }
    static bool isPcmLooping() { return pcmActive && pcmLoop; }

    // duty 0 y pin en bajo: el 2N2222 queda en corte total.
    static void stopPcm() {
        pcmActive = false;
#if defined(ESP32)
        stopPcmTimer();
        if (speakerPin >= 0 && pcmPwmReady) {
            ledcWrite((uint8_t)speakerPin, 0);
            gpio_set_level((gpio_num_t)speakerPin, 0);
            ledcDetach((uint8_t)speakerPin);
            pcmPwmReady = false;
            pwmReady = false;
            currentFreq = 0;
        }
#elif defined(PC_EMULATOR)
        simAudioPcmStop();
#endif
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
        stopPcm();
        stopSequence();
        startNote(freq, 0, false);
    }

    static void tone(uint16_t freq, uint16_t dur) {
        if (speakerPin < 0) return;
        if (dur == 0) {
            tone(freq);
            return;
        }
        stopPcm();
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
        stopPcm();
        startSequence(tonesArray);
    }

    static void tonesInRAM(uint16_t* tonesArray) {
        stopPcm();
        startSequence((const uint16_t*)tonesArray);
    }

    // duty = 0 y pad en bajo: 2N2222 en corte, 0 mA en reposo.
    static void silenceHardware() {
        stopPcm();
        stopSequence();
        silencePwm();
    }

    static void noTone() {
        silenceHardware();
    }

    // Respaldo de temporizacion cuando esp_timer no esta disponible.
    static void update() {
#if defined(ESP32)
        // El callback no puede detener su propio timer con seguridad: cuando
        // una muestra termina (pcmActive = false), el bucle principal apaga
        // aqui el periferico y deja el pin en bajo.
        if (!pcmActive && pcmTimerRunning) {
            stopPcmTimer();
            if (pcmPwmReady) {
                ledcWrite((uint8_t)speakerPin, 0);
                gpio_set_level((gpio_num_t)speakerPin, 0);
            }
        }
#elif defined(PC_EMULATOR)
        // SDL2 avisa aqui de que la muestra no bucleada ya se consumio.
        if (pcmActive && !pcmLoop && !simAudioPcmPlaying()) {
            pcmActive = false;
        }
#endif
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
inline const uint8_t* volatile ArduboyTones::pcmData = nullptr;
inline volatile uint32_t ArduboyTones::pcmLen = 0;
inline volatile uint32_t ArduboyTones::pcmPos = 0;
inline volatile bool ArduboyTones::pcmActive = false;
inline volatile bool ArduboyTones::pcmLoop = false;
inline volatile bool ArduboyTones::pcmPwmReady = false;
inline volatile bool ArduboyTones::pcmTimerRunning = false;
inline volatile uint8_t ArduboyTones::pcmGain = ArduboyTones::kPcmDefaultGain;
#if defined(ESP32)
inline esp_timer_handle_t ArduboyTones::noteTimer = nullptr;
inline hw_timer_t* ArduboyTones::pcmHwTimer = nullptr;
inline bool ArduboyTones::pcmHwAttached = false;
#endif
