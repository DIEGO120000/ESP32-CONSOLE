#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define EEPROM_STORAGE_SPACE_START 16

class EEPROM_Arduboy_Class {
private:
    uint8_t buffer[1024];
    bool dirty;
    Preferences prefs;

public:
    EEPROM_Arduboy_Class() : dirty(false) {
        memset(buffer, 0xFF, sizeof(buffer));
    }

    void begin(int size = 0) {
        prefs.begin("arduboy", false);
        if (prefs.isKey("eeprom_data")) {
            prefs.getBytes("eeprom_data", buffer, sizeof(buffer));
        } else {
            memset(buffer, 0xFF, sizeof(buffer));
        }
    }

    uint8_t read(int address) {
        if (address >= 0 && address < (int)sizeof(buffer)) {
            return buffer[address];
        }
        return 0xFF;
    }

    void write(int address, uint8_t value) {
        if (address >= 0 && address < (int)sizeof(buffer)) {
            if (buffer[address] != value) {
                buffer[address] = value;
                dirty = true;
            }
        }
    }

    void update(int address, uint8_t value) {
        write(address, value);
    }

    bool commit() {
        if (dirty) {
            prefs.putBytes("eeprom_data", buffer, sizeof(buffer));
            dirty = false;
            return true;
        }
        return true;
    }

    template<typename T>
    T &get(int address, T &t) {
        if (address >= 0 && (address + sizeof(T)) <= sizeof(buffer)) {
            memcpy(&t, &buffer[address], sizeof(T));
        }
        return t;
    }

    template<typename T>
    const T &put(int address, const T &t) {
        if (address >= 0 && (address + sizeof(T)) <= sizeof(buffer)) {
            if (memcmp(&buffer[address], &t, sizeof(T)) != 0) {
                memcpy(&buffer[address], &t, sizeof(T));
                dirty = true;
                commit();
            }
        }
        return t;
    }
};

inline EEPROM_Arduboy_Class EEPROM;
