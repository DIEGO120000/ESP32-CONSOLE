#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EEPROM.h"

// Hardware Pin Configuration for ESP32-C3 Super Mini
#define ARDUBOY_PIN_SDA        3
#define ARDUBOY_PIN_SCL        4

#define ARDUBOY_PIN_BTN1       2    // Left (or Up in combos)
#define ARDUBOY_PIN_BTN2       1    // Right (or Down in combos)
#define ARDUBOY_PIN_BTN3       0    // Button A (Action)
#define ARDUBOY_PIN_BTN4       5    // Button B (Menu)

#define OLED_ADDR              0x3C
#define SCREEN_WIDTH           128
#define SCREEN_HEIGHT          64
#define SCREEN_BUFFER_SIZE     (SCREEN_WIDTH * SCREEN_HEIGHT / 8) // 1024 bytes

// ATmega32U4 memory limits
#define FLASH_SIZE             32768
#define SRAM_SIZE              2560
#define TOTAL_RAM              (0x100 + SRAM_SIZE) // 0x00 to 0x0FF is registers & IO, 0x100+ is SRAM

class AvrEmulator {
public:
    uint8_t flash[FLASH_SIZE];
    uint8_t ram[TOTAL_RAM];
    uint16_t pc;
    uint8_t sreg;
    uint16_t sp;
    
    // Display buffer
    uint8_t displayBuffer[SCREEN_BUFFER_SIZE];
    uint16_t oledPage;
    uint16_t oledCol;
    bool oledDataMode;
    bool oledCsActive;

    // Timing & cycles
    uint32_t totalCycles;
    uint32_t timer0Cycles;
    bool isRunning;

    AvrEmulator() {
        reset();
    }

    void reset() {
        memset(ram, 0, sizeof(ram));
        memset(displayBuffer, 0, sizeof(displayBuffer));
        pc = 0;
        sreg = 0;
        sp = TOTAL_RAM - 1;
        oledPage = 0;
        oledCol = 0;
        oledDataMode = false;
        oledCsActive = false;
        totalCycles = 0;
        timer0Cycles = 0;
        isRunning = false;
    }

    void loadRom(const uint8_t *romData, size_t romSize) {
        reset();
        memset(flash, 0xFF, sizeof(flash));
        size_t copySize = (romSize > FLASH_SIZE) ? FLASH_SIZE : romSize;
        memcpy(flash, romData, copySize);
        isRunning = true;
    }

    // Hardware button reading mapped to ATmega32U4 PORT registers
    void updateButtons() {
        bool b1 = (digitalRead(ARDUBOY_PIN_BTN1) == LOW); // GPIO 2 (Left)
        bool b2 = (digitalRead(ARDUBOY_PIN_BTN2) == LOW); // GPIO 1 (Right)
        bool b3 = (digitalRead(ARDUBOY_PIN_BTN3) == LOW); // GPIO 0 (A)
        bool b4 = (digitalRead(ARDUBOY_PIN_BTN4) == LOW); // GPIO 5 (B)

        // Combos:
        bool btnUp = (b1 && b2);
        bool btnDown = (b3 && b4);
        bool btnLeft = b1 && !btnUp;
        bool btnRight = b2 && !btnUp;
        bool btnA = b3 && !btnDown;
        bool btnB = b4 && !btnDown;

        // PINF: UP (bit 7), DOWN (bit 6), LEFT (bit 5), RIGHT (bit 4) -> Active LOW on Arduboy
        uint8_t pinf = 0xFF;
        if (btnUp)    pinf &= ~(1 << 7);
        if (btnDown)  pinf &= ~(1 << 6);
        if (btnLeft)  pinf &= ~(1 << 5);
        if (btnRight) pinf &= ~(1 << 4);
        ram[0x2F] = pinf; // PINF register

        // PINE: A button (bit 6) -> Active LOW
        uint8_t pine = 0xFF;
        if (btnA) pine &= ~(1 << 6);
        ram[0x2C] = pine; // PINE register

        // PINB: B button (bit 4) -> Active LOW
        uint8_t pinb = 0xFF;
        if (btnB) pinb &= ~(1 << 4);
        ram[0x23] = pinb; // PINB register
    }

    // Fast screen refresh to I2C SSD1306
    void renderScreen() {
        Wire.beginTransmission(OLED_ADDR);
        Wire.write(0x00);
        Wire.write(0x21); Wire.write(0); Wire.write(SCREEN_WIDTH - 1);
        Wire.write(0x22); Wire.write(0); Wire.write((SCREEN_HEIGHT / 8) - 1);
        Wire.endTransmission();

        for (uint16_t i = 0; i < SCREEN_BUFFER_SIZE; i += 32) {
            Wire.beginTransmission(OLED_ADDR);
            Wire.write(0x40);
            Wire.write(&displayBuffer[i], 32);
            Wire.endTransmission();
        }
    }

