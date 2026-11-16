// =============================================================================
// 18 HOLES - pegamento entre el nucleo original (core/) y la consola ESP32-C6
// =============================================================================
// El juego original (ArduGolf) esta escrito contra una pequena capa de
// abstraccion declarada en core/game.hpp:
//
//     extern uint8_t* const buf;   framebuffer 1bpp vertical (128x64)
//     time_ms()                    contador de ms
//     poll_btns()                  estado instantaneo de los botones
//     audio_enabled()/toggle_audio()/save_audio_on_off()
//     play_tone(...)               tonos
//
// Aqui solo se implementan esas funciones sobre el HAL de la consola. Los .inc
// son copias literales de los .cpp del autor; se incluyen dentro del namespace
// `eighteen_holes` para que ni sus globales ni sus funciones puedan colisionar
// con las de otro cartucho.
// =============================================================================

#include <Arduino.h>
#include <Arduboy2.h>
#include <ArduboyTones.h>
#include <string.h>

// Dependencias de sistema que core/save.inc incluye por dentro: deben entrar
// al ambito global antes de abrir el namespace.
#include <Preferences.h>

#include "console_core.h"
#include "eighteen_holes.h"

namespace eighteen_holes {

#include "core/game.hpp"

#include "core/game.inc"
#include "core/draw.inc"
#include "core/render_scene.inc"
#include "core/physics.inc"
#include "core/camera.inc"
#include "core/graphics.inc"
#include "core/font.inc"
#include "core/mat.inc"
#include "core/mul.inc"
#include "core/div.inc"
#include "core/sincos.inc"
#include "core/save.inc"

// -----------------------------------------------------------------------------
// HAL: framebuffer. El original dibuja en vertical por paginas (bit 0 = fila
// superior de la pagina), exactamente el mismo formato que Arduboy2Core.
// -----------------------------------------------------------------------------
uint8_t* const buf = ::Arduboy2Core::getBuffer();

// -----------------------------------------------------------------------------
// HAL: tiempo y botones
// -----------------------------------------------------------------------------
uint16_t time_ms()
{
    return (uint16_t)millis();
}

uint8_t poll_btns()
{
    // Mismos bits que UP_BUTTON/DOWN_BUTTON/LEFT_BUTTON/RIGHT_BUTTON/A_BUTTON/B_BUTTON.
    // Se lee el estado que main.cpp ya muestreo en este frame (nextFrame) para
    // que el juego vea exactamente los mismos botones que el menu de consola.
    return ::Arduboy2Core::currentButtonState;
}

// -----------------------------------------------------------------------------
// HAL: audio. Se delega en el driver global para respetar el silencio (mute).
// -----------------------------------------------------------------------------
bool audio_enabled()
{
    return ::sound.getVolume() > 0;
}

void toggle_audio()
{
    ::setSoundEnabled(!(::sound.getVolume() > 0));
}

void save_audio_on_off()
{
    ::saveSystemSettings();
}

// Volver al titulo desde el menu de pausa de la consola.
void request_title()
{
    reset_to_title();
}

bool at_title()
{
    return state == st::TITLE;
}

} // namespace eighteen_holes

// -----------------------------------------------------------------------------
// Ultimo frame renderizado: el menu de pausa de la consola se dibuja encima.
// -----------------------------------------------------------------------------
static uint8_t eh_last_frame[1024];

void EighteenHolesGame::init()
{
    ::Arduboy2Core::clear();
    eighteen_holes::game_setup();
    // Un tick de render para que el titulo aparezca en el mismo frame en que se
    // entra al cartucho (el original hacia game_loop() + delay(30) en el setup).
    eighteen_holes::game_loop();
    memcpy(eh_last_frame, ::Arduboy2Core::getBuffer(), sizeof(eh_last_frame));
    lastTickMs = millis();
    initialized = true;
    exitRequested = false;
}

void EighteenHolesGame::update()
{
    if(!initialized)
        init();

    // El original corre a ~30 fps (frame + delay de 30 ms en el .ino). Se
    // mantiene esa cadencia para no alterar velocidades ni fisicas.
    constexpr uint32_t FRAME_MS = 33;
    uint32_t now = millis();
    if(now - lastTickMs < FRAME_MS)
        return;
    lastTickMs = now;

    eighteen_holes::game_loop();
    memcpy(eh_last_frame, eighteen_holes::buf, sizeof(eh_last_frame));
}

void EighteenHolesGame::draw()
{
    if(!initialized)
        return;

    memcpy(::Arduboy2Core::getBuffer(), eh_last_frame, sizeof(eh_last_frame));
}

bool EighteenHolesGame::isPlaying() const
{
    // El cartucho ocupa la pantalla desde su titulo hasta la salida, asi que la
    // pausa universal (y el apagado) estan disponibles tambien en el titulo.
    return initialized;
}

void EighteenHolesGame::returnToTitle()
{
    if(!initialized)
        init();
    else
        eighteen_holes::request_title();
}
