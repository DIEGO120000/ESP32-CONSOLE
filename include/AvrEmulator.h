#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "EEPROM.h"

// Hardware Pin Configuration for ESP32-C3 Super Mini
#define ARDUBOY_PIN_SDA        3    // OLED SDA
#define ARDUBOY_PIN_SCL        4    // OLED SCL

#define ARDUBOY_PIN_UP         2    // D-Pad UP
#define ARDUBOY_PIN_DOWN       1    // D-Pad DOWN
#define ARDUBOY_PIN_LEFT       0    // D-Pad LEFT
#define ARDUBOY_PIN_RIGHT      5    // D-Pad RIGHT
#define ARDUBOY_PIN_A          8    // Button A
#define ARDUBOY_PIN_B          9    // Button B

#define ARDUBOY_PIN_BUZZER     20   // Audio Speaker on GPIO 20

#define OLED_ADDR              0x3C
#define SCREEN_WIDTH           128
#define SCREEN_HEIGHT          64
#define SCREEN_BUFFER_SIZE     (SCREEN_WIDTH * SCREEN_HEIGHT / 8) // 1024 bytes

#define FLASH_WORDS            16384 // 32KB
#define TOTAL_DATA_RAM         4096  // Guarded RAM space

class AvrCpu {
public:
    uint16_t flash[FLASH_WORDS];
    uint8_t ram[TOTAL_DATA_RAM];
    uint16_t pc;
    uint8_t sreg;
    uint16_t sp;

    uint8_t displayBuffer[SCREEN_BUFFER_SIZE];
    uint8_t oledPage;
    uint8_t oledCol;

    uint32_t totalCycles;
    uint32_t timer0Tick;
    uint32_t lastAudioTick;
    bool isRunning;

    AvrCpu() {
        reset();
    }

    void initHardware() {
        pinMode(ARDUBOY_PIN_UP, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_DOWN, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_LEFT, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_RIGHT, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_A, INPUT_PULLUP);
        pinMode(ARDUBOY_PIN_B, INPUT_PULLUP);

        pinMode(ARDUBOY_PIN_BUZZER, OUTPUT);
        noTone(ARDUBOY_PIN_BUZZER);

        Wire.begin(ARDUBOY_PIN_SDA, ARDUBOY_PIN_SCL);
        Wire.setClock(400000);

        initOled();
    }

    void initOled() {
        const uint8_t initSeq[] = {
            0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
            0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
            0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
        };
        for (size_t i = 0; i < sizeof(initSeq); i++) {
            sendOledCmd(initSeq[i]);
        }
    }

    void sendOledCmd(uint8_t cmd) {
        Wire.beginTransmission(OLED_ADDR);
        Wire.write(0x00);
        Wire.write(cmd);
        Wire.endTransmission();
    }

    void reset() {
        memset(ram, 0, sizeof(ram));
        memset(displayBuffer, 0, sizeof(displayBuffer));
        pc = 0;
        sreg = 0;
        sp = 2815; // 0x0AFF
        ram[0x5D] = sp & 0xFF;        // SPL
        ram[0x5E] = (sp >> 8) & 0xFF; // SPH
        ram[0x4D] = 0x80;             // SPSR.SPIF = 1

        oledPage = 0;
        oledCol = 0;
        totalCycles = 0;
        timer0Tick = 0;
        lastAudioTick = 0;
        isRunning = false;
    }

    void loadRom(const uint8_t *romBytes, size_t romSize) {
        reset();
        memset(flash, 0xFF, sizeof(flash));
        size_t words = romSize / 2;
        if (words > FLASH_WORDS) words = FLASH_WORDS;

        for (size_t i = 0; i < words; i++) {
            flash[i] = pgm_read_byte(romBytes + (i * 2)) | (pgm_read_byte(romBytes + (i * 2) + 1) << 8);
        }
        isRunning = true;
    }

