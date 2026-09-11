# ESP32-CONSOLE

Consola de videojuegos retro y emulador Arduboy / mini juegos para microcontrolador ESP32 / ESP32-C6 Super Mini.

## 🚀 Características
- **Plataforma:** ESP32-C6 (PlatformIO / Arduino Framework)
- **Pantalla:** OLED SSD1306 128x64 (I2C)
- **Audio & Melodías:** Sintetizador de tonos y buzzer (GPIO 2) / ArduboyTones
- **LED RGB:** WS2812 integrado (GPIO 8)
- **Conectividad:** BLE (Bluetooth Low Energy)
- **Juegos y Roms:** Emulación y juegos retro integrados (Ping Pong, Breakout, Space Invaders, etc.)

## 🛠️ Requisitos y Compilación
Este proyecto está configurado para [PlatformIO](https://platformio.org/).

### Compilar y subir firmware:
```bash
# Compilar
pio run

# Flashear en ESP32-C6
pio run --target upload
```

### Scripts de Flasheo:
Incluye scripts de utilidad para flasheo automático y configuración rápida en ESP32-C6 / Super Mini:
- `./flash_esp32c6.sh`
- `./flash_auto.sh`
- `./flash_direct.sh`
