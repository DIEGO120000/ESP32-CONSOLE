// =============================================================================
// EMULADOR NATIVO DE LA CONSOLA (PC + SDL2)
// =============================================================================
// Renderiza el mismo framebuffer de 1024 bytes (128x64) que la consola fisica a
// escala 5x (640x320), con los cartuchos reales de ports/ compilados tal cual.
// No es un port del firmware: el nucleo ESP32 (Wi-Fi, NVS, OTA) se sustituye por
// los shims de sim/ y aqui se replica el menu y el bucle principal.
//
//   pio run -e pc_emulator && .pio/build/pc_emulator/program
//
// Teclas: flechas o WASD = cruceta | Enter/Espacio = CENTER | Z/J = A | X/K = B
//         P o Esc = pausa | Retroceso = volver al menu | Q = salir
// =============================================================================

#include <SDL2/SDL.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <thread>

#include "Arduino.h"
#include "Arduboy2.h"
#include "console_core.h"
#include "EEPROM.h"
#include "games_config.h"

#include "../ports/arcade_1943/arcade_1943.h"
#include "../ports/braindu/braindu.h"
#include "../ports/breakout/breakout.h"
#include "../ports/cyber_escape/cyber_escape.h"
#include "../ports/flappy_bird/flappy_bird.h"
#include "../ports/game_2048/game_2048.h"
#include "../ports/hangman/hangman.h"
#include "../ports/jump_man/jump_man.h"
#include "../ports/math_challenge/math_challenge.h"
#include "../ports/ping_pong/ping_pong.h"
#include "../ports/road_trip/road_trip.h"
#include "../ports/sea_battle/sea_battle.h"
#include "../ports/snake/snake.h"
#include "../ports/sopa_letras/sopa_letras.h"
#include "../ports/space_invaders/space_invaders.h"
#include "../ports/tic_tac_toe/tic_tac_toe.h"
#include "../ports/trench_run/trench_run.h"
#include "../ports/trex_runner/trex_runner.h"
#include "../ports/tris/tris.h"
#include "../ports/wolfenduino_3d/wolfenstein.h"

// =============================================================================
// NUCLEO SIMULADO (lo que en el firmware vive en main.cpp/console_core)
// =============================================================================
Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);

uint8_t sysVolume = 1;
uint8_t sysBrightness = 80;

PingPongGame pingPong;
BreakoutGame breakout;
SpaceInvadersGame spaceInvaders;
AirCombatGame airCombat;
FlappyGame flappy;
SnakeGame snake;
JumpManGame jumpMan;
Game2048 game2048;
TRexGame trexRunner;
TicTacToeGame ticTacToe;
BrainduTrainGame brainduTrain;
HangmanGame hangmanGame;
TrisGame trisGame;
MathChallengeGame mathChallenge;
WordSearchGame wordSearchGame;
TrenchRunGame trenchRun;
RoadTripGame roadTrip;
WolfensteinGame wolfenstein;
CyberEscapeGame cyberEscape;
SeaBattleGame seaBattle;

uint8_t Arduboy2Core::sBuffer[WIDTH * HEIGHT / 8] = {0};
uint8_t Arduboy2Core::currentButtonState = 0;
uint8_t Arduboy2Core::previousButtonState = 0;
bool Arduboy2Core::invertActive = false;

void applyBrightness(uint8_t percent) { sysBrightness = percent; }
void setSoundEnabled(bool on) { sysVolume = on ? 1 : 0; }
void loadSystemSettings() {}
void saveSystemSettings() {}
float getBatteryVoltage() { return Arduboy2Core::getBatteryVoltage(); }
int getBatteryPercentage() { return Arduboy2Core::getBatteryPercentage(); }
void triggerRgbLed(uint8_t, uint8_t, uint8_t, uint16_t) {}

// --- Tiempo -----------------------------------------------------------------
static std::chrono::steady_clock::time_point gStart = std::chrono::steady_clock::now();

unsigned long millis() {
    auto d = std::chrono::steady_clock::now() - gStart;
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}
unsigned long micros() {
    auto d = std::chrono::steady_clock::now() - gStart;
    return (unsigned long)std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}
void delay(unsigned long ms) { SDL_Delay((Uint32)ms); }
void delayMicroseconds(unsigned int us) { if (us > 1000) SDL_Delay(us / 1000); }

