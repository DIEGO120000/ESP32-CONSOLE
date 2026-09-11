#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EEPROM.h"

// =============================================================================
// ESP32-C6 SUPER MINI HARDWARE CONFIGURATION (Arduboy Compatibility Layer)
// =============================================================================
#define ARDUBOY_PIN_SDA        19   // OLED SDA -> GPIO 19
#define ARDUBOY_PIN_SCL        20   // OLED SCL -> GPIO 20

// 4 Physical Buttons (Active LOW with INPUT_PULLUP)
#define ARDUBOY_PIN_BTN1       14   // Button 1 (Left / Up)     -> GPIO 14
#define ARDUBOY_PIN_BTN2       0    // Button 2 (Right / Down)  -> GPIO 0
#define ARDUBOY_PIN_BTN3       7    // Button 3 (Action A / Ok) -> GPIO 7
#define ARDUBOY_PIN_BTN4       18   // Button 4 (Action B / Alt)-> GPIO 18

// BOOT Button (Integrated on-board ESP32-C6)
#define ARDUBOY_PIN_BOOT       9    // BOOT Button -> GPIO 9 (Active LOW, internal pullup)

// Audio Buzzer Output (Pin + del buzzer conectado a GPIO 2)
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

// SSD1306 / SH1106 Display Commands
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
        // 1. Initialize all 4 Hardware Buttons with INPUT_PULLUP
        pinMode(ARDUBOY_PIN_BTN1, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_BTN2, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_BTN3, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_BTN4, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_BOOT, INPUT_PULLUP);

        // 2. Initialize Audio Buzzer on GPIO 2
        pinMode(ARDUBOY_PIN_BUZZER, OUTPUT);
        noTone(ARDUBOY_PIN_BUZZER);

        // 3. Initialize I2C bus at 400kHz (SDA=19, SCL=20)
        Wire.begin(ARDUBOY_PIN_SDA, ARDUBOY_PIN_SCL);
        Wire.setClock(400000);
        delay(50);

        // Auto-detect I2C Address (0x3C or 0x3D)
        Wire.beginTransmission(SSD1306_I2C_ADDR);
        if (Wire.endTransmission() == 0) {
            i2cAddress = SSD1306_I2C_ADDR;
        } else {
            i2cAddress = 0x3D;
        }

        // 4. Initialize OLED Display (Universal Dual SSD1306 / SH1106 Sequence)
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
        Wire.write(0x8D); Wire.write(0x14); // Enable Charge Pump (SSD1306)
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
        // Universal Page-by-Page Rendering (Compatible with 100% of SSD1306, SH1106, SSD1309, SSD1315)
        for (uint8_t page = 0; page < 8; page++) {
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
        // Read 4 hardware buttons (Active LOW with pullups)
        bool b1 = (digitalRead(ARDUBOY_PIN_BTN1) == LOW); // GPIO 14 (Left / Up)
        bool b2 = (digitalRead(ARDUBOY_PIN_BTN2) == LOW); // GPIO 0  (Right / Down)
        bool b3 = (digitalRead(ARDUBOY_PIN_BTN3) == LOW); // GPIO 7  (Action A / Select)
        bool b4 = (digitalRead(ARDUBOY_PIN_BTN4) == LOW); // GPIO 18 (Action B / Back)

        uint8_t btnMask = 0;
        if (b1) btnMask |= (LEFT_BUTTON | UP_BUTTON);
        if (b2) btnMask |= (RIGHT_BUTTON | DOWN_BUTTON);
        if (b3) btnMask |= A_BUTTON;
        if (b4) btnMask |= B_BUTTON;

        return btnMask;
    }

    static void setRGBled(uint8_t red, uint8_t green, uint8_t blue) {}
    static void setRGBled(uint8_t color, uint8_t val) {}
    static void digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue) {}
};
