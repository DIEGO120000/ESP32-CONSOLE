#include "Arduboy2Core.h"

uint8_t Arduboy2Core::sBuffer[WIDTH * HEIGHT / 8] = {0};
uint8_t Arduboy2Core::currentButtonState = 0;
uint8_t Arduboy2Core::previousButtonState = 0;
uint8_t Arduboy2Core::i2cAddress = SSD1306_I2C_ADDR;
uint32_t Arduboy2Core::validPullupMask = 0;
float Arduboy2Core::smoothedVoltage = 0.0f;
int Arduboy2Core::smoothedPercentage = 0;
uint32_t Arduboy2Core::lastBatteryUpdateMs = 0;
