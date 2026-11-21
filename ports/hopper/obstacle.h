#ifndef OBSTACLE_H
#define OBSTACLE_H
#include <Arduboy2.h>
#include "hopper.h"
#include "frog.h"

namespace HopperGame {

class Obstacle {
  public:
    Obstacle(Arduboy2 *arduboy, int x, int y, int w, int s, int type);

    void update();
    void draw();
    boolean intersects(Frog *f);

  private:

    int x;
    int y;
    int w;
    int s;
    int type;
    Arduboy2 *arduboy;
};

} // namespace HopperGame

#endif
