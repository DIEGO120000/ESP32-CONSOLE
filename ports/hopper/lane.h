#ifndef LANE_H
#define LANE_H
#include <Arduboy2.h>
#include "obstacle.h"

namespace HopperGame {

class Lane {
  public:
    // y, n = number of obstacles, w = width of obstacles,
    // spd = speed of obstacles
    // spc = spacing of obstacles
    // type == CAR or LOG
    Lane(Arduboy2 *arduboy, int y, int n, int w, int spd, int spc, int type);

    // Libera la lista de obstaculos. Anadido para poder reiniciar el cartucho
    // sin fugas de memoria; no altera ninguna mecanica.
    ~Lane();

    void update();
    void draw();

  private:
    int y;
    int n;
    int w;
    int spd;
    int spc;
    int type;

    Obstacle **obstacles;
};

} // namespace HopperGame

#endif
