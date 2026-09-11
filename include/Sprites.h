#pragma once
#include <Arduino.h>
#include "Arduboy2Core.h"

class Sprites {
public:
    static void drawSelfMasked(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame) {
        draw(x, y, bitmap, frame, nullptr, 0, 0); // 0 = self masked
    }

    static void drawOverwrite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame) {
        draw(x, y, bitmap, frame, nullptr, 0, 1); // 1 = overwrite
    }

    static void drawPlusMask(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame) {
        draw(x, y, bitmap, frame, nullptr, 0, 2); // 2 = plus mask
    }

    static void drawExternalMask(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t frame, uint8_t mask_frame) {
        draw(x, y, bitmap, frame, mask, mask_frame, 3); // 3 = external mask
    }

private:
    static void draw(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame, const uint8_t *mask, uint8_t mask_frame, uint8_t mode) {
        uint8_t w = pgm_read_byte(bitmap++);
        uint8_t h = pgm_read_byte(bitmap++);

        if (x + w <= 0 || x >= WIDTH || y + h <= 0 || y >= HEIGHT) return;

        uint8_t hPages = (h + 7) / 8;
        uint16_t frameOffset = (uint16_t)w * hPages;

        const uint8_t *bmpPtr = bitmap;
        const uint8_t *maskPtr = mask;

        if (mode == 2) { // Plus mask (data and mask interleaved: data byte, mask byte, ...)
            bmpPtr += (frame * frameOffset * 2);
        } else {
            bmpPtr += (frame * frameOffset);
            if (mask) {
                maskPtr += (mask_frame * frameOffset);
            }
        }

        uint8_t *sBuffer = Arduboy2Core::getBuffer();

        for (uint8_t page = 0; page < hPages; page++) {
            int16_t drawY = y + (page * 8);
            if (drawY <= -8 || drawY >= HEIGHT) continue;

            uint8_t yOffset = (y < 0 && (y % 8 != 0)) ? (8 - (abs(y) % 8)) : (y % 8);
            if (y >= 0) yOffset = y % 8;

            int16_t startPage = drawY / 8;
            if (drawY < 0) startPage = (drawY - 7) / 8;

            for (uint8_t col = 0; col < w; col++) {
                int16_t drawX = x + col;
                if (drawX < 0 || drawX >= WIDTH) continue;

                uint8_t dataByte, maskByte;

                if (mode == 2) { // Plus mask
                    uint16_t idx = (page * w + col) * 2;
                    dataByte = pgm_read_byte(bmpPtr + idx);
                    maskByte = pgm_read_byte(bmpPtr + idx + 1);
                } else {
                    uint16_t idx = page * w + col;
                    dataByte = pgm_read_byte(bmpPtr + idx);
                    if (mode == 0) maskByte = dataByte; // Self masked
                    else if (mode == 1) maskByte = 0xFF; // Overwrite
                    else if (mode == 3 && maskPtr) maskByte = pgm_read_byte(maskPtr + idx);
                    else maskByte = 0xFF;
                }

                // Render into screen buffer (splitting into upper and lower page if unaligned)
                int16_t p1 = startPage;
                int16_t p2 = startPage + 1;

                if (p1 >= 0 && p1 < 8) {
                    uint16_t bufIdx = p1 * WIDTH + drawX;
                    uint8_t d = dataByte << yOffset;
                    uint8_t m = maskByte << yOffset;
                    sBuffer[bufIdx] = (sBuffer[bufIdx] & ~m) | (d & m);
                }

                if (yOffset > 0 && p2 >= 0 && p2 < 8) {
                    uint16_t bufIdx = p2 * WIDTH + drawX;
                    uint8_t d = dataByte >> (8 - yOffset);
                    uint8_t m = maskByte >> (8 - yOffset);
                    sBuffer[bufIdx] = (sBuffer[bufIdx] & ~m) | (d & m);
                }
            }
        }
    }
};

class SpritesB : public Sprites {};
