#pragma once
// =============================================================================
// NUCLEO COMPARTIDO DE LA CONSOLA (HAL / audio / energia / ajustes)
// =============================================================================
// Superficie minima que un cartucho de ports/ puede usar sin acoplarse a
// main.cpp: framebuffer, sintetizador, melodias, LED RGB, bateria y ajustes
// persistentes. Todo lo demas (pines, I2C, menu, pausa) sigue siendo privado
// del nucleo.

#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"
#if defined(PC_EMULATOR)
// Build nativo (sim/): stub sin Wi-Fi, misma superficie que usa ping_pong.
#include "sim_WiFiGamepads.h"
#else
#include "WiFiGamepadServer.h"   // declara wifiGamepads y el AP de juego 2P
#endif

extern Arduboy2 arduboy;
extern ArduboyTones sound;

// --- Ajustes persistentes (NVS "sys_cfg") -----------------------------------
extern uint8_t sysVolume;
extern uint8_t sysBrightness;

void applyBrightness(uint8_t percent);
void setSoundEnabled(bool on);
void loadSystemSettings();
void saveSystemSettings();

// --- Catalogo del menu principal (implementado en main.cpp) ------------------
// Permite al panel web listar y ocultar cartuchos en caliente. La mascara se
// persiste en NVS ("sys_cfg"/"games_mask") y no borra codigo de la Flash.
uint8_t menuGameCount();
const char* menuGameLabel(uint8_t idx);
bool menuGameEnabled(uint8_t idx);
// false si se intenta ocultar el ultimo cartucho visible.
bool menuGameSetEnabled(uint8_t idx, bool enabled);

// --- Telemetria de bateria (GPIO 2 / ADC1_CH2) -------------------------------
float getBatteryVoltage();
int getBatteryPercentage();

// --- LED RGB WS2812 (GPIO 8) -------------------------------------------------
#define RGB_LED_PIN 8
void triggerRgbLed(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs = 1000);

// --- Motor de melodias (buzzer GPIO 1) --------------------------------------
struct Note {
    uint16_t freq;
    uint16_t duration;
    uint16_t pause;
};

// Melodias compartidas por varios cartuchos; viven en src/console_core.cpp.
extern const Note pingPongVictoryNotes[4];
extern const Note pingPongDefeatNotes[4];
extern const Note tttVictoryNotes[4];
extern const Note tttDefeatNotes[4];
extern const Note hangmanVictoryNotes[3];
extern const Note hangmanDefeatNotes[4];

struct MelodyPlayer {
    const Note* currentNotes = nullptr;
    uint8_t totalNotes = 0;
    uint8_t noteIndex = 0;
    unsigned long nextNoteTime = 0;
    bool isPlaying = false;
    bool inPause = false;

    void play(const Note* notes, uint8_t count) {
        currentNotes = notes;
        totalNotes = count;
        noteIndex = 0;
        isPlaying = true;
        inPause = false;
        playCurrentNote();
    }

    void playCurrentNote() {
        if (!isPlaying || noteIndex >= totalNotes) {
            stop();
            return;
        }
        uint16_t freq = currentNotes[noteIndex].freq;
        uint16_t dur = currentNotes[noteIndex].duration;
        sound.tone(freq);
        nextNoteTime = millis() + dur;
        inPause = false;
    }

    void update() {
        if (!isPlaying) return;

        if (millis() >= nextNoteTime) {
            if (!inPause) {
                sound.noTone();
                uint16_t p = currentNotes[noteIndex].pause;
                if (p > 0) {
                    inPause = true;
                    nextNoteTime = millis() + p;
                } else {
                    noteIndex++;
                    playCurrentNote();
                }
            } else {
                inPause = false;
                noteIndex++;
                playCurrentNote();
            }
        }
    }

    void stop() {
        isPlaying = false;
        currentNotes = nullptr;
        noteIndex = 0;
        sound.noTone();
    }
};

extern MelodyPlayer melodyPlayer;
