#pragma once

// =============================================================================
// Defines del motor original de Catacombs of the Damned (jhhoward/Arduboy3D).
//
// Unica adaptacion de plataforma: en AVR se usaba <avr/pgmspace.h> y los
// registros de la MCU. En ESP32-C6 (RISC-V 32-bit) y en el emulador de PC la
// flash se direcciona como memoria normal, pero el acceso puede no estar
// alineado, asi que las macros pgm_read_* se resuelven con memcpy (lecturas
// seguras, sin depender de endianness ni de alineacion).
// =============================================================================

#if defined(_WIN32)
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR(s) (s)
#else
#include <Arduino.h>
#if defined(__AVR__)
#include <avr/pgmspace.h>
#endif
#endif

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif

// Lecturas seguras desde "flash" para ESP32-C6 y para el emulador nativo.
static inline uint8_t catacombs3d_pgm_read_u8(const void* addr)
{
	uint8_t value;
	memcpy(&value, addr, sizeof(value));
	return value;
}

static inline uint16_t catacombs3d_pgm_read_u16(const void* addr)
{
	uint16_t value;
	memcpy(&value, addr, sizeof(value));
	return value;
}

static inline uintptr_t catacombs3d_pgm_read_ptr(const void* addr)
{
	uintptr_t value;
	memcpy(&value, addr, sizeof(value));
	return value;
}

#undef pgm_read_byte
#undef pgm_read_word
#undef pgm_read_ptr

#define pgm_read_byte(addr) catacombs3d_pgm_read_u8((const void*)(addr))
#define pgm_read_word(addr) catacombs3d_pgm_read_u16((const void*)(addr))
#define pgm_read_ptr(addr) ((const void*)(uintptr_t)catacombs3d_pgm_read_ptr((const void*)(addr)))

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define TARGET_FRAMERATE 30
#define DEV_MODE 0

#define INPUT_LEFT 1
#define INPUT_RIGHT 2
#define INPUT_UP 4
#define INPUT_DOWN 8
#define INPUT_A 16
#define INPUT_B 32

#define COLOUR_WHITE 1
#define COLOUR_BLACK 0

#define FIXED_ANGLE_MAX 256

#define CAMERA_SCALE 1
#define CLIP_PLANE 32
#define CLIP_ANGLE 32
#define NEAR_PLANE_MULTIPLIER 130
#define NEAR_PLANE (DISPLAY_WIDTH * NEAR_PLANE_MULTIPLIER / 256)
#define HORIZON (DISPLAY_HEIGHT / 2)

#define CELL_SIZE 256
#define PARTICLES_PER_SYSTEM 8
#define BASE_SPRITE_SIZE 16
#define MAX_SPRITE_SIZE (DISPLAY_HEIGHT / 2)
#define MIN_TEXTURE_DISTANCE 4
#define MAX_QUEUED_DRAWABLES 12

#define TURN_SPEED 3
