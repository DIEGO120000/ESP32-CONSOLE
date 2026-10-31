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
#define ARDUBOY_PIN_UP         14   // Dirección UP    -> GPIO 14
#define ARDUBOY_PIN_DOWN       18   // Dirección DOWN  -> GPIO 18
#define ARDUBOY_PIN_RIGHT      19   // Dirección RIGHT -> GPIO 19
#define ARDUBOY_PIN_LEFT       5    // Dirección LEFT  -> GPIO 5 (Cruceta)
#define ARDUBOY_PIN_SELECT     20   // Click Central de la cruceta (Menú Ajustes) -> GPIO 20
#define PIN_NAV_CENTER         ARDUBOY_PIN_SELECT
#define ARDUBOY_PIN_CLICK      ARDUBOY_PIN_SELECT

// NES Style Action Buttons
#define PIN_BUTTON_A           3    // Botón A Físico (Soldado a GPIO 3) -> Select / OK / Confirmar
#define PIN_BUTTON_B           4    // Botón B Físico (Soldado a GPIO 4) -> Inactivo / Sin función por ahora
#define ARDUBOY_PIN_A          PIN_BUTTON_A
#define ARDUBOY_PIN_B          PIN_BUTTON_B

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

// Audio Buzzer Output Disabled (No buzzer connected)
#define ARDUBOY_PIN_BUZZER     -1