void randomSeed(unsigned long seed) { srand((unsigned)seed); }
long random(long max) { return (max <= 0) ? 0 : (long)(rand() % max); }
long random(long min, long max) { return (max <= min) ? min : min + (long)(rand() % (max - min)); }

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// =============================================================================
// VIDEO: ventana 640x320 (5x), paleta OLED azul sobre negro
// =============================================================================
static SDL_Window *gWindow = nullptr;
static SDL_Renderer *gRenderer = nullptr;
static SDL_Texture *gTexture = nullptr;
static uint32_t gPixels[WIDTH * HEIGHT];

static const SDL_Color OLED_ON  = {0x29, 0xB6, 0xF6, 0xFF}; // #29B6F6
static const SDL_Color OLED_OFF = {0x05, 0x08, 0x0C, 0xFF}; // #05080C

void simPresentFrame(const uint8_t *buffer, bool inverted) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            const uint8_t byte = buffer[(y / 8) * WIDTH + x];
            bool on = (byte >> (y % 8)) & 0x01;
            if (inverted) on = !on;
            const SDL_Color &c = on ? OLED_ON : OLED_OFF;
            gPixels[y * WIDTH + x] = (0xFFu << 24) | ((uint32_t)c.r << 16) |
                                     ((uint32_t)c.g << 8) | c.b;
        }
    }
    SDL_UpdateTexture(gTexture, nullptr, gPixels, WIDTH * sizeof(uint32_t));
    SDL_RenderClear(gRenderer);
    SDL_RenderCopy(gRenderer, gTexture, nullptr, nullptr);
    SDL_RenderPresent(gRenderer);
}

// =============================================================================
// AUDIO: onda cuadrada mono en el dispositivo por defecto
// =============================================================================
static std::atomic<uint16_t> gToneFreq(0);

static void audioCallback(void *, Uint8 *stream, int len) {
    Sint16 *out = reinterpret_cast<Sint16 *>(stream);
    const int samples = len / (int)sizeof(Sint16);
    static double phase = 0.0;
    const uint16_t freq = gToneFreq.load(std::memory_order_relaxed);

    for (int i = 0; i < samples; i++) {
        if (freq < 20) {
            out[i] = 0;
            continue;
        }
        phase += (double)freq / 44100.0;
        if (phase >= 1.0) phase -= 1.0;
        out[i] = (phase < 0.5) ? 3000 : -3000;
    }
}

void simAudioTone(uint16_t freq) { gToneFreq.store(freq); }
void simAudioOff() { gToneFreq.store(0); }

// =============================================================================
// TECLADO -> BOTONES (mismo mapeo que la consola fisica)
// =============================================================================
static uint8_t gButtons = 0;

uint8_t simButtonsState() { return gButtons; }

static void pollKeyboard(const Uint8 *keys) {
    uint8_t b = 0;
    auto any = [&](SDL_Scancode a, SDL_Scancode c) {
        return keys[a] || (c != SDL_SCANCODE_UNKNOWN && keys[c]);
    };
    if (any(SDL_SCANCODE_UP, SDL_SCANCODE_W)) b |= UP_BUTTON;
    if (any(SDL_SCANCODE_DOWN, SDL_SCANCODE_S)) b |= DOWN_BUTTON;
    if (any(SDL_SCANCODE_LEFT, SDL_SCANCODE_A)) b |= LEFT_BUTTON;
    if (any(SDL_SCANCODE_RIGHT, SDL_SCANCODE_D)) b |= RIGHT_BUTTON;
    if (any(SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE)) b |= CLICK_BUTTON;
    if (any(SDL_SCANCODE_Z, SDL_SCANCODE_J)) b |= A_BUTTON;
    if (any(SDL_SCANCODE_X, SDL_SCANCODE_K)) b |= B_BUTTON;
    gButtons = b;
}

// =============================================================================
// MENU DEL EMULADOR (misma lista de ports que el firmware)
// =============================================================================
struct SimEntry {
    const char *label;
    void (*init)();
    void (*update)();
    void (*draw)();
    bool *exitFlag;
};

