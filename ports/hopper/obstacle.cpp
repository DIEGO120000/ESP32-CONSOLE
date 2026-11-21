#include "obstacle.h"

#define OBSTACLE_HEIGHT 6

namespace HopperGame {

Obstacle::Obstacle(Arduboy2* b, int x, int y, int w, int s, int type): arduboy(b), x(x), y(y), w(w), s(s), type(type) {}

void Obstacle::update() {
  x = x + s;
  if (x > WIDTH + w) {
    x = 0 - w;
  }
  if (x < 0 - w) {
    x = WIDTH + w;
  }
}

// Check intersection with a Frog object.
boolean Obstacle::intersects(Frog *f) {
  return !(
    x + w                <= f->getX()                 ||
    x                    >= f->getX() + f->getSize()  ||
    y + OBSTACLE_HEIGHT  <= f->getY()                 ||
    y                    >= f->getY() + f->getSize()
  );
}

void Obstacle::draw() {
  if (type == CAR) {
    arduboy->fillRect(x, y, w, OBSTACLE_HEIGHT, WHITE);
  } else {
    arduboy->drawRect(x, y, w, OBSTACLE_HEIGHT, WHITE);
  }
}

} // namespace HopperGame
