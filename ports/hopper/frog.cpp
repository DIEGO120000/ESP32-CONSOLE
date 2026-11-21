#include "frog.h"
#include "hopper.h"

namespace HopperGame {

Frog::Frog(Arduboy2* b, ArduboyTones* s, int x, int y): arduboy(b), sound(s), x(x), y(y) {}

// Needs to be called at the start of the loop function
void Frog::updateButtonState() {
  previousButtonState = currentButtonState;
  currentButtonState = arduboy->buttonsState();
}

// Returns if a button was just pressed
bool Frog::buttonJustPressed(uint8_t button) {
  if (!(previousButtonState & button) && (currentButtonState & button)) {
    return true;
  }
  return false;
}

void Frog::update() {
  updateButtonState();

  int moved = 0;

  if (buttonJustPressed(RIGHT_BUTTON) && (x < WIDTH)) {
    x += LANESIZE;
    moved = 1;
  }

  if (buttonJustPressed(LEFT_BUTTON) && (x > 0)) {
    x -= LANESIZE;
    moved = 1;
  }

  if (buttonJustPressed(UP_BUTTON) && (y > 0)) {
    y -= LANESIZE;
    moved = 1;
  }

  if (buttonJustPressed(DOWN_BUTTON) && ((y + FROGSIZE*2) < HEIGHT)) {
    y += LANESIZE;
    moved = 1;
  }

  if (moved > 0) {
    sound->tone(1000, 100);
  }
}

int Frog::getX() {
  return x;
}

int Frog::getY() {
  return y;
}

int Frog::getSize() {
  return FROGSIZE*2;
}

void Frog::draw() {
  arduboy->fillCircle(x, y, FROGSIZE, WHITE);
}

} // namespace HopperGame
