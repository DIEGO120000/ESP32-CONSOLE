#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EEPROM.h"
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#endif

// =============================================================================
// ESP32-C6 HARDWARE CONFIGURATION (Arduboy HAL - OLED SSD1309 2.42" & D-Pad)
// =============================================================================
// I2C OLED Display Communication (SSD1309 / SSD1306 2.42", 128x64 @ 400kHz)
#define ARDUBOY_PIN_SDA        7    // OLED SDA -> GPIO 7
#define ARDUBOY_PIN_SCL        6    // OLED SCL -> GPIO 6

// 5-Way Mechanical Digital D-Pad Pins (Active LOW with INPUT_PULLUP)
#define PIN_UP                 20   // Dirección 1 (UP)    -> GPIO 20
#define PIN_DOWN               19   // Dirección 2 (DOWN)  -> GPIO 19
#define PIN_LEFT               18   // Dirección 3 (LEFT)  -> GPIO 18
#define PIN_RIGHT              15   // Dirección 4 (RIGHT) -> GPIO 15
#define PIN_CLICK              13   // Pulsación Central (CLICK) -> GPIO 13

// Centralized Pin Constants
#define ARDUBOY_PIN_UP         PIN_UP
#define ARDUBOY_PIN_DOWN       PIN_DOWN
#define ARDUBOY_PIN_LEFT       PIN_LEFT
#define ARDUBOY_PIN_RIGHT      PIN_RIGHT
#define ARDUBOY_PIN_CLICK      PIN_CLICK

// Legacy / compatibility aliases
#define ARDUBOY_PIN_BTN1       ARDUBOY_PIN_UP
#define ARDUBOY_PIN_BTN2       ARDUBOY_PIN_DOWN
#define ARDUBOY_PIN_BTN3       ARDUBOY_PIN_LEFT
#define ARDUBOY_PIN_BTN4       ARDUBOY_PIN_RIGHT

// BOOT Button (Integrated on-board ESP32-C6)
#define ARDUBOY_PIN_BOOT       9    // BOOT Button -> GPIO 9 (Active LOW, internal pullup)

// Audio Buzzer Output (Speaker connected to GPIO 2)
#define ARDUBOY_PIN_BUZZER     2    // Speaker/Buzzer -> GPIO 2

// Arduboy Button Bitmasks
#define LEFT_BUTTON            0x20
#define RIGHT_BUTTON           0x40
#define UP_BUTTON              0x80
#define DOWN_BUTTON            0x10
#define A_BUTTON               0x08
#define B_BUTTON               0x04

#define WIDTH                  128
#define HEIGHT                 64

#define INVERT                 2

#undef WHITE
#undef BLACK
#define WHITE                  1
#define BLACK                  0

// SSD1306 / SSD1309 Display Commands
#define SSD1306_I2C_ADDR       0x3C
#define SSD1306_DISPLAYOFF     0xAE
#define SSD1306_DISPLAYON      0xAF

class Arduboy2Core {
public:
    static uint8_t sBuffer[WIDTH * HEIGHT / 8];
    static uint8_t currentButtonState;
    static uint8_t previousButtonState;
    static uint8_t i2cAddress;

public:
    static void boot() {
        // 1. Initialize all 5 D-Pad Directions and BOOT Button with INPUT_PULLUP
        pinMode(ARDUBOY_PIN_UP, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_DOWN, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_LEFT, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_RIGHT, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_CLICK, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_BOOT, INPUT_PULLUP);

        // 2. Initialize Audio Buzzer on GPIO 2 (Full isolation & pulldown clamp)
        #if defined(ESP32)
        ledcDetach(ARDUBOY_PIN_BUZZER);
        esp_rom_gpio_pad_select_gpio((uint32_t)ARDUBOY_PIN_BUZZER);
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_BUZZER);
        gpio_set_direction((gpio_num_t)ARDUBOY_PIN_BUZZER, GPIO_MODE_OUTPUT);
        gpio_pullup_dis((gpio_num_t)ARDUBOY_PIN_BUZZER);
        gpio_pulldown_en((gpio_num_t)ARDUBOY_PIN_BUZZER);
        gpio_set_level((gpio_num_t)ARDUBOY_PIN_BUZZER, 0);
        #else
        pinMode(ARDUBOY_PIN_BUZZER, OUTPUT);
        digitalWrite(ARDUBOY_PIN_BUZZER, LOW);
        #endif

        // 3. Initialize I2C bus at 400kHz (SDA=7, SCL=6)
        Wire.begin(ARDUBOY_PIN_SDA, ARDUBOY_PIN_SCL);
        Wire.setClock(400000UL);
        delay(50);

