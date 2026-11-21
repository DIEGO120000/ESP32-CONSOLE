#include "lane.h"

namespace HopperGame {

Lane::Lane(Arduboy2* b, int y, int n, int w, int spd, int spc, int type): y(y), n(n), w(w), spd(spd), spc(spc), type(type) {
  obstacles = new Obstacle *[n];
  for (int i=0; i<n; i++) {
    int x = 0 + (i * spc) + w;
;    obstacles[i] = new Obstacle(b, x, y, w, spd, type);
  }
}

Lane::~Lane() {
  for (int i=0; i<n; i++) {
    delete obstacles[i];
  }
  delete[] obstacles;
}

void Lane::update() {
  for (int i=0; i<n; i++) {
    obstacles[i]->update();
  }
}

void Lane::draw() {
  for (int i=0; i<n; i++) {
    obstacles[i]->draw();
  }
}

} // namespace HopperGame
