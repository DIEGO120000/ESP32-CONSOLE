#pragma once
// =============================================================================
// Print SHIM (solo [env:pc_emulator])
// =============================================================================
// Arduboy2 hereda de Print. En PC replicamos lo justo para que arduboy.print()
// y arduboy.printf() funcionen sobre el framebuffer del emulador.
// =============================================================================

#include <Arduino.h>
#include <type_traits>

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buf, size_t n) {
        size_t sent = 0;
        while (sent < n) sent += write(buf[sent]);
        return sent;
    }

    size_t print(const char *s) {
        if (!s) return 0;
        return write(reinterpret_cast<const uint8_t *>(s), strlen(s));
    }
    size_t print(char c) { return write(static_cast<uint8_t>(c)); }
    size_t print(const __FlashStringHelper *s) {
        return print(reinterpret_cast<const char *>(s));
    }

    // Enteros: el template evita las ambiguedades de promocion (uint16_t ->
    // int vs unsigned int) que Arduino resuelve con sobrecargas por plataforma.
    template <typename T,
              typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    size_t print(T v, int base = DEC) {
        if (base == DEC && std::is_signed<T>::value) {
            long n = static_cast<long>(v);
            if (n < 0) {
                write('-');
                return 1 + printNumber(static_cast<unsigned long>(-n), 10);
            }
        }
        return printNumber(static_cast<unsigned long>(v), base);
    }

    size_t print(double v, int digits = 2) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.*f", digits, v);
        return print(buf);
    }

    size_t println() { return write('\r') + write('\n'); }
    size_t println(const char *s) { return print(s) + println(); }
    size_t println(char c) { return print(c) + println(); }
    size_t println(const __FlashStringHelper *s) { return print(s) + println(); }

    template <typename T,
              typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    size_t println(T v, int base = DEC) { return print(v, base) + println(); }

    size_t println(double v, int digits = 2) { return print(v, digits) + println(); }

    size_t printf(const char *fmt, ...) {
        char buf[256];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n < 0) return 0;
        if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;
        return write(reinterpret_cast<const uint8_t *>(buf), (size_t)n);
    }

private:
    size_t printNumber(unsigned long n, int base) {
        if (base < 2) base = 10;
        char buf[34];
        char *p = &buf[sizeof(buf) - 1];
        *p = '\0';
        do {
            const int d = (int)(n % base);
            *--p = (char)(d < 10 ? '0' + d : 'A' + d - 10);
            n /= base;
        } while (n);
        return print(p);
    }
};
