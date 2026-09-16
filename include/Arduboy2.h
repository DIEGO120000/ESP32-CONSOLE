#pragma once
#include <Arduino.h>
#include <Print.h>
#include "Arduboy2Core.h"
#include "Arduboy2Audio.h"
#include "Arduboy2Beep.h"
#include "ArduboyTones.h"
#include "Sprites.h"
#include "glcdfont.h"

#define ARDUBOY_LIB_VER 20000

class Arduboy2Base : public Print, public Arduboy2Core {
public:
    Arduboy2Audio audio;

    int16_t cursor_x = 0;
    int16_t cursor_y = 0;
    uint8_t textsize = 1;
    uint8_t textcolor = 1;
    uint8_t textbg = 0;
    bool wrap = true;

    uint8_t currentButtonState = 0;
    uint8_t previousButtonState = 0;

    uint8_t targetFrameRate = 60;
    uint16_t frameDurationMs = 1000 / 60;
    uint32_t lastFrameTime = 0;
    uint32_t frameCount = 0;
    uint8_t frameRateCpuLoad = 0;

    Arduboy2Base() {}

    void begin() {
        boot();
        audio.begin();
        setFrameRate(60);
    }

    void initRandomSeed() {
        randomSeed(analogRead(0) ^ millis());
    }

    void delayShort(uint16_t ms) {
        delay(ms);
    }

    void setFrameRate(uint8_t rate) {
        if (rate == 0) rate = 1;
        targetFrameRate = rate;
        frameDurationMs = 1000 / rate;
    }

    bool nextFrame() {
        uint32_t now = millis();
        uint32_t elapsed = now - lastFrameTime;

        if (elapsed < frameDurationMs) {
            delay(1);
            return false;
        }

        lastFrameTime = now;
        frameCount++;
        pollButtons();
        return true;
    }

    bool nextFrameDEV() {
        return nextFrame();
    }

    bool everyXFrames(uint8_t frames) {
        return (frameCount % frames) == 0;
    }

    uint8_t cpuLoad() {
        return frameRateCpuLoad;
    }

    void pollButtons() {
        previousButtonState = currentButtonState;
        currentButtonState = buttonsState();
    }

    bool pressed(uint8_t buttons) {
        return (currentButtonState & buttons) == buttons;
    }

    bool notPressed(uint8_t buttons) {
        return (currentButtonState & buttons) == 0;
    }

    bool justPressed(uint8_t button) {
        return ((currentButtonState & button) == button) && ((previousButtonState & button) == 0);
    }

    bool justReleased(uint8_t button) {
        return ((currentButtonState & button) == 0) && ((previousButtonState & button) == button);
    }

    // --- Drawing primitives ---
    void drawPixel(int16_t x, int16_t y, uint8_t color = WHITE) {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
        uint16_t idx = (y / 8) * WIDTH + x;
        uint8_t bit = 1 << (y % 8);
        if (color == WHITE) {
            sBuffer[idx] |= bit;
        } else if (color == BLACK) {
            sBuffer[idx] &= ~bit;
        } else if (color == INVERT) {
            sBuffer[idx] ^= bit;
        }
    }

