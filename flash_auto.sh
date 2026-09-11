#!/bin/bash
echo "[AUTO-FLASHER] Esperando activamente a que el ESP32-C3 se conecte en /dev/ttyACM0 o /dev/ttyUSB0 (hasta 10 minutos)..."

while true; do
    PORT=""
    if [ -e "/dev/ttyACM0" ]; then
        PORT="/dev/ttyACM0"
    elif [ -e "/dev/ttyUSB0" ]; then
        PORT="/dev/ttyUSB0"
    fi

    if [ -n "$PORT" ]; then
        echo "[AUTO-FLASHER] 🚀 ¡Dispositivo detectado en $PORT! Flasheando firmware Arduboy..."
        /home/diego/.platformio/penv/bin/esptool.py --chip esp32c3 --port "$PORT" --baud 921600 \
            --before default_reset --after hard_reset write_flash -z \
            --flash_mode dio --flash_freq 80m --flash_size 4MB \
            0x0000 .pio/build/esp32c3_super_mini/bootloader.bin \
            0x8000 .pio/build/esp32c3_super_mini/partitions.bin \
            0x10000 .pio/build/esp32c3_super_mini/firmware.bin
        
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 0 ]; then
            echo "[AUTO-FLASHER] ✅ ¡FLASHEO COMPLETADO EXITOSAMENTE A LA PLACA ESP32-C3!"
            exit 0
        else
            echo "[AUTO-FLASHER] Error durante la transferencia. Reintentando..."
            sleep 1
        fi
    fi
    sleep 0.3
done
