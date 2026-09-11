#pragma once
#include <Arduino.h>

class ProyectoBLEClass {
public:
    void init();
    void update();
    void draw();
    void stop();
    bool isActive() const { return running; }

private:
    bool running = false;
};

extern ProyectoBLEClass proyectoBLE;
