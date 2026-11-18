#pragma once
#include <Arduino.h>
#include "ArduboyTones.h"

// Capa de compatibilidad con la API original de Arduboy2Audio. Antes on()/off()
// eran vacias, asi que cualquier juego que silenciaba el audio desde su menu
// seguia sonando. Ahora se delega en el driver: off() deja el 2N2222 en corte
// (duty 0) y on() vuelve a habilitar las notas con el volumen global guardado.
class Arduboy2Audio {
public:
    static void on() { ArduboyTones::enableAudio(true); }
    static void off() { ArduboyTones::enableAudio(false); }
    static void toggle() {
        if (enabled()) {
            off();
        } else {
            on();
        }
    }
    static bool enabled() { return ArduboyTones::isAudioEnabled(); }
    static void saveOnOff() {}
    static void begin() {}
};
