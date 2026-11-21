#include "hopper_game.h"

#include "hopper.h"
#include "frog.h"
#include "obstacle.h"
#include "lane.h"
#include "console_core.h"

// =============================================================================
// Adaptador del cartucho Hopper. Los objetos del mundo se crean y destruyen en
// setup(), con los mismos parametros de los globales del Hopper.ino original:
//
//   Frog frog(&arduboy, &sound, 32, 58);
//   Lane lane1(&arduboy, 47, 3, 20,  1, 60, CAR);
//   Lane lane2(&arduboy, 39, 4, 15, -2, 40, CAR);
//   Lane lane3(&arduboy, 31, 3, 30,  1, 40, CAR);
//   Lane lane4(&arduboy, 15, 3, 20,  1, 70, LOG);
//   Lane lane5(&arduboy,  7, 3, 30, -1, 50, LOG);
//
// La consola (main.cpp) ya inicializa la HAL y dibuja el framebuffer, por lo
// que setup() no llama a arduboy.begin() ni loop() a arduboy.display().
// =============================================================================
namespace HopperGame {

static Frog *frog = nullptr;
static Lane *lanes[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

void setup() {
  delete frog;
  frog = nullptr;
  for (uint8_t i = 0; i < 5; i++) {
    delete lanes[i];
    lanes[i] = nullptr;
  }

  frog = new Frog(&arduboy, &sound, 32, 58);
  lanes[0] = new Lane(&arduboy, 47, 3, 20,  1, 60, CAR);
  lanes[1] = new Lane(&arduboy, 39, 4, 15, -2, 40, CAR);
  lanes[2] = new Lane(&arduboy, 31, 3, 30,  1, 40, CAR);
  lanes[3] = new Lane(&arduboy, 15, 3, 20,  1, 70, LOG);
  lanes[4] = new Lane(&arduboy,  7, 3, 30, -1, 50, LOG);
}

void update() {
  frog->update();
  for (uint8_t i = 0; i < 5; i++) lanes[i]->update();
}

static void drawSafetyZones() {
  // Zones start at y = 24 and y = 56
  arduboy.drawFastHLine(0, 23, 128, WHITE);
  arduboy.drawFastHLine(0, 29, 128, WHITE);
  arduboy.drawFastHLine(0, 55, 128, WHITE);
}

void draw() {
  arduboy.clear();

  drawSafetyZones();
  frog->draw();

  for (uint8_t i = 0; i < 5; i++) lanes[i]->draw();
}

// Equivalente exacto al cuerpo del loop() original (sin nextFrame/display,
// que pertenecen al bucle de la consola).
void loop() {
  update();
  draw();
}

} // namespace HopperGame

// -----------------------------------------------------------------------------
// Interfaz del cartucho
// -----------------------------------------------------------------------------
void HopperPort::init() {
  HopperGame::setup();
  frameDivider = 0;
}

void HopperPort::update() {
  // El original trabaja a 30 FPS (arduboy.setFrameRate(30)); el bucle de la
  // consola corre a 60 FPS. Actualizar un frame de cada dos conserva exactamente
  // la velocidad de los coches, troncos y saltos de la rana.
  if ((frameDivider++ & 0x01) != 0) return;
  HopperGame::update();
}

void HopperPort::draw() {
  HopperGame::draw();
}