static const SimEntry simMenu[] = {
#ifdef ENABLE_PING_PONG
    {"Ping Pong", [] { pingPong.init(); }, [] { pingPong.update(); }, [] { pingPong.draw(); }, &pingPong.exitRequested},
#endif
#ifdef ENABLE_BREAKOUT
    {"Breakout", [] { breakout.init(); }, [] { breakout.update(); }, [] { breakout.draw(); }, nullptr},
#endif
#ifdef ENABLE_SPACE_INVADERS
    {"Space Invaders", [] { spaceInvaders.init(); }, [] { spaceInvaders.update(); }, [] { spaceInvaders.draw(); }, nullptr},
#endif
#ifdef ENABLE_1943
    {"1943", [] { airCombat.init(); }, [] { airCombat.update(); }, [] { airCombat.draw(); }, nullptr},
#endif
#ifdef ENABLE_FLAPPY_BIRD
    {"Flappy Bird", [] { flappy.init(); }, [] { flappy.update(); }, [] { flappy.draw(); }, nullptr},
#endif
#ifdef ENABLE_SNAKE
    {"Snake", [] { snake.init(); }, [] { snake.update(); }, [] { snake.draw(); }, nullptr},
#endif
#ifdef ENABLE_JUMP_MAN
    {"Jump Man", [] { jumpMan.init(); }, [] { jumpMan.update(); }, [] { jumpMan.draw(); }, nullptr},
#endif
#ifdef ENABLE_2048
    {"2048", [] { game2048.init(); }, [] { game2048.update(); }, [] { game2048.draw(); }, nullptr},
#endif
#ifdef ENABLE_TREX_RUNNER
    {"T-Rex Runner", [] { trexRunner.init(); }, [] { trexRunner.update(); }, [] { trexRunner.draw(); }, nullptr},
#endif
#ifdef ENABLE_TIC_TAC_TOE
    {"Tic-Tac-Toe", [] { ticTacToe.init(); }, [] { ticTacToe.update(); }, [] { ticTacToe.draw(); }, nullptr},
#endif
#ifdef ENABLE_BRAINDU
    {"BrainduTrain", [] { brainduTrain.init(); }, [] { brainduTrain.update(); }, [] { brainduTrain.draw(); }, nullptr},
#endif
#ifdef ENABLE_HANGMAN
    {"Hangman", [] { hangmanGame.init(); }, [] { hangmanGame.update(); }, [] { hangmanGame.draw(); }, nullptr},
#endif
#ifdef ENABLE_TRIS
    {"TRIS", [] { trisGame.init(); }, [] { trisGame.update(); }, [] { trisGame.draw(); }, nullptr},
#endif
#ifdef ENABLE_MATH_CHALLENGE
    {"Math Challenge", [] { mathChallenge.init(); }, [] { mathChallenge.update(); }, [] { mathChallenge.draw(); }, &mathChallenge.exitRequested},
#endif
#ifdef ENABLE_SOPA_LETRAS
    {"Sopa de Letras", [] { wordSearchGame.init(); }, [] { wordSearchGame.update(); }, [] { wordSearchGame.draw(); }, &wordSearchGame.exitRequested},
#endif
#ifdef ENABLE_TRENCH_RUN
    {"Trench Run", [] { trenchRun.init(); }, [] { trenchRun.update(); }, [] { trenchRun.draw(); }, &trenchRun.exitRequested},
#endif
#ifdef ENABLE_ROAD_TRIP
    {"Road Trip", [] { roadTrip.init(); }, [] { roadTrip.update(); }, [] { roadTrip.draw(); }, &roadTrip.exitRequested},
#endif
#ifdef ENABLE_WOLFENDUINO_3D
    {"Wolfenstein 3D", [] { wolfenstein.init(); }, [] { wolfenstein.update(); }, [] { wolfenstein.draw(); }, nullptr},
#endif
#ifdef ENABLE_CYBER_ESCAPE
    {"Cyber Escape", [] { cyberEscape.init(); }, [] { cyberEscape.update(); }, [] { cyberEscape.draw(); }, &cyberEscape.exitRequested},
#endif
#ifdef ENABLE_SEA_BATTLE
    {"Sea Battle", [] { seaBattle.init(); }, [] { seaBattle.update(); }, [] { seaBattle.draw(); }, &seaBattle.exitRequested},
#endif
};

static const int SIM_MENU_COUNT = (int)(sizeof(simMenu) / sizeof(simMenu[0]));

static void drawSimMenu(int cursor) {
    arduboy.fillRect(0, 0, 128, 9, WHITE);
    arduboy.setTextColor(BLACK);
    arduboy.setCursor(4, 1);
    arduboy.print("PC EMULATOR 128x64");
    arduboy.setTextColor(WHITE);

    int start = 0;
    if (cursor > 3) start = cursor - 3;
    if (start + 5 > SIM_MENU_COUNT) start = (SIM_MENU_COUNT > 5) ? SIM_MENU_COUNT - 5 : 0;
    const int rows = (SIM_MENU_COUNT < 5) ? SIM_MENU_COUNT : 5;

    for (int i = 0; i < rows; i++) {
        const int idx = start + i;
        const int y = 11 + i * 9;
        if (idx == cursor) {
            arduboy.fillRect(2, y - 1, 124, 9, WHITE);
            arduboy.setTextColor(BLACK);
            arduboy.setCursor(6, y);
            arduboy.printf("> %d. %s", idx + 1, simMenu[idx].label);
            arduboy.setTextColor(WHITE);
        } else {
            arduboy.setCursor(6, y);
            arduboy.printf("  %d. %s", idx + 1, simMenu[idx].label);
        }
    }

    arduboy.drawFastHLine(0, 56, 128, WHITE);
    arduboy.setCursor(2, 57);
    arduboy.print("A:JUGAR ESC:PAUSA");
}

