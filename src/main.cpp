#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "Arduboy2.h"
#include "ArduboyTones.h"
#include "console_core.h"
#include "ProyectoBLE.h"
// Catálogo de ports (ver include/games_config.h para activar/desactivar)
#include "games_config.h"

#ifdef ENABLE_MATH_CHALLENGE
  #include "../ports/math_challenge/math_challenge.h"
#endif
#ifdef ENABLE_SOPA_LETRAS
  #include "../ports/sopa_letras/sopa_letras.h"
#endif
#ifdef ENABLE_TRENCH_RUN
  #include "../ports/trench_run/trench_run.h"
#endif
#ifdef ENABLE_ROAD_TRIP
  #include "../ports/road_trip/road_trip.h"
#endif
#ifdef ENABLE_WOLFENDUINO_3D
  #include "../ports/wolfenduino_3d/wolfenstein.h"
#endif
#ifdef ENABLE_CATACOMBS_3D
  #include "../ports/catacombs_3d/catacombs_3d.h"
#endif
#ifdef ENABLE_CYBER_ESCAPE
  #include "../ports/cyber_escape/cyber_escape.h"
#endif
#ifdef ENABLE_SEA_BATTLE
  #include "../ports/sea_battle/sea_battle.h"
#endif
#include "WiFiGamepadServer.h"
#include "ConsoleAdminWeb.h"

// Altavoz magnetico 8 ohm via NPN 2N2222 en GPIO 1: la nota sale del PWM
// (frecuencia) y el volumen del ciclo de trabajo (5 %..45 %).
// Escala de pasos: MUTE, 20 %, 40 %, 60 %, 80 %, 100 %.
enum VolumeLevel : uint8_t {
    VOL_MUTE = 0,
    VOL_20,
    VOL_40,
    VOL_60,
    VOL_80,
    VOL_100,
    VOL_STEPS
};

uint8_t sysVolume = VOL_60;
uint8_t lastAudibleVolume = VOL_60; // se restaura al reactivar desde el panel web
const char* const volumeLabels[VOL_STEPS] = {"MUTE", "20%", "40%", "60%", "80%", "100%"};

Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);



// =============================================================================
// SYSTEM CONFIGURATION & HARDWARE PERSISTENCE (NVS "sys_cfg")
// =============================================================================
uint8_t sysBrightness = 80; // 10% a 100% (pasos de 10%)
// Mascara de visibilidad del catalogo en STATE_MENU (bit i = menuEntries[i]).
// La escribe el panel web (/api/games) y se guarda en NVS "sys_cfg".
uint32_t menuVisibleMask = 0xFFFFFFFFUL;

void applyBrightness(uint8_t percent) {
    if (percent < 10) percent = 10;
    if (percent > 100) percent = 100;
    sysBrightness = percent;
    uint8_t contrastVal = (uint8_t)((sysBrightness * 255) / 100);
    Arduboy2Core::setContrast(contrastVal);
}

void saveSystemSettings();

// Aplica el nivel de volumen (0..5) al driver de audio y recuerda el ultimo
// nivel audible para poder reactivar el sonido desde el panel web.
void applyVolume(uint8_t level) {
    if (level >= VOL_STEPS) level = VOL_100;
    sysVolume = level;
    if (level > VOL_MUTE) lastAudibleVolume = level;
    sound.setVolume(level);
}

void setVolumeLevel(uint8_t level) {
    applyVolume(level);
}

// Paso de volumen desde AJUSTES DE SISTEMA: A sube, B baja hasta MUTE.
// La pantalla no cambia: solo se mueve el nivel y suena la confirmacion ya
// con el volumen nuevo (a 0 no suena nada).
void stepVolume(int8_t direction) {
    int16_t next = (int16_t)sysVolume + direction;
    if (next < VOL_MUTE) next = VOL_MUTE;
    if (next >= VOL_STEPS) next = VOL_100;
    applyVolume((uint8_t)next);
    saveSystemSettings();
    if (sysVolume == VOL_MUTE) {
        sound.noTone();
    } else {
        sound.tone(1200, 25);
    }
}

void loadSystemSettings() {
    uint8_t volumeVersion = 2;
    Preferences prefs;
    if (prefs.begin("sys_cfg", true)) {
        sysBrightness = prefs.getUChar("brightness", 80);
        sysVolume = prefs.getUChar("volume", VOL_60);
        volumeVersion = prefs.getUChar("vol_ver", 0);
        menuVisibleMask = prefs.getUInt("games_mask", 0xFFFFFFFFUL);
        // "games_cnt" = cartuchos que existian al guardar la mascara. Los
        // cartuchos anadidos despues de esa fecha entran visibles por defecto,
        // en vez de quedar ocultos por un bit que nadie pulso.
        const uint8_t savedCount = prefs.getUChar("games_cnt", menuGameCount());
        prefs.end();
        for (uint8_t i = savedCount; i < menuGameCount() && i < 32; i++) {
            menuVisibleMask |= (1UL << i);
        }
    } else {
        sysBrightness = 80;
        sysVolume = VOL_60;
        volumeVersion = 2;
        menuVisibleMask = 0xFFFFFFFFUL;
    }

    if (sysBrightness < 10 || sysBrightness > 100) sysBrightness = 80;

    // Migracion de NVS: antes "volume" era binario (0/1 del buzzer activo) y,
    // mas atras, 2/3 sin significado para el PWM. Sin marca de version (vol_ver
    // < 2) cualquier valor audible pasa a 60 %.
    if (volumeVersion < 2) {
        sysVolume = (sysVolume == VOL_MUTE) ? VOL_MUTE : VOL_60;
    }
    if (sysVolume >= VOL_STEPS) sysVolume = VOL_60;

    applyBrightness(sysBrightness);
    applyVolume(sysVolume);
}

void saveSystemSettings() {
    Preferences prefs;
    if (prefs.begin("sys_cfg", false)) {
        prefs.putUChar("brightness", sysBrightness);
        prefs.putUChar("volume", sysVolume);
        prefs.putUChar("vol_ver", 2); // 2 = escala de 6 pasos (0..5)
        prefs.putUInt("games_mask", menuVisibleMask);
        prefs.putUChar("games_cnt", menuGameCount());
        prefs.end();
    }
}

// Compatibilidad con el panel web (/api/settings "sound": 0/1): reactiva el
// ultimo nivel audible o silencia sin perderlo.
void setSoundEnabled(bool on) {
    applyVolume(on ? lastAudibleVolume : VOL_MUTE);
    if (on) {
        sound.tone(1200, 40); // confirmacion audible solo al reactivar
    }
}

// =============================================================================
// BATTERY TELEMETRY SUBSYSTEM (GPIO 2 / ADC1_CH2 - 100k/100k Voltage Divider)
// =============================================================================
uint32_t getBatteryRawMilliVolts() {
    return Arduboy2Core::getBatteryRawMilliVolts();
}

float getBatteryVoltage() {
    return Arduboy2Core::getBatteryVoltage();
}

int getBatteryPercentage() {
    float v = getBatteryVoltage();

    if (v >= 4.18f) return 100;
    if (v <= 3.30f) return 0;

    // Tramo 1: 4.18V a 4.00V (100% a 85%) - Caída inicial rápida
    if (v >= 4.00f) {
        return (int)(85.0f + ((v - 4.00f) / (4.18f - 4.00f)) * 15.0f);
    }
    // Tramo 2: 4.00V a 3.82V (85% a 60%)
    else if (v >= 3.82f) {
        return (int)(60.0f + ((v - 3.82f) / (4.00f - 3.82f)) * 25.0f);
    }
    // Tramo 3: 3.82V a 3.68V (60% a 30%) - Meseta principal de descarga
    else if (v >= 3.68f) {
        return (int)(30.0f + ((v - 3.68f) / (3.82f - 3.68f)) * 30.0f);
    }
    // Tramo 4: 3.68V a 3.50V (30% a 10%)
    else if (v >= 3.50f) {
        return (int)(10.0f + ((v - 3.50f) / (3.68f - 3.50f)) * 20.0f);
    }
    // Tramo 5: 3.50V a 3.30V (10% a 0%) - Zona crítica de corte
    else {
        return (int)(((v - 3.30f) / (3.50f - 3.30f)) * 10.0f);
    }
}

void getBatteryStatus(float &voltage, int &percentage) {
    voltage = getBatteryVoltage();
    percentage = getBatteryPercentage();
}

// =============================================================================
// RGB LED SYSTEM (GPIO 8 - WS2812 INTEGRATED)
// =============================================================================
unsigned long rgbLedOffTime = 0;
bool rgbLedActive = false;

void triggerRgbLed(uint8_t r, uint8_t g, uint8_t b, uint16_t durationMs) {
    rgbLedWrite(RGB_LED_PIN, r, g, b);
    delay(2); // Retraso de 2ms no perceptible para prevenir caída de tensión (brownout) entre periféricos
    rgbLedOffTime = millis() + durationMs;
    rgbLedActive = true;
}

void updateRgbLed() {
    if (rgbLedActive && millis() >= rgbLedOffTime) {
        rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
        rgbLedActive = false;
    }
}


// =============================================================================
// GLOBAL BITMAPS & SPRITES
// =============================================================================
#ifdef ENABLE_PING_PONG
#include "../ports/ping_pong/ping_pong.h"
#endif

#ifdef ENABLE_PING_PONG
PingPongGame pingPong;
#endif

#ifdef ENABLE_BREAKOUT
#include "../ports/breakout/breakout.h"
#endif

#ifdef ENABLE_BREAKOUT
BreakoutGame breakout;
#endif

#ifdef ENABLE_SPACE_INVADERS
#include "../ports/space_invaders/space_invaders.h"
#endif

#ifdef ENABLE_SPACE_INVADERS
SpaceInvadersGame spaceInvaders;
#endif

#ifdef ENABLE_1943
#include "../ports/arcade_1943/arcade_1943.h"
#endif

#ifdef ENABLE_1943
AirCombatGame airCombat;
#endif