    void updateButtons() {
        bool bUp    = (digitalRead(ARDUBOY_PIN_UP) == LOW);
        bool bDown  = (digitalRead(ARDUBOY_PIN_DOWN) == LOW);
        bool bLeft  = (digitalRead(ARDUBOY_PIN_LEFT) == LOW);
        bool bRight = (digitalRead(ARDUBOY_PIN_RIGHT) == LOW);
        bool bA     = (digitalRead(ARDUBOY_PIN_A) == LOW);
        bool bB     = (digitalRead(ARDUBOY_PIN_B) == LOW);

        if (bUp && bDown) bA = true;
        if (bLeft && bRight) bB = true;

        uint8_t pinf = 0xFF;
        if (bUp)    pinf &= ~(1 << 7);
        if (bDown)  pinf &= ~(1 << 6);
        if (bLeft)  pinf &= ~(1 << 5);
        if (bRight) pinf &= ~(1 << 4);
        ram[0x2F] = pinf;

        uint8_t pine = 0xFF;
        if (bA) pine &= ~(1 << 6);
        ram[0x2C] = pine;

        uint8_t pinb = 0xFF;
        if (bB) pinb &= ~(1 << 4);
        ram[0x23] = pinb;
    }

    void renderFrame() {
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

    inline bool getC() { return sreg & 1; }
    inline bool getZ() { return (sreg >> 1) & 1; }
    inline bool getN() { return (sreg >> 2) & 1; }
    inline bool getV() { return (sreg >> 3) & 1; }
    inline bool getS() { return (sreg >> 4) & 1; }
    inline bool getH() { return (sreg >> 5) & 1; }
    inline bool getT() { return (sreg >> 6) & 1; }
    inline bool getI() { return (sreg >> 7) & 1; }

    inline void setC(bool v) { if (v) sreg |= 1; else sreg &= ~1; }
    inline void setZ(bool v) { if (v) sreg |= 2; else sreg &= ~2; }
    inline void setN(bool v) { if (v) sreg |= 4; else sreg &= ~4; }
    inline void setV(bool v) { if (v) sreg |= 8; else sreg &= ~8; }
    inline void setS(bool v) { if (v) sreg |= 16; else sreg &= ~16; }
    inline void setH(bool v) { if (v) sreg |= 32; else sreg &= ~32; }
    inline void setT(bool v) { if (v) sreg |= 64; else sreg &= ~64; }
    inline void setI(bool v) { if (v) sreg |= 128; else sreg &= ~128; }

    inline void pushWord(uint16_t val) {
        if (sp >= 2) {
            ram[sp--] = val & 0xFF;
            ram[sp--] = (val >> 8) & 0xFF;
        }
    }

    inline uint16_t popWord() {
        if (sp <= 2813) {
            uint16_t high = ram[++sp];
            uint16_t low = ram[++sp];
            return (high << 8) | low;
        }
        return 0;
    }

    inline uint16_t getX() { return ram[26] | (ram[27] << 8); }
    inline void setX(uint16_t val) { ram[26] = val & 0xFF; ram[27] = (val >> 8) & 0xFF; }

    inline uint16_t getY() { return ram[28] | (ram[29] << 8); }
    inline void setY(uint16_t val) { ram[28] = val & 0xFF; ram[29] = (val >> 8) & 0xFF; }

    inline uint16_t getZval() { return ram[30] | (ram[31] << 8); }
    inline void setZval(uint16_t val) { ram[30] = val & 0xFF; ram[31] = (val >> 8) & 0xFF; }

    inline void writeRam(uint16_t addr, uint8_t val) {
        if (addr >= TOTAL_DATA_RAM) return;
        ram[addr] = val;

        // Speaker / Buzzer on PORTC6 (IO 0x08 -> RAM 0x28)
        if (addr == 0x28) {
            bool speakerState = (val & (1 << 6)) != 0;
            static bool lastSpeakerState = false;
            static uint32_t lastToggleMicros = 0;
            if (speakerState != lastSpeakerState) {
                lastSpeakerState = speakerState;
                uint32_t now = micros();
                uint32_t delta = now - lastToggleMicros;
                lastToggleMicros = now;
                if (delta > 40 && delta < 25000) {
                    uint16_t freq = 1000000 / (delta * 2);
                    if (freq >= 30 && freq <= 8000) {
                        tone(ARDUBOY_PIN_BUZZER, freq);
                        lastAudioTick = millis();
                    }
                }
            }
        }

        // SPDR write hook (IO 0x2E -> RAM 0x4E)
        if (addr == 0x4E) {
            ram[0x4D] = 0x80; // SPSR.SPIF ready

            bool isData = (ram[0x2B] & (1 << 4)) != 0; // PORTD4
            if (isData) {
                if (oledPage < 8 && oledCol < SCREEN_WIDTH) {
                    displayBuffer[oledPage * SCREEN_WIDTH + oledCol] = val;
                    oledCol++;
                    if (oledCol >= SCREEN_WIDTH) oledCol = 0;
                }
            } else {
                if ((val & 0xF0) == 0xB0) oledPage = val & 0x07;
                else if ((val & 0xF0) == 0x00) oledCol = (oledCol & 0xF0) | (val & 0x0F);
                else if ((val & 0xF0) == 0x10) oledCol = (oledCol & 0x0F) | ((val & 0x0F) << 4);
            }
        }
    }

    inline uint8_t readRam(uint16_t addr) {
        if (addr >= TOTAL_DATA_RAM) return 0xFF;
        if (addr == 0x4D) return 0x80;
        return ram[addr];
    }

    void executeInstruction() {
        uint16_t opcode = flash[pc];
        pc = (pc + 1) & (FLASH_WORDS - 1);
        totalCycles++;
        timer0Tick++;

        // Timer0 overflow interrupt
        if (timer0Tick >= 1024) {
            timer0Tick = 0;
            if (getI()) {
                pushWord(pc);
                setI(false);
                pc = 0x002E / 2;
            }
        }

        if (opcode == 0x0000) return;

        // RJMP (0xC000)
        if ((opcode & 0xF000) == 0xC000) {
            int16_t offset = opcode & 0x0FFF;
            if (offset & 0x0800) offset |= 0xF000;
            pc = (pc + offset) & (FLASH_WORDS - 1);
            return;
        }

        // RCALL (0xD000)
        if ((opcode & 0xF000) == 0xD000) {
            int16_t offset = opcode & 0x0FFF;
            if (offset & 0x0800) offset |= 0xF000;
            pushWord(pc);
            pc = (pc + offset) & (FLASH_WORDS - 1);
            return;
        }

        // RET (0x9508) / RETI (0x9518)
        if (opcode == 0x9508) { pc = popWord() & (FLASH_WORDS - 1); return; }
        if (opcode == 0x9518) { pc = popWord() & (FLASH_WORDS - 1); setI(true); return; }

        // JMP (0x940C) / CALL (0x940E)
        if ((opcode & 0xFE0E) == 0x940C) {
            uint16_t target = flash[pc];
            pc = (target >> 1) & (FLASH_WORDS - 1);
            return;
        }
        if ((opcode & 0xFE0E) == 0x940E) {
            uint16_t target = flash[pc];
            pushWord(pc + 1);
            pc = (target >> 1) & (FLASH_WORDS - 1);
            return;
        }

        // IJMP (0x9409) / ICALL (0x9509)
        if (opcode == 0x9409) { pc = (getZval() >> 1) & (FLASH_WORDS - 1); return; }
        if (opcode == 0x9509) { pushWord(pc); pc = (getZval() >> 1) & (FLASH_WORDS - 1); return; }

        // LDI (0xE000)
        if ((opcode & 0xF000) == 0xE000) {
            uint8_t d = 16 + ((opcode >> 4) & 0x0F);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
            ram[d] = k;
            return;
        }

        // SUBI (0x5000) / SBCI (0x4000)
        if ((opcode & 0xF000) == 0x5000) {
            uint8_t d = 16 + ((opcode >> 4) & 0x0F);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
            uint16_t res = (uint16_t)ram[d] - (uint16_t)k;
            ram[d] = res & 0xFF;
            setZ(ram[d] == 0);
            setC(res & 0x100);
            setN(ram[d] & 0x80);
            return;
        }
        if ((opcode & 0xF000) == 0x4000) {
            uint8_t d = 16 + ((opcode >> 4) & 0x0F);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
            uint16_t res = (uint16_t)ram[d] - (uint16_t)k - (getC() ? 1 : 0);
            ram[d] = res & 0xFF;
            setZ(ram[d] == 0);
            setC(res & 0x100);
            setN(ram[d] & 0x80);
            return;
        }

        // ORI (0x6000) / ANDI (0x7000)
        if ((opcode & 0xF000) == 0x6000) {
            uint8_t d = 16 + ((opcode >> 4) & 0x0F);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
            ram[d] |= k;
            setZ(ram[d] == 0);
            setN(ram[d] & 0x80);
            return;
        }
        if ((opcode & 0xF000) == 0x7000) {
            uint8_t d = 16 + ((opcode >> 4) & 0x0F);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
            ram[d] &= k;
            setZ(ram[d] == 0);
            setN(ram[d] & 0x80);
            return;
        }

        // CPI (0x3000)
        if ((opcode & 0xF000) == 0x3000) {
            uint8_t d = 16 + ((opcode >> 4) & 0x0F);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 4) & 0xF0);
            uint16_t res = (uint16_t)ram[d] - (uint16_t)k;
            setZ((res & 0xFF) == 0);
            setC(res & 0x100);
            setN(res & 0x80);
            return;
        }