static void drawPauseOverlay() {
    arduboy.fillRect(10, 20, 108, 24, BLACK);
    arduboy.drawRoundRect(10, 20, 108, 24, 4, WHITE);
    arduboy.setCursor(46, 26);
    arduboy.print("PAUSA");
    arduboy.setCursor(16, 36);
    arduboy.print("ESC/P: SEGUIR");
}

int main(int, char **) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    gWindow = SDL_CreateWindow("ESP32-C6 RetroConsole Emulator (128x64 OLED 2.42)",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               640, 320, SDL_WINDOW_SHOWN);
    if (!gWindow) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer) gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_SOFTWARE);
    if (!gRenderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(gWindow);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(gRenderer, WIDTH, HEIGHT);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetRenderDrawColor(gRenderer, OLED_OFF.r, OLED_OFF.g, OLED_OFF.b, 0xFF);

    gTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    SDL_SetTextureBlendMode(gTexture, SDL_BLENDMODE_NONE);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audioCallback;
    SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audioDev) SDL_PauseAudioDevice(audioDev, 0);

    EEPROM.begin();
    arduboy.begin();
    sound.setSpeakerPin(1);
    sound.setVolume(3);
    sound.enableAudio(true);
    srand((unsigned)time(nullptr));

    printf("[SIM] ESP32-C6 RetroConsole Emulator · %d cartuchos · 640x320 (5x)\n", SIM_MENU_COUNT);
    printf("[SIM] Teclas: flechas/WASD cruceta | Enter/Espacio CENTER | Z/J A | X/K B\n");
    printf("[SIM] P/Esc pausa | Retroceso volver al menu | Q salir\n");

    int cursor = 0;
    int current = -1; // -1 = menu
    bool paused = false;
    bool running = true;
    bool confirmRelease = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type != SDL_KEYDOWN) continue;
            const SDL_Keycode k = ev.key.keysym.sym;

            if (k == SDLK_q) { running = false; break; }
            if (k == SDLK_p || k == SDLK_ESCAPE) {
                if (current >= 0) {
                    paused = !paused;
                    if (paused) sound.noTone();
                }
                continue;
            }
            if (k == SDLK_BACKSPACE && current >= 0) {
                current = -1;
                paused = false;
                sound.noTone();
                confirmRelease = false;
            }
        }

        const Uint8 *keys = SDL_GetKeyboardState(nullptr);
        pollKeyboard(keys);

        // nextFrame() ya hace pollButtons() con el estado de teclado de este ciclo.
        if (!arduboy.nextFrame()) {
            SDL_Delay(1);
            continue;
        }

        arduboy.clear();

        if (current < 0) {
            if (!confirmRelease) {
                if (arduboy.notPressed(A_BUTTON)) confirmRelease = true;
            } else {
                if (arduboy.justPressed(UP_BUTTON)) { cursor = (cursor + SIM_MENU_COUNT - 1) % SIM_MENU_COUNT; sound.tone(880, 15); }
                if (arduboy.justPressed(DOWN_BUTTON)) { cursor = (cursor + 1) % SIM_MENU_COUNT; sound.tone(700, 15); }
                if (arduboy.justPressed(A_BUTTON) || arduboy.justPressed(CLICK_BUTTON)) {
                    current = cursor;
                    paused = false;
                    simMenu[current].init();
                    confirmRelease = false;
                }
            }
            drawSimMenu(cursor);
        } else {
            const SimEntry &e = simMenu[current];
            if (!paused) {
                e.update();
                if (e.exitFlag && *e.exitFlag) {
                    *e.exitFlag = false;
                    current = -1;
                    paused = false;
                    sound.noTone();
                    continue;
                }
            }
            e.draw();
            if (paused) drawPauseOverlay();
        }

        sound.update();
        melodyPlayer.update();
        arduboy.display();
    }

    if (audioDev) SDL_CloseAudioDevice(audioDev);
    SDL_DestroyTexture(gTexture);
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
    return 0;
}