        // Auto-detect I2C Address (0x3C or 0x3D)
        Wire.beginTransmission(SSD1306_I2C_ADDR);
        if (Wire.endTransmission() == 0) {
            i2cAddress = SSD1306_I2C_ADDR;
        } else {
            i2cAddress = 0x3D;
        }

        // 4. Initialize OLED Display (Universal Dual SSD1306 / SSD1309 / SH1106 Sequence)
        initDisplay();

        // 5. Initialize EEPROM persistence
        EEPROM.begin();

        clear();
        display();
    }

    static void sendCommand(uint8_t c) {
        Wire.beginTransmission(i2cAddress);
        Wire.write(0x00);
        Wire.write(c);
        Wire.endTransmission();
    }

    static void initDisplay() {
        Wire.beginTransmission(i2cAddress);
        Wire.write(0x00);
        Wire.write(0xAE); // Display OFF
        Wire.write(0xD5); Wire.write(0x80); // Display Clock Divide
        Wire.write(0xA8); Wire.write(0x3F); // Multiplex 64
        Wire.write(0xD3); Wire.write(0x00); // Display Offset 0
        Wire.write(0x40);                   // Start Line 0
        Wire.write(0x8D); Wire.write(0x14); // Enable Charge Pump (SSD1306/SSD1309)
        Wire.write(0xAD); Wire.write(0x8B); // Enable DC-DC Converter (SH1106)
        Wire.write(0xA1);                   // Segment Remap (Column 127 = SEG0)
        Wire.write(0xC8);                   // COM Output Scan Decrement
        Wire.write(0xDA); Wire.write(0x12); // COM Pins hardware config
        Wire.write(0x81); Wire.write(0xCF); // Contrast Control
        Wire.write(0xD9); Wire.write(0xF1); // Pre-charge Period
        Wire.write(0xDB); Wire.write(0x40); // VCOMH Deselect Level
        Wire.write(0xA4);                   // Entire Display ON from RAM
        Wire.write(0xA6);                   // Normal Display
        Wire.write(0xAF);                   // Display ON
        Wire.endTransmission();
        delay(30);
    }

    static void display() {
        // Universal Page-by-Page Rendering (Compatible with SSD1306, SSD1309, SH1106, SSD1315)
        for (uint8_t page = 0; page < 8; page++) {
            yield();
            Wire.beginTransmission(i2cAddress);
            Wire.write(0x00);
            Wire.write(0xB0 + page); // Set page address (0 to 7)
            Wire.write(0x00);        // Set lower column address 0
            Wire.write(0x10);        // Set higher column address 0
            Wire.endTransmission();

            const uint8_t *pageData = &sBuffer[page * WIDTH];
            for (uint8_t c = 0; c < 128; c += 32) {
                Wire.beginTransmission(i2cAddress);
                Wire.write(0x40); // Data stream
                Wire.write(pageData + c, 32);
                Wire.endTransmission();
            }
        }
        yield();
    }

    static void clear() {
        memset(sBuffer, 0, sizeof(sBuffer));
    }

    static void fillScreen(uint8_t color) {
        memset(sBuffer, color ? 0xFF : 0x00, sizeof(sBuffer));
    }

    static uint8_t* getBuffer() {
        return sBuffer;
    }

    static uint8_t buttonsState() {
        // Read 5-Way D-Pad (Active LOW with pullups) with temporal debounce filter
        static uint8_t debouncedState = 0;
        static uint8_t lastRawState = 0xFF;
        static uint32_t lastDebounceTime = 0;

        uint8_t raw = 0;
        if (digitalRead(ARDUBOY_PIN_UP) == LOW)    raw |= UP_BUTTON;
        if (digitalRead(ARDUBOY_PIN_DOWN) == LOW)  raw |= DOWN_BUTTON;
        if (digitalRead(ARDUBOY_PIN_LEFT) == LOW)  raw |= LEFT_BUTTON;
        if (digitalRead(ARDUBOY_PIN_RIGHT) == LOW) raw |= RIGHT_BUTTON;
        if (digitalRead(ARDUBOY_PIN_CLICK) == LOW) raw |= (A_BUTTON | B_BUTTON); // Universal OK / Confirm

        uint32_t now = millis();
        if (raw != lastRawState) {
            lastRawState = raw;
            lastDebounceTime = now;
        }

        if ((now - lastDebounceTime) >= 12) { // 12ms debounce window
            debouncedState = raw;
        }

        return debouncedState;
    }

    static void setRGBled(uint8_t red, uint8_t green, uint8_t blue) {}
    static void setRGBled(uint8_t color, uint8_t val) {}
    static void digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue) {}
};