        // MOV (0x2C00) / MOVW (0x0100)
        if ((opcode & 0xFC00) == 0x2C00) {
            uint8_t r = (opcode & 0x0F) | ((opcode >> 5) & 0x10);
            uint8_t d = (opcode >> 4) & 0x1F;
            ram[d] = ram[r];
            return;
        }
        if ((opcode & 0xFF00) == 0x0100) {
            uint8_t r = (opcode & 0x0F) << 1;
            uint8_t d = ((opcode >> 4) & 0x0F) << 1;
            ram[d] = ram[r];
            ram[d + 1] = ram[r + 1];
            return;
        }

        // IN (0xB000) / OUT (0xB800)
        if ((opcode & 0xF800) == 0xB000) {
            uint8_t r = (opcode >> 4) & 0x1F;
            uint8_t a = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
            ram[r] = readRam(0x20 + a);
            return;
        }
        if ((opcode & 0xF800) == 0xB800) {
            uint8_t r = (opcode >> 4) & 0x1F;
            uint8_t a = (opcode & 0x0F) | ((opcode >> 5) & 0x30);
            writeRam(0x20 + a, ram[r]);
            return;
        }

        // LDS (0x9000) / STS (0x9200)
        if ((opcode & 0xFE0F) == 0x9000) {
            uint8_t d = (opcode >> 4) & 0x1F;
            uint16_t k = flash[pc];
            pc = (pc + 1) & (FLASH_WORDS - 1);
            ram[d] = readRam(k);
            return;
        }
        if ((opcode & 0xFE0F) == 0x9200) {
            uint8_t r = (opcode >> 4) & 0x1F;
            uint16_t k = flash[pc];
            pc = (pc + 1) & (FLASH_WORDS - 1);
            writeRam(k, ram[r]);
            return;
        }

