#include <Arduino.h>
#include "Arduboy2.h"
#include "ArduboyTones.h"
#include "EEPROM.h"

#include "WolfPlatform.h"
#include "Engine.h"
#include "Sounds.h"

// Objetos globales de la consola (definidos en main.cpp).
extern Arduboy2 arduboy;
extern ArduboyTones sound;

ArduboyPlatform Platform;

uint8_t* GetScreenBuffer()
{
    return arduboy.getBuffer();
}

// -----------------------------------------------------------------------------
// Audio. El motor original reproduce patrones de tonos desde la memoria FX;
// aqui se traducen a pitidos del buzzer de la consola manteniendo el "feedback"
// de cada accion (disparo, impacto, puerta, objeto, muerte...).
// -----------------------------------------------------------------------------
void ArduboyPlatform::playSound(uint8_t id)
{
    if (m_isMuted) return;

    switch (id)
    {
    case ATKPISTOLSND:
    case ATKMACHINEGUNSND:
    case ATKGATLINGSND:
    case ATKKNIFESND:
    case SHOOTSND:
    case NAZIFIRESND:
    case SSFIRESND:
    case BOSSFIRESND:
        sound.tone(1100, 18);
        break;

    case HITENEMYSND:
    case HITWALLSND:
    case SHOOTDOORSND:
        sound.tone(220, 14);
        break;

    case TAKEDAMAGESND:
    case NAZIHITPLAYERSND:
        sound.tone(160, 40);
        break;

    case OPENDOORSND:
        sound.tone(330, 40);
        break;

    case CLOSEDOORSND:
        sound.tone(240, 35);
        break;

    case GETKEYSND:
    case GETAMMOSND:
    case GETMACHINESND:
    case GETGATLINGSND:
    case HEALTH1SND:
    case HEALTH2SND:
    case BONUS1SND:
    case BONUS2SND:
    case BONUS3SND:
    case BONUS4SND:
    case BONUS1UPSND:
        sound.tone(1400, 25);
        break;

    case SELECTWPNSND:
    case SELECTITEMSND:
        sound.tone(900, 20);
        break;

    case NOWAYSND:
    case NOITEMSND:
    case NOBONUSSND:
        sound.tone(200, 30);
        break;

    case PLAYERDEATHSND:
    case DEATHSCREAM1SND:
    case DEATHSCREAM2SND:
    case DEATHSCREAM3SND:
    case DEATHSCREAM4SND:
    case DEATHSCREAM5SND:
    case DEATHSCREAM6SND:
    case DEATHSCREAM7SND:
    case DEATHSCREAM8SND:
    case DEATHSCREAM9SND:
    case GAMEOVERSND:
        sound.tone(120, 90);
        break;

    case DOGBARKSND:
    case DOGDEATHSND:
        sound.tone(500, 30);
        break;

    case LEVELDONESND:
    case PERCENT100SND:
        sound.tone(1300, 60);
        break;

    case HEARTBEATSND:
        sound.tone(70, 30);
        break;

    case PUSHWALLSND:
        sound.tone(180, 60);
        break;

    default:
        sound.tone(700, 15);
        break;
    }
}

// -----------------------------------------------------------------------------
// Entrada. La cruceta se mapea 1:1. En partida el boton A dispara (como el B
// original) y, si se mantiene junto a LEFT/RIGHT, hace strafe (como el A
// original). En menus A/B conservan su papel original de seleccionar/volver.
// -----------------------------------------------------------------------------
void ArduboyPlatform::update()
{
    uint8_t in = 0;

    if (arduboy.pressed(UP_BUTTON))    in |= Input_Dpad_Up;
    if (arduboy.pressed(DOWN_BUTTON))  in |= Input_Dpad_Down;
    if (arduboy.pressed(LEFT_BUTTON))  in |= Input_Dpad_Left;
    if (arduboy.pressed(RIGHT_BUTTON)) in |= Input_Dpad_Right;

    bool gameplay =
        engine.gameState == GameState_Playing ||
        engine.gameState == GameState_StartingLevel ||
        engine.gameState == GameState_Loading ||
        engine.gameState == GameState_EnterNextLevel ||
        engine.gameState == GameState_Dead;

    bool a = arduboy.pressed(A_BUTTON);
    bool b = arduboy.pressed(B_BUTTON);
    bool horizontal = arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(RIGHT_BUTTON);

    static bool prevA = false;
    static uint32_t lastRelease = 0;
    static uint32_t switchUntil = 0;

    if (gameplay) {
        // B queda reservado a la pausa universal de main.cpp.
        if (a && !prevA && !horizontal && (millis() - lastRelease) < 320) {
            // Doble toque de A: ventana para cambiar de arma, como el original.
            switchUntil = millis() + 160;
        }

        if (switchUntil != 0 && (int32_t)(millis() - switchUntil) < 0) {
            in = Input_Btn_A;
        } else {
            switchUntil = 0;
            if (a) in |= horizontal ? Input_Btn_A : Input_Btn_B;
        }
    } else {
        if (a) in |= Input_Btn_A;
        if (b) in |= Input_Btn_B;
    }

    if (!a && prevA) lastRelease = millis();
    prevA = a;

    inputState = in;
}

void ERROR(const char* msg)
{
    Serial.print("WOLF3D: ");
    Serial.println(msg);
}

// -----------------------------------------------------------------------------
// Guardado: misma estructura que el save FX, en un bloque alto de la EEPROM
// emulada para no chocar con las puntuaciones de la consola.
// -----------------------------------------------------------------------------
static const int WOLF_SAVE_ADDRESS = 512;

void writeSaveFile(const uint8_t* buffer, int length)
{
    for (int i = 0; i < length; i++) {
        EEPROM.write(WOLF_SAVE_ADDRESS + i, buffer[i]);
    }
    EEPROM.commit();
}

bool readSaveFile(uint8_t* buffer, int length)
{
    if (EEPROM.read(WOLF_SAVE_ADDRESS) == 0xFF) {
        memset(buffer, 0, length);
        return false;
    }
    for (int i = 0; i < length; i++) {
        buffer[i] = EEPROM.read(WOLF_SAVE_ADDRESS + i);
    }
    return true;
}