#ifdef ENABLE_FLAPPY_BIRD
#include "../ports/flappy_bird/flappy_bird.h"
#endif

#ifdef ENABLE_FLAPPY_BIRD
FlappyGame flappy;
#endif

#ifdef ENABLE_SNAKE
#include "../ports/snake/snake.h"
#endif

#ifdef ENABLE_SNAKE
SnakeGame snake;
#endif

#ifdef ENABLE_JUMP_MAN
#include "../ports/jump_man/jump_man.h"
#endif

#ifdef ENABLE_JUMP_MAN
JumpManGame jumpMan;
#endif

#ifdef ENABLE_2048
#include "../ports/game_2048/game_2048.h"
#endif

#ifdef ENABLE_2048
Game2048 game2048;
#endif

#ifdef ENABLE_TREX_RUNNER
#include "../ports/trex_runner/trex_runner.h"
#endif

#ifdef ENABLE_TREX_RUNNER
TRexGame trexRunner;
#endif

#ifdef ENABLE_TIC_TAC_TOE
#include "../ports/tic_tac_toe/tic_tac_toe.h"
#endif

#ifdef ENABLE_TIC_TAC_TOE
TicTacToeGame ticTacToe;
#endif

#ifdef ENABLE_BRAINDU
#include "../ports/braindu/braindu.h"
#endif

#ifdef ENABLE_BRAINDU
BrainduTrainGame brainduTrain;
#endif

#ifdef ENABLE_HANGMAN
#include "../ports/hangman/hangman.h"
#endif

#ifdef ENABLE_HANGMAN
HangmanGame hangmanGame;
#endif

#ifdef ENABLE_TRIS
#include "../ports/tris/tris.h"
#endif

#ifdef ENABLE_TRIS
TrisGame trisGame;
#endif

#ifdef ENABLE_EIGHTEEN_HOLES
#include "../ports/eighteen_holes/eighteen_holes.h"
#endif

#ifdef ENABLE_EIGHTEEN_HOLES
EighteenHolesGame eighteenHoles;
#endif

#ifdef ENABLE_MATH_CHALLENGE
MathChallengeGame mathChallenge;
#endif
#ifdef ENABLE_SOPA_LETRAS
WordSearchGame wordSearchGame;
#endif
#ifdef ENABLE_TRENCH_RUN
TrenchRunGame trenchRun;
#endif
#ifdef ENABLE_ROAD_TRIP
RoadTripGame roadTrip;
#endif
#ifdef ENABLE_WOLFENDUINO_3D
WolfensteinGame wolfenstein;
#endif
#ifdef ENABLE_CATACOMBS_3D
Catacombs3DGame catacombs3D;
#endif
#ifdef ENABLE_CYBER_ESCAPE
CyberEscapeGame cyberEscape;
#endif
#ifdef ENABLE_SEA_BATTLE
SeaBattleGame seaBattle;
#endif



// =============================================================================
// SYSTEM STATE & SELECTOR MENU
// =============================================================================
enum SystemState {
    STATE_MENU = 0,
    STATE_PING_PONG,
    STATE_BREAKOUT,
    STATE_SPACE_INVADERS,
    STATE_1943,
    STATE_FLAPPY_BIRD,
    STATE_SNAKE,
    STATE_JUMP_MAN,
    STATE_PROYECTO_BLE,
    STATE_2048,
    STATE_TREX_RUNNER,
    STATE_TIC_TAC_TOE,
    STATE_BRAINDU_TRAIN,
    STATE_HANGMAN,
    STATE_TRIS,
#ifdef ENABLE_EIGHTEEN_HOLES
    STATE_EIGHTEEN_HOLES,
#endif
#ifdef ENABLE_MATH_CHALLENGE
    STATE_MATH_CHALLENGE,
#endif
#ifdef ENABLE_SOPA_LETRAS
    STATE_SOPA,
#endif
#ifdef ENABLE_TRENCH_RUN
    STATE_TRENCH_RUN,
#endif
#ifdef ENABLE_ROAD_TRIP
    STATE_ROAD_TRIP,
#endif
#ifdef ENABLE_WOLFENDUINO_3D
    STATE_WOLFENSTEIN,
#endif
#ifdef ENABLE_CATACOMBS_3D
    STATE_CATACOMBS,
#endif
#ifdef ENABLE_CYBER_ESCAPE
    STATE_CYBER_ESCAPE,
#endif
#ifdef ENABLE_SEA_BATTLE
    STATE_SEA_BATTLE,
#endif
    STATE_PIN_DIAGNOSTIC,
    STATE_SETTINGS,
    STATE_WEB_CONFIG
};

SystemState currentState = STATE_MENU;
SystemState previousStateBeforeSettings = STATE_MENU;
int8_t settingsCursor = 0;
int8_t menuCursor = 0;
// Registro del menu principal: una fila por juego. La numeracion visible se
// calcula en el render (indice + 1), de modo que desactivar un port desde
// include/games_config.h no deja numeros huecos ni desplaza la seleccion.
struct MenuEntry {
    const char* label;
    SystemState state;
};

const MenuEntry menuEntries[] = {
#ifdef ENABLE_PING_PONG
    { "Ping Pong",          STATE_PING_PONG },
#endif
#ifdef ENABLE_BREAKOUT
    { "Breakout",           STATE_BREAKOUT },
#endif
#ifdef ENABLE_SPACE_INVADERS
    { "Space Invaders",     STATE_SPACE_INVADERS },
#endif
#ifdef ENABLE_1943
    { "1943",               STATE_1943 },
#endif
#ifdef ENABLE_FLAPPY_BIRD
    { "Flappy Bird",        STATE_FLAPPY_BIRD },
#endif
#ifdef ENABLE_SNAKE
    { "Snake",              STATE_SNAKE },
#endif
#ifdef ENABLE_JUMP_MAN
    { "Jump Man",           STATE_JUMP_MAN },
#endif
    { "Proyecto BLE",       STATE_PROYECTO_BLE },
#ifdef ENABLE_2048
    { "2048",               STATE_2048 },
#endif
#ifdef ENABLE_TREX_RUNNER
    { "T-Rex Runner",       STATE_TREX_RUNNER },
#endif
#ifdef ENABLE_TIC_TAC_TOE
    { "Tic-Tac-Toe",        STATE_TIC_TAC_TOE },
#endif
#ifdef ENABLE_BRAINDU
    { "BrainduTrain",       STATE_BRAINDU_TRAIN },
#endif
#ifdef ENABLE_HANGMAN
    { "Hangman",            STATE_HANGMAN },
#endif
#ifdef ENABLE_TRIS
    { "TRIS",               STATE_TRIS },
#endif
#ifdef ENABLE_EIGHTEEN_HOLES
    { "18 Holes",           STATE_EIGHTEEN_HOLES },
#endif
#ifdef ENABLE_MATH_CHALLENGE
    { "Math Challenge",     STATE_MATH_CHALLENGE },
#endif
#ifdef ENABLE_SOPA_LETRAS
    { "Sopa de Letras",     STATE_SOPA },
#endif
#ifdef ENABLE_TRENCH_RUN
    { "Trench Run",         STATE_TRENCH_RUN },
#endif
#ifdef ENABLE_ROAD_TRIP
    { "Road Trip",          STATE_ROAD_TRIP },
#endif
#ifdef ENABLE_WOLFENDUINO_3D
    { "Wolfenstein 3D",     STATE_WOLFENSTEIN },
#endif
#ifdef ENABLE_CYBER_ESCAPE
    { "Cyber Escape",       STATE_CYBER_ESCAPE },
#endif
#ifdef ENABLE_SEA_BATTLE
    { "Sea Battle",         STATE_SEA_BATTLE },
#endif
#ifdef ENABLE_CATACOMBS_3D
    { "Catacombs 3D",       STATE_CATACOMBS },
#endif
    { "Test Pines Cruceta", STATE_PIN_DIAGNOSTIC }
};
const uint8_t MENU_COUNT = sizeof(menuEntries) / sizeof(menuEntries[0]);

// ---------------------------------------------------------------------------
// VISIBILIDAD DEL CATALOGO (panel web /api/games <-> NVS "games_mask")
// La mascara no borra codigo: el port sigue en la Flash y solo se oculta del
// carrusel. Para liberar espacio hay que desactivar su #define en
// include/games_config.h y subir el nuevo .bin por OTA.
// ---------------------------------------------------------------------------
bool menuGameVisible(uint8_t idx) {
    if (idx >= MENU_COUNT) return false;
    return (menuVisibleMask >> idx) & 0x1UL;
}

uint8_t menuVisibleCount() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
        if (menuGameVisible(i)) n++;
    }
    return n;
}

uint8_t menuFirstVisible() {
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
        if (menuGameVisible(i)) return i;
    }
    return 0;
}

// Siguiente entrada visible desde 'from' en la direccion 'dir' (+1/-1).
uint8_t menuStepVisible(uint8_t from, int8_t dir) {
    if (menuVisibleCount() == 0) return from;
    int16_t i = from;
    for (uint8_t n = 0; n < MENU_COUNT; n++) {
        i = (i + dir + MENU_COUNT) % MENU_COUNT;
        if (menuGameVisible((uint8_t)i)) return (uint8_t)i;
    }
    return from;
}

// API publica que consume ConsoleAdminWeb (declarada en console_core.h).
uint8_t menuGameCount() {
    return MENU_COUNT;
}

const char* menuGameLabel(uint8_t idx) {
    return (idx < MENU_COUNT) ? menuEntries[idx].label : nullptr;
}

bool menuGameEnabled(uint8_t idx) {
    return menuGameVisible(idx);
}