        // PUSH (0x920F) / POP (0x900F)
        if ((opcode & 0xFE0F) == 0x920F) {
            uint8_t r = (opcode >> 4) & 0x1F;
            if (sp > 0) ram[sp--] = ram[r];
            return;
        }
        if ((opcode & 0xFE0F) == 0x900F) {
            uint8_t d = (opcode >> 4) & 0x1F;
            if (sp < 2815) ram[d] = ram[++sp];
            return;
        }

        // ADIW (0x9600) / SBIW (0x9700)
        if ((opcode & 0xFE00) == 0x9600) {
            uint8_t r = 24 + ((opcode >> 3) & 0x06);
            uint8_t k = (opcode & 0x0F) | ((opcode >> 2) & 0x30);
            bool isSub = (opcode & 0x0100) != 0;
            uint16_t val = ram[r] | (ram[r + 1] << 8);
            uint16_t res = isSub ? (val - k) : (val + k);
            ram[r] = res & 0xFF;
            ram[r + 1] = (res >> 8) & 0xFF;
            setZ((res & 0xFFFF) == 0);
            setC(res & 0x10000);
            return;
        }

        // Branches (0xF000)
        if ((opcode & 0xF800) == 0xF000) {
            uint8_t s = opcode & 0x07;
            bool branchOnSet = ((opcode & 0x0400) == 0);
            bool bitVal = (sreg & (1 << s)) != 0;
            if (bitVal == branchOnSet) {
                int8_t k = (opcode >> 3) & 0x7F;
                if (k & 0x40) k |= 0x80;
                pc = (pc + k) & (FLASH_WORDS - 1);
            }
            return;
        }

        // CPSE (0x1000)
        if ((opcode & 0xFC00) == 0x1000) {
            uint8_t r = (opcode & 0x0F) | ((opcode >> 5) & 0x10);
            uint8_t d = (opcode >> 4) & 0x1F;
            if (ram[d] == ram[r]) {
                uint16_t nextOp = flash[pc];
                bool is32 = ((nextOp & 0xFE0E) == 0x940C || (nextOp & 0xFE0E) == 0x940E || 
                             (nextOp & 0xFE0F) == 0x9000 || (nextOp & 0xFE0F) == 0x9200);
                pc = (pc + (is32 ? 2 : 1)) & (FLASH_WORDS - 1);
            }
            return;
        }

