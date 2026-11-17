// =============================================================================
// CATACOMBS OF THE DAMNED - cartucho para la consola ESP32-C6.
//
// Envuelve el motor original de James Howard (jhhoward/Arduboy3D, MIT) sin
// modificarlo: los ticks corren a los 30 Hz originales y el dibujado va al
// framebuffer del OLED. El unico codigo nuevo es esta capa de integracion.
// =============================================================================

#include <Arduino.h>

#include "catacombs_3d.h"
#include "engine/Game.h"
#include "engine/Platform.h"
#include "engine/Defines.h"
#include "engine/Save.h"

extern Arduboy2 arduboy;
extern ArduboyTones sound;

using namespace Catacombs3D;

struct Catacombs3DGame::Impl
{
    uint32_t lastTick;
    uint16_t accumulator;
    uint32_t bHeldSince;
};

Catacombs3DGame::Catacombs3DGame()
{
    static Impl instance;
    impl = &instance;
    impl->lastTick = 0;
    impl->accumulator = 0;
    impl->bHeldSince = 0;
    exitRequested = false;
}

void Catacombs3DGame::init()
{
    Save::Begin();

    Game::Init();
    Game::SwitchState(Game::State::Menu);

    impl->lastTick = millis();
    impl->accumulator = 0;
    impl->bHeldSince = 0;
    exitRequested = false;
}

bool Catacombs3DGame::isPlaying() const
{
    return Game::IsPlaying();
}

void Catacombs3DGame::update()
{
    Platform::UpdateAudio();

    // En los menus internos, B mantenido vuelve al menu de la consola.
    if (!isPlaying())
    {
        if (arduboy.pressed(B_BUTTON))
        {
            if (impl->bHeldSince == 0)
            {
                impl->bHeldSince = millis();
            }
            else if (millis() - impl->bHeldSince >= 700)
            {
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

    // El motor original corre a 30 Hz; se mantiene ese ritmo con acumulador.
    uint32_t now = millis();
    uint32_t delta = now - impl->lastTick;
    impl->lastTick = now;
    if (delta > 250) delta = 250;
    impl->accumulator += (uint16_t)delta;

    const uint16_t step = 1000 / TARGET_FRAMERATE;
    uint8_t steps = 0;
    while (impl->accumulator >= step && steps < 4)
    {
        Game::Tick();
        impl->accumulator -= step;
        steps++;
    }
}

void Catacombs3DGame::draw()
{
    Game::Draw();
}
