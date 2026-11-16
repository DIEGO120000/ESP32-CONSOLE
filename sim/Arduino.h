#pragma once
// =============================================================================
// ARDUINO SHIM PARA EL EMULADOR DE PC ([env:pc_emulator])
// =============================================================================
// Solo se usa en el build nativo: replica lo que los ports usan de la API de
// Arduino (tiempo, azar, pines, Serial, PROGMEM...). No afecta al firmware.
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <cstdarg>
#include <algorithm>

#define ARDUINO 10819

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define ANALOG 3

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define PROGMEM
#define PGM_P const char *
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t *>(addr))
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t *>(addr))
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t *>(addr))
#define pgm_read_float(addr) (*reinterpret_cast<const float *>(addr))
// const-correcto: los arrays PROGMEM de punteros suelen ser const.
#define pgm_read_ptr(addr) (*reinterpret_cast<const void *const *>(addr))

inline void *memcpy_P(void *dst, const void *src, size_t n) { return memcpy(dst, src, n); }
inline size_t strlen_P(const char *s) { return strlen(s); }
inline int strcmp_P(const char *a, const char *b) { return strcmp(a, b); }
inline char *strcpy_P(char *dst, const char *src) { return strcpy(dst, src); }
inline uint32_t esp_random() { return ((uint32_t)rand() << 16) ^ (uint32_t)rand(); }

using std::min;
using std::max;

// Pines fisicos de la consola (algunos ports los consultan directamente).
#define ARDUBOY_PIN_UP      14
#define ARDUBOY_PIN_DOWN    18
#define ARDUBOY_PIN_RIGHT   19
#define ARDUBOY_PIN_LEFT     5
#define ARDUBOY_PIN_SELECT  20
#define PIN_NAV_CENTER      ARDUBOY_PIN_SELECT
#define PIN_BUTTON_A         3
#define PIN_BUTTON_B         4
#define ARDUBOY_PIN_BOOT     9
#define ARDUBOY_PIN_BUZZER   1
#define ARDUBOY_PIN_BATTERY  2

typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;

// Arduino define F("...") para cadenas en Flash. Arduboy2/ports lo pasan a
// print(), asi que basta con una etiqueta que el shim de Print reconozca.
class __FlashStringHelper;
#define F(str) (reinterpret_cast<const __FlashStringHelper *>(str))
#define PSTR(str) (str)

// --- Tiempo -----------------------------------------------------------------
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
inline void yield() {}
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t pin) { (void)pin; return HIGH; }
inline void digitalWrite(uint8_t, uint8_t) {}
inline int analogRead(uint8_t) { return 512; }
inline void rgbLedWrite(uint8_t, uint8_t, uint8_t, uint8_t) {}
inline void analogWrite(uint8_t, int) {}

// --- Azar y utilidades matematicas -----------------------------------------
void randomSeed(unsigned long seed);
long random(long max);
long random(long min, long max);

long map(long x, long in_min, long in_max, long out_min, long out_max);

template <typename T>
inline T constrain(T v, T lo, T hi) { return (v < lo) ? lo : ((v > hi) ? hi : v); }

template <typename T>
inline T sq(T x) { return x * x; }

// --- Serial (consola del PC) ------------------------------------------------
class SimSerial {
public:
    void begin(unsigned long) {}
    void end() {}
    void flush() { fflush(stdout); }
    size_t write(uint8_t c) { fputc(c, stdout); return 1; }
    size_t write(const uint8_t *buf, size_t n) { return fwrite(buf, 1, n, stdout); }

    size_t print(const char *s) { return s ? fputs(s, stdout) >= 0 ? strlen(s) : 0 : 0; }
    size_t print(char c) { fputc(c, stdout); return 1; }
    size_t print(const __FlashStringHelper *s) { return print(reinterpret_cast<const char *>(s)); }
    template <typename T>
    size_t print(T v) { return printf("%ld", (long)v); }
    size_t print(double v) { return printf("%.2f", v); }
    size_t println() { fputc('\n', stdout); return 1; }
    size_t println(const char *s) { print(s); return println(); }
    template <typename T>
    size_t println(T v) { print(v); return println(); }

    size_t printf(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int n = vfprintf(stdout, fmt, ap);
        va_end(ap);
        return n > 0 ? (size_t)n : 0;
    }
};

inline SimSerial Serial;
