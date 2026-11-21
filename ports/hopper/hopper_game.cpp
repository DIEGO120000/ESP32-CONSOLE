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
    hopperEeprom.poll();

    // El cambio de modo ocurre despues de mostrar el ultimo frame del modo
    // anterior, igual que en el loop() original.
    if (pendingDone) {
        pendingDone = false;
        mode = (mode == TITLE_MODE) ? GAME_MODE : TITLE_MODE;
        moduleTable[mode].initFunc();
    }

    if (moduleTable[mode].updateFunc()) {
        pendingDone = true;
    }
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
    HopperGame::setup();
}

void HopperPort::update() {
    HopperGame::update();
}

void HopperPort::draw() {
    HopperGame::draw();
}