    // Single step instruction execution
    inline void step() {
        if (pc >= (FLASH_SIZE / 2)) {
            pc = 0; // Wrap on overrun
        }

        uint16_t opcode = flash[pc * 2] | (flash[pc * 2 + 1] << 8);
        pc++;
        totalCycles++;
        timer0Cycles++;

        // Timer 0 overflow interrupt (approx every 1024 cycles for Arduino millis)
        if (timer0Cycles >= 1024) {
            timer0Cycles = 0;
            if (sreg & 0x80) { // Global interrupts enabled
                // Push PC to stack
                ram[sp--] = pc & 0xFF;
                ram[sp--] = (pc >> 8) & 0xFF;
                sreg &= ~0x80; // Clear I flag
                pc = 0x002E / 2; // TIM0_OVF vector
            }
        }

        // --- Core Instruction Decoder ---
        // NOP
        if (opcode == 0x0000) { return; }

        // RJMP (0xC000)
        if ((opcode & 0xF000) == 0xC000) {
            int16_t offset = opcode & 0x0FFF;
            if (offset & 0x0800) offset |= 0xF000;
            pc += offset;
            return;
        }

        // RCALL (0xD000)
        if ((opcode & 0xF000) == 0xD000) {
            int16_t offset = opcode & 0x0FFF;
            if (offset & 0x0800) offset |= 0xF000;
            ram[sp--] = pc & 0xFF;
            ram[sp--] = (pc >> 8) & 0xFF;
            pc += offset;
            return;
        }

        // RET (0x9508)
        if (opcode == 0x9508) {
            uint16_t high = ram[++sp];
            uint16_t low = ram[++sp];
            pc = (high << 8) | low;
            return;
        }

        // RETI (0x9518)
        if (opcode == 0x9518) {
            uint16_t high = ram[++sp];
            uint16_t low = ram[++sp];
            pc = (high << 8) | low;
            sreg |= 0x80; // Set I flag
            return;
        }

        // LDI (0xE000)
        if ((opcode & 0xF000) == 0xE000) {
            uint8_t d = 16 + ((opcode >> 4) & 0x0F);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
            ram[d] = k;
            return;
        }

        // IN (0xB000)
        if ((opcode & 0xF800) == 0xB000) {
            uint8_t r = (opcode >> 4) & 0x1F;
            uint8_t a = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
            ram[r] = ram[0x20 + a];
            return;
        }

        // OUT (0xB800)
        if ((opcode & 0xF800) == 0xB800) {
            uint8_t r = (opcode >> 4) & 0x1F;
            uint8_t a = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
            uint8_t val = ram[r];
            ram[0x20 + a] = val;

            // SPI Data Write Hook (SPDR = 0x2E -> I/O 0x4E in RAM)
            if (a == 0x2E || a == 0x0E) {
                // Check D/C pin state (PORTD bit 4 or PORTE bit 6)
                bool isData = (ram[0x2B] & (1 << 4)) != 0; // PORTD4
                if (isData) {
                    if (oledPage < 8 && oledCol < SCREEN_WIDTH) {
                        displayBuffer[oledPage * SCREEN_WIDTH + oledCol] = val;
                        oledCol++;
                        if (oledCol >= SCREEN_WIDTH) oledCol = 0;
                    }
                } else {
                    // Command interpreter
                    if ((val & 0xF0) == 0xB0) oledPage = val & 0x07;
                    else if ((val & 0xF0) == 0x00) oledCol = (oledCol & 0xF0) | (val & 0x0F);
                    else if ((val & 0xF0) == 0x10) oledCol = (oledCol & 0x0F) | ((val & 0x0F) << 4);
                }
            }
            return;
        }

        // Branch instructions BRBS / BRBC (0xF000)
        if ((opcode & 0xFC00) == 0xF000 || (opcode & 0xFC00) == 0xF400) {
            uint8_t s = opcode & 0x07;
            bool set = ((opcode & 0x0400) == 0);
            bool bitVal = (sreg & (1 << s)) != 0;
            if (bitVal == set) {
                int8_t k = (opcode >> 3) & 0x7F;
                if (k & 0x40) k |= 0x80;
                pc += k;
            }
            return;
        }

        // Fallback simple cycle advancement for unimplemented AVR opcodes
    }

    void runFrame(uint32_t cyclesPerFrame = 266666) { // 16MHz / 60fps = ~266,666 cycles
        updateButtons();
        uint32_t target = totalCycles + cyclesPerFrame;
        while (totalCycles < target && isRunning) {
            step();
        }
        renderScreen();
    }
};

extern AvrEmulator avrCore;
