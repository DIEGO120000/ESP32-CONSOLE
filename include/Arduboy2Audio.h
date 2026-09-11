#pragma once
#include <Arduino.h>

class Arduboy2Audio {
public:
    static void on() {}
    static void off() {}
    static void toggle() {}
    static bool enabled() { return true; }
    static void saveOnOff() {}
    static void begin() {}
};
