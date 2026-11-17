#pragma once

// =============================================================================
// Capa de plataforma (HAL) del motor Catacombs of the Damned para la consola
// ESP32-C6 y para el emulador nativo de PC.
//
// Sustituye a la implementacion AVR de Arduboy3D.ino:
//   - Entrada: lee el estado ya sondeado por main.cpp (no llama a pollButtons),
//     de modo que los flancos de los botones no se consumen dos veces.
//   - Video: escribe en el framebuffer de Arduboy2 (128x64, paginas).
//   - Audio: traduce los patrones de tonos originales al buzzer de la consola
//     o al audio SDL2, respetando el interruptor global de sonido.
//   - Progreso: el record se guarda en NVS (namespace "catacombs").
// =============================================================================

#include <stdint.h>

namespace Catacombs3D
{

class Platform
{
public:
	static uint8_t GetInput(void);
	static void SetLED(uint8_t r, uint8_t g, uint8_t b);
	static uint8_t* GetScreenBuffer();

	static void PlaySound(const uint16_t* audioPattern);
	static bool IsAudioEnabled();
	static void SetAudioEnabled(bool isEnabled);

	static void ExpectLoadDelay();

	static void FillScreen(uint8_t col);
	static void PutPixel(uint8_t x, uint8_t y, uint8_t colour);
	static void DrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap);
	static void DrawSolidBitmap(int16_t x, int16_t y, const uint8_t *bitmap);
	static void DrawSprite(int16_t x, int16_t y, const uint8_t *bitmap, const uint8_t *mask, uint8_t frame, uint8_t mask_frame);
	static void DrawSprite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame);

	static void DrawVLine(uint8_t x, int8_t y1, int8_t y2, uint8_t pattern);

	// Avanza el secuenciador de tonos. Lo llama el bucle del cartucho.
	static void UpdateAudio();
};

} // namespace Catacombs3D
