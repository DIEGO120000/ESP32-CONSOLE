// =============================================================================
// HAL del motor Catacombs of the Damned para ESP32-C6 / emulador PC.
//
// Todo el codigo del juego original (jhhoward/Arduboy3D, MIT) queda encapsulado
// en namespace Catacombs3D para no colisionar con Wolfenduino 3D ni con el
// nucleo de la consola. Aqui solo vive la capa de plataforma.
// =============================================================================

#include <Arduino.h>

#include "Arduboy2.h"
#include "ArduboyTones.h"

#include "Platform.h"
#include "Defines.h"
#include "Sounds.h"
#include "sfx_pcm.h"

// Objetos globales de la consola (definidos en src/main.cpp o sim/main_sim.cpp).
extern Arduboy2 arduboy;
extern ArduboyTones sound;

namespace Catacombs3D
{

static Sprites sprites;

uint8_t* Platform::GetScreenBuffer()
{
	return arduboy.getBuffer();
}

// -----------------------------------------------------------------------------
// Entrada. La cruceta se mapea 1:1 con el original. El boton A dispara la bola
// de fuego (era B en Arduboy) y, mantenido junto a LEFT/RIGHT, hace strafe
// lateral (era A en Arduboy). El boton B fisico queda reservado a la pausa
// universal de main.cpp y no llega al juego.
// -----------------------------------------------------------------------------
uint8_t Platform::GetInput()
{
	uint8_t result = 0;

	if (arduboy.pressed(UP_BUTTON))
		result |= INPUT_UP;
	if (arduboy.pressed(DOWN_BUTTON))
		result |= INPUT_DOWN;
	if (arduboy.pressed(LEFT_BUTTON))
		result |= INPUT_LEFT;
	if (arduboy.pressed(RIGHT_BUTTON))
		result |= INPUT_RIGHT;

	const bool a = arduboy.pressed(A_BUTTON);
	const bool horizontal = arduboy.pressed(LEFT_BUTTON) || arduboy.pressed(RIGHT_BUTTON);

	if (a)
	{
		if (horizontal)
			result |= INPUT_A; // strafe lateral
		else
			result |= INPUT_B; // disparar
	}

	return result;
}

void Platform::SetLED(uint8_t r, uint8_t g, uint8_t b)
{
	// La consola gestiona su propio LED RGB desde el nucleo; el juego no lo toca.
	(void)r;
	(void)g;
	(void)b;
}

void Platform::PutPixel(uint8_t x, uint8_t y, uint8_t colour)
{
	arduboy.drawPixel(x, y, colour);
}

// Adaptado de https://github.com/a1k0n/arduboy3d/blob/master/draw.cpp
static const uint8_t topmask_[] PROGMEM = {
	0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80 };
static const uint8_t bottommask_[] PROGMEM = {
	0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };

void Platform::DrawVLine(uint8_t x, int8_t y0_, int8_t y1_, uint8_t pattern)
{
	uint8_t *screenptr = arduboy.getBuffer() + x;

	if (y1_ < y0_ || y1_ < 0 || y0_ > 63) return;

	uint8_t y0 = y0_, y1 = y1_;
	if (y0_ < 0) y0 = 0;
	if (y1_ > 63) y1 = 63;

	uint8_t *page0 = screenptr + ((y0 & 0x38) << 4);
	uint8_t *page1 = screenptr + ((y1 & 0x38) << 4);
	if (page0 == page1)
	{
		uint8_t mask = pgm_read_byte(topmask_ + (y0 & 7))
			& pgm_read_byte(bottommask_ + (y1 & 7));
		*page0 &= ~mask;
		*page0 |= pattern & mask;
	}
	else
	{
		uint8_t mask = pgm_read_byte(topmask_ + (y0 & 7));
		*page0 &= ~mask;
		*page0 |= pattern & mask;
		page0 += 128;
		while (page0 != page1)
		{
			*page0 = pattern;
			page0 += 128;
		}
		mask = pgm_read_byte(bottommask_ + (y1 & 7));
		*page0 &= ~mask;
		*page0 |= pattern & mask;
	}
}

void Platform::DrawSprite(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t frame)
{
	sprites.drawPlusMask(x, y, bitmap, frame);
}

void Platform::DrawSprite(int16_t x, int16_t y, const uint8_t *bitmap,
	const uint8_t *mask, uint8_t frame, uint8_t mask_frame)
{
	sprites.drawExternalMask(x, y, bitmap, mask, frame, mask_frame);
}

void Platform::DrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap)
{
	uint8_t w = pgm_read_byte(&bitmap[0]);
	uint8_t h = pgm_read_byte(&bitmap[1]);
	arduboy.drawBitmap(x, y, bitmap + 2, w, h);
}

void Platform::DrawSolidBitmap(int16_t x, int16_t y, const uint8_t *bitmap)
{
	uint8_t w = pgm_read_byte(&bitmap[0]);
	uint8_t h = pgm_read_byte(&bitmap[1]);
	arduboy.fillRect(x, y, w, h, BLACK);
	arduboy.drawBitmap(x, y, bitmap + 2, w, h);
}

void Platform::FillScreen(uint8_t colour)
{
	arduboy.fillScreen(colour);
}

// -----------------------------------------------------------------------------
// Audio. Los patrones originales son pares (frecuencia Hz, duracion ms) en
// flash, terminados en 0x8000. Se reproducen nota a nota sin bloquear el
// render, respetando el volumen global (MUTE) de la consola. En el emulador
// ArduboyTones escribe en la onda cuadrada de SDL2.
// -----------------------------------------------------------------------------
static const uint16_t* tonePattern = nullptr;
static uint16_t tonePosition = 0;
static uint32_t toneNoteEnd = 0;
static bool tonePlaying = false;

static void stopPattern()
{
	tonePattern = nullptr;
	tonePlaying = false;
	toneNoteEnd = 0;
	sound.noTone();
}

static void startNextNote()
{
	if (!tonePattern)
		return;

	const uint16_t freq = pgm_read_word(&tonePattern[tonePosition]);
	const uint16_t duration = pgm_read_word(&tonePattern[tonePosition + 1]);

	if (freq & 0x8000)
	{
		stopPattern();
		return;
	}

	const uint16_t ms = duration ? duration : 1;
	if (freq == 0)
		sound.noTone();
	else
		sound.tone(freq, ms);

	toneNoteEnd = millis() + ms;
	tonePosition += 2;
	tonePlaying = true;
}

void Platform::PlaySound(const uint16_t* audioPattern)
{
	if (!audioPattern || !IsAudioEnabled())
		return;

	// La bola de fuego lanzada con la mano usa una llamarada PCM real
	// ('fishiuu') en lugar del patron de pitidos original.
	if (audioPattern == Sounds::Attack)
	{
		stopPattern();
		sound.playPcm(SFX_FIRE_FLAME, SFX_FIRE_FLAME_LEN);
		return;
	}

	tonePattern = audioPattern;
	tonePosition = 0;
	startNextNote();
}

void Platform::UpdateAudio()
{
	if (tonePlaying && (int32_t)(millis() - toneNoteEnd) >= 0)
		startNextNote();
}

bool Platform::IsAudioEnabled()
{
	return ArduboyTones::getVolume() > 0;
}

void Platform::SetAudioEnabled(bool isEnabled)
{
	ArduboyTones::setVolume(isEnabled ? 3 : 0);
	if (!isEnabled)
		stopPattern();
}

void Platform::ExpectLoadDelay()
{
	// El cartucho usa su propio acumulador de tiempo; no hay nada que reiniciar.
}

} // namespace Catacombs3D