// Battery Telemetry ADC Pin (GPIO 2 / ADC1_CH2, 1:2 Divider 100k/100k)
#define ARDUBOY_PIN_BATTERY    2

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
#define CLICK_BUTTON           _BV(1) // 0x02 (Click Central de la cruceta - GPIO 20)
#define SYSTEM_MENU_BUTTON     CLICK_BUTTON
#define SYS_BUTTON             CLICK_BUTTON

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

    static uint32_t validPullupMask;
    static float smoothedVoltage;
    static int smoothedPercentage;
    static uint32_t lastBatteryUpdateMs;

    static void calibrateButtonPins() {
        const uint8_t candidatePins[] = {
            ARDUBOY_PIN_UP, ARDUBOY_PIN_DOWN, ARDUBOY_PIN_RIGHT, ARDUBOY_PIN_LEFT,
            PIN_BUTTON_A, PIN_BUTTON_B, PIN_NAV_CENTER, ARDUBOY_PIN_BOOT
        };
        for (uint8_t p : candidatePins) {
            gpio_reset_pin((gpio_num_t)p);
            pinMode(p, INPUT_PULLUP);
        }
        delay(15);
        validPullupMask = 0;
        for (uint8_t p : candidatePins) {
            if (digitalRead(p) == HIGH) {
                validPullupMask |= (1UL << p);
            }
        }
    }

    static void initPins() {
        #if defined(ESP32)
        // Desvincular periféricos PWM previos en pines de botones
        ledcDetach(2);
        ledcDetach(3);
        ledcDetach(4);
        #endif

        const uint8_t primaryPins[] = {
            PIN_BUTTON_A, PIN_BUTTON_B,
            ARDUBOY_PIN_UP, ARDUBOY_PIN_DOWN, ARDUBOY_PIN_RIGHT, ARDUBOY_PIN_LEFT,
            PIN_NAV_CENTER, ARDUBOY_PIN_BOOT
        };
        for (uint8_t p : primaryPins) {
            gpio_reset_pin((gpio_num_t)p);
            pinMode(p, INPUT_PULLUP);
        }

        calibrateButtonPins();
    }

    static void boot() {
        // 1. Explicit unbinding & isolation of GPIO 13, GPIO 2, GPIO 3 and all buttons + BOOT
        initPins();

        // 2. Initialize Battery Telemetry ADC (GPIO 1 / ADC1_CH1)
        initBattery();

        // 3. Initialize I2C bus at 400kHz using EXCLUSIVELY GPIO 7 (SDA) and GPIO 6 (SCL)
        Wire.begin(7, 6);
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

    static void setContrast(uint8_t contrast) {
        Wire.beginTransmission(i2cAddress);
        Wire.write(0x00);
        Wire.write(0x81); // Set Contrast Control (SSD1306 / SSD1309)
        Wire.write(contrast);
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

        // UP: GPIO 14
        if (digitalRead(ARDUBOY_PIN_UP) == LOW) raw |= UP_BUTTON;

        // DOWN: GPIO 18
        if (digitalRead(ARDUBOY_PIN_DOWN) == LOW) raw |= DOWN_BUTTON;

        // LEFT: GPIO 5
        if (digitalRead(ARDUBOY_PIN_LEFT) == LOW) raw |= LEFT_BUTTON;

        // RIGHT: GPIO 19
        if (digitalRead(ARDUBOY_PIN_RIGHT) == LOW) raw |= RIGHT_BUTTON;

        // ACTION A: Botón A físico (GPIO 3) -> Seleccionar / Confirmar / Acción
        if (digitalRead(PIN_BUTTON_A) == LOW) raw |= A_BUTTON;

        // ACTION B: Botón B físico (GPIO 4) -> Disminuir en Ajustes / Acción B
        if (digitalRead(PIN_BUTTON_B) == LOW) raw |= B_BUTTON;

        // CLICK CENTRAL CRUCETA (GPIO 20): Click Central / Pausa en juegos
        if (digitalRead(PIN_NAV_CENTER) == LOW) raw |= CLICK_BUTTON;

        return raw;
    }

    static void setRGBled(uint8_t red, uint8_t green, uint8_t blue) {}
    static void setRGBled(uint8_t color, uint8_t val) {}
    static void digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue) {}

    // Battery Telemetry Subsystem (GPIO 2 - ADC1_CH2, 100k/100k Divider)
    static void updateBatteryTelemetry(bool forceUpdate = false) {
        uint32_t now = millis();
        // Frecuencia de muestreo controlada (cada 400ms para evitar parpadeos)
        if (!forceUpdate && lastBatteryUpdateMs != 0 && (now - lastBatteryUpdateMs < 400)) {
            return;
        }
        lastBatteryUpdateMs = now;

        // Muestreo con rechazo de valores atípicos (32 muestras)
        const int N = 32;
        uint32_t samples[N];
        for (int i = 0; i < N; i++) {
            uint32_t mv = analogReadMilliVolts(ARDUBOY_PIN_BATTERY);
            if (mv == 0) {
                mv = ((uint32_t)analogRead(ARDUBOY_PIN_BATTERY) * 3100UL) / 4095UL;
            }
            samples[i] = mv;
            delayMicroseconds(60);
        }

        // Ordenamiento por inserción para descartar picos de ruido (Wi-Fi/I2C)
        for (int i = 1; i < N; i++) {
            uint32_t key = samples[i];
            int j = i - 1;
            while (j >= 0 && samples[j] > key) {
                samples[j + 1] = samples[j];
                j--;
            }
            samples[j + 1] = key;
        }

        // Promediar el 50% central (descartar los 8 más bajos y 8 más altos)
        uint32_t sum = 0;
        for (int i = 8; i < 24; i++) {
            sum += samples[i];
        }
        float pinMv = (float)sum / 16.0f;
        float currentVoltage = (pinMv * 2.0f) / 1000.0f;

        // Filtro Exponencial (EMA): 85% retención, 15% nueva muestra para máxima estabilidad
        if (smoothedVoltage <= 0.5f || forceUpdate) {
            smoothedVoltage = currentVoltage;
        } else {
            smoothedVoltage = (smoothedVoltage * 0.85f) + (currentVoltage * 0.15f);
        }

        // Curva Li-Ion quimica multi-tramo (4.18V = 100%, 3.30V = 0%)
        int targetPct = 0;
        if (smoothedVoltage >= 4.18f) {
            targetPct = 100;
        } else if (smoothedVoltage <= 3.30f) {
            targetPct = 0;
        } else if (smoothedVoltage >= 4.00f) {
            targetPct = (int)(85.0f + ((smoothedVoltage - 4.00f) / (4.18f - 4.00f)) * 15.0f);
        } else if (smoothedVoltage >= 3.82f) {
            targetPct = (int)(60.0f + ((smoothedVoltage - 3.82f) / (4.00f - 3.82f)) * 25.0f);
        } else if (smoothedVoltage >= 3.68f) {
            targetPct = (int)(30.0f + ((smoothedVoltage - 3.68f) / (3.82f - 3.68f)) * 30.0f);
        } else if (smoothedVoltage >= 3.50f) {
            targetPct = (int)(10.0f + ((smoothedVoltage - 3.50f) / (3.68f - 3.50f)) * 20.0f);
        } else {
            targetPct = (int)(((smoothedVoltage - 3.30f) / (3.50f - 3.30f)) * 10.0f);
        }
        if (targetPct < 0) targetPct = 0;
        if (targetPct > 100) targetPct = 100;

        // Histéresis: No cambiar porcentaje si la variación es solo de 1% (evita oscilación)
        if (smoothedPercentage == 0 || forceUpdate) {
            smoothedPercentage = targetPct;
        } else {
            if (abs(targetPct - smoothedPercentage) >= 2) {
                if (targetPct > smoothedPercentage) {
                    smoothedPercentage++;
                } else {
                    smoothedPercentage--;
                }
            }
        }
    }

    static void initBattery() {
        #if defined(ESP32)
        gpio_reset_pin((gpio_num_t)ARDUBOY_PIN_BATTERY);
        pinMode(ARDUBOY_PIN_BATTERY, ANALOG);
        analogSetPinAttenuation(ARDUBOY_PIN_BATTERY, ADC_11db);
        #endif
        // Pre-cargar el filtro al encendido para que el primer valor sea inmediato y exacto
        for (int i = 0; i < 4; i++) {
            updateBatteryTelemetry(true);
            delay(10);
        }
    }

    static uint32_t getBatteryRawMilliVolts() {
        updateBatteryTelemetry(false);
        return (uint32_t)((smoothedVoltage * 1000.0f) / 2.0f);
    }

    static float getBatteryVoltage() {
        updateBatteryTelemetry(false);
        return smoothedVoltage;
    }

    static int getBatteryPercentage() {
        updateBatteryTelemetry(false);
        return smoothedPercentage;
    }
};