        // CP / CPC
        if ((opcode & 0xFC00) == 0x1400 || (opcode & 0xFC00) == 0x0400) {
            uint8_t r = (opcode & 0x0F) | ((opcode >> 5) & 0x10);
            uint8_t d = (opcode >> 4) & 0x1F;
            bool withCarry = (opcode & 0x1000) == 0;
            uint16_t res = (uint16_t)ram[d] - (uint16_t)ram[r] - ((withCarry && getC()) ? 1 : 0);
            setZ((res & 0xFF) == 0);
            setC(res & 0x100);
            setN(res & 0x80);
            return;
        }

        // ADD / ADC / SUB / SBC
        if ((opcode & 0xFC00) == 0x0C00 || (opcode & 0xFC00) == 0x1C00 || 
            (opcode & 0xFC00) == 0x1800 || (opcode & 0xFC00) == 0x0800) {
            uint8_t r = (opcode & 0x0F) | ((opcode >> 5) & 0x10);
            uint8_t d = (opcode >> 4) & 0x1F;
            bool isSub = (opcode & 0x1000) != 0;
            bool withCarry = (opcode & 0x0800) != 0;
            uint8_t cVal = (withCarry && getC()) ? 1 : 0;
            uint16_t res = isSub ? ((uint16_t)ram[d] - (uint16_t)ram[r] - cVal) 
                                 : ((uint16_t)ram[d] + (uint16_t)ram[r] + cVal);
            ram[d] = res & 0xFF;
            setZ(ram[d] == 0);
            setC(res & 0x100);
            setN(ram[d] & 0x80);
            return;
        }

        // AND / OR / EOR
        if ((opcode & 0xFC00) == 0x2000) { ram[(opcode >> 4) & 0x1F] &= ram[(opcode & 0x0F) | ((opcode >> 5) & 0x10)]; setZ(ram[(opcode >> 4) & 0x1F] == 0); return; }
        if ((opcode & 0xFC00) == 0x2800) { ram[(opcode >> 4) & 0x1F] |= ram[(opcode & 0x0F) | ((opcode >> 5) & 0x10)]; setZ(ram[(opcode >> 4) & 0x1F] == 0); return; }
        if ((opcode & 0xFC00) == 0x2400) { ram[(opcode >> 4) & 0x1F] ^= ram[(opcode & 0x0F) | ((opcode >> 5) & 0x10)]; setZ(ram[(opcode >> 4) & 0x1F] == 0); return; }

        // INC / DEC
        if ((opcode & 0xFE0F) == 0x9403) { uint8_t d = (opcode >> 4) & 0x1F; ram[d]++; setZ(ram[d] == 0); setN(ram[d] & 0x80); return; }
        if ((opcode & 0xFE0F) == 0x940A) { uint8_t d = (opcode >> 4) & 0x1F; ram[d]--; setZ(ram[d] == 0); setN(ram[d] & 0x80); return; }

        // Shifts & Rotates
        if ((opcode & 0xFE0F) == 0x9406) { // LSR
            uint8_t d = (opcode >> 4) & 0x1F;
            setC(ram[d] & 1);
            ram[d] >>= 1;
            setZ(ram[d] == 0);
            return;
        }
        if ((opcode & 0xFE0F) == 0x9407) { // ROR
            uint8_t d = (opcode >> 4) & 0x1F;
            bool oldC = getC();
            setC(ram[d] & 1);
            ram[d] = (ram[d] >> 1) | (oldC ? 0x80 : 0);
            setZ(ram[d] == 0);
            return;
        }
        if ((opcode & 0xFE0F) == 0x9402) { // SWAP
            uint8_t d = (opcode >> 4) & 0x1F;
            ram[d] = ((ram[d] & 0x0F) << 4) | ((ram[d] & 0xF0) >> 4);
            return;
        }

        // LPM
        if (opcode == 0x95C8) {
            uint16_t z = getZval();
            uint16_t w = flash[z >> 1];
            ram[0] = (z & 1) ? (w >> 8) : (w & 0xFF);
            return;
        }
        if ((opcode & 0xFE0E) == 0x9004) {
            uint8_t d = (opcode >> 4) & 0x1F;
            uint16_t z = getZval();
            uint16_t w = flash[z >> 1];
            ram[d] = (z & 1) ? (w >> 8) : (w & 0xFF);
            if (opcode & 1) setZval(z + 1);
            return;
        }