bool menuGameSetEnabled(uint8_t idx, bool enabled) {
    if (idx >= MENU_COUNT) return false;
    // Nunca dejar el menu vacio: siempre al menos un cartucho visible.
    if (!enabled && menuGameVisible(idx) && menuVisibleCount() <= 1) return false;

    const uint32_t before = menuVisibleMask;
    if (enabled) menuVisibleMask |= (1UL << idx);
    else menuVisibleMask &= ~(1UL << idx);
    if (menuVisibleMask == before) return true;

    saveSystemSettings(); // persistencia inmediata en NVS
    if (!menuGameVisible(menuCursor)) menuCursor = menuFirstVisible();
    return true;
}


// System Pause & Power Management State
bool gamePaused = false;
int8_t pauseMenuCursor = 0;
bool isScreenSleeping = false;
unsigned long bootRawLowStart = 0;
bool bootDebouncedDown = false;
unsigned long bootDebouncedPressStart = 0;
unsigned long exitComboStart = 0;
const unsigned long INACTIVITY_TIMEOUT_MS = 90000UL; // 90s timeout de inactividad
unsigned long lastActivityTime = 0;

bool isCurrentGameActive() {
    switch (currentState) {
#ifdef ENABLE_PING_PONG
        case STATE_PING_PONG:
            if (pingPong.mode == PP_MODE_SUBMENU) return false;
            if (pingPong.mode == PP_MODE_NORMAL) return (!pingPong.gameOver && !pingPong.victory);
            if (pingPong.mode == PP_MODE_DODGE) return (!pingPong.dodgeGameOver);
            if (pingPong.mode == PP_MODE_WIFI_2P) return (pingPong.wifiMatchStarted && !pingPong.gameOver);
            return false;

#endif
#ifdef ENABLE_BREAKOUT
        case STATE_BREAKOUT:
            return (!breakout.gameOver && !breakout.victory);

#endif
#ifdef ENABLE_SPACE_INVADERS
        case STATE_SPACE_INVADERS:
            return (!spaceInvaders.gameOver && !spaceInvaders.victory);

#endif
#ifdef ENABLE_1943
        case STATE_1943:
            return (!airCombat.gameOver);

#endif
#ifdef ENABLE_FLAPPY_BIRD
        case STATE_FLAPPY_BIRD:
            return (!flappy.gameOver);

#endif
#ifdef ENABLE_SNAKE
        case STATE_SNAKE:
            return (!snake.gameOver);

#endif
#ifdef ENABLE_JUMP_MAN
        case STATE_JUMP_MAN:
            return (!jumpMan.inGameOverScreen);

#endif
        case STATE_PROYECTO_BLE:
            return true;

#ifdef ENABLE_2048
        case STATE_2048:
            return (!game2048.gameOver);

#endif
#ifdef ENABLE_TREX_RUNNER
        case STATE_TREX_RUNNER:
            return (!trexRunner.gameOver);

#endif
#ifdef ENABLE_TIC_TAC_TOE
        case STATE_TIC_TAC_TOE:
            return (ticTacToe.state == TTT_PLAYING);

#endif
#ifdef ENABLE_BRAINDU
        case STATE_BRAINDU_TRAIN:
            return (brainduTrain.state == BT_STATE_ACTIVITY);

#endif
#ifdef ENABLE_HANGMAN
        case STATE_HANGMAN:
            return (hangmanGame.mode == HANGMAN_MODE_PLAY);

#endif
#ifdef ENABLE_TRIS
        case STATE_TRIS:
            return (trisGame.state == TRIS_STATE_PLAYING || trisGame.state == TRIS_STATE_FLASH);

#endif
#ifdef ENABLE_EIGHTEEN_HOLES
        case STATE_EIGHTEEN_HOLES:
            return eighteenHoles.isPlaying();
#endif
#ifdef ENABLE_MATH_CHALLENGE
        case STATE_MATH_CHALLENGE:
            return (mathChallenge.state == MATH_STATE_PLAYING || mathChallenge.state == MATH_STATE_FEEDBACK);
#endif

#ifdef ENABLE_SOPA_LETRAS
        case STATE_SOPA:
            return (wordSearchGame.state == SOPA_STATE_PLAYING);
#endif

#ifdef ENABLE_TRENCH_RUN
        case STATE_TRENCH_RUN:
            return trenchRun.isPlaying();
#endif

#ifdef ENABLE_ROAD_TRIP
        case STATE_ROAD_TRIP:
            return roadTrip.isPlaying();
#endif

#ifdef ENABLE_WOLFENDUINO_3D
        case STATE_WOLFENSTEIN:
            return wolfenstein.isPlaying();
#endif

#ifdef ENABLE_CATACOMBS_3D
        case STATE_CATACOMBS:
            return catacombs3D.isPlaying();
#endif

#ifdef ENABLE_CYBER_ESCAPE
        case STATE_CYBER_ESCAPE:
            return cyberEscape.isPlaying();
#endif

#ifdef ENABLE_SEA_BATTLE
        case STATE_SEA_BATTLE:
            // Solo la fase de combate admite pausa con B; en la colocacion B
            // rota el barco dentro del propio juego.
            return seaBattle.isCombat();
#endif

        default:
            return false;
    }
}

// Tecla de pausa universal del menu de consola. 18 Holes usa B *dentro* del
// juego (menu interno, cancelar apuntado, saltar animaciones), asi que en ese
// cartucho la pausa se dispara con el click central de la cruceta (GPIO 20).
bool universalPausePressed() {
#ifdef ENABLE_EIGHTEEN_HOLES
    if (currentState == STATE_EIGHTEEN_HOLES) {
        return arduboy.justPressed(SYSTEM_MENU_BUTTON);
    }
#endif
    return arduboy.justPressed(B_BUTTON);
}

bool currentGameHasSubmenu() {
    switch (currentState) {
#ifdef ENABLE_PING_PONG
        case STATE_PING_PONG:
            return true;
#endif
#ifdef ENABLE_TIC_TAC_TOE
        case STATE_TIC_TAC_TOE:
            return true;
#endif
#ifdef ENABLE_BRAINDU
        case STATE_BRAINDU_TRAIN:
            return true;
#endif
#ifdef ENABLE_HANGMAN
        case STATE_HANGMAN:
            return true;
#endif
#ifdef ENABLE_TRIS
        case STATE_TRIS:
            return true;
#endif
#ifdef ENABLE_EIGHTEEN_HOLES
        case STATE_EIGHTEEN_HOLES:
            return true;
#endif
#ifdef ENABLE_MATH_CHALLENGE
        case STATE_MATH_CHALLENGE:
            return true;
#endif
#ifdef ENABLE_SOPA_LETRAS
        case STATE_SOPA:
            return true;
#endif
#ifdef ENABLE_TRENCH_RUN
        case STATE_TRENCH_RUN:
            return true;
#endif
#ifdef ENABLE_ROAD_TRIP
        case STATE_ROAD_TRIP:
            return true;
#endif
#ifdef ENABLE_WOLFENDUINO_3D
        case STATE_WOLFENSTEIN:
            return true;
#endif
#ifdef ENABLE_CATACOMBS_3D
        case STATE_CATACOMBS:
            return true;
#endif
#ifdef ENABLE_CYBER_ESCAPE
        case STATE_CYBER_ESCAPE:
            return true;
#endif
#ifdef ENABLE_SEA_BATTLE
        case STATE_SEA_BATTLE:
            return true;
#endif
        default:
            return false;
    }
}

void returnToCurrentGameSubmenu() {
    melodyPlayer.stop();
    sound.noTone();
    sound.tone(880, 30);
    gamePaused = false;
    pauseMenuCursor = 0;
    Arduboy2Core::previousButtonState = Arduboy2Core::currentButtonState;
    switch (currentState) {
#ifdef ENABLE_PING_PONG
        case STATE_PING_PONG:
            pingPong.init();
            break;
#endif
#ifdef ENABLE_TIC_TAC_TOE
        case STATE_TIC_TAC_TOE:
            ticTacToe.init();
            break;
#endif
#ifdef ENABLE_BRAINDU
        case STATE_BRAINDU_TRAIN:
            brainduTrain.state = BT_STATE_MENU;
            break;
#endif
#ifdef ENABLE_HANGMAN
        case STATE_HANGMAN:
            hangmanGame.init();
            break;
#endif
#ifdef ENABLE_TRIS
        case STATE_TRIS:
            trisGame.init();
            break;
#endif
#ifdef ENABLE_EIGHTEEN_HOLES
        case STATE_EIGHTEEN_HOLES:
            eighteenHoles.returnToTitle();
            break;
#endif
#ifdef ENABLE_MATH_CHALLENGE
        case STATE_MATH_CHALLENGE:
            mathChallenge.state = MATH_STATE_MODE_SELECT;
            break;
#endif
#ifdef ENABLE_SOPA_LETRAS
        case STATE_SOPA:
            wordSearchGame.init();
            break;
#endif
#ifdef ENABLE_TRENCH_RUN
        case STATE_TRENCH_RUN:
            trenchRun.init();
            break;
#endif
#ifdef ENABLE_ROAD_TRIP
        case STATE_ROAD_TRIP:
            roadTrip.init();
            break;
#endif
#ifdef ENABLE_WOLFENDUINO_3D
        case STATE_WOLFENSTEIN:
            wolfenstein.init();
            break;
#endif
#ifdef ENABLE_CATACOMBS_3D
        case STATE_CATACOMBS:
            catacombs3D.init();
            break;
#endif
#ifdef ENABLE_CYBER_ESCAPE
        case STATE_CYBER_ESCAPE:
            cyberEscape.init();
            break;
#endif
#ifdef ENABLE_SEA_BATTLE
        case STATE_SEA_BATTLE:
            seaBattle.init();
            break;
#endif
        default:
            break;
    }
}

