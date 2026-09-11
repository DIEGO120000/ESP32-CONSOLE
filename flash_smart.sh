#!/bin/bash
echo "[MONITOR] Esperando a que el ESP32-C3 entre en modo Bootloader..."

while true; do
    if dmesg | tail -n 15 | grep -q "3c:dc:75:3a:23:e8\|3C:DC:75:3A:23:E8"; then
        echo "[BOOTLOADER] 🎯 ¡Modo Bootloader detectado! Grabando firmware..."
        /home/diego/.platformio/penv/bin/esptool --chip esp32c3 --port /dev/ttyACM0 --baud 460800 \
            --before no-reset --after hard-reset write_flash -z \
            --flash_mode dio --flash_freq 80m --flash_size 4MB \
            0x0000 .pio/build/esp32c3_super_mini/bootloader.bin \
            0x8000 .pio/build/esp32c3_super_mini/partitions.bin \
            0x10000 .pio/build/esp32c3_super_mini/firmware.bin
        
        if [ $? -eq 0 ]; then
            echo "=================================================="
            echo "🎉 ¡EXITO TOTAL! CONSOLA FLASHEADA AL 100%"
            echo "=================================================="
            exit 0
        fi
    fi
    sleep 0.2
done
