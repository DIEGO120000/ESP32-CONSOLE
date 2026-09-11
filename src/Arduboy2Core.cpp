#include "Arduboy2Core.h"

uint8_t Arduboy2Core::sBuffer[WIDTH * HEIGHT / 8] = {0};
uint8_t Arduboy2Core::currentButtonState = 0;
uint8_t Arduboy2Core::previousButtonState = 0;
uint8_t Arduboy2Core::i2cAddress = SSD1306_I2C_ADDR;
