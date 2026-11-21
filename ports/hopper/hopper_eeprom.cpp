#include "hopper_eeprom.h"

namespace HopperGame {

HopperEepromStore hopperEeprom;

void HopperEepromStore::ensureLoaded() {
    if (loaded) return;
    memset(buffer, 0xFF, sizeof(buffer));

    Preferences prefs;
    if (prefs.begin("hopper_rec", true)) {
        if (prefs.isKey("data")) {
            prefs.getBytes("data", buffer, sizeof(buffer));
        }
        prefs.end();
    }
    loaded = true;
}

uint8_t HopperEepromStore::read(int addr) {
    ensureLoaded();
    if (addr < 0 || addr >= (int)sizeof(buffer)) return 0xFF;
    return buffer[addr];
}

void HopperEepromStore::write(int addr, uint8_t value) {
    ensureLoaded();
    if (addr < 0 || addr >= (int)sizeof(buffer)) return;
    if (buffer[addr] == value) return;
    buffer[addr] = value;
    dirty = true;
    lastWrite = millis();
}

void HopperEepromStore::poll() {
    if (dirty && (millis() - lastWrite) >= 250) {
        commit();
    }
}

void HopperEepromStore::commit() {
    Preferences prefs;
    if (prefs.begin("hopper_rec", false)) {
        prefs.putBytes("data", buffer, sizeof(buffer));
        prefs.end();
    }
    dirty = false;
}

} // namespace HopperGame
