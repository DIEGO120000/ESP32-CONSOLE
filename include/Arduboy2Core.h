#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EEPROM.h"
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#endif

// =============================================================================
// ESP32-C6 HARDWARE CONFIGURATION (Arduboy HAL - OLED SSD1309 2.42" & D-Pad)
// =============================================================================
// I2C OLED Display Communication (SSD1309 / SSD1306 2.42", 128x64 @ 400kHz)
#define ARDUBOY_PIN_SDA        7    // OLED SDA -> GPIO 7
#define ARDUBOY_PIN_SCL        6    // OLED SCL -> GPIO 6

// 5-Way Mechanical Digital D-Pad Pins (Active LOW with INPUT_PULLUP)
#define ARDUBOY_PIN_UP         14   // Dirección UP    -> GPIO 14 (Reasignado)
#define ARDUBOY_PIN_DOWN       18   // Dirección DOWN  -> GPIO 18
#define ARDUBOY_PIN_RIGHT      19   // Dirección RIGHT -> GPIO 19
#define ARDUBOY_PIN_LEFT       5    // Dirección LEFT  -> GPIO 5 (Reasignado, liberado de USB D+)
#define ARDUBOY_PIN_SELECT     20   // Pulsación Central (Click / Enter) -> GPIO 20
#define ARDUBOY_PIN_CLICK      ARDUBOY_PIN_SELECT

// Pin del LED que se desea mantener apagado permanentemente (LED de estado en GPIO 15)
#define LED_PIN_CLAMP_OFF      15

// Aliases for compatibility
#define PIN_UP                 ARDUBOY_PIN_UP
#define PIN_DOWN               ARDUBOY_PIN_DOWN
#define PIN_RIGHT              ARDUBOY_PIN_RIGHT
#define PIN_LEFT               ARDUBOY_PIN_LEFT
#define PIN_SELECT             ARDUBOY_PIN_SELECT
#define PIN_CLICK              ARDUBOY_PIN_SELECT

// Legacy / compatibility aliases
#define ARDUBOY_PIN_BTN1       ARDUBOY_PIN_UP
#define ARDUBOY_PIN_BTN2       ARDUBOY_PIN_DOWN
#define ARDUBOY_PIN_BTN3       ARDUBOY_PIN_LEFT
#define ARDUBOY_PIN_BTN4       ARDUBOY_PIN_RIGHT

// BOOT Button (Integrated on-board ESP32-C6)
#define ARDUBOY_PIN_BOOT       9    // BOOT Button -> GPIO 9 (Active LOW, internal pullup)

// Audio Buzzer Output (Speaker connected to GPIO 2)
#define ARDUBOY_PIN_BUZZER     2    // Speaker/Buzzer -> GPIO 2

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

// Arduboy Button Bitmasks
#define UP_BUTTON              _BV(7) // 0x80
#define RIGHT_BUTTON           _BV(6) // 0x40
#define LEFT_BUTTON            _BV(5) // 0x20
#define DOWN_BUTTON            _BV(4) // 0x10
#define A_BUTTON               _BV(3) // 0x08
#define B_BUTTON               _BV(2) // 0x04

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
    static void init() {
        boot();
    }

    static void initPins() {
        #if defined(ESP32)
        // Desvincular el pad 13 del controlador USB-JTAG por seguridad
        gpio_reset_pin(GPIO_NUM_13);
        PIN_FUNC_SELECT(IO_MUX_GPIO13_REG, PIN_FUNC_GPIO);

        #if defined(USB_SERIAL_JTAG_CONF0_REG) && defined(USB_SERIAL_JTAG_USB_PAD_ENABLE)
        CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
        #endif

        // Configurar GPIO 13 desvinculado con pull-up por seguridad
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << GPIO_NUM_13);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_UP);
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_DOWN);
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_RIGHT);
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_LEFT);
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_SELECT);
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_BOOT);

        // Desactivar y apagar permanentemente el LED en GPIO 25
        gpio_reset_pin((gpio_num_t)LED_PIN_CLAMP_OFF);
        gpio_set_direction((gpio_num_t)LED_PIN_CLAMP_OFF, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)LED_PIN_CLAMP_OFF, 0);
        #endif

        pinMode(13, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_UP, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_DOWN, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_RIGHT, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_LEFT, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_SELECT, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_BOOT, INPUT_PULLUP);

        // Apagar LED en GPIO 25
        pinMode(LED_PIN_CLAMP_OFF, OUTPUT);
        digitalWrite(LED_PIN_CLAMP_OFF, LOW);
    }

    static void boot() {
        // 1. Explicit unbinding & isolation of GPIO 13 and all D-Pad pins + BOOT
        initPins();

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

        // 3. Initialize I2C bus at 400kHz using EXCLUSIVELY GPIO 7 (SDA) and GPIO 6 (SCL)
        Wire.begin(7, 6);
        Wire.setClock(400000UL);
        delay(50);

        // Post-I2C pin safety re-assertion for D-Pad
        #if defined(ESP32)
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_UP);
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_LEFT);
        #endif
        pinMode(ARDUBOY_PIN_UP, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_LEFT, INPUT_PULLUP);

        pinMode(LED_PIN_CLAMP_OFF, OUTPUT);
        digitalWrite(LED_PIN_CLAMP_OFF, LOW);

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
        uint8_t raw = 0;
        #if defined(ESP32)
        if (digitalRead(ARDUBOY_PIN_UP) == LOW)     raw |= UP_BUTTON;    // GPIO 14 (UP)
        if (digitalRead(ARDUBOY_PIN_DOWN) == LOW)   raw |= DOWN_BUTTON;  // GPIO 18 (DOWN)
        if (digitalRead(ARDUBOY_PIN_RIGHT) == LOW)  raw |= RIGHT_BUTTON; // GPIO 19 (RIGHT)
        if (digitalRead(ARDUBOY_PIN_LEFT) == LOW)   raw |= LEFT_BUTTON;  // GPIO 5 (LEFT)
        if (digitalRead(ARDUBOY_PIN_SELECT) == LOW) raw |= A_BUTTON;     // GPIO 20 (A / CLICK)
        #else
        if (digitalRead(ARDUBOY_PIN_UP) == LOW)     raw |= UP_BUTTON;
        if (digitalRead(ARDUBOY_PIN_DOWN) == LOW)   raw |= DOWN_BUTTON;
        if (digitalRead(ARDUBOY_PIN_RIGHT) == LOW)  raw |= RIGHT_BUTTON;
        if (digitalRead(ARDUBOY_PIN_LEFT) == LOW)   raw |= LEFT_BUTTON;
        if (digitalRead(ARDUBOY_PIN_SELECT) == LOW) raw |= A_BUTTON;
        #endif
        return raw;
    }

    static void setRGBled(uint8_t red, uint8_t green, uint8_t blue) {}
    static void setRGBled(uint8_t color, uint8_t val) {}
    static void digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue) {}
};