    uint8_t getPixel(uint8_t x, uint8_t y) {
        if (x >= WIDTH || y >= HEIGHT) return 0;
        uint16_t idx = (y / 8) * WIDTH + x;
        return (sBuffer[idx] >> (y % 8)) & 0x01;
    }

    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color = WHITE) {
        if (x < 0 || x >= WIDTH || h <= 0) return;
        if (y < 0) { h += y; y = 0; }
        if (y + h > HEIGHT) h = HEIGHT - y;
        if (h <= 0) return;

        for (int16_t i = 0; i < h; i++) {
            drawPixel(x, y + i, color);
        }
    }

    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color = WHITE) {
        if (y < 0 || y >= HEIGHT || w <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (x + w > WIDTH) w = WIDTH - x;
        if (w <= 0) return;

        for (int16_t i = 0; i < w; i++) {
            drawPixel(x + i, y, color);
        }
    }

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color = WHITE) {
        int16_t steep = abs(y1 - y0) > abs(x1 - x0);
        if (steep) {
            int16_t t; t = x0; x0 = y0; y0 = t;
            t = x1; x1 = y1; y1 = t;
        }
        if (x0 > x1) {
            int16_t t; t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
        }
        int16_t dx = x1 - x0;
        int16_t dy = abs(y1 - y0);
        int16_t err = dx / 2;
        int16_t ystep = (y0 < y1) ? 1 : -1;

        for (; x0 <= x1; x0++) {
            if (steep) drawPixel(y0, x0, color);
            else drawPixel(x0, y0, color);
            err -= dy;
            if (err < 0) {
                y0 += ystep;
                err += dx;
            }
        }
    }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color = WHITE) {
        drawFastHLine(x, y, w, color);
        drawFastHLine(x, y + h - 1, w, color);
        drawFastVLine(x, y, h, color);
        drawFastVLine(x + w - 1, y, h, color);
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color = WHITE) {
        for (int16_t i = x; i < x + w; i++) {
            drawFastVLine(i, y, h, color);
        }
    }

    void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color = WHITE) {
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;

        drawPixel(x0, y0 + r, color);
        drawPixel(x0, y0 - r, color);
        drawPixel(x0 + r, y0, color);
        drawPixel(x0 - r, y0, color);

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;

            drawPixel(x0 + x, y0 + y, color);
            drawPixel(x0 - x, y0 + y, color);
            drawPixel(x0 + x, y0 - y, color);
            drawPixel(x0 - x, y0 - y, color);
            drawPixel(x0 + y, y0 + x, color);
            drawPixel(x0 - y, y0 + x, color);
            drawPixel(x0 + y, y0 - x, color);
            drawPixel(x0 - y, y0 - x, color);
        }
    }

    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color = WHITE) {
        drawFastVLine(x0, y0 - r, 2 * r + 1, color);
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;

            drawFastVLine(x0 + x, y0 - y, 2 * y + 1, color);
            drawFastVLine(x0 - x, y0 - y, 2 * y + 1, color);
            drawFastVLine(x0 + y, y0 - x, 2 * x + 1, color);
            drawFastVLine(x0 - y, y0 - x, 2 * x + 1, color);
        }
    }

    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color = WHITE) {
        drawLine(x0, y0, x1, y1, color);
        drawLine(x1, y1, x2, y2, color);
        drawLine(x2, y2, x0, y0, color);
    }

    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color = WHITE) {
        int16_t a, b, y, last;
        if (y0 > y1) { int16_t t; t=y0; y0=y1; y1=t; t=x0; x0=x1; x1=t; }
        if (y1 > y2) { int16_t t; t=y1; y1=y2; y2=t; t=x1; x1=x2; x2=t; }
        if (y0 > y1) { int16_t t; t=y0; y0=y1; y1=t; t=x0; x0=x1; x1=t; }

        if (y0 == y2) {
            a = b = x0;
            if (x1 < a) a = x1; else if (x1 > b) b = x1;
            if (x2 < a) a = x2; else if (x2 > b) b = x2;
            drawFastHLine(a, y0, b - a + 1, color);
            return;
        }

        int16_t dx01 = x1 - x0, dy01 = y1 - y0;
        int16_t dx02 = x2 - x0, dy02 = y2 - y0;
        int16_t dx12 = x2 - x1, dy12 = y2 - y1;
        int32_t sa = 0, sb = 0;

        if (y1 == y2) last = y1;
        else last = y1 - 1;

        for (y = y0; y <= last; y++) {
            a = x0 + sa / dy01;
            b = x0 + sb / dy02;
            sa += dx01;
            sb += dx02;
            if (a > b) { int16_t t = a; a = b; b = t; }
            drawFastHLine(a, y, b - a + 1, color);
        }

        sa = (int32_t)dx12 * (y - y1);
        sb = (int32_t)dx02 * (y - y0);
        for (; y <= y2; y++) {
            a = x1 + sa / dy12;
            b = x0 + sb / dy02;
            sa += dx12;
            sb += dx02;
            if (a > b) { int16_t t = a; a = b; b = t; }
            drawFastHLine(a, y, b - a + 1, color);
        }
    }

    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color = WHITE) {
        drawFastHLine(x + r, y, w - 2 * r, color);
        drawFastHLine(x + r, y + h - 1, w - 2 * r, color);
        drawFastVLine(x, y + r, h - 2 * r, color);
        drawFastVLine(x + w - 1, y + r, h - 2 * r, color);
        // Draw corners
        int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, cx = 0, cy = r;
        while (cx < cy) {
            if (f >= 0) { cy--; ddF_y += 2; f += ddF_y; }
            cx++; ddF_x += 2; f += ddF_x;
            drawPixel(x + w - r - 1 + cx, y + r - cy, color);
            drawPixel(x + r - cx, y + r - cy, color);
            drawPixel(x + w - r - 1 + cx, y + h - r - 1 + cy, color);
            drawPixel(x + r - cx, y + h - r - 1 + cy, color);
            drawPixel(x + w - r - 1 + cy, y + r - cx, color);
            drawPixel(x + r - cy, y + r - cx, color);
            drawPixel(x + w - r - 1 + cy, y + h - r - 1 + cx, color);
            drawPixel(x + r - cy, y + h - r - 1 + cx, color);
        }
    }

    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint8_t color = WHITE) {
        fillRect(x + r, y, w - 2 * r, h, color);
        // Fill quarter circles on the edges
        int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, cx = 0, cy = r;
        while (cx < cy) {
            if (f >= 0) { cy--; ddF_y += 2; f += ddF_y; }
            cx++; ddF_x += 2; f += ddF_x;
            drawFastVLine(x + r - cx, y + r - cy, 2 * cy + 1 + h - 2 * r - 1, color);
            drawFastVLine(x + w - r - 1 + cx, y + r - cy, 2 * cy + 1 + h - 2 * r - 1, color);
            drawFastVLine(x + r - cy, y + r - cx, 2 * cx + 1 + h - 2 * r - 1, color);
            drawFastVLine(x + w - r - 1 + cy, y + r - cx, 2 * cx + 1 + h - 2 * r - 1, color);
        }
    }

    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color = WHITE) {
        int16_t byteWidth = (w + 7) / 8;
        uint8_t byte = 0;
        for (int16_t j = 0; j < h; j++) {
            for (int16_t i = 0; i < w; i++) {
                if (i & 7) byte <<= 1;
                else byte = pgm_read_byte(bitmap + j * byteWidth + i / 8);
                if (byte & 0x80) drawPixel(x + i, y + j, color);
            }
        }
    }

    void drawSlowXYBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color = WHITE) {
        drawBitmap(x, y, bitmap, w, h, color);
    }

    void drawArduboyBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint8_t color = WHITE) {
        int16_t pages = (h + 7) / 8;
        for (int16_t p = 0; p < pages; p++) {
            for (int16_t col = 0; col < w; col++) {
                uint8_t byte = pgm_read_byte(bitmap + (p * w) + col);
                for (uint8_t bit = 0; bit < 8; bit++) {
                    if (p * 8 + bit < h) {
                        if (byte & (1 << bit)) {
                            drawPixel(x + col, y + p * 8 + bit, color);
                        }
                    }
                }
            }
        }
    }

    // --- Text rendering ---
    void setCursor(int16_t x, int16_t y) {
        cursor_x = x;
        cursor_y = y;
    }

    void setTextSize(uint8_t s) {
        textsize = (s > 0) ? s : 1;
    }

    void setTextColor(uint8_t c) {
        textcolor = c;
        textbg = (c == WHITE) ? BLACK : WHITE;
    }

    void setTextColor(uint8_t c, uint8_t bg) {
        textcolor = c;
        textbg = bg;
    }

    void setTextWrap(bool w) {
        wrap = w;
    }

    size_t write(uint8_t c) override {
        if (c == '\n') {
            cursor_y += textsize * 8;
            cursor_x = 0;
        } else if (c == '\r') {
            // skip
        } else {
            drawChar(cursor_x, cursor_y, c, textcolor, textbg, textsize);
            cursor_x += textsize * 6;
            if (wrap && (cursor_x > (WIDTH - textsize * 6))) {
                cursor_y += textsize * 8;
                cursor_x = 0;
            }
        }
        return 1;
    }

    void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size) {
        for (int8_t i = 0; i < 5; i++) {
            uint8_t line = pgm_read_byte(&font5x7[c * 5 + i]);
            for (int8_t j = 0; j < 8; j++, line >>= 1) {
                if (line & 1) {
                    if (size == 1) drawPixel(x + i, y + j, color);
                    else fillRect(x + (i * size), y + (j * size), size, size, color);
                } else if (bg != color) {
                    if (size == 1) drawPixel(x + i, y + j, bg);
                    else fillRect(x + (i * size), y + (j * size), size, size, bg);
                }
            }
        }
        if (bg != color) {
            if (size == 1) drawFastVLine(x + 5, y, 8, bg);
            else fillRect(x + 5 * size, y, size, 8 * size, bg);
        }
    }
};

class Arduboy2 : public Arduboy2Base {};
