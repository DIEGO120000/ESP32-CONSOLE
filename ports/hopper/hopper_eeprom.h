#pragma once

#include <Arduino.h>
#include <Preferences.h>

// =============================================================================
// EEPROM AISLADA DE HOPPER
//
// title.cpp original usa la API eeprom_* de AVR sobre la direccion 800. Para no
// compartir el blob "arduboy" del resto de cartuchos, este shim guarda sus
// 1024 bytes en la NVS con namespace propio ("hopper_rec"). El juego sigue
// leyendo/escribiendo las mismas direcciones y el mismo checksum.
//
// Las escrituras se agrupan: el juego escribe ~20 bytes al terminar partida y
// se confirman 250 ms despues, no en cada byte.
//
// El commit real (putBytes de 1 KB a NVS) se aplaza ademas hasta que la
// partida activa termina: escribir la flash durante el juego es un pico de
// corriente que puede tumbar el regulador. setCommitAllowed(false) mantiene el
// record solo en RAM; flush() lo fuerza en los cambios de pantalla.
// =============================================================================
namespace HopperGame {

class HopperEepromStore {
public:
    uint8_t read(int addr);
    void write(int addr, uint8_t value);
    void poll();
    // Habilita/deshabilita el volcado a flash de poll().
    void setCommitAllowed(bool allowed);
    // Confirma en flash lo pendiente, sin esperar los 250 ms.
    void flush();

private:
    void ensureLoaded();
    void commit();

    uint8_t buffer[1024];
    bool loaded = false;
    bool dirty = false;
    bool commitAllowed = true;
    uint32_t lastWrite = 0;
};

extern HopperEepromStore hopperEeprom;

// --- API compatible con avr/eeprom.h usada por title.cpp ---------------------
inline void eeprom_busy_wait() {}

inline uint8_t eeprom_read_byte(const uint8_t *p) {
    return hopperEeprom.read((int)(uintptr_t)p);
}

inline uint16_t eeprom_read_word(const uint16_t *p) {
    int a = (int)(uintptr_t)p;
    return (uint16_t)hopperEeprom.read(a) |
           ((uint16_t)hopperEeprom.read(a + 1) << 8);
}

inline uint32_t eeprom_read_dword(const uint32_t *p) {
    int a = (int)(uintptr_t)p;
    return (uint32_t)hopperEeprom.read(a) |
           ((uint32_t)hopperEeprom.read(a + 1) << 8) |
           ((uint32_t)hopperEeprom.read(a + 2) << 16) |
           ((uint32_t)hopperEeprom.read(a + 3) << 24);
}

inline void eeprom_read_block(void *dst, const void *src, size_t n) {
    int a = (int)(uintptr_t)src;
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) d[i] = hopperEeprom.read(a + (int)i);
}

inline void eeprom_write_byte(uint8_t *p, uint8_t value) {
    hopperEeprom.write((int)(uintptr_t)p, value);
}

inline void eeprom_write_word(uint16_t *p, uint16_t value) {
    int a = (int)(uintptr_t)p;
    hopperEeprom.write(a, (uint8_t)(value & 0xFF));
    hopperEeprom.write(a + 1, (uint8_t)(value >> 8));
}

inline void eeprom_write_dword(uint32_t *p, uint32_t value) {
    int a = (int)(uintptr_t)p;
    for (int i = 0; i < 4; i++) {
        hopperEeprom.write(a + i, (uint8_t)(value >> (8 * i)));
    }
}

inline void eeprom_write_block(const void *src, void *dst, size_t n) {
    int a = (int)(uintptr_t)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) hopperEeprom.write(a + (int)i, s[i]);
}

} // namespace HopperGame
