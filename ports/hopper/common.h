#ifndef COMMON_H
#define COMMON_H

#include "MyArduboy.h"

// El codigo original se apoya en los macros min/max de AVR, que aceptan
// tipos mezclados. El core ESP32 usa plantillas de std::min/max y rechaza
// esas comparaciones (int8_t/int, uint8_t/int). Se restauran los macros.
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

namespace HopperGame {

/*  Defines  */

//#define DEBUG
#define APP_TITLE       "HOPPER"
#define APP_CODE        "OBN-Y02"
#define APP_VERSION     "0.22"
#define APP_RELEASED    "NOVEMBER 2017"

/*  Typedefs  */

typedef unsigned char   uchar;
typedef unsigned int    uint;

/*  Global Functions  */

void initLogo(void);
bool updateLogo(void);
void drawLogo(void);

void initTitle(void);
bool updateTitle(void);
void drawTitle(void);
uint8_t setLastScore(int score, uint32_t time);

void initGame(void);
bool updateGame(void);
void drawGame(void);
// true cuando la partida ya termino (pantalla de Game Over): unico momento en
// que el despachador permite confirmar el record en flash.
bool isGameOver(void);

/*  Global Variables  */

extern MyArduboy arduboy;
extern bool exitRequested;

/*  For Debugging  */

#ifdef DEBUG
extern bool             dbgPrintEnabled;
extern char             dbgRecvChar;
#define dprint(...)     (!dbgPrintEnabled || Serial.print(__VA_ARGS__))
#define dprintln(...)   (!dbgPrintEnabled || Serial.println(__VA_ARGS__))
#else
#define dprint(...)
#define dprintln(...)
#endif

} // namespace HopperGame

#endif // COMMON_H
