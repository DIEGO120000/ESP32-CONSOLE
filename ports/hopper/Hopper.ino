#include <Arduboy2.h>
#include <ArduboyTones.h>
#include "hopper.h"
#include "frog.h"
#include "obstacle.h"
#include "lane.h"

Arduboy2 arduboy;
ArduboyTones sound(arduboy.audio.enabled);

Frog frog(&arduboy, &sound, 32, 58);
Lane lane1(&arduboy, 47, 3, 20, 1, 60, CAR);
Lane lane2(&arduboy, 39, 4, 15, -2, 40, CAR);
Lane lane3(&arduboy, 31, 3, 30, 1, 40, CAR);
Lane lane4(&arduboy, 15, 3, 20, 1, 70, LOG);
Lane lane5(&arduboy,  7, 3, 30, -1, 50, LOG);


void setup() {
  arduboy.begin();
  arduboy.setFrameRate(30);
}

void drawSafetyZones() {
  // Zones start at y = 24 and y = 56
  arduboy.drawFastHLine(0, 23, 128, WHITE);
  arduboy.drawFastHLine(0, 29, 128, WHITE);
  arduboy.drawFastHLine(0, 55, 128, WHITE);
}

void loop() {
  // pause render until it's time for the next frame
  if (!(arduboy.nextFrame()))
    return;
  frog.update();
  lane1.update();
  lane2.update();
  lane3.update();
  lane4.update();
  lane5.update();

  arduboy.clear();

  drawSafetyZones();
  frog.draw();

  lane1.draw();
  lane2.draw();
  lane3.draw();
  lane4.draw();
  lane5.draw();

  arduboy.display();
}