void exitCurrentGameToMainMenu() {
#ifdef ENABLE_PING_PONG
    if (currentState == STATE_PING_PONG) {
        pingPong.init();
        if (wifiGamepads.isRunning) wifiGamepads.stop();
    } else
#endif
    if (currentState == STATE_PROYECTO_BLE) {
        proyectoBLE.stop();
#ifdef ENABLE_2048
    } else if (currentState == STATE_2048) {
        game2048.saveHighScore();
#endif
#ifdef ENABLE_MATH_CHALLENGE
    } else if (currentState == STATE_MATH_CHALLENGE) {
        mathChallenge.saveHighScore();
#endif
#ifdef ENABLE_SOPA_LETRAS
    } else if (currentState == STATE_SOPA) {
        wordSearchGame.saveHighScore();
#endif
#ifdef ENABLE_CYBER_ESCAPE
    } else if (currentState == STATE_CYBER_ESCAPE) {
        // Restaura la inversion de pantalla (niveles nocturnos) antes del menu
        cyberEscape.init();
#endif
#ifdef ENABLE_SEA_BATTLE
    } else if (currentState == STATE_SEA_BATTLE) {
        seaBattle.init();
#endif
#ifdef ENABLE_EIGHTEEN_HOLES
    } else if (currentState == STATE_EIGHTEEN_HOLES) {
        eighteenHoles.init();
#endif
#ifdef ENABLE_CATACOMBS_3D
    } else if (currentState == STATE_CATACOMBS) {
        catacombs3D.init();
#endif
    }
    melodyPlayer.stop();
    sound.noTone();
    sound.tone(600, 30);
    gamePaused = false;
    pauseMenuCursor = 0;
    Arduboy2Core::previousButtonState = Arduboy2Core::currentButtonState;
    currentState = STATE_MENU;
}

void enterDeepSleep(const char* message = "APAGANDO...") {
    if (wifiGamepads.isRunning) {
        wifiGamepads.stop();
    }
    WiFi.mode(WIFI_OFF);
    if (currentState == STATE_PROYECTO_BLE) {
        proyectoBLE.stop();
#ifdef ENABLE_2048
    } else if (currentState == STATE_2048) {
        game2048.saveHighScore();
#endif
#ifdef ENABLE_MATH_CHALLENGE
    } else if (currentState == STATE_MATH_CHALLENGE) {
        mathChallenge.saveHighScore();
#endif
#ifdef ENABLE_SOPA_LETRAS
    } else if (currentState == STATE_SOPA) {
        wordSearchGame.saveHighScore();
#endif
    }
    melodyPlayer.stop();
    arduboy.clear();
    arduboy.fillRect(10, 18, 108, 28, BLACK);
    arduboy.drawRect(10, 18, 108, 28, WHITE);
    arduboy.drawRect(12, 20, 104, 24, WHITE);
    arduboy.setCursor(24, 28);
    arduboy.print(message);
    arduboy.display();

    // Power-off sound sequence on GPIO 1
    sound.tone(880, 50); delay(70);
    sound.tone(659, 70); delay(90);
    sound.tone(440, 100); delay(120);
    sound.tone(220, 160); delay(200);
    sound.noTone();

    // Turn off display to conserve power
    arduboy.clear();
    arduboy.display();
    Arduboy2Core::sendCommand(SSD1306_DISPLAYOFF);

    // Turn off sound and RGB LED
    sound.noTone();
    rgbLedWrite(RGB_LED_PIN, 0, 0, 0);

    // Configure wake-up on GPIO 9 (BOOT button pressed LOW)
    esp_deep_sleep_enable_gpio_wakeup((1ULL << ARDUBOY_PIN_BOOT), ESP_GPIO_WAKEUP_GPIO_LOW);

    // Wait until button is released to prevent immediate wakeup
    while (digitalRead(ARDUBOY_PIN_BOOT) == LOW) {
        delay(20);
    }
    delay(100);
    esp_deep_sleep_start();
}

void setup() {
    // Altavoz magnetico en GPIO 1: PWM LEDC real (nota por frecuencia, volumen
    // por duty). Debe fijarse antes del pitido de encendido.
    ArduboyTones::setSpeakerPin(ARDUBOY_PIN_BUZZER);
    ArduboyTones::enableAudio(true);
    applyVolume(VOL_60);
    sound.noTone();

    // 1. Inicialización y aislamiento de todos los pines de control (Cruceta 5 vías + Botones A/B + BOOT)
    Arduboy2Core::initPins();
    gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_BOOT);
    pinMode(ARDUBOY_PIN_BOOT, INPUT_PULLUP);

    // 2. Forzar explícitamente el estado global al MENÚ PRINCIPAL
    currentState = STATE_MENU;
    menuCursor = 0;
    gamePaused = false;
    pauseMenuCursor = 0;
    isScreenSleeping = false;

    // -------------------------------------------------------------------------
    // POWER-ON: REQUERIR PULSACIÓN SOSTENIDA DE 3 SEGUNDOS TRAS DEEP SLEEP
    // -------------------------------------------------------------------------
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        uint32_t pressStart = millis();
        bool powerOnConfirmed = false;

        while (digitalRead(ARDUBOY_PIN_BOOT) == LOW) {
            if (millis() - pressStart >= 3000) {
                powerOnConfirmed = true;
                break; // Cumplió los 3 segundos continuos
            }
            delay(10);
        }

        if (!powerOnConfirmed) {
            // Si el usuario soltó antes de 3s: abortar arranque y volver de inmediato a Deep Sleep
            esp_deep_sleep_enable_gpio_wakeup((1ULL << ARDUBOY_PIN_BOOT), ESP_GPIO_WAKEUP_GPIO_LOW);
            esp_deep_sleep_start();
        }

        // Pitido breve de confirmación de encendido en el Buzzer (GPIO 1)
        sound.tone(1400, 60);
        delay(80);
        sound.noTone();

        // Esperar a que se suelte el botón BOOT para no disparar pausas inmediatas
        while (digitalRead(ARDUBOY_PIN_BOOT) == LOW) {
            delay(10);
        }
        delay(50);
    }

    Serial.begin(115200);
    delay(50);

    // Boot Arduboy2 Core (inicializa bus I2C en SDA=7, SCL=6 y pantalla OLED)
    arduboy.begin();
    arduboy.setFrameRate(60);

    // Asegurar configuración limpia y aislamiento de todos los pines de control
    Arduboy2Core::initPins();
    Arduboy2Core::initBattery();

    // Ensure RGB LED starts OFF
    rgbLedWrite(RGB_LED_PIN, 0, 0, 0);

    // Asegurar que la radio Wi-Fi inicie completamente apagada (modo 1P y ahorro de energia)
    WiFi.mode(WIFI_OFF);

    // Cargar y aplicar brillo de pantalla y volumen desde NVS ("sys_cfg")
    loadSystemSettings();

    // Panel web de administracion + OTA: registra sus rutas en el WebServer del
    // AP de mandos. El AP arranca solo en el modo 2P Wi-Fi de Ping Pong.
    consoleAdmin.begin(wifiGamepads.httpServer());

    // Estabilización y purga de transitorios de arranque en botones
    delay(100);
    for (uint8_t i = 0; i < 5; i++) {
        arduboy.pollButtons();
        delay(10);
    }

    // Garantizar que la consola inicie SIEMPRE en el menú principal multijuegos
    currentState = STATE_MENU;
    menuCursor = 0;
    lastActivityTime = millis();

    Serial.printf("[ESP32-C6] Retro Console Ready -> Menu Principal (OLED SSD1309 2.42\", UP=%d, DOWN=%d, RIGHT=%d, LEFT=%d, CTR=%d, A=%d, B=%d, SDA=7, SCL=6)\n",
                  ARDUBOY_PIN_UP, ARDUBOY_PIN_DOWN, ARDUBOY_PIN_RIGHT, ARDUBOY_PIN_LEFT, PIN_NAV_CENTER, PIN_BUTTON_A, PIN_BUTTON_B);
    Serial.printf("[DIAG] Monitor de Controles Activo (115200 baudios): D-Pad(UP=%d,DOWN=%d,RIGHT=%d,LEFT=%d), Center(20), A(%d), B(%d)\n",
                  ARDUBOY_PIN_UP, ARDUBOY_PIN_DOWN, ARDUBOY_PIN_RIGHT, ARDUBOY_PIN_LEFT, PIN_BUTTON_A, PIN_BUTTON_B);
}

void launchSelectedGame() {
    if (menuCursor < 0 || menuCursor >= MENU_COUNT) return;
    if (!menuGameVisible(menuCursor)) return; // entrada oculta desde el panel web
    sound.tone(1760, 50);
    gamePaused = false;
    pauseMenuCursor = 0;
    const SystemState next = menuEntries[menuCursor].state;
    switch (next) {
#ifdef ENABLE_PING_PONG
        case STATE_PING_PONG:      pingPong.init();     break;
#endif
#ifdef ENABLE_BREAKOUT
        case STATE_BREAKOUT:       breakout.init();     break;
#endif
#ifdef ENABLE_SPACE_INVADERS
        case STATE_SPACE_INVADERS: spaceInvaders.init();break;
#endif
#ifdef ENABLE_1943
        case STATE_1943:           airCombat.init();    break;
#endif
#ifdef ENABLE_FLAPPY_BIRD
        case STATE_FLAPPY_BIRD:    flappy.init();       break;
#endif
#ifdef ENABLE_SNAKE
        case STATE_SNAKE:          snake.init();        break;
#endif
#ifdef ENABLE_JUMP_MAN
        case STATE_JUMP_MAN:       jumpMan.init();      break;
#endif
        case STATE_PROYECTO_BLE:   proyectoBLE.init();  break;
#ifdef ENABLE_2048
        case STATE_2048:           game2048.init();     break;
#endif
#ifdef ENABLE_TREX_RUNNER
        case STATE_TREX_RUNNER:    trexRunner.init();   break;
#endif
#ifdef ENABLE_TIC_TAC_TOE
        case STATE_TIC_TAC_TOE:    ticTacToe.init();    break;
#endif
#ifdef ENABLE_BRAINDU
        case STATE_BRAINDU_TRAIN:  brainduTrain.init(); break;
#endif
#ifdef ENABLE_HANGMAN
        case STATE_HANGMAN:        hangmanGame.init();  break;
#endif
#ifdef ENABLE_TRIS
        case STATE_TRIS:           trisGame.init();     break;
#endif
#ifdef ENABLE_EIGHTEEN_HOLES
        case STATE_EIGHTEEN_HOLES: eighteenHoles.init();break;
#endif
#ifdef ENABLE_MATH_CHALLENGE
        case STATE_MATH_CHALLENGE: mathChallenge.init();break;
#endif
#ifdef ENABLE_SOPA_LETRAS
        case STATE_SOPA:           wordSearchGame.init();break;
#endif
#ifdef ENABLE_TRENCH_RUN
        case STATE_TRENCH_RUN:     trenchRun.init();    break;
#endif
#ifdef ENABLE_ROAD_TRIP
        case STATE_ROAD_TRIP:      roadTrip.init();     break;
#endif
#ifdef ENABLE_WOLFENDUINO_3D
        case STATE_WOLFENSTEIN:    wolfenstein.init();  break;
#endif
#ifdef ENABLE_CATACOMBS_3D
        case STATE_CATACOMBS:      catacombs3D.init();  break;
#endif
#ifdef ENABLE_CYBER_ESCAPE
        case STATE_CYBER_ESCAPE:   cyberEscape.init();  break;
#endif
#ifdef ENABLE_SEA_BATTLE
        case STATE_SEA_BATTLE:     seaBattle.init();    break;
#endif
        case STATE_PIN_DIAGNOSTIC: break;   // sin init: pantalla de diagnostico
        default: break;
    }
    currentState = next;
}

