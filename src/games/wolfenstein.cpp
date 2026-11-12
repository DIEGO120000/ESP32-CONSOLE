// =============================================================================
// WOLFENSTEIN 3D - Port de WolfenduinoFX para la consola ESP32-C6.
//
// El motor, los mapas, las texturas, los sprites y los menus son los del
// proyecto original de James Howard (jhhoward/WolfenduinoFX, licencia MIT):
// 10 niveles del episodio shareware, enemigos, armas, puertas secretas,
// 4 dificultades y tabla de records.
//
// Lo unico adaptado es la capa de plataforma:
//   - Los datos FX (261 KB) se incrustan en flash mediante diskRead().
//   - El dibujado escribe en el framebuffer de Arduboy2 de esta consola.
//   - El guardado usa la EEPROM emulada.
//   - El audio se traduce a tonos del buzzer.
//
// Controles:
//   Cruceta       : mover / girar (como el original)
//   A             : disparar / abrir puertas
//   A + LEFT/RIGHT: strafe lateral
//   B             : pausa universal (main.cpp); mantenido en menus sale al menu
// =============================================================================

#include <Arduino.h>

#include "games/wolfenstein.h"
#include "../wolf3d/Engine.h"
#include "../wolf3d/WolfPlatform.h"


struct WolfensteinGame::Impl
{
    uint32_t lastTick;
    uint32_t bHeldSince;
    uint16_t accumulator;
};

WolfensteinGame::WolfensteinGame()
{
    static Impl instance;
    impl = &instance;
    impl->lastTick = 0;
    impl->bHeldSince = 0;
    impl->accumulator = 0;
    exitRequested = false;
}

void WolfensteinGame::init()
{
    Platform.m_isMuted = false;
    Platform.inputState = 0;

    engine.init();

    impl->lastTick = millis();
    impl->bHeldSince = 0;
    impl->accumulator = 0;
    exitRequested = false;
}

bool WolfensteinGame::isPlaying() const
{
    switch (engine.gameState)
    {
        case GameState_Playing:
        case GameState_StartingLevel:
        case GameState_Loading:
        case GameState_EnterNextLevel:
        case GameState_Dead:
            return true;
        default:
            return false;
    }
}

void WolfensteinGame::update()
{
    Platform.update();

    // En menus, B se usa para volver; mantenerlo pulsado sale a la consola.
    if (!isPlaying())
    {
        if (arduboy.pressed(B_BUTTON))
        {
            if (impl->bHeldSince == 0) {
                impl->bHeldSince = millis();
            } else if (millis() - impl->bHeldSince >= 700) {
                impl->bHeldSince = 0;
                exitRequested = true;
                return;
            }
        }
        else
        {
            impl->bHeldSince = 0;
        }
    }
    else
    {
        impl->bHeldSince = 0;
    }

    // El motor original corre a 25 Hz; se mantiene ese ritmo con acumulador.
    uint32_t now = millis();
    uint32_t delta = now - impl->lastTick;
    impl->lastTick = now;
    if (delta > 250) delta = 250;
    impl->accumulator += (uint16_t)delta;

    const uint16_t step = 1000 / TARGET_FRAMERATE;
    uint8_t steps = 0;
    while (impl->accumulator >= step && steps < 4)
    {
        engine.update();
        impl->accumulator -= step;
        steps++;
    }
}

void WolfensteinGame::draw()
{
    engine.draw();
}
