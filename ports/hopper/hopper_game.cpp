#include "hopper_game.h"
#include "common.h"

// =============================================================================
// Despachador equivalente al loop() de hopper.ino. La consola ya marca el
// ritmo de 60 FPS (el original fijaba 60) y dibuja el framebuffer, por lo que
// aqui solo se separa la actualizacion del dibujado.
// =============================================================================
namespace HopperGame {

enum MODE {
    LOGO_MODE = 0,
    TITLE_MODE,
    GAME_MODE
};

typedef struct {
    void (*initFunc)(void);
    bool (*updateFunc)(void);
    void (*drawFunc)(void);
} MODULE_FUNCS;

MyArduboy arduboy;

static const MODULE_FUNCS moduleTable[] = {
    { initLogo,  updateLogo,  drawLogo },
    { initTitle, updateTitle, drawTitle },
    { initGame,  updateGame,  drawGame },
};

static uint8_t mode = LOGO_MODE;
static bool pendingDone = false;

void setup() {
    mode = LOGO_MODE;
    pendingDone = false;
    moduleTable[mode].initFunc();
}

void update() {
    // Latch propio de botones y avance de la melodia en curso.
    arduboy.latchButtons();
    arduboy.tunes.update();

    // Solo se permite tocar la flash fuera de la partida activa: el commit de
    // 1 KB a NVS hunde la alimentacion justo cuando el jugador esta saltando.
    const bool activePlay = (mode == GAME_MODE) && !isGameOver();
    hopperEeprom.setCommitAllowed(!activePlay);
    hopperEeprom.poll();

    // El cambio de modo ocurre despues de mostrar el ultimo frame del modo
    // anterior, igual que en el loop() original.
    if (pendingDone) {
        pendingDone = false;
        hopperEeprom.flush();
        mode = (mode == TITLE_MODE) ? GAME_MODE : TITLE_MODE;
        moduleTable[mode].initFunc();
    }

    if (moduleTable[mode].updateFunc()) {
        pendingDone = true;
    }

    // El despachador corre dentro del bucle principal de la consola: ceder el
    // CPU cada frame evita que un frame largo dispare el Task Watchdog.
    yield();
}

void draw() {
    moduleTable[mode].drawFunc();
}

void loop() {
    update();
    draw();
}

} // namespace HopperGame

void HopperPort::init() {
    HopperGame::hopperEeprom.flush(); // record pendiente de la sesion anterior
    exitRequested = false;
    HopperGame::exitRequested = false;
    HopperGame::setup();
}

void HopperPort::update() {
    HopperGame::update();
    if (HopperGame::exitRequested) {
        HopperGame::hopperEeprom.flush(); // salida al menu: momento seguro para la flash
        exitRequested = true;
        HopperGame::exitRequested = false;
    }
}

void HopperPort::draw() {
    HopperGame::draw();
}