        // LD / ST (X, Y, Z)
        if ((opcode & 0xFE0F) == 0x900C) { ram[(opcode >> 4) & 0x1F] = readRam(getX()); return; }
        if ((opcode & 0xFE0F) == 0x900D) { uint16_t x = getX(); ram[(opcode >> 4) & 0x1F] = readRam(x); setX(x + 1); return; }
        if ((opcode & 0xFE0F) == 0x900E) { uint16_t x = getX() - 1; setX(x); ram[(opcode >> 4) & 0x1F] = readRam(x); return; }

        if ((opcode & 0xFE0F) == 0x920C) { writeRam(getX(), ram[(opcode >> 4) & 0x1F]); return; }
        if ((opcode & 0xFE0F) == 0x920D) { uint16_t x = getX(); writeRam(x, ram[(opcode >> 4) & 0x1F]); setX(x + 1); return; }
        if ((opcode & 0xFE0F) == 0x920E) { uint16_t x = getX() - 1; setX(x); writeRam(x, ram[(opcode >> 4) & 0x1F]); return; }

        // LDD / STD (Y, Z displacement)
        if ((opcode & 0xD200) == 0x8000) {
            uint8_t d = (opcode >> 4) & 0x1F;
            uint8_t q = (opcode & 0x07) | ((opcode >> 7) & 0x18) | ((opcode >> 8) & 0x20);
            uint16_t base = (opcode & 0x0008) ? getY() : getZval();
            ram[d] = readRam(base + q);
            return;
        }
        if ((opcode & 0xD200) == 0x8200) {
            uint8_t r = (opcode >> 4) & 0x1F;
            uint8_t q = (opcode & 0x07) | ((opcode >> 7) & 0x18) | ((opcode >> 8) & 0x20);
            uint16_t base = (opcode & 0x0008) ? getY() : getZval();
            writeRam(base + q, ram[r]);
            return;
        }

        // SBRC / SBRS
        if ((opcode & 0xF800) == 0xF800) {
            uint8_t r = (opcode >> 4) & 0x1F;
            uint8_t b = opcode & 0x07;
            bool checkSet = ((opcode & 0x0200) != 0);
            if (((ram[r] & (1 << b)) != 0) == checkSet) {
                uint16_t nextOp = flash[pc];
                bool is32 = ((nextOp & 0xFE0E) == 0x940C || (nextOp & 0xFE0E) == 0x940E || 
                             (nextOp & 0xFE0F) == 0x9000 || (nextOp & 0xFE0F) == 0x9200);
                pc = (pc + (is32 ? 2 : 1)) & (FLASH_WORDS - 1);
            }
            return;
        }

        // SBI / CBI / SBIS / SBIC
        if ((opcode & 0xFD00) == 0x9800) {
            uint8_t a = (opcode >> 3) & 0x1F;
            uint8_t b = opcode & 0x07;
            bool setBit = (opcode & 0x0200) != 0;
            uint8_t val = readRam(0x20 + a);
            if (setBit) val |= (1 << b); else val &= ~(1 << b);
            writeRam(0x20 + a, val);
            return;
        }
        if ((opcode & 0xFD00) == 0x9900) {
            uint8_t a = (opcode >> 3) & 0x1F;
            uint8_t b = opcode & 0x07;
            bool checkSet = (opcode & 0x0200) != 0;
            if (((readRam(0x20 + a) & (1 << b)) != 0) == checkSet) {
                uint16_t nextOp = flash[pc];
                bool is32 = ((nextOp & 0xFE0E) == 0x940C || (nextOp & 0xFE0E) == 0x940E || 
                             (nextOp & 0xFE0F) == 0x9000 || (nextOp & 0xFE0F) == 0x9200);
                pc = (pc + (is32 ? 2 : 1)) & (FLASH_WORDS - 1);
            }
            return;
        }

        // SEI / CLI / BSET / BCLR
        if ((opcode & 0xFF8F) == 0x9408) {
            uint8_t s = (opcode >> 4) & 0x07;
            bool setBit = (opcode & 0x0070) == 0x0000;
            if (setBit) sreg |= (1 << s); else sreg &= ~(1 << s);
            return;
        }
    }

    void runFrameCycles(uint32_t cycles = 150000) {
        updateButtons();
        uint32_t target = totalCycles + cycles;
        while (totalCycles < target && isRunning) {
            for (int i = 0; i < 1000; i++) {
                executeInstruction();
            }
            yield();
        }
        if (millis() - lastAudioTick > 35) {
            noTone(ARDUBOY_PIN_BUZZER);
        }
        renderFrame();
    }
};

extern AvrCpu avrCpu;