// -------------------------------------------------------------------------
// TRANSICIÓN HORIZONTAL (SLIDE) ESTILO TAMAGOTCHI: MENU <-> AJUSTES
// -------------------------------------------------------------------------
static uint8_t slideBufMenu[1024];
static uint8_t slideBufSettings[1024];

void drawMainMenuScreen() {
    // Si el cartucho seleccionado se oculto desde el panel web, saltar al
    // primero visible sin reiniciar la consola.
    if (!menuGameVisible(menuCursor)) menuCursor = menuFirstVisible();

    // Header Bar
    arduboy.fillRect(0, 0, 128, 9, WHITE);
    arduboy.setTextColor(BLACK);
    arduboy.setCursor(1, 0);
    arduboy.print("ESP32-C6 RETRO ARCADE");
    arduboy.setTextColor(WHITE);

    // Menu Viewport: solo entradas visibles, numeradas 1..N sin huecos.
    uint8_t visible[MENU_COUNT];
    uint8_t visCount = 0;
    for (uint8_t i = 0; i < MENU_COUNT; i++) {
        if (menuGameVisible(i)) visible[visCount++] = i;
    }
    if (visCount == 0) { visible[0] = menuCursor; visCount = 1; }

    uint8_t pos = 0;
    while (pos + 1 < visCount && visible[pos] != menuCursor) pos++;

    uint8_t startIdx = 0;
    if (pos > 3) startIdx = pos - 3;
    if (startIdx + 5 > visCount) startIdx = (visCount > 5) ? (visCount - 5) : 0;
    const uint8_t rows = (visCount < 5) ? visCount : 5;

    for (uint8_t i = 0; i < rows; i++) {
        uint8_t itemIdx = visible[startIdx + i];
        int16_t y = 11 + (i * 9);
        if (itemIdx == menuCursor) {
            arduboy.fillRect(2, y - 1, 124, 9, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(6, y);
            arduboy.print("> ");
            arduboy.printf("%d. %s", startIdx + i + 1, menuEntries[itemIdx].label);
            arduboy.setTextColor(WHITE);
        } else {
            arduboy.setCursor(6, y);
            arduboy.printf("  %d. %s", startIdx + i + 1, menuEntries[itemIdx].label);
        }
    }

    // Bottom Navigation Bar
    arduboy.drawFastHLine(0, 56, 128, WHITE);
    arduboy.setCursor(2, 57);
    arduboy.print("A:JUGAR | RIGHT:AJUSTES");
}

void drawSettingsScreen() {
    // Header Bar
    arduboy.fillRect(0, 0, 128, 9, WHITE);
    arduboy.setTextColor(BLACK);
    arduboy.setCursor(12, 1);
    arduboy.print("AJUSTES DE SISTEMA");
    arduboy.setTextColor(WHITE);

    // Item 0: Brillo Pantalla (10% a 100%)
    int16_t y0 = 12;
    if (settingsCursor == 0) {
        arduboy.fillRect(2, y0 - 1, 124, 9, WHITE);
        arduboy.setTextColor(BLACK);
    }
    arduboy.setCursor(4, y0);
    arduboy.printf("1.BRILLO : [ %3d%% ]", sysBrightness);
    arduboy.setTextColor(WHITE);

    // Item 1: Volumen / Audio
    int16_t y1 = 22;
    if (settingsCursor == 1) {
        arduboy.fillRect(2, y1 - 1, 124, 9, WHITE);
        arduboy.setTextColor(BLACK);
    }
    arduboy.setCursor(4, y1);
    arduboy.printf("2.SONIDO : [ %-5s ]", volumeLabels[sysVolume]);
    arduboy.setTextColor(WHITE);

    // Item 2: Batería (Lectura ADC en GPIO 2 - Voltaje y Porcentaje Li-Ion estabilizado)
    int16_t y2 = 32;
    float vBat = getBatteryVoltage();
    int batPct = getBatteryPercentage();

    if (settingsCursor == 2) {
        arduboy.fillRect(2, y2 - 1, 124, 9, WHITE);
        arduboy.setTextColor(BLACK);
    } else {
        arduboy.setTextColor(WHITE);
    }

    char batStr[24];
    snprintf(batStr, sizeof(batStr), "BATT: %d%% (%.2fV)", batPct, vBat);
    int16_t strLen = (int16_t)strlen(batStr);
    int16_t xBat = (128 - (strLen * 6)) / 2;
    if (xBat < 2) xBat = 2;

    arduboy.setCursor(xBat, y2);
    arduboy.print(batStr);
    arduboy.setTextColor(WHITE);

    // Item 3: Panel web de configuracion / OTA (brillo y sonido ya se guardan
    // solos en NVS, asi que no hace falta un boton de guardar).
    int16_t y3 = 43;
    if (settingsCursor == 3) {
        arduboy.fillRect(2, y3 - 1, 124, 9, WHITE);
        arduboy.setTextColor(BLACK);
        arduboy.setCursor(14, y3);
        arduboy.print("> CONFIG WEB <");
        arduboy.setTextColor(WHITE);
    } else {
        arduboy.setCursor(16, y3);
        arduboy.print("[ CONFIG WEB ]");
    }

    // Footer
    arduboy.drawFastHLine(0, 56, 128, WHITE);
    arduboy.setCursor(4, 57);
    arduboy.print("B:-  A:+  <:SALIR");
}

// Pantalla del AP de configuracion (Ajustes -> CONFIG WEB).
void drawConfigWebScreen() {
    arduboy.fillRect(0, 0, 128, 9, WHITE);
    arduboy.setTextColor(BLACK);
    arduboy.setCursor(16, 1);
    arduboy.print("CONFIGURACION WEB");
    arduboy.setTextColor(WHITE);

    arduboy.setCursor(1, 11);
    arduboy.print("AP: RetroArcade-ESP32");
    arduboy.setCursor(1, 21);
    arduboy.print("Web: http://config");
    arduboy.setCursor(1, 31);
    arduboy.print("IP:  192.168.4.1");
    arduboy.setCursor(1, 41);
    arduboy.print("mDNS: pingpong.local");

    arduboy.drawFastHLine(0, 56, 128, WHITE);
    arduboy.setCursor(1, 57);
    arduboy.print("B:APAGAR WIFI Y SALIR");
}

void enterConfigWebMode() {
    sound.tone(1300, 30);
    melodyPlayer.stop();
    wifiGamepads.begin();
    lastActivityTime = millis();
    currentState = STATE_WEB_CONFIG;
}

void exitConfigWebMode() {
    sound.tone(700, 30);
    wifiGamepads.stop();
    lastActivityTime = millis();
    settingsCursor = 3;
    currentState = STATE_SETTINGS;
}

void transitionMenuToSettings() {
    sound.tone(1100, 25);

    // 1. Capturar pantalla de Menú
    arduboy.clear();
    drawMainMenuScreen();
    memcpy(slideBufMenu, arduboy.getBuffer(), 1024);

    // 2. Capturar pantalla de Ajustes
    arduboy.clear();
    drawSettingsScreen();
    memcpy(slideBufSettings, arduboy.getBuffer(), 1024);

    // 3. Desplazamiento horizontal fluido (Menú sale hacia la izquierda, Ajustes entra desde la derecha)
    // 8 pasos de 16 píxeles a través de I2C 400kHz = ~184ms
    for (int16_t offset = 16; offset <= 128; offset += 16) {
        uint8_t* dst = arduboy.getBuffer();
        for (uint8_t page = 0; page < 8; page++) {
            uint8_t* pageDst = dst + (page * 128);
            uint8_t* menuSrc = slideBufMenu + (page * 128);
            uint8_t* settSrc = slideBufSettings + (page * 128);

            if (offset < 128) {
                memcpy(pageDst, menuSrc + offset, 128 - offset);
            }
            memcpy(pageDst + (128 - offset), settSrc, offset);
        }
        arduboy.display();
        yield();
        delay(1);
    }
    settingsCursor = 0;
    currentState = STATE_SETTINGS;
}

void transitionSettingsToMenu() {
    saveSystemSettings();
    sound.tone(880, 25);

    // 1. Capturar pantalla de Ajustes
    arduboy.clear();
    drawSettingsScreen();
    memcpy(slideBufSettings, arduboy.getBuffer(), 1024);

    // 2. Capturar pantalla de Menú
    arduboy.clear();
    drawMainMenuScreen();
    memcpy(slideBufMenu, arduboy.getBuffer(), 1024);

    // 3. Desplazamiento horizontal invertido (Ajustes sale hacia la derecha, Menú entra desde la izquierda)
    for (int16_t offset = 16; offset <= 128; offset += 16) {
        uint8_t* dst = arduboy.getBuffer();
        for (uint8_t page = 0; page < 8; page++) {
            uint8_t* pageDst = dst + (page * 128);
            uint8_t* menuSrc = slideBufMenu + (page * 128);
            uint8_t* settSrc = slideBufSettings + (page * 128);

            memcpy(pageDst, menuSrc + (128 - offset), offset);
            if (offset < 128) {
                memcpy(pageDst + offset, settSrc, 128 - offset);
            }
        }
        arduboy.display();
        yield();
        delay(1);
    }
    gamePaused = false;
    currentState = STATE_MENU;
}

void loop() {
    yield();

    // Reinicio diferido tras una OTA correcta y keep-awake durante la subida.
    consoleAdmin.update();
    if (consoleAdmin.isUpdating()) {
        lastActivityTime = millis();
    }

    // -------------------------------------------------------------------------
    // DIAGNÓSTICO EN TIEMPO REAL POR SERIAL (115200) - LECTURA DE CONTROLES
    // -------------------------------------------------------------------------
    const uint8_t diagPins[7] = {ARDUBOY_PIN_UP, ARDUBOY_PIN_DOWN, ARDUBOY_PIN_RIGHT, ARDUBOY_PIN_LEFT, PIN_NAV_CENTER, PIN_BUTTON_A, PIN_BUTTON_B};
    static unsigned long lastPinTrigger[7] = {0, 0, 0, 0, 0, 0, 0};
    for (uint8_t i = 0; i < 7; i++) {
        if (digitalRead(diagPins[i]) == LOW) {
            unsigned long now = millis();
            if (now - lastPinTrigger[i] >= 150) {
                lastPinTrigger[i] = now;
                Serial.printf("PIN ACTIVADO: GPIO %d\n", diagPins[i]);
            }
        }
    }
    // Non-blocking 1s RGB LED auto-off timer & tone auto-detach
    updateRgbLed();
    sound.update();
    melodyPlayer.update();



    // -------------------------------------------------------------------------
    // STANDBY POR SOFTWARE / SCREEN BLANKING (WAKEUP INMEDIATO SIN BUG I2C)
    // -------------------------------------------------------------------------
    if (isScreenSleeping && !consoleAdmin.isUpdating()) {
        melodyPlayer.stop();
        sound.noTone();
        // En reposo: omitir renderizado y físicas; monitorizar controles NES + D-Pad + BOOT
        bool btnUp = (digitalRead(ARDUBOY_PIN_UP) == LOW);           // GPIO 14 (UP)
        bool btnDown = (digitalRead(ARDUBOY_PIN_DOWN) == LOW);       // GPIO 18 (DOWN)
        bool btnRight = (digitalRead(ARDUBOY_PIN_RIGHT) == LOW);     // GPIO 19 (RIGHT)
        bool btnLeft = (digitalRead(ARDUBOY_PIN_LEFT) == LOW);       // GPIO 5  (LEFT)
        bool btnSelect = (digitalRead(PIN_NAV_CENTER) == LOW);       // GPIO 20 (CLICK CENTRAL)
        bool btnA = (digitalRead(PIN_BUTTON_A) == LOW);              // GPIO 3  (A - OK / SELECT)
        bool btnB = (digitalRead(PIN_BUTTON_B) == LOW);              // GPIO 4  (B - INACTIVO)
        bool boot = (digitalRead(ARDUBOY_PIN_BOOT) == LOW);          // GPIO 9  (BOOT)

        if (btnUp || btnDown || btnRight || btnLeft || btnSelect || btnA || btnB || boot) {
            // Despertar inmediato: Encender panel OLED, restaurar brillo y refrescar pantalla
            Arduboy2Core::sendCommand(SSD1306_DISPLAYON);
            applyBrightness(sysBrightness);
            arduboy.display();
            sound.tone(1200, 25);
            isScreenSleeping = false;
            lastActivityTime = millis();
            bootRawLowStart = 0;
            bootDebouncedDown = false;
            delay(150); // Debounce de despertar
        } else {
            delay(10);
        }
        return;
    }

    // Strict software debounce for BOOT button (GPIO 9): minimum 50ms continuous LOW
    bool bootPinLow = (digitalRead(ARDUBOY_PIN_BOOT) == LOW);
    if (bootPinLow) {
        if (bootRawLowStart == 0) {
            bootRawLowStart = millis();
        } else if (!bootDebouncedDown && (millis() - bootRawLowStart >= 50)) {
            // Valid continuous 50ms LOW confirmed
            bootDebouncedDown = true;
            bootDebouncedPressStart = millis();
            lastActivityTime = millis();
        }
    } else {
        bootRawLowStart = 0;
        if (bootDebouncedDown) {
            unsigned long duration = millis() - bootDebouncedPressStart;
            bootDebouncedDown = false;
            if (duration >= 50 && duration < 1000) {
                // Short press:
                if (currentState == STATE_PIN_DIAGNOSTIC) {
                    sound.tone(1000, 20);
                    currentState = STATE_MENU;
                } else if (currentState == STATE_MENU) {
                    sound.tone(1000, 20);
                    currentState = STATE_PIN_DIAGNOSTIC;
#ifdef ENABLE_TIC_TAC_TOE
                } else if (currentState == STATE_TIC_TAC_TOE) {
                    ticTacToe.onBootShortPress();
#endif
#ifdef ENABLE_BRAINDU
                } else if (currentState == STATE_BRAINDU_TRAIN) {
                    brainduTrain.onBootShortPress();
#endif
#ifdef ENABLE_HANGMAN
                } else if (currentState == STATE_HANGMAN) {
                    hangmanGame.onBootShortPress();
#endif
#ifdef ENABLE_TRIS
                } else if (currentState == STATE_TRIS) {
                    trisGame.onBootShortPress();
#endif
#ifdef ENABLE_MATH_CHALLENGE
                } else if (currentState == STATE_MATH_CHALLENGE) {
                    mathChallenge.onBootShortPress();
#endif
                } else if (currentState != STATE_MENU && currentState != STATE_SETTINGS && currentState != STATE_WEB_CONFIG) {
                    if (gamePaused) {
                        gamePaused = false;
                        sound.tone(900, 30);
                    } else if (isCurrentGameActive()) {
                        gamePaused = true;
                        pauseMenuCursor = 0;
                        melodyPlayer.stop();
                        sound.noTone();
                        sound.tone(600, 30);
                    }
                }
            } else if (duration >= 1000 && duration < 3000) {
                // Medium press (1s - 3s hold):
#ifdef ENABLE_BRAINDU
                if (currentState == STATE_BRAINDU_TRAIN) {
                    brainduTrain.onBootHold1s();
                }
#endif
            }
        }
    }

    // Check for sustained hold (>= 3000ms) to trigger Deep Sleep
    if (bootDebouncedDown && (millis() - bootDebouncedPressStart >= 3000)) {
        bootDebouncedDown = false;
        bootRawLowStart = 0;
        enterDeepSleep("APAGANDO...");
    }

    // Reset inactivity timer whenever any button is pressed
    if (arduboy.buttonsState() != 0 || bootDebouncedDown) {
        lastActivityTime = millis();
    }

    // Mandos moviles por Wi-Fi (Pong 2P): cada comando valido recibido por
    // WebSocket cuenta como actividad, igual que un boton fisico.
    if (wifiGamepads.isRunning && wifiGamepads.lastInputMs != 0) {
        if ((long)(wifiGamepads.lastInputMs - lastActivityTime) > 0) {
            lastActivityTime = wifiGamepads.lastInputMs;
        }
    }

    // Durante una partida LAN activa la pantalla permanece encendida aunque no
    // se pulse ningun control fisico de la consola.
#ifdef ENABLE_PING_PONG
    bool lanMatchActive = (currentState == STATE_PING_PONG &&
                           pingPong.mode == PP_MODE_WIFI_2P &&
                           pingPong.wifiMatchStarted &&
                           !pingPong.gameOver);
#else
    bool lanMatchActive = false;
#endif

    // Auto Standby tras 90s de inactividad (Screen Blanking)
    if (!lanMatchActive && currentState != STATE_WEB_CONFIG &&
        millis() - lastActivityTime >= INACTIVITY_TIMEOUT_MS) {
        melodyPlayer.stop();
        sound.noTone();
        Arduboy2Core::sendCommand(SSD1306_DISPLAYOFF);
        isScreenSleeping = true;
        return;
    }

    if (!arduboy.nextFrame()) return;

    arduboy.clear();

    bool wasPausedThisFrame = gamePaused;

    // GESTOR GLOBAL DE PAUSA Y SALIDA UNIVERSAL (BOTON B, A, UP, DOWN)
    if (currentState != STATE_MENU && currentState != STATE_SETTINGS &&
        currentState != STATE_WEB_CONFIG && currentState != STATE_PIN_DIAGNOSTIC) {
        if (gamePaused) {
            bool hasSub = currentGameHasSubmenu();
            int8_t pauseItemCount = hasSub ? 3 : 2;

            // 1. Navegación vertical entre opciones con UP y DOWN
            if (arduboy.justPressed(UP_BUTTON)) {
                sound.tone(880, 15);
                pauseMenuCursor--;
                if (pauseMenuCursor < 0) {
                    pauseMenuCursor = pauseItemCount - 1;
                }
            } else if (arduboy.justPressed(DOWN_BUTTON)) {
                sound.tone(700, 15);
                pauseMenuCursor++;
                if (pauseMenuCursor >= pauseItemCount) {
                    pauseMenuCursor = 0;
                }
            }

            // 2. Botón de pausa (B o click central): despausar de inmediato
            if (universalPausePressed()) {
                sound.tone(900, 30);
                gamePaused = false;
            }
            // 3. Botón A: Ejecutar opción seleccionada
            else if (arduboy.justPressed(A_BUTTON)) {
                if (pauseMenuCursor == 0) {
                    // 'Continuar': Reanuda la partida
                    sound.tone(900, 30);
                    gamePaused = false;
                } else if (hasSub && pauseMenuCursor == 1) {
                    // 'Menu Juego': Regresa al menú interno del juego actual
                    returnToCurrentGameSubmenu();
                } else {
                    // 'Menu Consola': Detiene el juego por completo, limpia estados temporales y transiciona a STATE_MENU
                    exitCurrentGameToMainMenu();
                }
            }
        } else {
            // Activar pausa con Botón B si el juego está en curso activo.
            if (isCurrentGameActive() && universalPausePressed()) {
                gamePaused = true;
                pauseMenuCursor = 0;
                melodyPlayer.stop();
                sound.noTone();
                sound.tone(600, 30);
            }
        }
    }

    bool allowGameUpdate = !gamePaused && !wasPausedThisFrame;

    // Universal Exit Combo: Hold A + B (o LEFT + B) >= 450ms returns to menu
    // Excluido en Trench Run y Road Trip: A/LEFT forman parte del control de
    // juego y el combo se disparaba por accidente en plena partida.
    if (currentState != STATE_MENU && currentState != STATE_PIN_DIAGNOSTIC &&
        currentState != STATE_SETTINGS && currentState != STATE_WEB_CONFIG
#ifdef ENABLE_TRENCH_RUN
        && currentState != STATE_TRENCH_RUN
#endif
#ifdef ENABLE_ROAD_TRIP
        && currentState != STATE_ROAD_TRIP
#endif
#ifdef ENABLE_WOLFENDUINO_3D
        && currentState != STATE_WOLFENSTEIN
#endif
#ifdef ENABLE_CATACOMBS_3D
        // En Catacombs A dispara y B pausa: el combo A+B no debe salir al menu.
        && currentState != STATE_CATACOMBS
#endif
#ifdef ENABLE_CYBER_ESCAPE
        && currentState != STATE_CYBER_ESCAPE
#endif
#ifdef ENABLE_SEA_BATTLE
        && currentState != STATE_SEA_BATTLE
#endif
#ifdef ENABLE_EIGHTEEN_HOLES
        // En 18 Holes A y B son controles de juego (golpear / menu interno).
        && currentState != STATE_EIGHTEEN_HOLES
#endif
        ) {
        bool comboHeld = (arduboy.pressed(LEFT_BUTTON) && arduboy.pressed(B_BUTTON)) ||
                         (arduboy.pressed(A_BUTTON) && arduboy.pressed(B_BUTTON));
        if (comboHeld) {
            if (exitComboStart == 0) {
                exitComboStart = millis();
            } else if (millis() - exitComboStart >= 450) {
                if (currentState == STATE_PROYECTO_BLE) {
                    proyectoBLE.stop();
#ifdef ENABLE_2048
                } else if (currentState == STATE_2048) {
                    game2048.saveHighScore();
#endif
#ifdef ENABLE_MATH_CHALLENGE
                } else if (currentState == STATE_MATH_CHALLENGE) {
                    mathChallenge.saveHighScore();
#endif
                }
                melodyPlayer.stop();
                sound.tone(600, 30);
                gamePaused = false;
                currentState = STATE_MENU;
                exitComboStart = 0;
            }
        } else {
            exitComboStart = 0;
        }
    }

    switch (currentState) {
        case STATE_MENU: {
            gamePaused = false;

            drawMainMenuScreen();

            // Bloquear lectura de controles en la misma trama de transición desde la pausa
            if (!wasPausedThisFrame) {
                // Navegación Vertical: EXCLUSIVAMENTE con la cruceta (UP y DOWN)
                if (arduboy.justPressed(UP_BUTTON)) {
                    sound.tone(880, 15);
                    menuCursor = menuStepVisible((uint8_t)menuCursor, -1);
                }
                if (arduboy.justPressed(DOWN_BUTTON)) {
                    sound.tone(700, 15);
                    menuCursor = menuStepVisible((uint8_t)menuCursor, +1);
                }

                // Transición de deslizamiento horizontal hacia Ajustes al pulsar RIGHT
                if (arduboy.justPressed(RIGHT_BUTTON)) {
                    transitionMenuToSettings();
                    break;
                }

                // Confirmación y lanzamiento al presionar Botón A (GPIO 3)
                if (arduboy.justPressed(A_BUTTON)) {
                    launchSelectedGame();
                }
            }
            break;
        }

#ifdef ENABLE_PING_PONG
        case STATE_PING_PONG:
            if (pingPong.exitRequested) {
                pingPong.exitRequested = false;
                if (wifiGamepads.isRunning) wifiGamepads.stop();
                currentState = STATE_MENU;
                break;
            }
            if (allowGameUpdate) pingPong.update();
            pingPong.draw();
            break;

#endif
#ifdef ENABLE_BREAKOUT
        case STATE_BREAKOUT:
            if (allowGameUpdate) breakout.update();
            breakout.draw();
            break;

#endif
#ifdef ENABLE_SPACE_INVADERS
        case STATE_SPACE_INVADERS:
            if (allowGameUpdate) spaceInvaders.update();
            spaceInvaders.draw();
            break;

#endif
#ifdef ENABLE_1943
        case STATE_1943:
            if (allowGameUpdate) airCombat.update();
            airCombat.draw();
            break;

#endif
#ifdef ENABLE_FLAPPY_BIRD
        case STATE_FLAPPY_BIRD:
            if (allowGameUpdate) flappy.update();
            flappy.draw();
            break;

#endif
#ifdef ENABLE_SNAKE
        case STATE_SNAKE:
            if (allowGameUpdate) snake.update();
            snake.draw();
            break;

#endif
#ifdef ENABLE_JUMP_MAN
        case STATE_JUMP_MAN:
            if (allowGameUpdate) jumpMan.update();
            jumpMan.draw();
            break;

#endif
        case STATE_PROYECTO_BLE:
            if (allowGameUpdate) proyectoBLE.update();
            proyectoBLE.draw();
            break;

#ifdef ENABLE_2048
        case STATE_2048:
            if (allowGameUpdate) game2048.update();
            game2048.draw();
            break;

#endif
#ifdef ENABLE_TREX_RUNNER
        case STATE_TREX_RUNNER:
            if (allowGameUpdate) trexRunner.update();
            trexRunner.draw();
            break;

#endif
#ifdef ENABLE_TIC_TAC_TOE
        case STATE_TIC_TAC_TOE:
            if (allowGameUpdate) ticTacToe.update();
            ticTacToe.draw();
            break;

#endif
#ifdef ENABLE_BRAINDU
        case STATE_BRAINDU_TRAIN:
            if (allowGameUpdate) brainduTrain.update();
            brainduTrain.draw();
            break;

#endif
#ifdef ENABLE_HANGMAN
        case STATE_HANGMAN:
            if (allowGameUpdate) hangmanGame.update();
            hangmanGame.draw();
            break;

#endif
#ifdef ENABLE_TRIS
        case STATE_TRIS:
            if (allowGameUpdate) trisGame.update();
            trisGame.draw();
            break;

#endif
#ifdef ENABLE_MATH_CHALLENGE
        case STATE_MATH_CHALLENGE:
            if (allowGameUpdate) mathChallenge.update();
            mathChallenge.draw();
            if (mathChallenge.exitRequested) {
                mathChallenge.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_SOPA_LETRAS
        case STATE_SOPA:
            if (allowGameUpdate) wordSearchGame.update();
            wordSearchGame.draw();
            if (wordSearchGame.exitRequested) {
                wordSearchGame.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_TRENCH_RUN
        case STATE_TRENCH_RUN:
            if (allowGameUpdate) trenchRun.update();
            trenchRun.draw();
            if (trenchRun.exitRequested) {
                trenchRun.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_ROAD_TRIP
        case STATE_ROAD_TRIP:
            if (allowGameUpdate) roadTrip.update();
            roadTrip.draw();
            if (roadTrip.exitRequested) {
                roadTrip.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_WOLFENDUINO_3D
        case STATE_WOLFENSTEIN:
            if (allowGameUpdate) wolfenstein.update();
            wolfenstein.draw();
            if (wolfenstein.exitRequested) {
                wolfenstein.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_CATACOMBS_3D
        case STATE_CATACOMBS:
            if (allowGameUpdate) catacombs3D.update();
            catacombs3D.draw();
            if (catacombs3D.exitRequested) {
                catacombs3D.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_CYBER_ESCAPE
        case STATE_CYBER_ESCAPE:
            if (allowGameUpdate) cyberEscape.update();
            cyberEscape.draw();
            if (cyberEscape.exitRequested) {
                cyberEscape.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_SEA_BATTLE
        case STATE_SEA_BATTLE:
            if (allowGameUpdate) seaBattle.update();
            seaBattle.draw();
            if (seaBattle.exitRequested) {
                seaBattle.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

#ifdef ENABLE_EIGHTEEN_HOLES
        case STATE_EIGHTEEN_HOLES:
            if (allowGameUpdate) eighteenHoles.update();
            eighteenHoles.draw();
            if (eighteenHoles.exitRequested) {
                eighteenHoles.exitRequested = false;
                currentState = STATE_MENU;
            }
            break;
#endif

        case STATE_PIN_DIAGNOSTIC: {
            // Header Bar
            arduboy.fillRect(0, 0, 128, 9, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(4, 1);
            arduboy.print("DIAGNOSTICO DE CONTROLES");
            arduboy.setTextColor(WHITE);

            bool pUp    = arduboy.pressed(UP_BUTTON) || (digitalRead(ARDUBOY_PIN_UP) == LOW);
            bool pDown  = arduboy.pressed(DOWN_BUTTON) || (digitalRead(ARDUBOY_PIN_DOWN) == LOW);
            bool pRight = arduboy.pressed(RIGHT_BUTTON) || (digitalRead(ARDUBOY_PIN_RIGHT) == LOW);
            bool pLeft  = arduboy.pressed(LEFT_BUTTON) || (digitalRead(ARDUBOY_PIN_LEFT) == LOW);
            bool pCtr   = arduboy.pressed(SYSTEM_MENU_BUTTON) || (digitalRead(PIN_NAV_CENTER) == LOW);
            bool pA     = arduboy.pressed(A_BUTTON) || (digitalRead(PIN_BUTTON_A) == LOW);
            bool pB     = arduboy.pressed(B_BUTTON) || (digitalRead(PIN_BUTTON_B) == LOW);
            bool pBoot  = (digitalRead(ARDUBOY_PIN_BOOT) == LOW);

            // Escaneo en vivo de todos los pines de control y candidatos
            int activeScanPin = -1;
            const uint8_t scanAll[] = {23, 22, 12, 21, 1, 0};
            for (uint8_t sp : scanAll) {
                if ((Arduboy2Core::validPullupMask & (1UL << sp)) && digitalRead(sp) == LOW) {
                    activeScanPin = sp;
                    break;
                }
            }

            // D-Pad: UP (GPIO 14)
            if (pUp) { arduboy.fillRect(26, 12, 22, 11, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(26, 12, 22, 11, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(28, 14); arduboy.print("U14");

            // D-Pad: CTR / OK (GPIO 20)
            if (pCtr) { arduboy.fillRect(26, 24, 22, 11, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(26, 24, 22, 11, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(28, 26); arduboy.print("C20");

            // D-Pad: DOWN (GPIO 18)
            if (pDown) { arduboy.fillRect(26, 36, 22, 11, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(26, 36, 22, 11, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(28, 38); arduboy.print("D18");

            // D-Pad: LEFT (GPIO 05)
            if (pLeft) { arduboy.fillRect(3, 24, 22, 11, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(3, 24, 22, 11, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(5, 26); arduboy.print("L05");

            // D-Pad: RIGHT (GPIO 19)
            if (pRight) { arduboy.fillRect(49, 24, 22, 11, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(49, 24, 22, 11, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(51, 26); arduboy.print("R19");

            // Action Buttons: BOTON B (GPIO 04 - INACTIVO)
            if (pB) { arduboy.fillRect(78, 22, 22, 13, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(78, 22, 22, 13, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(81, 25); arduboy.print("B04");

            // Action Buttons: BOTON A (GPIO 03 - OK / SELECCIONAR)
            if (pA) { arduboy.fillRect(103, 16, 22, 13, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(103, 16, 22, 13, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(106, 19); arduboy.print("A03");

            // BOOT (GPIO 9)
            if (pBoot) { arduboy.fillRect(80, 37, 44, 10, WHITE); arduboy.setTextColor(BLACK); }
            else { arduboy.drawRect(80, 37, 44, 10, WHITE); arduboy.setTextColor(WHITE); }
            arduboy.setCursor(84, 39); arduboy.print("BOOT 9");
            arduboy.setTextColor(WHITE);

            // Audio feedback per pin
            static uint8_t lastPlayedPin = 255;
            if (pUp && lastPlayedPin != ARDUBOY_PIN_UP) { sound.tone(1200, 30); lastPlayedPin = ARDUBOY_PIN_UP; }
            else if (pDown && lastPlayedPin != ARDUBOY_PIN_DOWN) { sound.tone(800, 30); lastPlayedPin = ARDUBOY_PIN_DOWN; }
            else if (pLeft && lastPlayedPin != ARDUBOY_PIN_LEFT) { sound.tone(950, 30); lastPlayedPin = ARDUBOY_PIN_LEFT; }
            else if (pRight && lastPlayedPin != ARDUBOY_PIN_RIGHT) { sound.tone(1100, 30); lastPlayedPin = ARDUBOY_PIN_RIGHT; }
            else if (pCtr && lastPlayedPin != PIN_NAV_CENTER) { sound.tone(1400, 30); lastPlayedPin = PIN_NAV_CENTER; }
            else if (pA && lastPlayedPin != PIN_BUTTON_A) { sound.tone(1760, 40); lastPlayedPin = PIN_BUTTON_A; }
            else if (pB && lastPlayedPin != PIN_BUTTON_B) { sound.tone(1500, 40); lastPlayedPin = PIN_BUTTON_B; }
            else if (pBoot && lastPlayedPin != 9) { sound.tone(600, 30); lastPlayedPin = 9; }
            else if (!pUp && !pDown && !pRight && !pLeft && !pCtr && !pA && !pB && !pBoot) { lastPlayedPin = 255; }

            // Footer / Status
            arduboy.drawFastHLine(0, 50, 128, WHITE);
            if (activeScanPin >= 0) {
                arduboy.setCursor(4, 53);
                arduboy.printf("PIN ACTIVO: GPIO %02d!", activeScanPin);
            } else if (pA) {
                arduboy.setCursor(4, 53); arduboy.print("BOTON A: GPIO 03 (OK/+)");
            } else if (pB) {
                arduboy.setCursor(4, 53); arduboy.print("BOTON B: GPIO 04 (B/-)");
            } else if (pCtr) {
                arduboy.setCursor(4, 53); arduboy.print("CLICK CRUCETA: GPIO 20");
            } else if (pUp) {
                arduboy.setCursor(4, 53); arduboy.print("CRUCETA ARRIBA: GPIO 14");
            } else if (pDown) {
                arduboy.setCursor(4, 53); arduboy.print("CRUCETA ABAJO: GPIO 18");
            } else if (pRight) {
                arduboy.setCursor(4, 53); arduboy.print("CRUCETA DERECHA: GPIO 19");
            } else if (pLeft) {
                arduboy.setCursor(4, 53); arduboy.print("CRUCETA IZQ: GPIO 05");
            } else if (pBoot) {
                arduboy.setCursor(4, 53); arduboy.print("BOTON BOOT: GPIO 09");
            } else {
                arduboy.setCursor(6, 53); arduboy.print("Pulsa pin | BOOT:Menu");
            }
            break;
        }

        case STATE_WEB_CONFIG: {
            // Atiende DNS captivo, HTTP (/config, /admin, /api/*, OTA) y mDNS.
            wifiGamepads.update();
            lastActivityTime = millis();

            // Durante una OTA no se permite apagar el AP.
            if (!consoleAdmin.isUpdating() && arduboy.justPressed(B_BUTTON)) {
                exitConfigWebMode();
                break;
            }
            drawConfigWebScreen();
            break;
        }

        case STATE_SETTINGS: {
            drawSettingsScreen();

            // Regreso al carrusel con animación de deslizamiento al pulsar LEFT
            if (arduboy.justPressed(LEFT_BUTTON)) {
                transitionSettingsToMenu();
                break;
            }

            // Navegación Vertical entre opciones con UP y DOWN
            if (arduboy.justPressed(UP_BUTTON)) {
                sound.tone(880, 15);
                settingsCursor--;
                if (settingsCursor < 0) settingsCursor = 3;
            }
            if (arduboy.justPressed(DOWN_BUTTON)) {
                sound.tone(700, 15);
                settingsCursor++;
                if (settingsCursor > 3) settingsCursor = 0;
            }

            // DISMINUIR / DESACTIVAR con Botón B (GPIO 4)
            if (arduboy.justPressed(B_BUTTON)) {
                if (settingsCursor == 0) {
                    if (sysBrightness > 10) {
                        applyBrightness(sysBrightness - 10);
                        sound.tone(1000, 20);
                    } else {
                        sound.tone(600, 15);
                    }
                } else if (settingsCursor == 1) {
                    stepVolume(-1); // baja 20 % por pulsacion hasta MUTE, sin salir
                } else if (settingsCursor == 2 || settingsCursor == 3) {
                    sound.tone(900, 20);
                }
            }

            // AUMENTAR / ACTIVAR / SELECCIONAR con Botón A (GPIO 3)
            if (arduboy.justPressed(A_BUTTON)) {
                if (settingsCursor == 0) {
                    if (sysBrightness < 100) {
                        applyBrightness(sysBrightness + 10);
                        sound.tone(1400, 20);
                    } else {
                        sound.tone(700, 15);
                    }
                } else if (settingsCursor == 1) {
                    stepVolume(+1); // sube hasta 100 % (duty 45 %)
                } else if (settingsCursor == 2) {
                    sound.tone(900, 20);
                } else if (settingsCursor == 3) {
                    enterConfigWebMode();
                    break;
                }
            }
            break;
        }
    }

    // Cuadro de Pausa Universal Centrado con drawRoundRect
    if (currentState != STATE_MENU && currentState != STATE_PIN_DIAGNOSTIC && currentState != STATE_SETTINGS && currentState != STATE_WEB_CONFIG && gamePaused) {
        bool hasSub = currentGameHasSubmenu();

        arduboy.fillRect(14, 10, 100, 44, BLACK);
        arduboy.drawRoundRect(14, 10, 100, 44, 3, WHITE);

        arduboy.setTextColor(WHITE);
        arduboy.setCursor((128 - (9 * 6)) / 2, 13);
        arduboy.print("[ PAUSA ]");

        if (hasSub) {
            arduboy.setCursor(20, 23);
            arduboy.print(pauseMenuCursor == 0 ? "> Continuar" : "  Continuar");

            arduboy.setCursor(20, 32);
            arduboy.print(pauseMenuCursor == 1 ? "> Menu Juego" : "  Menu Juego");

            arduboy.setCursor(20, 41);
            arduboy.print(pauseMenuCursor == 2 ? "> Menu Consola" : "  Menu Consola");
        } else {
            arduboy.setCursor(20, 25);
            arduboy.print(pauseMenuCursor == 0 ? "> Continuar" : "  Continuar");

            arduboy.setCursor(20, 36);
            arduboy.print(pauseMenuCursor == 1 ? "> Menu Consola" : "  Menu Consola");
        }
    }

    if (consoleAdmin.isUpdating()) consoleAdmin.drawOverlay();

    arduboy.display();
}
